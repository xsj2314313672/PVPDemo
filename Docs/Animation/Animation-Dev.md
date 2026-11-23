# PVP 动画模块开发文档

> 维度：动画（AnimInstance / locomotion 变量 / 动画蓝图生成）
> 模块：Animation
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-23
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Animation/README.md`（导航与变量权威表）、`Docs/Animation/Animation-Integration.md`（依赖与接口契约）

---

## 1. 目的与范围

本文覆盖 `UPVPAnimInstanceBase` 的实现方式、线程安全约定、新增变量的开发 SOP、踩坑与验证步骤。

覆盖范围：

- `NativeInitializeAnimation` / `NativeUpdateAnimation` / `NativeThreadSafeUpdateAnimation` 的职责划分。
- Move / Run / Jump 三组 locomotion 变量的采集与计算。
- `ABP_PVP_FP_Locomotion` 的自动生成流程（C++ 编辑器构建器 + Python commandlet）、四状态图设计与 `DefaultSlot`。
- `ABP_PVP_FP_Weapon_AR` 的参考姿势 + `DefaultSlot` 图，以及 `EnsureMontageSlot` 原地插入流程。

不覆盖：

- 动画资产制作（动画序列、混合空间、Montage 内容）。
- 输入绑定与键位（见 `Docs/Input/`）。
- 角色移动逻辑与走跑速度切换（见 `Docs/Character/`）。
- GAS 技能动画、相机模式栈、程序化动画（后续阶段）。

---

## 2. 当前状态与源码结构

| 文件 | 内容 |
| --- | --- |
| `Source/PVPDemo/Animation/PVPAnimInstanceBase.h` | 类声明、12 个蓝图只读输出、`MoveSpeedThreshold` 配置、缓存的角色 / 移动组件、游戏线程采样快照 |
| `Source/PVPDemo/Animation/PVPAnimInstanceBase.cpp` | 初始化缓存、游戏线程采样、工作线程计算、无效引用兜底与默认值重置 |
| `Source/PVPDemoEditor/PVPAnimBlueprintBuilder.h/.cpp` | 编辑器期动画图构建器：创建 ABP、搭状态机 / 状态 / 过渡 / 规则、编译并写 JSON 报告；新增 `BuildWeaponAnimBlueprint`（参考姿势 + Slot）与 `EnsureMontageSlot`（对已有 ABP 原地插入 Slot） |
| `Content/Python/setup_pvp_anim_blueprint.py` | 资产加载、旧目标清理、调用构建器、编译 / 保存 / 读回、Markdown 报告 |
| `Content/Python/setup_pvp_weapon.py` | 武器 ABP / 武器 BP / 角色 ABP Slot / 地图拾取物生成与读回；不删除 `ABP_PVP_FP_Locomotion` |
| `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion` | 生成物：父类 `UPVPAnimInstanceBase`，`Ground` / `JumpStart` / `Falling` / `Land` 四状态，StateMachine → `DefaultSlot` → Root |
| `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Weapon_AR` | 生成物：父类 `UAnimInstance`，`A_PVP_WEP_AR_Reference` → `DefaultSlot` → Root |
| `Source/PVPDemo/PVPDemo.Build.cs` | **未修改**；`Engine` 与 `AnimGraphRuntime`（`UKismetAnimationLibrary`）均已在既有依赖中声明 |

实现要点：

- `NativeInitializeAnimation`：`Super` 后缓存 `APVPBaseCharacter` 与 `UCharacterMovementComponent`；Owner 类型不符或缺少移动组件时用 `LogPVPAnimInstanceBase` 告警一次，不崩溃。
- `NativeUpdateAnimation`：调用 `Super` 后缓存失效时重新解析（兼容编辑器 / 调试流程），然后把速度、角色 Yaw、`MaxWalkSpeed`、`RunSpeed`、`bIsRunning`、`IsMovingOnGround()`、`IsFalling()` 写入私有快照；不写蓝图输出。
- `NativeThreadSafeUpdateAnimation`：只读快照与 `MoveSpeedThreshold`，计算并写入全部蓝图输出；快照无效时调用 `ResetAnimationState()` 输出默认值。
- 私有缓存的 `CachedCharacter` / `CachedMovementComponent` 使用 `TObjectPtr` + `Transient`，参与 GC 追踪但不序列化、不进入蓝图。
- 编译结果：`PVPDemoEditor Win64 Development` 通过（UHT 反射代码正常生成）。

**当前状态**：`ABP_PVP_FP_Locomotion` 已由 `setup_pvp_anim_blueprint.py` 生成并编译通过，四状态与 6 条过渡已读回；用户已在编辑器中手动完成接线：`BP_PVPBaseCharacter` 的 Mesh AnimClass 切到 `ABP_PVP_FP_Locomotion`。`setup_pvp_weapon.py` 已用 `EnsureMontageSlot` 在该 ABP 的 StateMachine 与 Root 之间原地插入 `DefaultSlot`，并生成武器 ABP `ABP_PVP_FP_Weapon_AR`（参考姿势 + `DefaultSlot`）。`APVPBaseCharacter` 构造函数默认 AnimClass 仍是 `ABP_PVP_FP_BaseCharacter`（`/Game/PVP/Animation/AnimBP/`），C++ 未改，蓝图实例覆盖后新 ABP 实际生效；视觉 / PIE 复核待做。注意该默认 ABP 是迁移资产，其内部 Cast 目标为 `BP_PVP_BaseCharacter`（见第 5 节踩坑 5），与本次新建的 ABP 无关。

---

## 3. 设计决策与理由

| 决策 | 理由 |
| --- | --- |
| 游戏线程采样 + 动画工作线程计算 | `UAnimInstance` 的注释明确建议在 `NativeUpdateAnimation` 收集数据、把主要工作放到 `NativeThreadSafeUpdateAnimation`；后者由 `FAnimInstanceProxy::UpdateAnimation()` 调用，单线程立即更新路径也会经过 `ParallelUpdateAnimation`，因此无论是否启用多线程动画更新都能刷新变量 |
| 在工作线程只读私有快照 | 工作线程不接触 `GetOwningActor()` / `GetWorld()` 等 UObject 状态，避免线程安全风险；同时让蓝图变量与逻辑状态之间有一帧一致的快照 |
| `SpeedRatio` 以 `RunSpeed` 为分母 | `MaxWalkSpeed` 会随走 / 跑切换，用它做分母时走路和奔跑的比例都接近 1，无法驱动混合；用 `RunSpeed` 做分母可得到走路约 0.5、奔跑约 1.0 的稳定区间 |
| `bIsMoving` 用速度阈值而非移动模式 | `UCharacterMovementComponent::IsMovingOnGround()` 只判断 `MOVE_Walking` / `MOVE_NavWalking`，站着不动同样为 true，不能表达"正在移动" |
| `Direction` 复用 `UKismetAnimationLibrary::CalculateDirection` | 与引擎方向混合空间的 -180～180 度约定一致，避免自写角度换算引入左右反向问题 |
| 空中状态拆成 `bIsInAir` / `bIsJumping` / `bIsFalling` | 状态机通常需要区分上升与下降；`bIsInAir` 保留原始移动模式语义，`bIsJumping` / `bIsFalling` 由垂直速度细分且互斥 |
| 不提供 Look 变量 | 当前 1P 视角由 Controller 的 `ControlRotation` 驱动，动画不参与视角旋转；过早引入 AimPitch 会与相机方案耦合 |
| 在角色蓝图覆盖 AnimClass，不改 C++ 构造期默认 | 新增 `ABP_PVP_FP_Locomotion` 后在 `BP_PVPBaseCharacter` 上手动切换 Mesh AnimClass 完成接线；C++ 默认仍是 `ABP_PVP_FP_BaseCharacter`，避免改构造函数影响所有派生类；视觉确认仍需在 PIE / Persona 完成 |
| 编辑器 C++ 构建器 + Python commandlet | `UEdGraph::AddNode` / `CreateNode` 未反射，`ue5-automation` bridge 对 AnimBP 只读；只能在编辑器 C++ 中搭图，由 Python 负责资产生命周期 |
| Ground 内用 BlendListByBool 叠加 Run | `BS_PVP_FP_AR_Locomotion_Standing` 只有 Idle / Walk 样本，没有 Run 样本；单独用 Speed 驱动会把跑步播成走路 |
| BlendSpace 的 Horizontal / Vertical 用 sin/cos 驱动 | 该混合空间是方向平面（样本点 -100/0/100），不是 Speed/Direction 轴；用 `sin(Direction)*100` / `cos(Direction)*100` 才能命中 F/B/L/R 样本 |
| 方向轴用 `bIsMoving` 门控 | 静止时 `Direction=0`，`cos(0)*100=100` 会落到 Walk_F 样本；用 `SelectFloat` 在非移动时输出 0，回到 Idle `(0,0)` |
| Land → Ground 用自动规则 | `Jump_End` 单次播放，`bAutomaticRuleBasedOnSequencePlayerInState` 按剩余时间自动过渡，无需额外的状态时间图 |
| 角色 ABP 原地插入 `DefaultSlot` | 删除重建 `ABP_PVP_FP_Locomotion` 可能丢失 `BP_PVPBaseCharacter` 的 Mesh AnimClass 覆盖；`EnsureMontageSlot` 只改图、不换资产，幂等 |
| 武器 ABP 用 `A_PVP_WEP_AR_Reference` + `DefaultSlot` | 无 Montage 时保持武器参考姿势；有 Montage 时由 Slot 覆盖；与迁移武器 Montage 的 Slot 名一致 |
| 成对 Montage 由 `Equipment/` 发起 | 角色手臂与武器是两个骨架，配对逻辑集中在 `APVPWeaponActor`；动画层只接收 `Montage_Play`，不反向控制装备 |

---

## 4. 开发流程 SOP

### 4.1 新增一个动画变量

1. 在 `PVPAnimInstanceBase.h` 增加 `UPROPERTY(Transient, BlueprintReadOnly, Category = "PVP|Animation|Locomotion")` 变量与默认值，注释写明单位与语义。
2. 若数据来自角色：在 `NativeUpdateAnimation` 采样到私有快照字段，**不要**在工作线程直接访问角色。
3. 在 `NativeThreadSafeUpdateAnimation` 由快照计算输出；在 `ResetAnimationState()` 中把新变量重置为默认值。
4. 同步 `Docs/Animation/README.md` 第 3.2 / 3.3 节与 `Animation-Integration.md` 第 4 节；语义变化时刷新三份文档文首日期。
5. 编译 `PVPDemoEditor`，在动画蓝图中确认变量可读取且随动作变化。

### 4.2 新增一个动作状态的采集

1. 确认动作已有权威逻辑状态：输入绑定与角色属性见 `Docs/Input/README.md` 第 3 节、`Docs/Character/README.md` 第 3 节；不要从输入系统直接取键位。
2. 在 `NativeUpdateAnimation` 中采样，快照只放值类型（`bool` / `float` / `FVector` / `FRotator`）或缓存的组件指针。
3. 在 `NativeThreadSafeUpdateAnimation` 计算输出；若涉及多个状态组合，按 README 第 3.3 节的规则保持互斥与全覆盖。
4. 在 `ResetAnimationState()` 补默认值，确保引用失效时动画蓝图不会读到上一帧的残留状态。
5. 按第 5 节检查线程安全边界，按第 6 节做 PIE 冒烟。

### 4.3 生成 `ABP_PVP_FP_Locomotion`

目标资产由布局单一事实来源路由：`pvp_logic_layout.target_dir_for_asset_name('ABP_PVP_FP_Locomotion')` = `/Game/PVP/Animation/AnimBP`。脚本不允许把目标硬编码到 `Characters/` 或 `Actors/`。

1. 关闭编辑器，编译 `PVPDemoEditor`（构建器在编辑器模块中）。
2. 运行 commandlet（正斜杠路径，避免 `Unreal Projects` 的空格/反斜杠解析问题）：
   ```text
   UnrealEditor-Cmd.exe "D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -run=PythonScript ^
       -Script="D:/UE/Unreal Projects/PVPDemo/Content/Python/setup_pvp_anim_blueprint.py" ^
       -unattended -nosplash -nullrhi
   ```
3. 脚本阶段：校验骨骼 / 网格 / BlendSpace / 序列 → 删除旧目标资产（`delete_asset` + 清理 `.uasset` 文件）→ 调用 `UPVPAnimBlueprintBuilder.BuildLocomotionAnimBlueprint` → `compile_blueprint` → 保存 → 读回父类 / 骨骼 / AnimGraph / 四状态 / 6 条过渡 → 写报告。
4. 运行 `python Content/Python/verify_pvp_asset_layout.py`，确认 `Animation/AnimBP` 数量 4、总数 78、旧路径零残留。
5. 打开 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion` 的 Persona 预览，确认 Idle / Walk / Run / Jump 姿态；接线已由用户在编辑器中完成：`BP_PVPBaseCharacter` 的 Mesh Anim Class 已指向它；重跑生成脚本后需确认该覆盖仍在。

