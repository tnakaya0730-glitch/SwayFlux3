#include "http_transfer_impl.hpp"
#include <cstdio>
#include <stdexcept>

namespace swayflux3 {
std::size_t HttpTransferJob::Impl::header_callback(char *data, std::size_t size, std::size_t count,
                                                   void *opaque) noexcept {
    auto &self = *static_cast<Impl *>(opaque);
    try {
        if (self.context->cancellation_requested())
            return 0;
        self.watchdog->activity();
        const auto bytes = size * count;
        if (self.response.push(std::string_view(data, bytes), self.options.max_response_header_bytes)) {
            const auto status = self.response.info.status;
            if (detail::redirect_status(status) && !self.response.location.empty() &&
                self.options.direction == TransferDirection::download && self.options.follow_redirects) {
                self.redirect_pending = true;
                return 0;
            }
            // 1xxと認証交渉中の401/407は最終応答として通知しません。
            const bool authentication = (status == 401 && self.origin_authentication) ||
                                        (status == 407 && self.proxy_authentication);
            if (status >= 200 && !authentication)
                self.prepare_response();
        }
        return bytes;
    } catch (const std::exception &ex) {
        self.callback_error = ex.what();
    } catch (...) {
        self.callback_error = "response header callback exception";
    }
    return 0;
}
std::size_t HttpTransferJob::Impl::write_callback(char *data, std::size_t size, std::size_t count,
                                                  void *opaque) noexcept {
    auto &self = *static_cast<Impl *>(opaque);
    try {
        if (self.context->cancellation_requested())
            return 0;
        const auto bytes = size * count;
        if (self.response.info.status < 200 || self.response.info.status >= 300)
            return bytes;
        self.prepare_response();
        if (self.options.direction == TransferDirection::upload)
            return bytes;
        const auto transferred = self.progress.transferred_bytes;
        const auto limit = self.options.max_download_bytes;
        if (limit && (transferred > *limit || bytes > *limit - transferred))
            throw std::runtime_error("decoded download size limit exceeded");
        if (self.expected_body &&
            (transferred > *self.expected_body || bytes > *self.expected_body - transferred))
            throw std::runtime_error("range body exceeds requested length");
        const auto begin = Clock::now();
        self.sink->write(data, bytes);
        self.watchdog->exclude(Clock::now() - begin);
        self.progress.transferred_bytes += bytes;
        self.check_control();
        return bytes;
    } catch (const std::exception &ex) {
        self.callback_error = ex.what();
    } catch (...) {
        self.callback_error = "download sink exception";
    }
    return 0;
}
std::size_t HttpTransferJob::Impl::read_callback(char *output, std::size_t size, std::size_t count,
                                                 void *opaque) noexcept {
    auto &self = *static_cast<Impl *>(opaque);
    try {
        if (self.context->cancellation_requested())
            return CURL_READFUNC_ABORT;
        const auto begin = Clock::now();
        const auto n = self.input->read(output, size * count);
        self.watchdog->exclude(Clock::now() - begin);
        self.progress.transferred_bytes = self.input->consumed();
        return n;
    } catch (const std::exception &ex) {
        self.callback_error = ex.what();
    } catch (...) {
        self.callback_error = "upload source exception";
    }
    return CURL_READFUNC_ABORT;
}
int HttpTransferJob::Impl::seek_callback(void *opaque, curl_off_t offset, int origin) noexcept {
    auto &self = *static_cast<Impl *>(opaque);
    try {
        self.check_control();
        if (origin != SEEK_SET || offset != 0)
            return CURL_SEEKFUNC_CANTSEEK;
        const auto begin = Clock::now();
        self.input->rewind();
        self.watchdog->exclude(Clock::now() - begin);
        self.progress.transferred_bytes = 0;
        self.check_control();
        return CURL_SEEKFUNC_OK;
    } catch (const std::exception &ex) {
        self.callback_error = ex.what();
    } catch (...) {
        self.callback_error = "upload rewind exception";
    }
    return CURL_SEEKFUNC_FAIL;
}
int HttpTransferJob::Impl::progress_callback(void *opaque, curl_off_t, curl_off_t download, curl_off_t,
                                             curl_off_t upload) noexcept {
    auto &self = *static_cast<Impl *>(opaque);
    if (download != self.last_download || upload != self.last_upload)
        self.watchdog->activity();
    self.last_download = download;
    self.last_upload = upload;
    return self.context->cancellation_requested() ? 1 : 0;
}
} // namespace swayflux3
