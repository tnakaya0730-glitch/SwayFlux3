#include "http_proxy.hpp"
#include <stdexcept>

namespace swayflux3::detail {
void validate_proxy(const ProxyOptions &o) {
    if (o.mode == ProxyMode::explicit_proxy && o.url.empty())
        throw std::invalid_argument("explicit proxy requires URL");
    if (o.url.find_first_of("\r\n") != std::string::npos || o.url.find('\0') != std::string::npos ||
        o.username.find('\0') != std::string::npos || o.password.find('\0') != std::string::npos ||
        (o.no_proxy && (o.no_proxy->find_first_of("\r\n") != std::string::npos ||
                        o.no_proxy->find('\0') != std::string::npos)))
        throw std::invalid_argument("invalid proxy configuration");
}
void configure_proxy(CurlSession &curl, const ProxyOptions &o) {
    validate_proxy(o);
    if (o.mode == ProxyMode::direct)
        check(curl_easy_setopt(curl.easy, CURLOPT_PROXY, ""));
    else if (o.mode == ProxyMode::explicit_proxy)
        check(curl_easy_setopt(curl.easy, CURLOPT_PROXY, o.url.c_str()));
    if (o.no_proxy)
        check(curl_easy_setopt(curl.easy, CURLOPT_NOPROXY, o.no_proxy->c_str()));
    if (!o.username.empty())
        check(curl_easy_setopt(curl.easy, CURLOPT_PROXYUSERNAME, o.username.c_str()));
    if (!o.password.empty())
        check(curl_easy_setopt(curl.easy, CURLOPT_PROXYPASSWORD, o.password.c_str()));
    long authentication = CURLAUTH_ANY;
    switch (o.authentication) {
    case ProxyAuthentication::any:
        break;
    case ProxyAuthentication::basic:
        authentication = CURLAUTH_BASIC;
        break;
    case ProxyAuthentication::digest:
        authentication = CURLAUTH_DIGEST;
        break;
    case ProxyAuthentication::negotiate:
        authentication = CURLAUTH_NEGOTIATE;
        break;
    case ProxyAuthentication::ntlm:
        authentication = CURLAUTH_NTLM;
        break;
    }
    check(curl_easy_setopt(curl.easy, CURLOPT_PROXYAUTH, authentication));
}
} // namespace swayflux3::detail