**手动兜底**（构建器不可用时）：新建 Animation Blueprint → 父类 `PVPAnimInstanceBase` → 按 README 第 3.4 节搭四状态与过渡。注意 Ground 的 BlendSpace 轴是 `Horizontal` / `Vertical`，必须按 `sin/cos + bIsMoving` 驱动，不能直接接 `Speed` / `Direction`。

### 4.4 生成武器 ABP 并给角色 ABP 加 Slot

1. 关闭编辑器，编译 `PVPDemoEditor`（`BuildWeaponAnimBlueprint` / `EnsureMontageSlot` 在编辑器模块中）。
2. 运行 commandlet（路径用正斜杠）：
   ```text
   UnrealEditor-Cmd.exe "D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -run=PythonScript ^
       -Script="D:/UE/Unreal Projects/PVPDemo/Content/Python/setup_pvp_weapon.py" ^
       -unattended -nosplash -nullrhi
   ```
3. 脚本阶段：校验依赖 → 删除 / 重建 `ABP_PVP_FP_Weapon_AR` → `EnsureMontageSlot` 给 `ABP_PVP_FP_Locomotion` 加 `DefaultSlot` → 复用 / 创建 `BP_PVP_Weapon_AR` 并设 CDO → 放置 / 更新地图拾取物 → 逐项读回。
4. 期望：`Saved/PVPWeaponReport/report.md` 中 `result: PASS`，`weapon_abp.*` / `character_slot.*` / `character.locomotion_reference` 全部 PASS。
5. 重新打开编辑器，确认 `ABP_PVP_FP_Locomotion` 的 AnimGraph 为 `StateMachine → DefaultSlot → Root`，`ABP_PVP_FP_Weapon_AR` 为 `SequencePlayer(A_PVP_WEP_AR_Reference) → DefaultSlot → Root`。

