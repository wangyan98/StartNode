# StartNode — 节点式实时仿真软件技术规格书

> **版本**: 0.4 (Draft)
> **日期**: 2026-06-22
> **状态**: 待评审
>
> **变更摘要 (0.3 → 0.4)**: §2A 反射框架按已实现代码重写(GCC 16 工具链、UE 命名、`[[= ...]]` 注解语法); §5.1/§7 主编译器由 Clang 改为 GCC 16; §8 编码规范改为 UE 风格。`reflect` 模块(Phase 0)已完成,38/38 测试通过。

---

## 1. 项目概述

### 1.1 愿景

StartNode 是一个类 Houdini 的节点式实时仿真软件，采用统一节点图架构，所有仿真（水、火、布料）与程序化生成均在 GPU Compute Shader 上执行，通过 Vulkan RHI 实现跨平台实时渲染。UI 层基于 ImGui + ImRefl，利用 C++26 反射自动生成节点属性面板。

**核心架构原则**: UI 与核心逻辑完全解耦。所有节点图求值、仿真计算、资产管线等功能构成独立的 **Headless Core**，可脱离 GUI 运行。项目产出两个可执行文件：
- **`startnode`** — GUI 编辑器（交互式节点编辑 + 实时预览）
- **`startnode-cli`** — 命令行工具（批处理、自动化管线、CI/CD 集成）

### 1.2 核心目标

- 实时交互式节点图编辑，所见即所得
- **Headless 优先**: 所有核心功能均可通过 CLI 访问，不依赖窗口系统
- **反射驱动 (Reflection-Driven)**: 最大化利用 C++26 `<meta>` 反射，贯穿序列化、节点注册、UI 生成、插件发现全流程，消除手写样板代码
- **插件化架构 (Plugin Architecture)**: 所有功能模块（节点、仿真求解器、资产格式、渲染Pass）均可作为动态插件加载/卸载，支持第三方扩展
- **设计模式解耦**: 模块间通过接口抽象、事件总线、服务定位器等模式通信，可独立编译/测试/替换
- GPU 驱动的高性能仿真管线
- 跨平台（Windows / macOS / Linux）
- 支持主流 DCC 格式互操作

### 1.3 目标用户

- 技术美术 (TA)
- 特效开发者
- 仿真研究人员
- 独立游戏/影视工作室

---

## 2. 系统架构

### 2.1 总体分层

```
┌─────────────────────────────────────────────────────────────┐
│                      Frontends (可执行文件)                   │
│  ┌──────────────────────┐  ┌──────────────────────────────┐ │
│  │  startnode (GUI)     │  │  startnode-cli (命令行)       │ │
│  │  ImGui + ImRefl +    │  │  参数解析 + 批处理引擎 +      │ │
│  │  Node Editor +       │  │  脚本接口                     │ │
│  │  Viewport            │  │                              │ │
│  └──────────┬───────────┘  └──────────────┬───────────────┘ │
│             │                             │                 │
│             └──────────┬──────────────────┘                 │
│                        ▼                                    │
├─────────────────────────────────────────────────────────────┤
│              Plugin Manager + Dynamic Plugins               │
│         (.so/.dll/.dylib 动态加载, 热插拔)                   │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┐  │
│  │ Node     │ Solver   │ Asset    │ Render   │ Third-   │  │
│  │ Plugins  │ Plugins  │ Plugins  │ Plugins  │ Party    │  │
│  └──────────┴──────────┴──────────┴──────────┴──────────┘  │
├─────────────────────────────────────────────────────────────┤
│                  Headless Core (共享库)                       │
│  ┌──────────────┬──────────────┬───────────────────────┐    │
│  │  Node Graph  │   Renderer   │   Simulation Engine   │    │
│  │   Engine     │   (Realtime) │   (GPU Compute)       │    │
│  ├──────────────┴──────────────┴───────────────────────┤    │
│  │                  Asset Pipeline                      │    │
│  │          (DCC Import / Export / Cache)               │    │
│  ├──────────────────────────────────────────────────────┤    │
│  │                   Core Library                       │    │
│  │  ┌────────────────────────────────────────────────┐ │    │
│  │  │  C++26 Reflection Framework (reflect)       │ │    │
│  │  │  序列化 / 类型注册 / 插件发现 / UI绑定 / 调试  │ │    │
│  │  └────────────────────────────────────────────────┘ │    │
│  │  Math / Memory / Job System / Event Bus / Logging   │    │
│  ├──────────────────────────────────────────────────────┤    │
│  │                   Vulkan RHI                         │    │
│  │     (跨平台渲染硬件抽象层, 支持 Headless 模式)        │    │
│  ├──────────────────────────────────────────────────────┤    │
│  │              Platform Abstraction                    │    │
│  │     (Window / Input / FileSystem / Headless)        │    │
│  └──────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Headless Core vs Frontend 边界

**Headless Core** 包含所有可无窗口运行的模块：

| 模块 | Headless | 需要窗口 |
|------|----------|---------|
| `platform` (FileSystem/Timer) | Yes | — |
| `platform` (Window/Input) | — | GUI only |
| `core` | Yes | — |
| `rhi` (Compute/Transfer) | Yes | — |
| `rhi` (Swapchain/Present) | — | GUI only |
| `nodegraph` | Yes | — |
| `simulation` | Yes | — |
| `pcg` | Yes | — |
| `asset` | Yes | — |
| `renderer` | Yes (离屏渲染) | GUI 实时预览 |

**Frontend** 是薄层，仅处理用户交互：
- **GUI (`ui` + `app`)**: ImGui 界面、节点编辑器画布、Viewport 嵌入
- **CLI (`cli`)**: 命令解析、批处理循环、进度输出

### 2.3 模块依赖关系

```
Platform ← Core ← Vulkan RHI ← Renderer
                              ← Simulation Engine
                 ← Node Graph Engine
                 ← Asset Pipeline

Headless Core ← ui  ← app   (GUI 可执行文件)
              ← cli             (CLI 可执行文件)
```

---

## 2A. C++26 反射框架 (`reflect` + `meta_core`)

### 2A.1 设计理念

C++26 的 `<meta>` 反射不仅用于 ImRefl（UI 生成），而是作为 **全系统的元编程基石**，贯穿以下场景：

| 应用场景 | 传统做法 | 反射驱动做法 |
|----------|---------|-------------|
| UI 属性面板 | 手写 ImGui 控件代码 | `ImRefl::Input()` 自动生成 |
| 序列化/反序列化 | 手写 `toJson()`/`fromJson()` | 编译期遍历成员自动序列化（`StartNode::Reflect::toJson`/`fromJson`） |
| 节点注册 | 宏 `REGISTER_NODE(...)` | 反射参数 struct 自动发现端口和参数 |
| 插件元数据 | 手写导出函数 | 反射 `FPluginInfo` struct 自动导出 |
| CLI 参数绑定 | 手动 `parser.add("--xxx")` | 反射 struct 成员自动映射到 CLI flag |
| 调试/日志 | 手写 `operator<<` | `formatReflected` 自动格式化任意 struct |
| 类型注册表 | 手动 type-id 映射 | 编译期 `makeTypeId<T>()` 生成唯一 `FTypeId` |
| Undo/Redo diff | 手写成员比较 | 反射遍历自动生成 diff（后续阶段） |

> **实现状态**: 序列化(JSON)、类型注册表、格式化、注解解析已在 Phase 0 完成(`StartNode/` 子目录,38/38 测试通过)。Binary 序列化、Diff、节点端口自动发现、CLI 参数绑定、插件元数据导出属后续阶段(Phase 2+)。

### 2A.2 核心反射工具

```cpp
namespace StartNode::Reflect {

// ── 类型标识 ──

// 64-bit 类型 ID,编译期由类型全限定名的 FNV-1a 哈希生成。
// 同一类型在所有翻译单元/动态库中哈希一致。
struct FTypeId {
    std::uint64_t value = 0;
    auto operator==(const FTypeId&) const -> bool = default;
};

template <typename T>
consteval auto makeTypeId() -> FTypeId;          // = makeTypeIdFromInfo(^^T)
consteval auto makeTypeIdFromInfo(std::meta::info) -> FTypeId;  // 成员类型走这条(避免 splice)

// ── 类型描述符 ──

// 单个非静态数据成员的运行时描述。全部数据在 consteval 阶段预解析,
// 不持有 std::meta::info(否则类型会变成 consteval-only,无法运行时存储)。
struct FMemberDescriptor {
    char name[64] = {};          // 成员名(拷入定长缓冲,非指针——define_static_array 要求结构化类型)
    std::size_t offset = 0;      // 距对象首字节偏移(offset_of().bytes)
    FTypeId typeId;              // 成员类型哈希
    bool isTransient = false;    // 预解析自 [[= StartNode::Anno::transient]]
};

// 运行时类型描述符。members 是 consteval 生成的静态数组的 span。
struct FTypeDescriptor {
    FTypeId typeId;
    char name[64] = {};
    std::size_t sizeOf = 0;
    std::span<const FMemberDescriptor> members;
    void (*defaultConstruct)(void*) = nullptr;
    void (*copyConstruct)(void*, const void*) = nullptr;
    void (*destruct)(void*) = nullptr;
};

template <typename T>
consteval auto makeTypeDescriptor() -> FTypeDescriptor;  // 反射 T 的成员生成静态数组

// ── 类型注册表 ──

class UTypeRegistry {
public:
    static auto instance() -> UTypeRegistry&;

