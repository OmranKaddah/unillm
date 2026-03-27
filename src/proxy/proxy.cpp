#include "unillm/proxy.hpp"

#include "core/internal/json.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace unillm {

namespace {

using internal::Json;

[[nodiscard]] std::string reason_phrase(int status) {
  switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default: return "Unknown";
  }
}

[[nodiscard]] ChatRequest parse_chat_request(const ProxyRequest& request) {
  const Json json = Json::parse(request.body);
  ChatRequest chat;
  chat.model = json["model"].as_string();
  chat.stream = json["stream"].as_bool(false);
  if (json.contains("temperature")) {
    chat.temperature = json["temperature"].as_number();
  }
  if (json.contains("max_tokens")) {
    chat.max_tokens = json["max_tokens"].as_int();
  }
  for (const auto& message : json["messages"].as_array()) {
    chat.messages.push_back(Message {
      .role = [&]() {
      const auto& role = message["role"].as_string();
      if (role == "system") return Role::System;
      if (role == "assistant") return Role::Assistant;
      if (role == "tool") return Role::Tool;
      return Role::User;
      }(),
      .content = message["content"].as_string()
    });
  }
  return chat;
}

[[nodiscard]] EmbeddingRequest parse_embedding_request(const ProxyRequest& request) {
  const Json json = Json::parse(request.body);
  EmbeddingRequest embedding;
  embedding.model = json["model"].as_string();
  if (json["input"].is_array()) {
    for (const auto& entry : json["input"].as_array()) {
      embedding.input.push_back(entry.as_string());
    }
  } else {
    embedding.input.push_back(json["input"].as_string());
  }
  return embedding;
}

[[nodiscard]] std::string to_openai_chat_json(const ChatResponse& response) {
  Json::array choices;
  for (const auto& choice : response.choices) {
    choices.push_back(Json(internal::make_object({
      {"index", choice.index},
      {"finish_reason", choice.finish_reason},
      {"message", Json(internal::make_object({
        {"role", "assistant"},
        {"content", choice.message.content}
      }))}
    })));
  }
  return Json(internal::make_object({
    {"id", response.id},
    {"object", "chat.completion"},
    {"model", response.model},
    {"choices", choices},
    {"usage", Json(internal::make_object({
      {"prompt_tokens", response.usage.prompt_tokens},
      {"completion_tokens", response.usage.completion_tokens},
      {"total_tokens", response.usage.total_tokens}
    }))}
  })).dump();
}

[[nodiscard]] std::string to_openai_embeddings_json(const EmbeddingResponse& response) {
  Json::array data;
  for (const auto& row : response.data) {
    Json::array values;
    for (const double value : row.embedding) {
      values.push_back(value);
    }
    data.push_back(Json(internal::make_object({
      {"object", "embedding"},
      {"index", row.index},
      {"embedding", values}
    })));
  }
  return Json(internal::make_object({
    {"object", "list"},
    {"model", response.model},
    {"data", data},
    {"usage", Json(internal::make_object({
      {"prompt_tokens", response.usage.prompt_tokens},
      {"completion_tokens", response.usage.completion_tokens},
      {"total_tokens", response.usage.total_tokens}
    }))}
  })).dump();
}

[[nodiscard]] std::string to_openai_models_json(const std::vector<ModelInfo>& models) {
  Json::array data;
  for (const auto& model : models) {
    Json::object entry {
      {"id", model.id},
      {"object", "model"},
      {"owned_by", model.owned_by.value_or(model.provider)}
    };
    data.push_back(Json(std::move(entry)));
  }
  return Json(internal::make_object({
    {"object", "list"},
    {"data", data}
  })).dump();
}

[[nodiscard]] std::string make_error_json(const std::string& message) {
  return Json(internal::make_object({
    {"error", Json(internal::make_object({
      {"message", message}
    }))}
  })).dump();
}

void write_all(int fd, const std::string& text) {
  std::size_t written = 0;
  while (written < text.size()) {
    const auto result = ::send(fd, text.data() + written, text.size() - written, 0);
    if (result <= 0) {
      throw std::runtime_error("socket write failed");
    }
    written += static_cast<std::size_t>(result);
  }
}

