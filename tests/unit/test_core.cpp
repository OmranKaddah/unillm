#include "core/internal/http.hpp"
#include "unillm/providers.hpp"
#include "unillm/unillm.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace {

class MockTransport final : public unillm::IHttpTransport {
public:
  mutable std::unordered_map<std::string, int> calls;

  [[nodiscard]] unillm::HttpResponse send(const unillm::HttpRequest& request) const override {
    ++calls[request.url];
    if (request.url.find("retry.example/chat/completions") != std::string::npos && calls[request.url] == 1) {
      return {.status_code = 503, .body = R"({"error":"try again"})"};
    }

    if (request.url.find("openai.example") != std::string::npos) {
      if (request.url.find("/chat/completions") != std::string::npos) {
        return {.status_code = 200, .body = R"({"id":"chat-openai","model":"gpt-4.1-mini","choices":[{"index":0,"message":{"role":"assistant","content":"OpenAI reply"},"finish_reason":"stop"}],"usage":{"prompt_tokens":4,"completion_tokens":3,"total_tokens":7}})"};
      }
      if (request.url.find("/embeddings") != std::string::npos) {
        return {.status_code = 200, .body = R"({"model":"text-embedding-3-small","data":[{"index":0,"embedding":[0.1,0.2,0.3]}],"usage":{"prompt_tokens":2,"completion_tokens":0,"total_tokens":2}})"};
      }
      return {.status_code = 200, .body = R"({"data":[{"id":"gpt-4.1-mini","owned_by":"openai"}]})"};
    }

    if (request.url.find("anthropic.example") != std::string::npos) {
      if (request.url.find("/messages") != std::string::npos) {
        return {.status_code = 200, .body = R"({"id":"msg-123","model":"claude-3-7-sonnet","content":[{"type":"text","text":"Anthropic reply"}],"stop_reason":"end_turn","usage":{"input_tokens":10,"output_tokens":5}})"};
      }
      return {.status_code = 200, .body = R"({"data":[{"id":"claude-3-7-sonnet","owned_by":"anthropic"}]})"};
    }

    if (request.url.find("gemini.example") != std::string::npos) {
      if (request.url.find(":generateContent") != std::string::npos) {
        return {.status_code = 200, .body = R"({"candidates":[{"content":{"parts":[{"text":"Gemini reply"}]},"finishReason":"STOP"}],"usageMetadata":{"promptTokenCount":6,"candidatesTokenCount":4,"totalTokenCount":10}})"};
      }
      if (request.url.find(":batchEmbedContents") != std::string::npos) {
        return {.status_code = 200, .body = R"({"embeddings":[{"values":[0.9,0.8]},{"values":[0.7,0.6]}]})"};
      }
      return {.status_code = 200, .body = R"({"data":[{"id":"gemini-2.0-flash","owned_by":"google"}]})"};
    }

    if (request.url.find("nim.example") != std::string::npos) {
      if (request.url.find("/chat/completions") != std::string::npos) {
        return {.status_code = 200, .body = R"({"id":"chat-nim","model":"meta/llama-3.1-70b-instruct","choices":[{"index":0,"message":{"role":"assistant","content":"NIM reply"},"finish_reason":"stop"}],"usage":{"prompt_tokens":8,"completion_tokens":2,"total_tokens":10}})"};
      }
      if (request.url.find("/embeddings") != std::string::npos) {
        return {.status_code = 200, .body = R"({"model":"nvidia/nv-embedqa","data":[{"index":0,"embedding":[0.4,0.5]}],"usage":{"prompt_tokens":3,"completion_tokens":0,"total_tokens":3}})"};
      }
      return {.status_code = 200, .body = R"({"data":[{"id":"meta/llama-3.1-70b-instruct","owned_by":"nvidia"}]})"};
    }

    if (request.url.find("retry.example") != std::string::npos) {
      if (request.url.find("/models") != std::string::npos) {
        return {.status_code = 200, .body = R"({"data":[{"id":"retry-model","owned_by":"retry"}]})"};
      }
      return {.status_code = 200, .body = R"({"id":"chat-retry","model":"retry-model","choices":[{"index":0,"message":{"role":"assistant","content":"Recovered"},"finish_reason":"stop"}],"usage":{"prompt_tokens":1,"completion_tokens":1,"total_tokens":2}})"};
    }

    return {.status_code = 404, .body = R"({"error":"unknown"})"};
  }
};

unillm::ClientConfig make_config() {
  return {
    .providers = {
      {.kind = unillm::ProviderKind::OpenAI, .name = "openai", .api_key = "k1", .base_url = "https://openai.example"},
      {.kind = unillm::ProviderKind::Anthropic, .name = "anthropic", .api_key = "k2", .base_url = "https://anthropic.example"},
      {.kind = unillm::ProviderKind::Gemini, .name = "gemini", .api_key = "k3", .base_url = "https://gemini.example"},
      {.kind = unillm::ProviderKind::NvidiaNim, .name = "nim", .api_key = "k4", .base_url = "https://nim.example"},
      {.kind = unillm::ProviderKind::OpenAI, .name = "retry", .api_key = "k5", .base_url = "https://retry.example"}
    },
    .routes = {
      {.alias = "fast-chat", .provider_name = "openai", .model_name = "gpt-4.1-mini"},
      {.alias = "claude", .provider_name = "anthropic", .model_name = "claude-3-7-sonnet"},
      {.alias = "flash", .provider_name = "gemini", .model_name = "gemini-2.0-flash"},
      {.alias = "nim-chat", .provider_name = "nim", .model_name = "meta/llama-3.1-70b-instruct"},
      {.alias = "embed-openai", .provider_name = "openai", .model_name = "text-embedding-3-small"},
      {.alias = "embed-gemini", .provider_name = "gemini", .model_name = "gemini-embedding-001"},
      {.alias = "embed-nim", .provider_name = "nim", .model_name = "nvidia/nv-embedqa"},
      {.alias = "retry-chat", .provider_name = "retry", .model_name = "retry-model"}
    },
    .default_provider = "openai",
    .retry_policy = {.max_attempts = 2, .initial_backoff = std::chrono::milliseconds(1), .max_backoff = std::chrono::milliseconds(2)}
  };
}

