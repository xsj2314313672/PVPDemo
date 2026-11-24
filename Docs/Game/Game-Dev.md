# PVP 会话与玩家入口模块开发文档

> 维度：会话与玩家入口（GameMode / PlayerController / 默认 IMC 注入）
> 模块：Game
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Game/README.md`（导航与入口速查）、`Docs/Game/Game-Integration.md`（依赖与接口契约）

本文只写**怎么实现、怎么开发、踩过什么坑**。入口装配、控制器参数与启动时序的权威表在 `Docs/Game/README.md` 第 3 节，动作与键位在 `Docs/Input/README.md` 第 3 节。

---

## 1. 目的与范围

覆盖范围：

- `Source/PVPDemo/Game/PVPGameMode.h/.cpp`：默认类装配与入口日志；
- `Source/PVPDemo/Game/PVPPlayerController.h/.cpp`：输入模式、默认 IMC 注入、附身记录、角色访问器；
- `Content/PVP/Game/`、`Content/PVP/Maps/L_PVP_Showcase` 的框架资产装配与校验；
- 入口相关踩坑、校验与 PIE 冒烟。

不覆盖：角色本体（`Docs/Character/`）、输入动作与键位（`Docs/Input/`）、技能与属性（`Docs/GAS/`）、对局流程（未实现）。

---

## 2. 当前状态与源码结构

### 2.1 当前状态

- 游戏模式：`APVPGameMode : AGameModeBase`，构造设 `PlayerControllerClass = APVPPlayerController`、`DefaultPawnClass = APVPBaseCharacter`、`bStartPlayersAsSpectators = false`。
- 玩家控制器：`APVPPlayerController : APlayerController`，构造关闭鼠标光标与点击 / 悬停，`ConstructorHelpers` 注入 `IMC_PVP_Default`；`BeginPlay` 切 GameOnly 并应用默认 IMC。
- 装配落点：`BP_PVPGameMode` 的 CDO 指向 `BP_PVPBaseCharacter` / `BP_PVPPlayerController`；`L_PVP_Showcase` 的 WorldSettings 指向 `BP_PVPGameMode`。
- 输入绑定职责：控制器只注入 IMC，原生动作绑定在 Pawn（见 `Docs/Input/Input-Integration.md` 第 5.1 节）。
- 对局层：GameState / PlayerState / 回合 / 比分均为空。

### 2.2 源码结构

| 文件 / 区域 | 职责 |
| --- | --- |
| `PVPGameMode.h` | 类声明与职责注释（不硬编码蓝图路径的原因、GameState / PlayerState 留待后续） |
| `PVPGameMode.cpp` — 构造函数 | 设 `PlayerControllerClass` / `DefaultPawnClass` / `bStartPlayersAsSpectators` |
| `PVPGameMode.cpp` — `InitGame` / `StartPlay` | 仅 `Super` + 一条日志，便于从日志确认实际生效的 GameMode 与默认类 |
| `PVPPlayerController.h` | `DefaultMappingContexts`（`EditDefaultsOnly` / `BlueprintReadOnly`）、`GetPVPCharacter()`（`BlueprintPure`）、覆写声明 |
| `PVPPlayerController.cpp` — 构造函数 | 关闭光标与点击事件；`ConstructorHelpers::FObjectFinder<UInputMappingContext>` 加载 `IMC_PVP_Default` 并追加 Priority 0 |
| `PVPPlayerController.cpp` — `BeginPlay` | `SetInputMode(GameOnly)` → 本地玩家守卫 → `ClearAllMappings` → 按优先级 `AddMappingContext` → 数量 / 缺失日志 |
| `PVPPlayerController.cpp` — `SetupInputComponent` | 仅 `Super`；注释说明绑定在 Pawn 上 |
| `PVPPlayerController.cpp` — `OnPossess` | 记录实际 Pawn 类名与包路径 |
| `PVPPlayerController.cpp` — `GetPVPCharacter` | `Cast<APVPBaseCharacter>(GetPawn())` |

### 2.3 本模块涉及的资产

| 资产 | 角色 | 来源 |
| --- | --- | --- |
| `BP_PVPGameMode` | 会话入口蓝图：CDO 指向 `BP_PVPBaseCharacter` 与 `BP_PVPPlayerController` | `Content/PVP/Game/`，由 `setup_pvp_game_framework.py` 创建 |
| `BP_PVPPlayerController` | 继承 `APVPPlayerController`，可扩展 `DefaultMappingContexts` | 同上 |
| `L_PVP_Showcase` | 唯一自有地图，WorldSettings 指向 `BP_PVPGameMode`，出生点为 `PlayerStart` | `Content/PVP/Maps/`，由同一脚本复制并改造 |

- 资产数量只看 `Docs/Content-Structure.md`，本文不复制总数。
- `.uasset` / `.umap` 只能由 UE 编辑器或 `Content/Python/` 脚本产出，禁止手写 / 伪造。

---

## 3. 设计决策与理由

1. **C++ 只设原生类，蓝图负责具体资产**。`APVPGameMode` 构造函数不 `FClassFinder` 任何蓝图，避免内容缺失时类无法构造；指向 `BP_PVPBaseCharacter` 的职责由 `BP_PVPGameMode` 的 CDO 承担。
2. **默认 IMC 注入放控制器**。默认上下文属于"玩家入口"的职责，任何 Pawn（含后续观战 / 载具）进入时都能拿到一致基座；Pawn 只负责自己的动作绑定。
3. **注入前先 `ClearAllMappings`**。保证一次 Possess / 重进关卡后映射集合确定，不受上一次运行的残留影响；代价是蓝图里额外注入的上下文会被清掉（踩坑 3）。
4. **本地玩家守卫**。专用服务器与 listen server 上的远端控制器没有 `LocalPlayer`，`GetLocalPlayer()` 为空时整段跳过，避免空指针与无意义日志。
5. **绑定留在 Pawn**。控制器 `SetupInputComponent` 只调 `Super`，原生动作与未来技能输入共用 Pawn 上的 `UPVPInputComponent` 与一份 `DA_PVP_InputConfig`，避免两处各绑一次导致重复触发。
6. **日志当作可观测契约**。`InitGame` / `StartPlay` / `OnPossess` / IMC 数量各一条日志，能直接从日志判断"用了哪个 GameMode、哪个 Pawn、注入了几份上下文"，不依赖断点。
7. **`DefaultMappingContexts` 可扩展不可重复**。数组是 `EditDefaultsOnly`，蓝图追加新上下文即可；但同一 IMC 不允许既在数组里又在蓝图事件里 `AddMappingContext`。

---

## 4. 开发流程 SOP

### 4.1 替换默认 Pawn 或玩家控制器

1. **改 C++ 默认**：在 `APVPGameMode` 构造函数换 `DefaultPawnClass` / `PlayerControllerClass`，保持为原生类，不要写蓝图路径。
2. **改蓝图 CDO**：在 `BP_PVPGameMode` 中把对应类指向目标蓝图；两处不一致时以蓝图 CDO 为准，并在提交说明中写清。
3. **补输入契约**：新 Pawn 若继承 `APVPBaseCharacter`，需要自己的 CDO 挂 `DA_PVP_InputConfig`；若完全自定义，按 `Docs/Input/Input-Integration.md` 第 5.2 节实现输入。
4. **校验**：重编译 → 跑 `verify_pvp_game_framework.py` → PIE 检查 `StartPlay default_pawn=...` 日志与实际 Pawn 一致。

### 4.2 新增控制器能力（覆写点 / 蓝图可配属性）

1. **确定归属**：玩家入口相关（输入模式、上下文、附身、会话入口）留在 `Game/`；角色行为放 `Character/`；键位放 `Input/`。
2. **优先用覆写点**：UE 已提供 `BeginPlay` / `OnPossess` / `OnUnPossess` / `SetupInputComponent` / `PlayerTick` 等，不新造并行流程。
3. **属性命名与分类**：`UPROPERTY` 用 `Category = "PVP|<域>"`，需要蓝图读的加 `BlueprintReadOnly`，需要配置的用 `EditDefaultsOnly`。
4. **加日志**：入口类行为变化必须留一条日志（含关键对象名），便于 PIE 与打包后排查。
5. **依赖方向自检**：不得从控制器反向要求 `Character/` 感知 `Game/`；控制器 include 角色头文件是允许的单向依赖。
6. **同步文档**：`Docs/Game/README.md` 第 3 节能力表、`Game-Integration.md` 第 4 / 7 节、必要时 `Docs/Source-Structure.md` 2.7 节。

### 4.3 重建框架资产与地图

```text
UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
    -Script="<project>/Content/Python/setup_pvp_game_framework.py" -unattended -nosplash -nullrhi
UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
    -Script="<project>/Content/Python/verify_pvp_game_framework.py" -unattended -nosplash -nullrhi
```

- `setup_*` 幂等：移动 `BP_PVPBaseCharacter`、创建模式 / 控制器蓝图、复制并改造 `L_PVP_Showcase`、重定向第三方引用，最多 6 轮定点重映射。
- `verify_*` 每条断言都是硬断言，首个失败即非零退出；报告总会写出。
- 脚本会改写 `BP_PVPGameMode` CDO 的默认类，手工改动蓝图后重跑脚本会被覆盖——手工调整前先确认脚本是否覆盖该字段。

---

## 5. 踩坑清单

### 5.1 进入关卡没有角色、也没有控制器日志

- **现象**：PIE 后原地空场，无 Pawn、无输入，控制台完全没有 `LogPVPGameMode` / `LogPVPPlayerController` 输出。
- **根因**：`Config/DefaultEngine.ini` 未设置 `GlobalDefaultGameMode`，`GameDefaultMap` 仍是引擎模板地图；只有 `L_PVP_Showcase` 的 WorldSettings 指向 `BP_PVPGameMode`，其他关卡走引擎默认 `AGameModeBase`。
- **正确做法**：在 `L_PVP_Showcase` 中 PIE；或在目标关卡的 WorldSettings 里把 GameMode Override 设为 `BP_PVPGameMode`。不要改 `GameDefaultMap` 指向玩法地图来绕过（会污染全局默认）。
- **验证方式**：PIE 日志出现 `InitGame map=/Game/PVP/Maps/L_PVP_Showcase` 与 `StartPlay default_pawn=BP_PVPBaseCharacter_C`。

### 5.2 蓝图 CDO 未指蓝图 Pawn，输入全部失效

- **现象**：能出生，但 WASD / 鼠标 / 空格全部无响应，日志出现 `no UPVPInputConfig assigned`，没有 `bound 9/9`。
- **根因**：`BP_PVPGameMode` 的 CDO 仍使用 C++ 默认 `APVPBaseCharacter`（纯原生 CDO 没有 `DA_PVP_InputConfig`），而非挂着 DA 的 `BP_PVPBaseCharacter`。
- **正确做法**：确认 `BP_PVPGameMode` CDO 的 `DefaultPawnClass` 指向 `BP_PVPBaseCharacter`；重建框架后重跑 `setup_pvp_input.py` 确保 DA 挂在该蓝图 CDO 上。
- **验证方式**：`StartPlay default_pawn=BP_PVPBaseCharacter_C`；角色日志 `bound 9/9`。

### 5.3 IMC 重复注入或蓝图上下文被清空

- **现象**：一次按键触发两次（视角灵敏度 / 移动速度翻倍），或蓝图里配的 IMC 在 PIE 后不生效。
- **根因**：`BeginPlay` 先 `ClearAllMappings()` 再按 `DefaultMappingContexts` 注入；若蓝图事件里也 `AddMappingContext` 同一 IMC，事件按上下文数量叠加；反之蓝图后注入的上下文会在下一次 `BeginPlay` 被清掉。
- **正确做法**：IMC 只由 `APVPPlayerController` 注入；需要额外上下文时追加到 `DefaultMappingContexts` 数组（可被蓝图覆盖 / 扩展），不在蓝图事件里另加。表现层不得增删 IMC。
- **验证方式**：日志 `applied 1 default input mapping context(s)`；PIE 中一次按键只触发一次回调；切换 Pawn 后输入仍正常。

### 5.4 远端控制器没有 `applied ... context(s)` 日志

- **现象**：listen server 的第二个客户端（或专用服务器）控制器日志里没有 IMC 数量行，也没有任何输入。
- **根因**：`BeginPlay` 中 `GetLocalPlayer()` 对无本地玩家的控制器返回空，整段注入被跳过；远端客户端本身不持有映射上下文，输入由各自客户端注入。
- **正确做法**：不要为了"日志一致"去掉守卫；联机下每个客户端各自注入自己的控制器上下文。若确实需要在服务器侧处理输入，走 RPC / 技能输入而非本地映射。
- **验证方式**：PIE 双客户端运行时，各端日志各自出现一次 `applied 1 ...`；服务器端无该行属预期。

### 5.5 在控制器里绑定原生动作导致重复触发

- **现象**：WASD / 鼠标响应异常（动作被执行两次）或 `bound 9/9` 日志之外还有一套绑定，行为难以定位。
- **根因**：`APVPBaseCharacter` 已经通过 `UPVPInputComponent` 按 Tag 绑定；若在 `APVPPlayerController::SetupInputComponent` 里再绑定同一 IA，两套绑定都会响应。
- **正确做法**：控制器只负责输入模式与 IMC 注入，`SetupInputComponent` 保持仅 `Super`；技能输入未来走 GAS / Hero 层，也不放控制器。
- **验证方式**：控制器 `SetupInputComponent` 中无 `BindAction` / `BindNativeAction` 调用；PIE 中一次按键一次响应。

### 5.6 移动 / 重命名 IMC 资产后输入全废

- **现象**：编辑器中 IMC 改名或换目录后，PIE 输入全部无响应，日志出现 `no default input mapping context applied (IMC asset missing?), input will not work.`
- **根因**：`APVPPlayerController` 构造函数用 `ConstructorHelpers::FObjectFinder<UInputMappingContext>` 硬引用 `/Game/PVP/Input/IMC_PVP_Default.IMC_PVP_Default`；这是本模块唯一的资产硬引用，路径变更后查找失败，构造函数静默跳过（不崩）。
- **正确做法**：保持 IMC 的名字与目录不变；确需迁移时同步修改 `PVPPlayerControllerDefaults::DefaultInputMappingContextPath`，并检查 `Content/Python/setup_pvp_input.py` 中的 IMC 路径常量，重编译后重跑脚本。
- **验证方式**：日志恢复 `applied 1 default input mapping context(s)`；PIE 中 WASD / 鼠标正常。

---

## 6. 校验、报告与冒烟

### 6.1 脚本校验

```text
UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
    -Script="<project>/Content/Python/setup_pvp_game_framework.py" -unattended -nosplash -nullrhi
UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
    -Script="<project>/Content/Python/verify_pvp_game_framework.py" -unattended -nosplash -nullrhi
```

`verify_pvp_game_framework.py` 的核心断言（硬断言，失败非零退出）：

- 第三方源地图与 `_BuiltData` 哈希未变化（原包不被改写）；
- `BP_PVPBaseCharacter` 存在、旧路径 `/Game/PVPDemo/Characters/...` 已不存在，且父类为 `PVPBaseCharacter`；
- `BP_PVPGameMode` / `BP_PVPPlayerController` 存在；`BP_PVPGameMode` CDO 的 `DefaultPawnClass` 路径包含 `/Game/PVP/Characters/`、`PlayerControllerClass` 路径包含 `/Game/PVP/Game/`；
- `L_PVP_Showcase` 的 Actor 数量守恒、有 `PlayerStart`、无第三方 Actor、无旧角色实例，WorldSettings GameMode Override 为 `BP_PVPGameMode`；
- `/Game/PVP` 与 `/Game/PVPDemo` 内不再残留 `/Game/InfimaGames/` 活跃引用（仅允许已知悬挂软引用）。

报告：`Saved/PVPGameFrameworkReport/verify.json|md`、`report.json|md`。

### 6.2 运行时日志检查

| 日志 | 期望 | 处理 |
| --- | --- | --- |
| `InitGame map=/Game/PVP/Maps/L_PVP_Showcase` | 出现 | 未出现说明关卡没用 `BP_PVPGameMode`（踩坑 1） |
| `StartPlay default_pawn=BP_PVPBaseCharacter_C` | 蓝图类 | 显示 `PVPBaseCharacter` 说明 CDO 未指蓝图（踩坑 2） |
| `OnPossess pawn=... path=/Game/PVP/Maps/...` | Pawn 为蓝图类 | Pawn 路径异常时检查生成逻辑 |
| `applied 1 default input mapping context(s)` | 数字 ≥ 1 | 为 0 / 无该行见踩坑 3、5.6 |
| `BeginPlay (GameOnly input mode)` | 出现 | 未有输出说明控制器未生效 |

### 6.3 PIE 冒烟清单

- 进入 `L_PVP_Showcase` 即出生为 `BP_PVPBaseCharacter`，视角为第一人称，可移动。
- 鼠标光标隐藏、点击不选中场景物体（GameOnly 生效）。
- WASD / 鼠标 / 空格 / LeftShift 均响应且只响应一次（无 IMC 重复）。
- PIE 停止再启动，输入行为一致（`ClearAllMappings` 生效）。
- 若联机测试：客户端各自注入一次 IMC，服务器端无本地注入属预期。

---

## 7. 已知限制与后续路线

- **无对局流程**：GameState / PlayerState / 回合 / 比分 / 胜负 / 重开未实现。
- **无观战与出生规则**：`bStartPlayersAsSpectators=false` 固定；无队伍、出生点选择。
- **无 UI 输入模式**：只有 GameOnly；菜单 / 结算的光标与模式切换未接。
- **无按键重绑定**：未接 Enhanced Input User Settings。
- **无联机验证**：本地玩家守卫为联机预留，但未做实际的 listen server / dedicated 测试。
- **IMC 硬编码路径**：控制器构造函数依赖 `IMC_PVP_Default` 的固定路径（踩坑 5.6），改名必须同步 C++ 与脚本。
- **技能入口未接**：ASC 规划挂在 PlayerState，接入时需在本模块新增初始化点，见 `Docs/GAS/`。

---

## 8. 相关文件

- 源码
  - `Source/PVPDemo/Game/PVPGameMode.h/.cpp`
  - `Source/PVPDemo/Game/PVPPlayerController.h/.cpp`
- 资产
  - `Content/PVP/Game/BP_PVPGameMode`、`BP_PVPPlayerController`
  - `Content/PVP/Maps/L_PVP_Showcase`
  - `Content/PVP/Characters/BP_PVPBaseCharacter`（默认 Pawn）、`Content/PVP/Input/IMC_PVP_Default`
- 配置与工具
  - `Config/DefaultEngine.ini`、`Config/DefaultInput.ini`
  - `Content/Python/setup_pvp_game_framework.py`、`verify_pvp_game_framework.py`
  - `Saved/PVPGameFrameworkReport/`
- 相关文档
  - `Docs/Game/README.md`、`Docs/Game/Game-Integration.md`
  - `Docs/Input/README.md` 第 4 节、`Docs/Input/Input-Dev.md` 踩坑 4
