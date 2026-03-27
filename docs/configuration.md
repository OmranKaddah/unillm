# Configuration

`unillm` is configured with `unillm::ClientConfig`.

## ProviderConfig

Each provider requires:

- `kind`: `OpenAI`, `Anthropic`, `Gemini`, or `NvidiaNim`
- `name`: local name used by routes
- `api_key`: secret token
- `base_url`: provider base endpoint

Optional:

- `api_version`
- `timeout`
- `default_headers`

## ModelRoute

`ModelRoute` maps a stable alias to a provider/model pair:

- `alias`: app-facing model alias (`"fast-chat"`)
- `provider_name`: provider `name`
- `model_name`: remote model ID

This lets your app stay stable when model IDs change.

## RetryPolicy

`RetryPolicy` applies to sync and async operations:

- `max_attempts`
- `initial_backoff`
- `max_backoff`

Retryable failures include HTTP `429` and `5xx`.

## TOML Config

You can load TOML with:

```cpp
auto config = unillm::UnifiedClient::load_config_file("unillm.toml");
```

Environment overrides:

- `UNILLM_<PROVIDER_NAME>_API_KEY`
- `UNILLM_<PROVIDER_NAME>_BASE_URL`
