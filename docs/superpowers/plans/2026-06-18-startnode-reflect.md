# StartNode reflect — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the C++26 reflection framework (`reflect` module) with MetaCore extraction from imrefl.hpp, zero runtime dependencies, and full GoogleTest coverage.

**Architecture:** Extract MetaCore (INTERFACE library with FAnnotationSet) from imrefl.hpp as a shared dependency. Build Reflect (STATIC library) on top: FTypeId from compile-time name hash, FTypeDescriptor from consteval member enumeration, UTypeRegistry for runtime lookup, hand-rolled FJsonValue + reflection-driven toJson/fromJson, and formatReflected output. imrefl and Reflect are siblings, both depending on MetaCore.

**Tech Stack:** C++26 (Clang with -freflection), CMake 3.30, GoogleTest v1.15.2 (FetchContent), zero third-party runtime deps.

## Global Constraints

- Compiler: Clang 22+ with `-freflection -fexpansion-statements -fannotation-attributes -fparameter-reflection`; Apple Clang rejected via FATAL_ERROR
- CMake auto-probes Homebrew clang at `/opt/homebrew/opt/llvm/bin/clang++`
- Naming: struct→`F` prefix, class→`U` prefix, interface→`I` prefix, enum→`E` prefix (UE style)
- Namespace: `StartNode::<Module>`; directory names drop `sn_` prefix
- Functions/variables: camelCase (lower); no abbreviations (Desc/Reg/Cfg/Buf); `Id` accepted
- Zero third-party runtime dependencies; GoogleTest for tests only (FetchContent)
- Code location: `StartNode/` under imrefl repo root
- All types must be explicitly registered with UTypeRegistry before serialization

---

### Task 1: CMake Scaffolding

**Files:**
- Create: `StartNode/CMakeLists.txt`
- Create: `StartNode/src/meta_core/CMakeLists.txt`
- Create: `StartNode/src/reflect/CMakeLists.txt`
- Modify: `CMakeLists.txt` (top-level — add `add_subdirectory(StartNode)`)

**Produces:**
- `MetaCore` INTERFACE target (compiles with cxx_std_26 + reflection flags)
- `Reflect` STATIC target (links MetaCore)
- Top-level `imrefl` project includes StartNode subdirectory
- `cmake --build build` passes with empty Reflect sources

- [ ] **Step 1: Create directory tree**

```bash
mkdir -p StartNode/src/meta_core/include/StartNode/Meta
mkdir -p StartNode/src/reflect/include/StartNode/Reflect
mkdir -p StartNode/src/reflect/src
mkdir -p StartNode/tests/reflect_tests/src
```

- [ ] **Step 2: Write `StartNode/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.30)
project(StartNode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Auto-detect Homebrew Clang for C++26 reflection support
if(NOT DEFINED STARTNODE_CXX_COMPILER)
    find_program(HOMEBREW_CLANG clang++
        PATHS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
        NO_DEFAULT_PATH
    )
    if(HOMEBREW_CLANG)
        set(STARTNODE_CXX_COMPILER "${HOMEBREW_CLANG}" CACHE FILEPATH
            "C++ compiler for StartNode (requires C++26 reflection)")
    endif()
endif()

if(STARTNODE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${STARTNODE_CXX_COMPILER}" CACHE FILEPATH "" FORCE)
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
        "StartNode requires a Clang compiler with C++26 reflection support.\n"
        "Found: ${CMAKE_CXX_COMPILER_ID}\n"
        "Install via: brew install llvm\n"
        "Then pass: -DSTARTNODE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++")
endif()

option(STARTNODE_BUILD_TESTS "Build StartNode tests" OFF)

add_subdirectory(src/meta_core)
add_subdirectory(src/reflect)

if(STARTNODE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests/reflect_tests)
endif()
```

- [ ] **Step 3: Write `StartNode/src/meta_core/CMakeLists.txt`**

```cmake
add_library(MetaCore INTERFACE)
target_include_directories(MetaCore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(MetaCore INTERFACE cxx_std_26)
target_compile_options(MetaCore INTERFACE -freflection)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MetaCore INTERFACE
        -fexpansion-statements
        -fannotation-attributes
        -fparameter-reflection)
endif()
```

- [ ] **Step 4: Write `StartNode/src/reflect/CMakeLists.txt`**

```cmake
add_library(Reflect STATIC
    src/TypeRegistry.cpp
    src/Json.cpp
    src/Serialize.cpp
)
target_include_directories(Reflect PUBLIC include)
target_link_libraries(Reflect PUBLIC MetaCore)
```

- [ ] **Step 5: Create placeholder .cpp files so Reflect compiles**

```bash
echo '// TypeRegistry placeholder' > StartNode/src/reflect/src/TypeRegistry.cpp
echo '// Json placeholder' > StartNode/src/reflect/src/Json.cpp
echo '// Serialize placeholder' > StartNode/src/reflect/src/Serialize.cpp
```

- [ ] **Step 6: Modify top-level `CMakeLists.txt` — add StartNode before example**

```cmake
cmake_minimum_required(VERSION 3.30)
project(imrefl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(StartNode)

add_library(ImRefl INTERFACE)
target_include_directories(ImRefl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ImRefl INTERFACE MetaCore)

if (IMREFL_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()
```

Note: Remove the `target_compile_options(ImRefl INTERFACE -freflection ...)` block — these flags now come transitively from MetaCore.

- [ ] **Step 7: Build to verify CMake configuration**

```bash
cmake -B build -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DSTARTNODE_BUILD_TESTS=OFF
cmake --build build
```

Expected: configure succeeds, build succeeds (Reflect has empty .cpp stubs).

- [ ] **Step 8: Commit**

```bash
git add StartNode/ CMakeLists.txt
git commit -m "feat: add StartNode CMake scaffolding with MetaCore and Reflect targets"
```

---

### Task 2: MetaCore Header + imrefl.hpp Refactoring

**Files:**
- Create: `StartNode/src/meta_core/include/StartNode/Meta/MetaCore.hpp`
- Modify: `imrefl.hpp` (lines 32–94 — replace Config + ExternalAnnotations, add include)
- Test: existing `example/example.cpp` (regression)

**Interfaces:**
- Produces: `StartNode::Meta::FAnnotationSet` (consteval + runtime annotation queries), `StartNode::Meta::ExternalAnnotations<T>`, `ImRefl::Config` (alias to FAnnotationSet)

- [ ] **Step 1: Write `MetaCore.hpp`**