**手动兜底**：在 Persona 中给角色 ABP 插入 Slot 节点（Slot Name = `DefaultSlot`）并连到 Root；武器 ABP 新建 `SequencePlayer(A_PVP_WEP_AR_Reference)` → Slot → Root。不要删除重建角色 ABP。

---

## 5. 踩坑清单

### 踩坑 1：在工作线程访问 Owner / World

| 要素 | 内容 |
| --- | --- |
| 现象 | `NativeThreadSafeUpdateAnimation` 内调用 `GetOwningActor()`、`GetWorld()` 或蓝图侧的普通函数后，出现线程安全断言 / 偶发数据竞争，动画蓝图读取到不一致状态 |
| 根因 | 该回调由 `FAnimInstanceProxy::UpdateAnimation()` 在动画工作线程执行；UObject 图的读写约定只保证游戏线程安全，蓝图 AnimGraph 也只允许调用标记为 `BlueprintThreadSafe` 的函数 |
| 正确做法 | 所有角色 / 组件状态在 `NativeUpdateAnimation`（游戏线程）采样到私有快照，工作线程只读快照与配置值；需要引擎工具函数时只用纯数学函数 |
| 验证方式 | 检查 `PVPAnimInstanceBase.cpp` 的 `NativeThreadSafeUpdateAnimation` 内无 `GetOwningActor` / `GetWorld` / `Cast` 调用；PIE 中多帧移动无断言与状态跳变 |

