---
name: ABP_PVP_FP_Locomotion 动画蓝图落地
overview: 按新的逻辑资产布局，将动画蓝图生成路径改为 /Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion，并同步更新 pvp_logic_layout.py 的期望资产与计数、文档和验证，仍不修改角色接线。
todos:
  - id: editor-builder
    content: 用 [subagent:code-explorer] 核对 API 后新增 PVPAnimBlueprintBuilder 并补齐 Build.cs 依赖
    status: completed
  - id: python-script-layout
    content: 用 [skill:UE Python Scripting] 新增 setup_pvp_anim_blueprint.py，并同步 pvp_logic_layout 资产与计数
    status: completed
  - id: build-generate
    content: 编译 PVPDemoEditor 并运行 commandlet 生成 ABP，修复图与编译错误
    status: completed
    dependencies:
      - editor-builder
      - python-script-layout
  - id: verify-graph-layout
    content: 用 [skill:UE5 蓝图自动化] 读回校验四状态、过渡、资源引用与布局计数
    status: completed
    dependencies:
      - build-generate
  - id: docs-sync
    content: 同步 Animation 三件套、Content/Source Structure、Character 与 AGENTS 索引
    status: completed
    dependencies:
      - verify-graph-layout
---

## 产品概述

在项目新的逻辑资产布局下新增第一人称动画蓝图 `ABP_PVP_FP_Locomotion`，父类为 `UPVPAnimInstanceBase`，生成路径修正为 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion`。动画图使用现有 Idle / Walk / Run / Jump 资产输出完整地面与空中姿态，并实现状态过渡；本轮只创建资产，不修改角色接线。

## 核心功能

- 新建动画蓝图资产，骨骼使用 `SKEL_PVP_Mannequin`，预览网格使用 `SKM_PVP_FP_Manny_Simple`，父类为 `UPVPAnimInstanceBase`。
- Ground 状态同时覆盖 Idle / Walk / 左右平移与 Run：`BS_PVP_FP_AR_Locomotion_Standing` 输出走/待机姿态，`A_PVP_FP_AR_Run_F_Loop` 输出奔跑姿态，通过 `bIsRunning AND bIsMoving` 驱动 `BlendListByBool` 平滑混合。
- 跳跃拆分为 `JumpStart`、`Falling`、`Land` 三个状态，分别播放 `A_PVP_FP_AR_Jump_Start`、`A_PVP_FP_AR_Jump_Falling_Loop`、`A_PVP_FP_AR_Jump_End`。
- 过渡规则：Ground 进入起跳用 `bIsJumping`，Ground 直接下落用 `bIsFalling`，JumpStart 进入 Falling 用 `bIsFalling`，Falling 进入 Land 用 `bIsOnGround`，Land 在 Jump_End 播完后自动回 Ground。
- 地面混合空间按自身轴配置自动连接 `Speed` 与 `Direction`，保证前进、后退、左右平移与行走速度正确；若为 1D 只连接 `Speed`。
- 生成过程可重复执行：校验目标资产、骨骼与动画依赖，创建、编译、读回验证并输出报告。
- 同步新的逻辑资产布局：在 `pvp_logic_layout.py` 中登记新 ABP，`Animation/AnimBP` 数量 3 增至 4，`Content/PVP` 总数 77 增至 78，并更新文档计数。
- 不修改 `APVPBaseCharacter`、`BP_PVPBaseCharacter` 与现有 `ABP_PVP_FP_BaseCharacter`；新动画蓝图处于“资产可用、未接线”状态。

## Tech Stack

- 引擎与语言：Unreal Engine 5.9，C++ 编辑器期代码，Python 编辑器脚本。
- 运行时动画实例：现有 `UPVPAnimInstanceBase`（`Source/PVPDemo/Animation/PVPAnimInstanceBase.h/.cpp`）。
- 编辑器构建器：在 `Source/PVPDemoEditor/` 新增 `UPVPAnimBlueprintBuilder`（`UBlueprintFunctionLibrary`）。
- 资产创建：`UAnimBlueprintFactory` 与 `FGraphNodeCreator` 构建 AnimGraph、StateMachine、State、Transition、BlendSpacePlayer、SequencePlayer、BlendListByBool。
- 调用方式：`UnrealEditor-Cmd.exe ... -run=PythonScript -Script=.../setup_pvp_anim_blueprint.py -unattended -nosplash -nullrhi`。
- 依赖变更：`PVPDemoEditor.Build.cs` 新增 `AnimGraph`、`AnimGraphRuntime`、`BlueprintGraph`、`Kismet`；运行时 `PVPDemo.Build.cs` 不变。

## Implementation Approach

`ue5-automation` bridge 未连接，且 AnimBlueprint 写入通道未开放；纯 Python 无法调用未反射的 `UEdGraph::AddNode` / `CreateNode`。因此采用项目已有的“编辑器期 C++ 库 + Python commandlet”模式：Python 负责资产加载、删除旧目标资产、调用构建器、保存与报告；C++ 负责创建 AnimBlueprint 资产并搭建完整 AnimGraph。

### 资产与状态机设计

目标资产：`/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion`。

| 状态 | 内容 | 说明 |
| --- | --- | --- |
| `Ground` | `BS_PVP_FP_AR_Locomotion_Standing` 与 `A_PVP_FP_AR_Run_F_Loop`，经 `BlendListByBool` 混合 | bool 为 `bIsRunning AND bIsMoving`；BlendSpace 的 X 接 `Speed`，2D 时 Y 接 `Direction` |
| `JumpStart` | `A_PVP_FP_AR_Jump_Start` | 单次播放，不循环 |
| `Falling` | `A_PVP_FP_AR_Jump_Falling_Loop` | 循环播放 |
| `Land` | `A_PVP_FP_AR_Jump_End` | 单次播放，播完自动回 Ground |


| 源状态 | 目标状态 | 规则 | 交叉淡入淡出 |
| --- | --- | --- | --- |
| Entry | Ground | 直接连接 | 无 |
| Ground | JumpStart | `bIsJumping` | 0.10s |
| Ground | Falling | `bIsFalling` | 0.10s |
| JumpStart | Falling | `bIsFalling` | 0.10s |
| JumpStart | Land | `bIsOnGround` | 0.08s |
| Falling | Land | `bIsOnGround` | 0.10s |
| Land | Ground | `bAutomaticRuleBasedOnSequencePlayerInState`，按 `Jump_End` 剩余时间触发 | 0.20s |


### 图构建流程

1. 校验父类为 `UAnimInstance` 子类、骨骼与动画资产有效，且所有动画使用 `SKEL_PVP_Mannequin`。
2. 通过 `UAnimBlueprintFactory` 创建 AnimBlueprint，设置 `ParentClass`、`TargetSkeleton`、`PreviewSkeletalMesh`。
3. 在 AnimGraph 中找到 `UAnimGraphNode_Root`，创建 `UAnimGraphNode_StateMachine` 并连接 Pose 输出到 Root。
4. 创建四个 `UAnimStateNode`，使用 `FBlueprintEditorUtils::RenameGraphWithSuggestion` 命名为 `Ground` / `JumpStart` / `Falling` / `Land`。
5. Ground 状态内创建 BlendSpacePlayer、Run SequencePlayer、BlendListByBool 与 `BooleanAND(bIsRunning, bIsMoving)`，连接 Pose 与 `bActiveValue`，输出到 StateResult。
6. 其余状态内创建对应 SequencePlayer，连接 Pose 到 StateResult；按资产循环设置播放。
7. 创建状态转移节点，调用 `UAnimStateTransitionNode::CreateConnections`，设置交叉淡入淡出与规则；规则图使用 `UK2Node_VariableGet` 读取变量，必要时用 `UK2Node_CallFunction` 调用 `UKismetMathLibrary` 布尔函数。
8. Land 到 Ground 的转移启用自动规则，绑定 `Jump_End` 播放剩余时间，转移图不连接逻辑，避免与自动规则冲突。
9. 调用 `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified` 与 `FKismetEditorUtilities::CompileBlueprint`，检查 `UBlueprint::Status` 与 `GeneratedClass`。
10. 将状态、过渡、混合空间轴、样本与编译结果写入 `Saved/PVPAnimBlueprintReport/report.json`。

### 新的逻辑资产布局同步

- `pvp_logic_layout.py` 是逻辑资产布局唯一事实来源：`EXPECTED_ASSETS['Animation/AnimBP']` 新增 `ABP_PVP_FP_Locomotion`；`EXPECTED_COUNTS['Animation/AnimBP']` 改为 4；`EXPECTED_TOTAL` 改为 78。
- `verify_pvp_asset_layout.py` 无需改逻辑，但其 `layout.Animation/AnimBP.assets`、`.count` 与 `layout.total` 必须通过。
- 新 ABP 必须放在 `Animation/AnimBP/`，不得放回 `Characters/` 或 `Actors/`；`ABP_` 前缀路由已由布局脚本保证。

### 实现注意事项

- Ground 内必须处理“按住 Shift 但静止”的情况：使用 `bIsRunning AND bIsMoving`，避免静止时播放 Run 循环。
- `BS_PVP_FP_AR_Locomotion_Standing` 不含 Run 样本，不能仅靠 `Speed` 驱动跑步；必须保留 Run SequencePlayer 混合分支。
- 混合空间轴顺序在构建时读取 `UBlendSpace::GetBlendParameter(0/1)` 与样本范围：优先使用显示名，空时以约 0 至 300 判定 `Speed`、约 -180 至 180 判定 `Direction`；1D 只接 `Speed`。
- 状态转移规则图使用 `bCanEnterTransition` 引脚；自动过渡的转移图不连接逻辑。
- Land 状态只放一个 `Jump_End` SequencePlayer，确保自动规则能找到对应资产播放器。
- 脚本重复执行时只删除并重建 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion`，不触碰其他资产；失败时不保存半成品并返回非零退出码。
- `-nullrhi` commandlet 下不启动 PIE；本轮验证以资产结构读回、编译成功、布局校验与报告为准，Persona 预览由用户手动确认。
- 不修改 `APVPBaseCharacter`、`BP_PVPBaseCharacter` 与现有 ABP；文档明确记录“未接线”。

