# PVP 角色模块开发文档

> 维度：角色（Character / 移动状态 / 1P 相机内置）
> 模块：Character
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Character/README.md`（导航与能力速查）、`Docs/Character/Character-Integration.md`（依赖与接口契约）

本文只写**怎么实现、怎么开发、踩过什么坑**。组件默认值、移动状态与输入消费的权威表在 `Docs/Character/README.md` 第 3 节，动作与键位在 `Docs/Input/README.md` 第 3 节，本文均不复制。

---

## 1. 目的与范围

覆盖范围：

- `Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp` 的组件装配、移动状态与原生输入回调；
- 与参考蓝图 `BP_TFA_BaseCharacter` 的默认值对齐流程（`Content/Python/` 的 dump / compare 脚本）；
- 角色相关踩坑、校验与 PIE 冒烟。

不覆盖：输入语义与键位（`Docs/Input/`）、默认 Pawn / 控制器装配（`Docs/Game/`）、动画与程序化实现（`Animation/`，基础 locomotion 变量与 `ABP_PVP_FP_Locomotion` 见 `Docs/Animation/`；新 ABP 已在 `BP_PVPBaseCharacter` 上接线、程序化未实现）、相机模式栈（`Camera/`，预留）、技能与属性（`Docs/GAS/`）。

当前范围只覆盖**第一人称单网格**角色与走 / 跑两档速度。

---

## 2. 当前状态与源码结构

### 2.1 当前状态

- 角色基类：`APVPBaseCharacter`（`UCLASS(Blueprintable)`，`ACharacter` 子类），蓝图子类为 `BP_PVPBaseCharacter`。
- 视角：`CameraFP` 挂在 `CharacterMesh0` 的 `head` 骨骼上，`bUsePawnControlRotation=true`，旋转来自 Controller 的 Yaw / Pitch，位置随第一人称动画。
- 移动：`bUseControllerRotationYaw=true`；`MaxWalkSpeed` 由 `ApplyMovementSpeed` 统一应用，按住 `InputTag.Run` 在 `WalkSpeed`(300) / `RunSpeed`(600) 之间切换，瞄准期间固定走速。
- 瞄准：按住 `InputTag.Aim` 进入 `bIsAiming`；`Tick` 只在 FOV 过渡期间开启，`CameraFP` 在 100 与 `AimFieldOfView`(70) 之间 `FInterpTo`；武器丢失时自动退出。
- 射击：`InputTag.Fire` 只触发一次成对 Montage，具体 Montage 选择由 `APVPWeaponActor` 负责（腰射 / 瞄准版）。
- 输入：通过 `OverrideInputComponentClass` 使用 `UPVPInputComponent`，在 `SetupPlayerInputComponent` 中按 Tag 绑定 6 个动作 / 9 条事件；绑定关系见 `Docs/Input/README.md` 第 3 节。
- 装备：构造期创建 `UPVPEquipmentComponent`（当前武器 / 一次一把）；武器拾取、附着与成对 Montage 由 `Equipment/` 负责，角色不实现武器逻辑。
- 组件默认值：以参考蓝图导出值（`Saved/PVPCharacterDump/tfa_base_character.json`）为准，见导航第 3.1 节。

### 2.2 源码结构

| 文件 / 区域 | 职责 |
| --- | --- |
| `PVPBaseCharacter.h` | 公开组件与属性（`CameraFP`、`InputConfig`、`WalkSpeed`、`RunSpeed`、`bIsRunning`、`bIsAiming`、`AimFieldOfView`、`AimInterpolationSpeed`）、输入回调声明、`TODO(3P)` 规划注释 |
| `PVPBaseCharacter.cpp` — `PVPBaseCharacterDefaults` | 胶囊 / Mesh / 相机挂点 / FOV / 第一人称 Mesh 与 AnimBP 路径的集中常量 |
| `PVPBaseCharacter.cpp` — 构造函数 | 胶囊、旋转跟随、眼高、输入组件类型、初始速度、Mesh 与 AnimBP 查找、`CameraFP` 创建与挂接、`EquipmentComponent` 创建、Tick 默认关闭 |
| `PVPBaseCharacter.cpp` — `BeginPlay` | 重置跑步 / 瞄准状态，缓存 `DefaultFieldOfView`，并按蓝图覆盖后的 `WalkSpeed` 应用 `MaxWalkSpeed` |
| `PVPBaseCharacter.cpp` — `Tick` | 瞄准或 FOV 过渡期间开启：FOV `FInterpTo`、武器丢失检测；仅在松开瞄准且 FOV 收敛后关闭 Tick |
| `PVPBaseCharacter.cpp` — `SetupPlayerInputComponent` | Cast 校验输入组件 → 校验 `InputConfig` → 绑定 9 条事件 → 打印 `bound N/9` |
| `PVPBaseCharacter.cpp` — `Input_Move` / `Input_Look` / `Input_Jump*` / `Input_Run*` / `Input_Aim*` / `Input_FireStarted` / `SetRunning` / `SetAiming` / `ApplyMovementSpeed` | 移动基向量、视角写 Controller、起跳、速度切换、瞄准状态与 FOV、射击转发、Verbose 日志 |

### 2.3 本模块涉及的资产

| 资产 | 角色 | 来源 |
| --- | --- | --- |
| `BP_PVPBaseCharacter` | 继承 `APVPBaseCharacter` 的玩家角色蓝图；CDO 挂 `DA_PVP_InputConfig` | `Content/PVP/Characters/`，由 `compare_tfa_character.py` 创建 / 复用 |
| `BPI_PVP_AnimationState`、`ABP_PVP_Manny_PostProcess` | 动画状态接口与后处理动画蓝图（角色表现链） | `Content/PVP/Animation/Interfaces/`、`Content/PVP/Animation/AnimBP/` |
| `SKM_PVP_FP_Manny_Simple`、`ABP_PVP_FP_BaseCharacter` | 构造期 `ConstructorHelpers` 引用的第一人称网格与动画蓝图 | `/Game/PVPDemo/Characters/Mannequins/Meshes/`、`/Game/PVP/Animation/AnimBP/`（工程自有，不依赖第三方包） |
| `ABP_PVP_FP_Locomotion` | 新 locomotion ABP（父类 `UPVPAnimInstanceBase`，四状态 + Ground 内 `bIsAiming` 瞄准混合） | `/Game/PVP/Animation/AnimBP/`；已挂到 `BP_PVPBaseCharacter` 的 Mesh AnimClass（C++ 默认未改），重建后由 `setup_pvp_weapon.py` 读回 CDO 校验 |
| `BP_PVP_Weapon_AR` / `AM_PVP_FP_AR_Fire_Aimed` | 瞄准 / 射击阶段的武器与瞄准射击 Montage | `/Game/PVP/Actors/`、`/Game/PVPDemo/Weapons/AssaultRifle/Animations/Character/FP/Combat/`；由 `setup_pvp_weapon.py` 生成并配置到武器 CDO |

- 资产数量只看 `Docs/Content-Structure.md`，本文不复制总数。
- `.uasset` 只能由 UE 编辑器或 `Content/Python/` 脚本产出，禁止手写 / 伪造。

---

## 3. 设计决策与理由

1. **默认值对齐参考蓝图而非凭手感**。胶囊、Mesh 相对变换、眼高、相机挂点与 FOV 全部取自 `BP_TFA_BaseCharacter` 的导出值，保证 1P 视图与原包观感一致；改动必须过 `compare_tfa_character.py`。
2. **相机位置归动画，旋转归 Controller**。`CameraFP` 挂在 `head` 骨骼上，头 bob / 蹲伏姿态自然带出；`bUsePawnControlRotation=true` 让视角只受 `ControlRotation` 控制，动画骨骼旋转（后坐、倾斜、受击）不会改变瞄准方向。
3. **移动只用 Yaw 构造平面基向量**。用 `FRotationMatrix(FRotator(0, Yaw, 0))` 取 `X` / `Y` 轴，抬头低头不影响移动平面，避免"看天时移动变慢 / 斜着走"。
4. **Look 不在 C++ 取反**。俯仰极性由 `IMC_PVP_Default` 的 `InputModifierNegate(Y)` 唯一修正，避免两处取反互相抵消（跨模块约定见 `Docs/Input/Input-Dev.md` 踩坑 1）。
5. **输入组件类型走 Pawn 钩子**。`APawn::OverrideInputComponentClass` 只影响本角色，不动 `Config/DefaultInput.ini` 的全局 `DefaultInputComponentClass`，否则会影响工程内所有 Pawn。
6. **速度在 `BeginPlay` 重新应用**。蓝图子类默认值在 C++ 构造之后覆盖组件属性，只在构造函数写 `MaxWalkSpeed` 会被蓝图 CDO 覆盖；`BeginPlay` 再按 `WalkSpeed` 写一次才是可靠时机。
7. **速度切换无逐帧开销**。`SetRunning` / `SetAiming` 状态相同直接返回，只有状态变化时由 `ApplyMovementSpeed` 写一次 `MaxWalkSpeed`。
8. **瞄准状态归角色，FOV 用条件 Tick**。`bIsAiming` 是角色逻辑状态，Animation 只读，Camera 由角色写 FOV；`Tick` 默认关闭，只在瞄准或 FOV 未收敛时开启；瞄准期间保留 Tick 以检测武器丢失，松开瞄准且 FOV 收敛后自动关闭。相机模式栈仍未实现，`Camera/Modes/` 不提前创建。
9. **瞄准期间强制走速**。aimed 混合空间没有跑步样本，`ApplyMovementSpeed` 在 `bIsAiming` 时忽略 `bIsRunning`；`bIsRunning` 仍保留 Shift 语义，松开瞄准后若仍按住则恢复跑速。
10. **空手不允许瞄准 / 射击**。`SetAiming(true)` 与 `Input_FireStarted` 都先检查 `EquipmentComponent->HasWeapon()`；避免无武器时出现变焦或空手 Montage。
11. **相机暂留本模块**。`Camera/` 模式栈处于预留阶段，提前拆分会引入无人消费的抽象；等模式栈落地时再把 `CameraFP` 与瞄准 FOV 迁出，迁移边界见 `Character-Integration.md` 第 6 节。
12. **失败降级不崩溃**。`InputConfig` 缺失、输入组件类型不符只告警并跳过绑定；`FObjectFinder` 失败只是网格 / 动画为空，角色仍可生成，便于空内容工程启动。
13. **装备组件由角色持有，但武器逻辑不写在角色里**。`Character → Equipment` 单向；`UPVPEquipmentComponent` 只依赖 `ACharacter` 与 `APVPWeaponActor`，避免 `Equipment/` 反向包含 `Character/` 具体类；瞄准 / 射击只调用武器公开接口，不读武器内部状态。

---

## 4. 开发流程 SOP（改造角色默认值 / 新增组件）

1. **先确认当前基线**：跑 `dump_tfa_character.py` 与 `compare_tfa_character.py`，记录 `Saved/PVPCharacterDump/parity_report.md` 是否全绿，避免把既有偏差误判为本次引入。
2. **只改一处默认值**：C++ 默认值（`PVPBaseCharacterDefaults` / 构造函数）与蓝图 CDO 二选一。需要全工程生效的改 C++；单蓝图特例改 CDO，并在提交说明中写清。
3. **保持相机挂接不变**：新增 3P 组件时在 `CameraFP` 之后追加，**不要**重挂或重建 `CameraFP`（其挂点 `head`、相对旋转 `(0,90,-90)`、FOV 100 是被脚本校验的契约）。
4. **移动相关改动确认应用时机**：凡是影响 `MaxWalkSpeed` 的默认值，必须同时在 `BeginPlay` 的重新应用路径里可见，否则蓝图覆盖会失效（踩坑 3）。
5. **新增输入回调**：动作定义与 IMC 键位在 `Content/PVP/Input/`，绑定与 Tag 声明按 `Docs/Input/Input-Dev.md` 第 4 节 SOP；本模块只实现处理函数，并在 `SetupPlayerInputComponent` 中增加绑定 + 调整 `ExpectedBindings`。
6. **校验**：重新编译 → 跑 `compare_tfa_character.py`（默认值类改动必跑）→ 进入 `L_PVP_Showcase` 做 PIE 冒烟（第 6 节）。
7. **同步文档**（缺一视为任务未完成）：
   - `Docs/Character/README.md` 第 3 节（能力速查）与第 5 节（未实现能力）；
   - `Docs/Character/Character-Integration.md` 第 2 / 4 / 7 节（若涉及接口、依赖或资产契约）；
   - `Docs/Input/README.md` 第 3 节（若动作 / 回调 / 绑定数量变化）；
   - `Docs/Source-Structure.md` 2.1 节（若文件或实现状态变化）。
8. **新增组件**：在构造函数创建后同步本文第 2 节、导航第 3.1 / 4 节与 `Character-Integration.md` 第 2 / 4 节；若组件属于其他模块（如 `EquipmentComponent`），逻辑与文档落在该模块，角色只保留创建与只读属性。

---

## 5. 踩坑清单

### 5.1 原生绑定被跳过（绑定数为 0）

- **现象**：日志出现 `<pawn>: input component is UEnhancedInputComponent, expected UPVPInputComponent; native bindings skipped.` 或 `<pawn>: no UPVPInputConfig assigned; native bindings skipped.`，WASD / 鼠标 / 空格全部无响应。
- **根因**：① Pawn 实际使用的输入组件不是 `UPVPInputComponent`，常见于构造函数改动后未重编译，或有人把 `Config/DefaultInput.ini` 的 `DefaultInputComponentClass` 改成别的类型；② `BP_PVPBaseCharacter` 的 CDO 没有挂 `DA_PVP_InputConfig`。
- **正确做法**：保留构造函数中的 `OverrideInputComponentClass = UPVPInputComponent::StaticClass()`；不要改全局 `DefaultInputComponentClass`；CDO 的 `InputConfig` 由 `setup_pvp_input.py` 赋值，缺失时重跑脚本而不是手工乱填。
- **验证方式**：日志恢复为 `bound 9/9 native input bindings (config=DA_PVP_InputConfig)`；`Saved/PVPInputReport/report.md` 中 DA 挂接项为通过。

### 5.2 视角"不转"或"上下相反"时先分清归属

- **现象**：转向失灵 / 只能左右看 / 上下反向，容易误判为角色或相机代码问题。
- **根因**：视角链路是 `IA_PVP_Look → IMC 修饰器 → Input_Look → AddControllerYaw/PitchInput → ControlRotation → CameraFP`。反向几乎都来自 IMC 的 Negate 配置，而"完全不转"多来自输入未绑定或 `CameraFP->bUsePawnControlRotation` 被改。
- **正确做法**：极性只在 `IMC_PVP_Default` 的 `IA_PVP_Look → Mouse2D` 上以 `InputModifierNegate`（仅 Y）修正，`Input_Look` 内保持 `AddControllerPitchInput(LookAxisVector.Y)` 不取反；`CameraFP->bUsePawnControlRotation` 保持 `true`。
- **验证方式**：`Saved/PVPInputReport/report.md` 中 `IA_PVP_Look Negate(Y): yes`；PIE 四向检查正常；代码中不存在 `-LookAxisVector.Y`。

### 5.3 蓝图覆盖走 / 跑速度后不生效

- **现象**：在 `BP_PVPBaseCharacter` 里把 `WalkSpeed` / `RunSpeed` 改成别的值，PIE 里移动速度仍是 300 / 600，或跑步切换后速度回落到旧值。
- **根因**：蓝图类默认值在 C++ 构造函数之后应用，构造函数里写入的 `MaxWalkSpeed = WalkSpeed` 会被蓝图 CDO 对 `CharacterMovement` 的序列化值覆盖；只在构造函数写一次不可靠。
- **正确做法**：`BeginPlay` 中重置 `bIsRunning=false` 并调用 `ApplyMovementSpeed`；`SetRunning` 不直接写速度，统一走 `ApplyMovementSpeed`（其中包含 `bIsAiming` 门控）。新增速度档位时只改这一个入口。
- **验证方式**：PIE 中按住 / 松开 LeftShift，Verbose 日志 `running=... max_walk_speed=...` 的数值与蓝图设置一致；`GetCharacterMovement()->MaxWalkSpeed` 在 `BeginPlay` 后等于 `WalkSpeed`。

### 5.4 误把动画骨骼旋转当作视角修改

- **现象**：在 AnimBP 里旋转 `head` 骨骼想实现后坐 / 转头，结果相机位置动了、瞄准方向没变；或者反过来，以为 `CameraFP` 会自动跟随俯仰。
- **根因**：`CameraFP->bUsePawnControlRotation=true` 时，相机**旋转**完全由 `ControlRotation` 决定，骨骼旋转只影响相机**位置**（挂点姿态）。
- **正确做法**：视角变化走 `AddControllerPitchInput` / `AddControllerYawInput`、`CameraModifier` 或 `CameraShake`；动画只需驱动位置与身体姿态。表现层不得直接写 `ControlRotation`。
- **验证方式**：PIE 中只播动画不改输入时，准星方向保持不变、画面位置随头 bob 轻微变化。

### 5.5 瞄准 FOV 不恢复 / 瞄准时仍能跑

- **现象**：松开右键后镜头仍停在 70，或瞄准时按住 Shift 仍以 600 移动，导致 aimed 混合空间滑步。
- **根因**：① FOV 插值依赖 `Tick`，若 `PrimaryActorTick.bCanEverTick=false` 或 `SetActorTickEnabled(true)` 缺失，`SetAiming` 只改状态不改画面；② 速度只由 `SetRunning` 写，`SetAiming` 没有重新应用 `MaxWalkSpeed`。
- **正确做法**：`SetAiming` 每次状态变化都 `SetActorTickEnabled(true)` 并调用 `ApplyMovementSpeed`；`Tick` 在松开瞄准且 FOV 收敛后把 FOV 设为精确目标值再关闭 Tick（瞄准期间保持 Tick 以检测武器丢失）；`ApplyMovementSpeed` 用 `bIsRunning && !bIsAiming` 计算跑速。
- **验证方式**：PIE 中按住 / 松开右键，`CameraFP` FOV 在 100 ↔ 70 平滑变化并最终停住；按住 Shift + 右键时 `MaxWalkSpeed=300`，松开右键仍按 Shift 时恢复 600；`aiming=true max_walk_speed=300.0` 的 Verbose 日志出现。

### 5.6 第三方网格 / 动画蓝图缺失时角色"裸奔"（已修复）

- **现象**：克隆工程后未导入原包，PIE 能出生、能移动，但看不到手臂与武器，日志中可能出现 `FObjectFinder` 找不到对象的告警。
- **根因（历史）**：`PVPBaseCharacter.cpp` 构造函数此前通过 `ConstructorHelpers::FObjectFinder` / `FClassFinder` 引用 `/Game/InfimaGames/TacticalFPSAnimations/...` 的 `SKM_FP_Manny_Simple` 与 `ABP_TFA_FP_BaseCharacter`；该包不随仓库分发。
- **现状**：构造期路径已改为自有 `/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple` 与 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter`，克隆工程无需导入原包；改动后须重跑对照脚本，并确认 `verify_pvp_asset_layout.py` 的 `source.*` 三项通过。新 ABP 的接线通过 `BP_PVPBaseCharacter` 的 Mesh AnimClass 覆盖完成，C++ 默认 ABP 保持不变。
- **验证方式**：`Saved/PVPCharacterDump/parity_report.md` 中组件树与 Mesh / AnimBP 项通过；PIE 中可看到第一人称手臂。

