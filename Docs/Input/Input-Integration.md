# PVP 输入模块接口与依赖文档

> 维度：输入（Enhanced Input / InputTag / InputComponent）
> 模块：Input
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Input/README.md`（导航与动作速查）、`Docs/Input/Input-Dev.md`（开发流程与踩坑）

本文只写**依赖谁、暴露什么、别人怎么调用**。已实现动作、键位与回调函数的权威表在 `Docs/Input/README.md` 第 3 节。

---

## 1. 模块定位与边界

**职责**：

- 定义输入语义：`InputTag.*` → `InputAction` → 绑定事件 → 处理函数。
- 提供配置类型与绑定组件：`UPVPInputConfig`、`UPVPInputComponent`。
- 提供玩家入口的默认映射上下文注入点：`APVPPlayerController::DefaultMappingContexts`。

**非职责**：

- 不定义业务行为（移动、跳跃、跑步速度属于 `Character/`）。
- 不计算视角（属于 `Character/` 当前实现与后续 `Camera/`）。
- 不消费表现状态（Animation / Camera 只读结果）。
- 不管理技能输入（留给 GAS）。

---

## 2. 模块与外部依赖

### 2.1 Build.cs 依赖

运行时模块 `PVPDemo` 中与输入相关的依赖：

| 依赖 | 用途 |
| --- | --- |
| `EnhancedInput` | `UInputAction`、`UInputMappingContext`、`UEnhancedInputComponent`、`UEnhancedInputLocalPlayerSubsystem` |
| `GameplayTags` | `InputTag.*` |
| `InputCore` | `FKey` 等 |

- 原生输入**无需** `GameplayAbilities`；GAS 只影响未来的技能输入。
- 本模块当前没有新增第三方依赖。

### 2.2 配置依赖

`Config/DefaultInput.ini`：

- `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`
- `DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent`
- `bEnableLegacyInputScales=True`，`MouseX/Y`、`Mouse2D` 轴配置保持现状。

约定：

- 输入模块**不修改**该文件的全局项；`UPVPInputComponent` 通过 `APawn::OverrideInputComponentClass` 在 Pawn 级覆盖。
- 换组件类型的正确做法见 `Input-Dev.md` 踩坑 3，禁止改全局 `DefaultInputComponentClass` 来满足单个 Pawn。

---

## 3. 数据流与交互

```text
                    Content/PVP/Input
  ┌─────────────────────────────────────────────────────────────┐
  │ IMC_PVP_Default (键鼠 → IA)                                  │
  │ DA_PVP_InputConfig (InputTag → IA)                           │
  │ IA_PVP_*（动作定义）                                          │
  └─────────────────────────────────────────────────────────────┘
                              │
                              ▼
              Enhanced Input (UEnhancedInputLocalPlayerSubsystem)
                              │  AddMappingContext（PlayerController.BeginPlay）
                              ▼
              UPVPInputComponent（Pawn 的 InputComponent）
                              │  BindNativeAction(Config, Tag, ...)
                              ▼
                    APVPBaseCharacter
                    ┌──────────┬──────────┬──────────┬──────────┬──────────┐
                    ▼          ▼          ▼          ▼          ▼          ▼
                    Move       Look       Jump        Run        Aim        Fire
                    Movement   Controller  Jump/Stop  MaxWalk    SetAiming  PlayFire/AimedFire
                    Yaw/Pitch   Jumping    + bIsRunning            (EquipmentComponent)
                    │          │                     │          │
                    │          ▼                     │          ▼
                    │    CameraFP（bUsePawnControlRotation=true，│  FOV 100 ↔ 70（Tick 插值）
                    │    位置随 head 骨骼，旋转随 ControlRotation）│
                    ▼                                        ▼
                    Animation（只读 bIsRunning / bIsAiming / ControlRotation，不反向写输入）
```

**资产引用链**：

```text
BP_PVPGameMode (DefaultPawnClass / PlayerControllerClass)
        ├── BP_PVPBaseCharacter
        │       └── CDO.InputConfig → DA_PVP_InputConfig
        │                               ├── InputAction: IA_PVP_Move
        │                               ├── InputAction: IA_PVP_Look
        │                               ├── InputAction: IA_PVP_Jump
        │                               ├── InputAction: IA_PVP_Run
        │                               ├── InputAction: IA_PVP_ADS   (InputTag.Aim)
        │                               └── InputAction: IA_PVP_Fire  (InputTag.Fire)
        └── BP_PVPPlayerController
                └── C++ 构造函数 → IMC_PVP_Default（默认映射上下文）

