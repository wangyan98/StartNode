# StartNode reflect — 反射框架设计规格书

> **版本**: 0.1
> **日期**: 2026-06-18
> **状态**: 待评审
> **上级规格**: SPEC_StartNode.md Phase 0（子集）

---

## 1. 目标与范围

### 1.1 实现什么

StartNode 的 `reflect` 模块（C++26 反射框架），做成可独立编译、独立验证的最小闭环。同时从现有 `imrefl.hpp` 抽取出无 ImGui 依赖的"meta 核心"为独立 INTERFACE 库 `MetaCore`，供 imrefl 与 reflect 共享。

### 1.2 不做

core（日志/EventBus/UUID）、plugin（PluginManager/Registry/ServiceLocator）、platform、rhi、引擎 Facade、节点图、diff/patch、binary 序列化——留给后续 spec。

### 1.3 成功标准（可验证）

1. `MetaCore` 作为独立 INTERFACE 库编译通过；imrefl.hpp 改为依赖它后，现有 `example/example.cpp` 构建+运行行为不回归。
2. `reflect` 模块编译通过，提供：内省（成员遍历）、`UTypeRegistry`/`FTypeDescriptor`、反射驱动 JSON 序列化（`toJson`/`fromJson`）、`formatReflected`、`StartNode::Anno` 注解系统。
3. 一个 `reflect_tests` 可执行：定义带注解的 struct → 注册 → 序列化为 JSON → 反序列化 → 断言相等；并用 `formatReflected` 格式化输出。全绿。
4. 零第三方运行时依赖（不引入 nlohmann/json、spdlog）。JSON 自写 writer/reader，格式化用 `std::format`。测试使用 GoogleTest（FetchContent）。
5. `FTypeId` 基于 `^^T` 的 fully-qualified name + 编译期 64-bit hash，跨编译单元一致。

### 1.4 张力声明

抽取 MetaCore 会改动已工作的 `imrefl.hpp`，范围超出"纯新增 reflect"。本 spec 将 imrefl 回归验证列为显式工作项。

### 1.5 命名规范（覆盖 SPEC_StartNode.md 第 8 节）

- **类型前缀**（UE 风格）：struct → `F`，class → `U`，interface → `I`，enum → `E`。例：`FTypeDescriptor`、`UTypeRegistry`、`INode`、`EPortDataType`
- **命名空间**：`StartNode::<Module>`。例：`StartNode::Reflect`、`StartNode::Meta`
- **物理目录**：去 `sn_` 前缀。模块目录为 `meta_core/`、`reflect/`（非 `sn_meta_core/`、`sn_reflect/`）
- **函数与变量**：小驼峰 camelCase。例：`registerType`、`typeId`、`memberName`
- **头文件路径**：`StartNode/Reflect/Annotations.hpp` 等，与命名空间对应
- **函数名**：完整清晰，不缩写。不准使用 `Desc`、`Reg`、`Cfg`、`Ptr`、`Buf`；`Id` 是可接受的标准缩写

---

## 2. 目录结构与 CMake

### 2.1 目录结构

```
imrefl/                          # 现有仓库根
├── imrefl.hpp                   # 改为 include MetaCore 头，删除重复定义
├── imrefl_glm.hpp
├── example/                     # 现有，作为 imrefl 回归验证
├── CMakeLists.txt               # 顶层，加 StartNode 子目录
└── StartNode/                   # 新建子项目
    ├── CMakeLists.txt           # StartNode 顶层
    ├── src/
    │   ├── meta_core/           # 抽出的共享 meta 核心（独立 INTERFACE 目标）
    │   │   ├── CMakeLists.txt
    │   │   └── include/StartNode/Meta/
    │   │       └── MetaCore.hpp # FAnnotationSet / ExternalAnnotations
    │   └── reflect/             # 反射框架模块
    │       ├── CMakeLists.txt
    │       ├── include/StartNode/Reflect/
    │       │   ├── Annotations.hpp   # StartNode::Anno 注解定义
    │       │   ├── TypeId.hpp        # FTypeId + makeTypeId
    │       │   ├── TypeDescriptor.hpp# FTypeDescriptor / FMemberDescriptor
    │       │   ├── TypeRegistry.hpp  # UTypeRegistry
    │       │   ├── Serialize.hpp     # toJson / fromJson
    │       │   ├── Json.hpp          # FJsonValue
    │       │   └── Format.hpp        # formatReflected
    │       └── src/
    │           ├── TypeRegistry.cpp
    │           ├── Json.cpp
    │           └── Serialize.cpp
    └── tests/
        └── reflect_tests/
            ├── CMakeLists.txt
            └── src/
                ├── TestMetaCore.cpp
                ├── TestTypeId.cpp
                ├── TestTypeDescriptor.cpp
                ├── TestTypeRegistry.cpp
                ├── TestJson.cpp
                ├── TestFormat.cpp
                └── TestMain.cpp
```

