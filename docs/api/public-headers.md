# Public Headers

This page maps source headers to generated API pages.

## `include/unillm/unillm.hpp`

Core SDK surface:

- `unillm::UnifiedClient`
- `unillm::UnifiedError`
- `unillm::ClientConfig`, `ProviderConfig`, `ModelRoute`, `RetryPolicy`
- `unillm::ChatRequest`, `ChatResponse`, `EmbeddingRequest`, `EmbeddingResponse`
- `unillm::ModelInfo`, streaming event types, and aliases

Generated links:

- `api/doxygen/unillm_8hpp.html`
- `api/doxygen/unillm_8hpp_source.html`

## `include/unillm/providers.hpp`

Provider metadata surface:

- `unillm::ProviderCapabilities`
- `unillm::ProviderDescriptor`
- `unillm::built_in_provider_catalog()`

Generated links:

- `api/doxygen/providers_8hpp.html`
- `api/doxygen/providers_8hpp_source.html`

## `include/unillm/proxy.hpp`

Proxy API surface:

- `unillm::ProxyServerOptions`
- `unillm::ProxyRequest`, `ProxyResponse`
- `unillm::ProxyApplication`
- `unillm::ProxyServer`

Generated links:

- `api/doxygen/proxy_8hpp.html`
- `api/doxygen/proxy_8hpp_source.html`

## Notes

If your generated file names differ slightly, use `api/doxygen/files.html` to navigate by file.
