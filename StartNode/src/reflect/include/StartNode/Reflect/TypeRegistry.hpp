#ifndef STARTNODE_REFLECT_TYPEREGISTRY_HPP
#define STARTNODE_REFLECT_TYPEREGISTRY_HPP

#include <StartNode/Reflect/TypeDescriptor.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace StartNode::Reflect {

// Singleton registry mapping FTypeId → FTypeDescriptor.
// Types must be explicitly registered before serialization or
// formatting. All registration is single-threaded in this spec.
class UTypeRegistry
{
public:
    static auto instance() -> UTypeRegistry&;

    // Register a type T. Generates FTypeDescriptor at compile time
    // via consteval reflection. Safe to call multiple times (no-op).
    template <typename T>
    void registerType()
    {
        constexpr auto descriptor = makeTypeDescriptor<T>();
        const auto id = descriptor.typeId;
        if (registry.find(id) == registry.end()) {
            registry[id] = descriptor;
            nameIndex[descriptor.name] = id;
        }
    }

    // Look up by type id. Returns nullptr if not registered.
    auto find(FTypeId id) const -> const FTypeDescriptor*
    {
        const auto it = registry.find(id);
        if (it != registry.end()) { return &it->second; }
        return nullptr;
    }

    // Look up by type name. Returns nullptr if not registered.
    auto find(std::string_view name) const -> const FTypeDescriptor*
    {
        const auto it = nameIndex.find(std::string(name));
        if (it != nameIndex.end()) { return find(it->second); }
        return nullptr;
    }

    // Snapshot of all registered type descriptors.
    auto registeredTypes() const -> std::vector<const FTypeDescriptor*>
    {
        std::vector<const FTypeDescriptor*> result;
        result.reserve(registry.size());
        for (const auto& [id, desc] : registry) {
            result.push_back(&desc);
        }
        return result;
    }

private:
    UTypeRegistry() = default;

    std::unordered_map<FTypeId, FTypeDescriptor> registry;
    std::unordered_map<std::string, FTypeId> nameIndex;
};

// Register all built-in scalar types (int, float, double, bool,
// std::string, etc.) that serialization dispatch depends on.
// Must be called explicitly before any serialize/format operation.
void registerBuiltinTypes();

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_TYPEREGISTRY_HPP
