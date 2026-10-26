#pragma once
#include <chrono>
#include <cstdint>
#include <curl/curl.h>
#include <string>

namespace swayflux3::detail {
void check(CURLcode code);
void check_multi(CURLMcode code);
void init_curl();
long milliseconds(std::chrono::milliseconds value);
curl_off_t curl_size(std::uint64_t value);
struct CurlSession {
    CURL *easy = curl_easy_init();
    CURLM *multi = curl_multi_init();
    curl_slist *headers = nullptr;
    bool added = false;
    ~CurlSession();
    void header(const std::string &value);
    void reset_request();
};
} // namespace swayflux3::detail