void test_chat_paths(const std::shared_ptr<MockTransport>& transport) {
  unillm::UnifiedClient client(make_config(), transport);

  auto openai = client.chat_sync({.model = "fast-chat", .messages = {{.role = unillm::Role::User, .content = "Hi"}}});
  assert(openai.provider == "openai");
  assert(openai.choices.front().message.content == "OpenAI reply");

  auto anthropic = client.chat_sync({.model = "claude", .messages = {{.role = unillm::Role::User, .content = "Hi"}}});
  assert(anthropic.provider == "anthropic");
  assert(anthropic.choices.front().message.content == "Anthropic reply");

  auto gemini = client.chat_sync({.model = "flash", .messages = {{.role = unillm::Role::User, .content = "Hi"}}});
  assert(gemini.provider == "gemini");
  assert(gemini.choices.front().message.content == "Gemini reply");

  auto nim = client.chat_sync({.model = "nim-chat", .messages = {{.role = unillm::Role::User, .content = "Hi"}}});
  assert(nim.provider == "nim");
  assert(nim.choices.front().message.content == "NIM reply");
}

void test_embeddings(const std::shared_ptr<MockTransport>& transport) {
  unillm::UnifiedClient client(make_config(), transport);

  auto openai = client.embed_sync({.model = "embed-openai", .input = {"hello"}});
  assert(openai.data.size() == 1);
  assert(openai.data.front().embedding.size() == 3);

  auto gemini = client.embed_sync({.model = "embed-gemini", .input = {"one", "two"}});
  assert(gemini.data.size() == 2);

  auto nim = client.embed_sync({.model = "embed-nim", .input = {"hello"}});
  assert(nim.data.front().embedding.front() == 0.4);

  bool threw = false;
  try {
    (void)client.embed_sync({.model = "claude", .input = {"hello"}});
  } catch (const unillm::UnifiedError&) {
    threw = true;
  }
  assert(threw);
}

void test_models_and_catalog(const std::shared_ptr<MockTransport>& transport) {
  unillm::UnifiedClient client(make_config(), transport);
  auto models = client.list_models_sync();
  assert(models.size() == 5);

  auto catalog = unillm::built_in_provider_catalog();
  assert(catalog.size() == 4);
  assert(!catalog[1].capabilities.embeddings);
}

void test_retry(const std::shared_ptr<MockTransport>& transport) {
  unillm::UnifiedClient client(make_config(), transport);
  auto response = client.chat_sync({.model = "retry-chat", .messages = {{.role = unillm::Role::User, .content = "retry"}}});
  assert(response.choices.front().message.content == "Recovered");
  assert(transport->calls["https://retry.example/chat/completions"] == 2);
}

void test_streaming(const std::shared_ptr<MockTransport>& transport) {
  unillm::UnifiedClient client(make_config(), transport);
  std::string text;
  bool saw_done = false;
  client.chat_stream_sync(
    {.model = "fast-chat", .messages = {{.role = unillm::Role::User, .content = "stream"}}},
    [&](const unillm::StreamEvent& event) {
      if (event.type == unillm::StreamEventType::Delta) {
        text += event.delta;
      }
      if (event.type == unillm::StreamEventType::MessageStop) {
        saw_done = true;
      }
    }
  );
  assert(text.find("OpenAI") != std::string::npos);
  assert(saw_done);
}

void test_toml_loader() {
  const auto path = std::filesystem::temp_directory_path() / "unillm_test.toml";
  std::ofstream out(path);
  out << R"(
[client]
default_provider = "openai"
retry_max_attempts = 3

[providers.openai]
kind = "openai"
api_key = "from_file"
base_url = "https://openai.example"
timeout_ms = 1234
header.x-test = "1"

[routes.fast-chat]
provider = "openai"
model = "gpt-4.1-mini"
)";
  out.close();

  ::setenv("UNILLM_OPENAI_API_KEY", "from_env", 1);
  const auto config = unillm::UnifiedClient::load_config_file(path);
  assert(config.providers.size() == 1);
  assert(config.providers.front().api_key == "from_env");
  assert(config.routes.front().model_name == "gpt-4.1-mini");
}

}  // namespace

int main() {
  test_chat_paths(std::make_shared<MockTransport>());
  test_embeddings(std::make_shared<MockTransport>());
  test_models_and_catalog(std::make_shared<MockTransport>());
  test_retry(std::make_shared<MockTransport>());
  test_streaming(std::make_shared<MockTransport>());
  test_toml_loader();
  std::cout << "unillm_core_tests passed\n";
}