---

## 6. 校验、报告与冒烟

### 6.1 脚本校验

```text
UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
    -Script="<project>/Content/Python/dump_tfa_character.py" -unattended -nosplash -nullrhi
UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
    -Script="<project>/Content/Python/compare_tfa_character.py" -unattended -nosplash -nullrhi
```

- `dump_tfa_character.py`：导出参考蓝图 `BP_TFA_BaseCharacter` 的组件树、挂接关系、相对变换、相机与角色属性，写入 `Saved/PVPCharacterDump/tfa_base_character.json`。
- `compare_tfa_character.py`：创建 / 复用 `BP_PVPBaseCharacter`，在 TFA 演示图中同时生成参考角色与派生角色并逐项比对（组件、挂接、相对变换、相机属性、角色层视图属性）；结构不符抛异常 → commandlet 非零退出。
- 比对容差：位置 `0.01`、角度 `0.01`；`CameraFP.auto_activate` 属预期差异（参考蓝图按视角切换相机，本类仅 1P 所以默认激活）。
- 报告：`Saved/PVPCharacterDump/parity_report.md`。

### 6.2 运行时日志

进入 `L_PVP_Showcase` 后应看到：

```text
LogPVPBaseCharacter: BP_PVPBaseCharacter_C_0: bound 9/9 native input bindings (config=DA_PVP_InputConfig)
```

