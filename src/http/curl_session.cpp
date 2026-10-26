#include "curl_session.hpp"
#include <limits>
#include <new>
#include <stdexcept>

namespace swayflux3::detail {
void check(CURLcode code) {
    if (code != CURLE_OK)
        throw std::runtime_error(curl_easy_strerror(code));
}
void check_multi(CURLMcode code) {
    if (code != CURLM_OK)
        throw std::runtime_error(curl_multi_strerror(code));
}
struct CurlGlobal {
    CurlGlobal() { check(curl_global_init(CURL_GLOBAL_DEFAULT)); }
    ~CurlGlobal() { curl_global_cleanup(); }
};
void init_curl() {
    static CurlGlobal global;
    (void)global;
}

long milliseconds(std::chrono::milliseconds value) {
    if (value.count() < 0 || value.count() > (std::numeric_limits<long>::max)())
        throw std::invalid_argument("timeout out of range");
    return static_cast<long>(value.count());
}
curl_off_t curl_size(std::uint64_t value) {
    if (value > static_cast<std::uint64_t>((std::numeric_limits<curl_off_t>::max)()))
        throw std::invalid_argument("size exceeds curl_off_t");
    return static_cast<curl_off_t>(value);
}
CurlSession::~CurlSession() {
    if (added)
        curl_multi_remove_handle(multi, easy);
    if (easy)
        curl_easy_cleanup(easy);
    if (multi)
        curl_multi_cleanup(multi);
    if (headers)
        curl_slist_free_all(headers);
}
void CurlSession::header(const std::string &value) {
    auto *next = curl_slist_append(headers, value.c_str());
    if (!next)
        throw std::bad_alloc();
    headers = next;
}
void CurlSession::reset_request() {
    if (added) {
        check_multi(curl_multi_remove_handle(multi, easy));
        added = false;
    }
    curl_easy_reset(easy);
    curl_slist_free_all(headers);
    headers = nullptr;
}
} // namespace swayflux3::detail
