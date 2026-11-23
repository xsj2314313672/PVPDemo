---
name: ABP_PVP_FP_Locomotion 动画蓝图落地
overview: 用编辑器期 C++ 构建器 + Python commandlet 在 /Game/PVP/Characters/ 生成 ABP_PVP_FP_Locomotion（父类 UPVPAnimInstanceBase），搭建 Ground/Run 混合 + JumpStart/Falling/Land 四状态跳跃机，编译校验并同步文档，不修改角色接线。
todos:
  - id: extend-editor-builder
    content: 用 [subagent:code-explorer] 核对 UE 5.9 API 后新增 PVPAnimBlueprintBuilder，并在 Build.cs 补齐动画图依赖
    status: pending
  - id: python-setup-script
    content: 用 [skill:UE Python Scripting] 新增 setup_pvp_anim_blueprint.py，加载资产、调用构建器、保存并输出报告
    status: pending
  - id: generate-and-compile
    content: 编译 PVPDemoEditor 并运行 commandlet，生成 ABP_PVP_FP_Locomotion，修复图或编译错误
    status: pending
    dependencies:
      - extend-editor-builder
      - python-setup-script
  - id: verify-graph
    content: 用 [skill:UE5 蓝图自动化] 的读回与编译验证纪律核验四状态、过渡规则、资源引用与报告
    status: pending
    dependencies:
      - generate-and-compile
  - id: docs-sync
    content: 同步 Animation 三件套、Source-Structure、Content-Structure、Character README 与 AGENTS 索引
    status: pending
    dependencies:
      - verify-graph
---

## 产品概述

新增一个可用的第一人称动画蓝图资产 `ABP_PVP_FP_Locomotion`，父类为 `UPVPAnimInstanceBase`，使用现有 Idle / Walk / Run / Jump 动画输出完整的地面与空中姿态，并通过状态机与混合节点实现正常过渡。本轮只创建动画蓝图资产，不修改角色蓝图与现有动画蓝图。

## 核心功能

- 新建动画蓝图资产，骨骼使用 `SKEL_PVP_Mannequin`，预览网格使用 `SKM_PVP_FP_Manny_Simple`，父类为 `UPVPAnimInstanceBase`。
- 地面状态同时覆盖 Idle / Walk / 左右平移与 Run：以 `BS_PVP_FP_AR_Locomotion_Standing` 输出走/待机姿态，用 `A_PVP_FP_AR_Run_F_Loop` 输出奔跑姿态，通过 `bIsRunning AND bIsMoving` 在两者之间平滑混合。
- 跳跃状态拆分为 `JumpStart`、`Falling`、`Land` 三个状态，分别播放 `A_PVP_FP_AR_Jump_Start`、`A_PVP_FP_AR_Jump_Falling_Loop`、`A_PVP_FP_AR_Jump_End`。
- 状态过渡规则：地面进入起跳使用 `bIsJumping`，地面直接下落使用 `bIsFalling`，起跳进入下落使用 `bIsFalling`，下落进入落地使用 `bIsOnGround`，落地动作播完后自动回到地面状态。
- 地面混合空间根据自身轴配置自动连接 `Speed` 与 `Direction`，保证前进、后退、左右平移与行走速度正确。
- 生成过程可重复执行，先校验目标资产、骨骼与动画依赖，再创建、编译、读回验证并输出报告。
- 不修改 `APVPBaseCharacter`、`BP_PVPBaseCharacter` 与现有 `ABP_PVP_FP_BaseCharacter`；新动画蓝图处于“资产可用、未接线”状态。
- 同步动画模块文档、资产清单、脚本索引与依赖说明，记录状态机结构、过渡规则、验证方式与后续接线步骤。

## 技术栈

- 引擎与语言：Unreal Engine 5.9，C++ 编辑器期代码，Python 编辑器脚本。
- 运行时动画实例：现有 `UPVPAnimInstanceBase`（`Source/PVPDemo/Animation/PVPAnimInstanceBase.h/.cpp`）。
- 编辑器构建器：在 `Source/PVPDemoEditor/` 新增 `UPVPAnimBlueprintBuilder`（`UBlueprintFunctionLibrary`）。
- 资产创建：`UAnimBlueprintFactory` + `FGraphNodeCreator` 构建 AnimGraph、StateMachine、State、Transition、BlendSpacePlayer、SequencePlayer、BlendListByBool。
- 调用方式：`UnrealEditor-Cmd.exe ... -run=PythonScript -Script=.../setup_pvp_anim_blueprint.py -unattended -nosplash -nullrhi`。
- 依赖变更：`PVPDemoEditor.Build.cs` 新增 `AnimGraph`、`AnimGraphRuntime`、`BlueprintGraph`、`Kismet`；运行时 `PVPDemo.Build.cs` 不变。

## 实现方案

