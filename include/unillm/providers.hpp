#pragma once

#include "unillm/unillm.hpp"

/**
 * @file providers.hpp
 * @brief Public provider catalog and capability metadata.
 */
namespace unillm {

/**
 * @brief Feature support flags for a provider adapter.
 */
struct ProviderCapabilities {
  /// Supports chat completion endpoints.
  bool chat {false};
  /// Supports embedding endpoints.
  bool embeddings {false};
  /// Supports model listing endpoints.
  bool model_listing {false};
  /// Supports streaming APIs.
  bool streaming {false};
};

/**
 * @brief Human-readable provider metadata entry.
 */
struct ProviderDescriptor {
  /// Canonical provider label.
  std::string name;
  /// Provider kind enum.
  ProviderKind kind {ProviderKind::OpenAI};
  /// Feature support flags.
  ProviderCapabilities capabilities;
};

/**
 * @brief Return catalog of built-in providers and capabilities.
 */
[[nodiscard]] std::vector<ProviderDescriptor> built_in_provider_catalog();

}  // namespace unillm
