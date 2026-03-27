# unillm

`unillm` is a unified C++23 SDK and local proxy for multiple LLM providers.

It gives you one typed API for:

- Chat completions
- Embeddings
- Model listing
- OpenAI-compatible local proxying

## Why use it?

- One SDK for OpenAI, Anthropic, Gemini, and NVIDIA NIM
- Route aliases so application code does not hardcode remote model IDs
- Synchronous and asynchronous interfaces
- Strongly typed request/response structs
- Built-in retry policy for transient failures

## Documentation map

- **Getting Started**: project setup and first request
- **Configuration**: provider setup, routing, retry behavior
- **API Guide**: all public types and how to use them safely
- **Examples**: runnable examples including NVIDIA NIM contact parsing
- **Generated API Reference**: full Doxygen pages from public headers

## Open source checklist

Before publishing:

1. Replace placeholders in `mkdocs.yml`:
   - `site_url`
   - `repo_url`
   - `repo_name`
2. Ensure secrets are not committed (`.env`, API keys).
3. Enable GitHub Pages:
   - Repository `Settings` -> `Pages` -> `Build and deployment` -> `GitHub Actions`.
