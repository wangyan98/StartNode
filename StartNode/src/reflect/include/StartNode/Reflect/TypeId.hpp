#ifndef STARTNODE_REFLECT_TYPEID_HPP
#define STARTNODE_REFLECT_TYPEID_HPP

#include <cstdint>
#include <meta>
#include <string_view>

namespace StartNode::Reflect {

// 64-bit type identifier generated from the fully-qualified type name
// at compile time via consteval hashing. Consistent across translation
// units and shared libraries for the same type.
struct FTypeId
{
    std::uint64_t value = 0;

    auto operator==(const FTypeId& other) const -> bool = default;
    auto operator!=(const FTypeId& other) const -> bool = default;
};

} // namespace StartNode::Reflect

// std::hash support for FTypeId so it can be used as unordered_map key.
template <>
struct std::hash<StartNode::Reflect::FTypeId>
{
    auto operator()(const StartNode::Reflect::FTypeId& id) const -> std::size_t
    {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

namespace StartNode::Reflect {

namespace detail {

// FNV-1a 64-bit hash — consteval for compile-time use.
consteval auto fnv1a64(std::string_view str) -> std::uint64_t
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offsetBasis;
    for (char c : str) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= prime;
    }
    return hash;
}

// Build a type id directly from a reflection info handle. Used inside
// consteval member enumeration where the member type is only available
// as a std::meta::info (splicing it into a template argument via
// [: type_of(member) :] is rejected by GCC 16 in a template body).
consteval auto makeTypeIdFromInfo(std::meta::info typeInfo) -> FTypeId
{
    return FTypeId{fnv1a64(std::meta::display_string_of(typeInfo))};
}

} // namespace detail

// Generate a compile-time type ID from the type's fully-qualified
// reflection display string. Same type → same string → same hash
// across all translation units.
template <typename T>
consteval auto makeTypeId() -> FTypeId
{
    return detail::makeTypeIdFromInfo(^^T);
}

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_TYPEID_HPP
