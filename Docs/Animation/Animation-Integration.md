# PVP 动画模块接口与依赖文档

> 维度：动画（AnimInstance / locomotion 变量 / 动画蓝图生成）
> 模块：Animation
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Animation/README.md`（导航与变量权威表）、`Docs/Animation/Animation-Dev.md`（开发流程与踩坑）

本文只写**依赖谁、暴露什么、别人怎么调用**。已实现变量与其语义的唯一权威表在 `Docs/Animation/README.md` 第 3 节；实现细节与踩坑在 `Docs/Animation/Animation-Dev.md`。

---

## 1. 模块定位与边界

**职责**：

- 提供动画实例父类 `UPVPAnimInstanceBase : UAnimInstance`，新动画蓝图选择它为父类即可获得 locomotion 变量。
- 把 `APVPBaseCharacter` 与 `UCharacterMovementComponent` 的逻辑状态整理为蓝图只读变量，供 AnimGraph / 状态机 / 混合空间读取。
- 定义游戏线程采样与动画工作线程计算的职责边界，保证表现层只读、不反向写逻辑状态。
- 提供武器动画蓝图 `ABP_PVP_FP_Weapon_AR` 与角色 / 武器共享的 `DefaultSlot`，供 `Equipment/` 成对播放 Montage。

**非职责**：

- 不定义角色逻辑与输入绑定（`Character/` 与 `Input/` 负责；跑步速度切换在 `APVPBaseCharacter::SetRunning`）。
- 不参与视角旋转：Look 由 Controller 的 `ControlRotation` 驱动，动画不输出 AimPitch / Look 变量。
- 不制作美术动画内容（动画序列 / 混合空间 / Montage）；逻辑动画蓝图 `ABP_PVP_FP_Locomotion` 由编辑器期构建器 + `Content/Python/` 脚本产出。
- 不实现 GAS 技能动画、程序化动画、1P/3P 拆分与相机模式栈（后续阶段）。
- 不修改角色类 C++ 构造期默认值与现有动画蓝图；`ABP_PVP_FP_Locomotion` 已由用户在编辑器中挂到 `BP_PVPBaseCharacter` 的 Mesh AnimClass，接线通过蓝图实例覆盖完成。

---

## 2. 模块与外部依赖

### 2.1 Build.cs 依赖

运行时模块 `PVPDemo` 中与动画相关的依赖：

| 依赖 | 用途 |
| --- | --- |
| `Engine` | `UAnimInstance`、动画生命周期与线程回调 |
| `AnimGraphRuntime` | `UKismetAnimationLibrary::CalculateDirection`（`Direction` 变量） |

- `Source/PVPDemo/PVPDemo.Build.cs` **未修改**：上述两项均已在既有依赖中声明，本模块没有新增第三方依赖。
- `Source/PVPDemoEditor/PVPDemoEditor.Build.cs` 新增 `AnimGraph`、`AnimGraphRuntime`、`BlueprintGraph`（编辑器期动画图构建器）；`UnrealEd` / `AssetTools` 已存在。
- 计算结果：编译 `PVPDemoEditor Win64 Development` 通过（UHT 反射代码正常生成）。

### 2.2 源码与资产依赖

| 依赖对象 | 位置 | 说明 |
| --- | --- | --- |
| `APVPBaseCharacter` | `Source/PVPDemo/Character/PVPBaseCharacter.h` | 采集 `Velocity`、`MaxWalkSpeed`、`RunSpeed`、`bIsRunning` |
| `UCharacterMovementComponent` | 引擎 | 采集地面 / 空中移动模式与垂直速度 |
| `ABP_PVP_FP_Locomotion` | `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion` | 本次生成的 locomotion ABP，父类 `UPVPAnimInstanceBase`；四状态 + 6 条过渡；StateMachine 与 Root 之间已原地插入 `DefaultSlot`；已接到 `BP_PVPBaseCharacter` 的 Mesh AnimClass |
| `ABP_PVP_FP_Weapon_AR` | `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Weapon_AR` | 武器 ABP，父类 `UAnimInstance`；`SequencePlayer(A_PVP_WEP_AR_Reference)` → `DefaultSlot` → Root；骨骼 `SKEL_PVP_AR` |
| `ABP_PVP_FP_BaseCharacter` | `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter`，`APVPBaseCharacter` 构造函数通过 `ConstructorHelpers` 装配 | 迁移自第三方的现有动画蓝图，已改用工程自有路径，不再依赖 `/Game/InfimaGames`；仍是 C++ 构造期默认，但在 `BP_PVPBaseCharacter` 上已被覆盖 |
| `BP_PVPBaseCharacter` | `/Game/PVP/Characters/BP_PVPBaseCharacter` | Mesh 网格仍来自 C++ 默认；AnimClass 已在编辑器中显式覆盖为 `ABP_PVP_FP_Locomotion` |

**接线现状**：`ABP_PVP_FP_Locomotion` 已生成、编译并读回，用户已在编辑器中把 `BP_PVPBaseCharacter` 的 Mesh Anim Class 指向它；`setup_pvp_weapon.py` 已原地插入 `DefaultSlot` 并生成武器 ABP；`APVPBaseCharacter` 构造期默认仍是 `ABP_PVP_FP_BaseCharacter`，蓝图实例覆盖后新 ABP 实际生效。PIE / 视觉复核，以及生成脚本重跑后的覆盖复查仍待做。

---

## 3. 数据流与交互

```text
  APVPBaseCharacter (逻辑状态，Input → Character 产生)
        │  Velocity / MaxWalkSpeed / RunSpeed / bIsRunning / bIsAiming
        │  UCharacterMovementComponent.MovementMode
        ▼
  UPVPAnimInstanceBase
        │
        ├─ NativeInitializeAnimation（游戏线程，一次）
        │     缓存 CachedCharacter / CachedMovementComponent（TObjectPtr + Transient）
        │
        ├─ NativeUpdateAnimation（游戏线程，每帧）
        │     采样到私有快照：速度 / Yaw / MaxWalkSpeed / RunSpeed /
        │     bIsRunning / bIsAiming / IsMovingOnGround() / IsFalling()
        │     ※ 不写蓝图输出
        │
        └─ NativeThreadSafeUpdateAnimation（动画工作线程，每帧）
              只读快照 + MoveSpeedThreshold，计算并写入全部蓝图只读变量
              快照无效 → ResetAnimationState() 输出默认值
        │
        ▼
  ABP_PVP_FP_Locomotion（继承本类）→ AnimGraph / 四状态机 / Standing+Run 与 Aimed 混合
        │  只读 Velocity / Speed / Direction / bIs* 等
        ▼
  角色姿态（表现）
