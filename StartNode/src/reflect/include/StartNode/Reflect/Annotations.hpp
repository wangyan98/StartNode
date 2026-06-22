#ifndef STARTNODE_REFLECT_ANNOTATIONS_HPP
#define STARTNODE_REFLECT_ANNOTATIONS_HPP

#include <meta>
#include <string_view>

namespace StartNode::Anno {

// ── System-behavior annotations ──
//
// These are attached to reflected members using the GCC 16 annotation
// attribute syntax, which differs from the Clang P2996 draft the
// original spec assumed:
//
//   [[= StartNode::Anno::transient]]            // empty marker
//   [[= StartNode::Anno::category("UI")]]       // parameterized
//
// The leading `=` is required by GCC. Empty markers need a constexpr
// instance (provided below); parameterized annotations need a
// consteval factory (also provided below) so the attribute payload is
// a constant expression. The factory stashes string payloads into
// define_static_string storage so the annotation value points at
// static storage.

// Marker: member is excluded from serialization and diff.
struct FTransient {};
inline static constexpr FTransient transient {};

// Marks a member as deprecated with a migration message.
struct FDeprecated { const char* message; };
consteval FDeprecated deprecated(std::string_view message)
{
    return {std::define_static_string(message)};
}

// Records the version when a member was introduced.
struct FVersion { int since; };
consteval FVersion version(int since) { return {since}; }

// Clamps numeric range for validation and UI hints.
struct FRange { double min; double max; };
consteval FRange range(double min, double max) { return {min, max}; }

// Human-readable display name (overrides reflected member name).
struct FDisplayName { const char* name; };
consteval FDisplayName displayName(std::string_view name)
{
    return {std::define_static_string(name)};
}

// Tooltip text shown in UI and documentation generation.
struct FTooltip { const char* text; };
consteval FTooltip tooltip(std::string_view text)
{
    return {std::define_static_string(text)};
}

// Organizes members into logical categories (for UI grouping).
struct FCategory { const char* category; };
consteval FCategory category(std::string_view category)
{
    return {std::define_static_string(category)};
}

} // namespace StartNode::Anno

#endif // STARTNODE_REFLECT_ANNOTATIONS_HPP
