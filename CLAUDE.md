# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This repository contains two related C++ projects:

- **ImRefl** — A header-only C++26 compile-time reflection library that auto-generates ImGui editor UIs for C++ types. Uses `<meta>` reflection, expansion statements, and annotation attributes. The main file is `imrefl.hpp` (with `imrefl_glm.hpp` for GLM support).
- **StartNode** — A node-based simulation framework (early stage) with headless core, plugin architecture, and graph evaluation engine. Located in `StartNode/`.

## Build Commands

Both projects require a C++26-capable **GCC 16.1+** with reflection support (`-freflection -std=c++26`). Note: Homebrew Clang 22 does not provide `<meta>`; only GCC 16+ implements P2996 reflection. Use `-DSTARTNODE_CXX_COMPILER=/opt/homebrew/bin/g++-16` to specify the compiler explicitly.

### ImRefl (header-only library)

```bash
# Build with example
cmake -B build -DIMREFL_BUILD_EXAMPLE=ON
cmake --build build
```

### StartNode

```bash
cd StartNode
cmake -B build -DSTARTNODE_CXX_COMPILER=/opt/homebrew/bin/g++-16
cmake --build build

# With options
cmake -B build -DSN_BUILD_TESTS=ON -DSN_BUILD_PLUGINS=ON
cmake --build build

# Run tests
./build/StartNode/tests/reflect_tests/reflect_tests
```

Build options: `SN_BUILD_GUI` (ON), `SN_BUILD_CLI` (ON), `SN_BUILD_PLUGINS` (ON), `SN_BUILD_TESTS` (OFF).

## Architecture

### ImRefl

Trait-based architecture using template specialization:

- **Entry point**: `ImRefl::Input<T>(name, value)` extracts compile-time type info via `^^T` and delegates to `Renderer<config, T>::Render()`.
- **Renderer specializations** handle: aggregates (recurse fields), enums, scalars, containers, pointers/smart pointers, optional/variant, tuple, bitset, complex, etc.
- **Config** is a consteval-only struct carrying `std::meta::info*` annotation pointers, passed as a non-type template parameter.
- **Annotations** (e.g., `ImRefl::slider`, `ImRefl::color`, `ImRefl::ignore`, `ImRefl::readonly`) control UI rendering per-field.
- **ExternalAnnotations<T>** template allows annotating third-party types you don't own.
- **ImGuiID** is an RAII wrapper for ImGui ID stack push/pop.

### StartNode

Layered architecture with plugin-driven extensibility (UE-style naming: `F`=struct, `U`=class, `I`=interface, `E`=enum):

- **Reflect** (`StartNode::Reflect`): FTypeId (compile-time FNV-1a hash), UTypeRegistry (singleton), FTypeDescriptor/FMemberDescriptor (consteval-generated), toJson/fromJson (reflection-driven JSON), formatReflected (debug output). 38/38 tests passing (Phase 0 complete).
- **Meta** (`StartNode::Meta`): FAnnotationSet (consteval-only annotation container, shared by ImRefl and Reflect).
- **Core** (`StartNode::Core`): UUID (64-bit), EventBus (type-safe pub/sub), spdlog-based logging.
- **Plugin** (`StartNode::Plugin`): IPlugin interface with `on_init()`/`on_shutdown()` lifecycle, PluginManager for dynamic .so/.dll loading, `URegistry<T>` generic factory, `SN_EXPORT_PLUGIN()` macro for C ABI.
- **NodeGraph** (`StartNode::NodeGraph`): INode interface (`evaluate(EvalContext)` → `vector<any>`), PortDescriptor/Port, Connection, NodeGraph (DAG with dirty propagation), EvalEngine (pull-based recursive evaluation).
- Plugins register node types; the graph instantiates them by name via URegistry.

## Key Conventions

### ImRefl
- ImRefl `Renderer::Render()` always returns `bool` (true if value changed).
- Every Renderer has dual overloads: mutable `T&` and const `const T&` (const renders as read-only).
- Container Renderers use `ImGuiID` guard for unique ImGui IDs per element.
- ImRefl uses its own style (e.g., `snake_case` names). Do not apply StartNode conventions to ImRefl code.

### StartNode (UE-style)
- **Type prefixes**: `F` for structs (`FTypeDescriptor`, `FTypeId`), `U` for classes (`UTypeRegistry`), `I` for interfaces (`INode`, `ISolver`), `E` for enums (`EPortDataType`).
- **Namespace**: `StartNode::<Module>` (no `sn::` prefix). E.g., `StartNode::Reflect`, `StartNode::Meta`.
- **Directories**: no `sn_` prefix. E.g., `reflect/`, `core/`, `plugin/`, `nodegraph/`.
- **Functions/variables**: `camelCase` (e.g., `registerType`, `makeTypeId`, `toJson`, `formatReflected`).
- **Headers**: `include/StartNode/<Module>/<Header>.hpp`, private impl in `src/`.
- **Annotation syntax (GCC 16)**: `[[= StartNode::Anno::transient]]` (with leading `=`). See SPEC §2A.3.
- **consteval boundary**: `FAnnotationSet` is consteval-only (holds `info*`); runtime code uses pre-parsed fields like `isTransient`.

### General
- Use nlohmann/json for JSON serialization in node parameters.
- The SPEC_StartNode.md file contains the full technical specification (in Chinese).
- **Reflection-first**: prefer reflection-driven serialization/formatting over hand-written boilerplate.
- **Interface-first**: module communication must go through interfaces (`I*`), no direct `#include` of internal impl headers.
- **Plugin-friendly**: new node types/solvers/asset formats should be implemented as plugins, even built-in ones.
