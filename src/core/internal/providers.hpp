#pragma once

#include "core/internal/http.hpp"
#include "core/internal/json.hpp"
#include "unillm/providers.hpp"

#include <memory>

namespace unillm::internal {

struct ResolvedRoute {
  ProviderConfig provider;
  std::string provider_label;
  std::string remote_model;
};

class IProviderAdapter {
public:
  virtual ~IProviderAdapter() = default;

  [[nodiscard]] virtual ProviderKind kind() const = 0;
  [[nodiscard]] virtual ProviderCapabilities capabilities() const = 0;
  [[nodiscard]] virtual HttpRequest make_chat_request(
    const ResolvedRoute& route,
    const ChatRequest& request
  ) const = 0;
  [[nodiscard]] virtual ChatResponse parse_chat_response(
    const ResolvedRoute& route,
    const HttpResponse& response
  ) const = 0;
  [[nodiscard]] virtual HttpRequest make_embeddings_request(
    const ResolvedRoute& route,
    const EmbeddingRequest& request
  ) const = 0;
  [[nodiscard]] virtual EmbeddingResponse parse_embeddings_response(
    const ResolvedRoute& route,
    const HttpResponse& response
  ) const = 0;
  [[nodiscard]] virtual HttpRequest make_models_request(const ProviderConfig& provider) const = 0;
  [[nodiscard]] virtual std::vector<ModelInfo> parse_models_response(
    const ProviderConfig& provider,
    const HttpResponse& response
  ) const = 0;

  virtual void stream_chat(
    const ResolvedRoute& route,
    const ChatRequest& request,
    const std::shared_ptr<IHttpTransport>& transport,
    StreamCallback callback
  ) const;
};

[[nodiscard]] std::shared_ptr<IProviderAdapter> make_provider_adapter(ProviderKind kind);
[[nodiscard]] ResolvedRoute resolve_route(const ClientConfig& config, const std::string& requested_model);

}  // namespace unillm::internal
