// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        src/json_parser.cpp
// Role:          Zero-Dependency Clean-Room JSON Parser Implementation
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#include "json_parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace logihertz {

const JsonValue* JsonValue::get(const std::string& key) const {
    if (type != JsonType::Object) {
        return nullptr;
    }
    for (const auto& pair : obj_val) {
        if (pair.first == key) {
            return &pair.second;
        }
    }
    return nullptr;
}

bool JsonValue::has_key(const std::string& key) const {
    return get(key) != nullptr;
}

namespace {

void skip_ws(const std::string& s, size_t& pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        pos++;
    }
}

JsonValue parse_val(const std::string& s, size_t& pos, std::string& err);

JsonValue parse_str(const std::string& s, size_t& pos, std::string& err) {
    if (pos >= s.size() || s[pos] != '"') {
        err = "Expected '\"'";
        return JsonValue();
    }
    pos++; // skip opening quote
    std::string res;
    while (pos < s.size()) {
        char c = s[pos++];
        if (c == '"') {
            JsonValue v(JsonType::String);
            v.str_val = res;
            return v;
        }
        if (c == '\\') {
            if (pos >= s.size()) {
                err = "Unterminated string escape";
                return JsonValue();
            }
            char esc = s[pos++];
            switch (esc) {
                case '"':  res += '"'; break;
                case '\\': res += '\\'; break;
                case '/':  res += '/'; break;
                case 'b':  res += '\b'; break;
                case 'f':  res += '\f'; break;
                case 'n':  res += '\n'; break;
                case 'r':  res += '\r'; break;
                case 't':  res += '\t'; break;
                case 'u': {
                    if (pos + 4 > s.size()) {
                        err = "Incomplete unicode escape";
                        return JsonValue();
                    }
                    pos += 4; // skip 4 hex chars
                    res += '?';
                    break;
                }
                default:
                    res += esc;
                    break;
            }
        } else {
            res += c;
        }
    }
    err = "Unterminated string literal";
    return JsonValue();
}

JsonValue parse_num(const std::string& s, size_t& pos, std::string& err) {
    size_t start = pos;
    if (pos < s.size() && s[pos] == '-') {
        pos++;
    }
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
        pos++;
    }
    if (pos < s.size() && s[pos] == '.') {
        pos++;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            pos++;
        }
    }
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
        pos++;
        if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
            pos++;
        }
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            pos++;
        }
    }

    std::string sub = s.substr(start, pos - start);
    try {
        double d = std::stod(sub);
        return JsonValue(d);
    } catch (...) {
        err = "Invalid number format: " + sub;
        return JsonValue();
    }
}

JsonValue parse_arr(const std::string& s, size_t& pos, std::string& err) {
    if (pos >= s.size() || s[pos] != '[') {
        err = "Expected '['";
        return JsonValue();
    }
    pos++; // skip '['
    JsonValue v(JsonType::Array);
    skip_ws(s, pos);
    if (pos < s.size() && s[pos] == ']') {
        pos++;
        return v;
    }

    while (pos < s.size()) {
        skip_ws(s, pos);
        JsonValue item = parse_val(s, pos, err);
        if (!err.empty()) return JsonValue();
        v.arr_val.push_back(std::move(item));

        skip_ws(s, pos);
        if (pos >= s.size()) {
            err = "Unterminated array";
            return JsonValue();
        }
        if (s[pos] == ']') {
            pos++;
            return v;
        }
        if (s[pos] == ',') {
            pos++;
        } else {
            err = "Expected ',' or ']' in array";
            return JsonValue();
        }
    }

    err = "Unterminated array at EOF";
    return JsonValue();
}

JsonValue parse_obj(const std::string& s, size_t& pos, std::string& err) {
    if (pos >= s.size() || s[pos] != '{') {
        err = "Expected '{'";
        return JsonValue();
    }
    pos++; // skip '{'
    JsonValue v(JsonType::Object);
    skip_ws(s, pos);
    if (pos < s.size() && s[pos] == '}') {
        pos++;
        return v;
    }

    while (pos < s.size()) {
        skip_ws(s, pos);
        if (pos >= s.size() || s[pos] != '"') {
            err = "Expected string key in object";
            return JsonValue();
        }
        JsonValue k = parse_str(s, pos, err);
        if (!err.empty()) return JsonValue();

        skip_ws(s, pos);
        if (pos >= s.size() || s[pos] != ':') {
            err = "Expected ':' after object key";
            return JsonValue();
        }
        pos++; // skip ':'

        skip_ws(s, pos);
        JsonValue val = parse_val(s, pos, err);
        if (!err.empty()) return JsonValue();
        v.obj_val.emplace_back(k.str_val, std::move(val));

        skip_ws(s, pos);
        if (pos >= s.size()) {
            err = "Unterminated object";
            return JsonValue();
        }
        if (s[pos] == '}') {
            pos++;
            return v;
        }
        if (s[pos] == ',') {
            pos++;
        } else {
            err = "Expected ',' or '}' in object";
            return JsonValue();
        }
    }

    err = "Unterminated object at EOF";
    return JsonValue();
}

