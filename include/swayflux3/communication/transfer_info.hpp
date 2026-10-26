#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace swayflux3 {
enum class TransferDirection { download, upload };
enum class PriorityResponse { continue_running, pause, limit_bandwidth, limit_bandwidth_or_pause };
// transferred_bytesは展開後の受信量、または読み取ったアップロード原本の量です。
// wire_bytesは圧縮後のHTTPボディ量で、HTTP/TLSヘッダーは含みません。
struct CommunicationInfo {
    TransferDirection direction = TransferDirection::download;
    std::uint64_t transferred_bytes = 0;
    std::optional<std::uint64_t> total_bytes;
    std::uint64_t wire_bytes = 0;
    double bytes_per_second = 0;
    std::uint64_t bandwidth_limit = 0;
    long http_status = 0;
};
// JobをSchedulerへmoveした後も、この共有オブジェクトで進捗を取得できます。
class TransferStatus {
  public:
    CommunicationInfo snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return info_;
    }
    // 通信Jobが更新し、利用者にはshared_ptr<const TransferStatus>を渡します。
    void report(CommunicationInfo info) {
        std::lock_guard<std::mutex> lock(mutex_);
        info_ = std::move(info);
    }

  private:
    mutable std::mutex mutex_;
    CommunicationInfo info_;
};

} // namespace swayflux3