## Architecture Design

```mermaid
graph LR
    A[setup_pvp_anim_blueprint.py] --> B[UPVPAnimBlueprintBuilder]
    B --> C[UAnimBlueprintFactory]
    B --> D[AnimGraph 状态机与过渡]
    B --> E[Ground: BlendSpace 与 Run BlendListByBool]
    B --> F[JumpStart / Falling / Land]
    D --> G[FKismetEditorUtilities 编译]
    C --> H[/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion]
    G --> I[Saved/PVPAnimBlueprintReport]
    A --> J[资产读回与布局校验]
```

数据流：`UPVPAnimInstanceBase` 的 `Speed / Direction / bIsRunning / bIsMoving / bIsJumping / bIsFalling / bIsOnGround` 只读驱动 AnimGraph；动画层不写回角色状态。

## Directory Structure

```text
Source/PVPDemoEditor/
├── PVPDemoEditor.Build.cs                         # [MODIFY] 新增 AnimGraph / AnimGraphRuntime / BlueprintGraph / Kismet
├── PVPAnimBlueprintBuilder.h                      # [NEW] UBlueprintFunctionLibrary 声明与构建入口
└── PVPAnimBlueprintBuilder.cpp                    # [NEW] 创建 ABP、搭建状态机/过渡/规则、编译并写报告

Content/Python/
├── setup_pvp_anim_blueprint.py                    # [NEW] 加载资产、删除旧目标 ABP、调用构建器、保存、校验并写报告
└── pvp_logic_layout.py                            # [MODIFY] AnimBP 新增 ABP_PVP_FP_Locomotion，计数 4，总数 78

Content/PVP/Animation/AnimBP/
└── ABP_PVP_FP_Locomotion.uasset                   # [NEW] 由编辑器 commandlet 产出，不手写

Docs/
├── Animation/README.md                            # [MODIFY] 新增 ABP 状态/过渡权威表与未接线状态
├── Animation/Animation-Dev.md                     # [MODIFY] 新增构建器 SOP、图结构、commandlet 踩坑与验收
├── Animation/Animation-Integration.md             # [MODIFY] 新增编辑器依赖、资产契约与变更影响面
├── Source-Structure.md                            # [MODIFY] PVPDemoEditor 新库与依赖结论
├── Content-Structure.md                           # [MODIFY] 逻辑资产 77→78、Animation 14→15、AnimBP 3→4、脚本登记
├── Character/README.md                            # [MODIFY] 新 ABP 已创建但未接线
└── AGENTS.md                                      # [MODIFY] 脚本与报告地图、Animation 模块现状
```