IMC_PVP_Default
        └── IA_PVP_*（默认键位映射 + 修饰器）
```

- `DA_PVP_InputConfig` 只引用已绑定的 6 个 IA（Move / Look / Jump / Run / ADS / Fire）；其余 IA 只被 IMC 引用。
- `IMC_PVP_Default` 由 `APVPPlayerController` 构造函数硬引用（`ConstructorHelpers::FObjectFinder`），保证空蓝图也能工作；`DefaultMappingContexts` 可被蓝图覆盖 / 扩展。
- `BP_PVPBaseCharacter` 的 `InputConfig` 由 `setup_pvp_input.py` 赋到 CDO；资产缺失时角色只打印警告并跳过绑定，不崩溃。

**与相机 / 动画的边界**：

- `CameraFP->bUsePawnControlRotation = true`：相机**位置**跟随 `head` 骨骼（头 bob、蹲伏姿态仍然生效），**旋转**直接使用 Controller 的 Yaw / Pitch，受 `ViewPitchMin/Max`（默认 ±89.9）限制。
- 动画骨骼旋转（后坐、倾斜、受击）**不会**改变视角；这类效果应通过 `AddPitchInput` / `CameraModifier` / `CameraShake` 实现。
- 动画层若要让手臂 / 武器跟随俯仰，应读取 `ControlRotation.Pitch` 或 `GetBaseAimRotation().Pitch` 驱动 AimOffset / 脊柱，不得反向驱动相机。
- `bUsePawnControlRotation` 是相机模式栈落地前的简单方案；`Camera/` 模式实现后应设回 `false`，由相机模式统一计算 View。

---

## 4. 对外接口

### 4.1 C++ 接口

| 接口 | 位置 | 用途 |
| --- | --- | --- |
| `PVPGameplayTags::InputTag_Move/Look/Jump/Run/Aim/Fire` | `GAS/PVPGameplayTags.h` | 输入语义 Tag，供绑定与扩展查询 |
| `UPVPInputConfig::FindNativeInputActionForTag(const FGameplayTag&)` | `Input/PVPInputConfig.h` | 按 Tag 查 IA，找不到返回 `nullptr` |
| `UPVPInputConfig::EnsureNativeInputActionSlots()` | `Input/PVPInputConfig.h` | 给旧 DA 原地追加缺失 Tag 槽（Move / Look / Jump / Run / Aim / Fire），返回新增数量，不删除资产 |
| `UPVPInputConfig::SetNativeInputActionByIndex(int32, UInputAction*)` | `Input/PVPInputConfig.h` | 编辑器 / 脚本按槽位写入 IA，标记包脏 |
| `UPVPInputComponent::BindNativeAction(Config, Tag, TriggerEvent, Object, Func)` | `Input/PVPInputComponent.h` | 按 Tag 绑定 Enhanced Input 事件，返回 `FEnhancedInputActionEventBinding*` 或 `nullptr` |
| `FPVPInputMappingContextAndPriority` | `Input/PVPInputConfig.h` | IMC + Priority 结构，供 `DefaultMappingContexts` 使用 |
| `APVPBaseCharacter::InputConfig` | `Character/PVPBaseCharacter.h` | `EditDefaultsOnly` 配置入口，蓝图可覆盖 |
| `APVPBaseCharacter::WalkSpeed / RunSpeed` | `Character/PVPBaseCharacter.h` | 走 / 跑速度（默认 300 / 600） |
| `APVPBaseCharacter::bIsRunning` | `Character/PVPBaseCharacter.h` | `BlueprintReadOnly`，供 Animation / Camera 只读 |
| `APVPBaseCharacter::bIsAiming` / `AimFieldOfView` / `AimInterpolationSpeed` | `Character/PVPBaseCharacter.h` | 瞄准状态与相机 FOV 契约；`bIsAiming` 供 Animation / Camera 只读，瞄准输入从 `IA_PVP_ADS` 进入 |
| `APVPPlayerController::DefaultMappingContexts` | `Game/PVPPlayerController.h` | 默认 IMC 数组，蓝图可替换 / 追加 |
| `APVPPlayerController::GetPVPCharacter()` | `Game/PVPPlayerController.h` | 取当前 Pawn 的 `APVPBaseCharacter`，失败返回 `nullptr` |

### 4.2 资产与配置接口

| 资产 / 配置 | 路径 | 消费方 |
| --- | --- | --- |
| `DA_PVP_InputConfig` | `/Game/PVP/Input/DA_PVP_InputConfig` | `BP_PVPBaseCharacter` CDO |
| `IMC_PVP_Default` | `/Game/PVP/Input/IMC_PVP_Default` | `APVPPlayerController` 构造函数 / `DefaultMappingContexts` |
| `IA_PVP_Move/Look/Jump/Run/ADS/Fire` | `/Game/PVP/Input/` | `DA_PVP_InputConfig`、`IMC_PVP_Default` |
| `BP_PVPBaseCharacter` | `/Game/PVP/Characters/BP_PVPBaseCharacter` | `BP_PVPGameMode` 的 DefaultPawnClass |

**配置契约**：

- `UPVPInputConfig::NativeInputActions` 的槽位顺序由 C++ 构造函数 / `EnsureNativeInputActionSlots` 固定为 Move / Look / Jump / Run / Aim / Fire；`setup_pvp_input.py` 的 `REQUIRED_ACTIONS` 必须同序，按索引写入（详见 `Input-Dev.md` 踩坑 2、5.5）。
- `DA_PVP_InputConfig` 的 Tag 不可由脚本构造，只能通过 `SetNativeInputActionByIndex` 按槽位赋值；旧 4 槽资产先由 `EnsureNativeInputActionSlots` 补齐，禁止删除 DA 重建。
- `IMC_PVP_Default` 中 `IA_PVP_Look` → `Mouse2D` 的 `InputModifierNegate` 必须仅 Y 轴生效，且 C++ 不再取反（详见 `Input-Dev.md` 踩坑 1）。
- 已绑定动作的键位、事件与回调见 `Docs/Input/README.md` 第 3 节。

### 4.3 Tag 契约

| Tag | 语义 | 绑定位置 |
| --- | --- | --- |
| `InputTag.Move` | 平面移动 | `APVPBaseCharacter::Input_Move` |
| `InputTag.Look` | 视角 Yaw/Pitch | `APVPBaseCharacter::Input_Look` |
| `InputTag.Jump` | 起跳 / 停止起跳 | `APVPBaseCharacter::Input_JumpStarted/Completed` |
| `InputTag.Run` | 按住跑 | `APVPBaseCharacter::Input_RunStarted/Completed` |
| `InputTag.Aim` | 按住瞄准 | `APVPBaseCharacter::Input_AimStarted/Completed` |
| `InputTag.Fire` | 射击一次 | `APVPBaseCharacter::Input_FireStarted` |

规则：

1. Tag 只表达动作语义，不表达按键；按键只在 IMC 中配置。
2. 原生 Tag 统一声明在 `GAS/PVPGameplayTags.h`，用 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` / `UE_DEFINE_GAMEPLAY_TAG`；不要在运行时用字符串临时 `RequestGameplayTag` 拼 Tag。
3. 命名沿用 `InputTag.<Action>`；后续 GAS 技能输入扩展为 `InputTag.Ability.<Skill>`，不复用原生动作 Tag。
4. 新增 Tag 只增加实际被消费的条目，不预造未使用 Tag。
5. Tag 的消费方只做"按 Tag 查 Action"，不应了解具体 IA 资产路径。

