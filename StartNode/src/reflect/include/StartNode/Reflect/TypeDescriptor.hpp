#ifndef STARTNODE_REFLECT_TYPEDESCRIPTOR_HPP
#define STARTNODE_REFLECT_TYPEDESCRIPTOR_HPP

#include <StartNode/Meta/MetaCore.hpp>
#include <StartNode/Reflect/Annotations.hpp>
#include <StartNode/Reflect/TypeId.hpp>

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>

namespace StartNode::Reflect {

// Maximum length stored for a member or type name. Member/type names
// are copied into fixed-size char buffers (rather than held as
// `const char*`) so that FMemberDescriptor / FTypeDescriptor remain
// STRUCTURAL types. This is required by GCC 16's std::define_static_array
// (which backs consteval descriptor materialization): structural types
// cannot contain pointers, and `const char*` would make the struct
// non-structural and the consteval-to-static promotion would fail.
inline constexpr std::size_t kMaxNameLength = 64;

// Describes a single non-static data member of a reflected type.
//
// This is a plain runtime type — no std::meta::info anywhere, so it
// can be stored in runtime containers (unordered_map, etc.) and read
// at runtime. All reflection-derived data (name, offset, type id,
// transient flag) is pre-resolved during consteval construction.
struct FMemberDescriptor
{
    char name[kMaxNameLength] = {};  // Member name (copied, NUL-terminated)
    std::size_t offset = 0;          // Byte offset from object start
    FTypeId typeId;                  // Compile-time hash of member type
    bool isTransient = false;        // Pre-resolved from FTransient annotation
};

// Runtime descriptor for a reflected type. Holds a span over a
// consteval-generated static array of FMemberDescriptor. Includes
// type-erased lifecycle functions for generic construction/destruction.
struct FTypeDescriptor
{
    FTypeId typeId;
    char name[kMaxNameLength] = {};              // Type name (copied)
    std::size_t sizeOf = 0;
    std::span<const FMemberDescriptor> members;  // View into static data

    // Type-erased factory functions
    void (*defaultConstruct)(void* memory) = nullptr;
    void (*copyConstruct)(void* memory, const void* source) = nullptr;
    void (*destruct)(void* memory) = nullptr;
};

// ── Generate FTypeDescriptor at compile time ──

namespace detail {

// Copy a string view into a fixed-size NUL-terminated buffer (consteval-
// safe; std::strcpy is not constexpr). Truncates to kMaxNameLength-1.
consteval void copyName(char* dst, std::string_view src)
{
    const std::size_t n = std::min<std::size_t>(src.size(), kMaxNameLength - 1);
    for (std::size_t i = 0; i < n; ++i) { dst[i] = src[i]; }
    dst[n] = '\0';
}

// True if `member` carries a [[= StartNode::Anno::transient]] annotation
// (or the equivalent via ExternalAnnotations<T>).
// Only meaningful for class types — primitives have no members.
template <typename T>
consteval bool memberIsTransient(std::meta::info member)
{
    const auto ctx = std::meta::access_context::current();
    const auto transientType = std::meta::remove_cvref(^^StartNode::Anno::FTransient);

    auto checkAnns = [&](std::meta::info m) {
        for (const auto a : std::meta::annotations_of(m)) {
            if (std::meta::remove_cvref(std::meta::type_of(a)) == transientType) {
                return true;
            }
        }
        return false;
    };

    if (checkAnns(member)) { return true; }

    // ExternalAnnotations<T> may annotate a field the author doesn't own.
    const auto external =
        std::meta::substitute(^^Meta::ExternalAnnotations, {^^T});
    if (std::meta::is_class_type(external)) {
        for (const auto extMember : std::meta::nonstatic_data_members_of(external, ctx)) {
            if (std::meta::identifier_of(member) == std::meta::identifier_of(extMember)) {
                if (checkAnns(extMember)) { return true; }
            }
        }
    }
    return false;
}

// Build a vector of FMemberDescriptor for type T by reflecting over its
// non-static data members. Returned by value to a consteval caller that
// immediately promotes it to static storage via define_static_array.
// Primitives and non-class types yield an empty member list.
template <typename T>
consteval auto buildMemberDescriptors() -> std::vector<FMemberDescriptor>
{
    if (!std::meta::is_class_type(^^T)) {
        return {};
    }
    const auto ctx = std::meta::access_context::current();
    const auto memberList = std::meta::nonstatic_data_members_of(^^T, ctx);

    std::vector<FMemberDescriptor> result;
    for (const auto member : memberList) {
        FMemberDescriptor desc{};
        copyName(desc.name, std::meta::identifier_of(member));
        desc.offset = static_cast<std::size_t>(std::meta::offset_of(member).bytes);
        desc.typeId = makeTypeIdFromInfo(std::meta::type_of(member));
        desc.isTransient = memberIsTransient<T>(member);
        result.push_back(desc);
    }
    return result;
}

} // namespace detail

// Compile-time helper: builds an FTypeDescriptor for T. The descriptor
// is a value (not a reference) so it can be copied into the runtime
// UTypeRegistry. The members span points at consteval-generated static
// storage whose lifetime is the whole program.
template <typename T>
consteval auto makeTypeDescriptor() -> FTypeDescriptor
{
    FTypeDescriptor desc{};
    desc.typeId = makeTypeId<T>();
    detail::copyName(desc.name, std::meta::display_string_of(^^T));
    desc.sizeOf = sizeof(T);

    auto memberVec = detail::buildMemberDescriptors<T>();
    auto staticMembers = std::define_static_array(memberVec);
    desc.members = std::span<const FMemberDescriptor>{
        staticMembers.data(), staticMembers.size()};

    desc.defaultConstruct = [](void* memory) { new (memory) T(); };
    desc.copyConstruct = [](void* memory, const void* source) {
        new (memory) T(*static_cast<const T*>(source));
    };
    desc.destruct = [](void* memory) { static_cast<T*>(memory)->~T(); };

    return desc;
}

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_TYPEDESCRIPTOR_HPP
