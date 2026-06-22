#include <gtest/gtest.h>
#include <StartNode/Reflect/Json.hpp>

#include <string>

using namespace StartNode::Reflect::Json;

// ── Test: Round-trip scalar values ──

TEST(Json, RoundTripNull)
{
    auto val = FJsonValue::makeNull();
    auto json = val.dump();
    EXPECT_EQ(json, "null");

    auto parsed = FJsonValue::parse(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->getType(), FJsonValue::EType::Null);
}

TEST(Json, RoundTripBool)
{
    auto val = FJsonValue::makeBool(true);
    auto json = val.dump();
    EXPECT_EQ(json, "true");

    auto parsed = FJsonValue::parse(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->getType(), FJsonValue::EType::Bool);
    EXPECT_TRUE(parsed->asBool());
}

TEST(Json, RoundTripInt)
{
    auto val = FJsonValue::makeInt(42);
    auto json = val.dump();
    EXPECT_EQ(json, "42");

    auto parsed = FJsonValue::parse(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->getType(), FJsonValue::EType::Int);
    EXPECT_EQ(parsed->asInt(), 42);
}

TEST(Json, RoundTripDouble)
{
    auto val = FJsonValue::makeDouble(3.14);
    auto json = val.dump();
    // Should contain "3.14"
    EXPECT_NE(json.find("3.14"), std::string::npos);

    auto parsed = FJsonValue::parse(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->getType(), FJsonValue::EType::Double);
    EXPECT_NEAR(parsed->asDouble(), 3.14, 0.001);
}

TEST(Json, RoundTripString)
{
    auto val = FJsonValue::makeString("hello world");
    auto json = val.dump();
    EXPECT_EQ(json, R"("hello world")");

    auto parsed = FJsonValue::parse(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->getType(), FJsonValue::EType::String);
    EXPECT_EQ(parsed->asString(), "hello world");
}

TEST(Json, RoundTripStringWithEscapes)
{
    auto val = FJsonValue::makeString("line1\nline2\t\"quoted\"");
    auto json = val.dump();
    auto parsed = FJsonValue::parse(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->asString(), "line1\nline2\t\"quoted\"");
}

TEST(Json, RoundTripArray)
{
    auto val = FJsonValue::makeArray({
        FJsonValue::makeInt(1),
        FJsonValue::makeInt(2),
        FJsonValue::makeString("three")
    });
    auto json = val.dump();
    EXPECT_EQ(json, R"([1,2,"three"])");

    auto parsed = FJsonValue::parse(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->getType(), FJsonValue::EType::Array);
    EXPECT_EQ(parsed->asArray().size(), 3u);
    EXPECT_EQ(parsed->asArray()[0].asInt(), 1);
    EXPECT_EQ(parsed->asArray()[2].asString(), "three");
}

TEST(Json, RoundTripObject)
{
    auto val = FJsonValue::makeObject({
        {"name", FJsonValue::makeString("test")},
        {"count", FJsonValue::makeInt(10)}
    });
    auto json = val.dump();
    EXPECT_EQ(json, R"({"name":"test","count":10})");

    auto parsed = FJsonValue::parse(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->getType(), FJsonValue::EType::Object);
    EXPECT_EQ(parsed->asObject().size(), 2u);
}

TEST(Json, PrettyPrint)
{
    auto val = FJsonValue::makeObject({
        {"key", FJsonValue::makeString("value")}
    });
    auto pretty = val.dump(2);
    EXPECT_NE(pretty.find('\n'), std::string::npos);
}

TEST(Json, ParseInvalid)
{
    EXPECT_FALSE(FJsonValue::parse("not json").has_value());
    EXPECT_FALSE(FJsonValue::parse("{unquoted}").has_value());
    EXPECT_FALSE(FJsonValue::parse("[1,").has_value());
    EXPECT_FALSE(FJsonValue::parse("").has_value());
}
