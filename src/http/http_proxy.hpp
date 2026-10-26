#pragma once
#include "curl_session.hpp"
#include <swayflux3/http/proxy.hpp>

namespace swayflux3::detail {
void validate_proxy(const ProxyOptions &options);
void configure_proxy(CurlSession &curl, const ProxyOptions &options);
} // namespace swayflux3::detail
