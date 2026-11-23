# PVP 输入模块导航

> 维度：输入（Enhanced Input / InputTag / InputComponent）
> 模块：Input
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-23
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Input/Input-Dev.md`（开发流程与踩坑）、`Docs/Input/Input-Integration.md`（依赖与接口契约）

本文件是输入模块的固定导航入口。进入本模块的任务先读本文件，再按第 2 节路由按需读取；本文第 3 节是**全模块唯一的动作 / 绑定 / 回调权威表**，其他文档不复制。

---

## 1. 模块定位与边界

**职责**：把物理按键翻译成 `InputTag` 语义，并路由到角色上的处理函数。

- 资产：`IA_PVP_*`（动作定义）、`IMC_PVP_Default`（键位 → IA 映射与修饰器）、`DA_PVP_InputConfig`（Tag → IA 配置）。
- 源码：`Source/PVPDemo/Input/`（配置类型与绑定组件）、`GAS/PVPGameplayTags`（InputTag 声明）。
- 接入点：`APVPPlayerController` 注入 IMC，`APVPBaseCharacter::SetupPlayerInputComponent` 按 Tag 绑定回调。

**非职责**（不属于本模块，不要在这里实现）：

- GAS 技能输入（`InputTag.Ability.*`）与 Ability 绑定。
- 相机模式栈与视角计算（当前由 `CameraFP` 的 `bUsePawnControlRotation` 提供）。
- 动画层输入消费（只读 `bIsRunning` / `ControlRotation`）。
- 按键重绑定 / Enhanced Input User Settings。

---

## 2. 文档清单与路由

| 文档 | 内容 |
| --- | --- |
| `Docs/Input/README.md` | 本导航：定位、路由、已实现能力速查、入口与日志、校验入口、维护规则 |
| `Docs/Input/Input-Dev.md` | 实现细节：源码结构、设计决策、新增动作 SOP、踩坑清单、校验与 PIE 冒烟、已知限制 |
| `Docs/Input/Input-Integration.md` | 契约：模块依赖、数据流、C++ / 资产 / 配置接口、其他模块调用方式、禁止依赖方向、变更影响面 |

**按场景路由**：

| 我要做的事 | 读哪里 |
| --- | --- |
| 确认当前已实现哪些动作、绑到哪个函数 | 本文第 3 节 |
| 新增一个输入动作 | `Input-Dev.md` 第 4 节（SOP） |
| 排查视角上下反向 / 只能左右看 | `Input-Dev.md` 第 5 节踩坑 1、第 6 节排错顺序 |
| 重建 `DA_PVP_InputConfig` 或调整动作顺序 | `Input-Dev.md` 第 5 节踩坑 2 |
| 排查"绑定数为 0 / Cast 失败" | `Input-Dev.md` 第 5 节踩坑 3、第 6 节日志表 |
| 排查输入重复、切换 Pawn 后输入异常 | `Input-Dev.md` 第 5 节踩坑 4 |
| 查 C++ 接口签名（配置、组件、Tag、属性） | `Input-Integration.md` 第 4 节 |
| 让动画 / 相机只读输入结果，或在别的 Pawn 上复用输入 | `Input-Integration.md` 第 5 节 |
| 确认哪些依赖方向被禁止 | `Input-Integration.md` 第 6 节 |
| 改接口前评估影响面 | `Input-Integration.md` 第 7 节 |
| 跑脚本校验 / 看校验报告 | 本文第 6 节 → `Input-Dev.md` 第 6 节 |

---

## 3. 已实现能力速查（权威表）

当前代码已绑定的动作共 **4 个**，共 **6 条绑定事件**：

| 语义 | InputTag | InputAction | ValueType | IMC 键位 | 绑定事件 | 回调函数 | 行为 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 平面移动 | `InputTag.Move` | `IA_PVP_Move` | Axis2D | W / A / S / D | Triggered | `APVPBaseCharacter::Input_Move` | 用 ControlRotation 的 **Yaw** 构造平面基向量，`AddMovementInput` 前后 + 左右；Pitch 不参与，抬头低头不影响移动平面 |
| 视角 | `InputTag.Look` | `IA_PVP_Look` | Axis2D | Mouse2D + **Negate(Y)** | Triggered | `APVPBaseCharacter::Input_Look` | `AddControllerYawInput(X)` + `AddControllerPitchInput(Y)`，C++ 不取反；相机通过 `bUsePawnControlRotation` 消费 Controller 旋转 |
| 跳跃 | `InputTag.Jump` | `IA_PVP_Jump` | Boolean | SpaceBar | Started / Completed | `Input_JumpStarted` / `Input_JumpCompleted` | `Jump()` / `StopJumping()` |
| 按住跑 | `InputTag.Run` | `IA_PVP_Run` | Boolean | LeftShift | Started / Completed | `Input_RunStarted` / `Input_RunCompleted` | `SetRunning` 切换 `MaxWalkSpeed`（`WalkSpeed` 300 / `RunSpeed` 600，可调）并维护 `bIsRunning` |

**代码位置**：

| 项 | 位置 |
| --- | --- |
| Tag 定义 | `Source/PVPDemo/GAS/PVPGameplayTags.h/.cpp`（`PVPGameplayTags::InputTag_Move/Look/Jump/Run`） |
| 绑定组件 | `Source/PVPDemo/Input/PVPInputComponent.h`（`UPVPInputComponent::BindNativeAction`） |
| 配置类型 | `Source/PVPDemo/Input/PVPInputConfig.h/.cpp`（`UPVPInputConfig::FindNativeInputActionForTag` / `SetNativeInputActionByIndex`） |
| 绑定点 | `APVPBaseCharacter::SetupPlayerInputComponent`（`Source/PVPDemo/Character/PVPBaseCharacter.cpp`） |
| 回调实现 | 同文件 `Input_Move` / `Input_Look` / `Input_JumpStarted` / `Input_JumpCompleted` / `Input_RunStarted` / `Input_RunCompleted` / `SetRunning` |
| InputComponent 类型覆盖 | `APVPBaseCharacter` 构造函数：`OverrideInputComponentClass = UPVPInputComponent::StaticClass()` |
| IMC 注入 | `APVPPlayerController` 构造函数 + `BeginPlay`（`Source/PVPDemo/Game/PVPPlayerController.cpp`） |

**未绑定的输入动作**：`IA_PVP_Fire`、`IA_PVP_ADS`、`IA_PVP_Crouch`、`IA_PVP_Sprint` 等其余 IA 已在 `IMC_PVP_Default` 中有键位映射，但 `APVPBaseCharacter` 未绑定，属于 GAS / 武器 / 相机等后续阶段。

---

## 4. 启动与运行时入口

| 阶段 | 执行者 | 行为 |
| --- | --- | --- |
| 构造函数 | `APVPBaseCharacter` | 指定 `OverrideInputComponentClass`，初始 `MaxWalkSpeed = WalkSpeed`，`bIsRunning = false` |
| 构造函数 | `APVPPlayerController` | `ConstructorHelpers::FObjectFinder` 加载 `/Game/PVP/Input/IMC_PVP_Default.IMC_PVP_Default`，填入 `DefaultMappingContexts`（Priority 0）；缺失只告警不崩 |
| `BeginPlay` | `APVPPlayerController` | `SetInputMode(FInputModeGameOnly())` → 取 `UEnhancedInputLocalPlayerSubsystem` → `ClearAllMappings()` → 按优先级 `AddMappingContext` |
| Possess 后 | `APVPBaseCharacter` | `SetupPlayerInputComponent` 中按 Tag 绑定 4 个动作 / 6 个事件；绑定只执行一次 |

**关键日志**：

```text
LogPVPBaseCharacter: ... bound 6/6 native input bindings (config=DA_PVP_InputConfig)
LogPVPPlayerController: ... applied 1 default input mapping context(s)
```

失败信号与处理见 `Input-Dev.md` 第 6 节日志表。

---

## 5. 未实现能力与扩展方向

- 其余 `IA_PVP_*`（Fire / ADS / Crouch / Sprint 等）未在角色上绑定，留给 GAS / 武器 / 相机阶段。
- GAS 技能输入尚未设计，建议扩展为 `InputTag.Ability.<Skill>`，不要复用原生动作 Tag。
- 未接 Enhanced Input User Settings / 按键重绑定，`DefaultMappingContexts` 未注册到用户设置。
- 当前 IMC 只配置键鼠，无 Gamepad 映射。
- Run 的 `MaxWalkSpeed` 只在本地 / 自主代理应用，网络权威速度未接（代码内有 `TODO(Network/GAS)`）。
- 相机模式栈、1P/3P 切换、程序化相机效果未实现；`bUsePawnControlRotation` 是相机层落地前的简单方案。

---

## 6. 校验与报告入口

- 校验脚本：`Content/Python/setup_pvp_input.py`（校验 IA 类型、IMC 键位与 `IA_PVP_Look` 的 Negate(Y)、幂等重建 `DA_PVP_InputConfig` 并挂到 `BP_PVPBaseCharacter` CDO）。
- 报告产物：`Saved/PVPInputReport/report.json`、`Saved/PVPInputReport/report.md`。
- 运行命令与报告关键项：`Input-Dev.md` 第 6 节。
- PIE 冒烟清单：`Input-Dev.md` 第 6 节。

---

## 7. 文档维护规则

| 变更 | 必须同步 |
| --- | --- |
| 新增 / 删除 / 重新绑定动作，或修改键位、ValueType、回调函数 | 本文第 3 节 + `Input-Integration.md` 第 4 / 5 节 + `Input-Dev.md` 第 4 节 SOP |
| 修改对外接口签名、依赖方向、资产契约、配置项 | `Input-Integration.md` 第 2 / 4 / 6 / 7 节；本文第 3 节「代码位置」 |
| 出现新的坑点或排错步骤 | `Input-Dev.md` 第 5 节（四要素齐全） |
| 新增本模块的维护文档 | 本文第 2 节文档清单登记，命名 `Input-<主题>.md` |
| 任何改动 | 三份文档文首「最后更新」刷新为当天日期 |

- 易变数字（资产总数、文件数）只写在 `Docs/Content-Structure.md`，本文不复制。
- 本模块文档与代码不一致时，以代码为准并当轮修正文档。