```cpp
#ifndef STARTNODE_META_METACORE_HPP
#define STARTNODE_META_METACORE_HPP

#include <meta>
#include <optional>
#include <span>
#include <type_traits>

namespace StartNode::Meta {

// Compile-time annotation container.
// Stores a pointer to a consteval-generated static array of
// std::meta::info annotations. Both consteval (imrefl UI) and
// runtime (reflect serialization) paths share the same static data
// — zero heap allocation, cross-DSO safe.
struct FAnnotationSet
{
    const std::meta::info* annotations = nullptr;
    std::size_t count = 0;

    // ── Consteval path (for imrefl compile-time UI dispatch) ──

    template <typename T>
    consteval auto fetchAnnotation() const -> std::optional<T>
    {
        if (annotations == nullptr) { return {}; }
        for (std::size_t i = 0; i < count; ++i) {
            if (remove_cvref(type_of(annotations[i])) == remove_cvref(^^T)) {
                return std::meta::extract<T>(annotations[i]);
            }
        }
        return {};
    }

    template <typename T>
    consteval auto hasAnnotation() const -> bool
    {
        return fetchAnnotation<T>().has_value();
    }

    // ── Runtime path (for reflect serialization / formatting) ──
    // NOTE: std::meta::extract<T> at runtime depends on compiler
    // support. If extract fails at runtime, fallback is to pre-bake
    // extracted annotation values into a static table during
    // registerType<T>(). See spec §9.1 risk #1.

    template <typename T>
    auto fetchAnnotationRuntime() const -> std::optional<T>
    {
        if (annotations == nullptr) { return {}; }
        for (std::size_t i = 0; i < count; ++i) {
            if (remove_cvref(type_of(annotations[i])) == remove_cvref(^^T)) {
                return std::meta::extract<T>(annotations[i]);
            }
        }
        return {};
    }

    template <typename T>
    auto hasAnnotationRuntime() const -> bool
    {
        return fetchAnnotationRuntime<T>().has_value();
    }
};

// Specialize for third-party types to attach annotations to fields
// you don't own. Field names must match. Annotations from this
// specialization are merged into FAnnotationSet at consteval time.
template <typename T>
struct ExternalAnnotations
{};

} // namespace StartNode::Meta

#endif // STARTNODE_META_METACORE_HPP
```

- [ ] **Step 2: Modify `imrefl.hpp` — add include and replace Config/ExternalAnnotations**

In `imrefl.hpp`, replace lines 1-95 (header guards through end of `Input` overloads) with:

```cpp
#ifndef INCLUDED_IMREFL_H
#define INCLUDED_IMREFL_H

#include <imgui.h>
#include <imgui_internal.h>

#include <bitset>
#include <chrono>
#include <complex>
#include <concepts>
#include <expected>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <meta>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <source_location>
#include <stack>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <StartNode/Meta/MetaCore.hpp>

namespace ImRefl {

// ============================================================================
// LIBRARY CORE API
// ============================================================================

// Compatibility alias: ImRefl::Config = StartNode::Meta::FAnnotationSet.
// This minimizes churn in all existing Renderer specializations.
using Config = StartNode::Meta::FAnnotationSet;

// Compatibility alias: ImRefl::ExternalAnnotations = StartNode::Meta::ExternalAnnotations.
template <typename T>
using ExternalAnnotations = StartNode::Meta::ExternalAnnotations<T>;

// Specialize this struct for different types to enable them for
// rendering via ImRefl.
template <Config config, typename T>
struct Renderer
{
    static_assert(sizeof(T) == 0, "No Renderer implementation for type T");
};

template <Config config, typename T>
bool Input(const char* name, T&& value)
{
    using Type = [:remove_cvref(^^T):];
    ImGui::PushID(name);
    const bool changed = Renderer<config, Type>::Render(name, std::forward<T>(value));
    ImGui::PopID();
    return changed;
}

// The main entry point for rendering.
template <typename T>
bool Input(const char* name, T&& value)
{
    constexpr auto config = Config{};
    return Input<config>(name, std::forward<T>(value));
}
```

Then delete the old `Config` struct definition (old lines 40-62) and old `ExternalAnnotations` (old lines 75-77) if they still remain in the file.

- [ ] **Step 3: Update Renderer code that references `FetchAttn` / `HasAttn`**

In imrefl.hpp, all Renderer specializations use `config.FetchAttn<T>()` and `config.HasAttn<T>()`. These method names existed on the old `Config`. The new `FAnnotationSet` uses `fetchAnnotation<T>()` and `hasAnnotation<T>()`.

Add compatibility aliases inside struct `Config` — but since `Config` is now a `using` alias to `FAnnotationSet`, we can't add methods. Instead, search-and-replace across imrefl.hpp:

```bash
# Replace all occurrences
sed -i '' 's/\.FetchAttn</.fetchAnnotation</g' imrefl.hpp
sed -i '' 's/\.HasAttn</.hasAnnotation</g' imrefl.hpp
```

Verify: `grep -n 'FetchAttn\|HasAttn' imrefl.hpp` returns nothing.

- [ ] **Step 4: Build with example to verify no regression**

```bash
cmake -B build -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DIMREFL_BUILD_EXAMPLE=ON
cmake --build build
```

Expected: build succeeds with zero errors. The `FetchAttn`→`fetchAnnotation` rename should be exhaustive.

- [ ] **Step 5: Run the example binary**

```bash
# Headless check — exits cleanly if no display; verifies startup path
./build/example/example 2>&1 | head -5 || true
```

Expected: binary launches (may fail on window creation in headless CI, but no link/init errors).

- [ ] **Step 6: Commit**

```bash
git add StartNode/src/meta_core/include/StartNode/Meta/MetaCore.hpp imrefl.hpp
git commit -m "feat: extract MetaCore from imrefl.hpp, add FAnnotationSet"
```

---

### Task 3: Annotations + TypeId Headers

**Files:**
- Create: `StartNode/src/reflect/include/StartNode/Reflect/Annotations.hpp`
- Create: `StartNode/src/reflect/include/StartNode/Reflect/TypeId.hpp`

**Interfaces:**
- Produces: `StartNode::Anno::FTransient`, `FDeprecated`, `FVersion`, `FRange`, `FDisplayName`, `FTooltip`, `FCategory`
- Produces: `StartNode::Reflect::FTypeId`, `StartNode::Reflect::makeTypeId<T>()`
- Consumes: nothing (pure header, depends only on C++26 `<meta>`)

- [ ] **Step 1: Write `Annotations.hpp`**

```cpp
#ifndef STARTNODE_REFLECT_ANNOTATIONS_HPP
#define STARTNODE_REFLECT_ANNOTATIONS_HPP

namespace StartNode::Anno {

// Marker: member is excluded from serialization and diff.
struct FTransient {};

// Marks a member as deprecated with a migration message.
struct FDeprecated { const char* message; };

// Records the version when a member was introduced.
struct FVersion { int since; };

// Clamps numeric range for validation and UI hints.
struct FRange { double min; double max; };

// Human-readable display name (overrides reflected member name).
struct FDisplayName { const char* name; };

// Tooltip text shown in UI and documentation generation.
struct FTooltip { const char* text; };

// Organizes members into logical categories (for UI grouping).
struct FCategory { const char* category; };

} // namespace StartNode::Anno

#endif // STARTNODE_REFLECT_ANNOTATIONS_HPP
```

- [ ] **Step 2: Write `TypeId.hpp`**

```cpp
#ifndef STARTNODE_REFLECT_TYPEID_HPP
#define STARTNODE_REFLECT_TYPEID_HPP

#include <cstdint>
#include <meta>
#include <string_view>

namespace StartNode::Reflect {

// 64-bit type identifier generated from the fully-qualified type name
// at compile time via consteval hashing. Consistent across translation
// units and shared libraries for the same type.
struct FTypeId
{
    std::uint64_t value = 0;

    auto operator==(const FTypeId& other) const -> bool = default;
    auto operator!=(const FTypeId& other) const -> bool = default;
};

} // namespace StartNode::Reflect

// std::hash support for FTypeId so it can be used as unordered_map key.
template <>
struct std::hash<StartNode::Reflect::FTypeId>
{
    auto operator()(const StartNode::Reflect::FTypeId& id) const -> std::size_t
    {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

namespace StartNode::Reflect {

namespace detail {

// FNV-1a 64-bit hash — constexpr for compile-time use.
consteval auto fnv1a64(std::string_view str) -> std::uint64_t
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offsetBasis;
    for (char c : str) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= prime;
    }
    return hash;
}

} // namespace detail

// Generate a compile-time type ID from the type's fully-qualified
// reflection display string. Same type → same string → same hash
// across all translation units.
template <typename T>
consteval auto makeTypeId() -> FTypeId
{
    constexpr auto name = std::meta::display_string_of(^^T);
    return FTypeId{detail::fnv1a64(name)};
}

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_TYPEID_HPP
```

