#include <gtest/gtest.h>
#include <StartNode/Reflect/TypeRegistry.hpp>
#include <StartNode/Reflect/Annotations.hpp>

#include <string_view>

using namespace StartNode::Reflect;

// Test aggregate type
struct FTestPoint
{
    float x = 1.0f;
    float y = 2.0f;
    [[= StartNode::Anno::transient]]
    int cachedHash = 0;
};

struct FEmptyStruct
{};

// ── Test: Register and find by id ──

TEST(TypeRegistry, RegisterAndFindById)
{
    auto& reg = UTypeRegistry::instance();
    reg.registerType<FTestPoint>();

    const auto id = makeTypeId<FTestPoint>();
    const auto* desc = reg.find(id);
    ASSERT_NE(desc, nullptr);
    EXPECT_GT(desc->sizeOf, 0u);
}

// ── Test: Register and find by name ──

TEST(TypeRegistry, RegisterAndFindByName)
{
    auto& reg = UTypeRegistry::instance();
    reg.registerType<FTestPoint>();

    const auto id = makeTypeId<FTestPoint>();
    const auto* desc = reg.find(id);
    ASSERT_NE(desc, nullptr);

    const auto* byName = reg.find(std::string_view(desc->name));
    ASSERT_NE(byName, nullptr);
    EXPECT_EQ(byName->typeId, id);
}

// ── Test: Member enumeration ──

TEST(TypeDescriptor, MemberEnumeration)
{
    auto& reg = UTypeRegistry::instance();
    reg.registerType<FTestPoint>();

    const auto id = makeTypeId<FTestPoint>();
    const auto* desc = reg.find(id);
    ASSERT_NE(desc, nullptr);

    // FTestPoint has 3 members: x, y, cachedHash
    EXPECT_EQ(desc->members.size(), 3u);

    // Check member names
    bool foundX = false, foundY = false, foundHash = false;
    for (const auto& m : desc->members) {
        if (std::string_view(m.name) == "x") {
            foundX = true;
            EXPECT_FALSE(m.isTransient);
        } else if (std::string_view(m.name) == "y") {
            foundY = true;
            EXPECT_FALSE(m.isTransient);
        } else if (std::string_view(m.name) == "cachedHash") {
            foundHash = true;
            EXPECT_TRUE(m.isTransient);
        }
    }
    EXPECT_TRUE(foundX);
    EXPECT_TRUE(foundY);
    EXPECT_TRUE(foundHash);
}

// ── Test: Factory functions ──

TEST(TypeDescriptor, FactoryFunctions)
{
    auto& reg = UTypeRegistry::instance();
    reg.registerType<FTestPoint>();

    const auto id = makeTypeId<FTestPoint>();
    const auto* desc = reg.find(id);
    ASSERT_NE(desc, nullptr);
    ASSERT_NE(desc->defaultConstruct, nullptr);
    ASSERT_NE(desc->copyConstruct, nullptr);
    ASSERT_NE(desc->destruct, nullptr);

    // Default construct
    alignas(FTestPoint) char memory[sizeof(FTestPoint)];
    desc->defaultConstruct(memory);
    auto* obj = reinterpret_cast<FTestPoint*>(memory);
    EXPECT_FLOAT_EQ(obj->x, 1.0f);
    EXPECT_FLOAT_EQ(obj->y, 2.0f);

    // Copy construct
    FTestPoint source;
    source.x = 10.0f;
    source.y = 20.0f;
    alignas(FTestPoint) char memory2[sizeof(FTestPoint)];
    desc->copyConstruct(memory2, &source);
    auto* obj2 = reinterpret_cast<FTestPoint*>(memory2);
    EXPECT_FLOAT_EQ(obj2->x, 10.0f);
    EXPECT_FLOAT_EQ(obj2->y, 20.0f);

    // Destruct (no crash)
    desc->destruct(memory);
    desc->destruct(memory2);
}

// ── Test: Builtin types ──

TEST(TypeRegistry, BuiltinTypes)
{
    auto& reg = UTypeRegistry::instance();
    registerBuiltinTypes();

    EXPECT_NE(reg.find(makeTypeId<bool>()), nullptr);
    EXPECT_NE(reg.find(makeTypeId<std::int32_t>()), nullptr);
    EXPECT_NE(reg.find(makeTypeId<float>()), nullptr);
    EXPECT_NE(reg.find(makeTypeId<double>()), nullptr);
    EXPECT_NE(reg.find(makeTypeId<std::string>()), nullptr);
}

// ── Test: Empty struct ──

TEST(TypeDescriptor, EmptyStruct)
{
    auto& reg = UTypeRegistry::instance();
    reg.registerType<FEmptyStruct>();

    const auto id = makeTypeId<FEmptyStruct>();
    const auto* desc = reg.find(id);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->members.size(), 0u);
}
