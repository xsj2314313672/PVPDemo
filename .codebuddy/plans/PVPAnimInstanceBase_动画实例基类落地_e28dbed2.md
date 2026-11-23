---
name: PVPAnimInstanceBase 动画实例基类落地
overview: 在 Source/PVPDemo/Animation/ 新增 UPVPAnimInstanceBase，将 Move / Run / Jump 的 locomotion 状态在 NativeThreadSafeUpdateAnimation 中采集为蓝图可读变量，不改角色与现有动画蓝图接线，并补齐 Animation 模块文档三件套与索引。
todos:
  - id: anim-instance-class
    content: 新增 PVPAnimInstanceBase.h/.cpp，缓存角色与移动组件并输出 Move/Run/Jump 蓝图变量
    status: completed
  - id: animation-docs-sync
    content: 创建 Docs/Animation 三件套并同步 AGENTS、Source-Structure、Content-Structure、README 状态与索引
    status: completed
    dependencies:
      - anim-instance-class
  - id: build-verify
    content: 编译 PVPDemoEditor，验证类可作动画蓝图父类且变量随移动、奔跑、跳跃变化
    status: completed
    dependencies:
      - anim-instance-class
---

## 用户需求

新增一个项目自定义动画实例类 `UPVPAnimInstanceBase`，作为后续动画蓝图的父类；类内保存并逐帧设置角色动画所需变量，当前只覆盖已实现的 Move / Run / Jump 动作。

## 核心功能

- 类初始化时自动获取所属角色与角色移动组件，无需动画蓝图手动绑定。
- 自动采集并输出移动相关变量：当前速度、平面速度、垂直速度、当前最大速度、速度比例、相对角色朝向的方向角、是否正在移动。
- 输出奔跑状态变量：`bIsRunning`，与角色按住 Run 的状态保持一致；动画蓝图可再结合是否移动自行判断。
- 输出跳跃与空中状态变量：是否在地面、是否在空中、是否处于上升跳跃、是否处于下落。
- 所有变量以蓝图只读方式暴露，动画蓝图可直接在 AnimGraph、状态机与混合空间中使用，不允许蓝图写入。
- Look 不纳入动画变量：当前第一人称视角由 Controller 驱动，不参与动画姿态。
- 不修改现有角色类、角色蓝图与第三方动画蓝图；本轮不创建动画蓝图资产，由用户后续在编辑器中创建并选择该 C++ 类作为父类。
- 同步动画模块文档与项目索引，记录变量契约、接线方式与验证步骤。

## Tech Stack

- 引擎与语言：Unreal Engine 5.9，C++，现有运行时模块 `PVPDemo`。
- 基类：`UAnimInstance`，新增 `UPVPAnimInstanceBase`，导出宏 `PVPDEMO_API`，类与文件保持 `PVP` 前缀。
- 依赖：只使用 `Engine` 与已声明的 `AnimGraphRuntime`（用于 `UKismetAnimationLibrary::CalculateDirection`）；`Source/PVPDemo/PVPDemo.Build.cs` **无需修改**。
- 资产：本轮不新增或修改 `.uasset`，不创建动画蓝图；`.uasset` 只能由编辑器或 `Content/Python/` 脚本产出。

## Implementation Approach

采用“游戏线程采样 + 工作线程计算”的线程安全动画实例模式，符合 `Docs/Source-Structure.md` 对动画层基类的规划，也符合 UE 5.9 `NativeUpdateAnimation` / `NativeThreadSafeUpdateAnimation` 的推荐用法。

- `NativeInitializeAnimation`：调用 `Super`，缓存 `APVPBaseCharacter` 与 `UCharacterMovementComponent`；Owner 不是目标角色时只告警一次，变量保持默认值。
- `NativeUpdateAnimation`：在游戏线程采样原始快照，包括 `Velocity`、角色 Yaw、`MaxWalkSpeed`、角色 `RunSpeed`、`bIsRunning`、`IsMovingOnGround()`、`IsFalling()`；不在此阶段做蓝图变量输出。
- `NativeThreadSafeUpdateAnimation`：在工作线程依据快照计算并写入所有蓝图只读变量，避免在动画线程访问 Owner、World 或打印日志。
- 方向角复用 `UKismetAnimationLibrary::CalculateDirection(Velocity, FRotator(0, ActorYaw, 0))`，输出 -180 到 180 度，保证与引擎混合空间约定一致。
- `SpeedRatio` 以角色的 `RunSpeed` 为分母并限制在 0 到 1：走路约 0.5、奔跑约 1.0，避免使用当前 `MaxWalkSpeed` 导致比例恒为 1。
- `bIsMoving` 使用平面速度阈值 1 cm/s 判断；`IsMovingOnGround()` 只表示移动模式，不能代替速度判断。
- 空中状态拆分：`bIsInAir` 表示 `IsFalling()`；`bIsJumping` 为空中且 `SpeedZ` 大于 0；`bIsFalling` 为空中且 `SpeedZ` 小于等于 0。
- 不修改 `APVPBaseCharacter`、`BP_PVPBaseCharacter` 与现有 `ABP_PVP_FP_BaseCharacter`；新类只作为可选父类，待用户在编辑器中创建 ABP 后生效。
- 性能：每个动画实例每帧仅做常数次浮点运算与少量拷贝，无堆分配、无逐帧日志；工作线程计算不占用游戏线程动画更新预算。

## Variable Contract

