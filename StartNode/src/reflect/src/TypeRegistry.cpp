#include <StartNode/Reflect/TypeRegistry.hpp>

#include <cstdint>
#include <string>

namespace StartNode::Reflect {

auto UTypeRegistry::instance() -> UTypeRegistry&
{
    static UTypeRegistry registry;
    return registry;
}

void registerBuiltinTypes()
{
    auto& reg = UTypeRegistry::instance();

    reg.registerType<bool>();
    reg.registerType<std::int32_t>();
    reg.registerType<std::int64_t>();
    reg.registerType<float>();
    reg.registerType<double>();
    reg.registerType<std::string>();
}

} // namespace StartNode::Reflect
