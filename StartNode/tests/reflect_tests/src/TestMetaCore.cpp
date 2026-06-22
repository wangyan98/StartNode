#include <gtest/gtest.h>
#include <StartNode/Meta/MetaCore.hpp>
#include <StartNode/Reflect/Annotations.hpp>

#include <meta>

// Test type carrying GCC-style annotations. The [[= ...]] form (leading
// `=`) is the GCC 16 annotation-attribute syntax; Clang's [[...]] form
// assumed by the original spec is not supported by GCC.
struct FTestAnnotated
{
    [[= StartNode::Anno::transient]]
    int transientField = 0;

    [[= StartNode::Anno::category("Test")]]
    float categoryField = 1.0f;
};

// External annotations for a third-party type.
struct FThirdParty
{
    int value = 42;
};

template <>
struct StartNode::Meta::ExternalAnnotations<FThirdParty>
{
    [[= StartNode::Anno::transient]]
    int value;
};

// ── Test: FAnnotationSet is consteval-usable ──
//
// FAnnotationSet holds const std::meta::info*, which on GCC 16 makes
// the whole type consteval-only — it cannot be constructed or read at
// runtime. Verify it works inside a constexpr context instead.

TEST(MetaCore, AnnotationSetConsteval)
{
    constexpr bool nullWhenDefault = [] {
        StartNode::Meta::FAnnotationSet set{};
        return set.annotations == nullptr && set.count == 0;
    }();
    EXPECT_TRUE(nullWhenDefault);
}

// ── Test: consteval annotation fetch resolves FTransient ──
//
// Build an FAnnotationSet from the reflected annotations of
// FTestAnnotated::transientField inside a consteval context, then
// confirm hasAnnotation<FTransient>() is true there and false for the
// category field.

namespace {

consteval bool transientFieldIsTransient()
{
    const auto ctx = std::meta::access_context::current();
    const auto mems =
        std::meta::nonstatic_data_members_of(^^FTestAnnotated, ctx);
    for (const auto m : mems) {
        if (std::meta::identifier_of(m) == "transientField") {
            const auto anns = std::meta::annotations_of(m);
            StartNode::Meta::FAnnotationSet set{anns.data(), anns.size()};
            return set.hasAnnotation<StartNode::Anno::FTransient>();
        }
    }
    return false;
}

consteval bool categoryFieldIsTransient()
{
    const auto ctx = std::meta::access_context::current();
    const auto mems =
        std::meta::nonstatic_data_members_of(^^FTestAnnotated, ctx);
    for (const auto m : mems) {
        if (std::meta::identifier_of(m) == "categoryField") {
            const auto anns = std::meta::annotations_of(m);
            StartNode::Meta::FAnnotationSet set{anns.data(), anns.size()};
            return set.hasAnnotation<StartNode::Anno::FTransient>();
        }
    }
    return true; // sentinel: should not reach
}

} // anonymous namespace

TEST(MetaCore, ConstevalAnnotationFetch)
{
    constexpr bool transientIsTransient = transientFieldIsTransient();
    constexpr bool categoryIsTransient = categoryFieldIsTransient();
    EXPECT_TRUE(transientIsTransient);
    EXPECT_FALSE(categoryIsTransient);
}

// ── Test: ExternalAnnotations specialization is visible ──
//
// Compile-time check: the specialization above compiles, so the
// template can be substituted in a consteval context.

TEST(MetaCore, ExternalAnnotationsExists)
{
    constexpr bool externalIsClass = std::meta::is_class_type(
        std::meta::substitute(^^StartNode::Meta::ExternalAnnotations,
                              {^^FThirdParty}));
    EXPECT_TRUE(externalIsClass);
    SUCCEED();
}