### 2.2 CMake 目标

**顶层 `imrefl/CMakeLists.txt`** 改动：
- `add_subdirectory(StartNode)` 在 `add_subdirectory(example)` 之前
- `ImRefl` INTERFACE 目标改为 `target_link_libraries(ImRefl INTERFACE MetaCore)`，去掉重复的 `-freflection` 等编译选项（改由 MetaCore 统一提供）

**StartNode/CMakeLists.txt**：

```cmake
cmake_minimum_required(VERSION 3.30)
project(StartNode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译器自动探查：优先 Homebrew clang
if(NOT DEFINED STARTNODE_CXX_COMPILER)
    find_program(HOMEBREW_CLANG clang++
        PATHS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
        NO_DEFAULT_PATH
    )
    if(HOMEBREW_CLANG)
        set(STARTNODE_CXX_COMPILER "${HOMEBREW_CLANG}" CACHE FILEPATH "C++ compiler for StartNode")
    endif()
endif()

if(STARTNODE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${STARTNODE_CXX_COMPILER}" CACHE FILEPATH "" FORCE)
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "StartNode requires a Clang compiler with C++26 reflection support")
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

**src/meta_core/CMakeLists.txt**：

```cmake
add_library(MetaCore INTERFACE)
target_include_directories(MetaCore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(MetaCore INTERFACE cxx_std_26)
target_compile_options(MetaCore INTERFACE -freflection)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MetaCore INTERFACE
        -fexpansion-statements -fannotation-attributes -fparameter-reflection)
endif()
```

**src/reflect/CMakeLists.txt**：

```cmake
add_library(Reflect STATIC
    src/TypeRegistry.cpp
    src/Json.cpp
    src/Serialize.cpp
)
target_include_directories(Reflect PUBLIC include)
target_link_libraries(Reflect PUBLIC MetaCore)
```

### 2.3 依赖方向

```
MetaCore (INTERFACE, 无依赖)
   ↑          ↑
imrefl      Reflect (STATIC)
```

---

## 3. MetaCore — 抽取与共享

### 3.1 职责

无 ImGui 依赖的反射元编程底座。从 `imrefl.hpp` 抽出，独立成 INTERFACE 库。

### 3.2 抽取内容（从 imrefl.hpp）

- `Config` 结构 → 重命名为 `FAnnotationSet`（注解容器，含 `fetchAnnotation<T>()` / `hasAnnotation<T>()`）
- `ExternalAnnotations<T>` 模板（为第三方类型附加注解）
- 注解提取的反射范式（`remove_cvref(type_of(attr))` 比较 + `std::meta::extract<T>`）

### 3.3 不抽取（留在 imrefl.hpp）

- 所有 ImRefl UI 注解定义（`Ignore`/`Readonly`/`Slider`/`Drag`/`Color` 等）——强 UI 语义
- 所有 `Renderer<>` 特化——依赖 ImGui
- `TreeNodeExNoDisable` 等 ImGui 工具函数

### 3.4 FAnnotationSet 双路径化

编译期路径服务 imrefl（consteval UI），运行时路径服务 reflect（运行时注解查询），两路径共享同一份编译期生成的静态数据，零堆分配。

```cpp
namespace StartNode::Meta {

struct FAnnotationSet {
    const std::meta::info* annotations = nullptr;
    std::size_t count = 0;

    // consteval 路径：imrefl 编译期 UI 用
    template <typename T>
    consteval auto fetchAnnotation() const -> std::optional<T>;

    template <typename T>
    consteval auto hasAnnotation() const -> bool;

    // 运行时路径：reflect 运行时查询用
    template <typename T>
    auto fetchAnnotationRuntime() const -> std::optional<T>;

