#pragma once
#include <string_view>
#include <swayflux3/http/http_response.hpp>

namespace swayflux3::detail {
std::string lower(std::string value);
struct ParsedContentRange {
    std::uint64_t first, last;
    std::optional<std::uint64_t> total;
};
ParsedContentRange parse_content_range(const std::string &value);
class HttpResponseParser {
  public:
    void reset(std::string url);
    // 1行を受け取り、応答ヘッダーの末尾ならtrueを返します。
    bool push(std::string_view line, std::size_t limit);
    HttpResponseInfo info;
    std::string location;
    bool complete = false;

  private:
    std::size_t bytes_ = 0;
    bool transfer_encoding_ = false;
};
} // namespace swayflux3::detail
