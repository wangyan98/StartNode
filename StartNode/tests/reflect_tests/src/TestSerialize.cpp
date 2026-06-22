#include <gtest/gtest.h>
#include <StartNode/Reflect/Serialize.hpp>
#include <StartNode/Reflect/Annotations.hpp>
#include <StartNode/Reflect/TypeRegistry.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace StartNode::Reflect;

// Test types
struct FVector3
{
    float x = 1.0f;
    float y = 2.0f;
    float z = 3.0f;
};

struct FPlayerInfo
{
    std::string name = "default";
    int score = 0;
    [[= StartNode::Anno::transient]]
    bool isOnline = false;
};

// ── Test: Scalar round-trip ──

TEST(Serialize, RoundTripInt)
{
    registerBuiltinTypes();

    int original = 42;
    auto json = toJson(original);
    ASSERT_TRUE(json.has_value()) << json.error();

    int restored = 0;
    auto result = fromJson(*json, restored);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(restored, 42);
}

TEST(Serialize, RoundTripFloat)
{
    registerBuiltinTypes();

    float original = 3.14f;
    auto json = toJson(original);
    ASSERT_TRUE(json.has_value());

    float restored = 0.0f;
    auto result = fromJson(*json, restored);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(restored, 3.14f, 0.001f);
}

TEST(Serialize, RoundTripString)
{
    registerBuiltinTypes();

    std::string original = "hello";
    auto json = toJson(original);
    ASSERT_TRUE(json.has_value());

    std::string restored;
    auto result = fromJson(*json, restored);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(restored, "hello");
}

// ── Test: Aggregate round-trip ──

TEST(Serialize, RoundTripAggregate)
{
    auto& reg = UTypeRegistry::instance();
    registerBuiltinTypes();
    reg.registerType<FVector3>();

    FVector3 original{10.0f, 20.0f, 30.0f};
    auto json = toJson(original);
    ASSERT_TRUE(json.has_value()) << json.error();

    FVector3 restored{};
    auto result = fromJson(*json, restored);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_FLOAT_EQ(restored.x, 10.0f);
    EXPECT_FLOAT_EQ(restored.y, 20.0f);
    EXPECT_FLOAT_EQ(restored.z, 30.0f);
}

// ── Test: Transient field skipped in serialization ──

TEST(Serialize, TransientSkipped)
{
    auto& reg = UTypeRegistry::instance();
    registerBuiltinTypes();
    reg.registerType<FPlayerInfo>();

    FPlayerInfo original;
    original.name = "Alice";
    original.score = 100;
    original.isOnline = true; // transient

    auto json = toJson(original);
    ASSERT_TRUE(json.has_value());

    // isOnline should NOT be in the JSON
    auto dumped = json->dump();
    EXPECT_NE(dumped.find("name"), std::string::npos);
    EXPECT_NE(dumped.find("score"), std::string::npos);
    EXPECT_EQ(dumped.find("isOnline"), std::string::npos);
}

// ── Test: Transient field not overwritten on deserialize ──

TEST(Serialize, TransientNotOverwritten)
{
    auto& reg = UTypeRegistry::instance();
    registerBuiltinTypes();
    reg.registerType<FPlayerInfo>();

    FPlayerInfo original;
    original.isOnline = true;

    auto json = toJson(original);
    ASSERT_TRUE(json.has_value());

    FPlayerInfo restored;
    restored.isOnline = true; // set before deserialize
    auto result = fromJson(*json, restored);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(restored.isOnline); // transient value preserved
}

// ── Test: Unknown JSON fields ignored (forward compatibility) ──

TEST(Serialize, UnknownFieldIgnored)
{
    auto& reg = UTypeRegistry::instance();
    registerBuiltinTypes();
    reg.registerType<FVector3>();

    auto json = Json::FJsonValue::parse(R"({"x":1.0,"y":2.0,"z":3.0,"extra":99})");
    ASSERT_TRUE(json.has_value());

    FVector3 restored{};
    auto result = fromJson(*json, restored);
    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(restored.x, 1.0f);
    EXPECT_FLOAT_EQ(restored.y, 2.0f);
    EXPECT_FLOAT_EQ(restored.z, 3.0f);
}

// ── Test: Type mismatch error ──

TEST(Serialize, TypeError)
{
    registerBuiltinTypes();

    auto json = Json::FJsonValue::makeString("not_an_int");

    int restored = 0;
    auto result = fromJson(json, restored);
    EXPECT_FALSE(result.has_value());
}

// ── Test: Unregistered type error ──
//
// FUnregisteredType is deliberately never registered with UTypeRegistry,
// so toJson must report an error rather than crash. Defined at file
// scope (not inside the test body) so reflection has a stable context.
struct FUnregisteredType { int x = 5; };

TEST(Serialize, UnregisteredType)
{
    FUnregisteredType obj;
    auto result = toJson(obj);
    EXPECT_FALSE(result.has_value());
}

// ── Test: vector round-trip ──

TEST(Serialize, RoundTripVector)
{
    registerBuiltinTypes();

    std::vector<float> original{1.0f, 2.0f, 3.0f};
    auto json = toJson(original);
    ASSERT_TRUE(json.has_value());

    std::vector<float> restored;
    auto result = fromJson(*json, restored);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(restored.size(), 3u);
    EXPECT_NEAR(restored[0], 1.0f, 0.001f);
    EXPECT_NEAR(restored[2], 3.0f, 0.001f);
}

// ── Test: optional round-trip ──

TEST(Serialize, RoundTripOptional)
{
    registerBuiltinTypes();

    std::optional<int> original = 42;
    auto json = toJson(original);
    ASSERT_TRUE(json.has_value());

    std::optional<int> restored;
    auto result = fromJson(*json, restored);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, 42);
}

TEST(Serialize, RoundTripOptionalNull)
{
    registerBuiltinTypes();

    std::optional<int> original = std::nullopt;
    auto json = toJson(original);
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ(json->getType(), Json::FJsonValue::EType::Null);
}