JsonValue parse_val(const std::string& s, size_t& pos, std::string& err) {
    skip_ws(s, pos);
    if (pos >= s.size()) {
        err = "Unexpected EOF while reading JSON value";
        return JsonValue();
    }

    char c = s[pos];
    if (c == '"') {
        return parse_str(s, pos, err);
    }
    if (c == '{') {
        return parse_obj(s, pos, err);
    }
    if (c == '[') {
        return parse_arr(s, pos, err);
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        return parse_num(s, pos, err);
    }
    if (s.compare(pos, 4, "true") == 0) {
        pos += 4;
        return JsonValue(true);
    }
    if (s.compare(pos, 5, "false") == 0) {
        pos += 5;
        return JsonValue(false);
    }
    if (s.compare(pos, 4, "null") == 0) {
        pos += 4;
        return JsonValue(JsonType::Null);
    }

    err = std::string("Unexpected token '") + c + "'";
    return JsonValue();
}

std::string escape_json_str(const std::string& str) {
    std::string out = "\"";
    for (char c : str) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    out += "\"";
    return out;
}

} // namespace

JsonValue JsonValue::parse(const std::string& input, std::string* out_err) {
    size_t pos = 0;
    std::string err;
    skip_ws(input, pos);
    JsonValue v = parse_val(input, pos, err);
    if (err.empty()) {
        skip_ws(input, pos);
        if (pos < input.size()) {
            err = "Trailing characters after JSON value";
        }
    }
    if (out_err != nullptr) {
        *out_err = err;
    }
    return v;
}

std::string JsonValue::to_string() const {
    switch (type) {
        case JsonType::Null: return "null";
        case JsonType::Bool: return bool_val ? "true" : "false";
        case JsonType::Number: {
            std::ostringstream ss;
            if (num_val == static_cast<int64_t>(num_val)) {
                ss << static_cast<int64_t>(num_val);
            } else {
                ss << num_val;
            }
            return ss.str();
        }
        case JsonType::String: return escape_json_str(str_val);
        case JsonType::Array: {
            std::string out = "[";
            for (size_t i = 0; i < arr_val.size(); ++i) {
                out += arr_val[i].to_string();
                if (i + 1 < arr_val.size()) out += ",";
            }
            out += "]";
            return out;
        }
        case JsonType::Object: {
            std::string out = "{";
            for (size_t i = 0; i < obj_val.size(); ++i) {
                out += escape_json_str(obj_val[i].first) + ":" + obj_val[i].second.to_string();
                if (i + 1 < obj_val.size()) out += ",";
            }
            out += "}";
            return out;
        }
    }
    return "null";
}

std::string JsonValue::to_pretty_string(int indent) const {
    std::string indent_str(indent * 2, ' ');
    std::string next_indent((indent + 1) * 2, ' ');

    switch (type) {
        case JsonType::Null: return "null";
        case JsonType::Bool: return bool_val ? "true" : "false";
        case JsonType::Number: {
            std::ostringstream ss;
            if (num_val == static_cast<int64_t>(num_val)) {
                ss << static_cast<int64_t>(num_val);
            } else {
                ss << num_val;
            }
            return ss.str();
        }
        case JsonType::String: return escape_json_str(str_val);
        case JsonType::Array: {
            if (arr_val.empty()) return "[]";
            std::string out = "[\n";
            for (size_t i = 0; i < arr_val.size(); ++i) {
                out += next_indent + arr_val[i].to_pretty_string(indent + 1);
                if (i + 1 < arr_val.size()) out += ",";
                out += "\n";
            }
            out += indent_str + "]";
            return out;
        }
        case JsonType::Object: {
            if (obj_val.empty()) return "{}";
            std::string out = "{\n";
            for (size_t i = 0; i < obj_val.size(); ++i) {
                out += next_indent + escape_json_str(obj_val[i].first) + ": " + obj_val[i].second.to_pretty_string(indent + 1);
                if (i + 1 < obj_val.size()) out += ",";
                out += "\n";
            }
            out += indent_str + "}";
            return out;
        }
    }
    return "null";
}

} // namespace logihertz