| 日志 | 含义 | 处理 |
| --- | --- | --- |
| `input component is ..., expected UPVPInputComponent` | 输入组件类型不符 | 检查构造函数与 `DefaultInput.ini`，重新编译（踩坑 1） |
| `no UPVPInputConfig assigned` | CDO 未挂 DA | 重跑 `setup_pvp_input.py` |
| `bound N/9` 且 N < 9 | DA 槽位缺 IA 或旧资产只有 4 槽 | 重跑脚本并核对槽位顺序（`Docs/Input/Input-Dev.md` 踩坑 2、5.5） |
| 无 `bound` 日志 | `SetupPlayerInputComponent` 未执行 | 确认已 Possess、Pawn 由 `BP_PVPBaseCharacter` 生成 |

### 6.3 PIE 冒烟清单

- 出生时 `BP_PVPBaseCharacter` 的 Mesh AnimClass 为 `ABP_PVP_FP_Locomotion`，角色姿态由四状态 ABP 驱动（C++ 构造期默认仍是 `ABP_PVP_FP_BaseCharacter`）。
- 出生后能看到第一人称手臂与武器，视角高度与参考蓝图一致。
- WASD 四向移动方向随视角；抬头 / 低头时移动平面不倾斜。
- 鼠标左右转视角、上抬看天、下压看地。
- 空格起跳、松开停止上升；LeftShift 按住切跑、松开回走。
- 跑步切换时 `MaxWalkSpeed` 在 300 / 600 间切换，无逐帧抖动。
- 空手按住右键 / 左键无反应；拾取武器后按住右键，FOV 平滑到约 70、角色切瞄准姿态；按住 Shift 仍为走速。
- 瞄准时左键播放瞄准射击动画，松开右键回腰射；左键腰射播放腰射 Montage。
- 丢枪 / 卸下武器后瞄准自动退出，FOV 回到 100。
- 静态站立播动画（头 bob / 待机）时准星方向不漂移（踩坑 4 的反向验证）。

