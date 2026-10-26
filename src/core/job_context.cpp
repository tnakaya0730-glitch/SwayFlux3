#include "runtime.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace swayflux3 {
bool is_terminal(JobState s) noexcept {
    return s == JobState::completed || s == JobState::cancelled || s == JobState::failed;
}
JobContext::JobContext(std::shared_ptr<detail::Runtime> r, std::shared_ptr<detail::Entry> e)
    : runtime_(std::move(r)), entry_(std::move(e)) {}
JobId JobContext::id() const noexcept { return entry_->snapshot.id; }
bool JobContext::cancellation_requested() const noexcept {
    return entry_->cancelled.load(std::memory_order_acquire);
}
JobSignals JobContext::signals() const {
    std::lock_guard<std::mutex> lock(runtime_->mutex);
    return entry_->signals;
}
JobSignals JobContext::wait_for_change(std::uint64_t version, std::chrono::milliseconds timeout) const {
    std::unique_lock<std::mutex> lock(runtime_->mutex);
    const auto ready = [&] {
        return entry_->signals.version != version || is_terminal(entry_->snapshot.state);
    };
    if (timeout == std::chrono::milliseconds::max())
        runtime_->changed.wait(lock, ready);
    else
        runtime_->changed.wait_for(lock, timeout, ready);
    return entry_->signals;
}
void JobContext::report(JobInfo info) const {
    std::lock_guard<std::mutex> lock(runtime_->mutex);
    if (!is_terminal(entry_->snapshot.state))
        entry_->snapshot.info = std::move(info);
}
bool JobContext::pause() const {
    std::vector<detail::Launch> launches;
    {
        std::lock_guard<std::mutex> lock(runtime_->mutex);
        auto &e = *entry_;
        if (!e.snapshot.info.capabilities.pause || e.finishing || is_terminal(e.snapshot.state))
            return false;
        ++e.resume_epoch;
        e.resume_pending = false;
        runtime_->transition(e, JobState::paused);
        launches = runtime_->reconcile();
    }
    runtime_->launch(std::move(launches));
    return true;
}
bool JobContext::resume() const { return resume_for(std::chrono::milliseconds::max()); }
bool JobContext::resume_for(std::chrono::milliseconds timeout) const {
    std::vector<detail::Launch> launches;
    std::uint64_t epoch;
    {
        std::lock_guard<std::mutex> lock(runtime_->mutex);
        auto &e = *entry_;
        if (is_terminal(e.snapshot.state) || e.finishing || e.signals.cancellation_requested)
            return false;
        if (e.snapshot.state == JobState::running) {
            e.resume_pending = false;
            return true;
        }
        if (e.snapshot.state == JobState::paused) {
            ++e.resume_epoch;
            e.sequence = runtime_->sequence++;
            runtime_->transition(e, JobState::queued);
            launches = runtime_->reconcile();
        }
        epoch = e.resume_epoch;
    }
    runtime_->launch(std::move(launches));
    std::unique_lock<std::mutex> lock(runtime_->mutex);
    const auto ready = [&] {
        const auto &e = *entry_;
        return e.snapshot.state != JobState::queued || e.signals.cancellation_requested ||
               e.resume_epoch != epoch;
    };
    bool changed = true;
    if (timeout == std::chrono::milliseconds::max())
        runtime_->changed.wait(lock, ready);
    else
        changed = runtime_->changed.wait_for(lock, timeout, ready);
    if (!changed) {
        // 同じmutex内で取り下げるため、枠の割当と競合しても二重返却しません。
        ++entry_->resume_epoch;
        runtime_->transition(*entry_, JobState::paused);
        launches = runtime_->reconcile();
        lock.unlock();
        runtime_->launch(std::move(launches));
        return false;
    }
    if (entry_->resume_epoch != epoch || entry_->signals.cancellation_requested)
        return false;
    if (entry_->snapshot.state != JobState::running)
        return false;
    entry_->resume_pending = false;
    return true;
}

} // namespace swayflux3
