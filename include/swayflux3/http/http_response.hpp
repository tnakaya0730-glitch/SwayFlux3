#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace swayflux3 {
struct HttpRange {
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
};
struct HttpResponseInfo {
    long status = 0;
    // HTTP表現の長さです。自動展開後の長さとは限りません。
    std::optional<std::uint64_t> content_length;
    std::string content_range;
    std::string etag;
    std::string content_encoding;
    // 認証用のuserinfoを取り除いた最終URLです。
    std::string url;
};
} // namespace swayflux3
