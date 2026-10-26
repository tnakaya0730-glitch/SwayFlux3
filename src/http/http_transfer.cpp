#include "http_transfer_impl.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace swayflux3 {
using Clock = std::chrono::steady_clock;
using detail::check;
using detail::check_multi;
using detail::curl_size;
HttpTransferJob::~HttpTransferJob() = default;
std::shared_ptr<const TransferStatus> HttpTransferJob::status() const noexcept { return status_; }
CommunicationInfo HttpTransferJob::communication_info() const { return status_->snapshot(); }
JobInfo HttpTransferJob::info() const {
    const auto policy = impl_->options.priority_response;
    JobInfo result;
    result.name = impl_->options.name.empty() ? "HTTP transfer" : impl_->options.name;
    result.capabilities = {
        policy == PriorityResponse::pause || policy == PriorityResponse::limit_bandwidth_or_pause, true,
        policy == PriorityResponse::limit_bandwidth || policy == PriorityResponse::limit_bandwidth_or_pause};
    result.resume_mode = result.capabilities.pause ? ResumeMode::from_checkpoint : ResumeMode::not_resumable;
    result.load = LoadHint::high;
    const auto current = status_->snapshot();
    result.progress = {current.transferred_bytes, current.total_bytes};
    return result;
}
JobOutcome HttpTransferJob::run(JobContext context) {
    try {
        return run_transfer(context);
    } catch (...) {
        if (context.cancellation_requested())
            return JobOutcome::cancelled;
        throw;
    }
}
JobOutcome HttpTransferJob::run_transfer(JobContext context) {
    auto &s = *impl_;
    const auto &o = s.options;
    if (s.used)
        throw std::logic_error("HttpTransferJob can only run once");
    s.used = true;
    s.context = context;
    if (context.cancellation_requested())
        return JobOutcome::cancelled;
    detail::init_curl();
    s.validate_features();
    s.watchdog = std::make_unique<detail::HttpWatchdog>(o.total_timeout, o.idle_timeout);
    s.initial_url = s.current_url = detail::inspect_url(o.url);
    s.prepare_transfer();
    detail::CurlSession curl;
    if (!curl.easy || !curl.multi)
        throw std::bad_alloc();
    s.last_report = Clock::now();
    const auto publish = [&] {
        curl_off_t wire = 0;
        check(curl_easy_getinfo(curl.easy,
                                o.direction == TransferDirection::download ? CURLINFO_SIZE_DOWNLOAD_T
                                                                           : CURLINFO_SIZE_UPLOAD_T,
                                &wire));
        s.progress.wire_bytes = s.wire_base + static_cast<std::uint64_t>((std::max)(curl_off_t{0}, wire));
        check(curl_easy_getinfo(curl.easy, CURLINFO_RESPONSE_CODE, &s.progress.http_status));
        const auto seconds = std::chrono::duration<double>(Clock::now() - s.watchdog->started()).count();
        s.progress.bytes_per_second =
            seconds > 0 ? static_cast<double>(s.progress.transferred_bytes) / seconds : 0;
        status_->report(s.progress);
        context.report(info());
        s.last_report = Clock::now();
    };
    const auto should_pause = [&](const JobSignals &signals) {
        return o.priority_response == PriorityResponse::pause
                   ? signals.priority.has_higher_priority_job
                   : o.priority_response == PriorityResponse::limit_bandwidth_or_pause &&
                         signals.priority.has_higher_priority_queued_job;
    };
    std::size_t redirects = 0;
    while (true) {
        s.response.reset(s.current_url.public_url);
        s.response_notified = false;
        s.redirect_pending = false;
        s.last_download = s.last_upload = 0;
        std::array<char, CURL_ERROR_SIZE> error{};
        s.configure(curl, error.data());
        s.watchdog->activity();
        check_multi(curl_multi_add_handle(curl.multi, curl.easy));
        curl.added = true;
        bool done = false, transport_paused = false;
        CURLcode result = CURLE_OK;
        std::uint64_t applied_limit = (std::numeric_limits<std::uint64_t>::max)();
        while (!done) {
            auto signals = context.signals();
            if (signals.cancellation_requested) {
                publish();
                return JobOutcome::cancelled;
            }
            s.watchdog->check();
            if (should_pause(signals)) {
                if (!transport_paused) {
                    const auto paused = curl_easy_pause(curl.easy, CURLPAUSE_ALL);
                    if (paused != CURLE_BAD_FUNCTION_ARGUMENT)
                        check(paused);
                    transport_paused = paused == CURLE_OK;
                }
                publish();
                if (!context.pause())
                    throw std::logic_error("pause rejected");
                while (should_pause(signals) && !signals.cancellation_requested)
                    signals = context.wait_for_change(signals.version, s.watchdog->remaining());
                if (signals.cancellation_requested) {
                    publish();
                    return JobOutcome::cancelled;
                }
                if (!context.resume_for(s.watchdog->remaining())) {
                    publish();
                    if (context.cancellation_requested())
                        return JobOutcome::cancelled;
                    throw std::runtime_error("HTTP total timeout exceeded while waiting for execution slot");
                }
                s.watchdog->activity();
                (void)s.watchdog->remaining();
                if (should_pause(context.signals()))
                    continue;
            }
            if (transport_paused) {
                transport_paused = false;
                check(curl_easy_pause(curl.easy, CURLPAUSE_CONT));
            }
            signals = context.signals();
            auto limit = o.bandwidth_limit;
            if ((o.priority_response == PriorityResponse::limit_bandwidth ||
                 o.priority_response == PriorityResponse::limit_bandwidth_or_pause) &&
                signals.priority.has_higher_priority_job)
                limit = limit ? (std::min)(limit, o.priority_bandwidth) : o.priority_bandwidth;
            if (limit != applied_limit) {
                check(curl_easy_setopt(curl.easy, CURLOPT_MAX_RECV_SPEED_LARGE, curl_size(limit)));
                check(curl_easy_setopt(curl.easy, CURLOPT_MAX_SEND_SPEED_LARGE, curl_size(limit)));
                s.progress.bandwidth_limit = applied_limit = limit;
                publish();
            }
            int running = 0;
            check_multi(curl_multi_perform(curl.multi, &running));
            int messages = 0;
            while (const auto *message = curl_multi_info_read(curl.multi, &messages))
                if (message->msg == CURLMSG_DONE) {
                    done = true;
                    result = message->data.result;
                }
            if (Clock::now() - s.last_report >= std::chrono::milliseconds(100))
                publish();
            if (!done)
                check_multi(curl_multi_poll(curl.multi, nullptr, 0, 50, nullptr));
        }
        publish();
        if (context.cancellation_requested())
            return JobOutcome::cancelled;
        if (!s.callback_error.empty())
            throw std::runtime_error(s.callback_error);
        s.watchdog->check();
        if (s.redirect_pending) {
            if (redirects++ >= o.max_redirects)
                throw std::runtime_error("HTTP redirect limit exceeded");
            s.current_url =
                detail::redirect_url(s.current_url, s.response.location, o.allow_insecure_redirect);
            s.wire_base = s.progress.wire_bytes;
            curl.reset_request();
            continue;
        }
        if (result != CURLE_OK)
            throw std::runtime_error(std::string("HTTP transfer failed: ") + curl_easy_strerror(result));
        s.prepare_response();
        status_->report(s.progress);
        if (s.progress.http_status < 200 || s.progress.http_status >= 300)
            throw std::runtime_error("HTTP status " + std::to_string(s.progress.http_status));
        if (s.expected_body && s.progress.transferred_bytes != *s.expected_body)
            throw std::runtime_error("range body length mismatch");
        s.check_control();
        if (s.sink)
            s.sink->finish();
        s.check_control();
        if (o.direction == TransferDirection::download)
            s.progress.total_bytes = s.progress.transferred_bytes;
        publish();
        s.check_control();
        return JobOutcome::completed;
    }
}
} // namespace swayflux3
