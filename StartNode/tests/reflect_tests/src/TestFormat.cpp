#include <gtest/gtest.h>
#include <StartNode/Reflect/Format.hpp>
#include <StartNode/Reflect/TypeRegistry.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace StartNode::Reflect;

struct FTestFormat
{
    int count = 5;
    float value = 3.14f;
};

// Deliberately never registered — formatReflected must render it as
// <unregistered type> rather than crash. Defined at file scope so
// reflection has a stable access context.
struct FUnregisteredFormatType { int x = 42; };

// ── Test: Format aggregate ──

TEST(Format, FormatAggregate)
{
    auto& reg = UTypeRegistry::instance();
    registerBuiltinTypes();
    reg.registerType<FTestFormat>();

    FTestFormat obj;
    auto formatted = formatReflected(obj);
    EXPECT_NE(formatted.find("FTestFormat"), std::string::npos);
    EXPECT_NE(formatted.find("count=5"), std::string::npos);
    EXPECT_NE(formatted.find("value="), std::string::npos);
}

// ── Test: Format unregistered type ──

TEST(Format, FormatUnregistered)
{
    FUnregisteredFormatType obj;
    auto formatted = formatReflected(obj);
    EXPECT_EQ(formatted, "<unregistered type>");
}

// ── Test: Format vector ──

TEST(Format, FormatVector)
{
    registerBuiltinTypes();
    std::vector<int> vec{1, 2, 3};
    auto formatted = formatReflected(vec);
    EXPECT_NE(formatted.find("[1, 2, 3]"), std::string::npos);
}

// ── Test: Format optional ──

TEST(Format, FormatOptionalSome)
{
    registerBuiltinTypes();
    std::optional<int> opt = 10;
    auto formatted = formatReflected(opt);
    EXPECT_NE(formatted.find("Some(10)"), std::string::npos);
}

TEST(Format, FormatOptionalNone)
{
    registerBuiltinTypes();
    std::optional<int> opt;
    auto formatted = formatReflected(opt);
    EXPECT_EQ(formatted, "None");
}
