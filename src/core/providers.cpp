#include "core/internal/providers.hpp"

#include "unillm/unillm.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <sstream>

namespace unillm {

namespace {

using internal::Json;
using internal::ResolvedRoute;

[[nodiscard]] std::string role_to_string(Role role) {
  switch (role) {
    case Role::System: return "system";
    case Role::User: return "user";
    case Role::Assistant: return "assistant";
    case Role::Tool: return "tool";
  }
  return "user";
}

[[nodiscard]] Role role_from_string(const std::string& value) {
  if (value == "system") {
    return Role::System;
  }
  if (value == "assistant") {
    return Role::Assistant;
  }
  if (value == "tool") {
    return Role::Tool;
  }
  return Role::User;
}

[[nodiscard]] std::unordered_map<std::string, std::string> default_headers(const ProviderConfig& provider) {
  auto headers = provider.default_headers;
  headers.emplace("Content-Type", "application/json");
  if (!provider.api_key.empty()) {
    if (provider.kind == ProviderKind::Anthropic) {
      headers["x-api-key"] = provider.api_key;
      headers["anthropic-version"] = provider.api_version.empty() ? "2023-06-01" : provider.api_version;
    } else if (provider.kind == ProviderKind::Gemini) {
      headers["x-goog-api-key"] = provider.api_key;
    } else {
      headers["Authorization"] = "Bearer " + provider.api_key;
    }
  }
  return headers;
}

[[nodiscard]] Json message_to_json(const Message& message) {
  return Json(internal::make_object({
    {"role", role_to_string(message.role)},
    {"content", message.content}
  }));
}

[[nodiscard]] TokenUsage parse_usage(const Json& value) {
  TokenUsage usage;
  usage.prompt_tokens = value["prompt_tokens"].as_int();
  usage.completion_tokens = value["completion_tokens"].as_int();
  usage.total_tokens = value["total_tokens"].as_int();
  if (usage.total_tokens == 0) {
    usage.total_tokens = usage.prompt_tokens + usage.completion_tokens;
  }
  return usage;
}

[[nodiscard]] std::vector<ModelInfo> parse_model_array(const Json& response, const std::string& provider_name) {
  std::vector<ModelInfo> models;
  const Json& data = response["data"];
  for (const auto& item : data.as_array()) {
    ModelInfo model;
    model.id = item["id"].as_string();
    model.provider = provider_name;
    if (item.contains("owned_by")) {
      model.owned_by = item["owned_by"].as_string();
    }
    models.push_back(std::move(model));
  }
  return models;
}

class OpenAICompatibleAdapter : public internal::IProviderAdapter {
public:
  explicit OpenAICompatibleAdapter(ProviderKind kind) : kind_(kind) {}

  [[nodiscard]] ProviderKind kind() const override {
    return kind_;
  }

  [[nodiscard]] ProviderCapabilities capabilities() const override {
    return {.chat = true, .embeddings = true, .model_listing = true, .streaming = true};
  }

  [[nodiscard]] HttpRequest make_chat_request(
    const ResolvedRoute& route,
    const ChatRequest& request
  ) const override {
    Json::array messages;
    for (const auto& message : request.messages) {
      messages.push_back(message_to_json(message));
    }

    Json::object payload {
      {"model", route.remote_model},
      {"messages", messages},
      {"stream", request.stream}
    };

    if (request.temperature.has_value()) {
      payload["temperature"] = *request.temperature;
    }
    if (request.max_tokens.has_value()) {
      payload["max_tokens"] = *request.max_tokens;
    }
    for (const auto& [key, value] : request.provider_options) {
      payload[key] = value;
    }

    return HttpRequest {
      .method = "POST",
      .url = route.provider.base_url + "/chat/completions",
      .headers = default_headers(route.provider),
      .body = Json(payload).dump(),
      .timeout_ms = route.provider.timeout.count()
    };
  }

  [[nodiscard]] ChatResponse parse_chat_response(
    const ResolvedRoute& route,
    const HttpResponse& response
  ) const override {
    const Json json = Json::parse(response.body);
    ChatResponse chat;
    chat.id = json["id"].as_string();
    chat.model = json["model"].as_string();
    chat.provider = route.provider_label;
    chat.usage = parse_usage(json["usage"]);
    chat.raw_json = response.body;

    for (const auto& choice : json["choices"].as_array()) {
      chat.choices.push_back(ChatChoice {
        .index = choice["index"].as_int(),
        .message = Message {
          .role = role_from_string(choice["message"]["role"].as_string()),
          .content = choice["message"]["content"].as_string()
        },
        .finish_reason = choice["finish_reason"].as_string()
      });
    }
    return chat;
  }

