#pragma once
#include <memory>
#include <optional>
#include <string>

namespace swayflux3 {
enum class ProxyMode { environment, direct, explicit_proxy };
enum class ProxyAuthentication { any, basic, digest, negotiate, ntlm };
struct ProxyOptions {
    ProxyMode mode = ProxyMode::environment;
    std::string url;
    std::string username;
    std::string password;
    std::optional<std::string> no_proxy;
    ProxyAuthentication authentication = ProxyAuthentication::any;
};
// Jobスレッドで要求ごとに呼びます。共有する実装の同期は利用側で行います。
// WindowsのPAC等はこのインターフェースの実装として分離できます。
class ProxyResolver {
  public:
    virtual ~ProxyResolver() = default;
    virtual ProxyOptions resolve(const std::string &url) = 0;
};
enum class RevocationPolicy { strict, best_effort };
} // namespace swayflux3
