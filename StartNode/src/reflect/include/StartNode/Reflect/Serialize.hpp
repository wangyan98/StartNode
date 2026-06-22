#ifndef STARTNODE_REFLECT_SERIALIZE_HPP
#define STARTNODE_REFLECT_SERIALIZE_HPP

#include <StartNode/Reflect/Json.hpp>
#include <StartNode/Reflect/TypeRegistry.hpp>

#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace StartNode::Reflect {

// ── Type trait helpers ──

namespace detail {

template <typename T>
concept IsVector = requires { typename T::value_type; }
    && std::same_as<T, std::vector<typename T::value_type>>;

template <typename T>
concept IsOptional = requires { typename T::value_type; }
    && std::same_as<T, std::optional<typename T::value_type>>;

} // namespace detail

// ── Forward declarations for runtime dispatch (defined in Serialize.cpp) ──

namespace detail {

// Serialize a value of an arbitrary reflected type, identified only by
// its FTypeId, through the runtime dispatch table. Aggregates recurse
// via FTypeDescriptor::members; scalars hit pre-registered converters.
auto dispatchToJson(FTypeId typeId, const void* pointer)
    -> std::expected<Json::FJsonValue, std::string>;

// Deserialize into `pointer` (typed by typeId) from a JSON value.
auto dispatchFromJson(FTypeId typeId, const Json::FJsonValue& json,
                      void* pointer) -> std::expected<void, std::string>;

} // namespace detail

// ── Public entry points (forward-declared so the detail helpers below
//    can recurse on vectors / optionals via qualified name lookup). ──

template <typename T>
auto toJson(const T& object) -> std::expected<Json::FJsonValue, std::string>;

template <typename T>
auto fromJson(const Json::FJsonValue& json, T& object)
    -> std::expected<void, std::string>;

// ── toJson template implementation ──

namespace detail {

template <typename T>
auto toJsonScalar(const T& value) -> std::expected<Json::FJsonValue, std::string>
{
    if constexpr (std::is_same_v<T, bool>) {
        return Json::FJsonValue::makeBool(value);
    } else if constexpr (std::is_integral_v<T>) {
        return Json::FJsonValue::makeInt(static_cast<std::int64_t>(value));
    } else if constexpr (std::is_floating_point_v<T>) {
        return Json::FJsonValue::makeDouble(static_cast<double>(value));
    } else if constexpr (std::is_same_v<T, std::string>) {
        return Json::FJsonValue::makeString(value);
    }
    return std::unexpected(std::string{"toJson: unsupported scalar type"});
}

template <typename T>
auto toJsonVector(const std::vector<T>& vec)
    -> std::expected<Json::FJsonValue, std::string>
{
    std::vector<Json::FJsonValue> elements;
    elements.reserve(vec.size());
    for (const auto& element : vec) {
        auto jsonElement = StartNode::Reflect::toJson(element);
        if (!jsonElement) { return std::unexpected(jsonElement.error()); }
        elements.push_back(std::move(*jsonElement));
    }
    return Json::FJsonValue::makeArray(std::move(elements));
}

template <typename T>
auto toJsonOptional(const std::optional<T>& opt)
    -> std::expected<Json::FJsonValue, std::string>
{
    if (!opt.has_value()) { return Json::FJsonValue::makeNull(); }
    return StartNode::Reflect::toJson(*opt);
}

template <typename T>
auto toJsonAggregate(const T& object)
    -> std::expected<Json::FJsonValue, std::string>
{
    const auto* descriptor = UTypeRegistry::instance().find(makeTypeId<T>());
    if (!descriptor) {
        return std::unexpected(
            std::format("toJson: type is not registered"));
    }

    std::vector<std::pair<std::string, Json::FJsonValue>> members;
    const auto* base = reinterpret_cast<const char*>(&object);
    for (const auto& member : descriptor->members) {
        if (member.isTransient) { continue; }
        const void* memberPointer = base + member.offset;
        auto jsonMember = detail::dispatchToJson(member.typeId, memberPointer);
        if (!jsonMember) { return std::unexpected(jsonMember.error()); }
        members.emplace_back(member.name, std::move(*jsonMember));
    }
    return Json::FJsonValue::makeObject(std::move(members));
}

} // namespace detail

// ── Public toJson entry point ──

