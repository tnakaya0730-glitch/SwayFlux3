#include "runtime.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace swayflux3 {
Scheduler::Scheduler(std::size_t n) {
    if (!n)
        throw std::invalid_argument("max_concurrent_jobs must be positive");
    runtime_ = std::make_shared<detail::Runtime>(n);
}
Scheduler::~Scheduler() { close(); }
JobHandle Scheduler::enqueue(std::unique_ptr<Job> job, int priority) {
    if (!job)
        throw std::invalid_argument("job must not be null");
    auto e = std::make_shared<detail::Entry>();
    e->snapshot.info = job->info();
    e->snapshot.priority = priority;
    e->job = std::move(job);
    std::vector<detail::Launch> launches;
    {
        std::lock_guard<std::mutex> lock(runtime_->mutex);
        if (runtime_->closed)
            throw std::logic_error("scheduler is closed");
        if (runtime_->next_id == std::numeric_limits<JobId>::max())
            throw std::overflow_error("JobId exhausted");
        e->snapshot.id = runtime_->next_id++;
        e->sequence = runtime_->sequence++;
        runtime_->entries.emplace(e->snapshot.id, e);
        runtime_->index(*e);
        launches = runtime_->reconcile();
    }
    runtime_->launch(std::move(launches));
    return JobHandle(runtime_, e);
}
void Scheduler::close() {
    std::lock_guard<std::mutex> lock(runtime_->mutex);
    runtime_->closed = true;
}
void Scheduler::wait_idle() const { (void)wait_idle_for(std::chrono::milliseconds::max()); }
bool Scheduler::wait_idle_for(std::chrono::milliseconds timeout) const {
    runtime_->check_wait();
    std::unique_lock<std::mutex> lock(runtime_->mutex);
    const auto ready = [&] { return runtime_->entries.empty(); };
    if (timeout == std::chrono::milliseconds::max()) {
        runtime_->changed.wait(lock, ready);
        return true;
    }
    return runtime_->changed.wait_for(lock, timeout, ready);
}
void Scheduler::shutdown() {
    close();
    wait_idle();
}

} // namespace swayflux3