### 踩坑 2：把 `IsMovingOnGround()` 当作"是否在移动"

| 要素 | 内容 |
| --- | --- |
| 现象 | 角色站立不动时 `bIsMoving` 仍为 true，待机状态无法进入 |
| 根因 | `UCharacterMovementComponent::IsMovingOnGround()` 只返回 `(MovementMode == MOVE_Walking || MOVE_NavWalking) && UpdatedComponent`，与速度无关 |
| 正确做法 | 用平面速度与阈值判断：`bIsMoving = Velocity.Size2D() > MoveSpeedThreshold`；`bIsOnGround` 单独保留移动模式语义 |
| 验证方式 | PIE 站立时 `bIsMoving=false`、`bIsOnGround=true`；按 W 后 `bIsMoving=true`、`Speed` 大于 0 |

### 踩坑 3：`SpeedRatio` 用 `MaxWalkSpeed` 做分母

| 要素 | 内容 |
| --- | --- |
| 现象 | 无论走路还是奔跑，`SpeedRatio` 都接近 1，1D 混合空间始终落在最右侧，走跑动画无区分 |
| 根因 | `APVPBaseCharacter::SetRunning` 会把 `MaxWalkSpeed` 在 `WalkSpeed`（300）与 `RunSpeed`（600）之间切换，用当前 `MaxWalkSpeed` 做分母时走路 300/300、奔跑 600/600 都为 1 |
| 正确做法 | 以角色固定的 `RunSpeed` 为分母并 `Clamp(0,1)`；`MaxSpeed` 仍输出当前 `MaxWalkSpeed` 供调试或其他逻辑使用 |
| 验证方式 | PIE 走路时 `SpeedRatio≈0.5`（速度接近 300），按住 Shift 奔跑时 `SpeedRatio≈1.0`，松开 Shift 回落 |

