#pragma once
#include <cstddef>

namespace swayflux3 {
// 受信チャンクの受け渡し口。HTTPや点群形式には依存しません。
// write中だけdataが有効です。遅いwriteは送受信へバックプレッシャーをかけます。
class DownloadSink {
  public:
    virtual ~DownloadSink() = default;
    virtual void write(const char *data, std::size_t size) = 0;
    virtual void finish() {}
};

} // namespace swayflux3
