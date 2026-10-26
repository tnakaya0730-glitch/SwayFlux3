#include <iostream>
#include <swayflux3/swayflux3.hpp>

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "transfer download|upload URL PATH\n";
        return 2;
    }
    try {
        swayflux3::HttpTransferOptions options;
        const std::string command = argv[1];
        if (command != "download" && command != "upload")
            return 2;
        options.direction = command == "upload" ? swayflux3::TransferDirection::upload
                                                : swayflux3::TransferDirection::download;
        options.url = argv[2];
        options.path = std::filesystem::u8path(argv[3]);
        options.priority_response = swayflux3::PriorityResponse::limit_bandwidth;
        // サーバーが対応している場合は、upload_compressionへgzipまたはzstdを指定できます。
        auto transfer = std::make_unique<swayflux3::HttpTransferJob>(std::move(options));
        const auto progress = transfer->status();
        swayflux3::Scheduler scheduler(4);
        auto handle = scheduler.enqueue(std::move(transfer), 10);
        const auto result = handle.wait();
        scheduler.shutdown();
        if (result.state != swayflux3::JobState::completed) {
            std::cerr << result.error << '\n';
            return 1;
        }
        std::cout << progress->snapshot().transferred_bytes << " bytes\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
