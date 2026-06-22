#ifndef STARTNODE_REFLECT_JSON_HPP
#define STARTNODE_REFLECT_JSON_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace StartNode::Reflect::Json {

// Minimal JSON value type. Represents the standard JSON data model:
// null, bool, int64, double, string, array, object.
// Zero third-party dependencies — hand-rolled writer and parser.
class FJsonValue
{
public:
    enum class EType
    {
        Null,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object
    };

    // ── Constructors ──

    static auto makeNull() -> FJsonValue;
    static auto makeBool(bool value) -> FJsonValue;
    static auto makeInt(std::int64_t value) -> FJsonValue;
    static auto makeDouble(double value) -> FJsonValue;
    static auto makeString(std::string value) -> FJsonValue;
    static auto makeArray(std::vector<FJsonValue> values) -> FJsonValue;
    static auto makeObject(
        std::vector<std::pair<std::string, FJsonValue>> members
    ) -> FJsonValue;

    // ── Accessors ──

    auto getType() const -> EType { return type; }

    auto asBool() const -> bool;
    auto asInt() const -> std::int64_t;
    auto asDouble() const -> double;
    auto asString() const -> const std::string&;
    auto asArray() const -> const std::vector<FJsonValue>&;
    auto asObject() const
        -> const std::vector<std::pair<std::string, FJsonValue>>&;

    // ── Serialize ──

    // Pretty-print to JSON string with optional indentation.
    auto dump(std::size_t indent = 0) const -> std::string;

    // Parse a JSON string. Returns nullopt on syntax error.
    static auto parse(std::string_view text) -> std::optional<FJsonValue>;

private:
    FJsonValue() = default;

    EType type = EType::Null;

    // Stored values (active member matches EType)
    bool boolValue = false;
    std::int64_t intValue = 0;
    double doubleValue = 0.0;
    std::string stringValue;
    std::vector<FJsonValue> arrayValue;
    std::vector<std::pair<std::string, FJsonValue>> objectValue;
};

} // namespace StartNode::Reflect::Json

#endif // STARTNODE_REFLECT_JSON_HPP
