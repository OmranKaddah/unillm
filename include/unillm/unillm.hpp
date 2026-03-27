#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace unillm {

/**
 * @brief Supported backend provider families.
 */
enum class ProviderKind {
  /// OpenAI-compatible provider (OpenAI REST API style).
  OpenAI,
  /// Anthropic Messages API.
  Anthropic,
  /// Google Gemini REST API.
  Gemini,
  /// NVIDIA NIM endpoints (OpenAI-compatible in this SDK).
  NvidiaNim
};

/**
 * @brief Message role used in chat conversations.
 */
enum class Role {
  /// High-priority behavioral instruction.
  System,
  /// End-user message.
  User,
  /// Assistant/model response.
  Assistant,
  /// Tool/function response message.
  Tool
};

/**
 * @brief Event type emitted by streaming callbacks.
 */
enum class StreamEventType {
  /// Stream started.
  MessageStart,
  /// Incremental token/text delta.
  Delta,
  /// Stream completed.
  MessageStop,
  /// Tool call event.
  ToolCall,
  /// Stream error event.
  Error
};

/**
 * @brief Single chat message payload.
 */
struct Message {
  /// Role of this message. Defaults to `Role::User`.
  Role role {Role::User};
  /// UTF-8 message text.
  std::string content;
  /// Optional participant/tool name.
  std::optional<std::string> name;
};

/**
 * @brief Generic map for provider-specific key/value extensions.
 */
using ExtensionMap = std::unordered_map<std::string, std::string>;

/**
 * @brief Provider-specific request options.
 *
 * Values are serialized as JSON string values by current adapters.
 */
using ProviderOptions = ExtensionMap;

/**
 * @brief Token accounting metadata returned by providers.
 */
struct TokenUsage {
  /// Prompt/input token count.
  std::int32_t prompt_tokens {0};
  /// Completion/output token count.
  std::int32_t completion_tokens {0};
  /// Combined token count.
  std::int32_t total_tokens {0};
};

/**
 * @brief One alternative returned in a chat completion.
 */
struct ChatChoice {
  /// Choice index as returned by provider.
  std::int32_t index {0};
  /// Message content for this choice.
  Message message;
  /// Provider finish reason (for example `stop`, `length`).
  std::string finish_reason;
};

/**
 * @brief Chat completion request.
 */
struct ChatRequest {
  /// Route alias or direct remote model ID.
  std::string model;
  /// Ordered chat messages for the model.
  std::vector<Message> messages;
  /// Optional sampling temperature.
  std::optional<double> temperature;
  /// Optional max generated token limit.
  std::optional<std::int32_t> max_tokens;
  /// Internal stream flag. Prefer `chat_stream*` APIs for streaming.
  bool stream {false};
  /// Provider-specific request fields.
  ProviderOptions provider_options;
};

/**
 * @brief Chat completion response.
 */
struct ChatResponse {
  /// Provider response ID when available.
  std::string id;
  /// Effective remote model ID.
  std::string model;
  /// Configured provider name that handled this request.
  std::string provider;
  /// Returned choices.
  std::vector<ChatChoice> choices;
  /// Token usage metadata.
  TokenUsage usage;
  /// Raw JSON payload returned by provider.
  std::string raw_json;
};

/**
 * @brief Embedding request payload.
 */
struct EmbeddingRequest {
  /// Route alias or direct remote embedding model ID.
  std::string model;
  /// Input strings to embed.
  std::vector<std::string> input;
  /// Provider-specific request fields.
  ProviderOptions provider_options;
};

/**
 * @brief One embedding vector row.
 */
struct EmbeddingData {
  /// Input index.
  std::int32_t index {0};
  /// Embedding vector values.
  std::vector<double> embedding;
};

/**
 * @brief Embedding response payload.
 */
struct EmbeddingResponse {
  /// Effective remote model ID.
  std::string model;
  /// Configured provider name that handled this request.
  std::string provider;
  /// Embedding rows.
  std::vector<EmbeddingData> data;
  /// Token usage metadata when available.
  TokenUsage usage;
  /// Raw JSON payload returned by provider.
  std::string raw_json;
};

/**
 * @brief Provider model catalog entry.
 */
struct ModelInfo {
  /// Provider model identifier.
  std::string id;
  /// Configured provider name.
  std::string provider;
  /// Optional owner string from provider metadata.
  std::optional<std::string> owned_by;
  /// Additional metadata (adapter-dependent).
  ExtensionMap metadata;
};

/**
 * @brief Streaming callback event payload.
 */
struct StreamEvent {
  /// Event kind.
  StreamEventType type {StreamEventType::Delta};
  /// Configured provider name.
  std::string provider;
  /// Effective remote model ID.
  std::string model;
  /// Delta text chunk for `StreamEventType::Delta`.
  std::string delta;
  /// Finish reason for `StreamEventType::MessageStop`.
  std::string finish_reason;
};

/**
 * @brief Exponential backoff retry configuration.
 */
struct RetryPolicy {
  /// Maximum attempts per request (including first attempt).
  std::int32_t max_attempts {2};
  /// Initial delay before first retry.
  std::chrono::milliseconds initial_backoff {150};
  /// Maximum delay cap for exponential backoff.
  std::chrono::milliseconds max_backoff {1500};
};

/**
 * @brief Provider connection and auth configuration.
 */