---

## 5. 其他模块调用方式

### 5.1 调用顺序与前置条件

```text
IMC 注入（APVPPlayerController::BeginPlay，ClearAllMappings → AddMappingContext）
        ↓
Possess → PawnClientRestart 创建 InputComponent
        ↓
APVPBaseCharacter::SetupPlayerInputComponent → BindNativeAction（按 Tag 绑定，一次）
        ↓
Enhanced Input 按键路由 → IA → 绑定事件 → 处理函数
```

前置条件：

- `BP_PVPBaseCharacter` 的 CDO 已挂 `DA_PVP_InputConfig`（脚本保证）。
- `IMC_PVP_Default` 资产存在，且已加入 `DefaultMappingContexts`。
- Pawn 使用 `UPVPInputComponent`（构造函数指定），否则绑定被跳过。
- DA 槽位顺序与脚本一致；旧资产必须已由 `EnsureNativeInputActionSlots` 迁移到 6 槽。

### 5.2 在自定义 Pawn 上复用输入

1. 继承 `APVPBaseCharacter`，自动获得 `OverrideInputComponentClass`、6 个动作绑定与状态属性；新蓝图仍需在 CDO 挂 `DA_PVP_InputConfig`（或直接复用 `BP_PVPBaseCharacter`），否则绑定会被跳过；或
2. 自行 `OverrideInputComponentClass = UPVPInputComponent::StaticClass()`，在 `SetupPlayerInputComponent` 中调用 `BindNativeAction`，并自行持有 `InputConfig`。

