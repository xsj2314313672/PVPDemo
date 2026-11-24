# PVPDemo 源码目录结构与职责说明

本文档描述 `Source/PVPDemo/` 下各文件夹的**分类**与**预计在项目中的作用**，对应 FPS 3C（Character / Camera / Control）技术方案的分层架构。

> 说明：文中的"FPS 3C"是技术方案名称（第一人称射击品类），**工程代码统一使用 `PVP` 作为类与文件前缀**（如 `APVPCharacter`），导出宏为 `PVPDEMO_API`。

> 当前状态：`Character/` 已落地 `PVPBaseCharacter.h/.cpp`（基础第一人称角色：胶囊与 Mesh 默认值对齐参考蓝图 `BP_TFA_BaseCharacter`，`CameraFP` 挂在 `head` 骨骼，已接入 Move / Look / Jump / Run / Aim / Fire 原生输入、走跑速度切换与瞄准 FOV 变焦）；`Input/` 已落地 `PVPInputConfig`（Tag ↔ InputAction 数据资产）与 `PVPInputComponent`（按 Tag 绑定 Enhanced Input）；`GAS/` 已落地 `PVPGameplayTags`（`InputTag.Move/Look/Jump/Run/Aim/Fire` 六个原生 Tag，其余 GAS 能力仍未搭建）；`Game/` 已落地 `PVPGameMode.h/.cpp` 与 `PVPPlayerController.h/.cpp`（会话与玩家入口：默认 Pawn / 玩家控制器、初始化与开场日志，PlayerController 在代码中默认加载并注入 `IMC_PVP_Default`）；`Animation/` 已落地 `PVPAnimInstanceBase.h/.cpp`（`UAnimInstance` 基类：游戏线程采样、动画线程计算 Move / Run / Jump / Aim locomotion 变量，含 `bIsAiming`）与逻辑资产 `ABP_PVP_FP_Locomotion`（四状态 locomotion 图 + Ground 内瞄准混合，已在 `BP_PVPBaseCharacter` 上接线；C++ 构造期默认仍为 `ABP_PVP_FP_BaseCharacter`）；`Equipment/` 已落地 `PVPWeaponActor.h/.cpp` 与 `PVPEquipmentComponent.h/.cpp`（1P/3P 双网格武器 Actor、靠近自动拾取、`ik_hand_gun` 附着、角色 / 武器成对 Montage），`PVPBaseCharacter` 构造期已挂 `EquipmentComponent`，逻辑资产新增 `BP_PVP_Weapon_AR` 与 `ABP_PVP_FP_Weapon_AR`；`Equipment/` 另落地配件槽类型与配件目录（`PVPWeaponAttachmentTypes.h`、`PVPWeaponAttachmentCatalog.h/.cpp`），武器 Actor 提供配件查询 / 显隐 / 换网格 / 应用预设接口，`BP_PVP_Weapon_AR` 已装配基础型 4 件配件；编辑器期工具模块 `Source/PVPDemoEditor/` 已落地资产引用重定向库、动画蓝图构建器（含武器 ABP 构建 / 原地 Slot 插入）与武器配件装配构建器（BP 组件树 + SCS socket 写入 + 读回）。`Camera/`、`Camera/Modes/` 仍为空目录。本文其余内容为规划职责，按实际实现进度同步更新。

> **阶段说明**：项目处于初期，`Camera/` 与 `Camera/Modes/` 下规划的**相机模式栈仅为架构预留，当前阶段不实现、不接入**。初期先用简单相机方案（`CameraFP` 挂在角色 `head` 骨骼上，位置由第一人称动画驱动；视角旋转由 Controller 的 `ControlRotation` 提供，`bUsePawnControlRotation=true`）跑通角色与输入，待基础稳定后再引入模式栈。其他规划文件也均为后续阶段的落点。

---

## 1. 目录总览