  [[nodiscard]] HttpRequest make_embeddings_request(
    const ResolvedRoute& route,
    const EmbeddingRequest& request
  ) const override {
    Json::array input;
    for (const auto& item : request.input) {
      input.push_back(item);
    }

    Json::object payload {
      {"model", route.remote_model},
      {"input", input}
    };
    for (const auto& [key, value] : request.provider_options) {
      payload[key] = value;
    }

    return HttpRequest {
      .method = "POST",
      .url = route.provider.base_url + "/embeddings",
      .headers = default_headers(route.provider),
      .body = Json(payload).dump(),
      .timeout_ms = route.provider.timeout.count()
    };
  }

  [[nodiscard]] EmbeddingResponse parse_embeddings_response(
    const ResolvedRoute& route,
    const HttpResponse& response
  ) const override {
    const Json json = Json::parse(response.body);
    EmbeddingResponse embedding;
    embedding.model = json["model"].as_string();
    embedding.provider = route.provider_label;
    embedding.usage = parse_usage(json["usage"]);
    embedding.raw_json = response.body;

    for (const auto& item : json["data"].as_array()) {
      EmbeddingData row {.index = item["index"].as_int()};
      for (const auto& value : item["embedding"].as_array()) {
        row.embedding.push_back(value.as_number());
      }
      embedding.data.push_back(std::move(row));
    }
    return embedding;
  }

  [[nodiscard]] HttpRequest make_models_request(const ProviderConfig& provider) const override {
    return HttpRequest {
      .method = "GET",
      .url = provider.base_url + "/models",
      .headers = default_headers(provider),
      .body = {},
      .timeout_ms = provider.timeout.count()
    };
  }

  [[nodiscard]] std::vector<ModelInfo> parse_models_response(
    const ProviderConfig& provider,
    const HttpResponse& response
  ) const override {
    return parse_model_array(Json::parse(response.body), provider.name);
  }

private:
  ProviderKind kind_;
};

class AnthropicAdapter final : public internal::IProviderAdapter {
public:
  [[nodiscard]] ProviderKind kind() const override {
    return ProviderKind::Anthropic;
  }

  [[nodiscard]] ProviderCapabilities capabilities() const override {
    return {.chat = true, .embeddings = false, .model_listing = true, .streaming = true};
  }

  [[nodiscard]] HttpRequest make_chat_request(
    const ResolvedRoute& route,
    const ChatRequest& request
  ) const override {
    Json::array messages;
    for (const auto& message : request.messages) {
      Json::object block {
        {"role", role_to_string(message.role)},
        {"content", Json::array {Json(internal::make_object({
          {"type", "text"},
          {"text", message.content}
        }))}}
      };
      messages.push_back(Json(std::move(block)));
    }

    Json::object payload {
      {"model", route.remote_model},
      {"messages", messages},
      {"stream", request.stream},
      {"max_tokens", request.max_tokens.value_or(512)}
    };
    if (request.temperature.has_value()) {
      payload["temperature"] = *request.temperature;
    }
    return HttpRequest {
      .method = "POST",
      .url = route.provider.base_url + "/messages",
      .headers = default_headers(route.provider),
      .body = Json(payload).dump(),
      .timeout_ms = route.provider.timeout.count()
    };
  }

  [[nodiscard]] ChatResponse parse_chat_response(
    const ResolvedRoute& route,
    const HttpResponse& response
  ) const override {
    const Json json = Json::parse(response.body);
    ChatResponse chat;
    chat.id = json["id"].as_string();
    chat.model = json["model"].as_string();
    chat.provider = route.provider_label;
    chat.raw_json = response.body;
    chat.usage.prompt_tokens = json["usage"]["input_tokens"].as_int();
    chat.usage.completion_tokens = json["usage"]["output_tokens"].as_int();
    chat.usage.total_tokens = chat.usage.prompt_tokens + chat.usage.completion_tokens;

    std::string text;
    for (const auto& block : json["content"].as_array()) {
      if (block["type"].as_string() == "text") {
        text += block["text"].as_string();
      }
    }

    chat.choices.push_back(ChatChoice {
      .index = 0,
      .message = Message {.role = Role::Assistant, .content = text},
      .finish_reason = json["stop_reason"].as_string()
    });
    return chat;
  }

  [[nodiscard]] HttpRequest make_embeddings_request(
    const ResolvedRoute& route,
    const EmbeddingRequest&
  ) const override {
    throw UnifiedError(route.provider_label, 400, false, "Anthropic embeddings are not supported in this SDK");
  }

