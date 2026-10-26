#include "communication/file_download_sink.hpp"
#include "http_proxy.hpp"
#include "http_transfer_impl.hpp"
#include <stdexcept>

namespace swayflux3 {
using detail::check;
using detail::curl_size;
using detail::milliseconds;
void HttpTransferJob::Impl::prepare_transfer() {
    const auto &o = options;
    progress.direction = o.direction;
    if (o.direction == TransferDirection::download) {
        sink = o.sink ? o.sink : std::make_shared<detail::FileDownloadSink>(o.path, o.buffer_size);
    } else {
        detail::UploadStreamOptions upload;
        upload.path = o.path;
        upload.buffer_size = o.buffer_size;
        upload.gzip_level = o.gzip_level;
        upload.zstd_level = o.zstd_level;
        switch (o.upload_compression) {
        case UploadCompression::none:
            upload.codec = detail::UploadCodec::none;
            break;
        case UploadCompression::gzip:
            upload.codec = detail::UploadCodec::gzip;
            break;
        case UploadCompression::zstd:
            upload.codec = detail::UploadCodec::zstd;
            break;
        }
        input = std::make_unique<detail::UploadStream>(upload);
        progress.total_bytes = input->total();
    }
}
void HttpTransferJob::Impl::configure(detail::CurlSession &curl, char *error) {
    const auto &o = options;
    check(curl_easy_setopt(curl.easy, CURLOPT_ERRORBUFFER, error));
    check(curl_easy_setopt(curl.easy, CURLOPT_URL, current_url.url.c_str()));
    origin_authentication = current_url.origin == initial_url.origin && initial_url.has_credentials;
    if (origin_authentication) {
        check(curl_easy_setopt(curl.easy, CURLOPT_USERNAME, initial_url.username.c_str()));
        check(curl_easy_setopt(curl.easy, CURLOPT_PASSWORD, initial_url.password.c_str()));
    }
    check(curl_easy_setopt(curl.easy, CURLOPT_PROTOCOLS_STR, "http,https"));
    check(curl_easy_setopt(curl.easy, CURLOPT_FOLLOWLOCATION, 0L));
    check(curl_easy_setopt(curl.easy, CURLOPT_NOSIGNAL, 1L));
    check(curl_easy_setopt(curl.easy, CURLOPT_CONNECTTIMEOUT_MS, milliseconds(o.connect_timeout)));
    const auto remaining = watchdog->remaining();
    check(curl_easy_setopt(curl.easy, CURLOPT_TIMEOUT_MS,
                           remaining == (std::chrono::milliseconds::max)() ? 0L : milliseconds(remaining)));
    check(curl_easy_setopt(curl.easy, CURLOPT_FAILONERROR, 0L));
    check(curl_easy_setopt(curl.easy, CURLOPT_SSL_VERIFYPEER, 1L));
    check(curl_easy_setopt(curl.easy, CURLOPT_SSL_VERIFYHOST, 2L));
    if (o.revocation_policy == RevocationPolicy::best_effort)
        check(curl_easy_setopt(curl.easy, CURLOPT_SSL_OPTIONS,
                               static_cast<long>(CURLSSLOPT_REVOKE_BEST_EFFORT)));
    const auto ca = o.ca_bundle.u8string();
    if (!ca.empty())
        check(curl_easy_setopt(curl.easy, CURLOPT_CAINFO, ca.c_str()));
    check(curl_easy_setopt(curl.easy, CURLOPT_TCP_KEEPALIVE, o.tcp_keepalive ? 1L : 0L));
    if (o.tcp_keepalive) {
        check(curl_easy_setopt(curl.easy, CURLOPT_TCP_KEEPIDLE, static_cast<long>(o.keepalive_idle.count())));
        check(curl_easy_setopt(curl.easy, CURLOPT_TCP_KEEPINTVL,
                               static_cast<long>(o.keepalive_interval.count())));
    }
    const auto proxy = o.proxy_resolver ? o.proxy_resolver->resolve(current_url.public_url) : o.proxy;
    // 環境変数やプロキシURL内の認証情報もlibcurlが解釈します。
    proxy_authentication = proxy.mode != ProxyMode::direct &&
                           (proxy.mode == ProxyMode::environment || !proxy.username.empty() ||
                            !proxy.password.empty() || proxy.url.find('@') != std::string::npos);
    detail::configure_proxy(curl, proxy);
    check(curl_easy_setopt(curl.easy, CURLOPT_HEADEROPT, static_cast<long>(CURLHEADER_SEPARATE)));
    check(curl_easy_setopt(curl.easy, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L));
    const char *accepted = "zstd, br, gzip, deflate";
    switch (o.response_encoding) {
    case ResponseEncoding::identity:
        accepted = "identity";
        break;
    case ResponseEncoding::gzip:
        accepted = "gzip";
        break;
    case ResponseEncoding::brotli:
        accepted = "br";
        break;
    case ResponseEncoding::zstd:
        accepted = "zstd";
        break;
    case ResponseEncoding::automatic:
        break;
    }
    if (o.range)
        accepted = "identity";
    check(curl_easy_setopt(curl.easy, CURLOPT_ACCEPT_ENCODING,
                           (o.decode_response || o.range) ? accepted : nullptr));
    check(curl_easy_setopt(curl.easy, CURLOPT_BUFFERSIZE, static_cast<long>(o.buffer_size)));
    check(curl_easy_setopt(curl.easy, CURLOPT_HEADERFUNCTION, &Impl::header_callback));
    check(curl_easy_setopt(curl.easy, CURLOPT_HEADERDATA, this));
    check(curl_easy_setopt(curl.easy, CURLOPT_WRITEFUNCTION, &Impl::write_callback));
    check(curl_easy_setopt(curl.easy, CURLOPT_WRITEDATA, this));
    check(curl_easy_setopt(curl.easy, CURLOPT_NOPROGRESS, 0L));
    check(curl_easy_setopt(curl.easy, CURLOPT_XFERINFOFUNCTION, &Impl::progress_callback));
    check(curl_easy_setopt(curl.easy, CURLOPT_XFERINFODATA, this));
    if (o.direction == TransferDirection::upload) {
        check(curl_easy_setopt(curl.easy, CURLOPT_UPLOAD_BUFFERSIZE, static_cast<long>(o.buffer_size)));
        check(curl_easy_setopt(curl.easy, CURLOPT_READFUNCTION, &Impl::read_callback));
        check(curl_easy_setopt(curl.easy, CURLOPT_READDATA, this));
        check(curl_easy_setopt(curl.easy, CURLOPT_SEEKFUNCTION, &Impl::seek_callback));
        check(curl_easy_setopt(curl.easy, CURLOPT_SEEKDATA, this));
        if (o.upload_method == UploadMethod::put)
            check(curl_easy_setopt(curl.easy, CURLOPT_UPLOAD, 1L));
        else
            check(curl_easy_setopt(curl.easy, CURLOPT_POST, 1L));
        curl_off_t length = curl_size(input->total());
        if (o.upload_compression != UploadCompression::none) {
            curl.header(o.upload_compression == UploadCompression::gzip ? "Content-Encoding: gzip"
                                                                        : "Content-Encoding: zstd");
            length = -1;
        }
        if (o.upload_method == UploadMethod::put)
            check(curl_easy_setopt(curl.easy, CURLOPT_INFILESIZE_LARGE, length));
        else
            check(curl_easy_setopt(curl.easy, CURLOPT_POSTFIELDSIZE_LARGE, length));
    }
    if (o.range)
        curl.header("Range: bytes=" + std::to_string(o.range->offset) + "-" +
                    std::to_string(o.range->offset + o.range->length - 1));
    if (current_url.origin == initial_url.origin)
        for (const auto &header : o.headers)
            curl.header(header);
    if (curl.headers)
        check(curl_easy_setopt(curl.easy, CURLOPT_HTTPHEADER, curl.headers));
}
void HttpTransferJob::Impl::check_control() const {
    if (context->cancellation_requested())
        throw std::runtime_error("HTTP transfer cancelled");
    (void)watchdog->remaining();
}
void HttpTransferJob::Impl::prepare_response() {
    check_control();
    if (response_notified)
        return;
    if (!response.complete || response.info.status < 200)
        throw std::runtime_error("incomplete HTTP response headers");
    const auto &r = response.info;
    const auto &o = options;
    progress.http_status = r.status;
    response_notified = true;
    if (o.on_response) {
        const auto begin = Clock::now();
        o.on_response(r);
        watchdog->exclude(Clock::now() - begin);
        check_control();
    }
    if (o.range) {
        if (!r.content_encoding.empty() && r.content_encoding != "identity")
            throw std::runtime_error("encoded range response rejected");
        const auto wanted = *o.range;
        if (r.status == 206) {
            const auto actual = detail::parse_content_range(r.content_range);
            if (actual.first != wanted.offset || actual.last != wanted.offset + wanted.length - 1)
                throw std::runtime_error("Content-Range does not match request");
        } else if (r.status != 200 || wanted.offset != 0 || !r.content_length ||
                   *r.content_length != wanted.length) {
            throw std::runtime_error("range response status or length does not match request");
        }
        if (r.content_length && *r.content_length != wanted.length)
            throw std::runtime_error("range Content-Length mismatch");
        expected_body = wanted.length;
        progress.total_bytes = wanted.length;
    } else if (o.direction == TransferDirection::download &&
               (!o.decode_response || r.content_encoding.empty() || r.content_encoding == "identity")) {
        progress.total_bytes = r.content_length;
    }
}
} // namespace swayflux3