### 踩坑 4：动画蓝图父类不对或角色蓝图未覆盖 AnimClass

| 要素 | 内容 |
| --- | --- |
| 现象 | 动画蓝图里看不到本类变量，或 PIE 中变量始终为默认值 / 角色停在参考姿势；当前 `BP_PVPBaseCharacter` 已接线，但重跑生成脚本或重置蓝图 CDO 可能丢失覆盖 |
| 根因 | 新动画蓝图沿用默认父类 `AnimInstance`；或 `BP_PVPBaseCharacter` 的 Mesh Anim Class 未指向 `ABP_PVP_FP_Locomotion`，只剩 C++ 构造期默认 `ABP_PVP_FP_BaseCharacter`（`/Game/PVP/Animation/AnimBP/`）。两者层级不同：C++ 默认作用于所有派生类，BP CDO 覆盖只作用于该角色蓝图 |
| 正确做法 | 动画蓝图 Class Settings 的 Parent Class 设为 `PVPAnimInstanceBase`；在 `BP_PVPBaseCharacter` 的 Mesh 组件上确认 Anim Class 为 `ABP_PVP_FP_Locomotion`，不要改 `PVPBaseCharacter.cpp` 的构造期路径 |
| 验证方式 | 打开动画蓝图能看到 `Speed` / `bIsMoving` 等变量；检查 `BP_PVPBaseCharacter` 的 Mesh Anim Class 指向新 ABP；PIE 移动时数值变化，日志中无 `expected APVPBaseCharacter` 警告 |

### 踩坑 5：迁移动画蓝图内部 Cast 目标与实际 Pawn 不一致

| 要素 | 内容 |
| --- | --- |
| 现象 | 角色 Mesh 已挂 `ABP_PVP_FP_BaseCharacter`，但动画变量（速度 / 状态）始终为默认值，姿态不随移动变化 |
| 根因 | 两个迁移动画蓝图（`ABP_PVP_FP_BaseCharacter` / `ABP_PVP_TP_BaseCharacter`）内部通过 Cast 访问 `BP_PVP_BaseCharacter` 的变量，而 GameMode 的 `DefaultPawnClass` 是兄弟类 `BP_PVPBaseCharacter`（父类为 C++ `APVPBaseCharacter`），Cast 必然失败 |
| 正确做法 | 让实际 Pawn 继承 Cast 目标，或把 ABP 内的 Cast 改为读取 `UPVPAnimInstanceBase` 变量 / `BPI_PVP_AnimationState` 接口；改图属独立任务，本轮只记录，不改蓝图图表 |
| 验证方式 | PIE 中观察 ABP 的 Cast 节点失败分支；或新建继承 `UPVPAnimInstanceBase` 的动画蓝图后对比变量是否随移动变化 |

### 踩坑 6：把 `BS_PVP_FP_AR_Locomotion_Standing` 当成 Speed / Direction 混合空间

| 要素 | 内容 |
| --- | --- |
| 现象 | 无论往哪走，姿态都偏向前进或直接不变化；跑步时仍播走路 |
| 根因 | 该混合空间轴是 `Horizontal` / `Vertical`（-100～100），样本点为前后左右与 Idle，不是 Speed（0～600）和 Direction（-180～180）；它只覆盖 Idle / Walk，Run 是独立序列 |
| 正确做法 | Ground 内用 `sin(Direction)*100` / `cos(Direction)*100` 驱动 X / Y；用 `bIsRunning AND bIsMoving` 在 BlendSpace 与 `A_PVP_FP_AR_Run_F_Loop` 之间 `BlendListByBool` 混合 |
| 验证方式 | 打开 ABP 的 Ground 状态，X 接 `DegSin → Multiply_IntFloat(100) → SelectFloat(bIsMoving)`，Y 接 `DegCos → Multiply_IntFloat(100) → SelectFloat(bIsMoving)`；PIE 前/后/左/右分别命中 F/B/L/R 样本 |

