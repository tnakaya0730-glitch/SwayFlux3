#include <iostream>
#include <swayflux3/http_transfer.hpp>
#include <swayflux3/scheduler.hpp>

// 解析やファイル保存を含まない、逐次受信と計測だけのサンプルです。
class Counter final : public swayflux3::DownloadSink {
  public:
    using Clock = std::chrono::steady_clock;
    Clock::time_point started = Clock::now();
    double first_chunk_ms = 0;
    std::uint64_t bytes = 0;
    void write(const char *, std::size_t size) override {
        if (!bytes)
            first_chunk_ms = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        bytes += size;
    }
};
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "stream_download URL\n";
        return 2;
    }
    try {
        auto sink = std::make_shared<Counter>();
        swayflux3::HttpTransferOptions options;
        options.url = argv[1];
        options.sink = sink;
        swayflux3::Scheduler scheduler(4);
        auto transfer = std::make_unique<swayflux3::HttpTransferJob>(options);
        auto progress = transfer->status();
        auto handle = scheduler.enqueue(std::move(transfer));
        auto result = handle.wait();
        scheduler.shutdown();
        if (result.state != swayflux3::JobState::completed) {
            std::cerr << result.error << '\n';
            return 1;
        }
        auto seconds = std::chrono::duration<double>(Counter::Clock::now() - sink->started).count();
        std::cout << "first_chunk_ms=" << sink->first_chunk_ms << " decoded_bytes=" << sink->bytes
                  << " wire_bytes=" << progress->snapshot().wire_bytes << " elapsed_s=" << seconds
                  << " decoded_MiB_per_s="
                  << (seconds > 0 ? static_cast<double>(sink->bytes) / (1024 * 1024) / seconds : 0) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
