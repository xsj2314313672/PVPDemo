# PVP 输入模块开发文档

> 维度：输入（Enhanced Input / InputTag / InputComponent）
> 模块：Input
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Input/README.md`（导航与动作速查）、`Docs/Input/Input-Integration.md`（依赖与接口契约）

本文只写**怎么实现、怎么开发、踩过什么坑**。已实现动作、键位、回调函数的权威表在 `Docs/Input/README.md` 第 3 节，本文不复制。

---

## 1. 目的与范围

覆盖范围：

- Enhanced Input 资产（`IA_PVP_*` / `IMC_PVP_Default` / `DA_PVP_InputConfig`）的创建与维护流程；
- `Source/PVPDemo/Input/`、`GAS/PVPGameplayTags`、`Character/PVPBaseCharacter`、`Game/PVPPlayerController` 的输入代码；
- 新增输入动作的标准步骤；
- 输入相关踩坑、校验、报告与 PIE 冒烟。

不覆盖：GAS 技能输入、相机模式栈、动画层实现、按键重绑定。以上属于各自模块文档。

当前范围只覆盖角色原生动作 **Move / Look / Jump / Run**。

---

## 2. 当前状态与源码结构

### 2.1 当前状态

- 输入方式：Enhanced Input。
- 默认映射上下文：`/Game/PVP/Input/IMC_PVP_Default`，由 `APVPPlayerController` 在代码中加载并注入，蓝图无需手动添加。
- 输入配置：`/Game/PVP/Input/DA_PVP_InputConfig`，挂在 `BP_PVPBaseCharacter` 的 CDO。
- 原生绑定：`APVPBaseCharacter::SetupPlayerInputComponent`，通过 `UPVPInputComponent::BindNativeAction` 按 Tag 绑定 6 个动作、共 9 个事件（Jump / Run / Aim 各 Started + Completed，Fire 只绑 Started）。
- 相机：`CameraFP` 使用 `bUsePawnControlRotation=true`，视角旋转来自 Controller，位置跟随 `head` 骨骼。

### 2.2 源码结构

| 文件 | 职责 |
| --- | --- |
| `Source/PVPDemo/Input/PVPInputConfig.h/.cpp` | `FPVPInputAction`（InputAction + InputTag）、`UPVPInputConfig`（`NativeInputActions`、`FindNativeInputActionForTag`、`SetNativeInputActionByIndex`、`EnsureNativeInputActionSlots`）、`FPVPInputMappingContextAndPriority`（IMC + Priority）；构造函数预置 6 个 Tag 槽位，迁移助手给旧资产补槽 |
| `Source/PVPDemo/Input/PVPInputComponent.h/.cpp` | `UEnhancedInputComponent` 子类；`BindNativeAction(Config, Tag, TriggerEvent, Object, Func)` 按 Tag 查 Action 并绑定，找不到返回 `nullptr` |
| `Source/PVPDemo/GAS/PVPGameplayTags.h/.cpp` | 原生 InputTag 声明与定义：`InputTag.Move/Look/Jump/Run/Aim/Fire` |
| `Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp` | `OverrideInputComponentClass = UPVPInputComponent`；`InputConfig`、`WalkSpeed/RunSpeed`、`bIsRunning`；`SetupPlayerInputComponent` 绑定 6 个动作；Move/Look/Jump/Run/Aim/Fire 处理函数 |
| `Source/PVPDemo/Game/PVPPlayerController.h/.cpp` | `DefaultMappingContexts`（代码默认 IMC）；`BeginPlay` 通过 `UEnhancedInputLocalPlayerSubsystem` 注入 IMC |

### 2.3 本模块涉及的资产

| 资产 | 角色 | 准备方式 |
| --- | --- | --- |
| `IA_PVP_<动作>` | 动作定义（ValueType：Axis2D / Axis1D / Boolean） | 编辑器创建，或由 `setup_pvp_input.py` 校验 / 修正 |
| `IMC_PVP_Default` | 键位 → IA 映射与修饰器（Look 的 Y Negate 在此） | 编辑器维护；脚本校验键位与修饰器 |
| `DA_PVP_InputConfig` | Tag → IA 配置，挂 `BP_PVPBaseCharacter` CDO | `setup_pvp_input.py` 幂等创建并按槽位写入 |

- 资产数量统计只看 `Docs/Content-Structure.md`，本文不复制总数。
- `.uasset` 只能由 UE 编辑器或 `Content/Python/` 脚本产出，禁止手写 / 伪造。

---

## 3. 设计决策与理由

1. **Tag 表达语义，按键只存在于 IMC**。C++ 不出现键名，只按 `InputTag.*` 绑定；换键只改 IMC，不用重编译。
2. **数据驱动**。动作 → InputAction 的映射放在 `DA_PVP_InputConfig`，新增动作优先扩展配置，不改绑定流程。
3. **单向数据流**。`Input → Character/GAS → Animation/Camera`；表现层只读逻辑状态，不反向修改输入或角色状态。
4. **输入归属清晰**。原生动作绑定在 Pawn；映射上下文注入在 PlayerController；表现层不添加 / 移除 IMC。
5. **一处取反**。Look 的 Y 轴极性只在 IMC 修正，C++ 保持 `AddControllerPitchInput(LookAxisVector.Y)` 不取反（见第 5 节踩坑 1）。
6. **用 Pawn 钩子而非全局配置指定输入组件**。`APawn::OverrideInputComponentClass` 只影响本角色；改 `Config/DefaultInput.ini` 的 `DefaultInputComponentClass` 会影响所有 Pawn，且与"谁需要 Tag 绑定能力"解耦不了（见踩坑 3）。
7. **默认 IMC 注入放 PlayerController**。默认上下文属于"玩家入口"的职责，保证任何 Pawn（含后续观战 / 载具）进入时都有一致的输入基座；Pawn 只负责自己的动作绑定。
8. **绑定只执行一次，不引入 Tick**。Move / Look 的处理是 O(1) 计算，Run 只在状态切换时写一次 `MaxWalkSpeed`。

---

## 4. 开发流程 SOP（新增输入动作）

1. **创建 IA**：`IA_PVP_<动作>`，按语义选择 ValueType（Axis2D / Boolean / Axis1D），放在 `Content/PVP/Input/`。
2. **配置 IMC**：在 `IMC_PVP_Default` 添加按键映射；需要轴修正时加 Modifier（Look 的 Y Negate 见踩坑 1）。
3. **声明 Tag**：在 `GAS/PVPGameplayTags.h/.cpp` 增加 `InputTag_<动作>` 声明与定义，只加实际被消费的 Tag。
4. **扩展 DA**：在 `DA_PVP_InputConfig` 增加 Tag → IA 条目；走脚本时用 `SetNativeInputActionByIndex` 或扩展 `setup_pvp_input.py`。改槽位顺序必须同时改 C++ 构造函数 / `EnsureNativeInputActionSlots` 与脚本（见踩坑 2、5.5）。
5. **绑定逻辑**：原生动作在 `APVPBaseCharacter::SetupPlayerInputComponent` 绑定；技能输入在 GAS / Hero 层绑定；保持 `Input → Character/GAS` 方向。
6. **日志与校验**：确认绑定数量日志正确，跑 `setup_pvp_input.py`，在 PIE 中做冒烟检查。
7. **同步文档**（缺一视为任务未完成）：
   - `Docs/Input/README.md` 第 3 节动作表与「代码位置」；
   - `Docs/Input/Input-Integration.md` 第 4 / 5 节接口与调用方式；
   - 本文第 4 节（若流程本身变化）；
   - `Docs/Content-Structure.md` 的资产数量与说明（数量按实际统计）；
   - 新增脚本或产物时同步 `AGENTS.md` 文档地图。

---

## 5. 踩坑清单

### 5.1 视角上下反向（Look 俯仰极性）

- **现象**：切换 `CameraFP->bUsePawnControlRotation = true` 后，左右视角正常，但上下方向相反（鼠标上移、视角下移）；或表现为"只能左右看"。
- **根因**：该相机路径下 Controller Pitch 的实际方向与鼠标 Y 的正方向约定相反；`Input_Look` 直传 `AddControllerPitchInput(LookAxisVector.Y)` 时不会自动纠正。
- **正确做法**：
  - 在 `IMC_PVP_Default` 的 `IA_PVP_Look` → `Mouse2D` 映射上添加 `InputModifierNegate`，只勾选 **Y**（`X=false, Y=true, Z=false`）；
  - C++ 侧保持 `AddControllerPitchInput(LookAxisVector.Y)` **不取反**，**只在一处取反**；
  - 重建 / 复制 IMC（例如重新导入原包资产）后必须重新确认该修饰器；
  - 若未来改为代码取反，必须同时移除 IMC 的 Negate(Y)，并更新本文与 `setup_pvp_input.py` 的校验逻辑，不允许两处同时取反。
- **验证方式**：
  - `Saved/PVPInputReport/report.md` 中 `IA_PVP_Look Negate(Y)` 为 `yes`；
  - PIE 四向检查：左 / 右 / 上 / 下方向与直觉一致；
  - C++ 中不存在额外的 `-LookAxisVector.Y`；
  - IMC 中 `IA_PVP_Look` 无重复 Negate 映射。

### 5.2 DA 槽位顺序错位

- **现象**：脚本写入后 `DA_PVP_InputConfig` 的 Tag 与 IA 不匹配；绑定数量仍是 `9/9`，但按键触发的行为错位（例如按空格跑）。
- **根因**：GameplayTag 无法从 Python 构造，`UPVPInputConfig` 构造函数 / `EnsureNativeInputActionSlots` 按 Move / Look / Jump / Run / Aim / Fire 预置槽位，`setup_pvp_input.py` 通过 `SetNativeInputActionByIndex` 按索引写入；两处顺序不一致时脚本按索引写错 IA。
- **正确做法**：
  - C++ 构造函数槽位顺序与脚本 `REQUIRED_ACTIONS` 顺序必须逐项一致；
  - 调整顺序时同时改 C++ 与脚本，重编译后重跑脚本重建 DA；
  - 不要在编辑器里手工调换槽位顺序而不改代码，下一次跑脚本会被覆盖。
- **验证方式**：报告中的槽位与 6 个动作一一对应；PIE 中 6 个动作行为正确；日志仍为 `bound 9/9`。

### 5.3 输入组件类型不符，绑定被跳过

- **现象**：日志出现 `<pawn>: input component is UEnhancedInputComponent, expected UPVPInputComponent; native bindings skipped.`，所有原生动作无响应。
- **根因**：Pawn 实际拿到的输入组件不是 `UPVPInputComponent`。常见来源是试图通过 `Config/DefaultInput.ini` 的 `DefaultInputComponentClass` 全局替换组件类型，或构造函数中的 `OverrideInputComponentClass` 未生效（未重编译）。
- **正确做法**：
  - 在 `APVPBaseCharacter` 构造函数设置 `OverrideInputComponentClass = UPVPInputComponent::StaticClass()`（UE 5.9 的 `APawn` 钩子）；
  - `DefaultInput.ini` 的 `DefaultInputComponentClass` 保持 `/Script/EnhancedInput.EnhancedInputComponent`，不为本角色改全局项；
  - 修改后重新编译，确认蓝图没有把 Pawn 换成别的输入组件配置。
- **验证方式**：不再出现该警告；日志 `bound 9/9 native input bindings (config=DA_PVP_InputConfig)`；WASD / 鼠标 / 空格 / LeftShift / 右键 / 左键均有响应。

### 5.4 IMC 重复注入

- **现象**：同一按键触发两次（移动速度或视角灵敏度翻倍）；或蓝图里配置了 IMC，但切换 Pawn / 重新 Possess 后输入失效。
- **根因**：`APVPPlayerController::BeginPlay` 先 `ClearAllMappings()` 再 `AddMappingContext`；若蓝图或其他系统也 `AddMappingContext`，同一 IMC 被重复添加，输入事件按上下文数量叠加。反过来，蓝图注入的上下文会被 `ClearAllMappings()` 清掉。
- **正确做法**：
  - IMC 只由 `APVPPlayerController` 注入，蓝图不手动 `AddMappingContext`；
  - 需要额外上下文时追加到 `DefaultMappingContexts` 数组（可被蓝图覆盖 / 扩展），而不是在蓝图事件里另行添加；
  - 表现层（Animation / Camera）不得增删 IMC。
- **验证方式**：日志 `applied 1 default input mapping context(s)`；PIE 中一次按键只触发一次回调（可临时加 Verbose 日志观察）；切换 Pawn 后输入仍正常。

### 5.5 旧 `DA_PVP_InputConfig` 只有 4 个 Tag 槽，新动作写不进去

- **现象**：`setup_pvp_input.py` 报 `has 4 native input action slots, expected 6`，Aim / Fire 未写入；或直接重建 DA 后 `BP_PVPBaseCharacter` CDO 指向空。
- **根因**：`NativeInputActions` 是序列化数组；资产在 Aim / Fire 之前保存后不会重新执行 C++ 构造函数，改构造函数只影响新建资产。重建 DA 又会替换被 BP CDO 引用的对象。
- **正确做法**：`setup_pvp_input.py` 先调用 `UPVPInputConfig::EnsureNativeInputActionSlots()` 原地补齐缺失 Tag 槽（顺序 Move / Look / Jump / Run / Aim / Fire），再按索引写 `InputAction`；不要删除 DA。
- **验证方式**：报告 `native tag slots migrated: 2`（首次升级）或 `0`（已迁移），`Required actions` 表 6 行且 `Errors` 为空；PIE 日志 `bound 9/9`。

---

## 6. 校验、报告与冒烟

### 6.1 脚本校验

```text
UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
    -Script="<project>/Content/Python/setup_pvp_input.py" -unattended -nosplash -nullrhi
