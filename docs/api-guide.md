# API Guide

This section summarizes every public API surface in the SDK.

## Core client

- `unillm::UnifiedClient`
  - `chat`, `chat_sync`
  - `chat_stream`, `chat_stream_sync`
  - `embed`, `embed_sync`
  - `list_models`, `list_models_sync`
  - `load_config_file`
  - `config`

## Chat types

- `Role`
- `Message`
- `ChatRequest`
- `ChatChoice`
- `ChatResponse`
- `StreamEventType`
- `StreamEvent`
- `StreamCallback`

## Embedding types

- `EmbeddingRequest`
- `EmbeddingData`
- `EmbeddingResponse`

## Models/catalog

- `ModelInfo`
- `ProviderKind`
- `ProviderCapabilities`
- `ProviderDescriptor`
- `built_in_provider_catalog`

## Configuration types

- `ProviderConfig`
- `ModelRoute`
- `ClientConfig`
- `RetryPolicy`
- `ExtensionMap`
- `ProviderOptions`

## Proxy APIs

- `ProxyServerOptions`
- `ProxyRequest`
- `ProxyResponse`
- `ProxyApplication`
- `ProxyServer`

## Error handling

- `UnifiedError`
  - `provider_name()`
  - `http_status()`
  - `retryable()`
  - `raw_body()`

For field-level and parameter-level details, see the generated Doxygen pages.
