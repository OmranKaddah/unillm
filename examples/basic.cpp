#include "unillm/unillm.hpp"

#include <iostream>

int main() {
  unillm::ClientConfig config {
    .providers = {
      {
        .kind = unillm::ProviderKind::OpenAI,
        .name = "openai",
        .api_key = "set-me",
        .base_url = "https://api.openai.com/v1"
      }
    },
    .routes = {
      {
        .alias = "fast-chat",
        .provider_name = "openai",
        .model_name = "gpt-4.1-mini"
      }
    },
    .default_provider = "openai"
  };

  unillm::UnifiedClient client(std::move(config));
  std::cout << "Configured providers: " << client.config().providers.size() << '\n';
  std::cout << "Use client.chat(...), client.embed(...), or run unillm_proxy with a TOML config.\n";
}