```
Source/
├── PVPDemo/                         运行时模块（Type: Runtime）
│   ├── PVPDemo.Build.cs             模块依赖声明（已有，本次不改动）
│   ├── PVPDemo.cpp / .h             模块入口（已有）
│   ├── Character/                   角色层：角色本体、移动、瞄准、视角、原生输入绑定（已有 PVPBaseCharacter.h/.cpp、EquipmentComponent）
│   ├── Game/                        会话与玩家入口层：游戏模式、玩家控制器、默认 IMC 注入（已有）
│   ├── Camera/                      相机层：相机组件（模式栈为预留，初期不实现；当前为空）
│   │   └── Modes/                   相机模式子类（仅预留目录，空）
│   ├── Animation/                   动画层：AnimInstance 基类（已有 PVPAnimInstanceBase，含瞄准状态；程序化动画未实现）
│   ├── Equipment/                   装备层：武器 Actor、装备组件、配件槽类型与配件目录（已有 PVPWeaponActor、PVPEquipmentComponent、PVPWeaponAttachmentTypes、PVPWeaponAttachmentCatalog）
│   ├── Input/                       输入层：Tag ↔ InputAction 配置与绑定组件（已有 PVPInputConfig、PVPInputComponent）
│   └── GAS/                         技能层：GAS 基础、Montage 任务、GameplayTag（已有 PVPGameplayTags，仅输入 Tag）
└── PVPDemoEditor/                   编辑器期模块（Type: Editor，仅编辑器加载，不进入打包）
    ├── PVPDemoEditor.Build.cs       编辑器模块依赖：UnrealEd / AssetTools / AssetRegistry / Projects
    ├── PVPDemoEditor.cpp / .h       模块入口（IMPLEMENT_MODULE）
    ├── PVPAssetRemapLibrary.h/.cpp  资产引用批量重定向工具（供 `Content/Python/` 脚本调用）
    ├── PVPAnimBlueprintBuilder.h/.cpp   动画蓝图构建器（locomotion 四状态 + Ground 瞄准混合 + 武器 ABP + 原地 Slot + 序列转 Montage）
    └── PVPWeaponAssemblyBuilder.h/.cpp  武器配件装配构建器（BP 组件树 + SCS socket 写入 + 读回报告）
```

整体遵循**单向数据流**：

```
输入(Input) ──► 权威状态(Character / GAS) ──► 表现(Animation / Camera)
```

表现层（动画、相机）只读游戏逻辑状态，不反向修改逻辑状态，这是多人联机下"表现不同步不影响逻辑"的基础。

---

## 2. 各文件夹分类与职责

### 2.1 `Character/` — 角色层（核心逻辑）

- **分类**：运行时核心逻辑 / 角色与移动
- **预计作用**：承载玩家角色的实体、网络预测移动、双网格可见性管理，以及供相机层读取数据的解耦接口。

| 规划文件 | 预计职责 |
| --- | --- |
| `PVPBaseCharacter.h/.cpp`（**已实现**） | 角色本体：`ACharacter` 子类，按参考蓝图默认值设置胶囊（半径 5 / 半高 88）、`CharacterMesh0` 相对变换与 `BaseEyeHeight`（64 / 蹲伏 32）；持有 `CameraFP`（`UCameraComponent`，挂 `head` 骨骼、FOV 100、`bUsePawnControlRotation=true`：位置随 head 骨骼姿态，视角旋转随 Controller 的 Yaw/Pitch）。已通过 `OverrideInputComponentClass` 使用 `UPVPInputComponent`，在 `SetupPlayerInputComponent` 中按 Tag 绑定 Move（视角朝向平面移动）/ Look（写 Controller Yaw/Pitch）/ Jump（Started/Completed）/ Run（按住切 `MaxWalkSpeed`，`WalkSpeed` 300 / `RunSpeed` 600 可调）/ Aim（按住 `bIsAiming`，FOV 100↔70、瞄准期间走速）/ Fire（播放腰射或瞄准成对 Montage），`bIsRunning` / `bIsAiming` 仅供表现层读取；构造期创建 `EquipmentComponent`（装备层，见 2.4）。当前为第一人称单网格，1P 手臂 + 3P 全身双网格与 `BecomeViewTarget` 回调待后续接入 |
| `PVPCharacterMovementComponent.h/.cpp` | 扩展 `UCharacterMovementComponent`：冲刺、探头的网络预测，移动状态只依赖 SavedMove 压缩标志 |
| `PVPPerspectiveComponent.h/.cpp` | 统一管理 1P/3P 可见性、阴影（隐藏身体保留投影）、第一人称渲染类型、不可见时 Tick 优化；预留可见下半身开关 |
| `PVPCameraTargetInterface.h` | 相机读取角色数据的纯接口，使相机不依赖具体角色类（载具、炮台、无人机亦可作为相机目标） |

