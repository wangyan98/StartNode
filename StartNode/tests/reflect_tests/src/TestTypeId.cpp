#include <gtest/gtest.h>
#include "TestTypeIdShared.hpp"

using namespace StartNode::Reflect;

// ── Test: Distinct types have different IDs ──

TEST(TypeId, DistinctTypes)
{
    auto intId = makeTypeId<int>();
    auto floatId = makeTypeId<float>();
    auto doubleId = makeTypeId<double>();

    EXPECT_NE(intId, floatId);
    EXPECT_NE(floatId, doubleId);
}

// ── Test: Cross-translation-unit consistency ──

TEST(TypeId, CrossTranslationUnit)
{
    auto idA = getCrossTUTypeId_A();
    auto idB = getCrossTUTypeId_B();

    // Same type defined in two different .cpp files must produce
    // the same FTypeId (via fully-qualified name → consistent hash).
    EXPECT_EQ(idA, idB);
    EXPECT_EQ(idA.value, idB.value);
}
