---
name: PVPDemo 依赖补全与架构目录创建计划
overview: 范围缩减：仅补全 PVPDemo 工程所需的插件与 Build.cs 模块依赖，并按技术方案创建架构目录空文件夹；不实现任何 C++ 类、GAS 基础或蓝图资产。
todos:
  - id: complete-dependencies
    content: 使用 [skill:UE5架构规划师] 补全 .uproject 插件与 Build.cs 模块依赖
    status: completed
  - id: create-architecture-folders
    content: 创建 8 个架构空文件夹，仅建目录不建文件
    status: completed
    dependencies:
      - complete-dependencies
---

## 用户要求

本次严格缩减范围，只做两件事：**补全依赖** + **列出（创建）架构空文件夹**。此前覆盖 C++ 类、GAS 逻辑、蓝图资产、Config 设置的宽范围计划全部作废。

## 产品概述

对现有 UE5.9 工程 `PVPDemo` 做工程准备：启用 FPS 3C 技术方案所需的引擎插件与 C++ 模块依赖，并按方案目录结构建立分层架构空目录，为后续代码与资产落地预留位置。纯工程配置调整，无界面视觉变更。

## 核心功能

- **插件启用**：在 `PVPDemo.uproject` 新增 `GameplayAbilities`、`AnimationWarping`、`AnimationBudgetAllocator`（均 `Enabled: true`），保留已有 `ModelingToolsEditorMode`。Enhanced Input 已在 `DefaultInput.ini` 生效，不重复启用；Pose Search / Chooser / Modular Gameplay 等按需项不启用。
- **模块依赖**：在 `PVPDemo.Build.cs` 的 `PublicDependencyModuleNames` 补充 `GameplayAbilities`、`GameplayTags`、`GameplayTasks`、`AnimGraphRuntime`、`Niagara`；按需模块不添加。
- **架构空目录**：创建 `Character`、`Camera`（含 `Modes`）、`Animation`、`Equipment`、`Input`、`GAS` 及 `Content/PVPDemo` 共 8 个空文件夹，仅建目录、不放任何文件。

## 范围边界（明确排除）

- 不创建任何 `.h` / `.cpp` 类文件，不写占位代码
- 不搭建 GAS 逻辑（ASC / PlayerState / AbilitySystemGlobals / ActorInfo 修正）
- 不创建蓝图、DataAsset、AnimBP、InputAction、GameplayCue 等资产
- 不修改 `Config/*.ini`（碰撞通道、Near Clip Plane、多线程动画、AbilitySystemGlobalsClassName 留待后续）
- 不做资产核查、不动现有 `InfimaGames/TacticalFPSAnimations` 包、不做联机验收

## 技术栈

- 引擎：Unreal Engine 5.9（模块 `PVPDemo`，`PCHUsage = UseExplicitOrSharedPCHs`）
- 配置载体：`PVPDemo.uproject`（JSON 插件声明）、`Source/PVPDemo/PVPDemo.Build.cs`（C# 模块规则）
- 方案基线：FPS 3C 技术方案 HTML 阶段 0 的依赖部分（文档面向 5.8，命名按工程实际 `PVPDemo` 改写）

## 实现方案

沿用工程现有配置结构与风格，做最小化增量修改：

1. **插件声明**：在 `.uproject` 的 `Plugins` 数组追加三个对象，字段为 `Name` + `Enabled: true`；`GameplayAbilities` / `AnimationWarping` / `AnimationBudgetAllocator` 不设 `TargetAllowList`（需全目标生效，与仅 Editor 的 `ModelingToolsEditorMode` 区分）。保持 JSON 合法、缩进与现有风格一致。
2. **模块依赖**：在 `PublicDependencyModuleNames.AddRange` 现有数组末尾追加五个模块名字符串，以逗号分隔；`PrivateDependencyModuleNames` 保持为空；不改动 `PCHUsage` 与目标文件。
3. **目录创建**：按方案分层结构创建 8 个空目录，与 `Source/PVPDemo` 现有 `PVPDemo.cpp/.h` 平级；目录创建需幂等（已存在则跳过），不生成 `.gitkeep` 或任何占位文件。

## 实施说明（执行细节）

- **依赖先于目录**：先完成插件与模块依赖修改再建目录，顺序对齐用户"先补全依赖，然后列目录"的要求。
- **命名规范**：全部使用 `PVPDemo` 工程命名，不引入文档的 `FPSGame` / `FPSGAME_API`，避免后续命名混用。
- **兼容性**：`GameplayAbilities` 在 5.9 中为引擎自带插件，无需外部下载；`AnimationBudgetAllocator` 启用后不影响运行时逻辑，工程当前无相关代码。
- **幂等与校验**：修改前确认目标字符串尚未存在，避免重复条目；完成后回读 `.uproject` 与 `Build.cs` 确认 JSON/C# 语法正确，目录清单逐项核对。
- **影响面控制**：不触碰 `Config`、`Content/InfimaGames` 与现有模板代码；不生成任何会被 UBT 编译的源文件，保证改动后工程仍可正常生成与编译。

## 目录结构

```
d:/UE/Unreal Projects/PVPDemo/
├── PVPDemo.uproject                         # [MODIFY] Plugins 数组新增 GameplayAbilities / AnimationWarping / AnimationBudgetAllocator（Enabled=true），保留 ModelingToolsEditorMode
├── Source/PVPDemo/
│   ├── PVPDemo.Build.cs                     # [MODIFY] Public 依赖追加 GameplayAbilities / GameplayTags / GameplayTasks / AnimGraphRuntime / Niagara；保持其余不变
│   ├── Character/                           # [NEW-目录] 角色层空目录（双网格角色、CMC、视角组件、相机接口）
│   ├── Camera/                              # [NEW-目录] 相机层空目录（相机组件、模式栈、相机管理器）
│   │   └── Modes/                           # [NEW-目录] 相机模式空目录（第一人称/ADS/第三人称/死亡）
│   ├── Animation/                           # [NEW-目录] 动画层空目录（AnimInstance、武器动画层、程序化动画）
│   ├── Equipment/                           # [NEW-目录] 装备层空目录（武器定义、武器 Actor、装备组件）
│   ├── Input/                               # [NEW-目录] 输入层空目录（Tag 与 InputAction 配置）
│   └── GAS/                                 # [NEW-目录] GAS 层空目录（Globals、Montage 任务、GameplayTag）
└── Content/PVPDemo/                         # [NEW-目录] 工程自有资产空目录，与第三方 InfimaGames 包分离
```

## 空文件夹清单（共 8 项，仅目录）

```
Source/PVPDemo/Character/
Source/PVPDemo/Camera/
Source/PVPDemo/Camera/Modes/
Source/PVPDemo/Animation/
Source/PVPDemo/Equipment/
Source/PVPDemo/Input/
Source/PVPDemo/GAS/
Content/PVPDemo/
```

## Agent Extensions

### Skill

- **UE5架构规划师**
- Purpose: 依据 UE5 模块边界与 Build.cs 依赖管理规范，核对本次插件与模块依赖清单的正确性（模块名拼写、Public/Private 归属、是否存在循环依赖风险），并确认命名与现有 `PVPDemo` 工程风格一致。
- Expected outcome: 产出经校验的依赖清单与最小增量修改方案，确保 `.uproject` 插件声明与 `PVPDemo.Build.cs` 依赖补全后语法合法、可被 UBT 正确解析，且无需额外外部依赖。