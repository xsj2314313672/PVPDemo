# PVP 动画模块导航

> 维度：动画（AnimInstance / locomotion 变量 / 动画蓝图接入）
> 模块：Animation
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Animation/Animation-Dev.md`（开发流程与踩坑）、`Docs/Animation/Animation-Integration.md`（依赖与接口契约）

本文件是动画模块的固定导航入口。进入本模块的任务先读本文件，再按第 2 节路由按需读取；本文第 3 节是**已实现动画变量与语义的唯一权威表**，其他文档不复制。

---

## 1. 模块定位与边界

**职责**：把角色逻辑状态整理成动画蓝图可读的变量，并提供 AnimInstance 生命周期入口。

- 源码：`Source/PVPDemo/Animation/PVPAnimInstanceBase.h/.cpp`（`UPVPAnimInstanceBase : UAnimInstance`）。
- 采集对象：`APVPBaseCharacter` 的 `Velocity`、`MaxWalkSpeed`、`RunSpeed`、`bIsRunning`，以及 `UCharacterMovementComponent` 的地面 / 空中模式。
- 输出：Move / Run / Jump / Aim 四组蓝图只读 locomotion 变量，供 AnimGraph、状态机与混合空间直接读取。
- 已落地动画蓝图：`/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion`（`Ground` / `JumpStart` / `Falling` / `Land` 四状态，Ground 内在 Standing+Run 与 Aimed 之间按 `bIsAiming` 混合）与 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Weapon_AR`（`A_PVP_WEP_AR_Reference` → `DefaultSlot` → Root）。
- Montage 接入：`ABP_PVP_FP_Locomotion` 已在 StateMachine 与 Root 之间原地插入 `DefaultSlot`；角色 / 武器 Montage 均使用 `DefaultSlot`，由 `Equipment/` 的成对播放接口触发（含 `AM_PVP_FP_AR_Fire_Aimed`）。
- 接线方式：`UPVPAnimInstanceBase` 作为角色动画蓝图父类；`ABP_PVP_FP_Locomotion` 由脚本生成并挂在 `BP_PVPBaseCharacter` 的 Mesh AnimClass；`APVPBaseCharacter` 构造期默认与现有 `ABP_PVP_FP_BaseCharacter` 均未改动，实际生效来自蓝图实例覆盖；重建 ABP 后由 `setup_pvp_weapon.py` 读取 CDO 实际 AnimClass 校验覆盖未丢。

**非职责**（不属于本模块，不要在这里实现）：

- Look 与视角：当前 1P 视角由 Controller 的 `ControlRotation` 驱动，不进入动画变量（动作语义见 `Docs/Input/README.md` 第 3 节）。
- 蹲伏 / 上半身分层 / AimOffset：动作尚未绑定或模块尚未实现，不提供变量；瞄准姿态已通过 `BS_PVP_FP_AR_Locomotion_Aimed` 与 `bIsAiming` 混合落地，武器 ABP 与 `DefaultSlot` 已落地。
- 动画蓝图资产本身：`.uasset` 只能由编辑器或 `Content/Python/` 脚本产出；`ABP_PVP_FP_Locomotion` 已由脚本生成，手工修改需回写构建器 / 生成脚本。
- 角色逻辑与输入绑定：`Character/` 负责输入回调与走跑速度切换，动画层只读。
- Montage / 技能动画、程序化动画、1P/3P 拆分：后续阶段（见第 5 节）。

---

## 2. 文档清单与路由

| 文档 | 内容 |
| --- | --- |
| `Docs/Animation/README.md` | 本导航：定位、路由、变量权威表、启动入口、未实现能力、校验入口、维护规则 |
| `Docs/Animation/Animation-Dev.md` | 实现细节：源码结构、线程安全设计、新增变量 SOP、踩坑清单、校验与 PIE 冒烟、已知限制 |
| `Docs/Animation/Animation-Integration.md` | 契约：模块依赖、数据流、C++ 接口、动画蓝图接入方式、禁止依赖方向、变更影响面 |

**按场景路由**：

