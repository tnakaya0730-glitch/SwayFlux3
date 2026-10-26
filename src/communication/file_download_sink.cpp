#include "file_download_sink.hpp"
#include <stdexcept>

namespace swayflux3::detail {
FileDownloadSink::FileDownloadSink(const std::filesystem::path &path, std::size_t buffer_size)
    : buffer_(buffer_size) {
#ifdef _WIN32
    if (_wfopen_s(&file_, path.c_str(), L"wb") != 0)
        file_ = nullptr;
#else
    file_ = std::fopen(path.c_str(), "wb");
#endif
    if (!file_)
        throw std::runtime_error("cannot open download destination");
    if (std::setvbuf(file_, buffer_.data(), _IOFBF, buffer_.size()) != 0) {
        std::fclose(file_);
        file_ = nullptr;
        throw std::runtime_error("cannot set download buffer");
    }
}

FileDownloadSink::~FileDownloadSink() {
    if (file_)
        std::fclose(file_);
}

void FileDownloadSink::write(const char *data, std::size_t size) {
    if (std::fwrite(data, 1, size, file_) != size)
        throw std::runtime_error("download write failed");
}

void FileDownloadSink::finish() {
    if (std::fflush(file_) != 0)
        throw std::runtime_error("download flush failed");
}
} // namespace swayflux3::detail
