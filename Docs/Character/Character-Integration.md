# PVP 角色模块接口与依赖文档

> 维度：角色（Character / 移动状态 / 1P 相机内置）
> 模块：Character
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-23
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Character/README.md`（导航与能力速查）、`Docs/Character/Character-Dev.md`（开发流程与踩坑）

本文只写**依赖谁、暴露什么、别人怎么调用**。组件默认值与移动状态的权威表在 `Docs/Character/README.md` 第 3 节，动作与键位在 `Docs/Input/README.md` 第 3 节。

---

## 1. 模块定位与边界

**职责**：

- 定义玩家角色本体：组件装配、1P 相机 `CameraFP`、移动状态（走 / 跑）与原生输入回调实现。
- 向 `Animation/`、`Camera/`、`Game/` 暴露只读状态与访问器。

**非职责**：

- 不定义输入语义、键位与 IMC（属于 `Input/` 与 `APVPPlayerController`）。
- 不负责默认 Pawn / 控制器装配与输入模式（属于 `Game/`）。
- 不实现相机模式栈（`Camera/` 预留）、技能与属性（`GAS/`）；动画实例与新动画蓝图由 `Animation/` 的 `UPVPAnimInstanceBase` / `ABP_PVP_FP_Locomotion` 只读采集本模块状态，但新 ABP 尚未接线（见 `Docs/Animation/`）。
- 不参与会话规则、对局流程与出生点选择。

---

## 2. 模块与外部依赖

### 2.1 Build.cs 依赖

运行时模块 `PVPDemo` 中与角色相关的依赖（均已在 `PVPDemo.Build.cs` 声明，**本次无新增**）：

| 依赖 | 用途 |
| --- | --- |
| `Engine` | `ACharacter`、`UCameraComponent`、`UCharacterMovementComponent`、`USkeletalMeshComponent` |
| `EnhancedInput` | 头文件中的 `FInputActionValue` |
| `InputCore` | 输入相关基础类型（随 Enhanced Input 一并使用） |
| `GameplayTags` | 已声明；角色经 `GAS/PVPGameplayTags` 使用原生 Tag（`FNativeGameplayTag` 类型由该模块提供） |

- 模块内部依赖：`Character/` 只依赖 `Input/`（`UPVPInputComponent`、`UPVPInputConfig`）与 `GAS/PVPGameplayTags`。
- 本模块当前没有新增第三方依赖。

### 2.2 配置依赖

- `Config/DefaultInput.ini`：**未修改**。`DefaultInputComponentClass` 保持 `/Script/EnhancedInput.EnhancedInputComponent`，角色通过 `APawn::OverrideInputComponentClass` 在 Pawn 级覆盖为 `UPVPInputComponent`。
- `Config/DefaultEngine.ini`：未设置 `GlobalDefaultGameMode`，`GameDefaultMap` 仍为引擎模板地图；玩法入口由 `L_PVP_Showcase` 的 WorldSettings 指向 `BP_PVPGameMode`（详见 `Docs/Game/`）。

### 2.3 第三方依赖（已解除）

- 构造期通过 `ConstructorHelpers` 引用的第一人称网格与动画蓝图已改用工程自有资产：`/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple` 与 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter`。
- 影响：克隆工程无需导入 `/Game/InfimaGames` 原包即可获得完整第一人称网格与动画；原包仅作开发期素材参考且不得修改。
- 回归校验：`Content/Python/verify_pvp_asset_layout.py` 断言源码中不再出现 `/Game/InfimaGames/`，且两处默认路径存在。

---

## 3. 数据流与交互

### 3.1 输入 → 角色 → 表现