struct ProviderConfig {
  /// Provider adapter kind.
  ProviderKind kind {ProviderKind::OpenAI};
  /// Local provider name used by routes and responses.
  std::string name;
  /// Provider API key/token.
  std::string api_key;
  /// Provider base URL (for example `https://api.openai.com/v1`).
  std::string base_url;
  /// Optional API version (used by providers that require one).
  std::string api_version;
  /// Request timeout.
  std::chrono::milliseconds timeout {30000};
  /// Extra default headers to send on every request.
  ExtensionMap default_headers;
};

/**
 * @brief Alias route from local model name to provider/model.
 */
struct ModelRoute {
  /// Local alias used in requests.
  std::string alias;
  /// Provider `name` from `ProviderConfig`.
  std::string provider_name;
  /// Remote model ID.
  std::string model_name;
};

/**
 * @brief Top-level client configuration.
 */
struct ClientConfig {
  /// Available provider definitions.
  std::vector<ProviderConfig> providers;
  /// Model alias routes.
  std::vector<ModelRoute> routes;
  /// Fallback provider name when no route alias matches.
  std::string default_provider;
  /// Retry behavior for retryable failures.
  RetryPolicy retry_policy;
};

/**
 * @brief Rich exception type for provider and transport failures.
 */
class UnifiedError : public std::runtime_error {
public:
  /**
   * @brief Construct a unified SDK error.
   * @param provider_name Provider name associated with the failure.
   * @param http_status HTTP status code, or `0` for transport-level errors.
   * @param retryable Whether the error is considered retryable.
   * @param message Human-readable message.
   * @param raw_body Raw HTTP response body when available.
   */
  UnifiedError(
    std::string provider_name,
    std::int32_t http_status,
    bool retryable,
    std::string message,
    std::string raw_body = {}
  );

  /**
   * @brief Name of provider that produced this error.
   */
  [[nodiscard]] const std::string& provider_name() const noexcept;
  /**
   * @brief HTTP status code or `0` if not available.
   */
  [[nodiscard]] std::int32_t http_status() const noexcept;
  /**
   * @brief True if operation may succeed on retry.
   */
  [[nodiscard]] bool retryable() const noexcept;
  /**
   * @brief Raw provider response body (if present).
   */
  [[nodiscard]] const std::string& raw_body() const noexcept;

private:
  std::string provider_name_;
  std::int32_t http_status_;
  bool retryable_;
  std::string raw_body_;
};

/**
 * @brief Callback signature for chat streaming.
 */
using StreamCallback = std::function<void(const StreamEvent&)>;

class IHttpTransport;

/**
 * @brief Unified SDK client for chat, embeddings, and model listing.
 */
class UnifiedClient {
public:
  /**
   * @brief Construct a client with a config and optional custom transport.
   * @param config Fully resolved client configuration.
   * @param transport Optional transport override (useful for tests/proxies).
   */
  explicit UnifiedClient(
    ClientConfig config,
    std::shared_ptr<IHttpTransport> transport = {}
  );

  /**
   * @brief Load a `ClientConfig` from TOML.
   * @param path Path to config file.
   * @return Parsed and env-overridden client config.
   */
  static ClientConfig load_config_file(const std::filesystem::path& path);

  /**
   * @brief Asynchronously send chat completion request.
   * @param request Chat request payload.
   * @return Future containing chat response.
   */
  [[nodiscard]] std::future<ChatResponse> chat(ChatRequest request) const;
  /**
   * @brief Asynchronously stream chat response.
   * @param request Chat request payload.
   * @param callback Stream callback invoked with events.
   * @return Future that resolves when stream finishes.
   */
  [[nodiscard]] std::future<void> chat_stream(ChatRequest request, StreamCallback callback) const;
  /**
   * @brief Asynchronously request embeddings.
   * @param request Embedding request payload.
   * @return Future containing embedding response.
   */
  [[nodiscard]] std::future<EmbeddingResponse> embed(EmbeddingRequest request) const;
  /**
   * @brief Asynchronously list provider models.
   * @param provider_name Optional provider name filter.
   * @return Future containing model entries.
   */
  [[nodiscard]] std::future<std::vector<ModelInfo>> list_models(
    std::optional<std::string> provider_name = std::nullopt
  ) const;

  /**
   * @brief Synchronous chat completion.
   * @param request Chat request payload.
   * @return Chat response payload.
   */
  [[nodiscard]] ChatResponse chat_sync(ChatRequest request) const;
  /**
   * @brief Synchronous streaming chat.
   * @param request Chat request payload.
   * @param callback Stream callback invoked with events.
   */
  void chat_stream_sync(ChatRequest request, StreamCallback callback) const;
  /**
   * @brief Synchronous embedding request.
   * @param request Embedding request payload.
   * @return Embedding response payload.
   */
  [[nodiscard]] EmbeddingResponse embed_sync(EmbeddingRequest request) const;
  /**
   * @brief Synchronous model listing.
   * @param provider_name Optional provider name filter.
   * @return Model entries.
   */
  [[nodiscard]] std::vector<ModelInfo> list_models_sync(
    std::optional<std::string> provider_name = std::nullopt
  ) const;

  /**
   * @brief Access immutable effective client config.
   */
  [[nodiscard]] const ClientConfig& config() const noexcept;

private:
  ClientConfig config_;
  std::shared_ptr<IHttpTransport> transport_;
};

}  // namespace unillm