| 我要做的事 | 读哪里 |
| --- | --- |
| 查当前有哪些动画变量、含义与默认值 | 本文第 3 节 |
| 重新生成 `ABP_PVP_FP_Locomotion` | 本文第 4 节 → `Animation-Dev.md` 第 4.2 节 |
| 修改状态 / 过渡 / BlendSpace 绑定 | 本文第 3.4 节 → `Animation-Dev.md` 第 3 / 4 节 |
| 确认新 ABP 的接线与生效方式 | 本文第 3.1 / 4 节（已接到 `BP_PVPBaseCharacter`，C++ 默认未改）→ `Animation-Integration.md` 第 4.2 / 5.1 节 |
| 排查变量不更新 / 全是默认值 | `Animation-Dev.md` 第 5 节踩坑 1、2、4 + 第 6 节 |
| 新增一个动画变量或采集一个新的动作状态 | `Animation-Dev.md` 第 4 节 SOP |
| 查 C++ 接口签名与变量契约 | `Animation-Integration.md` 第 4 节 |
| 让其他模块消费动画状态、确认依赖方向 | `Animation-Integration.md` 第 3 / 5 / 6 节 |
| 改接口前评估影响面 | `Animation-Integration.md` 第 7 节 |
| 编译与 PIE 验证 | 本文第 6 节 → `Animation-Dev.md` 第 6 节 |

---

## 3. 已实现能力速查（权威表）

### 3.1 类与生命周期

| 项 | 当前实现 | 说明 |
| --- | --- | --- |
| 类 | `UPVPAnimInstanceBase` | `UAnimInstance` 子类，`UCLASS(Blueprintable)`，可被动画蓝图继承 |
| 采集对象 | `APVPBaseCharacter` + `UCharacterMovementComponent` | `NativeInitializeAnimation` 中缓存，失败只告警并保持默认值 |
| 游戏线程采样 | `NativeUpdateAnimation` | 只采样原始值（速度 / Yaw / `MaxWalkSpeed` / `RunSpeed` / `bIsRunning` / 地面与空中模式） |
| 工作线程计算 | `NativeThreadSafeUpdateAnimation` | 依据采样快照写入全部蓝图变量；不访问 Owner / World，不打日志 |
| 动画蓝图资产 | `ABP_PVP_FP_Locomotion` | `/Game/PVP/Animation/AnimBP/`，父类 `UPVPAnimInstanceBase`，骨骼 `SKEL_PVP_Mannequin`，由 `Content/Python/setup_pvp_anim_blueprint.py` + 编辑器期 C++ 构建器生成 |
| 武器动画蓝图 | `ABP_PVP_FP_Weapon_AR` | `/Game/PVP/Animation/AnimBP/`，父类 `UAnimInstance`，骨骼 `SKEL_PVP_AR`，`SequencePlayer(A_PVP_WEP_AR_Reference)` → `DefaultSlot` → Root，由 `setup_pvp_weapon.py` + `BuildWeaponAnimBlueprint` 生成 |
| Montage Slot | `DefaultSlot` | `ABP_PVP_FP_Locomotion` 与 `ABP_PVP_FP_Weapon_AR` 均暴露；迁移的角色 / 武器 Montage 与新建的 `AM_PVP_FP_AR_Fire_Aimed` 全部使用该 Slot |
| 瞄准混合 | `Ground` 内 `BlendListByBool(bIsAiming)` | true → `BlendPose_0` 接 `BS_PVP_FP_AR_Locomotion_Aimed`，false → `BlendPose_1` 接 Standing+Run 结果；进入 0.15s / 退出 0.20s |
| 接线状态 | **新 ABP 已接到角色蓝图** | `APVPBaseCharacter` 构造期默认 AnimClass 仍是 `ABP_PVP_FP_BaseCharacter`（`/Game/PVP/Animation/AnimBP/`）；`BP_PVPBaseCharacter` 的 Mesh AnimClass 已手动切到 `ABP_PVP_FP_Locomotion`，实例覆盖 C++ 默认值；`ABP_PVP_FP_Locomotion` 已原地插入 `DefaultSlot`；视觉 / PIE 复核待做 |

### 3.2 变量契约

全部变量均为 `BlueprintReadOnly`，动画蓝图可读、不可写。

