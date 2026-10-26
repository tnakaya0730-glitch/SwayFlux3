#pragma once
#include "job.hpp"

namespace swayflux3 {
class Scheduler {
  public:
    explicit Scheduler(std::size_t max_concurrent_jobs);
    // 新規受付を止めます。既存Jobは継続します。待機にはshutdownを使います。
    ~Scheduler();
    Scheduler(const Scheduler &) = delete;
    Scheduler &operator=(const Scheduler &) = delete;
    JobHandle enqueue(std::unique_ptr<Job> job, int priority = 0);
    void close();
    void wait_idle() const;
    bool wait_idle_for(std::chrono::milliseconds timeout) const;
    void shutdown();

  private:
    std::shared_ptr<detail::Runtime> runtime_;
};
} // namespace swayflux3
