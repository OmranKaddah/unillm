#include "core/internal/http.hpp"
#include "unillm/proxy.hpp"

#include <cassert>
#include <iostream>
#include <memory>

namespace {

class ProxyMockTransport final : public unillm::IHttpTransport {
public:
  [[nodiscard]] unillm::HttpResponse send(const unillm::HttpRequest& request) const override {
    if (request.url.find("/models") != std::string::npos) {
      return {.status_code = 200, .body = R"({"data":[{"id":"gpt-4.1-mini","owned_by":"openai"}]})"};
    }
    if (request.url.find("/embeddings") != std::string::npos) {
      return {.status_code = 200, .body = R"({"model":"text-embedding-3-small","data":[{"index":0,"embedding":[0.1,0.2]}],"usage":{"prompt_tokens":1,"completion_tokens":0,"total_tokens":1}})"};
    }
    return {.status_code = 200, .body = R"({"id":"chat-1","model":"gpt-4.1-mini","choices":[{"index":0,"message":{"role":"assistant","content":"Proxy response"},"finish_reason":"stop"}],"usage":{"prompt_tokens":2,"completion_tokens":2,"total_tokens":4}})"};
  }
};

unillm::UnifiedClient make_client() {
  unillm::ClientConfig config {
    .providers = {
      {.kind = unillm::ProviderKind::OpenAI, .name = "openai", .api_key = "k1", .base_url = "https://openai.example"}
    },
    .routes = {
      {.alias = "fast-chat", .provider_name = "openai", .model_name = "gpt-4.1-mini"},
      {.alias = "embed", .provider_name = "openai", .model_name = "text-embedding-3-small"}
    },
    .default_provider = "openai"
  };
  return unillm::UnifiedClient(std::move(config), std::make_shared<ProxyMockTransport>());
}

}  // namespace

int main() {
  unillm::ProxyApplication app(make_client());

  const auto models = app.handle_request({.method = "GET", .target = "/v1/models"});
  assert(models.status == 200);
  assert(models.body.find("gpt-4.1-mini") != std::string::npos);

  const auto embeddings = app.handle_request({
    .method = "POST",
    .target = "/v1/embeddings",
    .body = R"({"model":"embed","input":["hello"]})"
  });
  assert(embeddings.status == 200);
  assert(embeddings.body.find("\"embedding\"") != std::string::npos);

  const auto chat = app.handle_request({
    .method = "POST",
    .target = "/v1/chat/completions",
    .body = R"({"model":"fast-chat","messages":[{"role":"user","content":"hi"}]})"
  });
  assert(chat.status == 200);
  assert(chat.body.find("Proxy response") != std::string::npos);

  const auto stream = app.handle_request({
    .method = "POST",
    .target = "/v1/chat/completions",
    .body = R"({"model":"fast-chat","stream":true,"messages":[{"role":"user","content":"hi"}]})"
  });
  assert(stream.status == 200);
  assert(stream.content_type == "text/event-stream");
  assert(stream.body.find("data: [DONE]") != std::string::npos);

  std::cout << "unillm_proxy_tests passed\n";
}
