#include <StartNode/Reflect/Serialize.hpp>

#include <StartNode/Reflect/TypeRegistry.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <unordered_map>
#include <utility>

namespace StartNode::Reflect::detail {

namespace {

// Shorthand for a successful void result — used so the fromJson
// dispatch lambdas have a single, explicit return type and GCC can
// deduce it.
inline auto ok() -> std::expected<void, std::string> { return {}; }

} // anonymous namespace

// Runtime dispatch table: FTypeId → toJson function pointer.
// Maps known scalar type ids to lambda converters; for aggregates
// recurses via FTypeDescriptor::members.
auto dispatchToJson(FTypeId typeId, const void* pointer)
    -> std::expected<Json::FJsonValue, std::string>
{
    using Dispatcher = auto(*)(const void*)
        -> std::expected<Json::FJsonValue, std::string>;

    static const std::unordered_map<FTypeId, Dispatcher> dispatchers = {
        { makeTypeId<bool>(), [](const void* p) -> std::expected<Json::FJsonValue, std::string> {
            return Json::FJsonValue::makeBool(*static_cast<const bool*>(p)); }},
        { makeTypeId<std::int32_t>(), [](const void* p) -> std::expected<Json::FJsonValue, std::string> {
            return Json::FJsonValue::makeInt(*static_cast<const std::int32_t*>(p)); }},
        { makeTypeId<std::int64_t>(), [](const void* p) -> std::expected<Json::FJsonValue, std::string> {
            return Json::FJsonValue::makeInt(*static_cast<const std::int64_t*>(p)); }},
        { makeTypeId<float>(), [](const void* p) -> std::expected<Json::FJsonValue, std::string> {
            return Json::FJsonValue::makeDouble(*static_cast<const float*>(p)); }},
        { makeTypeId<double>(), [](const void* p) -> std::expected<Json::FJsonValue, std::string> {
            return Json::FJsonValue::makeDouble(*static_cast<const double*>(p)); }},
        { makeTypeId<std::string>(), [](const void* p) -> std::expected<Json::FJsonValue, std::string> {
            return Json::FJsonValue::makeString(*static_cast<const std::string*>(p)); }},
    };

    auto it = dispatchers.find(typeId);
    if (it != dispatchers.end()) {
        return it->second(pointer);
    }

    // Aggregate: recurse via descriptor
    const auto* descriptor = UTypeRegistry::instance().find(typeId);
    if (!descriptor) {
        return std::unexpected(std::string{"dispatchToJson: unknown type id"});
    }

    std::vector<std::pair<std::string, Json::FJsonValue>> members;
    const auto* base = static_cast<const char*>(pointer);
    for (const auto& member : descriptor->members) {
        if (member.isTransient) { continue; }
        const void* memberPointer = base + member.offset;
        auto jsonMember = dispatchToJson(member.typeId, memberPointer);
        if (!jsonMember) { return std::unexpected(jsonMember.error()); }
        members.emplace_back(member.name, std::move(*jsonMember));
    }
    return Json::FJsonValue::makeObject(std::move(members));
}

// Runtime dispatch table: FTypeId → fromJson function pointer.
auto dispatchFromJson(FTypeId typeId, const Json::FJsonValue& json,
                      void* pointer) -> std::expected<void, std::string>
{
    using Dispatcher = auto(*)(const Json::FJsonValue&, void*)
        -> std::expected<void, std::string>;

    static const std::unordered_map<FTypeId, Dispatcher> dispatchers = {
        { makeTypeId<bool>(), [](const Json::FJsonValue& j, void* p) -> std::expected<void, std::string> {
            if (j.getType() != Json::FJsonValue::EType::Bool)
                return std::unexpected(std::string("fromJson: expected bool"));
            *static_cast<bool*>(p) = j.asBool();
            return {}; }},
        { makeTypeId<std::int32_t>(), [](const Json::FJsonValue& j, void* p) -> std::expected<void, std::string> {
            if (j.getType() != Json::FJsonValue::EType::Int)
                return std::unexpected(std::string("fromJson: expected int"));
            *static_cast<std::int32_t*>(p) = static_cast<std::int32_t>(j.asInt());
            return {}; }},
        { makeTypeId<std::int64_t>(), [](const Json::FJsonValue& j, void* p) -> std::expected<void, std::string> {
            if (j.getType() != Json::FJsonValue::EType::Int)
                return std::unexpected(std::string("fromJson: expected int64"));
            *static_cast<std::int64_t*>(p) = j.asInt();
            return {}; }},
        { makeTypeId<float>(), [](const Json::FJsonValue& j, void* p) -> std::expected<void, std::string> {
            if (j.getType() == Json::FJsonValue::EType::Double)
                *static_cast<float*>(p) = static_cast<float>(j.asDouble());
            else if (j.getType() == Json::FJsonValue::EType::Int)
                *static_cast<float*>(p) = static_cast<float>(j.asInt());
            else return std::unexpected(std::string("fromJson: expected number"));
            return {}; }},
        { makeTypeId<double>(), [](const Json::FJsonValue& j, void* p) -> std::expected<void, std::string> {
            if (j.getType() == Json::FJsonValue::EType::Double)
                *static_cast<double*>(p) = j.asDouble();
            else if (j.getType() == Json::FJsonValue::EType::Int)
                *static_cast<double*>(p) = static_cast<double>(j.asInt());
            else return std::unexpected(std::string("fromJson: expected number"));
            return {}; }},
        { makeTypeId<std::string>(), [](const Json::FJsonValue& j, void* p) -> std::expected<void, std::string> {
            if (j.getType() != Json::FJsonValue::EType::String)
                return std::unexpected(std::string("fromJson: expected string"));
            *static_cast<std::string*>(p) = j.asString();
            return {}; }},
    };

    auto it = dispatchers.find(typeId);
    if (it != dispatchers.end()) {
        return it->second(json, pointer);
    }

    // Aggregate: recurse via descriptor
    if (json.getType() != Json::FJsonValue::EType::Object) {
        return std::unexpected(std::string("fromJson: expected JSON object for aggregate"));
    }

    const auto* descriptor = UTypeRegistry::instance().find(typeId);
    if (!descriptor) {
        return std::unexpected(std::string("dispatchFromJson: unknown type id"));
    }

    const auto& jsonMembers = json.asObject();
    auto* base = static_cast<char*>(pointer);
    for (const auto& memberDesc : descriptor->members) {
        if (memberDesc.isTransient) { continue; }
        const Json::FJsonValue* field = nullptr;
        for (const auto& [key, value] : jsonMembers) {
            if (key == memberDesc.name) { field = &value; break; }
        }
        if (!field) { continue; }
        void* memberPointer = base + memberDesc.offset;
        auto result = dispatchFromJson(memberDesc.typeId, *field, memberPointer);
        if (!result) { return std::unexpected(result.error()); }
    }
    return {};
}

} // namespace StartNode::Reflect::detail
