#include <StartNode/Reflect/Json.hpp>

#include <cassert>
#include <cctype>
#include <cstring>
#include <format>
#include <string>
#include <utility>

namespace StartNode::Reflect::Json {

// ── Factory methods ──

auto FJsonValue::makeNull() -> FJsonValue { return {}; }

auto FJsonValue::makeBool(bool value) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Bool;
    v.boolValue = value;
    return v;
}

auto FJsonValue::makeInt(std::int64_t value) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Int;
    v.intValue = value;
    return v;
}

auto FJsonValue::makeDouble(double value) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Double;
    v.doubleValue = value;
    return v;
}

auto FJsonValue::makeString(std::string value) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::String;
    v.stringValue = std::move(value);
    return v;
}

auto FJsonValue::makeArray(std::vector<FJsonValue> values) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Array;
    v.arrayValue = std::move(values);
    return v;
}

auto FJsonValue::makeObject(
    std::vector<std::pair<std::string, FJsonValue>> members
) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Object;
    v.objectValue = std::move(members);
    return v;
}

// ── Accessors ──

auto FJsonValue::asBool() const -> bool
{
    assert(type == EType::Bool);
    return boolValue;
}

auto FJsonValue::asInt() const -> std::int64_t
{
    assert(type == EType::Int);
    return intValue;
}

auto FJsonValue::asDouble() const -> double
{
    assert(type == EType::Double);
    return doubleValue;
}

auto FJsonValue::asString() const -> const std::string&
{
    assert(type == EType::String);
    return stringValue;
}

auto FJsonValue::asArray() const -> const std::vector<FJsonValue>&
{
    assert(type == EType::Array);
    return arrayValue;
}

auto FJsonValue::asObject() const
    -> const std::vector<std::pair<std::string, FJsonValue>>&
{
    assert(type == EType::Object);
    return objectValue;
}

// ── JSON string escaping ──

namespace {

auto escapeString(std::string_view raw) -> std::string
{
    std::string result;
    result.reserve(raw.size() + 8);
    result += '"';
    for (char c : raw) {
        switch (c) {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n";  break;
        case '\r': result += "\\r";  break;
        case '\t': result += "\\t";  break;
        default:   result += c;      break;
        }
    }
    result += '"';
    return result;
}

void dumpImpl(const FJsonValue& val, std::string& out,
              std::size_t indent, std::size_t level)
{
    const auto pad = [&](std::size_t extra) {
        if (indent > 0) {
            out += '\n';
            out.append((level + extra) * indent, ' ');
        }
    };

    switch (val.getType()) {
    case FJsonValue::EType::Null:
        out += "null";
        break;
    case FJsonValue::EType::Bool:
        out += val.asBool() ? "true" : "false";
        break;
    case FJsonValue::EType::Int:
        out += std::to_string(val.asInt());
        break;
    case FJsonValue::EType::Double: {
        auto s = std::format("{}", val.asDouble());
        // Ensure decimal point for valid JSON (JSON has no NaN/Inf)
        if (s.find('.') == std::string::npos &&
            s.find('e') == std::string::npos &&
            s.find('E') == std::string::npos) {
            s += ".0";
        }
        out += s;
        break;
    }
    case FJsonValue::EType::String:
        out += escapeString(val.asString());
        break;
    case FJsonValue::EType::Array:
        out += '[';
        for (std::size_t i = 0; i < val.asArray().size(); ++i) {
            if (i > 0) { out += ','; }
            pad(1);
            dumpImpl(val.asArray()[i], out, indent, level + 1);
        }
        pad(0);
        out += ']';
        break;
    case FJsonValue::EType::Object:
        out += '{';
        for (std::size_t i = 0; i < val.asObject().size(); ++i) {
            if (i > 0) { out += ','; }
            pad(1);
            const auto& [key, member] = val.asObject()[i];
            out += escapeString(key);
            out += ':';
            if (indent > 0) { out += ' '; }
            dumpImpl(member, out, indent, level + 1);
        }
        pad(0);
        out += '}';
        break;
    }
}

} // anonymous namespace

auto FJsonValue::dump(std::size_t indent) const -> std::string
{
    std::string result;
    dumpImpl(*this, result, indent, 0);
    if (indent > 0) { result += '\n'; }
    return result;
}

// ── JSON Parser ──

namespace {

class FJsonParser
{
public:
    explicit FJsonParser(std::string_view text)
        : cursor(text.data()), end(text.data() + text.size()) {}

    auto parse() -> std::optional<FJsonValue>
    {
        skipWhitespace();
        if (atEnd()) { return {}; }
        auto val = parseValue();
        skipWhitespace();
        if (!atEnd()) { return {}; } // trailing garbage
        return val;
    }

private:
    const char* cursor;
    const char* end;