### 踩坑 7：静止时 `Direction=0` 导致 `cos(0)*100` 播前进走

| 要素 | 内容 |
| --- | --- |
| 现象 | 站立不动时角色保持 Walk_F 姿态而不是 Idle，脚仍在走 |
| 根因 | `UPVPAnimInstanceBase::Direction` 在静止时输出 0；`cos(0)*100 = 100` 正好对应 BlendSpace 的前向 Walk 样本 |
| 正确做法 | 两个方向轴都经过 `bIsMoving` 门控（`SelectFloat` 非移动时输出 0），使 BlendSpace 落在 `(0,0)` 的 Idle 样本；不要用 `Direction` 直接作为角度轴 |
| 验证方式 | 站立时 BlendSpace 输入应为 `(0,0)`；按 W 后变为 `(0,100)`，按 A/D 后 X 变为 ∓100 |

### 踩坑 8：用 Python / ue5-automation 写 AnimGraph

| 要素 | 内容 |
| --- | --- |
| 现象 | Python 能创建 ABP 资产，但无法添加 AnimGraph / 状态机 / 过渡节点；bridge 只返回只读图信息 |
| 根因 | `UEdGraph::AddNode` / `CreateNode` 未反射给 Python；`ue5-automation` 文档明确 AnimBP 写入通道未开放 |
| 正确做法 | 在 `Source/PVPDemoEditor/` 用 `FGraphNodeCreator` + `UAnimGraphNode_*` / `UAnimState*` 构建图，Python commandlet 只负责加载、调用、保存与读回 |
| 验证方式 | 生成报告出现 `states` 4 项、`transitions` 6 项；ABP 编译 0 错误 |

### 踩坑 9：commandlet 删除旧 ABP 后文件仍在 / 构建器误判已存在

| 要素 | 内容 |
| --- | --- |
| 现象 | `EditorAssetLibrary.delete_asset` 返回 true，但同一进程内 `does_asset_exist` / `FPackageName::DoesPackageExist` 仍为 true，构建器拒绝创建 |
| 根因 | `ForceDeleteObjects` 删除 UObject 后，包文件在 commandlet 进程退出前可能仍留在磁盘 / 包缓存中 |
| 正确做法 | 删除后显式 `os.remove` 目标 `.uasset`；构建器用 `FindObject` 判断已加载对象，不要用 `DoesPackageExist` |
| 验证方式 | 连续运行两次 `setup_pvp_anim_blueprint.py`，第二次 `deleted_previous=true` 且仍能创建、编译、保存 |

### 踩坑 10：删除重建角色 ABP 丢失蓝图 AnimClass 覆盖

| 要素 | 内容 |
| --- | --- |
| 现象 | 重新生成 `ABP_PVP_FP_Locomotion` 后，`BP_PVPBaseCharacter` 的 Mesh AnimClass 回到 `ABP_PVP_FP_BaseCharacter`；或 `EnsureMontageSlot` 报“已有同名 Slot 但未接线” |
| 根因 | 删除重建同名资产会让蓝图实例覆盖在重新加载时失效；`EnsureMontageSlot` 对已有但未接在 source→root 之间的 Slot 不做猜测性重连 |
| 正确做法 | 给已有角色 ABP 加 Slot 时只用 `EnsureMontageSlot`；若该 Slot 已存在但未接线，在 Persona 中手动接好或删除该 ABP 后重建并重新设置 `BP_PVPBaseCharacter` 的 Mesh AnimClass；禁止用 Python 改继承 Mesh 组件 |
| 验证方式 | `Saved/PVPWeaponReport/report.md` 的 `character.locomotion_reference` 为 PASS；打开角色蓝图确认 Mesh AnimClass 仍指向 `ABP_PVP_FP_Locomotion` |

### 踩坑 11：Montage 不播放或只播一侧

