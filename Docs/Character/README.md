# PVP 角色模块导航

> 维度：角色（Character / 移动状态 / 1P 相机内置）
> 模块：Character
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-23
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Character/Character-Dev.md`（开发流程与踩坑）、`Docs/Character/Character-Integration.md`（依赖与接口契约）

本文件是角色模块的固定导航入口。进入本模块的任务先读本文件，再按第 2 节路由按需读取；本文第 3 节是**角色组件默认值 / 移动状态 / 输入消费入口的唯一权威表**，其他文档不复制。

---

## 1. 模块定位与边界

**职责**：玩家角色的本体积木与第一人称视图载体。

- 角色实体的组件装配：胶囊、`CharacterMesh0` 相对变换、`CameraFP`、动画蓝图。
- 移动状态：走 / 跑速度切换（`WalkSpeed` / `RunSpeed` / `bIsRunning`）与 `MaxWalkSpeed` 应用时机。
- 原生输入的处理函数实现：Move / Look / Jump / Run（绑定关系与键位的权威表在 `Docs/Input/README.md` 第 3 节）。
- 为表现层提供只读状态：`bIsRunning`、`ControlRotation`、`CameraFP`。

**非职责**（不属于本模块，不要在这里实现）：

- 输入语义与键位：`InputTag.*` → `IA_PVP_*` → 绑定事件由 `Input/` 与 `BP_PVPBaseCharacter` CDO 的 `DA_PVP_InputConfig` 承担。
- 默认映射上下文注入：由 `APVPPlayerController` 负责，见 `Docs/Game/`。
- 动画实现：AnimInstance、AimOffset、程序化动画属于 `Animation/`；当前已落地 `UPVPAnimInstanceBase` 与 `ABP_PVP_FP_Locomotion` 四状态 locomotion 图，见 `Docs/Animation/README.md`；新 ABP 尚未挂到角色，角色仍使用 `ABP_PVP_FP_BaseCharacter`。
- 相机模式栈：`Camera/`、`Camera/Modes/` 仅为预留目录；当前 1P 视角由本模块内的 `CameraFP` 承担。
- 技能 / 属性：ASC、AttributeSet、GameplayAbility 属于 `GAS/`（当前仅有原生 Tag），见 `Docs/GAS/`。

---

## 2. 文档清单与路由

| 文档 | 内容 |
| --- | --- |
| `Docs/Character/README.md` | 本导航：定位、路由、组件与能力速查、启动入口与日志、未实现能力、校验入口、维护规则 |
| `Docs/Character/Character-Dev.md` | 实现细节：源码结构、设计决策、改造 SOP、踩坑清单、校验与 PIE 冒烟、已知限制 |
| `Docs/Character/Character-Integration.md` | 契约：模块依赖、数据流、C++ / 资产接口、其他模块调用方式、禁止依赖方向、变更影响面 |

**按场景路由**：

| 我要做的事 | 读哪里 |
| --- | --- |
| 查组件默认值、相机参数、走跑速度与状态 | 本文第 3 节 |
| 查 4 个动作绑到哪个函数、键位与 ValueType | `Docs/Input/README.md` 第 3 节（本文只做链接） |
| 修改胶囊 / Mesh / 相机默认值并保持与参考蓝图一致 | `Character-Dev.md` 第 4 节（SOP）+ 第 6 节（`compare_tfa_character.py`） |
| 排查"绑定数为 0 / 输入无响应" | `Character-Dev.md` 第 5 节踩坑 1，日志见本文第 4 节 |
| 排查"跑步速度不对 / 蓝图改了速度不生效" | `Character-Dev.md` 第 5 节踩坑 3 |
| 排查"镜头不转 / 转头看不到手臂" | `Character-Dev.md` 第 5 节踩坑 2 / 4 |
| 查 C++ 接口签名与资产契约 | `Character-Integration.md` 第 4 节 |
| 让动画 / 相机只读角色状态，或跨模块取角色 | `Character-Integration.md` 第 5 节 |
| 确认哪些依赖方向被禁止 | `Character-Integration.md` 第 6 节 |
| 改接口 / 默认值前评估影响面 | `Character-Integration.md` 第 7 节 |
| 跑角色对照脚本、看 parity 报告 | 本文第 6 节 → `Character-Dev.md` 第 6 节 |

---

## 3. 已实现能力速查（权威表）

### 3.1 组件与默认值

| 项 | 当前值 | 说明 |
| --- | --- | --- |
| 胶囊尺寸 | 半径 `5` / 半高 `88` | `InitCapsuleSize`，对齐参考蓝图 `BP_TFA_BaseCharacter` |
| `CharacterMesh0` 相对变换 | 位移 `(0,0,-88)`、旋转 `(0,-90,0)` | 继承的 `Mesh` 组件 |
| `BaseEyeHeight` / `CrouchedEyeHeight` | `64` / `32` | 眼高，影响 AI 与部分相机计算 |
| Actor 旋转跟随 | Yaw 跟随 Controller；Pitch / Roll **不**跟随 | `bUseControllerRotationYaw=true`、Pitch/Roll `false` |
| `CameraFP` 挂点 | `head` **骨骼**（非 Socket），相对旋转 `(0,90,-90)` | 位置随第一人称动画的头 bob / 蹲伏姿态 |
| `CameraFP` 参数 | `FieldOfView=100`、`bUsePawnControlRotation=true`、`PostProcessBlendWeight=1.0`、`bAutoActivate=true` | 旋转来自 `ControlRotation`，动画不参与视角旋转 |
| Mesh / AnimBP | 构造期 `FObjectFinder`：`SKM_PVP_FP_Manny_Simple`、`ABP_PVP_FP_BaseCharacter` | 工程自有路径：`/Game/PVPDemo/Characters/Mannequins/Meshes/`、`/Game/PVP/Animation/AnimBP/`，不依赖第三方包 |
| 输入组件类型 | `OverrideInputComponentClass = UPVPInputComponent` | 通过 `APawn` 钩子指定，未改 `DefaultInput.ini` |

### 3.2 移动与跑步状态

| 属性 | 默认 | 访问级别 | 行为 |
| --- | --- | --- | --- |
| `WalkSpeed` | `300` | `EditDefaultsOnly` / `BlueprintReadOnly` | 未按住跑时的 `MaxWalkSpeed`，蓝图可覆盖 |
| `RunSpeed` | `600` | 同上 | 按住跑时的 `MaxWalkSpeed` |
| `bIsRunning` | `false` | `BlueprintReadOnly` | 跑步状态，**供 Animation / Camera 只读** |
| `MaxWalkSpeed` | 构造与 `BeginPlay` 各写一次 | 由 `UCharacterMovementComponent` 持有 | 构造写默认值，`BeginPlay` 按蓝图覆盖后的 `WalkSpeed` 重写 |

**速度切换只发生在状态变化时**：`SetRunning` 先比较 `bIsRunning`，相同则直接返回，不做逐帧写入；`TODO(Network/GAS)` 标注了当前仅本地 / 自主代理生效。

### 3.3 输入消费入口（动作权威表在 `Docs/Input/README.md` 第 3 节）

| 语义 | InputTag | 回调函数 | 本模块内的实际行为 |
| --- | --- | --- | --- |
| 平面移动 | `InputTag.Move` | `Input_Move` | 只用 `ControlRotation.Yaw` 构造平面基向量，`MovementVector.Y` 走前后、`MovementVector.X` 走左右，Pitch 不参与 |
| 视角 | `InputTag.Look` | `Input_Look` | `AddControllerYawInput(X)` + `AddControllerPitchInput(Y)`，C++ **不取反**（极性由 IMC 的 Negate(Y) 修正） |
| 跳跃 | `InputTag.Jump` | `Input_JumpStarted` / `Input_JumpCompleted` | `Jump()` / `StopJumping()` |
| 按住跑 | `InputTag.Run` | `Input_RunStarted` / `Input_RunCompleted` | `SetRunning(true)` / `SetRunning(false)` → 切换 `MaxWalkSpeed` |

- 绑定共 **4 个动作、6 条绑定事件**，在 `SetupPlayerInputComponent` 内一次性完成。
- 键位、ValueType、IMC 修饰器、`DA_PVP_InputConfig` 槽位顺序以 `Docs/Input/README.md` 第 3 节为准，本文不复写。

---

## 4. 启动与运行时入口

| 阶段 | 执行者 | 行为 |
| --- | --- | --- |
| 构造函数 | `APVPBaseCharacter` | 设置胶囊 / Mesh 相对变换 / 眼高 / 旋转跟随；`OverrideInputComponentClass = UPVPInputComponent`；初始 `MaxWalkSpeed = WalkSpeed`、`bIsRunning = false`；`FObjectFinder` 装配 Mesh 与 AnimBP；创建 `CameraFP` 并挂 `head` 骨骼 |
| `BeginPlay` | `APVPBaseCharacter` | 重置 `bIsRunning = false`，按**蓝图覆盖后**的 `WalkSpeed` 重写 `MaxWalkSpeed` |
| Possess 后 | `APVPBaseCharacter::SetupPlayerInputComponent` | 校验输入组件类型与 `InputConfig`，按 Tag 绑定 6 条事件并打印绑定计数 |
| 运行时 | Animation / Camera | 只读 `bIsRunning`、`ControlRotation`、`CameraFP` 变换，不写角色状态 |

**关键日志**（`LogPVPBaseCharacter`）：

```text
LogPVPBaseCharacter: <pawn>: bound 6/6 native input bindings (config=DA_PVP_InputConfig)
LogPVPBaseCharacter: <pawn>: running=true max_walk_speed=600.0
```

失败信号与排错顺序见 `Character-Dev.md` 第 5 节、第 6 节。

---

## 5. 未实现能力与扩展方向

- **3P 与透视切换**：参考蓝图中的 `CameraPivotPoint → SpringArm → CameraTP`、`PP_Bodycam`、`SpotLight` 与 `IA_TFA_ToggleCameraPerspective` / `IA_TFA_ToggleCameraAnimation` 均未实现（头文件内 `TODO(3P)`）。
- **双网格**：当前只有 1P 单网格；1P 手臂 + 3P 全身的可见性管理、`BecomeViewTarget` 回调未接入（规划中的 `PVPPerspectiveComponent`）。
- **相机模式栈**：`Camera/` 与 `Camera/Modes/` 为预留目录，`CameraFP` 暂置于本模块，待相机系统进入实现阶段再迁出。
- **移动组件扩展**：`PVPCharacterMovementComponent`（冲刺 / 探头的网络预测）未实现。
- **网络权威速度**：`MaxWalkSpeed` 仅本地 / 自主代理应用，代码内 `TODO(Network/GAS)` 指向后续复制化或 GAS 属性接管。
- **技能与属性**：无 ASC / AttributeSet / 血量 / 伤害，属 `GAS/` 后续阶段，见 `Docs/GAS/README.md` 第 5 节。
- **第三方网格依赖（已解除）**：C++ 构造期已改引用自有 `SKM_PVP_FP_Manny_Simple` 与 `ABP_PVP_FP_BaseCharacter`（`Content/PVP/Animation/AnimBP/`），克隆工程无需导入 `/Game/InfimaGames` 原包；迁移后的 ABP 内部 Cast 目标错位问题见 `Animation-Dev.md` 踩坑 5。

---

## 6. 校验与报告入口

| 入口 | 作用 | 产物 |
| --- | --- | --- |
| `Content/Python/dump_tfa_character.py` | 导出参考蓝图 `BP_TFA_BaseCharacter` 的组件树与属性 | `Saved/PVPCharacterDump/tfa_base_character.json`、`tfa_base_character.md` |
| `Content/Python/compare_tfa_character.py` | 在 TFA 演示图中生成 / 复用 `BP_PVPBaseCharacter`，与参考蓝图逐项比对 1P 数据；结构不符直接抛异常使 commandlet 非零退出 | `Saved/PVPCharacterDump/parity_report.md` |
| `Content/Python/setup_pvp_input.py` | 校验 IA / IMC，并把 `DA_PVP_InputConfig` 挂到 `BP_PVPBaseCharacter` CDO | `Saved/PVPInputReport/report.md` |
| `Content/Python/verify_pvp_game_framework.py` | 校验地图与框架资产的自包含性（含 `BP_PVPBaseCharacter`） | `Saved/PVPGameFrameworkReport/verify.md` |
| PIE 冒烟 | 移动 / 视角 / 跳跃 / 跑步四向检查 | 清单见 `Character-Dev.md` 第 6 节 |

运行命令、报告关键项与冒烟步骤见 `Character-Dev.md` 第 6 节。

---

## 7. 文档维护规则

| 变更 | 必须同步 |
| --- | --- |
| 组件装配、默认值、相机参数变化（胶囊 / Mesh / 眼高 / FOV / 挂点） | 本文第 3.1 节 + `Character-Dev.md` 第 4 节 SOP + `compare_tfa_character.py` 的校验项 |
| 走 / 跑速度、`bIsRunning` 语义变化 | 本文第 3.2 节 + `Character-Integration.md` 第 4 / 7 节（Animation / Camera 只读方受影响） |
| 新增 / 删除 / 重绑动作或回调函数 | `Docs/Input/README.md` 第 3 节（权威表）+ 本文第 3.3 节链接与行为列 + `Docs/Input/Input-Dev.md` 第 4 节 |
| 对外接口签名、依赖方向、资产契约变化 | `Character-Integration.md` 第 2 / 4 / 6 / 7 节；本文第 3 节 |
| 出现新的坑点或校验步骤 | `Character-Dev.md` 第 5 / 6 节（踩坑四要素齐全） |
| 新增本模块维护文档 | 本文第 2 节文档清单登记，命名 `Character-<主题>.md` |
| 任何改动 | 三份文档文首「最后更新」刷新为当天日期 |

- 动作 / 键位 / 回调的权威表只存在于 `Docs/Input/README.md` 第 3 节；本文只写本模块内的消费行为，不复制键位与 ValueType。
- 资产总数、文件数只写在 `Docs/Content-Structure.md` / `Docs/Source-Structure.md`，本文不复制。
- 本模块文档与代码不一致时，以代码为准并当轮修正文档。