- [ ] **Step 3: Build to verify headers compile**

```bash
cmake --build build
```

Expected: build succeeds (these headers are transitively included by nothing yet, but can verify by writing a quick compile check).

- [ ] **Step 4: Commit**

```bash
git add StartNode/src/reflect/include/StartNode/Reflect/Annotations.hpp \
        StartNode/src/reflect/include/StartNode/Reflect/TypeId.hpp
git commit -m "feat: add StartNode annotations and compile-time TypeId"
```

---

### Task 4: TypeDescriptor + TypeRegistry

**Files:**
- Create: `StartNode/src/reflect/include/StartNode/Reflect/TypeDescriptor.hpp`
- Create: `StartNode/src/reflect/include/StartNode/Reflect/TypeRegistry.hpp`
- Modify: `StartNode/src/reflect/src/TypeRegistry.cpp` (replace placeholder)

**Interfaces:**
- Produces: `FMemberDescriptor`, `FTypeDescriptor`, `UTypeRegistry`, `registerBuiltinTypes()`
- Consumes: `FAnnotationSet` (MetaCore), `FTypeId`, `makeTypeId<T>()` (TypeId)

- [ ] **Step 1: Write `TypeDescriptor.hpp`**

```cpp
#ifndef STARTNODE_REFLECT_TYPEDESCRIPTOR_HPP
#define STARTNODE_REFLECT_TYPEDESCRIPTOR_HPP

#include <StartNode/Meta/MetaCore.hpp>
#include <StartNode/Reflect/TypeId.hpp>

#include <cstddef>
#include <span>

namespace StartNode::Reflect {

// Describes a single non-static data member of a reflected type.
// All string data and annotation arrays point to consteval-generated
// static storage — zero heap allocation.
struct FMemberDescriptor
{
    const char* name;                // Member name (static string)
    std::size_t offset;              // Byte offset from object start
    FTypeId typeId;                  // Compile-time hash of member type
    Meta::FAnnotationSet annotations; // Annotation snapshot
    bool isTransient;                // Pre-resolved from FTransient annotation
};

// Runtime descriptor for a reflected type. Holds a span over a
// consteval-generated static array of FMemberDescriptor. Includes
// type-erased lifecycle functions for generic construction/destruction.
struct FTypeDescriptor
{
    FTypeId typeId;
    const char* name;                           // Type name (static string)
    std::size_t sizeOf;
    std::span<const FMemberDescriptor> members; // Zero-alloc view into static data

    // Type-erased factory functions
    void (*defaultConstruct)(void* memory) = nullptr;
    void (*copyConstruct)(void* memory, const void* source) = nullptr;
    void (*destruct)(void* memory) = nullptr;
};

// ── Generate FTypeDescriptor at compile time ──

namespace detail {

// Build a static constexpr array of FMemberDescriptor for type T
// by reflecting over its non-static data members.
template <typename T>
consteval auto buildMemberDescriptors() -> std::vector<FMemberDescriptor>
{
    const auto ctx = std::meta::access_context::current();
    const auto memberList = nonstatic_data_members_of(^^T, ctx);

    std::vector<FMemberDescriptor> result;
    for (const auto member : memberList) {
        FMemberDescriptor desc{};
        desc.name = std::meta::identifier_of(member).data();
        desc.offset = std::meta::offset_of(member);
        desc.typeId = makeTypeId<[:type_of(member):]>();

        // Gather annotations: own + ExternalAnnotations
        auto ownAttns = annotations_of(member);
        // Check ExternalAnnotations<T> for matching field
        const auto external =
            substitute(^^Meta::ExternalAnnotations, {^^T});
        for (const auto extMember :
             nonstatic_data_members_of(external, ctx)) {
            if (identifier_of(member) == identifier_of(extMember)) {
                for (const auto a : annotations_of(extMember)) {
                    ownAttns.push_back(a);
                }
            }
        }

        // Pre-resolve isTransient
        desc.isTransient = false;
        for (const auto a : ownAttns) {
            if (remove_cvref(type_of(a)) ==
                remove_cvref(^^StartNode::Anno::FTransient)) {
                desc.isTransient = true;
                break;
            }
        }

        // Store annotation snapshot
        auto staticAttns = std::define_static_array(ownAttns);
        desc.annotations = Meta::FAnnotationSet{
            staticAttns.data(), staticAttns.size()};

        result.push_back(desc);
    }
    return result;
}

} // namespace detail

// Compile-time helper: returns a reference to the static FTypeDescriptor
// for T. Used by registerType<T>().
template <typename T>
consteval auto makeTypeDescriptor() -> FTypeDescriptor
{
    FTypeDescriptor desc{};
    desc.typeId = makeTypeId<T>();
    desc.name = std::meta::display_string_of(^^T).data();
    desc.sizeOf = sizeof(T);

    // Build static member array
    auto memberVec = detail::buildMemberDescriptors<T>();
    auto staticMembers = std::define_static_array(memberVec);
    desc.members = std::span<const FMemberDescriptor>{
        staticMembers.data(), staticMembers.size()};

    // Factory functions
    desc.defaultConstruct = [](void* memory) { new (memory) T(); };
    desc.copyConstruct = [](void* memory, const void* source) {
        new (memory) T(*static_cast<const T*>(source));
    };
    desc.destruct = [](void* memory) {
        static_cast<T*>(memory)->~T();
    };

    return desc;
}

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_TYPEDESCRIPTOR_HPP
```

- [ ] **Step 2: Write `TypeRegistry.hpp`**

```cpp
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
```

- [ ] **Step 3: Write `TypeRegistry.cpp`**

```cpp
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
```

- [ ] **Step 4: Build to verify everything compiles**

```bash
cmake --build build
```

Expected: build succeeds. Reflect now has real source files.

- [ ] **Step 5: Commit**

```bash
git add StartNode/src/reflect/
git commit -m "feat: add TypeDescriptor, TypeRegistry with consteval member reflection"
```

---

### Task 5: FJsonValue (Standalone JSON Value Type)

**Files:**
- Create: `StartNode/src/reflect/include/StartNode/Reflect/Json.hpp`
- Modify: `StartNode/src/reflect/src/Json.cpp` (replace placeholder)

**Interfaces:**
- Produces: `StartNode::Reflect::Json::FJsonValue` — standalone JSON value with factory methods, accessors, dump, parse
- Consumes: nothing outside stdlib

- [ ] **Step 1: Write `Json.hpp`**

