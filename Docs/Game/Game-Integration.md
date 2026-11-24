# PVP 会话与玩家入口模块接口与依赖文档

> 维度：会话与玩家入口（GameMode / PlayerController / 默认 IMC 注入）
> 模块：Game
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Game/README.md`（导航与入口速查）、`Docs/Game/Game-Dev.md`（开发流程与踩坑）

本文只写**依赖谁、暴露什么、别人怎么调用**。入口装配与启动时序的权威表在 `Docs/Game/README.md` 第 3 节。

---

## 1. 模块定位与边界

**职责**：

- 装配一局游戏的默认类与出生规则（当前仅"出生即操控"）。
- 提供玩家入口：GameOnly 输入模式、默认 IMC 注入、附身记录、只读角色访问器。
- 提供可观测的入口日志（`InitGame` / `StartPlay` / `BeginPlay` / `OnPossess`）。

**非职责**：

- 不实现角色行为、移动与相机（`Character/`）。
- 不定义输入语义、键位或 IA 资产（`Input/`）；本模块只消费 `UPVPInputConfig` 的 IMC 结构类型。
- 不管理技能、属性与 Cue（`GAS/`，未实现）。
- 不承载对局规则：GameState / PlayerState / 比分 / 回合未实现。

---

## 2. 模块与外部依赖

### 2.1 Build.cs 依赖

| 依赖 | 用途 |
| --- | --- |
| `Engine` | `AGameModeBase`、`APlayerController`、`ULocalPlayer`、`FInputModeGameOnly` |
| `EnhancedInput` | `UInputMappingContext`、`UEnhancedInputLocalPlayerSubsystem` |
| `InputCore` | 输入基础类型 |
| 模块内 `Input/` | `FPVPInputMappingContextAndPriority`（`DefaultMappingContexts` 元素类型） |
| 模块内 `Character/` | `APVPBaseCharacter`（默认 Pawn 与 `GetPVPCharacter` 的 Cast 目标） |

- 以上依赖均已在 `PVPDemo.Build.cs` 声明，**本次无新增依赖**。
- 本模块当前没有新增第三方依赖。

### 2.2 配置依赖

| 配置 | 当前值 | 影响 |
| --- | --- | --- |
| `Config/DefaultEngine.ini` → `GameDefaultMap` | `/Engine/Maps/Templates/OpenWorld` | 直接启动不会进入玩法地图，PIE 需手动打开 `L_PVP_Showcase` |
| `Config/DefaultEngine.ini` → `GlobalDefaultGameMode` | **未设置** | 非 `L_PVP_Showcase` 关卡不会使用 `BP_PVPGameMode`（见 `Game-Dev.md` 踩坑 1） |
| 关卡 WorldSettings | `L_PVP_Showcase` 指向 `BP_PVPGameMode` | 玩法入口的唯一落点 |
| `Config/DefaultInput.ini` | 未修改，保持 Enhanced Input 默认类 | 控制器通过子系统注入 IMC，不改全局输入组件类型 |

### 2.3 第三方依赖

- 本模块无第三方代码依赖。
- 框架重建脚本会校验 `/Game/PVP` 与 `/Game/PVPDemo` 内不再引用 `/Game/InfimaGames/`；第三方原包只作为一次性迁移源。

---

## 3. 数据流与交互

### 3.1 启动时序

```text
L_PVP_Showcase（WorldSettings.GameMode = BP_PVPGameMode）
        ▼
APVPGameMode::InitGame                → 日志 map / options
APVPGameMode::StartPlay               → 日志 default_pawn / player_controller
        ▼
引擎生成 PlayerController（BP_PVPPlayerController）
        ├── 构造函数：关闭光标与点击，加载 IMC_PVP_Default 到 DefaultMappingContexts（Priority 0）
        ├── OnPossess：记录 Pawn 类名与路径
        └── BeginPlay：SetInputMode(GameOnly)
                        → GetLocalPlayer() 守卫
                        → ClearAllMappings → AddMappingContext（按优先级）
        ▼
生成 Pawn（BP_PVPBaseCharacter）
        └── SetupPlayerInputComponent：按 Tag 绑定 6 个动作 / 9 条事件
        ▼
Enhanced Input 按键 → IA → 绑定事件 → APVPBaseCharacter 回调
```

### 3.2 资产引用链

```text
L_PVP_Showcase
  └── WorldSettings.GameMode → BP_PVPGameMode（CDO）
        ├── DefaultPawnClass       → BP_PVPBaseCharacter
        │       └── InputConfig    → DA_PVP_InputConfig → IA_PVP_*
        └── PlayerControllerClass  → BP_PVPPlayerController
                └── C++ 构造函数 → IMC_PVP_Default（DefaultMappingContexts[0]，Priority 0）