    template <typename T>
    auto hasAnnotationRuntime() const -> bool;
};

template <typename T>
struct ExternalAnnotations {};

}
```

**关键约束**：`annotations` 指向的 `meta::info` 数组必须是 consteval 生成的静态数据（`static constexpr` 数组）。imrefl 走 consteval 路径行为完全不变；reflect 在运行时持有 `FAnnotationSet` 指向同一份静态数据，零堆分配，跨 so 一致（数据在编译期固定）。

### 3.5 imrefl.hpp 改动

- `#include "StartNode/Meta/MetaCore.hpp"`
- 删除 `Config`、`ExternalAnnotations` 定义
- 保留别名 `namespace ImRefl { using Config = StartNode::Meta::FAnnotationSet; }` 以最小化 example 改动
- `FetchAttn`/`HasAttn` 方法名保留为 imrefl 内 aliases

### 3.6 回归验证

`example/example.cpp` 构建并运行（CMake `-DIMREFL_BUILD_EXAMPLE=ON`），UI 行为与抽取前一致。

---

## 4. 注解系统（StartNode::Anno）

### 4.1 职责

定义系统行为注解，控制序列化、分类、显示等。与 ImRefl UI 注解并存但独立——ImRefl:: 控 UI，StartNode::Anno:: 控系统行为。

### 4.2 注解定义（Annotations.hpp）

```cpp
namespace StartNode::Anno {

struct FTransient {};
struct FDeprecated { const char* message; };
struct FVersion { int since; };
struct FRange { double min; double max; };
struct FDisplayName { const char* name; };
struct FTooltip { const char* text; };
struct FCategory { const char* category; };

}
```

### 4.3 使用示例

```cpp
struct FFluidSolverParams {
    [[StartNode::Anno::FCategory("Grid")]]
    [[StartNode::Anno::FRange(16, 1024)]]
    [[StartNode::Anno::FTooltip("Resolution of the simulation grid")]]
    int gridResolution = 128;

    [[StartNode::Anno::FCategory("Physics")]]
    float viscosity = 0.01f;

    [[StartNode::Anno::FTransient]]
    int particleCount = 0;
};
```

所有注解 struct 统一带 `F` 前缀。

---

## 5. TypeId、TypeDescriptor、TypeRegistry

### 5.1 FTypeId

基于 `^^T` 的 fully-qualified name 经编译期 64-bit hash 生成，跨编译单元/跨 so 一致。

```cpp
namespace StartNode::Reflect {

struct FTypeId {
    std::uint64_t value = 0;
    auto operator==(const FTypeId&) const -> bool = default;
};

template <typename T>
consteval auto makeTypeId() -> FTypeId;

}
```

- **源**：fully-qualified name（从 `^^T` 编译期拼接 `std::meta::identifier_of` / name）
- **Hash**：constexpr 的 64-bit hash（FNV-1a 或类似），编译期完成
- **一致性保证**：同名同类型 → 同 id；不同名/不同编译单元仍一致

### 5.2 FTypeDescriptor / FMemberDescriptor

丰富描述符（含成员表），运行时可遍历，数据全部指向编译期静态数组（零堆分配）。

```cpp
namespace StartNode::Reflect {

struct FMemberDescriptor {
    const char* name;               // 指向静态字符串
    std::size_t offset;             // 成员偏移
    FTypeId typeId;                 // 成员类型的 typeId
    FAnnotationSet annotations;     // 注解快照（指向静态数组）
    bool isTransient;               // FTransient 预解析
};

struct FTypeDescriptor {
    FTypeId typeId;
    const char* name;               // 指向静态字符串
    std::size_t sizeOf;
    std::span<const FMemberDescriptor> members;  // 指向静态数组

    // 工厂函数（类型擦除）
    auto (*defaultConstruct)(void* memory) -> void;
    auto (*copyConstruct)(void* memory, const void* other) -> void;
    auto (*destruct)(void* memory) -> void;
};

}
```

**生成方式**：consteval 函数遍历 `nonstatic_data_members_of(^^T)`，为每个成员填 `FMemberDescriptor`，存入 `static constexpr` 数组，构造 `FTypeDescriptor` 指向它。`isTransient` 预解析供序列化热路径快速判断。

### 5.3 UTypeRegistry