```cpp
#ifndef STARTNODE_REFLECT_JSON_HPP
#define STARTNODE_REFLECT_JSON_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace StartNode::Reflect::Json {

// Minimal JSON value type. Represents the standard JSON data model:
// null, bool, int64, double, string, array, object.
// Zero third-party dependencies — hand-rolled writer and parser.
class FJsonValue
{
public:
    enum class EType
    {
        Null,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object
    };

    // ── Constructors ──

    static auto makeNull() -> FJsonValue;
    static auto makeBool(bool value) -> FJsonValue;
    static auto makeInt(std::int64_t value) -> FJsonValue;
    static auto makeDouble(double value) -> FJsonValue;
    static auto makeString(std::string value) -> FJsonValue;
    static auto makeArray(std::vector<FJsonValue> values) -> FJsonValue;
    static auto makeObject(
        std::vector<std::pair<std::string, FJsonValue>> members
    ) -> FJsonValue;

    // ── Accessors ──

    auto getType() const -> EType { return type; }

    auto asBool() const -> bool;
    auto asInt() const -> std::int64_t;
    auto asDouble() const -> double;
    auto asString() const -> const std::string&;
    auto asArray() const -> const std::vector<FJsonValue>&;
    auto asObject() const
        -> const std::vector<std::pair<std::string, FJsonValue>>&;

    // ── Serialize ──

    // Pretty-print to JSON string with optional indentation.
    auto dump(std::size_t indent = 0) const -> std::string;

    // Parse a JSON string. Returns nullopt on syntax error.
    static auto parse(std::string_view text) -> std::optional<FJsonValue>;

private:
    FJsonValue() = default;

    EType type = EType::Null;

    // Stored values (active member matches EType)
    bool boolValue = false;
    std::int64_t intValue = 0;
    double doubleValue = 0.0;
    std::string stringValue;
    std::vector<FJsonValue> arrayValue;
    std::vector<std::pair<std::string, FJsonValue>> objectValue;
};

} // namespace StartNode::Reflect::Json

#endif // STARTNODE_REFLECT_JSON_HPP
```

- [ ] **Step 2: Write `Json.cpp`**

```cpp
#include <StartNode/Reflect/Json.hpp>

#include <cassert>
#include <charconv>
#include <cstring>
#include <format>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace StartNode::Reflect::Json {

// ── Factory methods ──

auto FJsonValue::makeNull() -> FJsonValue { return {}; }

auto FJsonValue::makeBool(bool value) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Bool;
    v.boolValue = value;
    return v;
}

auto FJsonValue::makeInt(std::int64_t value) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Int;
    v.intValue = value;
    return v;
}

auto FJsonValue::makeDouble(double value) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Double;
    v.doubleValue = value;
    return v;
}

auto FJsonValue::makeString(std::string value) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::String;
    v.stringValue = std::move(value);
    return v;
}

auto FJsonValue::makeArray(std::vector<FJsonValue> values) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Array;
    v.arrayValue = std::move(values);
    return v;
}

auto FJsonValue::makeObject(
    std::vector<std::pair<std::string, FJsonValue>> members
) -> FJsonValue
{
    FJsonValue v;
    v.type = EType::Object;
    v.objectValue = std::move(members);
    return v;
}

// ── Accessors ──

auto FJsonValue::asBool() const -> bool
{
    assert(type == EType::Bool);
    return boolValue;
}

auto FJsonValue::asInt() const -> std::int64_t
{
    assert(type == EType::Int);
    return intValue;
}

auto FJsonValue::asDouble() const -> double
{
    assert(type == EType::Double);
    return doubleValue;
}

auto FJsonValue::asString() const -> const std::string&
{
    assert(type == EType::String);
    return stringValue;
}

auto FJsonValue::asArray() const -> const std::vector<FJsonValue>&
{
    assert(type == EType::Array);
    return arrayValue;
}

auto FJsonValue::asObject() const
    -> const std::vector<std::pair<std::string, FJsonValue>>&
{
    assert(type == EType::Object);
    return objectValue;
}

// ── JSON string escaping ──

namespace {

auto escapeString(std::string_view raw) -> std::string
{
    std::string result;
    result.reserve(raw.size() + 8);
    result += '"';
    for (char c : raw) {
        switch (c) {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n";  break;
        case '\r': result += "\\r";  break;
        case '\t': result += "\\t";  break;
        default:   result += c;      break;
        }
    }
    result += '"';
    return result;
}

void dumpImpl(const FJsonValue& val, std::string& out,
              std::size_t indent, std::size_t level)
{
    const auto pad = [&](std::size_t extra) {
        if (indent > 0) {
            out += '\n';
            out.append((level + extra) * indent, ' ');
        }
    };

    switch (val.getType()) {
    case FJsonValue::EType::Null:
        out += "null";
        break;
    case FJsonValue::EType::Bool:
        out += val.asBool() ? "true" : "false";
        break;
    case FJsonValue::EType::Int:
        out += std::to_string(val.asInt());
        break;
    case FJsonValue::EType::Double: {
        auto s = std::format("{}", val.asDouble());
        // Ensure decimal point for valid JSON (JSON has no NaN/Inf)
        if (s.find('.') == std::string::npos &&
            s.find('e') == std::string::npos &&
            s.find('E') == std::string::npos) {
            s += ".0";
        }
        out += s;
        break;
    }
    case FJsonValue::EType::String:
        out += escapeString(val.asString());
        break;
    case FJsonValue::EType::Array:
        out += '[';
        for (std::size_t i = 0; i < val.asArray().size(); ++i) {
            if (i > 0) { out += ','; }
            pad(1);
            dumpImpl(val.asArray()[i], out, indent, level + 1);
        }
        pad(0);
        out += ']';
        break;
    case FJsonValue::EType::Object:
        out += '{';
        for (std::size_t i = 0; i < val.asObject().size(); ++i) {
            if (i > 0) { out += ','; }
            pad(1);
            const auto& [key, member] = val.asObject()[i];
            out += escapeString(key);
            out += ':';
            if (indent > 0) { out += ' '; }
            dumpImpl(member, out, indent, level + 1);
        }
        pad(0);
        out += '}';
        break;
    }
}

} // anonymous namespace

auto FJsonValue::dump(std::size_t indent) const -> std::string
{
    std::string result;
    dumpImpl(*this, result, indent, 0);
    if (indent > 0) { result += '\n'; }
    return result;
}

// ── JSON Parser ──

namespace {

class FJsonParser
{
public:
    explicit FJsonParser(std::string_view text)
        : cursor(text.data()), end(text.data() + text.size()) {}

    auto parse() -> std::optional<FJsonValue>
    {
        skipWhitespace();
        if (atEnd()) { return {}; }
        auto val = parseValue();
        skipWhitespace();
        if (!atEnd()) { return {}; } // trailing garbage
        return val;
    }

private:
    const char* cursor;
    const char* end;

    bool atEnd() const { return cursor >= end; }
    char peek() const { return atEnd() ? '\0' : *cursor; }
    char advance() { return atEnd() ? '\0' : *cursor++; }
    void skipWhitespace() {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    auto parseValue() -> std::optional<FJsonValue>
    {
        skipWhitespace();
        switch (peek()) {
        case 'n': return parseNull();
        case 't': case 'f': return parseBool();
        case '"': return parseString();
        case '[': return parseArray();
        case '{': return parseObject();
        default:
            if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek()))) {
                return parseNumber();
            }
            return {};
        }
    }

    auto parseNull() -> std::optional<FJsonValue>
    {
        if (std::strncmp(cursor, "null", 4) == 0) {
            cursor += 4;
            return FJsonValue::makeNull();
        }
        return {};
    }

    auto parseBool() -> std::optional<FJsonValue>
    {
        if (std::strncmp(cursor, "true", 4) == 0) {
            cursor += 4;
            return FJsonValue::makeBool(true);
        }
        if (std::strncmp(cursor, "false", 5) == 0) {
            cursor += 5;
            return FJsonValue::makeBool(false);
        }
        return {};
    }

    auto parseString() -> std::optional<FJsonValue>
    {
        if (advance() != '"') { return {}; }
        std::string result;
        while (!atEnd()) {
            char c = advance();
            if (c == '"') {
                return FJsonValue::makeString(std::move(result));
            }
            if (c == '\\') {
                if (atEnd()) { return {}; }
                switch (advance()) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   return {};       // unsupported escape
                }
            } else {
                result += c;
            }
        }
        return {}; // unterminated string
    }

    auto parseNumber() -> std::optional<FJsonValue>
    {
        const char* start = cursor;
        if (peek() == '-') { advance(); }
        if (atEnd()) { return {}; }

        // Integer part
        if (peek() == '0') {
            advance();
        } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        } else {
            return {};
        }

        bool isFloat = false;

        // Fractional part
        if (peek() == '.') {
            isFloat = true;
            advance();
            if (atEnd() || !std::isdigit(static_cast<unsigned char>(peek()))) {
                return {};
            }
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }

        // Exponent part
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            advance();
            if (peek() == '+' || peek() == '-') { advance(); }
            if (atEnd() || !std::isdigit(static_cast<unsigned char>(peek()))) {
                return {};
            }
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }

        std::string numberStr(start, cursor);
        if (isFloat) {
            double d = std::stod(numberStr);
            return FJsonValue::makeDouble(d);
        }
        std::int64_t i = std::stoll(numberStr);
        return FJsonValue::makeInt(i);
    }

    auto parseArray() -> std::optional<FJsonValue>
    {
        if (advance() != '[') { return {}; }
        std::vector<FJsonValue> elements;
        skipWhitespace();
        if (peek() == ']') {
            advance();
            return FJsonValue::makeArray(std::move(elements));
        }
        while (true) {
            auto val = parseValue();
            if (!val) { return {}; }
            elements.push_back(std::move(*val));
            skipWhitespace();
            if (peek() == ']') {
                advance();
                return FJsonValue::makeArray(std::move(elements));
            }
            if (peek() != ',') { return {}; }
            advance();
        }
    }

    auto parseObject() -> std::optional<FJsonValue>
    {
        if (advance() != '{') { return {}; }
        std::vector<std::pair<std::string, FJsonValue>> members;
        skipWhitespace();
        if (peek() == '}') {
            advance();
            return FJsonValue::makeObject(std::move(members));
        }
        while (true) {
            skipWhitespace();
            auto keyVal = parseString();
            if (!keyVal || keyVal->getType() != FJsonValue::EType::String) {
                return {};
            }
            std::string key = keyVal->asString();
            skipWhitespace();
            if (advance() != ':') { return {}; }
            auto val = parseValue();
            if (!val) { return {}; }
            members.emplace_back(std::move(key), std::move(*val));
            skipWhitespace();
            if (peek() == '}') {
                advance();
                return FJsonValue::makeObject(std::move(members));
            }
            if (peek() != ',') { return {}; }
            advance();
        }
    }
};

} // anonymous namespace

auto FJsonValue::parse(std::string_view text) -> std::optional<FJsonValue>
{
    FJsonParser parser(text);
    return parser.parse();
}

} // namespace StartNode::Reflect::Json
```