[[nodiscard]] ProxyRequest read_request(int client_fd) {
  std::string buffer;
  char temp[4096];
  while (buffer.find("\r\n\r\n") == std::string::npos) {
    const auto bytes = ::recv(client_fd, temp, sizeof(temp), 0);
    if (bytes <= 0) {
      throw std::runtime_error("socket read failed");
    }
    buffer.append(temp, temp + bytes);
  }

  const auto header_end = buffer.find("\r\n\r\n");
  std::string head = buffer.substr(0, header_end);
  std::string body = buffer.substr(header_end + 4);
  std::istringstream stream(head);

  ProxyRequest request;
  stream >> request.method >> request.target;
  std::string http_version;
  stream >> http_version;
  std::string line;
  std::getline(stream, line);

  std::size_t content_length = 0;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    while (!value.empty() && value.front() == ' ') {
      value.erase(value.begin());
    }
    request.headers[key] = value;
    if (key == "Content-Length") {
      content_length = static_cast<std::size_t>(std::stoul(value));
    }
  }

  while (body.size() < content_length) {
    const auto bytes = ::recv(client_fd, temp, sizeof(temp), 0);
    if (bytes <= 0) {
      throw std::runtime_error("socket read failed");
    }
    body.append(temp, temp + bytes);
  }
  request.body = std::move(body);
  return request;
}

void write_http_response(int client_fd, const ProxyResponse& response) {
  std::ostringstream out;
  out << "HTTP/1.1 " << response.status << ' ' << reason_phrase(response.status) << "\r\n";
  out << "Content-Type: " << response.content_type << "\r\n";
  out << "Content-Length: " << response.body.size() << "\r\n";
  for (const auto& [key, value] : response.headers) {
    out << key << ": " << value << "\r\n";
  }
  out << "Connection: close\r\n\r\n";
  out << response.body;
  write_all(client_fd, out.str());
}

}  // namespace

ProxyApplication::ProxyApplication(UnifiedClient client) : client_(std::move(client)) {}

ProxyResponse ProxyApplication::handle_request(const ProxyRequest& request) const {
  try {
    if (request.method == "GET" && request.target == "/v1/models") {
      return ProxyResponse {
        .status = 200,
        .content_type = "application/json",
        .body = to_openai_models_json(client_.list_models_sync())
      };
    }

    if (request.method == "POST" && request.target == "/v1/embeddings") {
      const auto embedding = client_.embed_sync(parse_embedding_request(request));
      return ProxyResponse {
        .status = 200,
        .content_type = "application/json",
        .body = to_openai_embeddings_json(embedding)
      };
    }

    if (request.method == "POST" && request.target == "/v1/chat/completions") {
      ChatRequest chat = parse_chat_request(request);
      if (!chat.stream) {
        return ProxyResponse {
          .status = 200,
          .content_type = "application/json",
          .body = to_openai_chat_json(client_.chat_sync(std::move(chat)))
        };
      }

      std::ostringstream sse;
      client_.chat_stream_sync(chat, [&](const StreamEvent& event) {
        if (event.type == StreamEventType::Delta) {
          sse << "data: " << Json(internal::make_object({
            {"choices", Json::array {Json(internal::make_object({
              {"delta", Json(internal::make_object({{"content", event.delta}}))}
            }))}}
          })).dump() << "\n\n";
        }
        if (event.type == StreamEventType::MessageStop) {
          sse << "data: [DONE]\n\n";
        }
      });

      return ProxyResponse {
        .status = 200,
        .content_type = "text/event-stream",
        .headers = {{"Cache-Control", "no-cache"}},
        .body = sse.str()
      };
    }

    return ProxyResponse {
      .status = 404,
      .content_type = "application/json",
      .body = make_error_json("unknown route")
    };
  } catch (const UnifiedError& error) {
    return ProxyResponse {
      .status = error.http_status() == 0 ? 500 : error.http_status(),
      .content_type = "application/json",
      .body = make_error_json(error.what())
    };
  } catch (const std::exception& error) {
    return ProxyResponse {
      .status = 500,
      .content_type = "application/json",
      .body = make_error_json(error.what())
    };
  }
}

ProxyServer::ProxyServer(UnifiedClient client, ProxyServerOptions options) :
  client_(std::move(client)),
  options_(std::move(options)) {}

void ProxyServer::run() {
  const int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    throw std::runtime_error("Unable to create server socket");
  }

  int opt = 1;
  ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in address {};
  address.sin_family = AF_INET;
  address.sin_port = htons(options_.port);
  if (::inet_pton(AF_INET, options_.host.c_str(), &address.sin_addr) != 1) {
    ::close(server_fd);
    throw std::runtime_error("Invalid bind address");
  }

  if (::bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    ::close(server_fd);
    throw std::runtime_error("Unable to bind server socket");
  }

  if (::listen(server_fd, 16) < 0) {
    ::close(server_fd);
    throw std::runtime_error("Unable to listen on server socket");
  }

  const ProxyApplication app(client_);
  while (true) {
    const int client_fd = ::accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      continue;
    }

    try {
      const ProxyRequest request = read_request(client_fd);
      write_http_response(client_fd, app.handle_request(request));
    } catch (...) {
      write_http_response(client_fd, ProxyResponse {
        .status = 500,
        .content_type = "application/json",
        .body = make_error_json("proxy request handling failed")
      });
    }
    ::close(client_fd);
  }
}

}  // namespace unillm
