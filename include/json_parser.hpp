// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        include/json_parser.hpp
// Role:          Zero-Dependency Clean-Room JSON Lexer & Parser
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#ifndef LH_JSON_PARSER_HPP
#define LH_JSON_PARSER_HPP

#include <string>
#include <vector>
#include <utility>

namespace logihertz {

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    JsonType type{JsonType::Null};
    bool bool_val{false};
    double num_val{0.0};
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::vector<std::pair<std::string, JsonValue>> obj_val;

    JsonValue() = default;
    explicit JsonValue(JsonType t) : type(t) {}
    explicit JsonValue(bool b) : type(JsonType::Bool), bool_val(b) {}
    explicit JsonValue(double n) : type(JsonType::Number), num_val(n) {}
    explicit JsonValue(std::string s) : type(JsonType::String), str_val(std::move(s)) {}
    explicit JsonValue(const char* s) : type(JsonType::String), str_val(s) {}

    bool is_null() const { return type == JsonType::Null; }
    bool is_bool() const { return type == JsonType::Bool; }
    bool is_number() const { return type == JsonType::Number; }
    bool is_string() const { return type == JsonType::String; }
    bool is_array() const { return type == JsonType::Array; }
    bool is_object() const { return type == JsonType::Object; }

    bool as_bool(bool default_val = false) const {
        return (type == JsonType::Bool) ? bool_val : default_val;
    }

    double as_double(double default_val = 0.0) const {
        return (type == JsonType::Number) ? num_val : default_val;
    }

    int64_t as_int64(int64_t default_val = 0) const {
        return (type == JsonType::Number) ? static_cast<int64_t>(num_val) : default_val;
    }

    std::string as_string(const std::string& default_val = "") const {
        return (type == JsonType::String) ? str_val : default_val;
    }

    const JsonValue* get(const std::string& key) const;
    bool has_key(const std::string& key) const;

    static JsonValue parse(const std::string& input, std::string* out_err = nullptr);
    std::string to_string() const;
    std::string to_pretty_string(int indent = 0) const;
};

} // namespace logihertz

#endif // LH_JSON_PARSER_HPP
