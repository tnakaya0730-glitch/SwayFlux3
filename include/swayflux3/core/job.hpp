#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace swayflux3 {
namespace detail {
struct Runtime;
struct Entry;
} // namespace detail

using JobId = std::uint64_t;
struct PriorityContext {
    bool has_higher_priority_job = false;
    bool has_higher_priority_queued_job = false;
};
struct JobCapabilities {
    bool pause = false;
    bool interrupt = false;
    bool change_execution_conditions = false;
};
enum class ResumeMode { from_checkpoint, from_beginning, not_resumable };
enum class LoadHint { unknown, low, medium, high };
struct Progress {
    std::uint64_t completed = 0;
    std::optional<std::uint64_t> total;
};
// 通信などの処理種類に依存しない、Job自身の情報です。
struct JobInfo {
    std::string name;
    JobCapabilities capabilities;
    ResumeMode resume_mode = ResumeMode::not_resumable;
    LoadHint load = LoadHint::unknown;
    Progress progress;
};
enum class JobState { queued, running, paused, completed, cancelled, failed };
bool is_terminal(JobState state) noexcept;
struct JobSnapshot {
    JobId id = 0;
    int priority = 0;
    JobState state = JobState::queued;
    JobInfo info;
    std::string error;
};
struct JobSignals {
    PriorityContext priority;
    bool cancellation_requested = false;
    std::uint64_t version = 0;
};
enum class JobOutcome { completed, cancelled };

// コピー可能な制御窓口。変更通知は最新状態に集約されます。
class JobContext {
  public:
    JobId id() const noexcept;
    JobSignals signals() const;
    // 通知全体のロックを取らず、キャンセル要求だけを参照します。
    bool cancellation_requested() const noexcept;
    JobSignals wait_for_change(std::uint64_t version,
                               std::chrono::milliseconds timeout = (std::chrono::milliseconds::max)()) const;
    void report(JobInfo info) const;
    // 実I/O・子タスクを停止した後に呼びます。I/O停止自体は行いません。
    bool pause() const;
    // 実行枠を取得するまで待機します。キャンセルまたは別スレッドでのpauseでfalseです。
    bool resume() const;
    // 期限内に枠を取得できなければ要求を取り下げ、pausedのままfalseを返します。
    bool resume_for(std::chrono::milliseconds timeout) const;

  private:
    friend struct detail::Runtime;
    JobContext(std::shared_ptr<detail::Runtime>, std::shared_ptr<detail::Entry>);
    std::shared_ptr<detail::Runtime> runtime_;
    std::shared_ptr<detail::Entry> entry_;
};

// runは最初の実行枠取得後、専用スレッドで一度だけ呼びます。
// 優先度への反応と協調キャンセルはJob自身が決めます。
class Job {
  public:
    virtual ~Job() = default;
    virtual JobInfo info() const = 0;
    virtual JobOutcome run(JobContext context) = 0;
};

class JobHandle {
  public:
    JobSnapshot snapshot() const;
    JobSnapshot wait() const;
    std::optional<JobSnapshot> wait_for(std::chrono::milliseconds timeout) const;
    // 未開始Jobは取消。開始済みJobにはinterrupt対応時だけ要求します。
    bool request_cancel() const;
    // 状態と投入順を保って優先度を変更します。終了処理中・終了済みならfalseです。
    bool set_priority(int priority) const;

  private:
    friend class Scheduler;
    JobHandle(std::shared_ptr<detail::Runtime>, std::shared_ptr<detail::Entry>);
    std::shared_ptr<detail::Runtime> runtime_;
    std::shared_ptr<detail::Entry> entry_;
};
} // namespace swayflux3
