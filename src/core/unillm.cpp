#include "unillm/unillm.hpp"

#include "core/internal/http.hpp"
#include "core/internal/providers.hpp"
#include "core/internal/toml.hpp"

#include <algorithm>
#include <thread>

namespace unillm {

namespace {

[[nodiscard]] bool should_retry(const HttpResponse& response) {
  return response.status_code == 429 || response.status_code >= 500;
}

[[nodiscard]] bool should_retry(const UnifiedError& error) {
  return error.retryable();
}

template <typename Operation>
auto with_retries(
  const ClientConfig& config,
  const ProviderConfig& provider,
  Operation&& operation
) -> decltype(operation()) {
  std::chrono::milliseconds backoff = config.retry_policy.initial_backoff;
  for (int attempt = 1; ; ++attempt) {
    try {
      return operation();
    } catch (const UnifiedError& error) {
      const bool retry = should_retry(error) && attempt < config.retry_policy.max_attempts;
      if (!retry) {
        throw;
      }
      std::this_thread::sleep_for(backoff);
      backoff = std::min(backoff * 2, config.retry_policy.max_backoff);
    } catch (...) {
      if (attempt >= config.retry_policy.max_attempts) {
        throw;
      }
      std::this_thread::sleep_for(backoff);
      backoff = std::min(backoff * 2, config.retry_policy.max_backoff);
    }
  }
}

}  // namespace

UnifiedError::UnifiedError(
  std::string provider_name,
  std::int32_t http_status,
  bool retryable,
  std::string message,
  std::string raw_body
) :
  std::runtime_error(std::move(message)),
  provider_name_(std::move(provider_name)),
  http_status_(http_status),
  retryable_(retryable),
  raw_body_(std::move(raw_body)) {}

const std::string& UnifiedError::provider_name() const noexcept {
  return provider_name_;
}

std::int32_t UnifiedError::http_status() const noexcept {
  return http_status_;
}

bool UnifiedError::retryable() const noexcept {
  return retryable_;
}

const std::string& UnifiedError::raw_body() const noexcept {
  return raw_body_;
}

UnifiedClient::UnifiedClient(
  ClientConfig config,
  std::shared_ptr<IHttpTransport> transport
) :
  config_(std::move(config)),
  transport_(transport ? std::move(transport) : make_default_transport()) {}

ClientConfig UnifiedClient::load_config_file(const std::filesystem::path& path) {
  return internal::load_toml_config(path);
}

std::future<ChatResponse> UnifiedClient::chat(ChatRequest request) const {
  return std::async(std::launch::async, [this, request = std::move(request)]() mutable {
    return chat_sync(std::move(request));
  });
}

std::future<void> UnifiedClient::chat_stream(ChatRequest request, StreamCallback callback) const {
  return std::async(std::launch::async, [this, request = std::move(request), callback = std::move(callback)]() mutable {
    chat_stream_sync(std::move(request), std::move(callback));
  });
}

std::future<EmbeddingResponse> UnifiedClient::embed(EmbeddingRequest request) const {
  return std::async(std::launch::async, [this, request = std::move(request)]() mutable {
    return embed_sync(std::move(request));
  });
}

std::future<std::vector<ModelInfo>> UnifiedClient::list_models(
  std::optional<std::string> provider_name
) const {
  return std::async(std::launch::async, [this, provider_name = std::move(provider_name)]() mutable {
    return list_models_sync(std::move(provider_name));
  });
}

ChatResponse UnifiedClient::chat_sync(ChatRequest request) const {
  const auto route = internal::resolve_route(config_, request.model);
  const auto adapter = internal::make_provider_adapter(route.provider.kind);

  return with_retries(config_, route.provider, [&]() {
    HttpRequest http_request = adapter->make_chat_request(route, request);
    HttpResponse response = transport_->send(http_request);
    if (response.status_code >= 400) {
      throw UnifiedError(
        route.provider_label,
        static_cast<std::int32_t>(response.status_code),
        should_retry(response),
        "Provider returned an error for chat request",
        response.body
      );
    }
    return adapter->parse_chat_response(route, response);
  });
}

void UnifiedClient::chat_stream_sync(ChatRequest request, StreamCallback callback) const {
  const auto route = internal::resolve_route(config_, request.model);
  const auto adapter = internal::make_provider_adapter(route.provider.kind);
  request.stream = true;
  adapter->stream_chat(route, request, transport_, std::move(callback));
}

EmbeddingResponse UnifiedClient::embed_sync(EmbeddingRequest request) const {
  const auto route = internal::resolve_route(config_, request.model);
  const auto adapter = internal::make_provider_adapter(route.provider.kind);

  return with_retries(config_, route.provider, [&]() {
    HttpRequest http_request = adapter->make_embeddings_request(route, request);
    HttpResponse response = transport_->send(http_request);
    if (response.status_code >= 400) {
      throw UnifiedError(
        route.provider_label,
        static_cast<std::int32_t>(response.status_code),
        should_retry(response),
        "Provider returned an error for embeddings request",
        response.body
      );
    }
    return adapter->parse_embeddings_response(route, response);
  });
}

std::vector<ModelInfo> UnifiedClient::list_models_sync(std::optional<std::string> provider_name) const {
  std::vector<ModelInfo> models;
  for (const auto& provider : config_.providers) {
    if (provider_name && *provider_name != provider.name) {
      continue;
    }
    const auto adapter = internal::make_provider_adapter(provider.kind);
    auto fetch = [&]() {
      HttpRequest request = adapter->make_models_request(provider);
      HttpResponse response = transport_->send(request);
      if (response.status_code >= 400) {
        throw UnifiedError(
          provider.name,
          static_cast<std::int32_t>(response.status_code),
          should_retry(response),
          "Provider returned an error for models request",
          response.body
        );
      }
      return adapter->parse_models_response(provider, response);
    };

    auto provider_models = with_retries(config_, provider, fetch);
    models.insert(models.end(), provider_models.begin(), provider_models.end());
  }
  return models;
}

const ClientConfig& UnifiedClient::config() const noexcept {
  return config_;
}

}  // namespace unillm
