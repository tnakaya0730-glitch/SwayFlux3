#include "http_response_parser.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <stdexcept>

namespace swayflux3::detail {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}
namespace {
std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}
std::uint64_t number(std::string_view value) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::runtime_error("invalid numeric HTTP response header");
    return result;
}
void unique(std::string &destination, std::string_view value) {
    if (!destination.empty() && destination != value)
        throw std::runtime_error("conflicting HTTP response headers");
    destination = std::string(value);
}
} // namespace
void HttpResponseParser::reset(std::string url) {
    info = {};
    info.url = std::move(url);
    location.clear();
    complete = false;
    bytes_ = 0;
    transfer_encoding_ = false;
}
bool HttpResponseParser::push(std::string_view line, std::size_t limit) {
    if (line.size() > limit || bytes_ > limit - line.size())
        throw std::runtime_error("HTTP response headers exceed limit");
    bytes_ += line.size();
    if (line.size() >= 2 && line.substr(line.size() - 2) == "\r\n")
        line.remove_suffix(2);
    else if (!line.empty() && line.back() == '\n')
        line.remove_suffix(1);
    if (line.find_first_of(std::string_view("\r\n\0", 3)) != std::string_view::npos)
        throw std::runtime_error("invalid HTTP response header");
    if (line.substr(0, 5) == "HTTP/") {
        const auto space = line.find(' ');
        if (space == std::string_view::npos)
            throw std::runtime_error("invalid HTTP status");
        auto status_text = trim(line.substr(space + 1));
        const auto end = status_text.find(' ');
        const auto status = number(status_text.substr(0, end));
        if (status < 100 || status > 599)
            throw std::runtime_error("invalid HTTP status");
        auto url = info.url;
        info = {};
        info.url = std::move(url);
        info.status = static_cast<long>(status);
        complete = false;
        location.clear();
        transfer_encoding_ = false;
        return false;
    }
    if (complete)
        return false; // トレーラーは応答メタデータを上書きしません。
    if (line.empty()) {
        if (transfer_encoding_ && info.content_length)
            throw std::runtime_error("ambiguous HTTP body framing");
        complete = true;
        return true;
    }
    const auto colon = line.find(':');
    if (colon == std::string_view::npos)
        throw std::runtime_error("invalid HTTP response header");
    const auto name = lower(std::string(line.substr(0, colon)));
    const auto value = trim(line.substr(colon + 1));
    if (name == "content-length") {
        const auto size = number(value);
        if (info.content_length && *info.content_length != size)
            throw std::runtime_error("conflicting Content-Length");
        info.content_length = size;
    } else if (name == "content-range")
        unique(info.content_range, value);
    else if (name == "etag")
        unique(info.etag, value);
    else if (name == "location")
        unique(location, value);
    else if (name == "content-encoding") {
        if (!info.content_encoding.empty())
            info.content_encoding += ", ";
        info.content_encoding += lower(std::string(value));
    } else if (name == "transfer-encoding")
        transfer_encoding_ = true;
    return false;
}
ParsedContentRange parse_content_range(const std::string &value) {
    const auto view = std::string_view(value);
    const auto dash = view.find('-'), slash = view.find('/');
    if (lower(value.substr(0, 6)) != "bytes " || dash == std::string_view::npos ||
        slash == std::string_view::npos || dash >= slash)
        throw std::runtime_error("invalid Content-Range");
    ParsedContentRange result{number(view.substr(6, dash - 6)),
                              number(view.substr(dash + 1, slash - dash - 1)), std::nullopt};
    if (view.substr(slash + 1) != "*")
        result.total = number(view.substr(slash + 1));
    if (result.first > result.last || (result.total && *result.total <= result.last))
        throw std::runtime_error("invalid Content-Range");
    return result;
}
} // namespace swayflux3::detail