```text
Content/PVP/Input
  IMC_PVP_Default（键位 → IA + 修饰器）
  DA_PVP_InputConfig（InputTag → IA）
        │  APVPPlayerController.BeginPlay：AddMappingContext
        ▼
UPVPInputComponent（APVPBaseCharacter 的 InputComponent）
        │  SetupPlayerInputComponent：按 Tag 绑定 4 个动作 / 6 条事件
        ▼
APVPBaseCharacter
  ├── Input_Move  → AddMovementInput（ControlRotation.Yaw 平面）
  ├── Input_Look  → AddControllerYawInput / AddControllerPitchInput
  ├── Input_Jump* → Jump / StopJumping
  └── Input_Run*  → SetRunning → MaxWalkSpeed
        │
        ├── CharacterMovement：MaxWalkSpeed 走 / 跑
        ├── Controller：ControlRotation（Yaw / Pitch，Yaw 同时驱动 Actor 朝向）
        ├── CameraFP：bUsePawnControlRotation 消费 ControlRotation，位置挂 head 骨骼
        └── Animation：只读 bIsRunning / ControlRotation（不反向写）
```

### 3.2 资产引用链

```text
L_PVP_Showcase（WorldSettings.GameMode → BP_PVPGameMode）
        └── BP_PVPGameMode（CDO）
                ├── DefaultPawnClass → BP_PVPBaseCharacter
                │       └── CDO.InputConfig → DA_PVP_InputConfig
                │               ├── IA_PVP_Move / IA_PVP_Look
                │               └── IA_PVP_Jump / IA_PVP_Run
                └── PlayerControllerClass → BP_PVPPlayerController
```

- `BP_PVPBaseCharacter` 继承 `APVPBaseCharacter`，CDO 的 `InputConfig` 由 `setup_pvp_input.py` 写入。
- `DA_PVP_InputConfig` 只引用已绑定的 4 个 IA；`IMC_PVP_Default` 的其余映射不进入角色绑定。
- 原生 C++ 类（`APVPBaseCharacter`）不硬编码任何蓝图路径，装配关系全部落在蓝图 CDO 与地图 WorldSettings 上。

---

## 4. 对外接口

### 4.1 C++ 接口

| 接口 | 位置 | 类型 | 用途 |
| --- | --- | --- | --- |
| `CameraFP` | `Character/PVPBaseCharacter.h` | `TObjectPtr<UCameraComponent>`，`VisibleAnywhere` / `BlueprintReadOnly` | 第一人称相机；蓝图可改 FOV 等视图参数，**不得重挂或重建** |
| `InputConfig` | 同上 | `TObjectPtr<UPVPInputConfig>`，`EditDefaultsOnly` / `BlueprintReadOnly` | Tag → IA 配置，`SetupPlayerInputComponent` 消费 |
| `WalkSpeed` | 同上 | `float`，默认 `300`，`EditDefaultsOnly`，`ClampMin=0` | 未按住跑时的 `MaxWalkSpeed` |
| `RunSpeed` | 同上 | `float`，默认 `600`，`EditDefaultsOnly`，`ClampMin=0` | 按住跑时的 `MaxWalkSpeed` |
| `bIsRunning` | 同上 | `bool`，`BlueprintReadOnly` | 跑步状态，表现层只读 |
| `Input_Move` / `Input_Look` / `Input_JumpStarted` / `Input_JumpCompleted` / `Input_RunStarted` / `Input_RunCompleted` | 同上（`protected`） | `void(const FInputActionValue&)` | 原生输入回调，只由 `SetupPlayerInputComponent` 绑定 |
| `SetRunning(bool)` | 同上（`protected`） | `void` | 切换跑步状态并写 `MaxWalkSpeed`；状态相同直接返回 |
| `BeginPlay` / `SetupPlayerInputComponent` | 同上 | `AActor` / `APawn` 虚函数覆写 | 速度重应用与输入绑定入口 |

### 4.2 资产与配置接口