```

- C++ 默认类（`APVPBaseCharacter` / `APVPPlayerController`）不引用任何蓝图；蓝图装配全部落在 CDO 与 WorldSettings。
- `IMC_PVP_Default` 是控制器构造函数内的唯一硬引用资产，路径变更会告警而非崩溃（见 `Game-Dev.md` 踩坑 5.6）。

### 3.3 与输入模块的职责切分

| 层 | 负责 | 不负责 |
| --- | --- | --- |
| `Game/`（控制器） | 输入模式、IMC 注入时机与顺序、本地玩家守卫 | 动作绑定、键位、IA 资产 |
| `Input/` | Tag ↔ IA 配置类型、绑定组件、IMC 数据结构 | 注入时机、输入模式 |
| `Character/` | 动作处理函数与状态 | 注入 IMC、设置输入模式 |

绑定与键位的完整契约见 `Docs/Input/Input-Integration.md` 第 3 / 5 节。

---

## 4. 对外接口

### 4.1 C++ 接口

| 接口 | 位置 | 类型 | 用途 |
| --- | --- | --- | --- |
| `APVPGameMode` 的类默认 | `Game/PVPGameMode.cpp` 构造函数 | — | `PlayerControllerClass` / `DefaultPawnClass` / `bStartPlayersAsSpectators`，蓝图可覆盖 |
| `APVPGameMode::InitGame` / `StartPlay` | `Game/PVPGameMode.h` | `AGameModeBase` 覆写 | 会话入口可观测点，当前仅 `Super` + 日志 |
| `DefaultMappingContexts` | `Game/PVPPlayerController.h` | `TArray<FPVPInputMappingContextAndPriority>`，`EditDefaultsOnly` / `BlueprintReadOnly` | 默认 IMC 列表，蓝图可替换 / 追加；元素类型来自 `Input/PVPInputConfig.h` |
| `GetPVPCharacter()` | 同上 | `UFUNCTION(BlueprintPure)`，返回 `APVPBaseCharacter*` | 当前 Pawn 的角色访问器，非角色 Pawn 返回 `nullptr` |
| `BeginPlay` / `SetupInputComponent` / `OnPossess` | 同上 | `APlayerController` 覆写 | 输入模式与注入、绑定职责声明、附身记录 |

**契约**：

- `DefaultMappingContexts` 中同一 IMC 不得重复出现；也不得与蓝图事件里的 `AddMappingContext` 叠加（`Game-Dev.md` 踩坑 3）。
- `GetPVPCharacter()` 只读；调用方不得据此修改角色状态（改状态应通过输入或 GAS）。
- `APVPGameMode` 构造函数不得加入蓝图路径查找，否则内容缺失时类无法构造。

### 4.2 资产与配置接口

| 资产 / 配置 | 路径 | 消费方 |
| --- | --- | --- |
| `BP_PVPGameMode` | `/Game/PVP/Game/BP_PVPGameMode` | `L_PVP_Showcase` 的 WorldSettings |
| `BP_PVPPlayerController` | `/Game/PVP/Game/BP_PVPPlayerController` | `BP_PVPGameMode` CDO 的 `PlayerControllerClass` |
| `BP_PVPBaseCharacter` | `/Game/PVP/Characters/BP_PVPBaseCharacter` | `BP_PVPGameMode` CDO 的 `DefaultPawnClass` |
| `L_PVP_Showcase` | `/Game/PVP/Maps/L_PVP_Showcase` | 唯一玩法入口地图，出生点 `PlayerStart` |
| `IMC_PVP_Default` | `/Game/PVP/Input/IMC_PVP_Default` | `APVPPlayerController` 构造函数（硬编码路径） |

**蓝图约束**：

- `verify_pvp_game_framework.py` 断言 `BP_PVPBaseCharacter` 的父类、`BP_PVPGameMode` CDO 的默认 Pawn / 控制器路径；模式与控制器蓝图只断言存在性，父类由 `setup_pvp_game_framework.py` 保证，手工改父类后需重跑脚本校验。
- 在蓝图里扩展 `DefaultMappingContexts` 是支持的；在蓝图事件里注入 IMC 不支持。

---

## 5. 其他模块调用方式

### 5.1 输入模块如何被本模块驱动

```text
APVPPlayerController::BeginPlay
        → UEnhancedInputLocalPlayerSubsystem::ClearAllMappings()
        → AddMappingContext(DefaultMappingContexts[i].InputMapping, Priority)
