# Getting Started

## Requirements

- CMake `>= 3.28`
- C++23 compiler
- `libcurl` development package

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run basic example

```bash
./build/unillm_example
```

## First real chat call

```cpp
#include "unillm/unillm.hpp"

int main() {
  unillm::ClientConfig config{
    .providers = {{
      .kind = unillm::ProviderKind::OpenAI,
      .name = "openai",
      .api_key = "sk-...",
      .base_url = "https://api.openai.com/v1"
    }},
    .routes = {{
      .alias = "fast-chat",
      .provider_name = "openai",
      .model_name = "gpt-4.1-mini"
    }},
    .default_provider = "openai"
  };

  unillm::UnifiedClient client(std::move(config));
  const auto response = client.chat_sync({
    .model = "fast-chat",
    .messages = {
      {.role = unillm::Role::User, .content = "Hello from unillm"}
    }
  });
}
```