    template <typename T>
    void registerType();               // consteval 生成 FTypeDescriptor 并登记(重复调用幂等)

    auto find(FTypeId id) const -> const FTypeDescriptor*;
    auto find(std::string_view name) const -> const FTypeDescriptor*;
    auto registeredTypes() const -> std::vector<const FTypeDescriptor*>;
};

void registerBuiltinTypes();           // bool/int32/int64/float/double/string

// ── 自动序列化(JSON) ──

// 遍历所有非 transient 反射成员,递归序列化。
// std::vector / std::optional / 标量 / 注册的聚合各自有专门路径。
template <typename T>
auto toJson(const T& obj) -> std::expected<Json::FJsonValue, std::string>;

template <typename T>
auto fromJson(const Json::FJsonValue& json, T& obj) -> std::expected<void, std::string>;
// transient 成员序列化时跳过,反序列化时不覆盖(保留运行时值)。
// 未知 JSON 字段被忽略(向前兼容)。

// ── 自动格式化 ──

template <typename T>
auto formatReflected(const T& obj) -> std::string;   // "FPlayerInfo{name=\"Alice\", score=100}"

}  // namespace StartNode::Reflect

// ── 下列为后续阶段(Phase 2+),Phase 0 尚未实现 ──
//   Binary 序列化: to_binary / from_binary
//   自动 diff:     diff<T>(a, b) -> std::vector<MemberDiff>(用于 Undo/Redo)
```

### 2A.3 注解系统

注解分为两层：**ImRefl 注解**（UI 相关）和 **StartNode 注解**（系统行为）。

> **语法说明 (GCC 16)**: 注解使用 GCC 的属性赋值语法 `[[= ...]]`（前导 `=`），而非 C++26 草案的 `[[...]]` 形式。空标记注解需要一个 `constexpr` 全局实例;带参注解需要一个 `consteval` 工厂函数（字符串载荷经 `std::define_static_string` 存入静态存储）。两者均在 `Annotations.hpp` 中随结构体一并提供。

```cpp
// StartNode 系统注解 — 控制序列化/节点/插件行为
namespace StartNode::Anno {

struct FTransient {};                                  // 该成员不参与序列化
inline static constexpr FTransient transient {};       // [[= StartNode::Anno::transient]] 用

struct FDeprecated { const char* message; };
consteval FDeprecated deprecated(std::string_view message);

struct FVersion { int since; };
consteval FVersion version(int since);

struct FRange { double min; double max; };
consteval FRange range(double min, double max);

struct FDisplayName { const char* name; };
consteval FDisplayName displayName(std::string_view name);

struct FTooltip { const char* text; };
consteval FTooltip tooltip(std::string_view text);

struct FCategory { const char* category; };
consteval FCategory category(std::string_view category);

}  // namespace StartNode::Anno

// 使用示例
struct FluidSolverParams {
    [[= StartNode::Anno::category("Grid")]]
    [[= StartNode::Anno::range(16, 1024)]]
    [[= StartNode::Anno::tooltip("Resolution of the simulation grid")]]
    [[= ImRefl::slider(16, 1024)]]
    int gridResolution = 128;

    [[= StartNode::Anno::category("Physics")]]
    [[= ImRefl::drag(0.0f, 1.0f)]]
    float viscosity = 0.01f;

    [[= StartNode::Anno::transient]]   // 不序列化, 运行时计算值
    [[= ImRefl::readonly]]
    int particleCount = 0;
};
```

> **运行时与 consteval 的边界**: `FAnnotationSet`（持有 `const std::meta::info*`）是 **consteval-only 类型**——在 GCC 16 上,含 `info*` 的结构体无法运行时构造/读取。因此 ImRefl 的编译期 UI 分发在 consteval 上下文内消费它;Reflect 的运行时序列化路径不持有 `FAnnotationSet`,而是在 consteval 构造 `FTypeDescriptor` 时把所需信息（如 `isTransient`）预解析为普通运行时类型。详见 §7 风险表第 1 项。

### 2A.4 反射在各模块中的应用

```
reflect (核心反射工具, 目录: StartNode/src/reflect/, 命名空间 StartNode::Reflect)
    ├── core/Serialization    → 自动 JSON 序列化(toJson/fromJson)
    ├── nodegraph             → 自动发现节点端口/参数, 自动注册 (后续阶段)
    ├── ui (ImRefl)           → 自动生成属性面板
    ├── cli                   → 反射 struct → CLI 参数映射 (后续阶段)
    ├── plugin                → 反射 FPluginInfo 元数据 (后续阶段)
    ├── asset                 → 资产类型自动注册 (后续阶段)
    └── core/Log              → formatReflected 自动格式化调试输出

注: MetaCore(StartNode/src/meta_core/, StartNode::Meta)提供 FAnnotationSet,
    为 ImRefl(编译期 UI)与 Reflect(运行时序列化)共享的注解基础设施。
```

---

## 2B. 插件架构 (`plugin`)

### 2B.1 插件类型

所有功能模块均可作为内置（静态链接）或外置（动态加载）插件存在：

| 插件类型 | 接口 | 示例 |
|----------|------|------|
| **NodePlugin** | 注册一组节点类型 | `plugin_geo_basic.so` (Box/Sphere/Grid) |
| **SolverPlugin** | 注册仿真求解器 | `plugin_flip_fluid.so` |
| **AssetPlugin** | 注册导入/导出格式 | `plugin_usd.so`, `plugin_fbx.so` |
| **RenderPlugin** | 注册渲染 Pass / 后处理效果 | `plugin_volumetric.so` |
| **UIPlugin** | 注册自定义 UI 面板 (GUI only) | `plugin_curve_editor.so` |

### 2B.2 插件生命周期

```
发现 → 加载 → 注册 → 运行 → 卸载
```

```cpp
// 插件通过反射自动导出元数据, 无需手写导出函数
struct MyPluginInfo {
    static constexpr auto name        = "FLIP Fluid Solver";
    static constexpr auto version     = sn::Version{1, 0, 0};
    static constexpr auto author      = "StartNode Team";
    static constexpr auto description = "GPU FLIP/APIC fluid simulation";
    static constexpr auto dependencies = std::array{"rhi", "nodegraph"};

    // 插件需要实现的接口
    using plugin_interface = sn::SolverPlugin;
};

// 唯一需要的导出宏 — 仅此一处样板代码
SN_EXPORT_PLUGIN(MyPluginInfo, MyFlipSolverPlugin)
```

### 2B.3 PluginManager

```cpp
namespace StartNode::Plugin {

class UPluginManager {
public:
    // 扫描目录下的 .so/.dll/.dylib 文件
    void scanDirectory(const std::filesystem::path& dir);

    // 加载单个插件
    auto load(const std::filesystem::path& path) -> FPluginHandle;

    // 卸载插件 (仅开发期热重载)
    void unload(FPluginHandle handle);

    // 按类型查询已加载插件
    template <typename PluginInterface>
    auto query() -> std::vector<PluginInterface*>;

    // 生命周期回调
    void initializeAll();   // 调用每个插件的 onInit()
    void shutdownAll();     // 调用每个插件的 onShutdown()

    // 插件事件
    EventBus::Handle onPluginLoaded;
    EventBus::Handle onPluginUnloaded;
};

}  // namespace StartNode::Plugin
```

### 2B.4 插件发现与加载

```
启动流程:
  1. 扫描默认插件目录:
     - <install>/plugins/          (内置插件)
     - ~/.startnode/plugins/       (用户插件)
     - 环境变量 SN_PLUGIN_PATH    (自定义路径)
  2. 对每个 .so/.dll:
     a. dlopen → 查找 plugin_info 符号
     b. 通过反射读取 PluginInfo 元数据
     c. 检查依赖是否满足 (dependencies)
     d. 版本兼容性检查
  3. 按依赖拓扑排序, 依次调用 on_init()
  4. 各插件向 Registry 注册自己的类型:
     - NodePlugin → NodeRegistry
     - SolverPlugin → SolverRegistry
     - AssetPlugin → AssetFormatRegistry
     - RenderPlugin → RenderPassRegistry
```

### 2B.5 插件目录结构

```
plugins/
├── builtin/                    # 随软件分发的内置插件
│   ├── plugin_geo_basic.so
│   ├── plugin_sim_fluid.so
│   ├── plugin_sim_smoke.so
│   ├── plugin_sim_cloth.so
│   ├── plugin_pcg.so
│   ├── plugin_asset_gltf.so
│   ├── plugin_asset_obj.so
│   └── ...
└── user/                       # 用户/第三方插件
    ├── my_custom_solver.so
    └── studio_proprietary_nodes.so
