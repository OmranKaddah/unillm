#include "unillm/unillm.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct CompanyContactData {
  std::string company_name;
  std::string website;
  std::string phone;
  std::string email;
  std::string headquarters;
  std::vector<std::string> socials;
};

struct NimAttemptResult {
  CompanyContactData contact;
  std::string raw_output;
  std::string selected_model;
  std::string base_url;
};

[[nodiscard]] std::string trim(const std::string& value) {
  auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  });
  auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  }).base();
  if (first >= last) {
    return {};
  }
  return std::string(first, last);
}

[[nodiscard]] std::optional<std::string> load_env_from_dotenv(
  const std::filesystem::path& dotenv_path,
  const std::string& key
) {
  std::ifstream file(dotenv_path);
  if (!file.is_open()) {
    return std::nullopt;
  }

  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const auto equals_pos = line.find('=');
    if (equals_pos == std::string::npos) {
      continue;
    }
    std::string parsed_key = trim(line.substr(0, equals_pos));
    if (parsed_key != key) {
      continue;
    }
    std::string parsed_value = trim(line.substr(equals_pos + 1));
    if (!parsed_value.empty() && parsed_value.front() == '"' && parsed_value.back() == '"' &&
        parsed_value.size() >= 2) {
      parsed_value = parsed_value.substr(1, parsed_value.size() - 2);
    }
    return parsed_value;
  }
  return std::nullopt;
}

[[nodiscard]] std::string require_env_value(const std::string& key) {
  if (const char* direct = std::getenv(key.c_str())) {
    std::string value = trim(direct);
    if (!value.empty()) {
      return value;
    }
  }

  if (auto from_dotenv = load_env_from_dotenv(".env", key); from_dotenv.has_value()) {
    return *from_dotenv;
  }

  throw std::runtime_error(
    "Missing " + key + ". Set it in your shell environment or in a local .env file."
  );
}

[[nodiscard]] std::string extract_json_object(const std::string& text) {
  const auto start = text.find('{');
  const auto end = text.rfind('}');
  if (start == std::string::npos || end == std::string::npos || end < start) {
    throw std::runtime_error("Model output did not contain a valid JSON object.");
  }
  return text.substr(start, end - start + 1);
}

[[nodiscard]] std::string extract_json_string_field(
  const std::string& json,
  const std::string& field_name
) {
  const std::string quoted_name = "\"" + field_name + "\"";
  const auto name_pos = json.find(quoted_name);
  if (name_pos == std::string::npos) {
    throw std::runtime_error("Missing field in JSON output: " + field_name);
  }

  const auto colon_pos = json.find(':', name_pos + quoted_name.size());
  if (colon_pos == std::string::npos) {
    throw std::runtime_error("Invalid JSON output near field: " + field_name);
  }

  const auto first_quote = json.find('"', colon_pos + 1);
  if (first_quote == std::string::npos) {
    throw std::runtime_error("Expected string value for field: " + field_name);
  }

  std::string value;
  bool escaped = false;
  for (std::size_t i = first_quote + 1; i < json.size(); ++i) {
    const char ch = json[i];
    if (escaped) {
      value.push_back(ch);
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      return value;
    }
    value.push_back(ch);
  }

  throw std::runtime_error("Unterminated string value for field: " + field_name);
}

[[nodiscard]] std::vector<std::string> extract_json_string_array_field(
  const std::string& json,
  const std::string& field_name
) {
  const std::string quoted_name = "\"" + field_name + "\"";
  const auto name_pos = json.find(quoted_name);
  if (name_pos == std::string::npos) {
    throw std::runtime_error("Missing array field in JSON output: " + field_name);
  }

  const auto colon_pos = json.find(':', name_pos + quoted_name.size());
  const auto open_bracket = json.find('[', colon_pos);
  const auto close_bracket = json.find(']', open_bracket);
  if (colon_pos == std::string::npos || open_bracket == std::string::npos ||
      close_bracket == std::string::npos || close_bracket < open_bracket) {
    throw std::runtime_error("Invalid array value for field: " + field_name);
  }

  std::vector<std::string> items;
  std::size_t cursor = open_bracket + 1;
  while (cursor < close_bracket) {
    const auto quote_start = json.find('"', cursor);
    if (quote_start == std::string::npos || quote_start >= close_bracket) {
      break;
    }

    std::string value;
    bool escaped = false;
    std::size_t i = quote_start + 1;
    for (; i < close_bracket; ++i) {
      const char ch = json[i];
      if (escaped) {
        value.push_back(ch);
        escaped = false;
        continue;
      }
      if (ch == '\\') {
        escaped = true;
        continue;
      }
      if (ch == '"') {
        items.push_back(value);
        break;
      }
      value.push_back(ch);
    }
    if (i >= close_bracket) {
      throw std::runtime_error("Unterminated array item for field: " + field_name);
    }
    cursor = i + 1;
  }

  return items;
}

[[nodiscard]] CompanyContactData parse_company_contact_json(const std::string& text) {
  const std::string json = extract_json_object(text);

  CompanyContactData contact;
  contact.company_name = extract_json_string_field(json, "company_name");
  contact.website = extract_json_string_field(json, "website");
  contact.phone = extract_json_string_field(json, "phone");
  contact.email = extract_json_string_field(json, "email");
  contact.headquarters = extract_json_string_field(json, "headquarters");
  contact.socials = extract_json_string_array_field(json, "socials");
  return contact;
}

[[nodiscard]] std::string to_lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