  [[nodiscard]] EmbeddingResponse parse_embeddings_response(
    const ResolvedRoute& route,
    const HttpResponse&
  ) const override {
    throw UnifiedError(route.provider_label, 400, false, "Anthropic embeddings are not supported in this SDK");
  }

  [[nodiscard]] HttpRequest make_models_request(const ProviderConfig& provider) const override {
    return HttpRequest {
      .method = "GET",
      .url = provider.base_url + "/models",
      .headers = default_headers(provider),
      .body = {},
      .timeout_ms = provider.timeout.count()
    };
  }

  [[nodiscard]] std::vector<ModelInfo> parse_models_response(
    const ProviderConfig& provider,
    const HttpResponse& response
  ) const override {
    return parse_model_array(Json::parse(response.body), provider.name);
  }
};

class GeminiAdapter final : public internal::IProviderAdapter {
public:
  [[nodiscard]] ProviderKind kind() const override {
    return ProviderKind::Gemini;
  }

  [[nodiscard]] ProviderCapabilities capabilities() const override {
    return {.chat = true, .embeddings = true, .model_listing = true, .streaming = true};
  }

  [[nodiscard]] HttpRequest make_chat_request(
    const ResolvedRoute& route,
    const ChatRequest& request
  ) const override {
    Json::array contents;
    for (const auto& message : request.messages) {
      contents.push_back(Json(internal::make_object({
        {"role", role_to_string(message.role)},
        {"parts", Json::array {Json(internal::make_object({{"text", message.content}}))}}
      })));
    }

    Json::object payload {
      {"contents", contents}
    };

    Json::object generation_config;
    if (request.temperature.has_value()) {
      generation_config["temperature"] = *request.temperature;
    }
    if (request.max_tokens.has_value()) {
      generation_config["maxOutputTokens"] = *request.max_tokens;
    }
    if (!generation_config.empty()) {
      payload["generationConfig"] = Json(std::move(generation_config));
    }

    return HttpRequest {
      .method = "POST",
      .url = route.provider.base_url + "/models/" + route.remote_model + ":generateContent",
      .headers = default_headers(route.provider),
      .body = Json(payload).dump(),
      .timeout_ms = route.provider.timeout.count()
    };
  }

  [[nodiscard]] ChatResponse parse_chat_response(
    const ResolvedRoute& route,
    const HttpResponse& response
  ) const override {
    const Json json = Json::parse(response.body);
    ChatResponse chat;
    chat.id = "gemini-" + route.remote_model;
    chat.model = route.remote_model;
    chat.provider = route.provider_label;
    chat.raw_json = response.body;

    std::string text;
    const auto& parts = json["candidates"][0]["content"]["parts"].as_array();
    for (const auto& part : parts) {
      if (part.contains("text")) {
        text += part["text"].as_string();
      }
    }

    chat.usage.prompt_tokens = json["usageMetadata"]["promptTokenCount"].as_int();
    chat.usage.completion_tokens = json["usageMetadata"]["candidatesTokenCount"].as_int();
    chat.usage.total_tokens = json["usageMetadata"]["totalTokenCount"].as_int();
    chat.choices.push_back(ChatChoice {
      .index = 0,
      .message = Message {.role = Role::Assistant, .content = text},
      .finish_reason = json["candidates"][0]["finishReason"].as_string()
    });
    return chat;
  }

  [[nodiscard]] HttpRequest make_embeddings_request(
    const ResolvedRoute& route,
    const EmbeddingRequest& request
  ) const override {
    Json::array requests;
    for (const auto& item : request.input) {
      requests.push_back(Json(internal::make_object({
        {"model", "models/" + route.remote_model},
        {"content", Json(internal::make_object({
          {"parts", Json::array {Json(internal::make_object({{"text", item}}))}}
        }))}
      })));
    }

    return HttpRequest {
      .method = "POST",
      .url = route.provider.base_url + "/models/" + route.remote_model + ":batchEmbedContents",
      .headers = default_headers(route.provider),
      .body = Json(internal::make_object({{"requests", requests}})).dump(),
      .timeout_ms = route.provider.timeout.count()
    };
  }

  [[nodiscard]] EmbeddingResponse parse_embeddings_response(
    const ResolvedRoute& route,
    const HttpResponse& response
  ) const override {
    const Json json = Json::parse(response.body);
    EmbeddingResponse embedding;
    embedding.model = route.remote_model;
    embedding.provider = route.provider_label;
    embedding.raw_json = response.body;

    int index = 0;
    for (const auto& item : json["embeddings"].as_array()) {
      EmbeddingData data {.index = index++};
      for (const auto& value : item["values"].as_array()) {
        data.embedding.push_back(value.as_number());
      }
      embedding.data.push_back(std::move(data));
    }
    return embedding;
  }

