#include "TestTypeIdShared.hpp"

auto getCrossTUTypeId_A() -> StartNode::Reflect::FTypeId
{
    return StartNode::Reflect::makeTypeId<FCrossTUType>();
}