```

前置条件：`DefaultMappingContexts` 至少有一个有效 IMC；存在本地玩家。详细契约见 `Docs/Input/Input-Integration.md` 第 5.1 节。

### 5.2 表现层 / UI 如何取角色

```text
APlayerController* PC = ...
APVPPlayerController* PVPController = Cast<APVPPlayerController>(PC);
APVPBaseCharacter* Character = PVPController ? PVPController->GetPVPCharacter() : nullptr;
```

- 蓝图侧等价于 `Get Player Controller → Cast To BP_PVPPlayerController → GetPVPCharacter`。
- 只读用途：读 `bIsRunning`、`ControlRotation`、相机变换；不写状态。

### 5.3 替换或新增入口类

1. 自定义 GameMode：继承 `APVPGameMode`，覆写 `InitGame` / `StartPlay` 时先调 `Super`，日志保留（便于确认生效类）。
2. 自定义控制器：继承 `APVPPlayerController`，保留 `DefaultMappingContexts` 语义与 `ClearAllMappings → AddMappingContext` 顺序；不要在 `SetupInputComponent` 里绑定原生动作。
3. 自定义 Pawn：继承 `APVPBaseCharacter` 或在 `SetupPlayerInputComponent` 中自行绑定，见 `Docs/Character/Character-Integration.md` 第 5.1 节。
4. 装配完成后同步 `BP_PVPGameMode` CDO，并跑 `verify_pvp_game_framework.py`。

### 5.4 技能入口（未实现）

- ASC 规划挂在 `PlayerState`，初始化点应在角色 `Possess` 之后；届时本模块可能新增 `OnPossess` 后的初始化钩子或 PlayerState 类指定。
- 当前不要在 Game 模块引入 `AbilitySystemComponent` / `GameplayAbility`。

---

## 6. 禁止的依赖方向

- `Character/` 不得包含 `Game/` 的头文件；角色不感知游戏模式与控制器实现。需要取角色时由 `Game/` 侧 Cast。
- `Input/` 不得包含 `Game/` 的头文件；控制器注入 IMC 属于 `Game → Input` 单向。
- `Game/` 不得直接引用 `IA_PVP_*` 资产或绑定键位；键位与动作只存在于 `Input/` 的 IMC / DA。
- 同一 IMC 不得由控制器与蓝图各注入一次；表现层不得增删 IMC。
- 不允许在 `APVPGameMode` 构造函数里硬编码蓝图路径（内容缺失会导致类无法构造）。

---

## 7. 变更影响面

| 改动 | 必须同步 |
| --- | --- |
| 默认 Pawn / 控制器类变化 | `BP_PVPGameMode` CDO、`Game-Dev.md` 第 4.1 节、本文第 4.2 / 5.3 节、`Docs/Character/Character-Integration.md` 第 3.2 节、`Docs/Input/Input-Integration.md` 第 3 节 |
| 输入模式或 IMC 注入逻辑变化 | `Docs/Input/README.md` 第 4 节、`Docs/Input/Input-Integration.md` 第 5.1 节、本文第 3.1 节、`Game-Dev.md` 踩坑 3 |
| `DefaultMappingContexts` 契约变化 | 本文第 4.1 节、`Game-Dev.md` 踩坑 3、`setup_pvp_input.py` 的 IMC 校验项 |
| `GetPVPCharacter` 签名或返回类型变化 | 所有调用方（UI / 表现层）、`Docs/Character/Character-Integration.md` 第 5.3 节 |
| 新增 GameMode 覆写点或对局流程 | 本文第 4.1 / 5.3 节、`README.md` 第 3.1 / 5 节、`Docs/Source-Structure.md` 2.7 节 |
| 地图或 WorldSettings 指向变化 | `Game-Dev.md` 踩坑 1、`verify_pvp_game_framework.py` 断言、`Docs/Content-Structure.md` 第 4 节 |
| 框架蓝图重建 | `Saved/PVPGameFrameworkReport/` 报告、本文第 4.2 节 |

- 任何一项改动完成后，同时刷新 `Docs/Game/` 三份文档文首的「最后更新」。
- 资产数量变化同步 `Docs/Content-Structure.md`；源码文件变化同步 `Docs/Source-Structure.md`。

---

## 8. 相关文件

- 源码
  - `Source/PVPDemo/Game/PVPGameMode.h/.cpp`
  - `Source/PVPDemo/Game/PVPPlayerController.h/.cpp`
  - `Source/PVPDemo/Input/PVPInputConfig.h`（`FPVPInputMappingContextAndPriority`）
  - `Source/PVPDemo/Character/PVPBaseCharacter.h`（默认 Pawn）
- 资产
  - `Content/PVP/Game/BP_PVPGameMode`、`BP_PVPPlayerController`
  - `Content/PVP/Maps/L_PVP_Showcase`
  - `Content/PVP/Characters/BP_PVPBaseCharacter`、`Content/PVP/Input/IMC_PVP_Default`
- 配置与工具
  - `Config/DefaultEngine.ini`、`Config/DefaultInput.ini`
  - `Content/Python/setup_pvp_game_framework.py`、`verify_pvp_game_framework.py`、`setup_pvp_input.py`
  - `Saved/PVPGameFrameworkReport/`