  [[nodiscard]] HttpRequest make_models_request(const ProviderConfig& provider) const override {
    return HttpRequest {
      .method = "GET",
      .url = provider.base_url + "/models",
      .headers = default_headers(provider),
      .body = {},
      .timeout_ms = provider.timeout.count()
    };
  }

  [[nodiscard]] std::vector<ModelInfo> parse_models_response(
    const ProviderConfig& provider,
    const HttpResponse& response
  ) const override {
    return parse_model_array(Json::parse(response.body), provider.name);
  }
};

}  // namespace

namespace internal {

void IProviderAdapter::stream_chat(
  const ResolvedRoute& route,
  const ChatRequest& request,
  const std::shared_ptr<IHttpTransport>& transport,
  StreamCallback callback
) const {
  ChatRequest stream_request = request;
  stream_request.stream = false;
  HttpRequest http_request = make_chat_request(route, stream_request);
  HttpResponse response = transport->send(http_request);
  if (response.status_code >= 400) {
    throw UnifiedError(
      route.provider_label,
      static_cast<std::int32_t>(response.status_code),
      response.status_code == 429 || response.status_code >= 500,
      "Provider returned an error for streaming request",
      response.body
    );
  }
  const ChatResponse chat = parse_chat_response(route, response);

  callback(StreamEvent {
    .type = StreamEventType::MessageStart,
    .provider = route.provider_label,
    .model = chat.model,
    .delta = {}
  });

  if (!chat.choices.empty()) {
    std::istringstream stream(chat.choices.front().message.content);
    std::string token;
    while (stream >> token) {
      callback(StreamEvent {
        .type = StreamEventType::Delta,
        .provider = route.provider_label,
        .model = chat.model,
        .delta = token + " "
      });
    }
    callback(StreamEvent {
      .type = StreamEventType::MessageStop,
      .provider = route.provider_label,
      .model = chat.model,
      .delta = {},
      .finish_reason = chat.choices.front().finish_reason
    });
  }
}

std::shared_ptr<IProviderAdapter> make_provider_adapter(ProviderKind kind) {
  switch (kind) {
    case ProviderKind::OpenAI:
      return std::make_shared<OpenAICompatibleAdapter>(ProviderKind::OpenAI);
    case ProviderKind::Anthropic:
      return std::make_shared<AnthropicAdapter>();
    case ProviderKind::Gemini:
      return std::make_shared<GeminiAdapter>();
    case ProviderKind::NvidiaNim:
      return std::make_shared<OpenAICompatibleAdapter>(ProviderKind::NvidiaNim);
  }
  throw std::runtime_error("Unsupported provider kind");
}

ResolvedRoute resolve_route(const ClientConfig& config, const std::string& requested_model) {
  for (const auto& route : config.routes) {
    if (route.alias != requested_model) {
      continue;
    }
    auto provider_it = std::find_if(config.providers.begin(), config.providers.end(), [&](const ProviderConfig& provider) {
      return provider.name == route.provider_name;
    });
    if (provider_it == config.providers.end()) {
      throw std::runtime_error("Route references unknown provider: " + route.provider_name);
    }
    return ResolvedRoute {
      .provider = *provider_it,
      .provider_label = provider_it->name,
      .remote_model = route.model_name
    };
  }

  const std::string provider_name = config.default_provider;
  auto provider_it = std::find_if(config.providers.begin(), config.providers.end(), [&](const ProviderConfig& provider) {
    return provider.name == provider_name;
  });
  if (provider_it == config.providers.end()) {
    throw std::runtime_error("No route for model and no valid default provider: " + requested_model);
  }
  return ResolvedRoute {
    .provider = *provider_it,
    .provider_label = provider_it->name,
    .remote_model = requested_model
  };
}

}  // namespace internal

std::vector<ProviderDescriptor> built_in_provider_catalog() {
  return {
    {.name = "openai", .kind = ProviderKind::OpenAI, .capabilities = {.chat = true, .embeddings = true, .model_listing = true, .streaming = true}},
    {.name = "anthropic", .kind = ProviderKind::Anthropic, .capabilities = {.chat = true, .embeddings = false, .model_listing = true, .streaming = true}},
    {.name = "gemini", .kind = ProviderKind::Gemini, .capabilities = {.chat = true, .embeddings = true, .model_listing = true, .streaming = true}},
    {.name = "nvidia_nim", .kind = ProviderKind::NvidiaNim, .capabilities = {.chat = true, .embeddings = true, .model_listing = true, .streaming = true}}
  };
}

}  // namespace unillm