```

### 2B.6 CMake 插件构建

```cmake
# 定义一个插件目标的辅助函数
function(startnode_add_plugin TARGET_NAME)
    add_library(${TARGET_NAME} SHARED ${ARGN})
    target_link_libraries(${TARGET_NAME} PRIVATE plugin_sdk)
    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX "plugin_"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/builtin"
    )
endfunction()

# 使用示例
startnode_add_plugin(geo_basic
    src/plugins/geo_basic/box_node.cpp
    src/plugins/geo_basic/sphere_node.cpp
    src/plugins/geo_basic/grid_node.cpp
)
```

---

## 2C. 设计模式与模块解耦

### 2C.1 模式总览

| 模式 | 应用位置 | 目的 |
|------|---------|------|
| **Service Locator** | 全局跨模块服务访问 | 模块间零直接依赖 |
| **Observer / Event Bus** | 模块间通知 | 发布-订阅解耦 |
| **Registry / Factory** | 节点/求解器/资产格式注册 | 开放-封闭原则，插件扩展 |
| **Strategy** | 仿真算法、渲染通道 | 运行时可替换算法 |
| **Command** | Undo/Redo | 操作可逆化 |
| **Visitor** | 节点图遍历、序列化 | 不修改节点类添加新操作 |
| **Mediator** | 节点图求值引擎 | 节点间不直接通信 |
| **Adapter** | RHI、DCC 格式 | 统一异构接口 |
| **Facade** | Headless Core API | 为 CLI/GUI 提供简洁入口 |
| **Chain of Responsibility** | 资产导入 | 按格式分派到对应处理器 |

### 2C.2 Service Locator

全局服务定位器，模块通过接口查询服务，不直接依赖具体实现。

```cpp
namespace StartNode {

class ServiceLocator {
public:
    static ServiceLocator& instance();

    // 注册服务 — 通过反射自动获取接口类型 ID
    template <typename Interface, typename Impl>
    void provide(std::unique_ptr<Impl> service);

    // 获取服务
    template <typename Interface>
    auto get() -> Interface&;

    // 可选: 获取可能不存在的服务
    template <typename Interface>
    auto try_get() -> Interface*;
};

// 使用示例: renderer 不直接依赖 rhi 的具体类型
auto& rhi = ServiceLocator::instance().get<IRHIDevice>();
auto cmd = rhi.create_command_buffer();
}
```

**注册时机**: 各模块在初始化时向 ServiceLocator 注册自己的服务实现。

```cpp
// rhi 模块初始化
void StartNode::Rhi::init() {
    auto device = std::make_unique<VulkanDevice>(config);
    ServiceLocator::instance().provide<IRHIDevice>(std::move(device));
}
```

### 2C.3 Event Bus (Observer)

类型安全的事件总线，基于反射自动生成事件 ID。

```cpp
namespace StartNode {

class EventBus {
public:
    static EventBus& instance();

    // 发布事件
    template <typename Event>
    void publish(const Event& event);

    // 订阅事件 — 返回 handle 用于取消订阅
    template <typename Event>
    auto subscribe(std::function<void(const Event&)> handler) -> SubscriptionHandle;

    void unsubscribe(SubscriptionHandle handle);
};

// 事件定义 — 纯数据 struct, 通过反射自动序列化/日志
struct NodeParamChangedEvent {
    UUID node_id;
    std::string param_name;
    // 反射自动格式化: "NodeParamChangedEvent{node_id=..., param_name=...}"
};

struct PluginLoadedEvent {
    std::string plugin_name;
    sn::Version version;
};

struct FrameAdvancedEvent {
    int frame;
};

}  // namespace StartNode
```

**事件流示例**:
```
用户在 UI 修改参数
  → ui 发布 NodeParamChangedEvent
  → nodegraph 订阅: 标记下游节点 Dirty
  → renderer 订阅: 标记需要重绘
  → core/Log 订阅: 记录操作日志 (用于 Undo)
```

### 2C.4 Registry / Factory

统一的类型注册表模式，所有可扩展组件共用。

```cpp
namespace StartNode {

// 通用注册表模板 — 插件和内置模块使用相同接口注册
template <typename Interface>
class URegistry {
public:
    static auto instance() -> URegistry&;

    // 注册类型 — 通过反射自动获取 typeName
    template <typename Impl>
    void registerType();

    // 工厂方法: 按名称创建实例
    auto create(std::string_view typeName) -> std::unique_ptr<Interface>;

    // 查询所有已注册类型
    auto registeredTypes() -> std::vector<const FTypeDescriptor*>;

    // 按 category 过滤 (利用 StartNode::Anno::category 注解)
    auto typesInCategory(std::string_view category) -> std::vector<const FTypeDescriptor*>;
};

// 各模块特化
using NodeRegistry          = URegistry<INode>;
using SolverRegistry        = URegistry<ISolver>;
using AssetImporterRegistry = URegistry<IAssetImporter>;
using AssetExporterRegistry = URegistry<IAssetExporter>;
using RenderPassRegistry    = URegistry<IRenderPass>;

}  // namespace StartNode
```

**节点注册示例** (反射驱动，零样板):

```cpp
// 定义节点参数 struct — 反射自动发现所有成员
struct FBoxNodeParams {
    [[= StartNode::Anno::category("Dimensions")]]
    [[= ImRefl::drag(0.01f, 100.0f)]]
    vec3 size = {1.0f, 1.0f, 1.0f};

    [[= StartNode::Anno::category("Topology")]]
    [[= ImRefl::slider(1, 64)]]
    ivec3 divisions = {1, 1, 1};
};

// 定义节点类 — 实现 INode 接口
class FBoxNode : public INode {
public:
    // 反射自动从 FBoxNodeParams 推断输入参数
    // 反射自动发现输出类型 (Geometry)
    using Params = FBoxNodeParams;
    using Output = FGeometryData;

    auto evaluate(const Params& params, EvalContext& ctx) -> Output override;
};

// 注册 — 一行代码，反射自动填充 FTypeDescriptor
// (端口名称/类型/默认值/注解 全部从 Params struct 推断)
SN_REGISTER_NODE(FBoxNode, "Geo::Box", "Create a box primitive")
```

### 2C.5 Strategy

仿真求解器和渲染通道使用 Strategy 模式，运行时可替换。

```cpp
// 流体求解器策略接口
class IFluidSolver : public ISolver {
public:
    virtual void step(float dt, FluidState& state, const FluidParams& params) = 0;
    virtual auto solver_name() const -> std::string_view = 0;
};

// 具体策略 — 可通过插件提供
class FLIPSolver : public IFluidSolver { ... };
class SPHSolver  : public IFluidSolver { ... };
class APICSolver : public IFluidSolver { ... };

// FluidSolverNode 通过 SolverRegistry 查找当前选择的求解器
class FluidSolverNode : public INode {
    void evaluate(...) override {
        auto& registry = SolverRegistry::instance();
        auto solver = registry.create(params.solver_type);  // "FLIP" / "SPH" / "APIC"
        solver->step(dt, state, params);
    }
};
```

### 2C.6 Command (Undo/Redo)

所有用户操作封装为 Command 对象，利用反射自动生成 diff。

```cpp
namespace StartNode {

class ICommand {
public:
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual auto description() const -> std::string = 0;
};

// 通用参数修改 Command — 利用反射自动 diff
template <typename Params>
class ParamChangeCommand : public ICommand {
    UUID node_id;
    Params old_value;   // 反射自动深拷贝
    Params new_value;

public:
    void execute() override {
        get_node(node_id)->set_params(new_value);
    }
    void undo() override {
        get_node(node_id)->set_params(old_value);
    }
    auto description() const -> std::string override {
        // 反射自动 diff: "Changed gridResolution: 128 → 256"
        auto diffs = StartNode::Reflect::diff(oldValue, newValue);
        return formatDiffs(diffs);
    }
};

class CommandHistory {
public:
    void execute(std::unique_ptr<ICommand> cmd);
    void undo();
    void redo();
    auto can_undo() const -> bool;
    auto can_redo() const -> bool;
};

}  // namespace StartNode
```

### 2C.7 Visitor (节点图遍历)

在不修改节点类的前提下，添加新的图遍历操作。

```cpp
namespace StartNode {

// Visitor 接口 — 新操作通过添加新 Visitor 实现
class INodeGraphVisitor {
public:
    virtual void visit(const Node& node) = 0;
    virtual void visit(const Connection& conn) = 0;
};

// 具体 Visitor: 序列化、验证、DOT 导出等
class SerializationVisitor : public INodeGraphVisitor { ... };
class ValidationVisitor     : public INodeGraphVisitor { ... };
class DotExportVisitor      : public INodeGraphVisitor { ... };
class DependencyAnalyzer    : public INodeGraphVisitor { ... };

}  // namespace StartNode
```

### 2C.8 Adapter (资产格式)

统一不同第三方库的异构接口。

```cpp
// 统一资产导入接口
class IAssetImporter {
public:
    virtual auto supported_extensions() const -> std::vector<std::string> = 0;
    virtual auto import(const std::filesystem::path& path) -> AssetData = 0;
    virtual auto can_import(const std::filesystem::path& path) const -> bool = 0;
};

