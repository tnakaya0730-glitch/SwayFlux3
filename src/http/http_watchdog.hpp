#pragma once
#include <chrono>
#include <stdexcept>

namespace swayflux3::detail {
class HttpWatchdog {
  public:
    using Clock = std::chrono::steady_clock;
    HttpWatchdog(std::chrono::milliseconds total, std::chrono::milliseconds idle)
        : total_(total), idle_(idle), started_(Clock::now()), activity_(started_) {}
    void activity() { activity_ = Clock::now(); }
    void exclude(Clock::duration duration) { activity_ += duration; }
    std::chrono::milliseconds remaining() const {
        if (!total_.count())
            return (std::chrono::milliseconds::max)();
        const auto result =
            total_ - std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started_);
        if (result.count() <= 0)
            throw std::runtime_error("HTTP total timeout exceeded");
        return result;
    }
    void check() const {
        (void)remaining();
        if (idle_.count() && Clock::now() - activity_ >= idle_)
            throw std::runtime_error("HTTP idle timeout exceeded");
    }
    Clock::time_point started() const { return started_; }

  private:
    std::chrono::milliseconds total_, idle_;
    Clock::time_point started_, activity_;
};
} // namespace swayflux3::detail
