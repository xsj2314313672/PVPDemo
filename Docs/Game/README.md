# PVP 会话与玩家入口模块导航

> 维度：会话与玩家入口（GameMode / PlayerController / 默认 IMC 注入）
> 模块：Game
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Game/Game-Dev.md`（开发流程与踩坑）、`Docs/Game/Game-Integration.md`（依赖与接口契约）

本文件是 Game 模块的固定导航入口。进入本模块的任务先读本文件，再按第 2 节路由按需读取；本文第 3 节是**入口装配 / 控制器参数 / 运行时入口的唯一权威表**，其他文档不复制。

---

## 1. 模块定位与边界

**职责**：回答"一局怎么开始"与"玩家如何进入世界"。

- 会话装配：默认 Pawn、默认玩家控制器、出生即操控（非观战）。
- 玩家入口：输入模式、默认映射上下文注入、附身记录、对当前角色的只读访问器。
- 入口日志：`InitGame` / `StartPlay` / `BeginPlay` / `OnPossess` 的可观测点。

**非职责**（不属于本模块，不要在这里实现）：

- 角色本体与移动：属于 `Character/`，见 `Docs/Character/`。
- 原生动作绑定：绑定点在 Pawn（`APVPBaseCharacter::SetupPlayerInputComponent`），控制器的 `SetupInputComponent` 只调用 `Super`。
- 输入语义、键位与 IA 资产：属于 `Input/`，权威表在 `Docs/Input/README.md` 第 3 节。
- 对局流程：GameState / PlayerState / 比分 / 回合 / 出生点选择均未实现。
- 技能与属性：属于 `GAS/`，见 `Docs/GAS/`。

---

## 2. 文档清单与路由

| 文档 | 内容 |
| --- | --- |
| `Docs/Game/README.md` | 本导航：定位、路由、入口装配速查、启动时序与日志、未实现能力、校验入口、维护规则 |
| `Docs/Game/Game-Dev.md` | 实现细节：源码结构、设计决策、装配 SOP、踩坑清单、校验与 PIE 冒烟、已知限制 |
| `Docs/Game/Game-Integration.md` | 契约：模块依赖、数据流与时序、C++ / 资产接口、其他模块调用方式、禁止依赖方向、变更影响面 |

**按场景路由**：

| 我要做的事 | 读哪里 |
| --- | --- |
| 查默认 Pawn / 控制器、输入模式、IMC 注入与附身入口 | 本文第 3 节 |
| 查输入动作、键位与回调函数 | `Docs/Input/README.md` 第 3 节 |
| 新增 / 替换默认 Pawn 或玩家控制器 | `Game-Dev.md` 第 4 节（SOP） |
| 排查"进关卡没角色 / 输入无响应" | `Game-Dev.md` 第 5 节踩坑 1、2 |
| 排查"按键触发两次 / 换 Pawn 后输入失效" | `Game-Dev.md` 第 5 节踩坑 3；`Docs/Input/Input-Dev.md` 踩坑 4 |
| 排查"客户端没有输入 / 日志缺 applied" | `Game-Dev.md` 第 5 节踩坑 4 |
| 查 C++ 接口签名与蓝图资产契约 | `Game-Integration.md` 第 4 节 |
| 查启动时序（InitGame → StartPlay → Possess → 绑定） | `Game-Integration.md` 第 3 / 5 节 |
| 确认哪些依赖方向被禁止 | `Game-Integration.md` 第 6 节 |
| 改接口 / 默认类前评估影响面 | `Game-Integration.md` 第 7 节 |
| 重建或校验框架资产与地图 | 本文第 6 节 → `Game-Dev.md` 第 6 节 |

---

## 3. 已实现能力速查（权威表）

### 3.1 会话装配

| 项 | C++ 默认（`APVPGameMode` 构造） | 蓝图覆盖（`BP_PVPGameMode` CDO） | 说明 |
| --- | --- | --- | --- |
| `PlayerControllerClass` | `APVPPlayerController` | `BP_PVPPlayerController` | 原生默认保证空内容工程也可玩 |
| `DefaultPawnClass` | `APVPBaseCharacter` | `BP_PVPBaseCharacter` | 蓝图需承担 CDO 的 `DA_PVP_InputConfig` 挂接 |
| `bStartPlayersAsSpectators` | `false` | 未改写（脚本不设置该字段） | 出生即操控 Pawn，无观战 / 等待状态 |
| 出生点 | 关卡内 `PlayerStart` | — | 地图侧职责，见 `L_PVP_Showcase` |
| 覆写点 | `InitGame` / `StartPlay` | — | 仅 `Super` + 日志，无规则逻辑 |
| 硬编码蓝图路径 | **无** | — | C++ 只设原生类，蓝图装配留给 CDO（见 `Game-Dev.md` 设计决策 2） |

### 3.2 玩家控制器参数与入口

| 项 | 当前值 / 行为 | 说明 |
| --- | --- | --- |
| `bShowMouseCursor` | `false` | FPS 视角，隐藏光标 |
| `bEnableClickEvents` / `bEnableMouseOverEvents` | `false` / `false` | 不做鼠标点击与悬停检测 |
| 输入模式 | `BeginPlay` 中 `SetInputMode(FInputModeGameOnly())` | 输入聚焦世界，UI 独占模式未接 |
| `DefaultMappingContexts` | 构造期由 `ConstructorHelpers` 注入 `IMC_PVP_Default`（Priority `0`） | `EditDefaultsOnly`，蓝图可替换 / 追加 |
| IMC 应用时机 | `BeginPlay`：`GetLocalPlayer()` 守卫 → `ClearAllMappings()` → 按 `Priority` 逐个 `AddMappingContext` | 无本地玩家时整段跳过（不告警、不崩） |
| 绑定职责 | `SetupInputComponent` 仅 `Super` | 原生动作绑定在 Pawn 上，避免两处抢绑定 |
| 附身记录 | `OnPossess` 打印 Pawn 类名与包路径 | 用于确认实际使用的 Pawn |
| 角色访问器 | `GetPVPCharacter()`（`BlueprintPure`） | `Cast<APVPBaseCharacter>(GetPawn())`，失败返回 `nullptr` |

### 3.3 启动时序

| 顺序 | 阶段 | 执行者 | 关键动作 |
| --- | --- | --- | --- |
| 1 | 地图加载 | 引擎 | `L_PVP_Showcase` 的 WorldSettings 指定 `BP_PVPGameMode` |
| 2 | `InitGame` | `APVPGameMode` | 调用 `Super` + 日志 `InitGame map=... options=...` |
| 3 | `StartPlay` | `APVPGameMode` | 调用 `Super` + 日志 `StartPlay default_pawn=... player_controller=...` |
| 4 | 生成玩家 | 引擎 | `PlayerControllerClass` / `DefaultPawnClass` 依 CDO 生效 |
| 5 | `OnPossess` | `APVPPlayerController` | 记录实际 Pawn 类名与路径 |
| 6 | 控制器 `BeginPlay` | `APVPPlayerController` | GameOnly 输入模式 → 清空并注入默认 IMC |
| 7 | `SetupPlayerInputComponent` | `APVPBaseCharacter` | 按 Tag 绑定 6 个动作 / 9 条事件（见 `Docs/Character/README.md` 第 3.3 节） |

---

## 4. 启动与运行时入口

**关键日志**：

```text
LogPVPGameMode: BP_PVPGameMode_C_0: InitGame map=/Game/PVP/Maps/L_PVP_Showcase options=
LogPVPGameMode: BP_PVPGameMode_C_0: StartPlay default_pawn=BP_PVPBaseCharacter_C player_controller=BP_PVPPlayerController_C
LogPVPPlayerController: BP_PVPPlayerController_C_0: OnPossess pawn=BP_PVPBaseCharacter_C path=/Game/PVP/Maps/L_PVP_Showcase...
LogPVPPlayerController: BP_PVPPlayerController_C_0: applied 1 default input mapping context(s)
LogPVPPlayerController: BP_PVPPlayerController_C_0: BeginPlay (GameOnly input mode)
LogPVPBaseCharacter: BP_PVPBaseCharacter_C_0: bound 9/9 native input bindings (config=DA_PVP_InputConfig)
```

| 日志 | 含义 |
| --- | --- |
| `StartPlay default_pawn=...` 为原生类名 | `BP_PVPGameMode` 的 CDO 未指向 `BP_PVPBaseCharacter`（走 C++ 默认） |
| `no default input mapping context applied` | `DefaultMappingContexts` 为空或 IMC 资产缺失 |
| 完全没有 `LogPVPPlayerController` 输出 | 当前关卡未使用 `BP_PVPGameMode`（见 `Game-Dev.md` 踩坑 1） |
| 没有 `applied ... context(s)` 但有其他控制器日志 | 无本地玩家（专用服务器 / listen server 远端控制器），属预期 |

失败信号与排错顺序见 `Game-Dev.md` 第 5 / 6 节。

---

## 5. 未实现能力与扩展方向

- **对局流程**：GameState / PlayerState / 回合 / 比分 / 胜负判定 / 重开均未实现。
- **观战与出生规则**：`bStartPlayersAsSpectators` 固定 `false`；无队伍、无出生点选择、无 `ChoosePlayerStart` 覆写。
- **UI 输入模式**：只有 `FInputModeGameOnly`；菜单 / 结算的 `UIOnly` / `GameAndUI` 切换与光标策略未接。
- **按键重绑定**：未注册 Enhanced Input User Settings，`DefaultMappingContexts` 未进用户设置。
- **网络**：无 `GameSession` / 联机流程 / 断线重连；`BeginPlay` 的本地玩家守卫为联机预留，但未验证。
- **暂停 / 观战相机 / 死亡处理**：均无。
- **技能系统入口**：ASC 尚未接入，未来 `.Build.cs` 与初始化位置（PlayerState）见 `Docs/GAS/`。

---

## 6. 校验与报告入口

| 入口 | 作用 | 产物 |
| --- | --- | --- |
| `Content/Python/setup_pvp_game_framework.py` | 幂等创建 / 修正 `BP_PVPGameMode`、`BP_PVPPlayerController`、`L_PVP_Showcase` 与自包含引用图 | `Saved/PVPGameFrameworkReport/report.json`、`report.md` |
| `Content/Python/verify_pvp_game_framework.py` | 硬断言校验：地图哈希、蓝图存在性与父类、CDO 默认类、WorldSettings 指向、第三方引用与悬挂引用 | `Saved/PVPGameFrameworkReport/verify.json`、`verify.md`（失败非零退出） |
| `Content/Python/setup_pvp_input.py` | 校验 IA / IMC 并挂 `DA_PVP_InputConfig`（控制器的注入目标） | `Saved/PVPInputReport/report.md` |
| PIE 冒烟 | 出生、输入、附身、双端（若联机）检查 | 清单见 `Game-Dev.md` 第 6 节 |

运行命令与报告关键项见 `Game-Dev.md` 第 6 节。

---

## 7. 文档维护规则

| 变更 | 必须同步 |
| --- | --- |
| 默认 Pawn / 玩家控制器类变化 | 本文第 3.1 / 3.3 节 + `Game-Integration.md` 第 4.2 / 5 节 + `BP_PVPGameMode` CDO + `Docs/Character/` 与 `Docs/Input/` 的装配描述 |
| 输入模式、IMC 注入时机或 `DefaultMappingContexts` 契约变化 | 本文第 3.2 节 + `Game-Integration.md` 第 3 / 4 / 7 节 + `Docs/Input/README.md` 第 4 节 |
| 新增覆写点（如对局流程、出生规则） | 本文第 3.1 / 5 节 + `Game-Dev.md` 第 3 / 4 节 + `Docs/Source-Structure.md` 2.7 节 |
| 对外接口签名、依赖方向、资产契约变化 | `Game-Integration.md` 第 2 / 4 / 6 / 7 节 |
| 出现新的坑点或校验步骤 | `Game-Dev.md` 第 5 / 6 节（踩坑四要素齐全） |
| 新增本模块维护文档 | 本文第 2 节文档清单登记，命名 `Game-<主题>.md` |
| 任何改动 | 三份文档文首「最后更新」刷新为当天日期 |

- 动作 / 键位 / 回调的权威表只存在于 `Docs/Input/README.md` 第 3 节；本文只写入口装配与注入，不复制键位。
- 资产总数、文件数只写在 `Docs/Content-Structure.md` / `Docs/Source-Structure.md`，本文不复制。
- 本模块文档与代码不一致时，以代码为准并当轮修正文档。
