# unillm

`unillm` is a C++23 unified SDK and local proxy for multiple LLM providers.

- Documentation: https://omrankaddah.github.io/unillm/
- API reference (Doxygen): https://omrankaddah.github.io/unillm/api/doxygen/
- Releases: https://github.com/OmranKaddah/unillm/releases

## Features

- Unified client for OpenAI, Anthropic, Gemini, and Nvidia NIM
- Model alias routing with provider-specific normalization
- Async-first SDK via `std::future`
- Streaming events for chat completions
- Local OpenAI-compatible proxy endpoints for chat, embeddings, and models
- Optional C++ module interface units for faster consumer imports
- Header facades retained for non-module consumers

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Enable modules when your compiler/CMake combination supports them:

```bash
cmake -S . -B build -G Ninja -DUNILLM_ENABLE_MODULES=ON
```

`UNILLM_ENABLE_MODULES` currently requires a CMake generator with C++ module support, such as `Ninja`.

## Documentation

This repository includes:

- Detailed API documentation in public headers (`include/unillm/*.hpp`)
- A MkDocs site under `docs/`
- Generated Doxygen API reference embedded into the docs site
- GitHub Actions workflow to publish docs to GitHub Pages

Build docs locally:

```bash
pip install -r docs/requirements.txt
./docs/build_docs.sh
```

Output:

- Static site: `site/`
- Doxygen pages inside the site: `site/api/doxygen/`

Key docs:

- Docs home: `https://omrankaddah.github.io/unillm/`
- API index: `https://omrankaddah.github.io/unillm/api/`
- Generated API from public headers: `https://omrankaddah.github.io/unillm/api/doxygen/`

To publish on GitHub Pages:

1. Set `site_url`, `repo_url`, and `repo_name` in `mkdocs.yml`.
2. Push to `main` (or run the `docs` workflow manually).
3. Enable `Settings -> Pages -> Build and deployment -> GitHub Actions`.

## Releases

Releases are published automatically when a Git tag matching `v*` is pushed.

Example:

```bash
git tag v0.1.0
git push origin v0.1.0
```

Release page:

- `https://github.com/OmranKaddah/unillm/releases`

## Config

`unillm_proxy` loads a TOML file like this:

```toml
[client]
default_provider = "openai"
retry_max_attempts = 3
retry_initial_backoff_ms = 150
retry_max_backoff_ms = 1500

[providers.openai]
kind = "openai"
api_key = "sk-..."
base_url = "https://api.openai.com/v1"
timeout_ms = 30000

[providers.anthropic]
kind = "anthropic"
api_key = "sk-ant-..."
base_url = "https://api.anthropic.com/v1"

[routes.fast-chat]
provider = "openai"
model = "gpt-4.1-mini"
```

Environment variables override file values using `UNILLM_<PROVIDER_NAME>_API_KEY` and `UNILLM_<PROVIDER_NAME>_BASE_URL`.

## Example

```cpp
#include "unillm/unillm.hpp"

int main() {
  auto config = unillm::UnifiedClient::load_config_file("unillm.toml");
  unillm::UnifiedClient client(config);
  auto response = client.chat_sync({
    .model = "fast-chat",
    .messages = {{.role = unillm::Role::User, .content = "Hello"}}
  });
}
```

Nvidia NIM contact parsing example:

```bash
cmake -S . -B build
cmake --build build --target unillm_nim_contact_example
NVIDIA_NIM_API_KEY=nvapi-... ./build/unillm_nim_contact_example "NVIDIA"
```

This example uses `https://integrate.api.nvidia.com/v1` and model
`nemotron-nano-12b-v2-vl`, then parses JSON contact fields from the model output.