不修改：`Source/PVPDemo/Character/PVPBaseCharacter.*`、`Source/PVPDemo/Animation/PVPAnimInstanceBase.*`、`Content/PVP/Characters/BP_PVPBaseCharacter.uasset`、`Content/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter.uasset`。

## Key Code Structures

```cpp
UFUNCTION(BlueprintCallable, Category = "PVP|Animation")
static UAnimBlueprint* BuildLocomotionAnimBlueprint(
    const FString& AssetPath,
    TSubclassOf<UAnimInstance> ParentClass,
    USkeleton* Skeleton,
    USkeletalMesh* PreviewMesh,
    UBlendSpace* GroundBlendSpace,
    UAnimSequence* RunLoop,
    UAnimSequence* JumpStart,
    UAnimSequence* JumpFallingLoop,
    UAnimSequence* JumpEnd,
    const FString& ReportPath);
```

验证入口：

- `Build.bat PVPDemoEditor Win64 Development -Project=...`
- `UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript -Script="<project>/Content/Python/setup_pvp_anim_blueprint.py" -unattended -nosplash -nullrhi`
- `python Content/Python/verify_pvp_asset_layout.py`

## Skill

- **UE Python Scripting**
- Purpose: 编写 `Content/Python/setup_pvp_anim_blueprint.py`，完成资产加载、旧资产清理、构建器调用、保存、结构断言与报告输出。
- Expected outcome: 脚本可在 `UnrealEditor-Cmd` 下重复运行，生成 `ABP_PVP_FP_Locomotion` 并产出 `Saved/PVPAnimBlueprintReport/report.json` 与 `report.md`。
- **UE5 蓝图自动化**
- Purpose: 复用其“先读后写、编译诊断、读回验证、verified 才通过”的验证纪律；由于 bridge 未连接且 AnimBP 写入未开放，实际写入走 C++ 构建器，验证走 Python/C++ 读回。
- Expected outcome: 资产结构、状态转移、编译状态均有读回证据，不以“创建成功”代替“图正确”。

## SubAgent

- **code-explorer**
- Purpose: 实现前定点核对 UE 5.9 编辑器 API 的准确头文件、节点类与引脚名，避免凭记忆写错。
- Expected outcome: 构建器使用的类、函数签名、引脚名与引擎源码一致，首次编译通过率最大化。