#ifndef TEST_TYPEID_SHARED_HPP
#define TEST_TYPEID_SHARED_HPP

#include <StartNode/Reflect/TypeId.hpp>

// Shared type used by TestTypeIdCrossTU — must be identical in both TUs
struct FCrossTUType
{
    int a = 1;
    double b = 2.0;
};

// Function declared here, defined in both TestTypeIdCrossTU_A.cpp
// and TestTypeIdCrossTU_B.cpp. They must return the same FTypeId.
auto getCrossTUTypeId_A() -> StartNode::Reflect::FTypeId;
auto getCrossTUTypeId_B() -> StartNode::Reflect::FTypeId;

#endif // TEST_TYPEID_SHARED_HPP