| 要素 | 内容 |
| --- | --- |
| 现象 | 拾取后角色手臂不动、武器不后坐 / 不换弹，或只有一侧播放 |
| 根因 | 目标 ABP 没有 `DefaultSlot` 节点；Montage 的 Slot 名不是 `DefaultSlot`；或把武器 Montage 播到了角色 Mesh（骨骼不匹配） |
| 正确做法 | 角色 ABP：`StateMachine → DefaultSlot → Root`；武器 ABP：`SequencePlayer(A_PVP_WEP_AR_Reference) → DefaultSlot → Root`；角色 Montage 必须基于 `SKEL_PVP_Mannequin`，武器 Montage 必须基于 `SKEL_PVP_AR`；通过 `APVPWeaponActor::PlayEquipMontage` / `PlayFireMontage` / `PlayReloadMontage` 成对播放 |
| 验证方式 | `weapon_abp.builder_slot` 与 `character_slot.report_slot` 均为 `DefaultSlot`；PIE 中装备 / 开火 / 换弹时角色手臂与武器同步，`LogPVPWeaponActor` 无 “no anim instance” 警告 |

### 踩坑 12：Montage 内第三方动画通知 Cast 失败

| 要素 | 内容 |
| --- | --- |
| 现象 | Montage 姿态正常播放，但弹壳、弹匣、治疗道具不生成 |
| 根因 | `Content/PVP/Animation/Notifies/` 下的通知蓝图 Cast 到迁移逻辑蓝图 `BP_PVP_BaseWeapon` / `BP_PVP_BaseCharacter`；新的 `APVPWeaponActor` 与 `BP_PVPBaseCharacter` 不是这些类 |
| 正确做法 | 本轮只保证 Montage 本体播放，把通知副作用列为已知限制；需要弹壳 / 弹匣时重写通知或改为接口调用，不要修改第三方原包 |
| 验证方式 | PIE 中 Montage 姿态与武器跟随正常；若必须验证弹壳，先接 `BP_PVP_BaseWeapon` 体系或补写通知 |

---

## 6. 校验、报告与冒烟

### 6.1 编译

```powershell
& "D:\UE\UE_5.9\UE_5.9\Engine\Build\BatchFiles\Build.bat" PVPDemoEditor Win64 Development `
  -Project="D:\UE\Unreal Projects\PVPDemo\PVPDemo.uproject" -WaitMutex -FromMsBuild
```

通过标准：`Result: Succeeded`，UHT 生成 `PVPAnimInstanceBase.generated.h` 并通过编译链接。

### 6.2 生成与结构校验

```text
UnrealEditor-Cmd.exe "D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -run=PythonScript ^
    -Script="D:/UE/Unreal Projects/PVPDemo/Content/Python/setup_pvp_anim_blueprint.py" ^
    -unattended -nosplash -nullrhi
python Content/Python/verify_pvp_asset_layout.py
```

通过标准：

- 脚本退出码 0，`Saved/PVPAnimBlueprintReport/report.md` 中 `result: PASS`；
- `builder_report.states` 为 `Ground / JumpStart / Falling / Land`，`transitions` 6 条；
- `builder.blendspace`、`asset.parent_class`、`asset.skeleton`、`asset.anim_graph`、`graph.compiled_report`、`graph.montage_slot=DefaultSlot` 全部 PASS；
- `wiring.*` 全部 PASS（Root / StateMachine / Montage Slot source + pose / Entry / BlendSpace X/Y / Run blend / 四个 state pose / Land 自动规则），`transition_rules=5`；
- 布局校验 `Animation/AnimBP.count=5`、`layout.total=80`、`refs.legacy_paths.absent` 通过。

武器 ABP / 角色 Slot 的生成与读回：

```text
UnrealEditor-Cmd.exe "D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -run=PythonScript ^
    -Script="D:/UE/Unreal Projects/PVPDemo/Content/Python/setup_pvp_weapon.py" ^
    -unattended -nosplash -nullrhi