| 变量 | 类型 | 含义 |
| --- | --- | --- |
| `Velocity` | `FVector` | 当帧完整速度（cm/s） |
| `Speed` | `float` | 平面速度 `Velocity.Size2D()`（cm/s），不含垂直分量 |
| `SpeedZ` | `float` | 垂直速度 `Velocity.Z`（cm/s），上升为正、下落为负 |
| `MaxSpeed` | `float` | 当前 `MaxWalkSpeed`：走路为 `WalkSpeed`（默认 300），按住跑为 `RunSpeed`（默认 600） |
| `SpeedRatio` | `float` | 平面速度相对角色 `RunSpeed` 的比例，限制在 0～1；走路约 0.5、奔跑约 1.0 |
| `Direction` | `float` | 速度相对角色前向的方向角，-180～180 度；静止时为 0，可直接喂给方向混合空间 |
| `bIsMoving` | `bool` | 平面速度是否超过 `MoveSpeedThreshold`（默认 1 cm/s） |
| `bIsRunning` | `bool` | 是否处于按住 Run 状态，镜像 `APVPBaseCharacter::bIsRunning`（静止按住也为 true；瞄准期间仍保留 Shift 语义） |
| `bIsAiming` | `bool` | 是否处于瞄准状态，镜像 `APVPBaseCharacter::bIsAiming`；驱动 Ground 内的 Aimed BlendSpace 分支 |
| `bIsOnGround` | `bool` | 移动模式是否为 `Walking` / `NavWalking` |
| `bIsInAir` | `bool` | 移动模式是否为 `Falling` |
| `bIsJumping` | `bool` | 空中且 `SpeedZ > 0`（上升段） |
| `bIsFalling` | `bool` | 空中且 `SpeedZ <= 0`（下降段，含跳跃顶点） |

配置项：

| 属性 | 默认 | 说明 |
| --- | --- | --- |
| `MoveSpeedThreshold` | `1.0` | `EditDefaultsOnly` / `ClampMin=0`；判定 `bIsMoving` 的平面速度阈值（cm/s） |

### 3.3 状态判定规则

- `bIsMoving` 只看平面速度，不使用 `IsMovingOnGround()`：后者只表示移动模式为可行走地面，站着不动同样为 true。
- `bIsOnGround` / `bIsInAir` 直接来自 `UCharacterMovementComponent` 的移动模式，不来自角色 Yaw 或输入。
- `bIsJumping` / `bIsFalling` 由 `bIsInAir` 与 `SpeedZ` 组合判定，两者互斥且覆盖空中的全部帧。
- `Direction` 使用 `UKismetAnimationLibrary::CalculateDirection(Velocity, FRotator(0, ActorYaw, 0))`，与引擎方向混合空间约定一致。
- `bIsAiming` 只镜像角色逻辑状态，不参与 `bIsRunning` 判定；瞄准时角色侧已把 `MaxWalkSpeed` 限制为走速，动画不再额外改速度。
- Look 不产生变量：视角旋转由 Controller 消费，不参与动画姿态。

### 3.4 已落地动画蓝图（`ABP_PVP_FP_Locomotion`）

| 状态 | 内容 | 说明 |
| --- | --- | --- |
| `Ground` | Standing 分支（`BS_PVP_FP_AR_Locomotion_Standing` + `A_PVP_FP_AR_Run_F_Loop`）与 `BS_PVP_FP_AR_Locomotion_Aimed`，经最外层 `BlendListByBool(bIsAiming)` 混合 | Standing 内层 bool 为 `bIsRunning AND bIsMoving`，Run 接 `BlendPose_0`、BlendSpace 接 `BlendPose_1`；两个 BlendSpace 轴均为 `Horizontal` / `Vertical`（-100～100），由 `sin(Direction)*100` / `cos(Direction)*100` 驱动，并用 `bIsMoving` 门控到 `(0,0)` 待机样本；外层 true 接 Aimed、false 接 Standing 结果 |
| `JumpStart` | `A_PVP_FP_AR_Jump_Start` | 单次播放 |
| `Falling` | `A_PVP_FP_AR_Jump_Falling_Loop` | 循环播放 |
| `Land` | `A_PVP_FP_AR_Jump_End` | 单次播放，播完自动回 `Ground` |

| 源状态 | 目标状态 | 规则 | 交叉淡入淡出 |
| --- | --- | --- | --- |
| Entry | `Ground` | 直接连接 | 无 |
| `Ground` | `JumpStart` | `bIsJumping` | 0.10s |
| `Ground` | `Falling` | `bIsFalling` | 0.10s |
| `JumpStart` | `Falling` | `bIsFalling` | 0.10s |
| `JumpStart` | `Land` | `bIsOnGround` | 0.08s |
| `Falling` | `Land` | `bIsOnGround` | 0.10s |
| `Land` | `Ground` | `Jump_End` 剩余时间自动过渡 | 0.20s |

> **角色 ABP 的 DefaultSlot**：`StateMachine → DefaultSlot → Root`；无 Montage 时 Slot 透传状态机姿态，播放 Montage 时由 Slot 覆盖。所有迁移 Montage 的 Slot 名为 `DefaultSlot`。

