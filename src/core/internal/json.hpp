#pragma once

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace unillm::internal {

class Json {
public:
  using array = std::vector<Json>;
  using object = std::map<std::string, Json>;

  Json() = default;
  Json(std::nullptr_t) : value_(nullptr) {}
  Json(bool value) : value_(value) {}
  Json(double value) : value_(value) {}
  Json(std::int64_t value) : value_(static_cast<double>(value)) {}
  Json(int value) : value_(static_cast<double>(value)) {}
  Json(std::string value) : value_(std::move(value)) {}
  Json(const char* value) : value_(std::string(value)) {}
  Json(array value) : value_(std::move(value)) {}
  Json(object value) : value_(std::move(value)) {}

  [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
  [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(value_); }
  [[nodiscard]] bool is_number() const { return std::holds_alternative<double>(value_); }
  [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(value_); }
  [[nodiscard]] bool is_array() const { return std::holds_alternative<array>(value_); }
  [[nodiscard]] bool is_object() const { return std::holds_alternative<object>(value_); }

  [[nodiscard]] bool as_bool(bool fallback = false) const {
    return is_bool() ? std::get<bool>(value_) : fallback;
  }

  [[nodiscard]] double as_number(double fallback = 0.0) const {
    return is_number() ? std::get<double>(value_) : fallback;
  }

  [[nodiscard]] int as_int(int fallback = 0) const {
    return is_number() ? static_cast<int>(std::llround(std::get<double>(value_))) : fallback;
  }

  [[nodiscard]] const std::string& as_string() const {
    if (!is_string()) {
      throw std::runtime_error("Json value is not a string");
    }
    return std::get<std::string>(value_);
  }

  [[nodiscard]] const array& as_array() const {
    if (!is_array()) {
      throw std::runtime_error("Json value is not an array");
    }
    return std::get<array>(value_);
  }

  [[nodiscard]] const object& as_object() const {
    if (!is_object()) {
      throw std::runtime_error("Json value is not an object");
    }
    return std::get<object>(value_);
  }

  [[nodiscard]] array& as_array() {
    if (!is_array()) {
      value_ = array {};
    }
    return std::get<array>(value_);
  }

  [[nodiscard]] object& as_object() {
    if (!is_object()) {
      value_ = object {};
    }
    return std::get<object>(value_);
  }

  [[nodiscard]] bool contains(const std::string& key) const {
    return is_object() && as_object().contains(key);
  }

  [[nodiscard]] const Json& operator[](const std::string& key) const {
    static const Json kNull {};
    if (!is_object()) {
      return kNull;
    }
    const auto& obj = as_object();
    const auto it = obj.find(key);
    return it == obj.end() ? kNull : it->second;
  }

  [[nodiscard]] Json& operator[](const std::string& key) {
    return as_object()[key];
  }

  [[nodiscard]] const Json& operator[](std::size_t index) const {
    static const Json kNull {};
    if (!is_array()) {
      return kNull;
    }
    const auto& arr = as_array();
    return index < arr.size() ? arr[index] : kNull;
  }

  [[nodiscard]] static Json parse(std::string_view text) {
    Parser parser(text);
    Json value = parser.parse_value();
    parser.skip_ws();
    if (!parser.eof()) {
      throw std::runtime_error("Unexpected trailing JSON content");
    }
    return value;
  }

  [[nodiscard]] std::string dump() const {
    std::ostringstream out;
    dump_to(out);
    return out.str();
  }

private:
  class Parser {
  public:
    explicit Parser(std::string_view text) : text_(text) {}

    [[nodiscard]] bool eof() const {
      return index_ >= text_.size();
    }

    void skip_ws() {
      while (!eof() && std::isspace(static_cast<unsigned char>(text_[index_]))) {
        ++index_;
      }
    }

    [[nodiscard]] Json parse_value() {
      skip_ws();
      if (eof()) {
        throw std::runtime_error("Unexpected end of JSON");
      }

      const char current = text_[index_];
      if (current == '"') {
        return Json(parse_string());
      }
      if (current == '{') {
        return Json(parse_object());
      }
      if (current == '[') {
        return Json(parse_array());
      }
      if (current == 't') {
        expect("true");
        return Json(true);
      }
      if (current == 'f') {
        expect("false");
        return Json(false);
      }
      if (current == 'n') {
        expect("null");
        return Json(nullptr);
      }
      if (current == '-' || std::isdigit(static_cast<unsigned char>(current))) {
        return Json(parse_number());
      }
      throw std::runtime_error("Unsupported JSON token");
    }

  private:
    [[nodiscard]] std::string parse_string() {
      consume('"');
      std::string result;
      while (!eof()) {
        const char c = text_[index_++];
        if (c == '"') {
          return result;
        }
        if (c == '\\') {
          if (eof()) {
            throw std::runtime_error("Invalid JSON escape");
          }
          const char escaped = text_[index_++];
          switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default: throw std::runtime_error("Unsupported JSON escape");
          }
          continue;
        }
        result.push_back(c);
      }
      throw std::runtime_error("Unterminated JSON string");
    }

    [[nodiscard]] double parse_number() {
      const std::size_t start = index_;
      if (text_[index_] == '-') {
        ++index_;
      }
      while (!eof() && std::isdigit(static_cast<unsigned char>(text_[index_]))) {
        ++index_;
      }
      if (!eof() && text_[index_] == '.') {
        ++index_;
        while (!eof() && std::isdigit(static_cast<unsigned char>(text_[index_]))) {
          ++index_;
        }
      }
      if (!eof() && (text_[index_] == 'e' || text_[index_] == 'E')) {
        ++index_;
        if (!eof() && (text_[index_] == '+' || text_[index_] == '-')) {
          ++index_;
        }
        while (!eof() && std::isdigit(static_cast<unsigned char>(text_[index_]))) {
          ++index_;
        }
      }
      return std::strtod(std::string(text_.substr(start, index_ - start)).c_str(), nullptr);
    }

    [[nodiscard]] object parse_object() {
      consume('{');
      object result;
      skip_ws();
      if (peek('}')) {
        consume('}');
        return result;
      }
      while (true) {
        skip_ws();
        const std::string key = parse_string();
        skip_ws();
        consume(':');
        result.emplace(key, parse_value());
        skip_ws();
        if (peek('}')) {
          consume('}');
          return result;
        }
        consume(',');
      }
    }

    [[nodiscard]] array parse_array() {
      consume('[');
      array result;
      skip_ws();
      if (peek(']')) {
        consume(']');
        return result;
      }
      while (true) {
        result.push_back(parse_value());
        skip_ws();
        if (peek(']')) {
          consume(']');
          return result;
        }
        consume(',');
      }
    }

    void expect(std::string_view token) {
      if (text_.substr(index_, token.size()) != token) {
        throw std::runtime_error("Unexpected JSON keyword");
      }
      index_ += token.size();
    }

    void consume(char expected) {
      skip_ws();
      if (eof() || text_[index_] != expected) {
        throw std::runtime_error("Unexpected JSON character");
      }
      ++index_;
    }

    [[nodiscard]] bool peek(char expected) const {
      return !eof() && text_[index_] == expected;
    }

    std::string_view text_;
    std::size_t index_ {0};
  };

  void dump_to(std::ostringstream& out) const {
    if (is_null()) {
      out << "null";
      return;
    }
    if (is_bool()) {
      out << (as_bool() ? "true" : "false");
      return;
    }
    if (is_number()) {
      out << std::get<double>(value_);
      return;
    }
    if (is_string()) {
      out << '"' << escape(as_string()) << '"';
      return;
    }
    if (is_array()) {
      out << '[';
      bool first = true;
      for (const auto& entry : as_array()) {
        if (!first) {
          out << ',';
        }
        first = false;
        entry.dump_to(out);
      }
      out << ']';
      return;
    }
    out << '{';
    bool first = true;
    for (const auto& [key, value] : as_object()) {
      if (!first) {
        out << ',';
      }
      first = false;
      out << '"' << escape(key) << '"' << ':';
      value.dump_to(out);
    }
    out << '}';
  }

  [[nodiscard]] static std::string escape(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (const char c : input) {
      switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result.push_back(c); break;
      }
    }
    return result;
  }

  std::variant<std::nullptr_t, bool, double, std::string, array, object> value_ {nullptr};
};

inline Json::object make_object(std::initializer_list<std::pair<const std::string, Json>> values) {
  Json::object object;
  for (const auto& [key, value] : values) {
    object.emplace(key, value);
  }
  return object;
}

}  // namespace unillm::internal