```

期望 `Saved/PVPWeaponReport/report.md` 中 `result: PASS`，且 `weapon_abp.builder_slot=DefaultSlot`、`weapon_abp.wiring.*`、`character_slot.report_wired=true`、`character.locomotion_reference` 全部 PASS。

### 6.3 Persona / PIE 冒烟清单

| 操作 | 预期 |
| --- | --- |
| 进入 `L_PVP_Showcase` 出生 | `BP_PVPBaseCharacter` 的 Mesh Anim Class 为 `ABP_PVP_FP_Locomotion`，新 ABP 实际生效 |
| 站立不动 | BlendSpace 输入 `(0,0)`，播 Idle；`Speed≈0`、`bIsMoving=false`、`bIsOnGround=true` |
| W / A / S / D | BlendSpace 输入分别接近 `(0,100)` / `(-100,0)` / `(0,-100)` / `(100,0)`，播对应 Walk 样本 |
| 按住 Shift + W | `bIsRunning=true AND bIsMoving=true`，BlendList 切到 `A_PVP_FP_AR_Run_F_Loop` |
| 松开 Shift + W | BlendList 切回 BlendSpace，播 Walk_F |
| 按 Space | `Ground → JumpStart`（`bIsJumping`）→ `Falling`（`bIsFalling`）→ `Land`（`bIsOnGround`）→ 自动回 `Ground` |
| 从高处下落 | `Ground → Falling`（`bIsFalling`）→ `Land`（`bIsOnGround`）→ 自动回 `Ground` |
| 走近地图中的 AR | 自动拾取并附着到 `ik_hand_gun`；角色手臂与武器同步播放 Equip Montage |
| 拾取后移动 / 跳跃 | 武器跟随手部，不脱手、不明显抖动 |

**注意**：新 ABP 已挂到 `BP_PVPBaseCharacter`，可直接按上表做 PIE 冒烟；结构 / 编译校验已由生成脚本覆盖，视觉结果仍需人工确认。

---

## 7. 已知限制与后续路线

- **已接线，待视觉复核**：`ABP_PVP_FP_Locomotion` 已创建、编译并手动挂到 `BP_PVPBaseCharacter` 的 Mesh AnimClass；C++ 构造期默认仍是 `ABP_PVP_FP_BaseCharacter`。PIE / Persona 视觉确认尚未记录，生成脚本重跑后需要复查蓝图覆盖是否保留。
- **Ground 为方向平面**：当前 BlendSpace 只覆盖 Idle / Walk；Run 由独立序列混合。若后续有速度分层需求，应新建带 Speed 轴的混合空间或增加速度混合，而不是把 `Speed` 直接接到现有轴。
- **单实例**：当前只有一个基类，1P 手臂 / 3P 全身未拆分；后续按规划派生 `UPVPArmsAnimInstance` / `UPVPBodyAnimInstance`。
- **无 Look / AimOffset 变量**：视角旋转不参与动画；瞄准、蹲伏、持枪等动作未实现，无对应变量。
- **Montage 已接入 / GAS 未接入**：角色 / 武器成对 Montage 已通过 `DefaultSlot` 播放；GAS 双网格 Montage 任务未实现。
- **第三方通知副作用**：`AN_PVP_*` / `ANS_PVP_*` Cast 到迁移逻辑蓝图，新的武器 / 角色类不生成弹壳 / 弹匣（见踩坑 12）。
- **无程序化动画**：Sway / Bob / Recoil / ADS 对齐 / 贴墙收枪均未实现。
- **网络**：采样的 `bIsRunning` 与 `MaxWalkSpeed` 目前只反映本地 / 自主代理状态，权威速度复制化后需复核变量语义。

---

## 8. 相关文件

- `Source/PVPDemo/Animation/PVPAnimInstanceBase.h/.cpp` — 本模块实现。
- `Source/PVPDemoEditor/PVPAnimBlueprintBuilder.h/.cpp` — 动画图构建器（含 `BuildWeaponAnimBlueprint` / `EnsureMontageSlot`）。
- `Content/Python/setup_pvp_anim_blueprint.py` — 生成 / 校验驱动脚本。
- `Content/Python/setup_pvp_weapon.py` — 武器 ABP / 角色 Slot 生成与读回脚本。
- `Content/Python/pvp_logic_layout.py` — 目标资产路径与计数（`Animation/AnimBP`，总数 80）。
- `Content/Python/verify_pvp_asset_layout.py` — 离线布局校验。
- `Saved/PVPWeaponReport/report.md` — 武器 ABP / 角色 Slot / 武器 BP / 地图拾取物读回报告。
- `Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp` — 被采集的逻辑状态来源（`bIsRunning` / `RunSpeed` / `MaxWalkSpeed`）。
- `Source/PVPDemo/PVPDemo.Build.cs` — 模块依赖（本轮未修改）。
- `Docs/Animation/README.md` — 变量权威表与接线入口。
- `Docs/Animation/Animation-Integration.md` — 依赖与接口契约。
- `Docs/Character/README.md` — 角色状态与输入消费速查。
- `Docs/Input/README.md` — Move / Look / Jump / Run 动作权威表。