// 各格式的 Adapter — 包装第三方库
class GltfImporter : public IAssetImporter {    // 包装 tinygltf
    auto import(...) -> AssetData override;
};
class FbxImporter : public IAssetImporter {     // 包装 ufbx
    auto import(...) -> AssetData override;
};
class UsdImporter : public IAssetImporter {     // 包装 OpenUSD
    auto import(...) -> AssetData override;
};

// Chain of Responsibility: 按扩展名自动选择 Importer
class AssetImportChain {
    std::vector<IAssetImporter*> importers;  // 从 AssetImporterRegistry 获取
public:
    auto import(const std::filesystem::path& path) -> AssetData {
        for (auto* imp : importers) {
            if (imp->can_import(path)) return imp->import(path);
        }
        throw UnsupportedFormatError(path);
    }
};
```

### 2C.9 Facade (Headless Core API)

为 CLI 和 GUI 提供统一的高层入口，隐藏内部模块交互复杂度。

```cpp
namespace StartNode {

// Headless Core 的统一外观接口
class StartNodeEngine {
public:
    // 初始化 (GUI 或 Headless)
    static auto create(EngineConfig config) -> std::unique_ptr<StartNodeEngine>;

    // 场景管理
    auto load_scene(const std::filesystem::path& path) -> Scene&;
    auto new_scene() -> Scene&;
    void save_scene(const std::filesystem::path& path);

    // 节点操作
    auto create_node(std::string_view type_name) -> NodeHandle;
    void connect(NodeHandle src, int port, NodeHandle dst, int port);
    void set_param(NodeHandle node, std::string_view name, const Value& val);

    // 求值
    void cook(NodeHandle target, int frame);
    void cook_range(NodeHandle target, FrameRange range, CookCallback cb);

    // 插件
    auto plugin_manager() -> PluginManager&;

    // 服务
    auto services() -> ServiceLocator&;
    auto events() -> EventBus&;
    auto commands() -> CommandHistory&;
};

}  // namespace StartNode
```

### 2C.10 模块间依赖与复用规则

```
  ┌─────────┐
  │reflect│ ← 所有模块均可依赖, 提供反射/序列化/类型注册基础设施
  └────┬─────┘
       │
  ┌────▼─────┐
  │ core  │ ← 所有模块均可依赖, 提供 Math/Log/Event/Memory/JobSystem
  └────┬─────┘
       │
  ┌────▼──────┐
  │ plugin │ ← 所有可扩展模块依赖, 提供 Registry/PluginManager/ServiceLocator
  └────┬──────┘
       │
  ┌────▼──────────┐
  │ nodegraph  │ ← 定义 INode 接口, 不依赖具体节点实现
  └───────────────┘

  具体节点/求解器/资产格式 → 作为插件独立编译, 仅依赖接口
```

**规则**:
1. **向下依赖**: 上层模块可依赖下层，反之禁止
2. **接口依赖**: 模块间通过接口（`INode`, `ISolver`, `IAssetImporter`）通信，不依赖具体类
3. **注册解耦**: 具体实现通过 `Registry` 注册，使用方通过名称/类型查询，无需 `#include` 对方
4. **事件解耦**: 跨模块通知通过 `EventBus`，发布方和订阅方互不知晓
5. **服务解耦**: 跨模块服务通过 `ServiceLocator`，提供方和消费方互不 include

---

## 3. 模块详细规格

### 3.1 Platform Abstraction (`platform`)

**职责**: 屏蔽操作系统差异，提供统一的窗口、输入和文件系统接口。支持 Headless 模式。

| 子模块 | Headless 可用 | 说明 |
|--------|:---:|------|
| `FileSystem` | Yes | 路径规范化、文件监视（hot-reload）、异步 IO |
| `Timer` | Yes | 高精度时钟、帧计时、性能计数器 |
| `Process` | Yes | 子进程管理、环境变量、信号处理 |
| `Window` | No | 窗口创建/销毁/事件循环，基于 GLFW 或 SDL3 |
| `Input` | No | 键盘/鼠标/手柄输入统一抽象 |

**Headless 初始化**: 当 `StartNode::Platform::init(headless=true)` 时，跳过窗口和输入子系统。
所有依赖窗口的模块通过编译期 (`#ifdef SN_HEADLESS`) 或运行时检查隔离。

**平台特殊处理**:
- macOS: 通过 MoltenVK 桥接 Vulkan → Metal
- Linux: 支持 X11 和 Wayland

---

### 3.2 Core Library (`core`)

**职责**: 项目范围内通用的基础设施。

| 子模块 | 说明 |
|--------|------|
| `Math` | 向量/矩阵/四元数（glm 或自研），SIMD 优化 |
| `Memory` | Arena 分配器、Pool 分配器、GPU 内存追踪 |
| `JobSystem` | 任务图调度器（线程池 + work-stealing），用于资产加载等 CPU 侧并行 |
| `Log` | 结构化日志，分级过滤（Debug/Info/Warn/Error），反射自动格式化 |
| `Event` | EventBus 事件总线，发布-订阅，模块间零耦合通信（详见 2C.3） |
| `UUID` | 全局唯一标识符生成（节点/资产/连接） |

---

### 3.2A 反射框架 (`reflect` + `meta_core`)

**职责**: 基于 C++26 `<meta>` 的全系统反射基础设施（详见 2A）。**Phase 0 已完成**。

| 子模块 | 说明 | 状态 |
|--------|------|------|
| `FAnnotationSet` (meta_core) | 注解容器,consteval-only,ImRefl 与 Reflect 共享 | ✅ |
| `UTypeRegistry` / `FTypeDescriptor` | 运行时类型注册表,`FTypeId` → `FTypeDescriptor` | ✅ |
| `toJson` / `fromJson` | 反射驱动 JSON 自动序列化 | ✅ |
| `formatReflected` | 反射自动格式化 | ✅ |
| `Annotations` | StartNode 注解(`FTransient`/`FRange`/`FCategory` 等) | ✅ |
| Binary 序列化 | 编译期布局的零开销二进制读写 | 后续阶段 |
| `diff` | 反射成员级 diff / patch（用于 Undo/Redo） | 后续阶段 |

> 目录: `StartNode/src/reflect/`(命名空间 `StartNode::Reflect`)、`StartNode/src/meta_core/`(`StartNode::Meta`)。零第三方运行时依赖;`libReflect.a` 仅含 libstdc++/libc 符号。38/38 GoogleTest 测试通过。

---

### 3.2B 插件框架 (`plugin`)

**职责**: 插件加载/卸载/注册管理（详见 2B）。

| 子模块 | 说明 |
|--------|------|
| `PluginManager` | 插件发现、加载、依赖解析、生命周期管理 |
| `PluginSDK` | 插件开发 SDK 头文件，定义 `SN_EXPORT_PLUGIN` 宏和接口 |
| `ServiceLocator` | 全局服务定位器（详见 2C.2） |
| `Registry<T>` | 通用类型注册表模板（详见 2C.4） |

---

### 3.3 Vulkan RHI (`rhi`)

**职责**: 封装 Vulkan API，提供简洁的渲染硬件抽象接口。支持 Headless (无 Surface/Swapchain) 模式。

#### 3.3.1 初始化模式

| 模式 | VkInstance 扩展 | 说明 |
|------|----------------|------|
| **GUI** | `VK_KHR_surface` + 平台扩展 | 完整渲染 + 呈现 |
| **Headless** | 无 surface 扩展 | 仅 Compute / Transfer / 离屏渲染，无 Swapchain |

```cpp
RHIDeviceConfig config;
config.mode = RHIMode::Headless;  // 或 RHIMode::Windowed
config.enable_validation = true;
auto device = RHIDevice::create(config);
```

#### 3.3.2 核心抽象

| 抽象类型 | 对应 Vulkan 概念 | 说明 |
|----------|------------------|------|
| `RHIDevice` | VkDevice + VkPhysicalDevice | GPU 设备及能力查询 |
| `RHISwapchain` | VkSwapchainKHR | 呈现链管理、帧同步 |
| `RHICommandBuffer` | VkCommandBuffer | 命令录制，支持 Graphics / Compute / Transfer |
| `RHIBuffer` | VkBuffer + VmaAllocation | GPU 缓冲区（Vertex/Index/Uniform/Storage） |
| `RHITexture` | VkImage + VkImageView | 纹理资源，支持 2D/3D/Cube/Array |
| `RHISampler` | VkSampler | 采样器状态 |
| `RHIPipeline` | VkPipeline | Graphics / Compute Pipeline，PSO 缓存 |
| `RHIRenderPass` | VkRenderPass + VkFramebuffer | 渲染通道和附件管理 |
| `RHIDescriptorSet` | VkDescriptorSet | 资源绑定集 |
| `RHIFence / Semaphore` | VkFence / VkSemaphore | GPU-CPU / GPU-GPU 同步 |

#### 3.3.2 内存管理

- 集成 VMA (Vulkan Memory Allocator)
- Staging buffer 池（CPU→GPU 数据上传）
- Ring buffer 用于每帧动态 uniform 数据

#### 3.3.3 Shader 管理

