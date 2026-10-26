#include "redirect_controller.hpp"
#include "http_response_parser.hpp"
#include <curl/curl.h>
#include <memory>
#include <stdexcept>

namespace swayflux3::detail {
namespace {
using Url = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;
void set(CURLU *url, CURLUPart part, const char *value, unsigned flags = 0) {
    if (curl_url_set(url, part, value, flags) != CURLUE_OK)
        throw std::invalid_argument("invalid HTTP URL or redirect");
}
std::string get(CURLU *url, CURLUPart part, unsigned flags = 0) {
    char *value = nullptr;
    if (curl_url_get(url, part, &value, flags) != CURLUE_OK)
        return {};
    std::unique_ptr<char, decltype(&curl_free)> owned(value, curl_free);
    return value;
}
UrlInfo describe(CURLU *url) {
    UrlInfo result;
    result.scheme = lower(get(url, CURLUPART_SCHEME));
    if (result.scheme != "http" && result.scheme != "https")
        throw std::invalid_argument("only HTTP and HTTPS URLs are supported");
    set(url, CURLUPART_FRAGMENT, nullptr);
    char *user = nullptr;
    result.has_credentials = curl_url_get(url, CURLUPART_USER, &user, 0) == CURLUE_OK;
    curl_free(user);
    result.username = get(url, CURLUPART_USER, CURLU_URLDECODE);
    result.password = get(url, CURLUPART_PASSWORD, CURLU_URLDECODE);
    result.origin = result.scheme + "://" + lower(get(url, CURLUPART_HOST)) + ":" +
                    std::to_string(std::stoul(get(url, CURLUPART_PORT, CURLU_DEFAULT_PORT)));
    set(url, CURLUPART_USER, nullptr);
    set(url, CURLUPART_PASSWORD, nullptr);
    set(url, CURLUPART_OPTIONS, nullptr);
    result.public_url = get(url, CURLUPART_URL);
    result.url = result.public_url;
    return result;
}
} // namespace
UrlInfo inspect_url(const std::string &value) {
    if (value.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid HTTP URL");
    Url url(curl_url(), curl_url_cleanup);
    if (!url)
        throw std::bad_alloc();
    set(url.get(), CURLUPART_URL, value.c_str());
    return describe(url.get());
}
UrlInfo redirect_url(const UrlInfo &from, const std::string &location, bool allow_insecure) {
    if (location.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid HTTP redirect");
    Url url(curl_url(), curl_url_cleanup);
    if (!url)
        throw std::bad_alloc();
    set(url.get(), CURLUPART_URL, from.url.c_str());
    // 新しく与えられたLocation内のuserinfoを拒否します。
    set(url.get(), CURLUPART_URL, location.c_str(), CURLU_DISALLOW_USER);
    auto target = describe(url.get());
    if (from.scheme == "https" && target.scheme != "https" && !allow_insecure)
        throw std::runtime_error("HTTPS redirect downgrade rejected");
    if (target.origin != from.origin)
        target.url = target.public_url;
    return target;
}
bool redirect_status(long status) noexcept {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}
} // namespace swayflux3::detail