---

## 7. 已知限制与后续路线

- **单网格**：仅有 1P 网格；1P 手臂 + 3P 全身双网格、可见性 / 阴影策略与 `PVPPerspectiveComponent` 未实现。
- **3P 未实现**：`CameraPivotPoint → SpringArm → CameraTP`、`PP_Bodycam`、`SpotLight` 与透视切换 / 相机动画切换输入均未接入（`TODO(3P)`）。
- **相机模式栈**：`Camera/` 与 `Camera/Modes/` 仅预留；`CameraFP` 与瞄准 FOV 暂留本模块，迁移时需同步 `Character-Integration.md` 第 6 节与 `Docs/Input/Input-Integration.md` 第 3 节。
- **瞄准玩法**：无 AimOffset、准星 UI、瞄准灵敏度缩放、后坐力与扩散；瞄准期间只能用走速（aimed 混合空间无跑步样本）。
- **瞄准输入边界**：按住右键时拾取武器不会自动进入瞄准，需要重新按一次右键。
- **网络权威速度**：`MaxWalkSpeed` 只在本机 / 自主代理写入，服务器未校验；待 `PVPCharacterMovementComponent` 或 GAS 属性接管（`TODO(Network/GAS)`）。
- **第三方引用（已解除）**：构造期已改引用自有 `SKM_PVP_FP_Manny_Simple` 与 `ABP_PVP_FP_BaseCharacter`，与 `Docs/Source-Structure.md` 2.1 记录一致；克隆工程无需导入原包。
- **无技能 / 属性**：血量、伤害、死亡表现等待 `GAS/` 落地，见 `Docs/GAS/README.md` 第 5 节。

---

## 8. 相关文件

- 源码
  - `Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp`
- 资产
  - `Content/PVP/Characters/BP_PVPBaseCharacter`、`BP_PVP_BaseCharacter`
  - `Content/PVP/Animation/Interfaces/BPI_PVP_AnimationState`、`Content/PVP/Animation/AnimBP/ABP_PVP_Manny_PostProcess`
  - `Content/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter`（构造期默认 AnimClass，未在本次改动）
  - `Content/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion`（`BP_PVPBaseCharacter` 覆盖后的 AnimClass）
  - `Content/PVP/Input/DA_PVP_InputConfig`（CDO 挂接）
  - `Content/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple`（构造期默认第一人称网格）
- 工具与报告
  - `Content/Python/dump_tfa_character.py`
  - `Content/Python/compare_tfa_character.py`
  - `Saved/PVPCharacterDump/tfa_base_character.json|parity_report.md`
- 相关文档
  - `Docs/Character/README.md`、`Docs/Character/Character-Integration.md`
  - `Docs/Input/README.md` 第 3 节、`Docs/Input/Input-Dev.md` 踩坑清单