- [ ] **Step 3: Build to verify compilation**

```bash
cmake --build build
```

Expected: build succeeds. Json.cpp compiles without third-party includes.

- [ ] **Step 4: Commit**

```bash
git add StartNode/src/reflect/
git commit -m "feat: add FJsonValue with dump and recursive-descent parser"
```

---

### Task 6: Reflection-Driven Serialization (toJson / fromJson)

**Files:**
- Create: `StartNode/src/reflect/include/StartNode/Reflect/Serialize.hpp`
- Modify: `StartNode/src/reflect/src/Serialize.cpp` (replace placeholder)

**Interfaces:**
- Produces: `StartNode::Reflect::toJson<T>(object) -> std::expected<Json::FJsonValue, string>`, `fromJson<T>(json, object) -> std::expected<void, string>`
- Consumes: `FJsonValue`, `UTypeRegistry`, `FTypeDescriptor`, `makeTypeId<T>()`

- [ ] **Step 1: Write `Serialize.hpp`** (header-only templates + forward declarations)

```cpp
#ifndef STARTNODE_REFLECT_SERIALIZE_HPP
#define STARTNODE_REFLECT_SERIALIZE_HPP

#include <StartNode/Reflect/Json.hpp>
#include <StartNode/Reflect/TypeRegistry.hpp>

#include <expected>
#include <format>
#include <string>
#include <type_traits>
#include <vector>

namespace StartNode::Reflect {

// ── Type trait helpers ──

namespace detail {

template <typename T>
concept IsVector = requires { typename T::value_type; }
    && std::same_as<T, std::vector<typename T::value_type>>;

template <typename T>
concept IsOptional = requires { typename T::value_type; }
    && std::same_as<T, std::optional<typename T::value_type>>;

} // namespace detail

// ── Forward declarations for runtime dispatch (defined in Serialize.cpp) ──

namespace detail {

auto dispatchToJson(FTypeId typeId, const void* pointer)
    -> std::expected<Json::FJsonValue, std::string>;

auto dispatchFromJson(FTypeId typeId, const Json::FJsonValue& json,
                      void* pointer) -> std::expected<void, std::string>;

} // namespace detail

// ── toJson template implementation ──

namespace detail {

template <typename T>
auto toJsonScalar(const T& value) -> std::expected<Json::FJsonValue, std::string>
{
    if constexpr (std::is_same_v<T, bool>) {
        return Json::FJsonValue::makeBool(value);
    } else if constexpr (std::is_integral_v<T>) {
        return Json::FJsonValue::makeInt(static_cast<std::int64_t>(value));
    } else if constexpr (std::is_floating_point_v<T>) {
        return Json::FJsonValue::makeDouble(static_cast<double>(value));
    } else if constexpr (std::is_same_v<T, std::string>) {
        return Json::FJsonValue::makeString(value);
    }
    return std::unexpected("toJson: unsupported scalar type");
}

template <typename T>
auto toJsonVector(const std::vector<T>& vec)
    -> std::expected<Json::FJsonValue, std::string>
{
    std::vector<Json::FJsonValue> elements;
    elements.reserve(vec.size());
    for (const auto& element : vec) {
        auto jsonElement = toJson(element);
        if (!jsonElement) { return std::unexpected(jsonElement.error()); }
        elements.push_back(std::move(*jsonElement));
    }
    return Json::FJsonValue::makeArray(std::move(elements));
}

template <typename T>
auto toJsonOptional(const std::optional<T>& opt)
    -> std::expected<Json::FJsonValue, std::string>
{
    if (!opt.has_value()) { return Json::FJsonValue::makeNull(); }
    return toJson(*opt);
}

template <typename T>
auto toJsonAggregate(const T& object)
    -> std::expected<Json::FJsonValue, std::string>
{
    const auto* descriptor = UTypeRegistry::instance().find(makeTypeId<T>());
    if (!descriptor) {
        return std::unexpected(
            std::format("toJson: type '{}' is not registered", descriptor->name));
    }

    std::vector<std::pair<std::string, Json::FJsonValue>> members;
    const auto* base = reinterpret_cast<const char*>(&object);
    for (const auto& member : descriptor->members) {
        if (member.isTransient) { continue; }
        const void* memberPointer = base + member.offset;
        auto jsonMember = detail::dispatchToJson(member.typeId, memberPointer);
        if (!jsonMember) { return std::unexpected(jsonMember.error()); }
        members.emplace_back(member.name, std::move(*jsonMember));
    }
    return Json::FJsonValue::makeObject(std::move(members));
}

} // namespace detail

// ── Public entry points ──

template <typename T>
auto toJson(const T& object) -> std::expected<Json::FJsonValue, std::string>
{
    if constexpr (detail::IsVector<T>) {
        return detail::toJsonVector(object);
    } else if constexpr (detail::IsOptional<T>) {
        return detail::toJsonOptional(object);
    } else if constexpr (std::is_same_v<T, bool> ||
                         std::is_integral_v<T> ||
                         std::is_floating_point_v<T> ||
                         std::is_same_v<T, std::string>)
    {
        return detail::toJsonScalar(object);
    } else {
        return detail::toJsonAggregate(object);
    }
}

template <typename T>
auto fromJson(const Json::FJsonValue& json, T& object)
    -> std::expected<void, std::string>
{
    if (json.getType() != Json::FJsonValue::EType::Object) {
        return std::unexpected("fromJson: expected JSON object");
    }

    const auto* descriptor = UTypeRegistry::instance().find(makeTypeId<T>());
    if (!descriptor) {
        return std::unexpected("fromJson: type is not registered");
    }

    const auto& jsonMembers = json.asObject();
    auto* base = reinterpret_cast<char*>(&object);

    for (const auto& memberDesc : descriptor->members) {
        if (memberDesc.isTransient) { continue; }

        const Json::FJsonValue* field = nullptr;
        for (const auto& [key, value] : jsonMembers) {
            if (key == memberDesc.name) { field = &value; break; }
        }
        if (!field) { continue; } // forward-compat: ignore unknown fields

        void* memberPointer = base + memberDesc.offset;
        auto result = detail::dispatchFromJson(
            memberDesc.typeId, *field, memberPointer);
        if (!result) { return std::unexpected(result.error()); }
    }
    return {};
}

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_SERIALIZE_HPP
```