| 资产 / 配置 | 路径 | 消费方 |
| --- | --- | --- |
| `BP_PVPBaseCharacter` | `/Game/PVP/Characters/BP_PVPBaseCharacter` | `BP_PVPGameMode` 的 `DefaultPawnClass`；CDO 挂 `DA_PVP_InputConfig` |
| `DA_PVP_InputConfig` | `/Game/PVP/Input/DA_PVP_InputConfig` | `APVPBaseCharacter::InputConfig`（CDO） |
| `IMC_PVP_Default` | `/Game/PVP/Input/IMC_PVP_Default` | `APVPPlayerController`（间接，由控制器注入） |
| `BPI_PVP_AnimationState` | `/Game/PVP/Animation/Interfaces/` | 角色表现链（动画状态契约） |
| `ABP_PVP_FP_Locomotion` | `/Game/PVP/Animation/AnimBP/` | 新建的 locomotion ABP（父类 `UPVPAnimInstanceBase`，四状态），尚未挂到角色 |
| `ABP_PVP_Manny_PostProcess` | `/Game/PVP/Animation/AnimBP/` | 骨骼网格体 `SKM_PVP_Manny` 的 `PostProcessAnimBlueprint` |

**契约**：

- `CameraFP` 的挂点（`CharacterMesh0` 的 `head` 骨骼）、相对旋转 `(0,90,-90)`、FOV `100`、`bUsePawnControlRotation=true` 是对照脚本的校验项，改动即需重跑 `compare_tfa_character.py`。
- `bIsRunning` 只读；表现层不得写回，也不得据此跳过逻辑更新。
- `WalkSpeed` / `RunSpeed` 的生效点是 `BeginPlay`，不是构造函数（见 `Character-Dev.md` 踩坑 3）。

---

## 5. 其他模块调用方式

### 5.1 `Input/` 如何驱动本模块

```text
APVPPlayerController.BeginPlay（IMC 注入）
        → Possess → SetupPlayerInputComponent
        → UPVPInputComponent::BindNativeAction(InputConfig, Tag, Event, this, &Callback)
        → 按键路由到本模块回调
```

前置条件：Pawn 使用 `UPVPInputComponent`；CDO 已挂 `DA_PVP_InputConfig`；DA 槽位顺序与 `setup_pvp_input.py` 一致。详细契约见 `Docs/Input/Input-Integration.md` 第 5.1 节。

### 5.2 `Game/` 如何装配本模块

- `APVPGameMode` 构造函数把 `DefaultPawnClass` 设为原生 `APVPBaseCharacter`；`BP_PVPGameMode` 的 CDO 再指向 `BP_PVPBaseCharacter`。
- `APVPPlayerController::GetPVPCharacter()` 返回 `Cast<APVPBaseCharacter>(GetPawn())`，供 UI / 表现层取当前角色。
- 依赖方向单向：`Game → Character`；角色侧不得引用 `Game/`（见第 6 节）。

### 5.3 表现层如何只读

- Animation：当前 `UPVPAnimInstanceBase` 只读 `bIsRunning`、`Velocity`、`RunSpeed` 与移动组件模式；`ABP_PVP_FP_Locomotion` 消费这些变量输出四状态姿态，但尚未挂到角色（见 `Docs/Animation/`）；后续若要手臂 / 武器跟随俯仰，再读 `ControlRotation.Pitch` / `GetBaseAimRotation()` 驱动 AimOffset / 脊柱。
- Camera（当前为内置 `CameraFP`）：消费 `ControlRotation` 与挂点位置，不写角色状态，不注入 IMC。
- UI（未实现）：经 `APVPPlayerController::GetPVPCharacter()` 读取，不直接改 `MaxWalkSpeed` / `bIsRunning`。

### 5.4 `GAS/` 扩展点（未实现）

- 未来 ASC 规划挂在 `PlayerState`，由角色初始化；速度、冲刺等属性建议改为 GAS 属性驱动，替换 `WalkSpeed` / `RunSpeed` 的本地写入。
- 当前不要在本模块引入 `AbilitySystemComponent` 或 `GameplayAbility` 相关代码，见 `Docs/GAS/README.md` 第 5 节。

### 5.5 相机模块迁移（预留，未开始）

`Camera/` 模式栈落地后，`CameraFP` 应从本模块迁出：

1. 新建 `Camera/PVPCameraComponent` 与模式栈，`CameraFP` 由相机组件管理；
2. `CameraFP->bUsePawnControlRotation` 设回 `false`，视角由相机模式计算；
3. 同步 `Docs/Input/Input-Integration.md` 第 3 节（Look 极性约定随相机管线重评）、`Docs/Character/` 三份文档与 `Docs/Source-Structure.md` 2.1 / 2.2 节。

