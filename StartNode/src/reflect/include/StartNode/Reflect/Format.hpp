#ifndef STARTNODE_REFLECT_FORMAT_HPP
#define STARTNODE_REFLECT_FORMAT_HPP

#include <StartNode/Reflect/TypeRegistry.hpp>

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace StartNode::Reflect {

namespace detail {

template <typename T>
concept IsVector = requires { typename T::value_type; }
    && std::same_as<T, std::vector<typename T::value_type>>;

template <typename T>
concept IsOptional = requires { typename T::value_type; }
    && std::same_as<T, std::optional<typename T::value_type>>;

// Forward declaration for recursive dispatch
template <typename T>
auto formatReflectedImpl(const T& object) -> std::string;

// Format a scalar via std::format (works for builtins with
// std::formatter, plus std::string).
template <typename T>
auto formatScalar(const T& value) -> std::string
{
    return std::format("{}", value);
}

// Format a vector: [elem1, elem2, ...]
template <typename T>
auto formatVector(const std::vector<T>& vec) -> std::string
{
    std::string result = "[";
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) { result += ", "; }
        result += formatReflectedImpl(vec[i]);
    }
    result += "]";
    return result;
}

// Format an optional: Some(value) or None
template <typename T>
auto formatOptional(const std::optional<T>& opt) -> std::string
{
    if (opt.has_value()) {
        return std::format("Some({})", formatReflectedImpl(*opt));
    }
    return "None";
}

// Runtime format dispatch for a member given its type id and pointer.
// Appends the formatted representation to `result`. Scalars are matched
// against the pre-registered builtin type ids; anything else recurses
// into formatAggregatePtr as a nested aggregate.
inline void formatMemberByTypeId(FTypeId typeId, const void* ptr,
                                 std::string& result);

// Format an aggregate (by type id + pointer) by walking its member
// descriptors. Used both for the top-level object and for nested
// aggregates reached via formatMemberByTypeId.
inline auto formatAggregatePtr(FTypeId typeId, const void* ptr) -> std::string
{
    const auto* desc = UTypeRegistry::instance().find(typeId);
    if (!desc) { return "<unregistered type>"; }

    std::string result;
    result += desc->name;
    result += '{';
    const auto* base = static_cast<const char*>(ptr);
    bool first = true;
    for (const auto& member : desc->members) {
        if (!first) { result += ", "; }
        first = false;
        result += member.name;
        result += '=';
        formatMemberByTypeId(member.typeId, base + member.offset, result);
    }
    result += '}';
    return result;
}

inline void formatMemberByTypeId(FTypeId typeId, const void* ptr,
                                 std::string& result)
{
    const auto boolId = makeTypeId<bool>();
    const auto int32Id = makeTypeId<std::int32_t>();
    const auto int64Id = makeTypeId<std::int64_t>();
    const auto floatId = makeTypeId<float>();
    const auto doubleId = makeTypeId<double>();
    const auto stringId = makeTypeId<std::string>();

    if (typeId == boolId) {
        result += std::format("{}", *static_cast<const bool*>(ptr));
    } else if (typeId == int32Id) {
        result += std::format("{}", *static_cast<const std::int32_t*>(ptr));
    } else if (typeId == int64Id) {
        result += std::format("{}", *static_cast<const std::int64_t*>(ptr));
    } else if (typeId == floatId) {
        result += std::format("{}", *static_cast<const float*>(ptr));
    } else if (typeId == doubleId) {
        result += std::format("{}", *static_cast<const double*>(ptr));
    } else if (typeId == stringId) {
        result += '"';
        result += *static_cast<const std::string*>(ptr);
        result += '"';
    } else {
        // Aggregate — recurse
        result += formatAggregatePtr(typeId, ptr);
    }
}

// Format an aggregate by walking its member descriptors. Used when the
// type T is statically known (top-level formatReflected entry).
template <typename T>
auto formatAggregate(const T& object) -> std::string
{
    return formatAggregatePtr(makeTypeId<T>(), &object);
}

// Top-level dispatch
template <typename T>
auto formatReflectedImpl(const T& object) -> std::string
{
    if constexpr (IsVector<T>) {
        return formatVector(object);
    } else if constexpr (IsOptional<T>) {
        return formatOptional(object);
    } else if constexpr (std::is_same_v<T, bool> ||
                         std::is_integral_v<T> ||
                         std::is_floating_point_v<T> ||
                         std::is_same_v<T, std::string>)
    {
        return formatScalar(object);
    } else {
        return formatAggregate(object);
    }
}

} // namespace detail

// Public entry point for formatting any reflected type.
template <typename T>
auto formatReflected(const T& object) -> std::string
{
    return detail::formatReflectedImpl(object);
}

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_FORMAT_HPP
