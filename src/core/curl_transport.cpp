#include "core/internal/http.hpp"

#include "unillm/unillm.hpp"

#include <curl/curl.h>

#include <memory>
#include <sstream>

namespace unillm {

namespace {

size_t append_body(void* contents, size_t size, size_t nmemb, void* userp) {
  const size_t total_size = size * nmemb;
  auto* body = static_cast<std::string*>(userp);
  body->append(static_cast<const char*>(contents), total_size);
  return total_size;
}

size_t append_header(char* buffer, size_t size, size_t nitems, void* userdata) {
  const size_t total_size = size * nitems;
  auto* headers = static_cast<std::unordered_map<std::string, std::string>*>(userdata);
  std::string line(buffer, total_size);
  const auto colon = line.find(':');
  if (colon != std::string::npos) {
    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
      value.pop_back();
    }
    while (!value.empty() && value.front() == ' ') {
      value.erase(value.begin());
    }
    (*headers)[std::move(key)] = std::move(value);
  }
  return total_size;
}

class CurlHttpTransport final : public IHttpTransport {
public:
  CurlHttpTransport() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }

  ~CurlHttpTransport() override {
    curl_global_cleanup();
  }

  [[nodiscard]] HttpResponse send(const HttpRequest& request) const override {
    CURL* handle = curl_easy_init();
    if (handle == nullptr) {
      throw UnifiedError("", 0, true, "Unable to initialize curl");
    }

    HttpResponse response;
    std::string response_body;
    struct curl_slist* header_list = nullptr;

    curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, append_body);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, append_header);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &response.headers);

    if (request.timeout_ms.has_value()) {
      curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, *request.timeout_ms);
    }

    if (!request.body.empty()) {
      curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.c_str());
      curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
    }

    for (const auto& [key, value] : request.headers) {
      std::ostringstream header;
      header << key << ": " << value;
      header_list = curl_slist_append(header_list, header.str().c_str());
    }

    if (header_list != nullptr) {
      curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list);
    }

    const CURLcode code = curl_easy_perform(handle);
    if (code != CURLE_OK) {
      const std::string message = curl_easy_strerror(code);
      curl_slist_free_all(header_list);
      curl_easy_cleanup(handle);
      throw UnifiedError("", 0, true, "curl transport failed: " + message);
    }

    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status_code);
    response.body = std::move(response_body);

    curl_slist_free_all(header_list);
    curl_easy_cleanup(handle);
    return response;
  }
};

}  // namespace

std::shared_ptr<IHttpTransport> make_default_transport() {
  return std::make_shared<CurlHttpTransport>();
}

}  // namespace unillm