> **当前实现与规划差异**：`Camera/` 层尚未落地，`CameraFP` 暂时内置于 `Character/PVPBaseCharacter`：相机位置跟随 `head` 骨骼（头 bob / 蹲伏姿态），视角旋转由 Controller 的 `ControlRotation` 提供（`bUsePawnControlRotation=true`），动画不参与视角旋转；待相机系统进入实现阶段再迁出到 `Camera/`。`PVPBaseCharacter.cpp` 通过 `ConstructorHelpers` 引用的第一人称网格与动画蓝图已改为工程自有资产：`/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple` 与 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter`，不再依赖 `/Game/InfimaGames` 原包。

**协作关系**：接收 `Input/` 的输入意图；将状态提供给 `Animation/` 与 `Camera/`；ASC 挂在 PlayerState 上由角色初始化。

> 角色模块文档见 `Docs/Character/`：`README.md`（导航与组件默认值 / 移动状态 / 输入消费速查）、`Character-Dev.md`（对照参考蓝图的改造 SOP 与踩坑清单）、`Character-Integration.md`（依赖方向、`CameraFP` 与只读状态接口、相机迁出边界）。

---

### 2.2 `Camera/` — 相机层（表现）

- **分类**：表现层 / 相机系统
- **预计作用（规划）**：以"相机模式栈"管理所有视角（第一人称、ADS、第三人称、死亡、观战、技能镜头），按权重平滑混合，并处理镜头碰撞与防穿帮。
- **当前阶段**：项目处于初期，**模式栈仅为预留**，暂不实现。当前 1P 视角由 `Character/PVPBaseCharacter` 的 `CameraFP` 承担：相机位置挂在 `head` 骨骼上跟随动画（头 bob / 蹲伏姿态），视角旋转由 Controller 的 `ControlRotation` 提供（`bUsePawnControlRotation=true`）。尚未单独实现相机组件、镜头碰撞与防穿帮；`PVPCameraComponent`、`PVPCameraMode` 及 `Modes/` 下的具体模式待后续阶段引入。

| 规划文件 | 预计职责 | 阶段 |
| --- | --- | --- |
| `PVPCameraComponent.h/.cpp` | 相机组件：规划中持有模式栈并决定当前相机模式 | 尚未实现（1P 视角暂由 `CameraFP` 承担） |
| `PVPCameraMode.h/.cpp` | 模式基类 + 栈容器：管理激活/混合权重，逐帧计算并混合 View 参数 | **预留，暂不实现** |
| `PVPPlayerCameraManager.h/.cpp` | 相机管理器：规划承载 CameraModifier（后坐力、受击）与 CameraShake | 按需要逐步引入 |

**协作关系**：通过 `Character/PVPCameraTargetInterface` 只读角色状态；规划中可被 `GAS/` 的技能镜头覆盖调用；基于 `ViewTarget` 工作，天然兼容回放与观战。

#### 2.2.1 `Camera/Modes/` — 相机模式子类

- **分类**：表现层 / 相机模式实现
- **当前阶段**：**仅预留目录，不创建任何相机模式类**。下表为后续阶段的设计规划，新增视角时只需增加一个子类，无需改动已有代码。

| 规划文件（预留） | 预计职责 |
| --- | --- |
| `PVPCameraMode_FirstPerson` | 眼高平滑、探头、贴墙碰撞、独立手臂 FOV 与 FP 缩放 |
| `PVPCameraMode_ADS` | 瞄准视角，FOV 由当前武器的倍镜决定（经接口读取，不依赖装备模块） |
| `PVPCameraMode_ThirdPerson` | 观战、死亡过渡、强制第三人称调试 |
| `PVPCameraMode_Death` | 死亡镜头平滑拉出，避免看到自身头部内部 |

---

### 2.3 `Animation/` — 动画层（表现）

- **分类**：表现层 / 动画系统
- **作用**：当前已落地 AnimInstance 基类，把角色 Move / Run / Jump 状态采集为动画蓝图可读变量；武器动画蓝图与 `DefaultSlot` 已落地，双 AnimInstance 拆分与程序化动画仍为后续规划。

| 文件 | 职责 |
| --- | --- |
| `PVPAnimInstanceBase.h/.cpp`（**已实现**） | `UAnimInstance` 基类：`NativeInitializeAnimation` 缓存 `APVPBaseCharacter` 与移动组件，`NativeUpdateAnimation` 在游戏线程采样，`NativeThreadSafeUpdateAnimation` 在动画工作线程输出 13 个 `BlueprintReadOnly` locomotion 变量（`Velocity` / `Speed` / `SpeedZ` / `MaxSpeed` / `SpeedRatio` / `Direction` / `bIsMoving` / `bIsRunning` / `bIsAiming` / `bIsOnGround` / `bIsInAir` / `bIsJumping` / `bIsFalling`）；不提供蓝图写入，Look 不参与动画变量 |
| `PVPBodyAnimInstance.h/.cpp` | 3P 全身：AimOffset、原地转身、探头脊柱 Roll |
| `PVPArmsAnimInstance.h/.cpp` | 1P 手臂状态与武器层链接 |
| `PVPWeaponAnimLayerBase.h/.cpp` | 武器动画层父类：从武器 DataAsset 读取序列，新增武器无需改主 ABP |
| `PVPProceduralAnimComponent.h/.cpp` | 程序化第一人称：Sway / Bob / Recoil / ADS 对齐 / 贴墙收枪 |
| `PVPSpring.h` | 弹簧阻尼工具类，供程序化动画使用 |

> **武器动画与 Slot**：`ABP_PVP_FP_Weapon_AR`（`A_PVP_WEP_AR_Reference` → `DefaultSlot` → Root）由 `PVPAnimBlueprintBuilder::BuildWeaponAnimBlueprint` 生成；`ABP_PVP_FP_Locomotion` 由 `BuildLocomotionAnimBlueprint` 生成四状态 + Ground 内 `bIsAiming` 的 Standing/Aimed 混合，并由 `EnsureMontageSlot` 原地插入 `DefaultSlot`；瞄准射击 `AM_PVP_FP_AR_Fire_Aimed` 由 `BuildMontageFromSequence` 从序列生成，所有 Montage 的 Slot 名均为 `DefaultSlot`。

**协作关系**：只读 `Character/` 与 `GAS/` 的状态；装备切换由 `Equipment/` 驱动动画层；相机位姿来自 `Camera/`。

> **接线状态**：`APVPBaseCharacter` 构造函数默认 AnimClass 仍是工程自有 `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter`（C++ 未改）；`BP_PVPBaseCharacter` 的 Mesh AnimClass 已在编辑器中手动切到 `ABP_PVP_FP_Locomotion`（由编辑器构建器生成的 `Ground` / `JumpStart` / `Falling` / `Land` 四状态图，Ground 内含 `bIsAiming` 瞄准混合），实例覆盖默认值并实际生效；`setup_pvp_weapon.py` 会读取 CDO 的 Mesh AnimClass 校验覆盖未丢，视觉 / PIE 复核仍待做。

> 动画模块文档见 `Docs/Animation/`：`README.md`（变量权威表与接线入口）、`Animation-Dev.md`（线程安全设计、新增变量 SOP 与踩坑）、`Animation-Integration.md`（依赖与接口契约）。

---

### 2.4 `Equipment/` — 装备层（已实现）

- **分类**：玩法数据 / 装备与武器
- **当前作用**：武器 Actor 同时持有 1P/3P 网格（3P 暂隐藏），负责拾取触发、附着 / 卸下 / 丢弃与成对 Montage；装备组件维护"一次一把"当前武器。武器配置采用 Actor 级属性；配件槽位类型与配件目录已落地（`PVPWeaponAttachmentTypes.h` / `PVPWeaponAttachmentCatalog.h/.cpp`），完整武器 DataAsset 仍留待多武器阶段。

| 文件 | 职责 |
| --- | --- |
| `PVPWeaponActor.h/.cpp`（**已实现**） | `APVPWeaponActor`：`SceneRoot` + `Mesh1P` / `Mesh3P` + `PickupSphere`；Actor 级网格 / AnimClass / 挂点 / 拾取半径 / 四组成对 Montage（Equip / Fire / AimedFire / Reload）配置，`OnConstruction` 应用；自动拾取、`ik_hand_gun` 附着、丢弃冷却、枪口查询、1P/3P 切换；`OnConstruction` / `BeginPlay` 扫描带 `PVP.Attachment.<Slot>` 标签的组件建槽位表，提供配件查询 / 显隐 / 换网格 / 应用预设接口 |
| `PVPEquipmentComponent.h/.cpp`（**已实现**） | `UPVPEquipmentComponent`：`CurrentWeapon`、"一次一把"、`EquipWeapon` / `UnequipCurrentWeapon` / `DropCurrentWeapon`；只依赖 `ACharacter` 与武器 Actor |
| `PVPWeaponAttachmentTypes.h`（**已实现**） | 配件槽位契约的唯一事实来源：`E_PVPWeaponAttachmentSlot`（None / Handguard / FrontSight / RearSight / Optic / ForeGrip / Muzzle / Laser / Magazine）、`FPVPWeaponAttachmentSpec`、`FPVPWeaponAttachmentOption`、`FPVPWeaponAttachmentSlotDefinition`、`FPVPWeaponAttachmentLoadout`，以及 `ATT_<Slot>` 命名、`PVP.Attachment.<Slot>` 标签、默认 socket 与父级映射的辅助函数 |
| `PVPWeaponAttachmentCatalog.h/.cpp`（**已实现**） | `UPVPWeaponAttachmentCatalog : UDataAsset`：全量槽位定义（含每个槽位的候选网格）与 `Basic` / `Tactical` 两套预设，提供按槽位 / 按预设查询与 `BuildLoadoutAssembly` / `BuildDefaultAssembly` |
| `PVPWeaponDefinition.h` | 仍为规划：DataAsset 化的武器网格 / 动画层 / Montage / 手感参数，供多武器阶段使用 |

**协作关系**：`Character → Equipment` 单向，角色构造期持有 `EquipmentComponent`；`Equipment/` 向 `Animation/` 发起 `Montage_Play`（`DefaultSlot`），为后续 `GAS/` 提供 `GetMuzzleLocation` / `GetActiveMesh`；武器 Actor 推荐不复制，各端本地生成。对应逻辑资产 `BP_PVP_Weapon_AR`、`ABP_PVP_FP_Weapon_AR` 与地图拾取物由 `Content/Python/setup_pvp_weapon.py` 生成；配件网格归位由 `Content/Python/reorg_pvp_weapon_attachments.py` 完成，配件目录与蓝图内装配由 `Content/Python/setup_pvp_weapon_assembly.py` + `PVPWeaponAssemblyBuilder` 完成。

> **类型落点契约**：与 C++ 契约绑定的类型定义随源码层（本例为 `Equipment/PVPWeaponAttachmentTypes.h`），`Content/PVP/Types/` 只放蓝图侧独立类型资产（`E_PVP_*` / `S_PVP_*` 蓝图资产）。两者不得互相镜像同一枚举，避免双份事实来源漂移。

> 装备模块文档见 `Docs/Equipment/`：`README.md`（导航与武器 / 装备 / Montage / 配件权威表）、`Equipment-Dev.md`（生成与装配 SOP、踩坑）、`Equipment-Integration.md`（依赖方向与接口契约）、`Equipment-Attachments.md`（配件分类、插槽映射、两套装配方案与换装 SOP）。

---

### 2.5 `Input/` — 输入层（配置 + 绑定）

- **分类**：运行时输入配置 / 绑定
- **作用**：把 InputAction 与 GameplayTag 绑定，使角色原生输入与后续 GAS 技能输入共用一套配置；同时提供按 Tag 绑定的 EnhancedInputComponent 子类。

| 已实现文件 | 职责 |
| --- | --- |
| `PVPInputConfig.h/.cpp` | `FPVPInputAction`（InputAction + InputTag）、`UPVPInputConfig : UDataAsset`（`NativeInputActions` + `FindNativeInputActionForTag`）、`FPVPInputMappingContextAndPriority`（IMC + Priority）；构造中按 Move / Look / Jump / Run / Aim / Fire 预置六个 Tag 槽位，`EnsureNativeInputActionSlots` 给旧 4 槽资产补槽，`SetNativeInputActionByIndex` 供编辑器 / 脚本按槽位写入 IA（GameplayTag 无法从脚本构造，因此由 C++ 预置） |
| `PVPInputComponent.h/.cpp` | `UEnhancedInputComponent` 子类：`BindNativeAction(Config, Tag, TriggerEvent, Object, Func)` 按 Tag 查 Action 并绑定，找不到时返回 nullptr 由调用方告警跳过 |

**协作关系**：配置资产为 `Content/PVP/Input/DA_PVP_InputConfig`（由 `Content/Python/setup_pvp_input.py` 生成并挂到 `BP_PVPBaseCharacter` CDO），引用同目录 6 个 `IA_PVP_*`（Move / Look / Jump / Run / ADS / Fire），旧 4 槽资产由 `EnsureNativeInputActionSlots` 原地迁移；映射上下文 `IMC_PVP_Default` 由 `APVPPlayerController` 在代码中默认加载（`DefaultMappingContexts`）。原生 Tag 声明位于 `GAS/PVPGameplayTags.h/.cpp`。工程已在 `Config/DefaultInput.ini` 启用 Enhanced Input，且**未修改**该文件——Pawn 通过 `APawn::OverrideInputComponentClass`（UE 5.9）指定 `UPVPInputComponent`。

> 输入模块文档见 `Docs/Input/`：`README.md`（导航与已实现动作 / 绑定 / 回调速查）、`Input-Dev.md`（开发流程、新增动作 SOP、踩坑清单，含 **Look 俯仰极性约定：IMC 的 `IA_PVP_Look` 必须保留 Negate(Y)**）、`Input-Integration.md`（依赖、模块交互与对外接口）。

---

### 2.6 `GAS/` — 技能层（GAS 基础）

- **分类**：技能系统 / 网络权威逻辑
- **预计作用**：从零搭建 GAS 基础设施，修正多网格下的关键坑点，为开火、瞄准、换弹、冲刺等技能提供支撑。

| 规划文件 | 预计职责 |
| --- | --- |
| `PVPGameplayTags.h/.cpp`（**已实现，仅输入 Tag**） | 原生 GameplayTag 声明与定义：`InputTag.Move/Look/Jump/Run/Aim/Fire`，供 `UPVPInputConfig` 与角色输入绑定使用；`State.*`、`GameplayCue.*` 等标签留待 GAS 阶段扩展 |
| `PVPAbilitySystemGlobals.h/.cpp` | 自定义 Globals：修正 `AllocAbilityActorInfo` 使用的 SkeletalMeshComponent，避免 Montage 播到 1P 手臂 |
| `PVPAbilityTask_PlayDualMontage.h/.cpp` | 双网格 Montage 播放任务：本地 1P 表现 + 服务器 3P 复制 |

**协作关系**：ASC 挂在 PlayerState 上；驱动 `Animation/` 的 Tag 属性映射、规划中的 `Camera/` 技能镜头与后坐、`Equipment/` 的武器表现。**当前仅落地输入相关原生 Tag，ASC、AbilitySystemGlobals、AbilityTask 等 GAS 基础设施仍未搭建，是后续阶段的起点。**

> 技能层模块文档见 `Docs/GAS/`：`README.md`（导航与原生 Tag 命名 / 语义速查、未实现能力清单）、`GAS-Dev.md`（新增 Tag SOP、声明与定义踩坑、GAS 接入路线）、`GAS-Integration.md`（Tag 契约、消费方与禁止依赖方向）。

---

### 2.7 `Game/` — 会话与玩家入口层

- **分类**：运行时核心逻辑 / 会话规则与玩家入口
- **作用**：承载"一局怎么开始"与"玩家如何进入世界"：游戏模式决定默认 Pawn、玩家控制器与出生规则；玩家控制器负责输入模式、附身关系与对当前角色的只读访问。与 `Character/`（角色实体）解耦，依赖方向 `Game → Character` 单向。

| 规划文件 | 预计职责 |
| --- | --- |
| `PVPGameMode.h/.cpp`（**已实现**） | `AGameModeBase` 子类：构造中设 `PlayerControllerClass = APVPPlayerController`、`DefaultPawnClass = APVPBaseCharacter`、`bStartPlayersAsSpectators = false`；覆写 `InitGame` / `StartPlay`，仅调用 `Super` 并各打一条日志。**不硬编码任何蓝图路径**，指向 `BP_PVPBaseCharacter` 的职责由 `BP_PVPGameMode` 的 CDO 承担；GameState / PlayerState / 对局流程为后续扩展位 |
| `PVPPlayerController.h/.cpp`（**已实现**） | `APlayerController` 子类：构造关闭鼠标光标，并用 `ConstructorHelpers` 把 `/Game/PVP/Input/IMC_PVP_Default` 填入 `DefaultMappingContexts`（`FPVPInputMappingContextAndPriority` 数组，蓝图可覆盖/扩展）；`BeginPlay` 切换 `FInputModeGameOnly`，通过 `UEnhancedInputLocalPlayerSubsystem` 先 `ClearAllMappings` 再按优先级 `AddMappingContext`，并记录上下文数量/缺失告警；`SetupInputComponent` 仅调用 `Super`（原生绑定在 Pawn 的 `UPVPInputComponent` 上）；`OnPossess` 记录实际 Pawn 类名与包路径；`GetPVPCharacter()` 为 `BlueprintPure` 的 `Cast<APVPBaseCharacter>(GetPawn())` |

**协作关系**：`Game/` 引用 `Character/` 的类与接口；`Character/` 不感知 `Game/`。日志类别定义在各 `.cpp` 内（`DEFINE_LOG_CATEGORY_STATIC`），头文件只放声明。

> 会话与入口模块文档见 `Docs/Game/`：`README.md`（导航与入口装配 / 启动时序 / 关键日志速查）、`Game-Dev.md`（默认类替换 SOP、IMC 注入与关卡装配踩坑）、`Game-Integration.md`（依赖方向、`DefaultMappingContexts` 与 `GetPVPCharacter` 契约、变更影响面）。

---

### 2.8 `PVPDemoEditor/` — 编辑器期工具模块

- **分类**：编辑器工具 / 资产维护
- **作用**：承载只能在编辑器环境使用的资产维护能力，避免把 `UnrealEd` / `AssetTools` 依赖带进运行时模块。模块类型 `Editor`，`LoadingPhase` 为 `PostEngineInit`，只在编辑器目标加载，不参与打包。

| 文件 | 职责 |
| --- | --- |
| `PVPAssetRemapLibrary.h/.cpp`（**已实现**） | `UBlueprintFunctionLibrary`，暴露 `RemapAssetReferences(旧包路径[], 新包路径[], 扫描前缀[], 报告路径)`：按映射构造 `FReplaceRequest` 批量调用 `ObjectTools::ForceReplaceReferences`，只在前缀命中的包内替换，随后标脏保存并写报告。供 `Content/Python/` 脚本驱动，用于"自有资产去第三方依赖"的一次性重定向 |
| `PVPAnimBlueprintBuilder.h/.cpp`（**已实现**） | `UBlueprintFunctionLibrary`，`BuildLocomotionAnimBlueprint(...)`：用 `UAnimBlueprintFactory` 创建 `ABP_PVP_FP_Locomotion`，用 `FGraphNodeCreator` 搭 `Ground` / `JumpStart` / `Falling` / `Land` 四状态、`BlendListByBool`（Run 混合）、`BlendSpacePlayer`、`SequencePlayer` 与 6 条过渡规则，编译并写 JSON 报告。供 `setup_pvp_anim_blueprint.py` 调用 |
| `PVPWeaponAssemblyBuilder.h/.cpp`（**已实现**） | `UBlueprintFunctionLibrary`，`AssembleAttachments(BlueprintPath, Specs, bRemoveUnlisted, ReportPath)` / `ReadbackAttachments(...)`：按槽位在武器蓝图 SCS 中增改 `ATT_<Slot>` `UStaticMeshComponent`、写 `USCS_Node::AttachToName`（socket 挂点只能 C++ 写）、设置 `PVP.Attachment.<Slot>` 标签与内置网格、编译保存，并输出写入 / 读回 JSON。供 `setup_pvp_weapon_assembly.py` 调用 |

**依赖**：`Core` / `CoreUObject` / `Engine` / `UnrealEd` / `AssetTools` / `AssetRegistry` / `Projects`；动画图构建依赖 `AnimGraph` / `AnimGraphRuntime` / `BlueprintGraph`；配件装配构建器在 `PublicDependencyModuleNames` 新增 `PVPDemo`（共享 `E_PVPWeaponAttachmentSlot` / `FPVPWeaponAttachmentSpec`，方向为 Editor → Runtime 单向）。**运行时模块 `PVPDemo.Build.cs` 无新增依赖**（`AGameModeBase`、`APlayerController`、`UStaticMeshComponent`、`UDataAsset` 均属 `Engine`）。

---

## 3. 根目录文件

| 文件 | 分类 | 作用 |
| --- | --- | --- |
| `PVPDemo.Build.cs` | 模块规则 | 声明模块依赖。当前已含 Core / CoreUObject / Engine / InputCore / EnhancedInput / GameplayAbilities / GameplayTags / GameplayTasks / AnimGraphRuntime / Niagara |
| `PVPDemo.cpp` / `PVPDemo.h` | 模块入口 | `IMPLEMENT_PRIMARY_GAME_MODULE`，目前为模板默认实现 |

---

## 4. 命名与依赖约定

- **命名**：导出宏统一使用 `PVPDEMO_API`，类与文件前缀统一使用 `PVP`（如 `APVPCharacter`、`UPVPPerspectiveComponent`）。技术方案文档中的 `FPSGame` / `FPSGAME_API` 仅为文档示例，**不引入工程**。
- **UE 规范**：`U` 用于 UObject 派生类、`A` 用于 Actor、`F` 用于普通结构体、`I` 用于接口、`E` 用于枚举。
- **依赖方向**：`Input → Character/GAS → Animation/Camera`，表现层不得反向修改逻辑状态；`Camera/` 通过接口解耦角色，`GAS/` 不直接依赖具体相机类；`Game → Character` 单向，`Character/` 不感知 `Game/`。
- **分层约束强度**：同一运行时模块内的分层（如 `Game/` 不得反向被 `Character/` 包含）是**约定约束**，UBT 无法在编译期强制；发现依赖成环时再抽取薄的公共模块，不提前拆分。
- **依赖变更结论**：本次新增 `Game/` 层与 `PVPDemoEditor/` 编辑器模块，`AGameModeBase` / `APlayerController` 属 `Engine` 模块，**运行时 `PVPDemo.Build.cs` 无新增依赖**；编辑器模块的依赖在其自身 `PVPDemoEditor.Build.cs` 中声明。
- **输入依赖结论**：新增 `Input/` 与 `GAS/PVPGameplayTags` 只用 `Build.cs` 已声明的 `EnhancedInput`、`GameplayTags`，**无新增模块依赖**；未修改 `Config/DefaultInput.ini`，改用 UE 5.9 的 `APawn::OverrideInputComponentClass` 指定 `UPVPInputComponent`。
- **动画依赖结论**：新增 `Animation/PVPAnimInstanceBase` 只用已声明的 `Engine` 与 `AnimGraphRuntime`（`UKismetAnimationLibrary::CalculateDirection`），**无新增模块依赖**，`PVPDemo.Build.cs` 未修改。
- **装备依赖结论**：新增 `Equipment/` 只用 `Engine`（`USkeletalMeshComponent` / `UAnimInstance` / `UAnimMontage` / `USphereComponent`），**运行时 `PVPDemo.Build.cs` 无新增依赖**；依赖方向 `Character → Equipment` 单向，`Equipment/` 不包含 `Character/` 具体类，也不依赖 `Game/` / `GAS/`；编辑器构建器沿用既有 `AnimGraph` / `AnimGraphRuntime` / `BlueprintGraph`。
- **动画蓝图构建依赖结论**：`PVPDemoEditor.Build.cs` 新增 `AnimGraph` / `AnimGraphRuntime` / `BlueprintGraph`（仅编辑器期，用于 `UAnimGraphNode_*`、状态机与 K2 规则节点）；运行时模块不引入这些依赖。
- **配件依赖结论**：`Equipment/PVPWeaponAttachmentTypes.h`、`Equipment/PVPWeaponAttachmentCatalog.h/.cpp` 与 `APVPWeaponActor` 的配件接口只用 `Engine`（`UDataAsset` / `UStaticMeshComponent` / `UStaticMesh`），**运行时 `PVPDemo.Build.cs` 无新增依赖**；编辑器构建器直接操作 `USimpleConstructionScript` / `USCS_Node`（`Engine` + `UnrealEd`），并在 `PVPDemoEditor.Build.cs` 的 `PublicDependencyModuleNames` 新增 `PVPDemo`。依赖方向保持 `Editor → Runtime` 单向，运行时不感知编辑器类型。
- **头文件**：优先前置声明，具体类型包含放在 `.cpp`；仅在需要反射处使用 `UCLASS/USTRUCT/UENUM`。

---

## 5. 与 Content 目录的关系

- `Content/PVPDemo/`：工程自有**美术**资产目录，按功能模块与源码分层相互对应；逻辑资产（蓝图、DataAsset、GA、地图等）不放入该目录，统一放 `Content/PVP/`。
- `Content/PVP/`：工程自有**逻辑**资产目录，按职责分为 `Animation/`（动画蓝图 / 动画接口 / 动画通知）、`Actors/`（从第三方复制的 Actor 蓝图）、`Characters/`（角色蓝图）、`Data/`（数据表）、`Game/`（游戏模式与控制器蓝图）、`Input/`（IA / IMC / 输入配置）、`Maps/`（关卡）、`Types/`（枚举与结构体）、`UI/`（控件蓝图），详见 `Docs/Content-Structure.md`。两个根均可引用 `/Script/PVPDemo.*` 原生类，且均不再引用 `/Game/InfimaGames`。
- `Content/InfimaGames/TacticalFPSAnimations/`：已导入的第三方 FPS 动画包，提供骨骼、武器动画、IMC/IA 等资产；实际 1P 网格可用挂点为 `ik_hand_gun`（`weapon_r` 未落到 `SKM_PVP_FP_Manny_Simple`），武器 Socket 为 `SOCKET_Muzzle` / `SOCKET_Sight_Rear`，接入前需核查是否满足方案要求。
- `Content/PVPDemo/Weapons/AssaultRifle/`：`Meshes/` 只留枪身本体与弹药 / 枪箱，散件按槽位归入 `Attachments/<Group>/`（Handguard / Sight / Optic / Grip / Muzzle / Laser / Magazine）；槽位与插槽的对应关系见 `Docs/Equipment/Equipment-Attachments.md`，落位由 `Content/Python/reorg_pvp_weapon_attachments.py` 幂等维护。

---

## 6. 后续演进（当前不在范围内）

- 相机：当前先用简单相机；**相机模式栈属于中后期内容**，待角色、输入、动画稳定后再引入；后续可再迁移到 `Gameplay Cameras`（Camera Rig）。
- 动画：`Pose Search`（Motion Matching）+ `Chooser` 替换 Locomotion 状态机；`Control Rig` 用于程序化 IK。
- 移动：`Mover 2.0` 独立分支评估，主线仍用 CMC 子类。
- 网络：`Iris` 复制系统、回放与击杀回放。