| 变量 | 类型 | 含义 |
| --- | --- | --- |
| `Velocity` | FVector | 采样时的完整速度 |
| `Speed` | float | 平面速度（`Velocity.Size2D()`） |
| `SpeedZ` | float | 垂直速度（`Velocity.Z`） |
| `MaxSpeed` | float | 当前 `MaxWalkSpeed`，随走跑切换变化 |
| `SpeedRatio` | float | 平面速度相对 `RunSpeed` 的比例，0 到 1 |
| `Direction` | float | 相对角色朝向的速度方向角，-180 到 180 度 |
| `bIsMoving` | bool | 平面速度是否超过 1 cm/s |
| `bIsRunning` | bool | 角色当前是否处于按住 Run 状态 |
| `bIsOnGround` | bool | 是否处于可行走地面模式 |
| `bIsInAir` | bool | 是否处于空中 |
| `bIsJumping` | bool | 空中且垂直速度大于 0 |
| `bIsFalling` | bool | 空中且垂直速度小于等于 0 |


全部变量使用 `UPROPERTY(BlueprintReadOnly, Category="PVP|Animation|Locomotion")`，不提供蓝图写入。缓存指针使用 `TObjectPtr` 并标记 `Transient`，避免 GC 与序列化污染。

## Implementation Notes

- 头文件只前置声明 `APVPBaseCharacter` 与 `UCharacterMovementComponent`，具体包含放在 `.cpp`，遵循现有头文件依赖习惯。
- 类声明加 `UCLASS(Blueprintable)`，确保用户可在编辑器中以该类为父类创建动画蓝图。
- `NativeThreadSafeUpdateAnimation` 中禁止调用 `GetOwningActor()`、`GetWorld()`、`UE_LOG` 等非线程安全接口；所有需要的状态必须在 `NativeUpdateAnimation` 提前采样。
- 若 Owner 不是 `APVPBaseCharacter` 或移动组件缺失，`NativeUpdateAnimation` 重置快照，线程安全更新输出默认值，不崩溃。
- 只在 `NativeInitializeAnimation` 告警一次，避免逐帧日志刷屏；不输出角色敏感信息。
- 依赖方向保持 `Input → Character → Animation`，动画层只读角色状态；`Character/` 不感知 `Animation/`。
- 不新增 `Build.cs` 依赖，不修改 `Config`，不提前实现 Camera 模式栈、GAS、Equipment、蹲伏、瞄准或持枪动画。
- 验证方式：编译 `PVPDemoEditor`；在编辑器中新建动画蓝图并确认父类列表出现 `UPVPAnimInstanceBase`；在 AnimGraph 中确认变量可读取；PIE 中分别测试 WASD、按住 Shift、按 Space，确认速度、方向、奔跑与空中状态变化。
- 文档同步：按 `Docs/AI-Workflow.md` 创建 `Docs/Animation/` 三件套，并同步 `AGENTS.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`、`README.md` 的动画模块状态与索引。

## Architecture Design

- 数据流：`Enhanced Input → APVPBaseCharacter / CharacterMovement → UPVPAnimInstanceBase → 动画蓝图 AnimGraph`。
- 类关系：`UAnimInstance` → `UPVPAnimInstanceBase` → 用户新建动画蓝图；后续 1P/3P 拆分时再派生 `UPVPBodyAnimInstance` / `UPVPArmsAnimInstance`，本轮不提前实现。
- 当前 `APVPBaseCharacter` 仍使用第三方动画蓝图副本，`BP_PVPBaseCharacter` 仍覆盖为 `ABP_PVP_FP_BaseCharacter`；新类处于“可用但未接线”状态，文档必须如实说明。
- 依赖方向：`Animation/` 可只读 `Character/`；`Character/` 不包含动画层头文件；`Game/` 与 `Animation/` 无直接依赖。

## Directory Structure

本次新增动画层基类与动画模块文档，并同步受影响的索引文档；不新增资产、不修改角色与现有动画蓝图。

```text
Source/PVPDemo/
└── Animation/
    ├── PVPAnimInstanceBase.h      # [NEW] UAnimInstance 基类声明。定义蓝图只读 locomotion 变量、三个 Native 生命周期覆写、缓存的角色与移动组件指针，以及采样快照字段。
    └── PVPAnimInstanceBase.cpp    # [NEW] 实现初始化缓存、游戏线程采样、工作线程变量计算；包含 Owner 类型告警、无效指针兜底、方向角与速度比例计算。

Docs/
├── Animation/
│   ├── README.md                  # [NEW] 模块导航：职责边界、文档路由、变量权威表、启动入口、未实现能力、验证与维护规则。
│   ├── Animation-Dev.md           # [NEW] 开发文档：源码结构、线程安全设计决策、新增变量 SOP、踩坑清单（采样/计算分离、线程安全限制、SpeedRatio 分母、ABP 接线）、验证与限制。
│   └── Animation-Integration.md   # [NEW] 接口契约：依赖方向、数据流、UPVPAnimInstanceBase 变量与生命周期接口、其他模块调用方式、禁止依赖、变更影响面。
├── Source-Structure.md            # [MODIFY] 文首当前状态、目录树 Animation 行、2.3 节 PVPAnimInstanceBase 标为已实现并补充变量说明、依赖变更结论。
└── Content-Structure.md           # [MODIFY] 第 5 节末尾源码状态说明加入 Animation 基类，明确本轮不新增资产、ABP 待创建。

AGENTS.md                          # [MODIFY] 项目速览源码状态、预留模块列表移除 Animation、2.1 模块文档目录新增 Animation 行。
README.md                          # [MODIFY] 项目简介补充动画实例基类；AI 协作说明更新已建模块文档为 Input 与 Animation。
```

保持不变：`Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp`、`Source/PVPDemo/PVPDemo.Build.cs`、`Content/PVP/Characters/BP_PVPBaseCharacter.uasset`、现有 `ABP_PVP_FP_BaseCharacter`。