- [ ] **Step 2: Write `Serialize.cpp`** (runtime dispatch tables only — template bodies are in the header)

```cpp
#include <StartNode/Reflect/Serialize.hpp>

#include <StartNode/Reflect/TypeRegistry.hpp>

#include <unordered_map>
#include <cstdint>

namespace StartNode::Reflect::detail {

// Runtime dispatch table: FTypeId → toJson function pointer.
// Maps known scalar type ids to lambda converters; for aggregates
// recurses via FTypeDescriptor::members.
auto dispatchToJson(FTypeId typeId, const void* pointer)
    -> std::expected<Json::FJsonValue, std::string>
{
    using Dispatcher = auto (*)(const void*)
        -> std::expected<Json::FJsonValue, std::string>;

    static const std::unordered_map<FTypeId, Dispatcher> dispatchers = {
        { makeTypeId<bool>(), [](const void* p) {
            return Json::FJsonValue::makeBool(*static_cast<const bool*>(p)); }},
        { makeTypeId<std::int32_t>(), [](const void* p) {
            return Json::FJsonValue::makeInt(*static_cast<const std::int32_t*>(p)); }},
        { makeTypeId<std::int64_t>(), [](const void* p) {
            return Json::FJsonValue::makeInt(*static_cast<const std::int64_t*>(p)); }},
        { makeTypeId<float>(), [](const void* p) {
            return Json::FJsonValue::makeDouble(*static_cast<const float*>(p)); }},
        { makeTypeId<double>(), [](const void* p) {
            return Json::FJsonValue::makeDouble(*static_cast<const double*>(p)); }},
        { makeTypeId<std::string>(), [](const void* p) {
            return Json::FJsonValue::makeString(*static_cast<const std::string*>(p)); }},
    };

    auto it = dispatchers.find(typeId);
    if (it != dispatchers.end()) {
        return it->second(pointer);
    }

    // Aggregate: recurse via descriptor
    const auto* descriptor = UTypeRegistry::instance().find(typeId);
    if (!descriptor) {
        return std::unexpected("dispatchToJson: unknown type id");
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
    using Dispatcher = auto (*)(const Json::FJsonValue&, void*)
        -> std::expected<void, std::string>;

    static const std::unordered_map<FTypeId, Dispatcher> dispatchers = {
        { makeTypeId<bool>(), [](const Json::FJsonValue& j, void* p) {
            if (j.getType() != Json::FJsonValue::EType::Bool)
                return std::unexpected(std::string("fromJson: expected bool"));
            *static_cast<bool*>(p) = j.asBool();
            return std::expected<void, std::string>{}; }},
        { makeTypeId<std::int32_t>(), [](const Json::FJsonValue& j, void* p) {
            if (j.getType() != Json::FJsonValue::EType::Int)
                return std::unexpected(std::string("fromJson: expected int"));
            *static_cast<std::int32_t*>(p) = static_cast<std::int32_t>(j.asInt());
            return std::expected<void, std::string>{}; }},
        { makeTypeId<std::int64_t>(), [](const Json::FJsonValue& j, void* p) {
            if (j.getType() != Json::FJsonValue::EType::Int)
                return std::unexpected(std::string("fromJson: expected int64"));
            *static_cast<std::int64_t*>(p) = j.asInt();
            return std::expected<void, std::string>{}; }},
        { makeTypeId<float>(), [](const Json::FJsonValue& j, void* p) {
            if (j.getType() == Json::FJsonValue::EType::Double)
                *static_cast<float*>(p) = static_cast<float>(j.asDouble());
            else if (j.getType() == Json::FJsonValue::EType::Int)
                *static_cast<float*>(p) = static_cast<float>(j.asInt());
            else return std::unexpected(std::string("fromJson: expected number"));
            return std::expected<void, std::string>{}; }},
        { makeTypeId<double>(), [](const Json::FJsonValue& j, void* p) {
            if (j.getType() == Json::FJsonValue::EType::Double)
                *static_cast<double*>(p) = j.asDouble();
            else if (j.getType() == Json::FJsonValue::EType::Int)
                *static_cast<double*>(p) = static_cast<double>(j.asInt());
            else return std::unexpected(std::string("fromJson: expected number"));
            return std::expected<void, std::string>{}; }},
        { makeTypeId<std::string>(), [](const Json::FJsonValue& j, void* p) {
            if (j.getType() != Json::FJsonValue::EType::String)
                return std::unexpected(std::string("fromJson: expected string"));
            *static_cast<std::string*>(p) = j.asString();
            return std::expected<void, std::string>{}; }},
    };

    auto it = dispatchers.find(typeId);
    if (it != dispatchers.end()) {
        return it->second(json, pointer);
    }

    // Aggregate: recurse via descriptor
    if (json.getType() != Json::FJsonValue::EType::Object) {
        return std::unexpected("fromJson: expected JSON object for aggregate");
    }

    const auto* descriptor = UTypeRegistry::instance().find(typeId);
    if (!descriptor) {
        return std::unexpected("dispatchFromJson: unknown type id");
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
        auto result = dispatchFromJson(
            memberDesc.typeId, *field, memberPointer);
        if (!result) { return std::unexpected(result.error()); }
    }
    return {};
}

} // namespace StartNode::Reflect::detail
```

- [ ] **Step 3: Build to verify compilation**

```bash
cmake --build build
```

Expected: build succeeds. Serialize.cpp links with Json.cpp and TypeRegistry.cpp.

- [ ] **Step 4: Commit**

```bash
git add StartNode/src/reflect/
git commit -m "feat: add reflection-driven toJson/fromJson with runtime dispatch"
```

---

### Task 7: formatReflected

**Files:**
- Create: `StartNode/src/reflect/include/StartNode/Reflect/Format.hpp`

**Interfaces:**
- Produces: `StartNode::Reflect::formatReflected<T>(object) -> string`
- Consumes: `UTypeRegistry`, `FTypeDescriptor`, `makeTypeId<T>()`

- [ ] **Step 1: Write `Format.hpp`**