[[nodiscard]] bool ends_with(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

[[nodiscard]] std::string resolve_model_id(
  unillm::UnifiedClient& client,
  const std::string& provider_name,
  const std::string& requested_model
) {
  const auto models = client.list_models_sync(provider_name);
  if (models.empty()) {
    return requested_model;
  }

  auto exact = std::find_if(models.begin(), models.end(), [&](const unillm::ModelInfo& info) {
    return info.id == requested_model;
  });
  if (exact != models.end()) {
    return exact->id;
  }

  const std::string lower_requested = to_lower(requested_model);
  auto fuzzy = std::find_if(models.begin(), models.end(), [&](const unillm::ModelInfo& info) {
    const std::string lower_id = to_lower(info.id);
    return ends_with(lower_id, "/" + lower_requested) || lower_id.find(lower_requested) != std::string::npos;
  });
  if (fuzzy != models.end()) {
    return fuzzy->id;
  }

  std::ostringstream known_models;
  const std::size_t preview_count = std::min<std::size_t>(models.size(), 8);
  for (std::size_t i = 0; i < preview_count; ++i) {
    known_models << "\n  - " << models[i].id;
  }

  throw std::runtime_error(
    "Requested model \"" + requested_model + "\" was not found in NIM model catalog."
    " Available sample:" + known_models.str()
  );
}

[[nodiscard]] NimAttemptResult run_contact_parse_attempt(
  const std::string& api_key,
  const std::string& base_url,
  const std::string& requested_model,
  const std::string& company_name
) {
  constexpr const char* kProviderName = "nvidia_nim";
  unillm::ClientConfig config {
    .providers = {
      {
        .kind = unillm::ProviderKind::NvidiaNim,
        .name = kProviderName,
        .api_key = api_key,
        .base_url = base_url
      }
    },
    .routes = {},
    .default_provider = kProviderName
  };

  const std::string prompt =
    "Extract business contact data for \"" + company_name + "\". "
    "Reply with JSON only (no markdown, no extra text) using this exact schema: "
    "{\"company_name\":\"\", \"website\":\"\", \"phone\":\"\", \"email\":\"\", "
    "\"headquarters\":\"\", \"socials\":[\"\"]}. "
    "If a field is unknown, return an empty string; for socials return an empty array.";

  unillm::UnifiedClient client(std::move(config));
  const std::string selected_model = resolve_model_id(client, kProviderName, requested_model);
  const unillm::ChatResponse response = client.chat_sync({
    .model = selected_model,
    .messages = {
      {
        .role = unillm::Role::System,
        .content = "You are a strict information extraction assistant."
      },
      {
        .role = unillm::Role::User,
        .content = prompt
      }
    },
    .temperature = 0.0
  });

  if (response.choices.empty()) {
    throw std::runtime_error("Model returned no choices.");
  }

  const std::string& llm_text = response.choices.front().message.content;
  return {
    .contact = parse_company_contact_json(llm_text),
    .raw_output = llm_text,
    .selected_model = selected_model,
    .base_url = base_url
  };
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "Usage: " << argv[0] << " \"<company name>\" [model-id]\n";
      return 1;
    }

    const std::string company_name = argv[1];
    const std::string requested_model = argc >= 3 ? argv[2] : "nemotron-nano-12b-v2-vl";
    const std::string api_key = require_env_value("NVIDIA_NIM_API_KEY");
    const std::vector<std::string> candidate_base_urls = {
      "https://integrate.api.nvidia.com/v1",
      "https://integrate.api.nvidia.com/v1/openai"
    };

    std::optional<unillm::UnifiedError> last_provider_error;
    std::optional<std::string> last_std_error;
    std::optional<NimAttemptResult> result;
    for (const auto& base_url : candidate_base_urls) {
      try {
        result = run_contact_parse_attempt(api_key, base_url, requested_model, company_name);
        break;
      } catch (const unillm::UnifiedError& error) {
        last_provider_error = error;
        const std::string lower_body = to_lower(error.raw_body());
        const bool is_not_found =
          error.http_status() == 404 ||
          lower_body.find("404") != std::string::npos ||
          lower_body.find("not found") != std::string::npos;
        if (!is_not_found) {
          throw;
        }
      } catch (const std::exception& error) {
        last_std_error = error.what();
      }
    }

    if (!result.has_value()) {
      if (last_provider_error.has_value()) {
        throw *last_provider_error;
      }
      throw std::runtime_error(last_std_error.value_or("All NIM attempts failed."));
    }

    const CompanyContactData& parsed = result->contact;
    const std::string& llm_text = result->raw_output;

    std::cout << "Company: " << parsed.company_name << '\n';
    std::cout << "Website: " << parsed.website << '\n';
    std::cout << "Phone: " << parsed.phone << '\n';
    std::cout << "Email: " << parsed.email << '\n';
    std::cout << "Headquarters: " << parsed.headquarters << '\n';
    std::cout << "Model used: " << result->selected_model << '\n';
    std::cout << "Base URL used: " << result->base_url << '\n';
    std::cout << "Socials:\n";
    for (const auto& social : parsed.socials) {
      std::cout << "  - " << social << '\n';
    }
    std::cout << "\nRaw model output:\n" << llm_text << '\n';
    return 0;
  } catch (const unillm::UnifiedError& error) {
    std::cerr << "Provider error from " << error.provider_name()
              << " (HTTP " << error.http_status() << "): " << error.what() << '\n';
    if (!error.raw_body().empty()) {
      std::cerr << "Body: " << error.raw_body() << '\n';
    }
    return 2;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 3;
  }
}