    bool atEnd() const { return cursor >= end; }
    char peek() const { return atEnd() ? '\0' : *cursor; }
    char advance() { return atEnd() ? '\0' : *cursor++; }
    void skipWhitespace() {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    auto parseValue() -> std::optional<FJsonValue>
    {
        skipWhitespace();
        switch (peek()) {
        case 'n': return parseNull();
        case 't': case 'f': return parseBool();
        case '"': return parseString();
        case '[': return parseArray();
        case '{': return parseObject();
        default:
            if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek()))) {
                return parseNumber();
            }
            return {};
        }
    }

    auto parseNull() -> std::optional<FJsonValue>
    {
        if (cursor + 4 <= end && std::strncmp(cursor, "null", 4) == 0) {
            cursor += 4;
            return FJsonValue::makeNull();
        }
        return {};
    }

    auto parseBool() -> std::optional<FJsonValue>
    {
        if (cursor + 4 <= end && std::strncmp(cursor, "true", 4) == 0) {
            cursor += 4;
            return FJsonValue::makeBool(true);
        }
        if (cursor + 5 <= end && std::strncmp(cursor, "false", 5) == 0) {
            cursor += 5;
            return FJsonValue::makeBool(false);
        }
        return {};
    }

    auto parseString() -> std::optional<FJsonValue>
    {
        if (advance() != '"') { return {}; }
        std::string result;
        while (!atEnd()) {
            char c = advance();
            if (c == '"') {
                return FJsonValue::makeString(std::move(result));
            }
            if (c == '\\') {
                if (atEnd()) { return {}; }
                switch (advance()) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   return {};       // unsupported escape
                }
            } else {
                result += c;
            }
        }
        return {}; // unterminated string
    }

    auto parseNumber() -> std::optional<FJsonValue>
    {
        const char* start = cursor;
        if (peek() == '-') { advance(); }
        if (atEnd()) { return {}; }

        // Integer part
        if (peek() == '0') {
            advance();
        } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        } else {
            return {};
        }

        bool isFloat = false;

        // Fractional part
        if (peek() == '.') {
            isFloat = true;
            advance();
            if (atEnd() || !std::isdigit(static_cast<unsigned char>(peek()))) {
                return {};
            }
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }

        // Exponent part
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            advance();
            if (peek() == '+' || peek() == '-') { advance(); }
            if (atEnd() || !std::isdigit(static_cast<unsigned char>(peek()))) {
                return {};
            }
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }

        std::string numberStr(start, cursor);
        try {
            if (isFloat) {
                std::size_t pos = 0;
                double d = std::stod(numberStr, &pos);
                if (pos != numberStr.size()) { return {}; }
                return FJsonValue::makeDouble(d);
            }
            std::size_t pos = 0;
            std::int64_t i = std::stoll(numberStr, &pos);
            if (pos != numberStr.size()) { return {}; }
            return FJsonValue::makeInt(i);
        } catch (...) {
            return {};
        }
    }

    auto parseArray() -> std::optional<FJsonValue>
    {
        if (advance() != '[') { return {}; }
        std::vector<FJsonValue> elements;
        skipWhitespace();
        if (peek() == ']') {
            advance();
            return FJsonValue::makeArray(std::move(elements));
        }
        while (true) {
            auto val = parseValue();
            if (!val) { return {}; }
            elements.push_back(std::move(*val));
            skipWhitespace();
            if (peek() == ']') {
                advance();
                return FJsonValue::makeArray(std::move(elements));
            }
            if (peek() != ',') { return {}; }
            advance();
        }
    }

    auto parseObject() -> std::optional<FJsonValue>
    {
        if (advance() != '{') { return {}; }
        std::vector<std::pair<std::string, FJsonValue>> members;
        skipWhitespace();
        if (peek() == '}') {
            advance();
            return FJsonValue::makeObject(std::move(members));
        }
        while (true) {
            skipWhitespace();
            auto keyVal = parseString();
            if (!keyVal || keyVal->getType() != FJsonValue::EType::String) {
                return {};
            }
            std::string key = keyVal->asString();
            skipWhitespace();
            if (advance() != ':') { return {}; }
            auto val = parseValue();
            if (!val) { return {}; }
            members.emplace_back(std::move(key), std::move(*val));
            skipWhitespace();
            if (peek() == '}') {
                advance();
                return FJsonValue::makeObject(std::move(members));
            }
            if (peek() != ',') { return {}; }
            advance();
        }
    }
};

} // anonymous namespace

auto FJsonValue::parse(std::string_view text) -> std::optional<FJsonValue>
{
    FJsonParser parser(text);
    return parser.parse();
}

} // namespace StartNode::Reflect::Json
