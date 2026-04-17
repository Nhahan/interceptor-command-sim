#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace icss::testsupport::minijson {

struct Value;
using Object = std::unordered_map<std::string, Value>;
using Array = std::vector<Value>;

struct Value {
    using Storage = std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object>;
    Storage storage;

    [[nodiscard]] bool is_object() const { return std::holds_alternative<Object>(storage); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<Array>(storage); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(storage); }
    [[nodiscard]] bool is_int() const { return std::holds_alternative<std::int64_t>(storage); }
    [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(storage); }

    [[nodiscard]] const Object& as_object() const { return std::get<Object>(storage); }
    [[nodiscard]] const Array& as_array() const { return std::get<Array>(storage); }
    [[nodiscard]] const std::string& as_string() const { return std::get<std::string>(storage); }
    [[nodiscard]] std::int64_t as_int() const { return std::get<std::int64_t>(storage); }
    [[nodiscard]] bool as_bool() const { return std::get<bool>(storage); }
};

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Value parse_value() {
        skip_ws();
        if (cursor_ >= text_.size()) {
            throw std::runtime_error("unexpected end of json");
        }
        const char ch = text_[cursor_];
        if (ch == '{') {
            return Value {parse_object()};
        }
        if (ch == '[') {
            return Value {parse_array()};
        }
        if (ch == '"') {
            return Value {parse_string()};
        }
        if (ch == 't' || ch == 'f') {
            return Value {parse_bool()};
        }
        if (ch == 'n') {
            parse_null();
            return Value {nullptr};
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            return Value {parse_int()};
        }
        throw std::runtime_error("unsupported json token");
    }

    void expect_finished() {
        skip_ws();
        if (cursor_ != text_.size()) {
            throw std::runtime_error("unexpected trailing json content");
        }
    }

private:
    void skip_ws() {
        while (cursor_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[cursor_]))) {
            ++cursor_;
        }
    }

    void expect(char ch) {
        skip_ws();
        if (cursor_ >= text_.size() || text_[cursor_] != ch) {
            throw std::runtime_error("unexpected json character");
        }
        ++cursor_;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        bool escaped = false;
        while (cursor_ < text_.size()) {
            const char ch = text_[cursor_++];
            if (!escaped) {
                if (ch == '\\') {
                    escaped = true;
                    continue;
                }
                if (ch == '"') {
                    return out;
                }
                out.push_back(ch);
                continue;
            }
            switch (ch) {
            case '\\': out.push_back('\\'); break;
            case '"': out.push_back('"'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            default: throw std::runtime_error("unsupported json escape");
            }
            escaped = false;
        }
        throw std::runtime_error("unterminated json string");
    }

    std::int64_t parse_int() {
        skip_ws();
        std::size_t start = cursor_;
        if (text_[cursor_] == '-') {
            ++cursor_;
        }
        while (cursor_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[cursor_]))) {
            ++cursor_;
        }
        return std::stoll(std::string{text_.substr(start, cursor_ - start)});
    }

    bool parse_bool() {
        skip_ws();
        if (text_.substr(cursor_, 4) == "true") {
            cursor_ += 4;
            return true;
        }
        if (text_.substr(cursor_, 5) == "false") {
            cursor_ += 5;
            return false;
        }
        throw std::runtime_error("invalid json bool");
    }

    void parse_null() {
        skip_ws();
        if (text_.substr(cursor_, 4) != "null") {
            throw std::runtime_error("invalid json null");
        }
        cursor_ += 4;
    }

    Array parse_array() {
        expect('[');
        Array array;
        skip_ws();
        if (cursor_ < text_.size() && text_[cursor_] == ']') {
            ++cursor_;
            return array;
        }
        while (true) {
            array.push_back(parse_value());
            skip_ws();
            if (cursor_ < text_.size() && text_[cursor_] == ']') {
                ++cursor_;
                return array;
            }
            expect(',');
        }
    }

    Object parse_object() {
        expect('{');
        Object object;
        skip_ws();
        if (cursor_ < text_.size() && text_[cursor_] == '}') {
            ++cursor_;
            return object;
        }
        while (true) {
            const auto key = parse_string();
            expect(':');
            object.emplace(key, parse_value());
            skip_ws();
            if (cursor_ < text_.size() && text_[cursor_] == '}') {
                ++cursor_;
                return object;
            }
            expect(',');
        }
    }

    std::string_view text_;
    std::size_t cursor_ {0};
};

inline Value parse(std::string_view text) {
    Parser parser(text);
    auto value = parser.parse_value();
    parser.expect_finished();
    return value;
}

inline const Value& require_field(const Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        throw std::runtime_error("missing json field: " + key);
    }
    return it->second;
}

}  // namespace icss::testsupport::minijson