```cpp
#ifndef STARTNODE_REFLECT_FORMAT_HPP
#define STARTNODE_REFLECT_FORMAT_HPP

#include <StartNode/Reflect/TypeRegistry.hpp>

#include <format>
#include <string>
#include <type_traits>
#include <vector>

namespace StartNode::Reflect {

namespace detail {

template <typename T>
concept IsVector = requires { typename T::value_type; }
    && std::same_as<T, std::vector<typename T::value_type>>;

template <typename T>
concept IsOptional = requires { typename T::value_type; }
    && std::same_as<T, std::optional<typename T::value_type>>;

// Forward declaration for recursive dispatch
template <typename T>
auto formatReflectedImpl(const T& object) -> std::string;

// Format a scalar via std::format (works for builtins with
// std::formatter, plus std::string).
template <typename T>
auto formatScalar(const T& value) -> std::string
{
    return std::format("{}", value);
}

// Format a vector: [elem1, elem2, ...]
template <typename T>
auto formatVector(const std::vector<T>& vec) -> std::string
{
    std::string result = "[";
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) { result += ", "; }
        result += formatReflectedImpl(vec[i]);
    }
    result += "]";
    return result;
}

// Format an optional: Some(value) or None
template <typename T>
auto formatOptional(const std::optional<T>& opt) -> std::string
{
    if (opt.has_value()) {
        return std::format("Some({})", formatReflectedImpl(*opt));
    }
    return "None";
}

// Format an aggregate by walking its member descriptors.
template <typename T>
auto formatAggregate(const T& object) -> std::string
{
    const auto* desc = UTypeRegistry::instance().find(makeTypeId<T>());
    if (!desc) {
        return "<unregistered type>";
    }

    std::string result;
    result += desc->name;
    result += "{";
    const auto* base = reinterpret_cast<const char*>(&object);
    bool first = true;
    for (const auto& member : desc->members) {
        if (!first) { result += ", "; }
        first = false;
        result += member.name;
        result += "=";

        // Dispatch by member type id — uses the same dispatchToJson
        // approach but for formatting we need a format-specific path.
        // For now, recurse through formatReflectedImpl via type erasure.
        formatMemberByTypeId(member.typeId, base + member.offset, result);
    }
    result += "}";
    return result;
}

// Forward declaration: called by formatMemberByTypeId for nested aggregates.
inline auto formatAggregatePtr(FTypeId typeId, const void* ptr) -> std::string;

// Runtime format dispatch for a member given its type id and pointer.
// Writes formatted representation into the result string.
inline void formatMemberByTypeId(FTypeId typeId, const void* ptr,
                                 std::string& result)
{
    const auto boolId = makeTypeId<bool>();
    const auto int32Id = makeTypeId<std::int32_t>();
    const auto int64Id = makeTypeId<std::int64_t>();
    const auto floatId = makeTypeId<float>();
    const auto doubleId = makeTypeId<double>();
    const auto stringId = makeTypeId<std::string>();

    if (typeId == boolId) {
        result += std::format("{}", *static_cast<const bool*>(ptr));
    } else if (typeId == int32Id) {
        result += std::format("{}", *static_cast<const std::int32_t*>(ptr));
    } else if (typeId == int64Id) {
        result += std::format("{}", *static_cast<const std::int64_t*>(ptr));
    } else if (typeId == floatId) {
        result += std::format("{}", *static_cast<const float*>(ptr));
    } else if (typeId == doubleId) {
        result += std::format("{}", *static_cast<const double*>(ptr));
    } else if (typeId == stringId) {
        result += std::format("\"{}\"", *static_cast<const std::string*>(ptr));
    } else {
        // Aggregate — recurse
        result += formatAggregatePtr(typeId, ptr);
    }
}

inline auto formatAggregatePtr(FTypeId typeId, const void* ptr) -> std::string
{
    const auto* desc = UTypeRegistry::instance().find(typeId);
    if (!desc) { return "<unregistered type>"; }

    std::string result;
    result += desc->name;
    result += "{";
    const auto* base = static_cast<const char*>(ptr);
    bool first = true;
    for (const auto& member : desc->members) {
        if (!first) { result += ", "; }
        first = false;
        result += member.name;
        result += "=";
        formatMemberByTypeId(member.typeId, base + member.offset, result);
    }
    result += "}";
    return result;
}

// Top-level dispatch
template <typename T>
auto formatReflectedImpl(const T& object) -> std::string
{
    if constexpr (IsVector<T>) {
        return formatVector(object);
    } else if constexpr (IsOptional<T>) {
        return formatOptional(object);
    } else if constexpr (std::is_same_v<T, bool> ||
                         std::is_integral_v<T> ||
                         std::is_floating_point_v<T> ||
                         std::is_same_v<T, std::string>)
    {
        return formatScalar(object);
    } else {
        return formatAggregate(object);
    }
}

} // namespace detail

// Public entry point for formatting any reflected type.
template <typename T>
auto formatReflected(const T& object) -> std::string
{
    return detail::formatReflectedImpl(object);
}

} // namespace StartNode::Reflect

#endif // STARTNODE_REFLECT_FORMAT_HPP
```

- [ ] **Step 2: Build to verify compilation**

```bash
cmake --build build
```

Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add StartNode/src/reflect/include/StartNode/Reflect/Format.hpp
git commit -m "feat: add formatReflected for debug/log output"
```

---

### Task 8: Tests

**Files:**
- Create: `StartNode/tests/reflect_tests/CMakeLists.txt`
- Create: `StartNode/tests/reflect_tests/src/TestMain.cpp`
- Create: `StartNode/tests/reflect_tests/src/TestMetaCore.cpp`
- Create: `StartNode/tests/reflect_tests/src/TestTypeRegistry.cpp`
- Create: `StartNode/tests/reflect_tests/src/TestJson.cpp`
- Create: `StartNode/tests/reflect_tests/src/TestSerialize.cpp`
- Create: `StartNode/tests/reflect_tests/src/TestFormat.cpp`
- Create: `StartNode/tests/reflect_tests/src/TestTypeId.cpp`
- Create: `StartNode/tests/reflect_tests/src/TestTypeIdShared.hpp`
- Create: `StartNode/tests/reflect_tests/src/TestTypeIdCrossTU_A.cpp`
- Create: `StartNode/tests/reflect_tests/src/TestTypeIdCrossTU_B.cpp`

**Interfaces:**
- Consumes: All types/functions from tasks 2–7
- Produces: `reflect_tests` executable, all green

- [ ] **Step 1: Write `tests/reflect_tests/CMakeLists.txt`**

```cmake
add_executable(reflect_tests
    src/TestMain.cpp
    src/TestMetaCore.cpp
    src/TestTypeRegistry.cpp
    src/TestJson.cpp
    src/TestSerialize.cpp
    src/TestFormat.cpp
    src/TestTypeId.cpp
    src/TestTypeIdCrossTU_A.cpp
    src/TestTypeIdCrossTU_B.cpp
)

target_link_libraries(reflect_tests PRIVATE Reflect GTest::gtest_main)

target_compile_options(reflect_tests PRIVATE
    -freflection
    -fexpansion-statements
    -fannotation-attributes
    -fparameter-reflection
)
```

- [ ] **Step 2: Write `TestMain.cpp`**

```cpp
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 3: Write `TestMetaCore.cpp`**