- SPIR-V 作为运行时格式
- 集成 glslang 或 shaderc 进行运行时编译（开发期）
- Shader 反射获取 descriptor layout
- Shader hot-reload 支持

#### 3.3.4 帧图 (Frame Graph)

- 声明式渲染通道依赖描述
- 自动资源生命周期管理与 barrier 插入
- 支持异步 Compute 与 Graphics 并行

---

### 3.4 实时渲染器 (`renderer`)

**职责**: 在 RHI 之上构建完整的实时渲染管线。

#### 3.4.1 渲染管线

```
Shadow Pass → G-Buffer Pass → Lighting Pass → Post-Processing → UI Overlay → Present
                                    ↑
                          Simulation Visualization
                          (粒子/网格/体积渲染)
```

#### 3.4.2 功能矩阵

| 功能 | 优先级 | 说明 |
|------|--------|------|
| PBR 材质 | P0 | Metallic-Roughness 工作流 |
| IBL 环境光 | P0 | HDR 环境贴图 + 预过滤 |
| 阴影映射 | P0 | CSM (级联阴影) |
| 线框/点云渲染 | P0 | 仿真数据可视化基础 |
| 粒子渲染 | P0 | GPU 粒子 Billboard / Sprite |
| 体积渲染 | P1 | Ray Marching，用于烟雾/火焰 |
| 网格渲染 | P0 | 三角面/线框/法线可视化 |
| 后处理 | P1 | Tone Mapping / Bloom / FXAA |
| Gizmo | P0 | 平移/旋转/缩放操控手柄 |
| Grid | P0 | 无限网格地面参考 |

#### 3.4.3 Viewport 系统

- 支持多 Viewport 同时渲染
- 每个 Viewport 独立相机
- Viewport 渲染结果作为 ImGui Texture 嵌入 UI

---

### 3.5 节点图引擎 (`nodegraph`)

**职责**: 统一节点图的核心数据结构、求值引擎和序列化。

#### 3.5.1 数据模型

```cpp
// 核心数据类型（通过节点端口传递）
enum class PortDataType {
    Geometry,       // 网格/点云/曲线
    VolumeField,    // 3D 标量/向量场（密度、速度等）
    Particles,      // 粒子集合
    Scalar,         // float / int
    Vector,         // vec2 / vec3 / vec4
    Matrix,         // mat4
    String,         // 文件路径、名称等
    Texture,        // GPU 纹理句柄
    Curve,          // 动画曲线 / 样条
    Any,            // 泛型，运行时类型检查
};
```

#### 3.5.2 节点结构

```cpp
struct Node {
    UUID id;
    std::string type_name;      // 注册的节点类型标识
    std::string display_name;   // UI 显示名

    std::vector<InputPort> inputs;
    std::vector<OutputPort> outputs;

    // 节点参数 — 通过 ImRefl 反射自动生成 UI
    // 每个具体节点类型定义自己的参数 struct
    void* params;

    vec2 position;              // 在节点编辑器中的位置
    NodeState state;            // Clean / Dirty / Error / Computing
};

struct Connection {
    UUID id;
    UUID src_node;
    uint32_t src_port;
    UUID dst_node;
    uint32_t dst_port;
};
```

#### 3.5.3 求值引擎

| 特性 | 说明 |
|------|------|
| 求值策略 | 拉取式 (Pull/Lazy)：从输出节点向上游递归求值 |
| 脏标记传播 | 参数修改 → 标记下游节点为 Dirty |
| 缓存 | 每个端口缓存上次计算结果，Clean 节点不重复计算 |
| 时间轴 | 支持帧驱动求值，仿真节点逐帧累积 |
| 错误处理 | 节点级错误隔离，不阻塞整个图 |
| GPU 调度 | 仿真节点直接提交 Compute Dispatch，结果保留在 GPU |

#### 3.5.4 节点类别

| 类别 | 前缀 | 示例节点 |
|------|------|----------|
| 几何 | `Geo::` | Box, Sphere, Grid, Merge, Transform, Subdivide, Scatter |
| 仿真 | `Sim::` | FluidSolver, SmokeSolver, ClothSolver, RBDSolver |
| 场 | `Field::` | NoiseField, GradientField, CurlField, AdvectField |
| 粒子 | `Particle::` | Emitter, Gravity, Turbulence, Collision, Kill |
| 数学 | `Math::` | Add, Multiply, Remap, Clamp, Noise, VectorMath |
| 程序化 | `PCG::` | ScatterPoints, CopyToPoints, VoronoiFracture, L-System |
| IO | `IO::` | FileImport, FileExport, CacheSequence |
| 渲染 | `Render::` | Material, Texture, Light, Camera |
| 工具 | `Util::` | Switch, Merge, Null, TimeShift, ForEach |

#### 3.5.5 节点注册机制 (反射 + 插件驱动)

节点通过 C++26 反射自动注册，无需手写端口声明或参数映射：

```cpp
// 1. 定义参数 struct — 反射自动发现所有成员
struct FFluidSolverParams {
    [[= StartNode::Anno::category("Grid")]]
    [[= ImRefl::slider(16, 512)]]
    int gridResolution = 128;

    [[= StartNode::Anno::category("Physics")]]
    [[= ImRefl::drag(0.0f, 1.0f)]]
    float viscosity = 0.01f;

    [[= StartNode::Anno::category("Physics")]]
    [[= ImRefl::drag(0.0f, 10.0f)]]
    float gravity = 9.8f;

    [[= ImRefl::color]]
    vec3 displayColor = {0.2f, 0.5f, 1.0f};

    [[= StartNode::Anno::transient]]
    [[= ImRefl::readonly]]
    int particleCount = 0;
};

// 2. 实现节点 — 继承 INode 接口
class FFluidSolverNode : public INode {
public:
    using Params = FFluidSolverParams;
    // 反射自动从 Params 推断所有输入端口
    // 反射自动从返回类型推断输出端口

    auto evaluate(const Params& p, EvalContext& ctx) -> FGeometryData override;
};

// 3. 注册 — 可在插件或内置模块中
SN_REGISTER_NODE(FFluidSolverNode, "Sim::FluidSolver", "GPU FLIP fluid solver")
// 展开后: 在 NodeRegistry 中注册, FTypeDescriptor 由反射自动填充
```

**反射自动推断内容**:
- 端口列表: 从 `Params` 的成员类型推断输入端口；从 `evaluate()` 返回类型推断输出端口
- 参数默认值: 从成员默认初始化值读取
- UI 控件: 从 `ImRefl::` 注解生成
- 序列化: 从反射自动生成 `toJson` / `fromJson`
- 分类/提示: 从 `StartNode::Anno::` 注解读取

---

### 3.6 UI 层 (`ui`)

**职责**: 基于 ImGui + ImRefl 的完整编辑器界面。

#### 3.6.1 窗口布局

```
┌──────────────────────────────────────────────────────────────┐
│  Menu Bar                                                     │
├──────────┬───────────────────────────────────┬───────────────┤
│          │                                   │               │
│  Scene   │         3D Viewport               │  Properties   │
│  Outliner│                                   │  Panel        │
│          │                                   │  (ImRefl      │
│          │                                   │   auto-gen)   │
│          ├───────────────────────────────────┤               │
│          │                                   │               │
│          │      Node Editor Canvas           │               │
│          │                                   │               │
├──────────┴───────────────────────────────────┴───────────────┤
│  Timeline / Playback Controls                                 │
├──────────────────────────────────────────────────────────────┤
│  Console / Log Output                                         │
└──────────────────────────────────────────────────────────────┘
```

#### 3.6.2 子模块

| 子模块 | 说明 |
|--------|------|
| `NodeEditorCanvas` | 节点拖拽/连线/选择/框选/缩放/平移，基于 ImNodes 或自研 |
| `PropertiesPanel` | 选中节点的参数面板，通过 ImRefl::Input 自动生成 |
| `ViewportWidget` | 3D 视口嵌入 ImGui，鼠标交互/Gizmo/相机控制 |
| `Timeline` | 帧控制条、播放/暂停/步进、关键帧标记 |
| `SceneOutliner` | 场景对象树形列表 |
| `Console` | 日志输出、命令行输入 |
| `AssetBrowser` | 文件系统浏览、资产拖放导入 |
| `Preferences` | 全局设置面板（同样由 ImRefl 反射生成） |

#### 3.6.3 ImRefl 集成要点

- 每个节点的参数 struct 使用 ImRefl 注解
- 属性面板调用 `ImRefl::Input("Params", node->params)` 即可渲染完整 UI
- 参数变更自动触发节点脏标记
- 利用 `ExternalAnnotations` 为第三方类型（glm::vec3 等）定义 UI 表现

---

### 3.7 命令行工具 (`cli`)

**职责**: 提供 headless 模式下的节点图求值、仿真批处理和资产转换能力，无需启动 GUI。

#### 3.7.1 可执行文件

`startnode-cli` — 独立可执行文件，链接 Headless Core，不链接 ImGui/ImRefl/GLFW。

#### 3.7.2 子命令