```cpp
namespace StartNode::Reflect {

class UTypeRegistry {
public:
    static auto instance() -> UTypeRegistry&;

    template <typename T>
    void registerType();

    auto find(FTypeId id) const -> const FTypeDescriptor*;
    auto find(std::string_view name) const -> const FTypeDescriptor*;
    auto registeredTypes() const -> std::vector<const FTypeDescriptor*>;

private:
    std::unordered_map<FTypeId, FTypeDescriptor> registry;
    std::unordered_map<std::string, FTypeId> nameIndex;
};

// 一次性注册内置类型，调用方在初始化时显式调用
void registerBuiltinTypes();

}
```

**注册方式**：所有类型显式注册（含 int/float/bool/string 等内置类型）。`registerBuiltinTypes()` 辅助函数一次性注册内置类型，调用方在初始化时显式调用而非自动。

**线程安全**：本 spec 不做（单线程）。plugin 阶段加 mutex。

---

## 6. JSON 序列化

### 6.1 FJsonValue（极简 JSON，零第三方依赖）

```cpp
namespace StartNode::Reflect::Json {

class FJsonValue {
public:
    enum class EType { Null, Bool, Int, Double, String, Array, Object };

    static auto makeNull() -> FJsonValue;
    static auto makeBool(bool value) -> FJsonValue;
    static auto makeInt(std::int64_t value) -> FJsonValue;
    static auto makeDouble(double value) -> FJsonValue;
    static auto makeString(std::string value) -> FJsonValue;
    static auto makeArray(std::vector<FJsonValue> values) -> FJsonValue;
    static auto makeObject(std::vector<std::pair<std::string, FJsonValue>> members) -> FJsonValue;

    auto getType() const -> EType;
    auto asBool() const -> bool;
    auto asInt() const -> std::int64_t;
    auto asDouble() const -> double;
    auto asString() const -> const std::string&;
    auto asArray() const -> const std::vector<FJsonValue>&;
    auto asObject() const -> const std::vector<std::pair<std::string, FJsonValue>>&;

    auto dump(std::size_t indent = 0) const -> std::string;
    static auto parse(std::string_view text) -> std::optional<FJsonValue>;
};

}
```

**实现范围**：标准 JSON 子集——null/bool/number/string/array/object，UTF-8，转义处理。不支持注释、尾随逗号等扩展。`parse` 是手写递归下降解析器。

### 6.2 反射驱动序列化

```cpp
namespace StartNode::Reflect {

template <typename T>
auto toJson(const T& object) -> std::expected<Json::FJsonValue, std::string>;

template <typename T>
auto fromJson(const Json::FJsonValue& json, T& object) -> std::expected<void, std::string>;

}
```

**toJson 流程**：
1. 查 `UTypeRegistry` 找 `FTypeDescriptor`（未注册 → 返回 expected error）
2. 标量类型（已注册的 int/float/bool/string）→ 直接写值
3. 聚合类型 → 遍历 `members`，跳过 `isTransient` 成员，按 `offset` 取指针、按 `typeId` 递归 `toJson`
4. `std::vector<T>` → JSON array，逐个递归
5. `std::optional<T>` → null 或递归值

**fromJson 流程**：逆向，按成员名查找 JSON 字段，按 offset 写回。未知字段忽略（前向兼容）。类型不匹配返回 expected error（含错误描述）。

**容器支持范围**：`std::vector<T>` + `std::optional<T>`。其他容器（map/set/variant/tuple）留给后续。

---

## 7. 格式化（formatReflected）

### 7.1 API

```cpp
namespace StartNode::Reflect {

template <typename T>
auto formatReflected(const T& object) -> std::string;

}
```

### 7.2 行为

- 聚合类型：`TypeName{memberName=value, ...}`
- 标量：`std::format("{}", value)`（内置类型有 std::formatter）
- `std::vector`：`[a, b, c]`
- `std::optional`：`Some(x)` 或 `None`
- 未注册类型：返回 `"<unregistered type>"`

### 7.3 不做

- 不做 `std::formatter<T>` 特化（编译期分派与运行时反射有张力）。仅提供 `formatReflected` 自由函数。
- 日志系统（属 core 模块）不在本 spec。`formatReflected` 是纯函数，core 将来调用。

---

## 8. 测试计划

### 8.1 测试结构

`StartNode/tests/reflect_tests/`，FetchContent 拉 GoogleTest v1.15.2，单可执行 `reflect_tests`。

### 8.2 测试用例

