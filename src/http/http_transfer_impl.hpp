#pragma once
#include "compression/upload_stream.hpp"
#include "curl_session.hpp"
#include "http_response_parser.hpp"
#include "http_watchdog.hpp"
#include "redirect_controller.hpp"
#include <swayflux3/http/http_transfer.hpp>

namespace swayflux3 {
struct HttpTransferJob::Impl {
    using Clock = std::chrono::steady_clock;
    explicit Impl(HttpTransferOptions value) : options(std::move(value)) {}
    HttpTransferOptions options;
    std::shared_ptr<DownloadSink> sink;
    std::unique_ptr<detail::UploadStream> input;
    std::optional<JobContext> context;
    bool used = false;
    std::string callback_error;
    CommunicationInfo progress;
    Clock::time_point last_report;
    std::unique_ptr<detail::HttpWatchdog> watchdog;
    detail::HttpResponseParser response;
    detail::UrlInfo initial_url, current_url;
    bool response_notified = false;
    bool redirect_pending = false;
    bool origin_authentication = false, proxy_authentication = false;
    std::optional<std::uint64_t> expected_body;
    std::uint64_t wire_base = 0;
    curl_off_t last_download = 0, last_upload = 0;

    static std::size_t write_callback(char *data, std::size_t size, std::size_t count, void *opaque) noexcept;
    static std::size_t read_callback(char *output, std::size_t size, std::size_t count,
                                     void *opaque) noexcept;
    static int progress_callback(void *opaque, curl_off_t, curl_off_t, curl_off_t, curl_off_t) noexcept;
    static int seek_callback(void *opaque, curl_off_t offset, int origin) noexcept;
    static std::size_t header_callback(char *data, std::size_t size, std::size_t count,
                                       void *opaque) noexcept;
    void validate_features() const;
    void prepare_transfer();
    void prepare_response();
    void check_control() const;
    void configure(detail::CurlSession &curl, char *error);
};
} // namespace swayflux3