```

`setup_pvp_input.py` 会：

- 校验 6 个 IA 存在且 ValueType 正确（Move / Look = Axis2D，Jump / Run / Aim / Fire = Boolean），必要时修正自有副本；
- 读取 `IMC_PVP_Default` 的映射，输出每个动作的按键；
- **校验 `IA_PVP_Look` 的 Negate(Y) 修饰器**，缺失时报错；
- 幂等创建 / 复用 `DA_PVP_InputConfig`，先调用 `EnsureNativeInputActionSlots` 补齐槽位，再按槽位写入 6 个 IA；
- 把 DA 赋给 `BP_PVPBaseCharacter` CDO 并编译保存；
- 输出 `Saved/PVPInputReport/report.json` 与 `report.md`。

报告关键项：`IA_PVP_Look Negate(Y): yes`、`native tag slots migrated: 2`（首次升级）、6 个动作的 ValueType / 按键、`Errors` 为空。

### 6.2 运行时日志

进入 `L_PVP_Showcase` 后应看到：

```text
LogPVPBaseCharacter: ... bound 9/9 native input bindings (config=DA_PVP_InputConfig)
LogPVPPlayerController: ... applied 1 default input mapping context(s)
```

| 日志 | 含义 | 处理 |
| --- | --- | --- |
| `input component is ..., expected UPVPInputComponent` | `OverrideInputComponentClass` 未生效 | 检查构造函数、重新编译（踩坑 3） |
| `no UPVPInputConfig assigned` | BP CDO 未挂 DA | 重跑 `setup_pvp_input.py` |
| `no default input mapping context applied` | IMC 缺失或未添加 | 检查 `/Game/PVP/Input/IMC_PVP_Default` 与 PlayerController 代码 |
| `bound N/9` 且 N < 9 | `DA_PVP_InputConfig` 槽位缺 IA 或旧资产只有 4 槽 | 重跑脚本，核对槽位顺序（踩坑 2、5.5） |

### 6.3 视角异常排错顺序

出现"只能左右看"或"上下相反"时按序检查：

1. `Saved/PVPInputReport/report.md` 中 `IA_PVP_Look Negate(Y)` 是否为 `yes`。
2. `CameraFP->bUsePawnControlRotation` 是否为 `true`（当前按方案 A）。
3. C++ 的 `AddControllerPitchInput` 参数是否被额外取反。
4. IMC 的 `IA_PVP_Look` 是否被其他映射覆盖，或存在重复 Negate。
5. 仍异常时，临时在 `Input_Look` 打印 `LookAxisVector.Y` 与 `Controller->GetControlRotation().Pitch`，区分"输入没到"与"相机没消费"。

### 6.4 PIE 冒烟清单

- WASD：前后左右移动，方向随视角。
- 鼠标：左右转视角；**上抬看天、下压看地**（检查 Negate(Y)）。
- 空格：起跳；松开停止继续上升。
- LeftShift：按住切跑、松开回走；`bIsRunning` 状态正确。
- 右键：空手无反应；拾取武器后按住进入瞄准（FOV 变化与姿态见 `Docs/Character/README.md`），松开退出。
- 左键：空手无反应；有武器时播放一次成对射击动画（腰射 / 瞄准版见 `Docs/Equipment/README.md`）。
- 抬头 / 低头时移动平面不倾斜（Move 只用 Yaw）。
- 一次按键只触发一次回调（无 IMC 重复注入）。

---

## 7. 已知限制与后续路线

- Run 的 `MaxWalkSpeed` 只在本地 / 自主代理应用，**网络权威速度未接**；待 GAS 或 `PVPCharacterMovementComponent` 接管（代码内有 `TODO(Network/GAS)`）。
- 其余 `IA_PVP_*`（Sprint、Crouch、Reload、Melee 等）未绑定，属于后续阶段；Aim / Fire 已绑定。
- 未接 Enhanced Input User Settings / 按键重绑定；`DefaultMappingContexts` 暂未注册到用户设置。
- 当前 IMC 只配置键鼠，无 Gamepad 映射。
- 相机模式栈、1P/3P 切换、程序化相机效果均未实现。
- Look 的 Y 轴 Negate 是当前相机管线的约定；更换相机管线时必须重新评估（踩坑 1）。

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
- 工具与报告
  - `Content/Python/setup_pvp_input.py`
  - `Saved/PVPInputReport/report.json|md`
