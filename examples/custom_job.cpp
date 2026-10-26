#include <iostream>
#include <swayflux3/scheduler.hpp>

// 高優先度Jobが来ても続行する、最小の独自Jobです。
class MyJob final : public swayflux3::Job {
  public:
    swayflux3::JobInfo info() const override {
        swayflux3::JobInfo result;
        result.name = "独自Job";
        return result;
    }
    swayflux3::JobOutcome run(swayflux3::JobContext context) override {
        std::cout << "Job " << context.id() << "\n";
        return swayflux3::JobOutcome::completed;
    }
};
int main() {
    swayflux3::Scheduler scheduler(2);
    auto handle = scheduler.enqueue(std::make_unique<MyJob>(), 10);
    const auto result = handle.wait();
    scheduler.shutdown();
    return result.state == swayflux3::JobState::completed ? 0 : 1;
}