### 3.5 武器动画蓝图（`ABP_PVP_FP_Weapon_AR`）

| 节点 | 内容 |
| --- | --- |
| `SequencePlayer` | `A_PVP_WEP_AR_Reference`，循环播放，保持武器参考姿势 |
| `Slot` | `DefaultSlot` |
| `Root` | 输出 |

- 无 Montage 时武器保持参考姿势；播放 `AM_PVP_FP_WEP_AR_*` 时由 `DefaultSlot` 覆盖。
- 武器 Montage 的骨骼必须是 `SKEL_PVP_AR`；角色 Montage 的骨骼必须是 `SKEL_PVP_Mannequin`。
- 成对播放由 `APVPWeaponActor::PlayEquipMontage` / `PlayFireMontage` / `PlayReloadMontage` 发起，动画层只接收 `Montage_Play`。

---

## 4. 启动与运行时入口

| 阶段 | 执行者 | 行为 |
| --- | --- | --- |
| 动画实例初始化 | `UPVPAnimInstanceBase::NativeInitializeAnimation` | 缓存 `APVPBaseCharacter` 与 `UCharacterMovementComponent`；Owner 不是目标角色或缺少移动组件时打一条 `LogPVPAnimInstanceBase` 警告，变量保持默认值 |
| 每帧（游戏线程） | `UPVPAnimInstanceBase::NativeUpdateAnimation` | 采样速度、角色 Yaw、`MaxWalkSpeed`、`RunSpeed`、`bIsRunning`、`IsMovingOnGround()`、`IsFalling()` 到快照；缓存失效时尝试重新解析 |
| 每帧（动画工作线程） | `UPVPAnimInstanceBase::NativeThreadSafeUpdateAnimation` | 由快照计算并写入所有蓝图变量；快照无效时重置为默认值 |
| 动画蓝图生成 | `setup_pvp_anim_blueprint.py` + `UPVPAnimBlueprintBuilder` | 加载骨骼 / Standing 与 Aimed BlendSpace / 跳跃序列 → 删除旧目标 ABP → 创建 `ABP_PVP_FP_Locomotion`、搭建四状态图、Ground 瞄准混合与规则 → 编译、保存、读回（含 aim 轴 / 样本） |
| 角色接线 | 用户手动完成 | `BP_PVPBaseCharacter` 的 Mesh Anim Class 已指向 `ABP_PVP_FP_Locomotion`，覆盖 `APVPBaseCharacter` 构造期默认 AnimClass；后续视觉复核见第 6 节 |

**关键日志**：

```text
LogPVPAnimInstanceBase: Warning: <AnimInstance>: owning actor is <Actor>, expected APVPBaseCharacter; animation variables stay at defaults.
LogPVPAnimInstanceBase: Warning: <AnimInstance>: <Character> has no character movement component; animation variables stay at defaults.
```

正常情况下不打日志；出现上述警告说明动画蓝图挂在了非 `APVPBaseCharacter` 的 Actor 上，或移动组件缺失。

---

## 5. 未实现能力与扩展方向

