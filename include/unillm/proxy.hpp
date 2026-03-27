#pragma once

#include "unillm/unillm.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace unillm {

/**
 * @brief Runtime options for the local HTTP proxy server.
 */
struct ProxyServerOptions {
  /// Bind host/IP.
  std::string host {"127.0.0.1"};
  /// Bind TCP port.
  std::uint16_t port {8080};
};

/**
 * @brief Simplified proxy request envelope.
 */
struct ProxyRequest {
  /// HTTP method (for example `GET`, `POST`).
  std::string method;
  /// Request target path (for example `/v1/chat/completions`).
  std::string target;
  /// HTTP headers.
  std::unordered_map<std::string, std::string> headers;
  /// Request body.
  std::string body;
};

/**
 * @brief Simplified proxy response envelope.
 */
struct ProxyResponse {
  /// HTTP status code.
  std::int32_t status {200};
  /// MIME content type header value.
  std::string content_type {"application/json"};
  /// Additional response headers.
  std::unordered_map<std::string, std::string> headers;
  /// Response body.
  std::string body;
};

/**
 * @brief In-process proxy request handler (no socket server).
 */
class ProxyApplication {
public:
  /**
   * @brief Construct handler with a configured client.
   */
  explicit ProxyApplication(UnifiedClient client);

  /**
   * @brief Process one proxy request and return response.
   * @param request Request envelope.
   */
  [[nodiscard]] ProxyResponse handle_request(const ProxyRequest& request) const;

private:
  UnifiedClient client_;
};

/**
 * @brief Minimal OpenAI-compatible HTTP proxy server.
 */
class ProxyServer {
public:
  /**
   * @brief Construct server with client and bind options.
   */
  ProxyServer(UnifiedClient client, ProxyServerOptions options = {});

  /**
   * @brief Run blocking server loop.
   */
  void run();

private:
  UnifiedClient client_;
  ProxyServerOptions options_;
};

}  // namespace unillm