```bash
# === 节点图操作 ===

# 求值节点图，将指定输出节点的结果写入文件
startnode-cli cook <scene.sn> --node "/out/export" --frames 1-240 --output ./cache/

# 仅求值单帧（默认当前帧）
startnode-cli cook <scene.sn> --node "/fluid_sim" --frame 100

# 列出场景中的所有节点
startnode-cli inspect <scene.sn> --list-nodes

# 查看某个节点的参数和端口
startnode-cli inspect <scene.sn> --node "/fluid_sim" --params

# 修改节点参数后求值（无需打开 GUI 手动调参）
startnode-cli cook <scene.sn> --node "/out/export" \
  --set "/fluid_sim.grid_resolution=256" \
  --set "/fluid_sim.viscosity=0.02"

# === 资产转换 ===

# 格式转换
startnode-cli convert input.fbx output.gltf
startnode-cli convert input.obj output.usd

# 批量转换
startnode-cli convert ./models/*.fbx --output-dir ./converted/ --format gltf

# === 仿真批处理 ===

# 运行仿真并缓存结果到磁盘
startnode-cli simulate <scene.sn> --solver "/fluid_sim" \
  --frames 1-500 --cache-dir ./sim_cache/

# 从缓存恢复并继续仿真
startnode-cli simulate <scene.sn> --solver "/fluid_sim" \
  --resume --frames 501-1000

# === 离屏渲染 ===

# 将 viewport 渲染到图片序列（Headless Vulkan 离屏渲染）
startnode-cli render <scene.sn> --camera "/render_cam" \
  --frames 1-240 --resolution 1920x1080 --output ./frames/

# === 工具 ===

# 验证场景文件完整性
startnode-cli validate <scene.sn>

# 输出节点图为 DOT 格式（可用 Graphviz 可视化）
startnode-cli graph <scene.sn> --format dot > graph.dot

# 查看 GPU 设备信息
startnode-cli info --gpu
```

#### 3.7.3 场景文件格式 (`.sn`)

节点图的序列化格式，CLI 和 GUI 共用同一格式：

```
scene.sn (JSON 或 Binary)
├── metadata: { version, author, created_at }
├── nodes: [
│     { id, type, display_name, position, params: {...} },
│     ...
│   ]
├── connections: [
│     { src_node, src_port, dst_node, dst_port },
│     ...
│   ]
└── settings: { frame_range, fps, global_params }
```

- GUI 编辑保存后，CLI 可直接加载求值
- CLI 修改参数后保存，GUI 可重新打开

#### 3.7.4 退出码

| 码 | 含义 |
|----|------|
| 0 | 成功 |
| 1 | 通用错误 |
| 2 | 参数错误 |
| 3 | 场景文件不存在或格式损坏 |
| 4 | 节点求值失败（具体错误输出到 stderr） |
| 5 | GPU 设备不可用 |

#### 3.7.5 进度输出

- 默认输出到 stderr：`[frame 42/240] Cooking /fluid_sim... 17.3%`
- `--quiet` 禁止进度输出
- `--json` 以 JSON 格式输出结果（便于脚本解析）
- `--log-level <debug|info|warn|error>` 控制日志级别

#### 3.7.6 脚本集成

CLI 设计为可被外部脚本/管线调用：

```python
# Python 管线示例
import subprocess, json

result = subprocess.run([
    "startnode-cli", "cook", "scene.sn",
    "--node", "/out/export",
    "--set", "/scatter.seed=42",
    "--json"
], capture_output=True)

output = json.loads(result.stdout)
print(f"Generated {output['point_count']} points")
```

---

### 3.8 仿真引擎 (`simulation`)

**职责**: 基于 GPU Compute Shader 的物理仿真管线。

#### 3.8.1 通用仿真框架

```
每帧循环:
  1. 读取节点图参数（从 CPU 传入 Uniform/Push Constant）
  2. Dispatch Compute Shader（一个或多个 pass）
  3. 结果留在 GPU Buffer 中
  4. 渲染器直接绑定该 Buffer 进行可视化
  → CPU 侧零拷贝（除非用户请求 readback）
```

#### 3.8.2 水/流体模拟 (`sim_fluid`)

| 项目 | 说明 |
|------|------|
| 方法 | 主要: FLIP/APIC（粒子-网格混合）；备选: SPH（纯粒子） |
| 网格 | MAC Grid（交错网格），存储在 3D Storage Buffer / Texture |
| 求解器步骤 | Advection → External Forces → Pressure Solve (Jacobi/PCG) → Projection |
| 边界 | SDF 碰撞体，支持静态和动态障碍物 |
| 表面重建 | Marching Cubes（GPU 实现）从粒子重建表面网格 |
| 输出端口 | Particles (position/velocity), Mesh (surface), VolumeField (pressure/velocity) |

#### 3.8.3 火/烟雾模拟 (`sim_smoke`)

| 项目 | 说明 |
|------|------|
| 方法 | 欧拉网格法，基于 Navier-Stokes |
| 存储 | 3D 体素网格：密度场、温度场、速度场 |
| 求解器步骤 | Advection → Buoyancy → Vorticity Confinement → Pressure Solve → Projection |
| 燃烧模型 | 简化燃料→温度→密度转换 |
| 渲染 | 体积 Ray Marching，Emission + Absorption 模型 |
| 输出端口 | VolumeField (density/temperature/velocity) |

#### 3.8.4 布料模拟 (`sim_cloth`)

| 项目 | 说明 |
|------|------|
| 方法 | PBD (Position Based Dynamics) 或 XPBD |
| 约束 | 距离约束、弯曲约束、固定点约束 |
| 碰撞 | 自碰撞检测（spatial hashing on GPU）、与 SDF 碰撞体的碰撞 |
| 输入 | 三角网格 + 约束参数（刚度、阻尼、迭代次数） |
| 输出端口 | Geometry (deformed mesh), Particles (vertices) |

#### 3.8.5 GPU 资源管理

- 仿真缓冲区通过 RHI 的 `RHIBuffer` (Storage Buffer) 分配
- 双缓冲 ping-pong 策略（当前帧/上一帧）
- Compute ↔ Graphics 通过 Pipeline Barrier 同步
- 可选 readback 用于 CPU 侧调试/导出

---

### 3.9 程序化内容生成 (`pcg`)

**职责**: 程序化建模与分布工具。

| 功能 | 说明 |
|------|------|
| Scatter | 在表面/体积内按密度分布点 |
| Copy to Points | 将模板几何体实例化到点集上（位置/旋转/缩放） |
| Voronoi Fracture | 基于 Voronoi 图切割网格 |
| L-System | 基于规则的分形/植物生成 |
| Noise | Perlin / Simplex / Worley，可作用于属性（位置/颜色/UV） |
| Curve Generation | 样条曲线生成，支持管道/放样等二次构造 |
| Boolean | 布尔运算（Union / Subtract / Intersect） |
| Remesh | 基于四面体/体素的重新网格化 |

---

### 3.10 资产管线 (`asset`)

**职责**: DCC 格式导入/导出与资产缓存。

#### 3.10.1 支持格式

| 格式 | 方向 | 库 | 说明 |
|------|------|----|------|
| USD (.usd/.usda/.usdc) | 导入/导出 | OpenUSD | 场景描述、材质、动画 |
| glTF / GLB | 导入/导出 | tinygltf / cgltf | PBR 材质、骨骼动画 |
| FBX | 导入 | OpenFBX / ufbx | 骨骼、动画、材质 |
| Alembic (.abc) | 导入/导出 | Alembic SDK | 烘焙几何序列缓存 |
| OBJ / MTL | 导入/导出 | tinyobjloader | 基础网格 |
| STL | 导入 | 自研 | 3D 打印用网格 |
| VDB (.vdb) | 导入/导出 | OpenVDB | 稀疏体积数据 |
| PLY | 导入/导出 | happly / 自研 | 点云 |

#### 3.10.2 缓存系统

- 仿真结果逐帧缓存到磁盘（自定义二进制格式或 Alembic）
- 缓存节点 (`IO::CacheSequence`) 控制回放和 bake
- 支持增量写入，不阻塞主线程

---

## 4. 数据流架构

### 4.1 几何数据表示

```cpp
struct GeometryData {
    // 属性化点集 — 类似 Houdini 的 Attribute 系统
    std::unordered_map<std::string, AttributeBuffer> point_attribs;
    // 必选: "P" (vec3) — 位置
    // 可选: "N" (vec3), "Cd" (vec3), "uv" (vec2), "id" (int), ...

    std::unordered_map<std::string, AttributeBuffer> vertex_attribs;
    std::unordered_map<std::string, AttributeBuffer> prim_attribs;
    std::unordered_map<std::string, AttributeBuffer> detail_attribs;

    IndexBuffer indices;        // 面索引
    PrimType prim_type;         // Points / Lines / Triangles / Quads
};

// 当数据在 GPU 时，AttributeBuffer 指向 RHIBuffer
// 当数据在 CPU 时（IO/调试），为 std::vector<T>
```

### 4.2 帧循环

#### GUI 模式