`ue5-automation` bridge 未连接，且该工具对 AnimBlueprint 仅支持读取，无法写入动画图节点；纯 Python 也无法调用未反射的 `UEdGraph::AddNode` / `CreateNode`。因此采用项目已有的“编辑器期 C++ 库 + Python commandlet”模式：Python 负责资产加载、删除旧目标资产、调用构建器、保存与报告；C++ 负责创建 AnimBlueprint 资产并搭建完整 AnimGraph。

### 资产与状态机设计

目标资产：`/Game/PVP/Characters/ABP_PVP_FP_Locomotion`。

| 状态 | 内容 | 说明 |
| --- | --- | --- |
| `Ground` | `BS_PVP_FP_AR_Locomotion_Standing` + `A_PVP_FP_AR_Run_F_Loop`，经 `BlendListByBool` 混合 | bool 为 `bIsRunning AND bIsMoving`；BS 的 X 接 `Speed`，若为 2D 则 Y 接 `Direction` |
| `JumpStart` | `A_PVP_FP_AR_Jump_Start` | 单次播放，不循环 |
| `Falling` | `A_PVP_FP_AR_Jump_Falling_Loop` | 循环播放 |
| `Land` | `A_PVP_FP_AR_Jump_End` | 单次播放，播完自动回 Ground |


过渡：

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

1. 校验父类是否为 `UAnimInstance` 子类、骨骼与动画资产是否有效。
2. 通过 `UAnimBlueprintFactory` 创建 AnimBlueprint，设置 `ParentClass`、`TargetSkeleton`、`PreviewSkeletalMesh`。
3. 在 `AnimGraph` 中找到 `UAnimGraphNode_Root`，创建 `UAnimGraphNode_StateMachine` 并连接 Pose 输出到 Root。
4. 创建四个 `UAnimStateNode`，使用 `FBlueprintEditorUtils::RenameGraphWithSuggestion` 将状态命名并置为 `Ground` / `JumpStart` / `Falling` / `Land`。
5. 在 Ground 状态内创建 BlendSpacePlayer、Run SequencePlayer、BlendListByBool 与 `BooleanAND(bIsRunning, bIsMoving)`，连接 Pose 与 `bActiveValue`，输出到 StateResult。
6. 在其余状态内创建对应 SequencePlayer，连接 Pose 到 StateResult；设置循环与播放速率。
7. 创建状态转移节点，调用 `UAnimStateTransitionNode::CreateConnections`，设置交叉淡入淡出与规则；规则图使用 `UK2Node_VariableGet` 读取 `bIsJumping` / `bIsFalling` / `bIsOnGround`，必要时使用 `UK2Node_CallFunction` 调用 `UKismetMathLibrary` 布尔函数。
8. Land 到 Ground 的转移启用自动规则，绑定 `Jump_End` 播放剩余时间。
9. `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified` 后调用 `FKismetEditorUtilities::CompileBlueprint`，检查 `UBlueprint::Status` 与 `GeneratedClass`。
10. 将状态、过渡、混合空间轴、样本、编译结果写入 `Saved/PVPAnimBlueprintReport/report.json`。

### 幂等与错误处理

- 脚本重复执行时只删除并重建目标资产 `/Game/PVP/Characters/ABP_PVP_FP_Locomotion`，不触碰其他资产。
- 资产缺失、骨骼不匹配、父类不合法、图节点创建失败或编译失败时立即报错并返回非零退出码，不保存半成品。
- 构建器对每个节点创建后校验关键引脚是否成功连接；失败时输出节点名与引脚名。
- 所有 `.uasset` 只通过编辑器/脚本产出，不手写二进制资产。

## 实现注意事项

- `Ground` 状态内必须处理“按住 Shift 但静止”的情况：使用 `bIsRunning AND bIsMoving`，避免静止时播放 Run 循环。
- `BS_PVP_FP_AR_Locomotion_Standing` 不含 Run 样本，不能仅靠 `Speed` 驱动跑步；必须保留 Run SequencePlayer 混合分支。
- 混合空间轴顺序在构建时读取 `UBlendSpace::GetBlendParameter(0/1)` 与样本范围判断：约 0～300 的轴接 `Speed`，约 -180～180 的轴接 `Direction`；若为 1D 只接 `Speed`。
- 状态转移规则图使用 `bCanEnterTransition` 引脚；自动过渡的转移图不要连接逻辑，避免与自动规则冲突警告。
- `Land` 状态只放一个 `Jump_End` SequencePlayer，确保自动规则能找到对应资产播放器。
- 所有动画资源均使用 `SKEL_PVP_Mannequin`，构建前断言骨骼一致。
- `-nullrhi` commandlet 下不启动 PIE；本轮验证以资产结构读回、编译成功、报告为准，Persona 预览由用户手动确认。
- 不修改 `BP_PVPBaseCharacter`、`APVPBaseCharacter` 与现有 ABP；文档明确记录“未接线”。

