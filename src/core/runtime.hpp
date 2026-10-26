#pragma once
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <swayflux3/core/scheduler.hpp>
#include <thread>
#include <vector>

namespace swayflux3::detail {
struct Entry {
    JobSnapshot snapshot;
    JobSignals signals;
    std::uint64_t sequence = 0;
    bool started = false;
    bool finishing = false;
    bool resume_pending = false;
    std::uint64_t resume_epoch = 0;
    std::atomic<bool> cancelled{false};
    std::unique_ptr<Job> job;
    std::thread::id worker;
};
struct Launch {
    std::shared_ptr<Entry> entry;
    std::unique_ptr<Job> job;
};
struct ReadyKey {
    int priority;
    std::uint64_t sequence;
    JobId id;
    bool operator<(const ReadyKey &other) const noexcept {
        if (priority != other.priority)
            return priority > other.priority;
        if (sequence != other.sequence)
            return sequence < other.sequence;
        return id < other.id;
    }
};
extern thread_local const void *current_runtime;
struct Runtime : std::enable_shared_from_this<Runtime> {
    explicit Runtime(std::size_t n) : capacity(n) {}
    std::mutex mutex;
    std::condition_variable changed;
    std::map<JobId, std::shared_ptr<Entry>> entries;
    std::map<JobId, std::shared_ptr<Entry>> observers;
    std::set<ReadyKey> ready;
    std::map<int, std::size_t> priorities, queued_priorities;
    std::optional<int> notified_highest, notified_queued;
    std::size_t capacity;
    std::size_t active = 0;
    bool closed = false;
    JobId next_id = 1;
    std::uint64_t sequence = 0;

    // 以下はmutexを保持して呼び出します。
    void index(Entry &entry);
    void unindex(Entry &entry);
    void transition(Entry &entry, JobState state);
    void mark_finishing(Entry &entry);
    void refresh(Entry &entry);

    // mutexを保持して呼び出し、起動対象を取り出します。
    std::vector<Launch> reconcile();
    void finish(const std::shared_ptr<Entry> &entry, JobState state, std::string error);
    // Jobの実行・破棄を伴うため、mutexを保持せず呼び出します。
    void launch(std::vector<Launch> launches);
    void check_wait() const;
};
} // namespace swayflux3::detail