| 分组 | 用例 | 验证点 |
|------|------|--------|
| `MetaCore` | `AnnotationFetch` | FAnnotationSet consteval + 运行时两路径都能取到注解值 |
| `MetaCore` | `ExternalAnnotations` | 第三方类型经 ExternalAnnotations 附加注解可查询 |
| `MetaCore` | `ImReflAlias` | ImRefl::Config 别名仍可用（回归） |
| `TypeId` | `CrossTranslationUnit` | 同一类型在两个 .cpp 里 makeTypeId<T>() 相等 |
| `TypeId` | `DistinctTypes` | 不同类型 id 不同 |
| `TypeDescriptor` | `MemberEnumeration` | 聚合的成员名/偏移/typeId 正确 |
| `TypeDescriptor` | `FactoryFunctions` | defaultConstruct/copyConstruct/destruct 正确 |
| `TypeRegistry` | `RegisterAndFind` | registerType 后按 id 和 name 都能找到 |
| `TypeRegistry` | `BuiltinTypes` | registerBuiltinTypes() 后 int/float/bool/string 可查 |
| `Json` | `RoundTripScalar` | int/float/bool/string toJson/fromJson 往返相等 |
| `Json` | `RoundTripAggregate` | 带 vector/optional 的 struct 序列化往返相等 |
| `Json` | `TransientSkipped` | [[FTransient]] 成员不出现在 JSON |
| `Json` | `UnknownFieldIgnored` | fromJson 忽略 JSON 中未知字段 |
| `Json` | `TypeError` | 类型不匹配返回 expected error |
| `Format` | `FormatAggregate` | formatReflected 输出正确的 Name{...} 格式 |
| `Format` | `FormatUnregistered` | 未注册类型返回 `"<unregistered type>"` |
| `Regression` | `ImReflExample` | imrefl example 构建+运行无回归（手动/CTest） |

### 8.3 跨编译单元测试

`CrossTranslationUnit` 需要两个 .cpp 各自调用 `makeTypeId<SomeType>()` 并比较相等——验证 name hash 跨 TU 一致。

---

## 9. 风险与开放项

### 9.1 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **运行时 `std::meta::extract<T>` 不可用** | FAnnotationSet::fetchAnnotationRuntime 无法在运行时从 meta::info 取注解值 | 实现 MetaCore 时先验证。fallback：consteval 预先把注解值烘焙成类型擦除静态表，运行时查表 |
| **imrefl.hpp 抽取导致回归** | example UI 行为变化 | CTest 检查退出码 + 实现期手动跑 example。保留 using Config = ... 别名最小化改动 |
| **typeId hash 碰撞** | 两个不同类型 id 相同 → Registry 查找错乱 | 64-bit FNV-1a，概率极低。未来可加注册时碰撞检测（同 id 不同 name 则报错） |
| **meta::info 跨 so 边界** | FAnnotationSet 持 meta::info* 指向静态数据，跨 so 传描述符时指针可能无效 | 本 spec 单进程单 so，不触发。plugin 阶段需重新评估——可能改用 name + 注解值烘焙而非裸 meta::info |
| **C++26 反射编译器锁定** | 必须用支持反射的 clang，Apple clang 不行 | CMake 自动探查 Homebrew clang，Apple clang 报 FATAL_ERROR |
| **反射编译时间长** | 大量模板实例化拖慢构建 | 本 spec 范围小，暂不优化。后续 precompiled header、显式实例化 |

### 9.2 开放项（本 spec 不解决）

- diff/patch（Undo/Redo 用）—— SPEC 2A.2 列了，本 spec 不做
- binary 序列化——不做
- `std::formatter<T>` 特化——不做（见第 7 节）
- 自动类型注册（全程序扫描）——plugin 阶段
- 线程安全 Registry——plugin 阶段
- 跨 so meta::info 稳定性——plugin 阶段重新评估
- 更多容器（map/set/variant/tuple）——后续
- 日志系统、EventBus、插件框架——后续 spec

### 9.3 验收门槛

1. MetaCore + Reflect + reflect_tests 全部编译通过（Homebrew clang 22+）
2. `reflect_tests` 全绿
3. imrefl `example/example.cpp` 构建运行无回归
4. 零第三方运行时依赖（仅测试用 GoogleTest）
5. 顶层 `cmake -B build -DSTARTNODE_BUILD_TESTS=ON && cmake --build build` 一键构建全部
