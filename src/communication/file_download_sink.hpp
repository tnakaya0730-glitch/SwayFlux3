#pragma once
#include <cstdio>
#include <filesystem>
#include <swayflux3/communication/download_sink.hpp>
#include <vector>

namespace swayflux3::detail {
class FileDownloadSink final : public DownloadSink {
  public:
    FileDownloadSink(const std::filesystem::path &path, std::size_t buffer_size);
    ~FileDownloadSink() override;
    void write(const char *data, std::size_t size) override;
    void finish() override;

  private:
    std::vector<char> buffer_;
    std::FILE *file_ = nullptr;
};
} // namespace swayflux3::detail
