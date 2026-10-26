#include "upload_stream.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <zlib.h>
#include <zstd.h>

namespace swayflux3::detail {
struct UploadStream::Impl {
    UploadCodec format;
    std::ifstream file;
    std::vector<char> buffer;
    std::uint64_t consumed = 0;
    std::uint64_t total = 0;
    bool eof = false;
    bool finished = false;
    z_stream gzip{};
    bool gzip_initialized = false;
    ZSTD_CStream *zstd = nullptr;
    int zstd_level;
    ZSTD_inBuffer zstd_input{nullptr, 0, 0};
    explicit Impl(const UploadStreamOptions &o)
        : format(o.codec), buffer(o.buffer_size), zstd_level(o.zstd_level) {
        file.open(o.path, std::ios::binary);
        if (!file)
            throw std::runtime_error("cannot open upload source");
        total = std::filesystem::file_size(o.path);
        if (format == UploadCodec::gzip) {
            if (deflateInit2(&gzip, o.gzip_level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
                throw std::runtime_error("gzip initialization failed");
            gzip_initialized = true;
        } else if (format == UploadCodec::zstd) {
            zstd = ZSTD_createCStream();
            if (!zstd)
                throw std::bad_alloc();
            const auto result = ZSTD_initCStream(zstd, o.zstd_level);
            if (ZSTD_isError(result)) {
                ZSTD_freeCStream(zstd);
                zstd = nullptr;
                throw std::runtime_error(ZSTD_getErrorName(result));
            }
        }
    }
    ~Impl() {
        if (gzip_initialized)
            deflateEnd(&gzip);
        if (zstd)
            ZSTD_freeCStream(zstd);
    }
    void rewind() {
        // 同じ原本ストリームを再利用し、圧縮済みバイト位置ではなく先頭へ戻します。
        file.clear();
        file.seekg(0, std::ios::end);
        const auto size = file.tellg();
        if (size < 0 || static_cast<std::uint64_t>(size) != total)
            throw std::runtime_error("upload file size changed");
        file.seekg(0, std::ios::beg);
        if (!file)
            throw std::runtime_error("cannot rewind upload source");
        if (gzip_initialized) {
            if (deflateReset(&gzip) != Z_OK)
                throw std::runtime_error("gzip reset failed");
            gzip.next_in = nullptr;
            gzip.avail_in = 0;
        }
        if (zstd) {
            const auto result = ZSTD_initCStream(zstd, zstd_level);
            if (ZSTD_isError(result))
                throw std::runtime_error(ZSTD_getErrorName(result));
            zstd_input = {nullptr, 0, 0};
        }
        consumed = 0;
        eof = finished = false;
    }
    std::size_t read_source(char *data, std::size_t size) {
        file.read(data, static_cast<std::streamsize>(size));
        const auto bytes = static_cast<std::size_t>(file.gcount());
        if (file.bad() || (file.fail() && !file.eof()))
            throw std::runtime_error("upload read failed");
        eof = file.eof();
        consumed += bytes;
        if (consumed > total || (eof && consumed != total))
            throw std::runtime_error("upload file size changed");
        return bytes;
    }
    std::size_t read(char *output, std::size_t capacity) {
        if (finished)
            return 0;
        if (format == UploadCodec::none)
            return read_source(output, (std::min)(capacity, buffer.size()));
        const auto before = consumed;
        if (format == UploadCodec::zstd) {
            ZSTD_outBuffer out{output, capacity, 0};
            while (out.pos < out.size && !finished) {
                if (zstd_input.pos == zstd_input.size && !eof) {
                    zstd_input = {buffer.data(), read_source(buffer.data(), buffer.size()), 0};
                }
                const auto result =
                    ZSTD_compressStream2(zstd, &out, &zstd_input, eof ? ZSTD_e_end : ZSTD_e_continue);
                if (ZSTD_isError(result))
                    throw std::runtime_error(ZSTD_getErrorName(result));
                finished = eof && result == 0;
                if (out.pos && consumed - before >= 4 * 1024 * 1024)
                    break;
            }
            return out.pos;
        }
        gzip.next_out = reinterpret_cast<Bytef *>(output);
        gzip.avail_out = static_cast<uInt>(capacity);
        while (gzip.avail_out && !finished) {
            if (!gzip.avail_in && !eof) {
                gzip.avail_in = static_cast<uInt>(read_source(buffer.data(), buffer.size()));
                gzip.next_in = reinterpret_cast<Bytef *>(buffer.data());
            }
            const auto result = deflate(&gzip, eof ? Z_FINISH : Z_NO_FLUSH);
            if (result == Z_STREAM_END)
                finished = true;
            else if (result != Z_OK)
                throw std::runtime_error("gzip compression failed");
            if (gzip.avail_out < capacity && consumed - before >= 4 * 1024 * 1024)
                break;
        }
        return capacity - gzip.avail_out;
    }
};
UploadStream::UploadStream(const UploadStreamOptions &o) : impl_(std::make_unique<Impl>(o)) {}
UploadStream::~UploadStream() = default;
std::size_t UploadStream::read(char *data, std::size_t size) { return impl_->read(data, size); }
void UploadStream::rewind() { impl_->rewind(); }
std::uint64_t UploadStream::consumed() const noexcept { return impl_->consumed; }
std::uint64_t UploadStream::total() const noexcept { return impl_->total; }
} // namespace swayflux3::detail
