#include "runtime.hpp"
#include <algorithm>
#include <stdexcept>

namespace swayflux3::detail {
thread_local const void *current_runtime = nullptr;

namespace {
void decrement(std::map<int, std::size_t> &counts, int priority) {
    auto it = counts.find(priority);
    if (--it->second == 0)
        counts.erase(it);
}
std::optional<int> highest(const std::map<int, std::size_t> &counts) {
    return counts.empty() ? std::nullopt : std::optional<int>(counts.rbegin()->first);
}
} // namespace
void Runtime::index(Entry &e) {
    if (e.finishing)
        return;
    if (e.snapshot.state == JobState::queued) {
        ready.insert({e.snapshot.priority, e.sequence, e.snapshot.id});
        ++queued_priorities[e.snapshot.priority];
    }
    if (e.snapshot.state == JobState::running)
        ++active;
    if (e.snapshot.state == JobState::queued || e.snapshot.state == JobState::running)
        ++priorities[e.snapshot.priority];
}
void Runtime::unindex(Entry &e) {
    if (e.finishing)
        return;
    if (e.snapshot.state == JobState::queued) {
        ready.erase({e.snapshot.priority, e.sequence, e.snapshot.id});
        decrement(queued_priorities, e.snapshot.priority);
    }
    if (e.snapshot.state == JobState::running)
        --active;
    if (e.snapshot.state == JobState::queued || e.snapshot.state == JobState::running)
        decrement(priorities, e.snapshot.priority);
}
void Runtime::transition(Entry &e, JobState state) {
    unindex(e);
    e.snapshot.state = state;
    index(e);
}
void Runtime::mark_finishing(Entry &e) {
    unindex(e);
    e.finishing = true;
}
void Runtime::refresh(Entry &e) {
    const auto top = highest(priorities), queued = highest(queued_priorities);
    const bool higher = top && *top > e.snapshot.priority;
    const bool waiting = queued && *queued > e.snapshot.priority;
    if (higher != e.signals.priority.has_higher_priority_job ||
        waiting != e.signals.priority.has_higher_priority_queued_job) {
        e.signals.priority = {higher, waiting};
        ++e.signals.version;
    }
}
std::vector<Launch> Runtime::reconcile() {
    std::vector<Launch> launches;
    while (active < capacity && !ready.empty()) {
        auto best = entries.at(ready.begin()->id);
        transition(*best, JobState::running);
        if (!best->started) {
            best->started = true;
            observers.emplace(best->snapshot.id, best);
            launches.push_back({best, std::move(best->job)});
        } else
            best->resume_pending = true;
    }
    const auto top = highest(priorities), queued = highest(queued_priorities);
    if (top != notified_highest || queued != notified_queued) {
        for (auto &pair : observers)
            refresh(*pair.second);
        notified_highest = top;
        notified_queued = queued;
    } else
        for (auto &item : launches)
            refresh(*item.entry);
    changed.notify_all();
    return launches;
}

void Runtime::finish(const std::shared_ptr<Entry> &e, JobState state, std::string error) {
    std::vector<Launch> launches;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (is_terminal(e->snapshot.state))
            return;
        transition(*e, state);
        e->resume_pending = false;
        e->snapshot.error = std::move(error);
        entries.erase(e->snapshot.id);
        observers.erase(e->snapshot.id);
        launches = reconcile();
    }
    launch(std::move(launches));
}

void Runtime::launch(std::vector<Launch> launches) {
    for (auto &item : launches) {
        auto e = item.entry;
        // shared_ptrで生成して、スレッド生成失敗時もmutex外でJobを破棄します。
        auto job = std::shared_ptr<Job>(std::move(item.job));
        auto self = shared_from_this();
        try {
            std::thread([self, e, job = std::move(job)]() mutable {
                current_runtime = self.get();
                {
                    std::lock_guard<std::mutex> lock(self->mutex);
                    e->worker = std::this_thread::get_id();
                }
                JobState result = JobState::completed;
                std::string error;
                try {
                    const auto outcome = job->run(JobContext(self, e));
                    result = outcome == JobOutcome::cancelled ? JobState::cancelled : JobState::completed;
                } catch (const std::exception &ex) {
                    result = JobState::failed;
                    error = ex.what();
                } catch (...) {
                    result = JobState::failed;
                    error = "unknown Job exception";
                }
                // Job所有の資源が終了してから、終端通知と実行枠の返却を行います。
                job.reset();
                self->finish(e, result, std::move(error));
                current_runtime = nullptr;
            }).detach();
        } catch (const std::exception &ex) {
            job.reset();
            finish(e, JobState::failed, ex.what());
        }
    }
}

void Runtime::check_wait() const {
    if (current_runtime == this)
        throw std::logic_error("cannot wait for scheduler idle inside its Job");
}
} // namespace swayflux3::detail
