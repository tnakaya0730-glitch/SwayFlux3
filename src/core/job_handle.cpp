#include "runtime.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace swayflux3 {
JobHandle::JobHandle(std::shared_ptr<detail::Runtime> r, std::shared_ptr<detail::Entry> e)
    : runtime_(std::move(r)), entry_(std::move(e)) {}
JobSnapshot JobHandle::snapshot() const {
    std::lock_guard<std::mutex> lock(runtime_->mutex);
    return entry_->snapshot;
}
std::optional<JobSnapshot> JobHandle::wait_for(std::chrono::milliseconds timeout) const {
    std::unique_lock<std::mutex> lock(runtime_->mutex);
    if (entry_->worker == std::this_thread::get_id() && !is_terminal(entry_->snapshot.state))
        throw std::logic_error("cannot wait for own Job");
    const auto ready = [&] { return is_terminal(entry_->snapshot.state); };
    if (timeout == std::chrono::milliseconds::max())
        runtime_->changed.wait(lock, ready);
    else if (!runtime_->changed.wait_for(lock, timeout, ready))
        return std::nullopt;
    return entry_->snapshot;
}
JobSnapshot JobHandle::wait() const { return *wait_for(std::chrono::milliseconds::max()); }
bool JobHandle::request_cancel() const {
    std::unique_ptr<Job> unstarted;
    std::vector<detail::Launch> launches;
    {
        std::lock_guard<std::mutex> lock(runtime_->mutex);
        auto &e = *entry_;
        if (is_terminal(e.snapshot.state) || e.finishing)
            return false;
        if (e.started && !e.snapshot.info.capabilities.interrupt)
            return false;
        if (e.signals.cancellation_requested)
            return true;
        e.signals.cancellation_requested = true;
        e.cancelled.store(true, std::memory_order_release);
        ++e.signals.version;
        if (!e.started) {
            runtime_->mark_finishing(e);
            unstarted = std::move(e.job);
        } else if (e.snapshot.state == JobState::queued || e.resume_pending) {
            ++e.resume_epoch;
            e.resume_pending = false;
            runtime_->transition(e, JobState::paused);
        }
        launches = runtime_->reconcile();
    }
    runtime_->launch(std::move(launches));
    if (unstarted) {
        unstarted.reset();
        runtime_->finish(entry_, JobState::cancelled, {});
    }
    return true;
}
bool JobHandle::set_priority(int priority) const {
    std::vector<detail::Launch> launches;
    {
        std::lock_guard<std::mutex> lock(runtime_->mutex);
        auto &e = *entry_;
        if (is_terminal(e.snapshot.state) || e.finishing)
            return false;
        if (e.snapshot.priority == priority)
            return true;
        runtime_->unindex(e);
        e.snapshot.priority = priority;
        runtime_->index(e);
        ++e.signals.version;
        runtime_->refresh(e);
        launches = runtime_->reconcile();
    }
    runtime_->launch(std::move(launches));
    return true;
}

} // namespace swayflux3
