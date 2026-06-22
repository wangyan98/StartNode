#ifndef STARTNODE_META_METACORE_HPP
#define STARTNODE_META_METACORE_HPP

#include <meta>
#include <optional>
#include <type_traits>

namespace StartNode::Meta {

// Compile-time annotation container.
//
// Stores a pointer to a consteval-generated static array of
// std::meta::info annotation handles. This is a CONSTEVAL-ONLY type:
// every accessor is consteval, because std::meta::info is a
// consteval-only type on GCC 16 and a struct that holds a
// `const std::meta::info*` becomes consteval-only itself — it cannot
// be stored in a runtime container or read at runtime.
//
// The imrefl UI path (compile-time Renderer dispatch) consumes this
// type entirely inside consteval contexts, where it works. The
// reflect runtime path (serialization / formatting) must NOT hold an
// FAnnotationSet; instead it pre-resolves whatever it needs (e.g.
// isTransient) into plain runtime types during consteval descriptor
// construction. See spec §9.1 risk #1.
struct FAnnotationSet
{
    const std::meta::info* annotations = nullptr;
    std::size_t count = 0;

    // ── Consteval path (for imrefl compile-time UI dispatch) ──

    template <typename T>
    consteval auto fetchAnnotation() const -> std::optional<T>
    {
        if (annotations == nullptr) { return {}; }
        for (std::size_t i = 0; i < count; ++i) {
            if (std::meta::remove_cvref(std::meta::type_of(annotations[i]))
                == std::meta::remove_cvref(^^T)) {
                return std::meta::extract<T>(annotations[i]);
            }
        }
        return {};
    }

    template <typename T>
    consteval auto hasAnnotation() const -> bool
    {
        return fetchAnnotation<T>().has_value();
    }
};

// Specialize for third-party types to attach annotations to fields
// you don't own. Field names must match. Annotations from this
// specialization are merged into FAnnotationSet at consteval time.
template <typename T>
struct ExternalAnnotations
{};

} // namespace StartNode::Meta

#endif // STARTNODE_META_METACORE_HPP