```
主循环 (每帧):
  1. Platform: 处理窗口/输入事件
  2. UI: ImGui NewFrame, 处理交互
  3. NodeGraph: 若有参数变更或帧推进 → 重新求值脏节点
     a. CPU 节点（IO/数学/PCG 部分）在 JobSystem 中执行
     b. GPU 仿真节点提交 Compute Dispatch 到 Compute Queue
  4. Renderer:
     a. Frame Graph 编译本帧所需的渲染通道
     b. 提交 Graphics Command Buffer
     c. 渲染结果输出到 Viewport Texture
  5. UI: ImGui Render（包含 Viewport Texture + Node Editor + Panels）
  6. RHI: Swapchain Present
```

#### Headless / CLI 模式

```
批处理循环 (startnode-cli cook):
  1. 加载 .sn 场景文件 → 构建节点图
  2. 应用 --set 参数覆盖
  3. for frame in frame_range:
     a. 设置当前帧
     b. NodeGraph: 从目标节点向上游 Pull 求值
        - CPU 节点在 JobSystem 中执行
        - GPU 节点提交 Compute Dispatch（Headless Vulkan）
     c. GPU Fence 等待计算完成
     d. Readback 结果到 CPU（如需写入磁盘）
     e. 写入输出文件（缓存/几何/图片）
     f. 输出进度到 stderr
  4. 清理资源，退出
```

**关键区别**: Headless 模式无渲染循环、无 Swapchain。GPU 仅用于 Compute/Transfer。
离屏渲染（`startnode-cli render`）会创建离屏 Framebuffer 替代 Swapchain。

---

## 5. 构建系统

### 5.1 工具链要求

| 工具 | 最低版本 | 说明 |
|------|----------|------|
| CMake | 3.30 | 项目构建 |
| GCC | 16.1+ | 主编译器,`-freflection -std=c++26`(Homebrew Clang 22 缺 `<meta>`,不可用) |
| Vulkan SDK | 1.3+ | RHI 底层 |
| Python | 3.10+ | 构建脚本/着色器编译工具（可选） |

> **编译器选型**: 实测 Homebrew Clang 22.1.x 不提供 `<meta>` 反射头,无法编译 P2996 代码。GCC 16.1 的 libstdc++ 完整实现 P2996(`-freflection`),且自带 `<inplace_vector>`/`<flat_map>` 等 C++26 库设施,作为主编译器。CMake 自动探测 `g++-16`,也可用 `-DSTARTNODE_CXX_COMPILER=/opt/homebrew/bin/g++-16` 显式指定。注解用 GCC 的 `[[= ...]]` 属性语法(见 §2A.3)。

### 5.2 CMake 项目结构

```
StartNode/
├── CMakeLists.txt              # 顶层
├── cmake/
│   ├── Dependencies.cmake      # 第三方依赖 FetchContent / find_package
│   ├── ShaderCompile.cmake     # GLSL → SPIR-V 编译规则
│   └── SnAddPlugin.cmake       # startnode_add_plugin() 辅助函数
├── src/
│   ├── platform/            # 平台抽象
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── reflect/             # C++26 反射框架 (所有模块依赖)
│   │   ├── CMakeLists.txt
│   │   ├── include/sn/reflect/
│   │   │   ├── type_registry.hpp
│   │   │   ├── serialization.hpp
│   │   │   ├── diff.hpp
│   │   │   ├── format.hpp
│   │   │   └── annotations.hpp
│   │   └── src/
│   ├── core/                # 核心库
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── plugin/              # 插件框架
│   │   ├── CMakeLists.txt
│   │   ├── include/sn/plugin/
│   │   │   ├── plugin_manager.hpp
│   │   │   ├── plugin_sdk.hpp      # 插件开发者 include 此文件
│   │   │   ├── service_locator.hpp
│   │   │   ├── registry.hpp
│   │   │   └── interfaces/         # 所有插件接口定义
│   │   │       ├── i_node.hpp
│   │   │       ├── i_solver.hpp
│   │   │       ├── i_asset_importer.hpp
│   │   │       ├── i_asset_exporter.hpp
│   │   │       └── i_render_pass.hpp
│   │   └── src/
│   ├── rhi/
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── renderer/
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── nodegraph/
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── simulation/          # 仿真引擎 (框架, 不含具体求解器)
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── pcg/                 # PCG 框架
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── asset/               # 资产管线框架
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── ui/                  # GUI only
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── app/                 # GUI 可执行文件 → startnode
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   └── cli/                 # CLI 可执行文件 → startnode-cli
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── cmd_cook.cpp
│       ├── cmd_simulate.cpp
│       ├── cmd_convert.cpp
│       ├── cmd_render.cpp
│       ├── cmd_inspect.cpp
│       └── cmd_validate.cpp
├── plugins/                    # 插件 (每个独立编译为 .so/.dll)
│   ├── CMakeLists.txt
│   ├── geo_basic/              # Box, Sphere, Grid, Transform, Merge
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── sim_fluid/              # FLIP/APIC/SPH 流体求解器
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── sim_smoke/              # 烟雾/火焰求解器
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── sim_cloth/              # 布料 PBD/XPBD 求解器
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── pcg_tools/              # Scatter, CopyToPoints, Noise, L-System
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── asset_gltf/             # glTF 导入/导出
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── asset_obj/              # OBJ 导入/导出
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── asset_fbx/              # FBX 导入
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── asset_usd/              # USD 导入/导出
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── asset_alembic/          # Alembic 导入/导出
│   │   ├── CMakeLists.txt
│   │   └── ...
│   └── asset_vdb/              # OpenVDB 导入/导出
│       ├── CMakeLists.txt
│       └── ...
├── shaders/
│   ├── compute/
│   │   ├── fluid_advect.comp
│   │   ├── fluid_pressure.comp
│   │   ├── smoke_step.comp
│   │   ├── cloth_pbd.comp
│   │   └── ...
│   └── graphics/
│       ├── pbr.vert / .frag
│       ├── particle.vert / .frag
│       ├── volume_raymarch.vert / .frag
│       ├── grid.vert / .frag
│       └── ...
├── assets/                     # 内置资源
├── tests/
│   ├── reflect_tests/
│   ├── plugin_tests/
│   ├── core_tests/
│   ├── nodegraph_tests/
│   ├── cli_tests/
│   └── ...
├── third_party/
└── docs/
```

### 5.3 CMake 构建目标

```cmake
# ── 基础层 ──
add_library(reflect STATIC ...)   # 反射框架, 所有模块依赖
add_library(core STATIC ...)      # 核心库
add_library(plugin STATIC ...)    # 插件框架 + ServiceLocator + Registry

# ── 插件 SDK (供插件开发者链接) ──
add_library(plugin_sdk INTERFACE)
target_link_libraries(plugin_sdk INTERFACE
    reflect core plugin     # 插件只需依赖接口和反射
)

# ── Headless Core ──
add_library(headless_core STATIC)
target_link_libraries(headless_core PUBLIC
    platform reflect core plugin
    rhi nodegraph simulation pcg asset renderer
)

# ── 内置插件 (每个编译为独立 .so/.dll) ──
startnode_add_plugin(geo_basic      plugins/geo_basic/*.cpp)
startnode_add_plugin(sim_fluid      plugins/sim_fluid/*.cpp)
startnode_add_plugin(sim_smoke      plugins/sim_smoke/*.cpp)
startnode_add_plugin(sim_cloth      plugins/sim_cloth/*.cpp)
startnode_add_plugin(pcg_tools      plugins/pcg_tools/*.cpp)
startnode_add_plugin(asset_gltf     plugins/asset_gltf/*.cpp)
startnode_add_plugin(asset_obj      plugins/asset_obj/*.cpp)
# ... 更多插件

# ── GUI 可执行文件 ──
add_executable(startnode src/app/main.cpp)
target_link_libraries(startnode PRIVATE headless_core ui)

# ── CLI 可执行文件 ──
add_executable(startnode-cli src/cli/main.cpp ...)
target_link_libraries(startnode-cli PRIVATE headless_core)

# ── 构建开关 ──
option(SN_BUILD_GUI     "Build GUI editor" ON)
option(SN_BUILD_CLI     "Build CLI tool" ON)
option(SN_BUILD_PLUGINS "Build builtin plugins" ON)
option(SN_BUILD_TESTS   "Build unit tests" ON)
```

### 5.4 第三方依赖清单

| 库 | 用途 | 集成方式 | GUI/CLI |
|----|------|----------|---------|
| Vulkan SDK | GPU API | find_package | 共用 |
| VMA | Vulkan 内存管理 | FetchContent | 共用 |
| GLFW | 窗口管理 | FetchContent | GUI only |
| ImGui | UI 框架 | FetchContent | GUI only |
| ImRefl | UI 反射 | 本地 (header-only) | GUI only |
| ImNodes / imnodes | 节点编辑器 UI | FetchContent | GUI only |
| glm | 数学库 | FetchContent | 共用 |
| glslang / shaderc | Shader 编译 | find_package (Vulkan SDK) | 共用 |
| CLI11 | 命令行参数解析 | FetchContent | CLI only |
| tinygltf | glTF 导入导出 | FetchContent | 共用 |
| ufbx | FBX 导入 | FetchContent | 共用 |
| tinyobjloader | OBJ 导入 | FetchContent | 共用 |
| OpenUSD | USD 支持 | find_package | 共用 |
| Alembic | 几何缓存 | find_package | 共用 |
| OpenVDB | 体积数据 | find_package | 共用 |
| stb_image | 图片加载 | FetchContent | 共用 |
| spdlog | 日志 | FetchContent | 共用 |
| nlohmann/json | JSON 序列化 | FetchContent | 共用 |
| Catch2 / GoogleTest | 单元测试 | FetchContent | 测试 |