```

**关键约束**：

- 工作线程不访问 `GetOwningActor()` / `GetWorld()`，不打日志；所有 UObject 状态在游戏线程采样为值类型快照。
- 表现层只读逻辑状态：**动画不得反向修改**角色、`Input/` 或 `Game/` 的状态。
- Look 不进入动画变量；若动画蓝图需要让手臂 / 武器跟随俯仰，应读 Controller 的 `ControlRotation.Pitch` 或 `GetBaseAimRotation().Pitch`，不得反向驱动相机（相机边界见 `Docs/Input/Input-Integration.md` 第 3 节）。

---

## 4. 对外接口

### 4.1 C++ 接口

| 接口 | 位置 | 用途 |
| --- | --- | --- |
| `UPVPAnimInstanceBase` | `Source/PVPDemo/Animation/PVPAnimInstanceBase.h` | `UCLASS(Blueprintable)`，动画蓝图父类 |
| `NativeInitializeAnimation()` | 同上 | 缓存 Owner 角色与移动组件；类型不符 / 缺组件时告警一次并保持默认值，不崩溃 |
| `NativeUpdateAnimation(float)` | 同上 | 游戏线程采样到私有快照，缓存失效时重新解析 |
| `NativeThreadSafeUpdateAnimation(float)` | 同上 | 工作线程由快照计算并写入全部蓝图变量 |
| `MoveSpeedThreshold` | 同上 | `EditDefaultsOnly` / `BlueprintReadOnly` / `ClampMin=0`，默认 `1.0` cm/s，`bIsMoving` 判定阈值 |

蓝图只读输出的权威表（13 个变量：Velocity / Speed / SpeedZ / MaxSpeed / SpeedRatio / Direction / bIsMoving / bIsRunning / bIsAiming / bIsOnGround / bIsInAir / bIsJumping / bIsFalling）与状态判定规则见 `Docs/Animation/README.md` 第 3.2 / 3.3 节，本文不复制。

**接口性质**：全部输出均为 `BlueprintReadOnly`，动画蓝图只能读、不能写；本类不提供蓝图写入接口，也不提供运行时 C++ 查询接口。

### 4.2 资产接口

| 资产 | 路径 | 消费方 / 状态 |
| --- | --- | --- |
| `ABP_PVP_FP_Locomotion` | `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion` | 父类 `UPVPAnimInstanceBase`；`Ground` / `JumpStart` / `Falling` / `Land`；Ground 内 `bIsAiming` 在 Standing+Run 与 Aimed 之间混合；StateMachine → `DefaultSlot` → Root；已接到 `BP_PVPBaseCharacter` 的 Mesh AnimClass |
| `BS_PVP_FP_AR_Locomotion_Aimed` | `/Game/PVPDemo/Weapons/AssaultRifle/Animations/Character/FP/Locomotion/` | 瞄准 Idle / Walk 方向平面混合空间；`setup_pvp_anim_blueprint.py` 校验轴与样本 |
| `AM_PVP_FP_AR_Fire_Aimed` | `/Game/PVPDemo/Weapons/AssaultRifle/Animations/Character/FP/Combat/` | 瞄准射击 Montage（单 `DefaultSlot` 轨道），由 `BuildMontageFromSequence` 生成，挂在 `BP_PVP_Weapon_AR` 的 `AimedFireMontage` |
| `ABP_PVP_FP_Weapon_AR` | `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Weapon_AR` | 父类 `UAnimInstance`；`A_PVP_WEP_AR_Reference` → `DefaultSlot` → Root；供 `APVPWeaponActor` 播放武器 Montage |
| `ABP_PVP_FP_BaseCharacter` | `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter` | C++ 构造期默认 AnimClass（迁移资产），内部 Cast 目标为 `BP_PVP_BaseCharacter`；在 `BP_PVPBaseCharacter` 上已被覆盖，不再实际生效 |
| `BP_PVPBaseCharacter` | `/Game/PVP/Characters/BP_PVPBaseCharacter` | Mesh AnimClass 已在编辑器中显式覆盖为 `ABP_PVP_FP_Locomotion`；网格仍来自 C++ 默认值 |

**接线契约**：

1. 新角色动画蓝图的 Parent Class 必须是 `PVPAnimInstanceBase`（不是 `AnimInstance`），否则无变量可用；武器 ABP 使用 `UAnimInstance` 即可。
2. 骨骼网格必须与目标一致：角色 Montage / ABP 用 `SKEL_PVP_Mannequin`，武器 Montage / ABP 用 `SKEL_PVP_AR`。
3. 本次切换已由编辑器手动完成（`BP_PVPBaseCharacter` 的 Mesh AnimClass → `ABP_PVP_FP_Locomotion`）；后续同类变更仍必须由编辑器或 `Content/Python/` 脚本完成，不能手改 `.uasset`。
4. 角色与武器 ABP 必须暴露 `DefaultSlot`；迁移 Montage 的 Slot 名全部是 `DefaultSlot`，改名会导致 Montage 不播放。

### 4.3 编辑器构建器与生成脚本

| 接口 | 位置 | 用途 |
| --- | --- | --- |
| `UPVPAnimBlueprintBuilder::BuildLocomotionAnimBlueprint(...)` | `Source/PVPDemoEditor/PVPAnimBlueprintBuilder.h` | 创建 ABP、搭四状态机 / 规则、Ground 内 `bIsAiming` 瞄准混合、插入 `DefaultSlot`、编译并写 `Saved/PVPAnimBlueprintReport/report.json`（含 aim 轴 / 样本 / 连线） |
| `UPVPAnimBlueprintBuilder::BuildWeaponAnimBlueprint(...)` | 同上 | 创建武器 ABP：`ReferencePose → DefaultSlot → Root`，写 `Saved/PVPWeaponReport/weapon_abp_builder.json` |
| `UPVPAnimBlueprintBuilder::EnsureMontageSlot(...)` | 同上 | 对已有 ABP 原地插入 `DefaultSlot`（幂等、不删除资产），写 `Saved/PVPWeaponReport/slot_builder.json` |
| `UPVPAnimBlueprintBuilder::BuildMontageFromSequence(...)` | 同上 | 从 `A_PVP_FP_AR_Fire_Aimed` 生成单 Slot Montage（强制 `DefaultSlot`、`Default` section），写 `Saved/PVPWeaponReport/aimed_fire_montage_builder.json` |
| `Content/Python/setup_pvp_anim_blueprint.py` | `Content/Python/` | 资产校验（含 Aimed BlendSpace）、旧目标清理、调用构建器、编译 / 保存 / 读回、写 Markdown 报告 |
| `Content/Python/setup_pvp_weapon.py` | `Content/Python/` | 瞄准射击 Montage / 武器 ABP / 角色 Slot / 武器 BP / 地图拾取物生成与读回，写 `Saved/PVPWeaponReport/report.{json,md}` |
| `pvp_logic_layout.py` | `Content/Python/` | `ABP_` 路由到 `Animation/AnimBP`；期望资产含 `ABP_PVP_FP_Locomotion` / `ABP_PVP_FP_Weapon_AR`，逻辑资产总数 81（`Data` 2） |

- 构建器属于编辑器模块，不进入打包；运行时模块不依赖 `AnimGraph` / `BlueprintGraph`。
- 生成物路径由 `pvp_logic_layout.target_dir_for_asset_name` 决定，禁止在脚本中硬编码 `Characters/` 或 `Actors/`。

---

## 5. 其他模块调用方式

### 5.1 动画蓝图消费变量（接入方）

1. `ABP_PVP_FP_Locomotion` 已由 `Docs/Animation/Animation-Dev.md` 第 4.3 节生成；需要改图时修改构建器并重跑脚本，不要手改生成物。
2. 在 AnimGraph / 状态机 / 混合空间读取 `Docs/Animation/README.md` 第 3.2 节变量；只读，不写。
3. 接线已完成：`BP_PVPBaseCharacter` 的 Mesh Anim Class 指向 `ABP_PVP_FP_Locomotion`；重跑生成脚本后由 `setup_pvp_weapon.py` 读取 CDO 校验该覆盖，并在编辑器视口确认角色不再停在参考姿势。
4. 武器动画：`Equipment/` 的 `APVPWeaponActor::PlayEquipMontage` / `PlayFireMontage` / `PlayAimedFireMontage` / `PlayReloadMontage` 会同时对角色 Mesh 与武器 Mesh 调用 `Montage_Play`；角色 Montage 用 `SKEL_PVP_Mannequin`，武器 Montage 用 `SKEL_PVP_AR`，Slot 均为 `DefaultSlot`。
5. PIE 按 `Docs/Animation/Animation-Dev.md` 第 6.3 节冒烟；变量全为默认值时先查该文档第 5 节踩坑 1 / 4。

### 5.2 其他 C++ 模块读取动画状态

- 当前没有 C++ 消费方：本模块只向动画蓝图暴露变量。
- 若未来需要 C++ 读取，应从角色 / 移动组件直接取权威状态（`APVPBaseCharacter::bIsRunning` 等），不要从 AnimInstance 反向取表现状态。
- 保持 `Character/GAS → Animation` 单向流动；表现层不得作为逻辑状态来源。

### 5.3 与 Camera / Look 的边界

- `bUsePawnControlRotation=true` 下相机旋转由 Controller 提供，动画骨骼旋转不改变视角。
- 本模块不输出 Look 相关变量；需要俯仰驱动上半身时读 `ControlRotation.Pitch` 或 `GetBaseAimRotation().Pitch`，不得反向修改 Controller / Camera。

---

## 6. 禁止的依赖方向

- `Animation/` 不得包含 `Game/`、`Input/`、`Equipment/` 的头文件；对 `Character/` 只做状态读取与前向声明缓存。
- 动画蓝图不得写入 `UPVPAnimInstanceBase` 的输出变量，也不得在 AnimGraph 中调用会修改角色 / 移动组件的函数。
- 工作线程路径（`NativeThreadSafeUpdateAnimation`）不得访问 Owner / World / 日志等 UObject 状态。
- 不允许在动画层注入 IMC、修改 `InputConfig` 或切换走跑速度。
- 不允许把动画变量当作逻辑判定依据反向驱动 `Character/` 或 GAS；权威状态始终在逻辑层。
- 相机模式栈落地前，不允许动画层尝试接管视角计算。

---

## 7. 变更影响面

| 改动 | 必须同步 |
| --- | --- |
| 新增动画变量 | `PVPAnimInstanceBase.h/.cpp`（含 `ResetAnimationState()` 默认值）、`Docs/Animation/README.md` 第 3.2 节、本文第 4 节、三份文档文首日期 |
| 变量语义 / 判定规则变化 | `Docs/Animation/README.md` 第 3.3 节、`Docs/Animation/Animation-Dev.md` 第 3 节 |
| 采集源变化（角色 / 移动组件字段） | `NativeUpdateAnimation` 采样逻辑、`Docs/Animation/Animation-Dev.md` 第 2 / 4 节 |
| 线程职责调整 | `Docs/Animation/Animation-Dev.md` 第 3 节设计决策与第 5 节踩坑 1 |
| 接线状态或 AnimClass 覆盖方式变化 | 资产由编辑器 / `Content/Python/` 产出；`Docs/Content-Structure.md`、`Docs/Source-Structure.md`、`Docs/Animation/README.md` 第 1 / 3.1 / 4 / 5 节、`Docs/Character/` 三件套与三份文首日期 |
| 状态 / 过渡 / BlendSpace 绑定变化 | `Docs/Animation/README.md` 第 3.4 节、`Docs/Animation/Animation-Dev.md` 第 3 / 4 / 5 节、本文第 4.3 节 |
| 瞄准变量 / Aimed BlendSpace / 瞄准截面变化 | `PVPAnimInstanceBase` 采样、`BuildLocomotionAnimBlueprint` 的 `AimBlendSpace` 参数、`setup_pvp_anim_blueprint.py` 读回项、`Docs/Animation/README.md` 第 3.2 / 3.4 节、`Docs/Character/` 瞄准状态 |
| 武器 ABP / `DefaultSlot` / 成对 Montage 变化 | `Docs/Animation/README.md` 第 3.1 / 3.5 节、`Docs/Animation/Animation-Dev.md` 第 3 / 4 / 5 节、本文第 4.2 / 4.3 / 5.1 节、`Docs/Equipment/` 三件套 |
| 构建器或生成脚本变化 | `Source/PVPDemoEditor/PVPAnimBlueprintBuilder.*`、`Content/Python/setup_pvp_anim_blueprint.py`、`Content/Python/setup_pvp_weapon.py`、`Docs/Source-Structure.md` 第 2.8 节、本文第 2.1 / 4.3 节 |
| Build.cs 依赖变化 | `Source/PVPDemo/PVPDemo.Build.cs`、`Source/PVPDemoEditor/PVPDemoEditor.Build.cs`、`Docs/Source-Structure.md` 模块依赖段、本文第 2.1 节 |
| 源码分层空 / 非空状态变化 | `Docs/Source-Structure.md` 文首与第 1 节目录树、`AGENTS.md` 模块表 |

- 任何一项改动完成后，同时刷新三份文档文首的「最后更新」。
- 结构性改动（模块 / 文档 / 脚本增删或改名）后运行 `verify_doc_index.py`，全绿方可收尾。

---

## 8. 相关文件

- 源码
  - `Source/PVPDemo/Animation/PVPAnimInstanceBase.h/.cpp`
  - `Source/PVPDemoEditor/PVPAnimBlueprintBuilder.h/.cpp`
  - `Source/PVPDemo/PVPDemo.Build.cs`（既有依赖 `Engine`、`AnimGraphRuntime`，本轮未改）
  - `Source/PVPDemoEditor/PVPDemoEditor.Build.cs`（新增 `AnimGraph` / `AnimGraphRuntime` / `BlueprintGraph`）
  - `Source/PVPDemo/Character/PVPBaseCharacter.h`（采集源，`bIsRunning` / `RunSpeed`）
- 工具
  - `Content/Python/setup_pvp_anim_blueprint.py`
  - `Content/Python/setup_pvp_weapon.py`
  - `Content/Python/pvp_logic_layout.py`
  - `Content/Python/verify_pvp_asset_layout.py`
- 文档
  - `Docs/Animation/README.md` — 变量权威表与接入入口
  - `Docs/Animation/Animation-Dev.md` — 实现细节、SOP 与踩坑
  - `Docs/Input/Input-Integration.md` — 相机 / Look 边界
  - `Docs/Source-Structure.md` — 分层职责与阶段约束
- 资产（现状）
  - `/Game/PVP/Characters/BP_PVPBaseCharacter` — Mesh Anim Class 已切到 `ABP_PVP_FP_Locomotion`（C++ 构造期默认仍为 `ABP_PVP_FP_BaseCharacter`）
- 规范
  - `AGENTS.md` — 上下文入口与硬约束
  - `Docs/AI-Workflow.md` — 模块文档制度与收尾校验
