#include "http_proxy.hpp"
#include "http_transfer_impl.hpp"
#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <zstd.h>

namespace swayflux3 {
using detail::curl_size;
using detail::milliseconds;
HttpTransferJob::HttpTransferJob(HttpTransferOptions options) : status_(std::make_shared<TransferStatus>()) {
    if (options.url.empty())
        throw std::invalid_argument("url must not be empty");
    if (options.buffer_size < 16384 || options.buffer_size > 2 * 1024 * 1024)
        throw std::invalid_argument("buffer_size must be 16 KiB through 2 MiB");
    if (options.path.empty() && (options.direction == TransferDirection::upload || !options.sink))
        throw std::invalid_argument("file path is required");
    if ((options.priority_response == PriorityResponse::limit_bandwidth ||
         options.priority_response == PriorityResponse::limit_bandwidth_or_pause) &&
        !options.priority_bandwidth)
        throw std::invalid_argument("priority_bandwidth must be positive");
    if (options.zstd_level < ZSTD_minCLevel() || options.zstd_level > ZSTD_maxCLevel())
        throw std::invalid_argument("invalid zstd_level");
    if (options.gzip_level < -1 || options.gzip_level > 9)
        throw std::invalid_argument("invalid gzip_level");
    if (!options.max_response_header_bytes)
        throw std::invalid_argument("response header limit must be positive");
    if (options.range) {
        const auto range = *options.range;
        if (options.direction != TransferDirection::download || !range.length ||
            range.offset > (std::numeric_limits<std::uint64_t>::max)() - (range.length - 1))
            throw std::invalid_argument("invalid HTTP range");
    }
    if (options.keepalive_idle.count() <= 0 || options.keepalive_interval.count() <= 0 ||
        options.keepalive_idle.count() > (std::numeric_limits<long>::max)() ||
        options.keepalive_interval.count() > (std::numeric_limits<long>::max)())
        throw std::invalid_argument("invalid TCP keepalive interval");
    detail::validate_proxy(options.proxy);
    for (const auto &header : options.headers) {
        if (header.find_first_of("\r\n") != std::string::npos || header.find('\0') != std::string::npos)
            throw std::invalid_argument("header contains newline");
        const auto separator = header.find_first_of(":;");
        auto name = header.substr(0, separator);
        if (name.empty() ||
            !std::all_of(name.begin(), name.end(),
                         [](unsigned char c) {
                             return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                    (c >= '0' && c <= '9') ||
                                    std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos;
                         }) ||
            (separator != std::string::npos && header[separator] == ';' && separator + 1 != header.size()))
            throw std::invalid_argument("invalid HTTP header name or empty-header syntax");
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == "content-length" || name == "transfer-encoding" || name == "content-encoding" ||
            name == "accept-encoding")
            throw std::invalid_argument("compression and body framing headers are "
                                        "managed by HttpTransferJob");
        if (options.range && name == "range")
            throw std::invalid_argument("range option and Range header conflict");
        if (name == "proxy-authorization")
            throw std::invalid_argument("use ProxyOptions for proxy authentication");
    }
    (void)milliseconds(options.connect_timeout);
    (void)milliseconds(options.total_timeout);
    (void)milliseconds(options.idle_timeout);
    (void)curl_size(options.bandwidth_limit);
    (void)curl_size(options.priority_bandwidth);
    CommunicationInfo initial;
    initial.direction = options.direction;
    status_->report(initial);
    impl_ = std::make_unique<Impl>(std::move(options));
}
void HttpTransferJob::Impl::validate_features() const {
    const auto *features = curl_version_info(CURLVERSION_NOW);
    if (options.decode_response && !options.range) {
        const auto encoding = options.response_encoding;
        const auto require = [&](int feature, const char *name) {
            if (!(features->features & feature))
                throw std::runtime_error(std::string("libcurl lacks response decoder: ") + name);
        };
        if (encoding == ResponseEncoding::automatic || encoding == ResponseEncoding::gzip)
            require(CURL_VERSION_LIBZ, "gzip");
        if (encoding == ResponseEncoding::automatic || encoding == ResponseEncoding::brotli)
            require(CURL_VERSION_BROTLI, "Brotli");
        if (encoding == ResponseEncoding::automatic || encoding == ResponseEncoding::zstd)
            require(CURL_VERSION_ZSTD, "Zstd");
    }
    if (options.revocation_policy == RevocationPolicy::best_effort &&
        (!features->ssl_version || std::string(features->ssl_version).find("Schannel") == std::string::npos))
        throw std::invalid_argument("best-effort revocation requires Schannel");
}
} // namespace swayflux3