template <typename T>
auto toJson(const T& object) -> std::expected<Json::FJsonValue, std::string>
{
    if constexpr (detail::IsVector<T>) {
        return detail::toJsonVector(object);
    } else if constexpr (detail::IsOptional<T>) {
        return detail::toJsonOptional(object);
    } else if constexpr (std::is_same_v<T, bool> ||
                         std::is_integral_v<T> ||
                         std::is_floating_point_v<T> ||
                         std::is_same_v<T, std::string>)
    {
        return detail::toJsonScalar(object);
    } else {
        return detail::toJsonAggregate(object);
    }
}

// ── fromJson template implementation ──

namespace detail {

template <typename T>
auto fromJsonScalar(const Json::FJsonValue& json, T& value)
    -> std::expected<void, std::string>
{
    if constexpr (std::is_same_v<T, bool>) {
        if (json.getType() != Json::FJsonValue::EType::Bool) {
            return std::unexpected(std::string{"fromJson: expected bool"});
        }
        value = json.asBool();
    } else if constexpr (std::is_integral_v<T>) {
        if (json.getType() == Json::FJsonValue::EType::Int) {
            value = static_cast<T>(json.asInt());
        } else {
            return std::unexpected(std::string{"fromJson: expected int"});
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        if (json.getType() == Json::FJsonValue::EType::Double) {
            value = static_cast<T>(json.asDouble());
        } else if (json.getType() == Json::FJsonValue::EType::Int) {
            value = static_cast<T>(json.asInt());
        } else {
            return std::unexpected(std::string{"fromJson: expected number"});
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (json.getType() != Json::FJsonValue::EType::String) {
            return std::unexpected(std::string{"fromJson: expected string"});
        }
        value = json.asString();
    }
    return {};
}

template <typename T>
auto fromJsonVector(const Json::FJsonValue& json, std::vector<T>& vec)
    -> std::expected<void, std::string>
{
    if (json.getType() != Json::FJsonValue::EType::Array) {
        return std::unexpected(std::string{"fromJson: expected array"});
    }
    vec.clear();
    vec.reserve(json.asArray().size());
    for (const auto& element : json.asArray()) {
        T item{};
        auto result = StartNode::Reflect::fromJson(element, item);
        if (!result) { return std::unexpected(result.error()); }
        vec.push_back(std::move(item));
    }
    return {};
}

template <typename T>
auto fromJsonOptional(const Json::FJsonValue& json, std::optional<T>& opt)
    -> std::expected<void, std::string>
{
    if (json.getType() == Json::FJsonValue::EType::Null) {
        opt.reset();
        return {};
    }
    T value{};
    auto result = StartNode::Reflect::fromJson(json, value);
    if (!result) { return std::unexpected(result.error()); }
    opt = std::move(value);
    return {};
}

template <typename T>
auto fromJsonAggregate(const Json::FJsonValue& json, T& object)
    -> std::expected<void, std::string>
{
    if (json.getType() != Json::FJsonValue::EType::Object) {
        return std::unexpected(std::string{"fromJson: expected JSON object"});
    }

    const auto* descriptor = UTypeRegistry::instance().find(makeTypeId<T>());
    if (!descriptor) {
        return std::unexpected(std::string{"fromJson: type is not registered"});
    }

    const auto& jsonMembers = json.asObject();
    auto* base = reinterpret_cast<char*>(&object);

    for (const auto& memberDesc : descriptor->members) {
        if (memberDesc.isTransient) { continue; }

        const Json::FJsonValue* field = nullptr;
        for (const auto& [key, value] : jsonMembers) {
            if (key == memberDesc.name) { field = &value; break; }
        }
        if (!field) { continue; } // forward-compat: ignore unknown fields

        void* memberPointer = base + memberDesc.offset;
        auto result = detail::dispatchFromJson(
            memberDesc.typeId, *field, memberPointer);
        if (!result) { return std::unexpected(result.error()); }
    }
    return {};
}

} // namespace detail

// ── Public fromJson entry point ──

template <typename T>
auto fromJson(const Json::FJsonValue& json, T& object)
    -> std::expected<void, std::string>
{
    if constexpr (detail::IsVector<T>) {
        return detail::fromJsonVector(json, object);
    } else if constexpr (detail::IsOptional<T>) {
        return detail::fromJsonOptional(json, object);
    } else if constexpr (std::is_same_v<T, bool> ||
                         std::is_integral_v<T> ||
                         std::is_floating_point_v<T> ||
                         std::is_same_v<T, std::string>)
    {
        return detail::fromJsonScalar(json, object);
    } else {
        return detail::fromJsonAggregate(json, object);
    }
}

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_SERIALIZE_HPP
