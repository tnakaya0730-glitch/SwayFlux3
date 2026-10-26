#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace swayflux3::detail {
enum class UploadCodec { none, gzip, zstd };
struct UploadStreamOptions {
    std::filesystem::path path;
    UploadCodec codec = UploadCodec::none;
    std::size_t buffer_size = 256 * 1024;
    int gzip_level = 1;
    int zstd_level = 1;
};
// HTTPの駆動から分離した、原本の読み取りとストリーミング圧縮です。
class UploadStream {
  public:
    explicit UploadStream(const UploadStreamOptions &options);
    ~UploadStream();
    std::size_t read(char *output, std::size_t capacity);
    void rewind();
    std::uint64_t consumed() const noexcept;
    std::uint64_t total() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace swayflux3::detail
