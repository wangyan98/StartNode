#include "TestTypeIdShared.hpp"

auto getCrossTUTypeId_B() -> StartNode::Reflect::FTypeId
{
    return StartNode::Reflect::makeTypeId<FCrossTUType>();
}