- **角色接线**：`APVPBaseCharacter` 构造函数默认 AnimClass 仍是 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter`（工程自有，不再引用 `/Game/InfimaGames`）；`BP_PVPBaseCharacter` 的 Mesh AnimClass 已手动切到 `ABP_PVP_FP_Locomotion`，实例覆盖默认值，新 ABP 已生效。
- **新 ABP 的运行时验证**：当前只做编译、图结构读回与布局校验；接线已完成，PIE / Persona 视觉确认待做，重跑生成脚本后还需复查蓝图覆盖是否保留。
- **Montage / 上下半身分层**：Ground 状态只处理整个人物的 locomotion；`DefaultSlot` 已接入，角色 / 武器成对 Montage（含瞄准射击）可播放，但瞄准偏移、上半身分层 / AimOffset 尚未接入。
- **Look 变量**：未提供 `AimPitch` / 视角偏航增量；如后续上半身或 AimOffset 需要，再按 `ControlRotation` 追加。
- **蹲伏**：动作尚未绑定或模块未实现，未预留空变量；瞄准已通过 `bIsAiming` + Aimed BlendSpace 接入，跳跃状态没有瞄准变体。
- **1P/3P 拆分**：后续派生 `UPVPArmsAnimInstance` / `UPVPBodyAnimInstance`；本轮不提前实现。
- **Montage / 技能动画**：`UAnimInstance` 原生 Montage 能力可用，角色 / 武器成对 Montage 已由 `APVPWeaponActor` 播放；GAS 双网格 Montage 任务未实现。
- **程序化动画**：Sway / Bob / Recoil / ADS 对齐 / 贴墙收枪均未实现。

---

## 6. 校验与报告入口

| 入口 | 作用 | 产物 |
| --- | --- | --- |
| UBT 编译 | 编译 `PVPDemoEditor`，确认类、UHT 反射与动画图构建器通过 | `UnrealEditor-PVPDemo.dll` |
| 动画蓝图生成 | 运行 `Content/Python/setup_pvp_anim_blueprint.py`，创建 / 重建 `ABP_PVP_FP_Locomotion`、编译并读回四状态 / 6 条过渡 / `DefaultSlot` | `Saved/PVPAnimBlueprintReport/report.json`、`report.md` |
| 武器动画蓝图与 Slot | 运行 `Content/Python/setup_pvp_weapon.py`，生成 `AM_PVP_FP_AR_Fire_Aimed` / `ABP_PVP_FP_Weapon_AR`、给 `ABP_PVP_FP_Locomotion` 原地插入 `DefaultSlot`、生成武器 BP 与关卡拾取物 | `Saved/PVPWeaponReport/report.json`、`report.md`、`aimed_fire_montage_builder.json`、`weapon_abp_builder.json`、`slot_builder.json` |
| 布局校验 | `Content/Python/verify_pvp_asset_layout.py` 校验 `Animation/AnimBP` 数量 5、逻辑资产总数 81、旧路径零残留 | `Saved/PVPAssetReorgReport/verify.{json,md}` |
| Persona / PIE 冒烟 | 打开新 ABP 检查待机 / 走 / 跑 / 跳跃姿态与过渡 | 无（手动，清单见 `Animation-Dev.md` 第 6 节） |

---

## 7. 文档维护规则

| 变更 | 必须同步 |
| --- | --- |
| 新增 / 删除 / 重命名动画变量，或修改语义与默认值 | 本文第 3.2 / 3.3 节 + `Animation-Integration.md` 第 4 节 + `Animation-Dev.md` 第 4 节 SOP |
| 修改生命周期（采样 / 计算时机、线程安全边界） | 本文第 3.1 / 4 节 + `Animation-Dev.md` 第 3 节 + `Animation-Integration.md` 第 4 节 |
| 新增采集来源（动作、组件、GAS 状态） | 本文第 3 节 + `Animation-Integration.md` 第 3 / 6 节；涉及输入语义时链接 `Docs/Input/README.md` 第 3 节 |
| 角色接线状态或 AnimClass 覆盖方式变化（C++ 默认 ABP / BP 实例覆盖） | 本文第 3.1 / 4 / 5 节 + `Animation-Integration.md` 第 4.2 / 5.1 节 + `Docs/Source-Structure.md` + `Docs/Character/README.md` 与 `Docs/Character/Character-Integration.md` 对应条目 |
| 动画蓝图状态 / 过渡 / 资产引用变化 | 本文第 3.4 节 + `Animation-Dev.md` 第 3 / 4 / 5 节 + `Animation-Integration.md` 第 4.2 节 |
| 武器 ABP / `DefaultSlot` / 成对 Montage 变化 | 本文第 3.1 / 3.5 / 5 节 + `Animation-Dev.md` 第 3 / 4 / 5 节 + `Animation-Integration.md` 第 4.2 / 4.3 节 + `Docs/Equipment/` |
| 修改构建器或生成脚本 | 本文第 4 / 6 节 + `Animation-Dev.md` 第 4.2 / 5 节 + `Docs/Source-Structure.md` 第 2.8 节 |
| 出现新的坑点或校验步骤 | `Animation-Dev.md` 第 5 / 6 节（踩坑四要素齐全） |
| 新增本模块维护文档 | 本文第 2 节文档清单登记，命名 `Animation-<主题>.md` |
| 任何改动 | 三份文档文首「最后更新」刷新为当天日期 |

- 变量语义与默认值的权威表只在本文第 3 节；其他模块文档只链接，不复制。
- 动作 / 键位 / 回调的权威表只在 `Docs/Input/README.md` 第 3 节。
- 资产总数、文件数只写在 `Docs/Content-Structure.md` / `Docs/Source-Structure.md`，本文不复制。
- 本模块文档与代码不一致时，以代码为准并当轮修正文档。
