#pragma once

#include "unillm/unillm.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace unillm::internal {

inline std::string trim(std::string value) {
  auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  return value;
}

inline std::string strip_quotes(std::string value) {
  value = trim(std::move(value));
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

inline ProviderKind parse_provider_kind(const std::string& value) {
  if (value == "openai") {
    return ProviderKind::OpenAI;
  }
  if (value == "anthropic") {
    return ProviderKind::Anthropic;
  }
  if (value == "gemini") {
    return ProviderKind::Gemini;
  }
  if (value == "nvidia_nim" || value == "nim" || value == "nvidia") {
    return ProviderKind::NvidiaNim;
  }
  throw std::runtime_error("Unknown provider kind: " + value);
}

inline std::string uppercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    if (std::isalnum(c)) {
      return static_cast<char>(std::toupper(c));
    }
    return '_';
  });
  return value;
}

inline ClientConfig load_toml_config(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Unable to open config file: " + path.string());
  }

  ClientConfig config;
  ProviderConfig* current_provider = nullptr;
  ModelRoute* current_route = nullptr;
  std::string section;

  std::string line;
  while (std::getline(in, line)) {
    const auto comment = line.find('#');
    if (comment != std::string::npos) {
      line.erase(comment);
    }
    line = trim(std::move(line));
    if (line.empty()) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      section = line.substr(1, line.size() - 2);
      current_provider = nullptr;
      current_route = nullptr;

      if (section.rfind("providers.", 0) == 0) {
        config.providers.push_back({});
        current_provider = &config.providers.back();
        current_provider->name = section.substr(std::string("providers.").size());
      } else if (section.rfind("routes.", 0) == 0) {
        config.routes.push_back({});
        current_route = &config.routes.back();
        current_route->alias = section.substr(std::string("routes.").size());
      }
      continue;
    }

    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }

    const std::string key = trim(line.substr(0, equals));
    const std::string raw_value = trim(line.substr(equals + 1));
    const std::string value = strip_quotes(raw_value);

    if (section == "client") {
      if (key == "default_provider") {
        config.default_provider = value;
      } else if (key == "retry_max_attempts") {
        config.retry_policy.max_attempts = std::stoi(value);
      } else if (key == "retry_initial_backoff_ms") {
        config.retry_policy.initial_backoff = std::chrono::milliseconds(std::stoi(value));
      } else if (key == "retry_max_backoff_ms") {
        config.retry_policy.max_backoff = std::chrono::milliseconds(std::stoi(value));
      }
      continue;
    }

    if (current_provider != nullptr) {
      if (key == "kind") {
        current_provider->kind = parse_provider_kind(value);
      } else if (key == "api_key") {
        current_provider->api_key = value;
      } else if (key == "base_url") {
        current_provider->base_url = value;
      } else if (key == "api_version") {
        current_provider->api_version = value;
      } else if (key == "timeout_ms") {
        current_provider->timeout = std::chrono::milliseconds(std::stoi(value));
      } else if (key.rfind("header.", 0) == 0) {
        current_provider->default_headers[key.substr(std::string("header.").size())] = value;
      }
      continue;
    }

    if (current_route != nullptr) {
      if (key == "provider") {
        current_route->provider_name = value;
      } else if (key == "model") {
        current_route->model_name = value;
      }
      continue;
    }
  }

  for (auto& provider : config.providers) {
    const std::string api_key_env = "UNILLM_" + uppercase(provider.name) + "_API_KEY";
    const std::string base_url_env = "UNILLM_" + uppercase(provider.name) + "_BASE_URL";
    if (const char* env_value = std::getenv(api_key_env.c_str())) {
      provider.api_key = env_value;
    }
    if (const char* env_value = std::getenv(base_url_env.c_str())) {
      provider.base_url = env_value;
    }
  }

  return config;
}

}  // namespace unillm::internal