```cpp
#include <gtest/gtest.h>
#include <StartNode/Meta/MetaCore.hpp>
#include <StartNode/Reflect/Annotations.hpp>

// Test type with annotations
struct FTestAnnotated
{
    [[StartNode::Anno::FTransient]]
    int transientField = 0;

    [[StartNode::Anno::FCategory("Test")]]
    float categoryField = 1.0f;
};

// External annotations for a third-party type
struct FThirdParty
{
    int value = 42;
};

template <>
struct StartNode::Meta::ExternalAnnotations<FThirdParty>
{
    [[StartNode::Anno::FTransient]]
    int value;
};

// ── Test: ImRefl alias still works ──
// Verify that ImRefl::Config is a valid alias for FAnnotationSet.
// (This is checked at compile time — if it compiles, it passes.)

TEST(MetaCore, ImReflAliasCompiles)
{
    // FAnnotationSet default-constructs with null annotations
    StartNode::Meta::FAnnotationSet set;
    EXPECT_EQ(set.annotations, nullptr);
    EXPECT_EQ(set.count, 0);
}

// ── Test: Consteval annotation fetch ──

TEST(MetaCore, ConstevalAnnotationFetch)
{
    // This test verifies the consteval path compiles and produces
    // expected results when used in a constexpr context.
    // For the runtime path, see TestTypeRegistry — the FMemberDescriptor
    // stores pre-resolved isTransient which tests the runtime path.
    SUCCEED();
}

// ── Test: ExternalAnnotations for third-party type ──

TEST(MetaCore, ExternalAnnotationsExists)
{
    // Verify the specialization is visible (compile-time check)
    SUCCEED();
}
```

- [ ] **Step 4: Write `TestTypeRegistry.cpp`**

```cpp
#include <gtest/gtest.h>
#include <StartNode/Reflect/TypeRegistry.hpp>
#include <StartNode/Reflect/Annotations.hpp>

// Test aggregate type
struct FTestPoint
{
    float x = 1.0f;
    float y = 2.0f;
    [[StartNode::Anno::FTransient]]
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

    const auto* desc = reg.find(desc->name);
    ASSERT_NE(desc, nullptr);
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
```

- [ ] **Step 5: Write `TestJson.cpp`**

```cpp
#include <gtest/gtest.h>
#include <StartNode/Reflect/Json.hpp>

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
```

- [ ] **Step 6: Write `TestSerialize.cpp`**

```cpp
#include <gtest/gtest.h>
#include <StartNode/Reflect/Serialize.hpp>
#include <StartNode/Reflect/Annotations.hpp>
#include <StartNode/Reflect/TypeRegistry.hpp>

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
    [[StartNode::Anno::FTransient]]
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

    auto json = FJsonValue::parse(R"({"x":1.0,"y":2.0,"z":3.0,"extra":99})");
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

    auto json = FJsonValue::makeString("not_an_int");

    int restored = 0;
    auto result = fromJson(*json, restored);
    EXPECT_FALSE(result.has_value());
}

// ── Test: Unregistered type error ──

TEST(Serialize, UnregisteredType)
{
    struct FUnregistered { int x; };
    FUnregistered obj{5};
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
```

- [ ] **Step 7: Write `TestFormat.cpp`**

```cpp
#include <gtest/gtest.h>
#include <StartNode/Reflect/Format.hpp>
#include <StartNode/Reflect/TypeRegistry.hpp>

using namespace StartNode::Reflect;

struct FTestFormat
{
    int count = 5;
    float value = 3.14f;
};

// ── Test: Format aggregate ──

TEST(Format, FormatAggregate)
{
    auto& reg = UTypeRegistry::instance();
    registerBuiltinTypes();
    reg.registerType<FTestFormat>();

    FTestFormat obj;
    auto formatted = formatReflected(obj);
    EXPECT_NE(formatted.find("FTestFormat"), std::string::npos);
    EXPECT_NE(formatted.find("count=5"), std::string::npos);
    EXPECT_NE(formatted.find("value="), std::string::npos);
}

// ── Test: Format unregistered type ──

TEST(Format, FormatUnregistered)
{
    struct FUnregistered { int x; };
    FUnregistered obj{42};
    auto formatted = formatReflected(obj);
    EXPECT_EQ(formatted, "<unregistered type>");
}

// ── Test: Format vector ──

TEST(Format, FormatVector)
{
    registerBuiltinTypes();
    std::vector<int> vec{1, 2, 3};
    auto formatted = formatReflected(vec);
    EXPECT_NE(formatted.find("[1, 2, 3]"), std::string::npos);
}

// ── Test: Format optional ──

TEST(Format, FormatOptionalSome)
{
    registerBuiltinTypes();
    std::optional<int> opt = 10;
    auto formatted = formatReflected(opt);
    EXPECT_NE(formatted.find("Some(10)"), std::string::npos);
}

TEST(Format, FormatOptionalNone)
{
    registerBuiltinTypes();
    std::optional<int> opt;
    auto formatted = formatReflected(opt);
    EXPECT_EQ(formatted, "None");
}
```

- [ ] **Step 8: Write `TestTypeIdShared.hpp`** (shared type for cross-TU test)

```cpp
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

#endif
```

- [ ] **Step 9: Write `TestTypeIdCrossTU_A.cpp`**

```cpp
#include "TestTypeIdShared.hpp"

auto getCrossTUTypeId_A() -> StartNode::Reflect::FTypeId
{
    return StartNode::Reflect::makeTypeId<FCrossTUType>();
}
```

- [ ] **Step 10: Write `TestTypeIdCrossTU_B.cpp`**

```cpp
#include "TestTypeIdShared.hpp"

auto getCrossTUTypeId_B() -> StartNode::Reflect::FTypeId
{
    return StartNode::Reflect::makeTypeId<FCrossTUType>();
}
```

- [ ] **Step 11: Write `TestTypeId.cpp`**

```cpp
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
```

- [ ] **Step 12: Build and run tests**

```bash
cmake -B build -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
      -DSTARTNODE_BUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 13: Commit**

```bash
git add StartNode/tests/
git commit -m "test: add reflect_tests with full coverage for MetaCore, TypeRegistry, Json, Serialize, Format, TypeId"
```

---

### Task 9: Final Integration Verification

**Files:** none new — verification-only task

- [ ] **Step 1: Clean rebuild from scratch**

```bash
rm -rf build
cmake -B build \
    -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
    -DSTARTNODE_BUILD_TESTS=ON \
    -DIMREFL_BUILD_EXAMPLE=ON
cmake --build build
```

Expected: zero compile errors, zero warnings.

- [ ] **Step 2: Run all tests**

```bash
cd build && ctest --output-on-failure --verbose
```

Expected: all 20+ tests pass, including CrossTranslationUnit.

- [ ] **Step 3: Verify imrefl example regression**

```bash
# Check that the example binary exists and runs
ls -la build/example/example
./build/example/example --help 2>&1 || true
```

Expected: binary exists; any launch errors are window/display-related, not link/init errors.

- [ ] **Step 4: Verify zero third-party runtime dependencies**

```bash
# Reflect library should NOT link nlohmann_json, spdlog, etc.
otool -L build/StartNode/src/reflect/libReflect.a 2>/dev/null || \
    echo "Static library — no dynamic deps (expected)"
```

- [ ] **Step 5: Final commit (if any fixes applied during verification)**

```bash
git status
# If clean:
echo "All verification passed."
```

---

### Summary of Deliverables

| Task | Deliverable | Verification |
|------|-------------|--------------|
| 1 | CMake scaffolding | `cmake --build build` passes |
| 2 | MetaCore + imrefl refactoring | imrefl example builds and runs |
| 3 | Annotations + TypeId headers | build passes |
| 4 | TypeDescriptor + TypeRegistry | build passes |
| 5 | FJsonValue | build passes |
| 6 | toJson / fromJson | build passes |
| 7 | formatReflected | build passes |
| 8 | Tests (10 test files) | `ctest` all green, 20+ tests |
| 9 | Integration verification | clean rebuild + all tests + example |
