#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace unillm {

struct HttpRequest {
  std::string method;
  std::string url;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
  std::optional<long> timeout_ms;
};

struct HttpResponse {
  long status_code {0};
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

class IHttpTransport {
public:
  virtual ~IHttpTransport() = default;
  [[nodiscard]] virtual HttpResponse send(const HttpRequest& request) const = 0;
};

std::shared_ptr<IHttpTransport> make_default_transport();

}  // namespace unillm
