#pragma once
#include "http_response.hpp"
#include "proxy.hpp"
#include <filesystem>
#include <functional>
#include <swayflux3/communication/communication_job.hpp>
#include <vector>

namespace swayflux3 {
enum class UploadMethod { put, post };
enum class UploadCompression { none, gzip, zstd };
enum class ResponseEncoding { automatic, identity, gzip, brotli, zstd };
struct HttpTransferOptions {
    std::string name;
    std::string url;
    TransferDirection direction = TransferDirection::download;
    // ダウンロード先／アップロード元。sink指定時はダウンロード先のpathは不要です。
    std::filesystem::path path;
    std::shared_ptr<DownloadSink> sink;
    UploadMethod upload_method = UploadMethod::put;
    UploadCompression upload_compression = UploadCompression::none;
    int gzip_level = 1;
    int zstd_level = 1;
    bool decode_response = true;
    ResponseEncoding response_encoding = ResponseEncoding::automatic;
    PriorityResponse priority_response = PriorityResponse::continue_running;
    // 0は通常時に無制限。priority_bandwidthは制限方針のとき必須です。
    std::uint64_t bandwidth_limit = 0;
    std::uint64_t priority_bandwidth = 64 * 1024;
    std::size_t buffer_size = 256 * 1024;
    std::optional<std::uint64_t> max_download_bytes;
    std::chrono::milliseconds connect_timeout{30000};
    // 0は全体の時間制限なし。一時停止時間も含みます。
    std::chrono::milliseconds total_timeout{0};
    std::filesystem::path ca_bundle;
    std::vector<std::string> headers;
    bool follow_redirects = true;
    std::size_t max_redirects = 5;
    bool allow_insecure_redirect = false;
    std::optional<HttpRange> range;
    std::size_t max_response_header_bytes = 256 * 1024;
    // 最終ヘッダー受信完了時に呼び、例外で中止できます。
    // 認証再送の可能性がある401/407は、最終結果が分かるまで通知を保留します。
    std::function<void(const HttpResponseInfo &)> on_response;
    // 0は無通信期限なし。意図的な停止・受信先の処理時間は除外します。
    std::chrono::milliseconds idle_timeout{0};
    bool tcp_keepalive = true;
    std::chrono::seconds keepalive_idle{60};
    std::chrono::seconds keepalive_interval{30};
    ProxyOptions proxy;
    std::shared_ptr<ProxyResolver> proxy_resolver;
    RevocationPolicy revocation_policy = RevocationPolicy::strict;
};

// libcurlのmulti APIを専用Jobスレッドで駆動します。
// ファイル全体をメモリーに保持せず、転送と圧縮処理を逐次実行します。
class HttpTransferJob final : public CommunicationJob {
  public:
    explicit HttpTransferJob(HttpTransferOptions options);
    ~HttpTransferJob() override;
    JobInfo info() const override;
    CommunicationInfo communication_info() const override;
    JobOutcome run(JobContext context) override;
    std::shared_ptr<const TransferStatus> status() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<TransferStatus> status_;
    JobOutcome run_transfer(JobContext context);
};
} // namespace swayflux3
