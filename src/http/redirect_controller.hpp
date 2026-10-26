#pragma once
#include <string>

namespace swayflux3::detail {
struct UrlInfo {
    std::string url, origin, scheme, public_url, username, password;
    bool has_credentials = false;
};
UrlInfo inspect_url(const std::string &url);
UrlInfo redirect_url(const UrlInfo &from, const std::string &location, bool allow_insecure);
bool redirect_status(long status) noexcept;
} // namespace swayflux3::detail