两种方式都**不要**注入 IMC：默认上下文由 `APVPPlayerController` 负责。新增 Pawn 时同步 `BP_PVPGameMode` 的 DefaultPawnClass。

### 5.3 表现层如何只读

- Animation：`GetPVPCharacter()->bIsRunning`、`bIsAiming`、`ControlRotation` / `GetBaseAimRotation()`。
- Camera：`ControlRotation` 与角色位置状态，不调用 `AddMappingContext`、不改 `InputConfig`。
- UI / 重绑定（未实现）：未来接 Enhanced Input User Settings 时扩展 `DefaultMappingContexts` 的注册方式，不要在 UI 层直接注入 IMC。

### 5.4 GAS / 技能输入扩展（未实现）

- 建议新增 `InputTag.Ability.*`，在 `InputConfig` 中扩展 AbilityInputActions 或在 Hero / ASC 层绑定。
- 不要复用 `InputTag.Move/Look/Jump/Run/Aim/Fire` 承载技能输入。
- 保持 `Input → Character/GAS` 方向，技能层不得反向驱动输入绑定点。

---

## 6. 禁止的依赖方向

- `Input/` 不得包含 `Game/`、`Animation/`、`Camera/`、`Equipment/` 的头文件。
- `Character/` 不得为了输入去引用 `Game/`。
- `Animation/`、`Camera/` 不得通过输入系统修改角色状态；只读 `bIsRunning`、`ControlRotation` 等。
- 不允许把键名写进 C++ 绑定；键名变化只改 IMC。
- 不允许蓝图与 `APVPPlayerController` 同时注入 IMC（见 `Input-Dev.md` 踩坑 4）。

---

## 7. 变更影响面

| 改动 | 必须同步 |
| --- | --- |
| Tag 重命名 / 增删 | `GAS/PVPGameplayTags.h/.cpp`、DA 槽位、所有绑定点、`setup_pvp_input.py`、`README.md` 第 3 节 |
| IA 增删或路径变更 | `IMC_PVP_Default` 映射、`DA_PVP_InputConfig`、脚本校验、`README.md` 第 3 节 |
| IMC 键位 / 修饰器调整 | 脚本校验项（尤其 Look Negate(Y)）、`README.md` 键位列、PIE 冒烟、`Input-Dev.md` 踩坑 1 |
| DA 槽位顺序调整 | C++ 构造函数 / `EnsureNativeInputActionSlots`、脚本 `REQUIRED_ACTIONS`、迁移旧 DA、`Input-Dev.md` 踩坑 2、5.5 |
| `BindNativeAction` 签名变更 | 所有绑定点（当前 `APVPBaseCharacter`，未来 Hero / 其他 Pawn） |
| IMC 注入位置 / 时机变更 | `APVPPlayerController`、`Game/` 蓝图约定、`README.md` 第 4 节、`Input-Integration.md` 第 5.1 节 |
| `bIsRunning` / `WalkSpeed` / `RunSpeed` 语义变更 | Animation、Camera 只读方；`README.md` 第 3 节 |
| 输入组件类型变更 | `APVPBaseCharacter` 构造函数、`Input-Dev.md` 踩坑 3 |

- 任何一项改动完成后，同时刷新三份文档文首的「最后更新」。
- 资产数量变化同步 `Docs/Content-Structure.md`；源码文件变化同步 `Docs/Source-Structure.md`。

---

## 8. 相关文件

- 源码
  - `Source/PVPDemo/Input/PVPInputConfig.h/.cpp`
  - `Source/PVPDemo/Input/PVPInputComponent.h/.cpp`
  - `Source/PVPDemo/GAS/PVPGameplayTags.h/.cpp`
  - `Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp`
  - `Source/PVPDemo/Game/PVPPlayerController.h/.cpp`
- 资产
  - `Content/PVP/Input/IA_PVP_*`
  - `Content/PVP/Input/IMC_PVP_Default`
  - `Content/PVP/Input/DA_PVP_InputConfig`
  - `Content/PVP/Characters/BP_PVPBaseCharacter`
  - `Content/PVP/Game/BP_PVPPlayerController`、`BP_PVPGameMode`
- 配置与工具
  - `Config/DefaultInput.ini`
  - `Content/Python/setup_pvp_input.py`
  - `Saved/PVPInputReport/report.json|md`