## 架构设计

```mermaid
graph LR
    A[setup_pvp_anim_blueprint.py] --> B[UPVPAnimBlueprintBuilder]
    B --> C[UAnimBlueprintFactory 创建 ABP]
    B --> D[AnimGraph 状态机与过渡]
    B --> E[Ground: BS + Run BlendListByBool]
    B --> F[JumpStart / Falling / Land]
    D --> G[FKismetEditorUtilities 编译]
    C --> H[/Game/PVP/Characters/ABP_PVP_FP_Locomotion]
    G --> I[Saved/PVPAnimBlueprintReport]
    A --> J[资产读回与报告校验]
```

数据流：`UPVPAnimInstanceBase` 的 `Speed / Direction / bIsRunning / bIsMoving / bIsJumping / bIsFalling / bIsOnGround` 只读驱动 AnimGraph；动画层不写回角色状态。

## 目录结构

```text
Source/PVPDemoEditor/
├── PVPDemoEditor.Build.cs                         # [MODIFY] 新增 AnimGraph / AnimGraphRuntime / BlueprintGraph / Kismet 编辑器依赖
├── PVPAnimBlueprintBuilder.h                      # [NEW] UBlueprintFunctionLibrary 声明；暴露 BuildLocomotionAnimBlueprint 与结构校验入口
└── PVPAnimBlueprintBuilder.cpp                    # [NEW] 创建 ABP、搭建状态机/状态/过渡/规则、连接 Pose、编译并写 JSON 报告

Content/Python/
└── setup_pvp_anim_blueprint.py                    # [NEW] 加载骨骼/网格/动画资产，删除旧目标 ABP，调用 C++ 构建器，保存并校验，写 Markdown 报告

Docs/
├── Animation/
│   ├── README.md                                  # [MODIFY] 第 3 节新增 ABP 状态/过渡权威表；第 5 节更新未接线状态；第 6 节补生成与校验入口
│   ├── Animation-Dev.md                           # [MODIFY] 新增构建器 SOP、图结构设计、commandlet 踩坑、验收步骤
│   └── Animation-Integration.md                   # [MODIFY] 新增编辑器依赖、资产契约、状态机接口与变更影响面
├── Source-Structure.md                            # [MODIFY] PVPDemoEditor 新增构建库与依赖结论
├── Content-Structure.md                           # [MODIFY] 逻辑资产数量与 Characters 表新增 ABP；脚本清单新增 setup_pvp_anim_blueprint.py
├── Character/README.md                            # [MODIFY] 说明新 ABP 已创建但角色仍未接线
└── AGENTS.md                                      # [MODIFY] Content/Python 与报告地图、Animation 模块现状更新
```

不修改：`Source/PVPDemo/Character/PVPBaseCharacter.*`、`Source/PVPDemo/Animation/PVPAnimInstanceBase.*`、`Content/PVP/Characters/BP_PVPBaseCharacter.uasset`、`Content/PVP/Actors/ABP_PVP_FP_BaseCharacter.uasset`。

## 关键接口

```cpp
UFUNCTION(BlueprintCallable, Category = "PVP|Animation")
static UAnimBlueprint* BuildLocomotionAnimBlueprint(
    const FString&amp; AssetPath,
    TSubclassOf&lt;UAnimInstance&gt; ParentClass,
    USkeleton* Skeleton,
    USkeletalMesh* PreviewMesh,
    UBlendSpace* GroundBlendSpace,
    UAnimSequence* RunLoop,
    UAnimSequence* JumpStart,
    UAnimSequence* JumpFallingLoop,
    UAnimSequence* JumpEnd,
    const FString&amp; ReportPath);
```

## Agent Extensions

### Skill

- **UE Python Scripting**
- Purpose: 编写 `Content/Python/setup_pvp_anim_blueprint.py`，完成资产加载、旧资产清理、构建器调用、保存、结构断言与报告输出。
- Expected outcome: 脚本可在 `UnrealEditor-Cmd` 下重复运行，生成 `ABP_PVP_FP_Locomotion` 并产出 `Saved/PVPAnimBlueprintReport/report.json` 与 `report.md`。
- **UE5 蓝图自动化**
- Purpose: 复用其“先读后写、编译诊断、读回验证、verified 才通过”的验证纪律；由于 bridge 未连接且 AnimBP 写入未开放，实际写入走 C++ 构建器，验证走 Python/C++ 读回。
- Expected outcome: 资产结构、状态转移、编译状态均有读回证据，不以“创建成功”代替“图正确”。

### SubAgent

- **code-explorer**
- Purpose: 在实现前定点核对 UE 5.9 编辑器 API 的准确头文件、节点类与引脚名，避免凭记忆写错。
- Expected outcome: 构建器使用的类、函数签名、引脚名与引擎源码一致，首次编译通过率最大化。