---

## 6. 禁止的依赖方向

- `Character/` 不得包含 `Game/`、`Animation/`、`Camera/`、`Equipment/` 的头文件；需要外部读取时由对方 include 本模块头文件。
- `Character/` 不得注入 IMC、不得设置输入模式、不得直接引用 `IA_PVP_*` / `IMC_PVP_Default` 资产。
- `Character/` 不得在运行时拼 GameplayTag 字符串；Tag 统一来自 `GAS/PVPGameplayTags.h`。
- `Animation/`、`Camera/` 不得写 `ControlRotation`、`bIsRunning`、`MaxWalkSpeed` 或调用 `SetRunning`。
- 不允许在蓝图与 C++ 同时设置同一默认值（以 C++ 为准或明确只在 CDO 覆盖），避免"改了没生效"。

---

## 7. 变更影响面

| 改动 | 必须同步 |
| --- | --- |
| 胶囊 / Mesh / 眼高 / 相机挂点 / FOV 默认值 | `compare_tfa_character.py` 校验基线、`Saved/PVPCharacterDump/parity_report.md`、本文第 4.2 节、`README.md` 第 3.1 节 |
| `WalkSpeed` / `RunSpeed` / `bIsRunning` 语义 | 本文第 4.1 节、`README.md` 第 3.2 节、Animation / Camera 只读方、`Docs/Input/README.md` 第 3 节行为列 |
| 新增 / 修改输入回调或绑定数量 | `SetupPlayerInputComponent` 的 `ExpectedBindings`、`Docs/Input/README.md` 第 3 节、`Docs/Input/Input-Integration.md` 第 4 / 5 节、`setup_pvp_input.py` |
| `InputConfig` 类型或槽位契约变化 | `Source/PVPDemo/Input/PVPInputConfig.h/.cpp`、`setup_pvp_input.py`、`Docs/Input/Input-Integration.md` 第 4.2 节 |
| 新增组件（3P / 透视 / 程序化） | 本文第 3 / 4 节、`README.md` 第 3.1 / 5 节、`Docs/Source-Structure.md` 2.1 节 |
| 输入组件类型或 `OverrideInputComponentClass` | 本文第 2.2 节、`Character-Dev.md` 踩坑 1、`Docs/Input/Input-Integration.md` 第 2.2 节 |
| 构造期第三方引用改为自有副本 | 本文第 2.3 节、`Character-Dev.md` 踩坑 5、`Docs/Source-Structure.md` 2.1 节 |
| 相机迁出到 `Camera/` | 本文第 5.5 / 6 节、`Docs/Source-Structure.md` 2.1 / 2.2 节、`Docs/Input/Input-Integration.md` 第 3 节 |

- 任何一项改动完成后，同时刷新 `Docs/Character/` 三份文档文首的「最后更新」。
- 资产数量变化同步 `Docs/Content-Structure.md`；源码文件变化同步 `Docs/Source-Structure.md`。

---

## 8. 相关文件

- 源码
  - `Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp`
  - `Source/PVPDemo/Input/PVPInputComponent.h/.cpp`（依赖方）
  - `Source/PVPDemo/Input/PVPInputConfig.h/.cpp`（依赖方）
  - `Source/PVPDemo/GAS/PVPGameplayTags.h/.cpp`（Tag 来源）
- 资产
  - `Content/PVP/Characters/BP_PVPBaseCharacter`
  - `Content/PVP/Input/DA_PVP_InputConfig`、`IMC_PVP_Default`、`IA_PVP_*`
  - `Content/PVP/Game/BP_PVPGameMode`、`BP_PVPPlayerController`
  - `Content/PVP/Maps/L_PVP_Showcase`
- 配置与工具
  - `Config/DefaultInput.ini`、`Config/DefaultEngine.ini`
  - `Content/Python/dump_tfa_character.py`、`compare_tfa_character.py`
  - `Saved/PVPCharacterDump/`