---

## 6. 分阶段实施路线

### Phase 0 — 基础骨架 + 反射 + 插件框架

- [ ] CMake 项目结构搭建（含 `SN_BUILD_GUI` / `SN_BUILD_CLI` / `SN_BUILD_PLUGINS` 开关）
- [ ] `reflect`: 核心反射工具（TypeRegistry, 自动序列化, Diff, Format）
- [ ] `core`: 日志（反射自动格式化）、数学类型、UUID、EventBus
- [ ] `plugin`: PluginManager、ServiceLocator、Registry 模板、PluginSDK
- [ ] `platform`: GLFW 窗口 + 输入事件 + Headless FileSystem/Timer
- [ ] `rhi`: Vulkan 初始化（GUI + Headless 两种模式）、Swapchain、基础 Command Buffer
- [ ] 验证: 能在窗口中清屏并显示 ImGui Demo Window
- [ ] 验证: 一个最小插件能被 PluginManager 加载并注册到 Registry
- [ ] `cli` 骨架: 参数解析 + `info --gpu` 子命令

### Phase 1 — 渲染基础

- [ ] `rhi`: Buffer/Texture/Pipeline 抽象完善
- [ ] `renderer`: PBR 网格渲染、无限网格、Gizmo（作为 RenderPass 注册）
- [ ] 3D Viewport 嵌入 ImGui
- [ ] `renderer`: 离屏渲染路径（Headless Framebuffer）

### Phase 2 — 节点图系统

- [ ] `nodegraph`: Node/Port/Connection 数据模型 + INode 接口
- [ ] 求值引擎（Pull 模式 + 脏标记 + EventBus 通知）
- [ ] `.sn` 场景文件反射自动序列化/反序列化
- [ ] `ui`: 节点编辑器画布、属性面板 (ImRefl 反射自动生成)
- [ ] CommandHistory (Undo/Redo) + 反射自动 diff
- [ ] `plugin_geo_basic`: Box, Sphere, Grid, Transform, Merge (首个内置插件)
- [ ] `cli`: `cook` / `inspect` / `validate` 子命令

### Phase 3 — 仿真 MVP

- [ ] `simulation`: Compute Shader 管线框架 + ISolver 接口
- [ ] `plugin_sim_fluid`: 流体仿真 (FLIP, Strategy 可切换 SPH)
- [ ] `plugin_sim_smoke`: 烟雾仿真 (基础欧拉求解器)
- [ ] 体积渲染 (Ray Marching, 注册为 RenderPass 插件)
- [ ] `cli`: `simulate` 子命令

### Phase 4 — 完善仿真 + PCG

- [ ] `plugin_sim_cloth`: 布料仿真 (PBD/XPBD)
- [ ] `plugin_pcg_tools`: Scatter, Copy to Points, Noise, L-System
- [ ] 碰撞系统（SDF 碰撞体）

### Phase 5 — 资产管线 (每种格式一个插件)

- [ ] `plugin_asset_gltf` / `plugin_asset_obj`: 导入导出
- [ ] `plugin_asset_fbx` / `plugin_asset_usd`: 导入
- [ ] `plugin_asset_alembic` / `plugin_asset_vdb`: 导入导出
- [ ] 缓存系统
- [ ] `cli`: `convert` 子命令

### Phase 6 — 打磨与优化

- [ ] 多 Viewport
- [ ] 插件热重载 (开发期)
- [ ] 节点预设/模板保存
- [ ] `cli`: `render` 子命令
- [ ] 性能分析工具
- [ ] 第三方插件开发文档 + 示例插件模板

---

## 7. 关键技术风险

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| C++26 反射编译器支持有限 | Homebrew Clang 22 缺 `<meta>`,实际只能用 GCC 16 | 主编译器定为 GCC 16(`-freflection`);反射工具集中在 `reflect`/`meta_core`,编译器升级时仅改一处。**已实测** GCC 16.1 可编译 P2996 全套用法 |
| GCC 16 反射 API 与 C++26 草案有出入 | 草案代码需适配(如 `offset_of` 返回 `member_offset`、`nonstatic_data_members_of` 需 `access_context`、注解用 `[[= ...]]`、`info*` 污染 consteval-only 类型) | 已在 Phase 0 全部解决并固化:`FTypeDescriptor` 用 `char[64]` 定长名(满足 `define_static_array` 结构化要求)、`FAnnotationSet` consteval-only、运行时只持有预解析数据 |
| 反射编译时间长 | 大量模板实例化拖慢构建 | 显式模板实例化 + precompiled header；插件独立编译隔离 |
| 插件 ABI 稳定性 | 插件/主程序版本不匹配导致崩溃 | 定义 `SN_PLUGIN_ABI_VERSION`，加载时检查；核心接口使用 C ABI 或 vtable |
| 插件热重载复杂度 | dlclose 后指针悬空 | 开发期 only；生产环境重启加载；引用计数防悬空 |
| macOS MoltenVK 功能子集 | 部分 Vulkan 特性不可用 | 避免使用 MoltenVK 不支持的扩展，CI 持续验证 |
| GPU Compute 调试困难 | 仿真 Bug 难以定位 | 提供 CPU fallback 参考实现用于验证；RenderDoc 集成 |
| 大规模仿真内存压力 | GPU 显存不足 | 支持 LOD、分块计算、异步 readback 到系统内存 |
| 第三方库兼容性 | OpenUSD 等构建复杂 | 各格式作为独立插件，可选编译，不影响核心 |

---

## 8. 编码规范

> 本节为权威约定,覆盖早期版本中的 `snake_case`/`sn::` 规范。所有 StartNode 代码(含 Phase 0 已实现的 `reflect`/`meta_core` 模块)均遵循以下风格。ImRefl 作为既有 UI 库保持其自身风格不变。

**类型前缀(UE 风格)**:
- `struct` → `F` 前缀,例:`FTypeDescriptor`、`FMemberDescriptor`、`FTypeId`
- `class` → `U` 前缀,例:`UTypeRegistry`、`URegistry<Interface>`
- 接口 → `I` 前缀,例:`INode`、`ISolver`、`IAssetImporter`
- `enum`/`enum class` → `E` 前缀,例:`EPortDataType`、`EType`

**命名空间**: `StartNode::<Module>`(去 `sn::` 前缀)。例:`StartNode::Reflect`、`StartNode::Meta`、`StartNode::Anno`、`StartNode::Core`。

**物理目录**: 去 `sn_` 前缀。模块目录为 `reflect/`、`meta_core/`、`core/`、`plugin/` 等(非 `sn_reflect/`)。头文件路径 `StartNode/Reflect/TypeRegistry.hpp`。

**函数与变量**: 小驼峰 `camelCase`(非下划线)。例:`registerType`、`makeTypeId`、`formatReflected`、`toJson`、`typeId`、`memberName`。
- `Id` 接受为缩写(`typeId`、`nodeId`),其余不缩写(`Descriptor` 不写成 `Desc`)。
- UE 类型前缀(大写字母)与 camelCase 函数/变量共存,这是指定的组合,严格遵守。

**其他沿用 UE 惯例**:
- 公开 API 头文件放在 `include/StartNode/<Module>/`,私有实现放在 `src/`
- 模块内部用 `namespace StartNode::<module_name>`
- 所有 GPU 资源创建/销毁通过 RHI 抽象,禁止直接调用 `vkCreate*`
- Shader 文件使用 `.vert` / `.frag` / `.comp` 扩展名
- 使用 `clang-format` 统一格式(附项目 `.clang-format` 配置)
- **反射优先**: 新增数据类型时优先考虑反射自动序列化/UI 生成,避免手写样板
- **接口优先**: 模块间通信必须通过接口(`I*`),禁止直接 `#include` 其他模块的内部实现头文件
- **插件友好**: 任何新增的节点类型/求解器/资产格式都应作为插件实现,即使是内置功能

---

## 附录 A: 术语表

| 术语 | 含义 |
|------|------|
| RHI | Render Hardware Interface，渲染硬件抽象层 |
| SDF | Signed Distance Field，有符号距离场 |
| PBD | Position Based Dynamics，基于位置的动力学 |
| FLIP | Fluid Implicit Particle，流体隐式粒子法 |
| APIC | Affine Particle-In-Cell，仿射粒子元胞法 |
| SPH | Smoothed Particle Hydrodynamics，光滑粒子流体力学 |
| MAC Grid | Marker-and-Cell Grid，标记与元胞网格 |
| PCG | Procedural Content Generation，程序化内容生成 |
| CSM | Cascaded Shadow Maps，级联阴影 |
| PSO | Pipeline State Object，管线状态对象 |
| DCC | Digital Content Creation，数字内容创建工具 |
