# PVPDemo Content 资产目录结构与职责说明

本文档描述 `Content/PVPDemo/` 下各**功能模块文件夹**及其子文件夹中资产的含义与作用。该目录是工程自有的美术资产库，按 FPS 玩法功能模块组织，与 `Source/PVPDemo/` 的源码分层相互呼应。

> **来源说明**：本目录资产从第三方包 `Content/InfimaGames/TacticalFPSAnimations` 复制而来，原始包完整保留且未被修改。迁移时仅纳入美术表现类资产（贴图、网格体、材质、动画、特效、音频、字体等）；逻辑资产（蓝图、蓝图接口、输入映射、数据表、地图）统一放在 `Content/PVP/`，见第 4 节。

> **命名说明**：资产统一改为 `PVP` 前缀并遵循 Unreal 官方类型前缀规范，例如 `T_TFA_Weapon → T_PVP_Weapon`。原包中的 `TFA` 前缀已被全部替换。

> **当前状态**：本目录共 **567 个美术资产**，`Content/PVP/` 共 **81 个逻辑资产**。`Content/Python/setup_pvp_game_framework.py` 已对两个根执行引用重定向并复核：**不存在任何指向第三方包的活引用**，仅剩 1 条指向原包中并不存在资产的悬空软引用（`SKM_Quinn`）。因此克隆工程后无需导入第三方包即可打开工程与运行自有地图。输入侧的 `DA_PVP_InputConfig` 由 `Content/Python/setup_pvp_input.py` 生成并挂到 `BP_PVPBaseCharacter`；`Animation/AnimBP/ABP_PVP_FP_Locomotion` 由 `Content/Python/setup_pvp_anim_blueprint.py` 生成并通过编译 / 状态机读回校验；武器侧的 `Animation/AnimBP/ABP_PVP_FP_Weapon_AR`、`Actors/BP_PVP_Weapon_AR` 与地图拾取物由 `Content/Python/setup_pvp_weapon.py` 生成，并给角色 ABP 原地插入 `DefaultSlot`；突击步枪散件已由 `Content/Python/reorg_pvp_weapon_attachments.py` 按槽位归入 `Weapons/AssaultRifle/Attachments/`，配件目录 `Data/DA_PVP_WeaponAttachmentCatalog` 与 `BP_PVP_Weapon_AR` 内的配件组件由 `Content/Python/setup_pvp_weapon_assembly.py` + `PVPWeaponAssemblyBuilder` 生成。逻辑资产已由 `Content/Python/reorg_pvp_logic_assets.py` 按职责归位：动画蓝图 / 动画接口 / 动画通知集中在 `Animation/`，枚举与结构体集中在 `Types/`，`Actors/` 只保留 Actor 蓝图；路由规则由 `Content/Python/pvp_logic_layout.py` 单点维护。

---

## 1. 目录总览

```
Content/PVPDemo/                       567   自有美术资产根
├── Audio/             65   音频：声音类、通用动作音效
│   ├── Classes/        2   声音类与混音效果预设
│   └── Common/        63   通用音效（Foley / Heal / Melee）
├── Characters/        47   角色：Mannequin 骨架/网格/材质/姿势
│   └── Mannequins/    47   角色模型全套资产
├── Common/             7   跨模块共享的母材质
│   └── Materials/      7
├── Environment/       48   场景环境：施工灯、地面/墙面/天空盒
├── Props/             11   可交互道具：注射器
├── UI/                 2   界面字体
│   └── Fonts/          2
├── VFX/               17   特效：枪口焰、烟雾
└── Weapons/          370   武器：突击步枪全套（含按槽位分类的散件）+ 通用激光
    ├── Common/         2   通用武器配件（激光束网格 / 材质）
    └── AssaultRifle/ 368   突击步枪全套资产
        ├── Attachments/ 20  按槽位归位的散件：Handguard 3 / Sight 4 / Optic 1 / Grip 2 / Muzzle 1 / Laser 1 / Magazine 8
        ├── Meshes/       7  枪身骨架 / 骨骼网格 / 物理资产 3 + 弹药 2 + 枪箱 2（非装配件保留原路径）
        ├── Materials/   12
        ├── Textures/    31
        ├── Animations/ 184
        └── Audio/      114

Content/PVP/                          81   自有逻辑资产根
├── Animation/         16   动画逻辑：动画蓝图、动画接口、动画通知
│   ├── AnimBP/         5   ABP_PVP_FP_BaseCharacter、ABP_PVP_FP_Locomotion、ABP_PVP_FP_Weapon_AR、ABP_PVP_TP_BaseCharacter、ABP_PVP_Manny_PostProcess
│   │   └── Environment/1   ABP_PVP_ConstructionLamp_Large 环境灯具动画蓝图
│   ├── Interfaces/     1   BPI_PVP_AnimationState 动画状态蓝图接口
│   └── Notifies/       9   5 个 AN_PVP_* 与 4 个 ANS_PVP_* 通用动画通知蓝图
├── Actors/            20   从第三方复制的 Actor 蓝图（墙体/灯具/信息板/配置/武器/弹匣/物理体/注射器）与 BP_PVP_Weapon_AR
├── Characters/         2   BP_PVPBaseCharacter、BP_PVP_BaseCharacter
├── Data/               2   DT_PVP_TextStyle 文本样式数据表、DA_PVP_WeaponAttachmentCatalog 配件目录
├── Game/               2   BP_PVPGameMode、BP_PVPPlayerController
├── Input/             31   29 个 IA_PVP_*、IMC_PVP_Default 与 DA_PVP_InputConfig 输入配置
├── Maps/               1   L_PVP_Showcase 工程自有关卡
├── Types/              5   4 个 E_PVP_* 枚举与 S_PVP_SpringData 结构体
└── UI/                 2   WBP_PVP_BodycamOverlay、WBP_PVP_Demo
```

---

## 2. 命名前缀对照

理解文件夹作用前，先明确资产名前缀所代表的资产类型（遵循 Unreal 官方规范）：

| 前缀 | 资产类型 | 说明 |
| --- | --- | --- |
| `T_` / `TC_` | Texture2D / TextureCube | 贴图；`TC_` 为立方体贴图（如 HDR 环境图） |
| `SM_` | StaticMesh | 静态网格体（不含骨骼的刚性模型） |
| `SK_` / `SKM_` | SkeletalMesh | 骨骼网格体（可播放动画的模型） |
| `SKEL_` | Skeleton | 骨架，定义骨骼层级与挂点 |
| `PHYS_` | PhysicsAsset | 物理资产，定义碰撞体与布娃娃约束 |
| `M_` / `MI_` | Material / MaterialInstanceConstant | 母材质 / 材质实例 |
| `A_` | AnimSequence 或 SoundWave | 动画序列（动画目录）或音频波形（音频目录），按所在文件夹区分 |
| `AM_` | AnimMontage | 动画蒙太奇，用于技能/动作分段播放 |
| `AN_` / `ANS_` | AnimNotify / AnimNotifyState | 动画通知 / 动画通知状态，在动画时间点触发逻辑 |
| `BS_` | BlendSpace | 混合空间，按参数混合动画（如移动速度） |
| `PA_` | PoseAsset | 姿势资产，保存单个骨骼姿态 |
| `CR_` / `IK_` / `RTG_` | ControlRig / IKRig / IKRetargeter | 控制绑定 / IK 解算 / 骨架重定向 |
| `NS_` / `NE_` | NiagaraSystem / NiagaraEmitter | Niagara 特效系统 / 发射器 |
| `SC_` / `SEF_` | SoundClass / SubmixEffectFilterPreset | 声音类 / 混音效果预设 |
| `Font_` / `Font_Face_` | Font / FontFace | 字体资源 / 字体 face |
| `BP_` / `BPI_` | Blueprint / BlueprintInterface | 蓝图 / 蓝图接口；逻辑资产，Actor 蓝图放 `Content/PVP/Actors/`，蓝图接口放 `Content/PVP/Animation/Interfaces/` |
| `ABP_` | AnimBlueprint | 动画蓝图；逻辑资产，放 `Content/PVP/Animation/AnimBP/`（环境类放 `AnimBP/Environment/`） |
| `WBP_` | WidgetBlueprint | 控件蓝图；逻辑资产，放 `Content/PVP/UI/` |
| `E_` / `S_` | UserDefinedEnum / UserDefinedStruct | 用户枚举 / 用户结构体；逻辑资产，放 `Content/PVP/Types/`。注意 `S_` 与美术前缀 `SM_`、`SK_`、`SKEL_` 不同，判断时按完整前缀区分 |
| `DA_` | DataAsset | 数据资产；逻辑资产，输入类放 `Content/PVP/Input/`，其余按模块放 `Content/PVP/Data/` |
| `L_` | World（Level） | 关卡地图；逻辑资产，放 `Content/PVP/Maps/` |

后缀常见约定：`_D` 基础色、`_N` 法线、`_ORM` AO/粗糙度/金属度打包贴图、`_MSR_MSK` 金属/高光/粗糙度遮罩、`_Cue` 音效提示（SoundCue）。`FP` 指第一人称，`TP` 指第三人称。

---

## 3. 各功能模块与子文件夹职责

### 3.1 `Audio/` — 音频模块

- **分类**：表现层 / 声音
- **作用**：存放全局声音类配置与角色通用动作音效，按玩法动作分类。武器相关音效不在此处，而是随武器放在 `Weapons/AssaultRifle/Audio/` 下。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Classes/` | 2 | `SC_PVP_Master` 为全局声音类，控制音量/优先级；`SEF_PVP_Muffled` 为闷响混音效果，用于消音或遮挡场景的音频处理 |
| `Common/Foley/` | 19 | 布料摩擦与脚步声（Foley），共 7 组布料音、10 组脚步音及对应 Cue，用于角色移动与衣物动作表现 |
| `Common/Heal/` | 34 | 注射器治疗动作全套音效：肾上腺素、瓶盖咬合、弹跳、持有、注射、吐出、刺入及各类 Cue，用于治疗/使用道具的听觉反馈 |
| `Common/Melee/` | 10 | 近战打击与挥空音（Impact / Whoosh）及 Cue，用于拳击等近战动作表现 |

---

### 3.2 `Characters/` — 角色模块

- **分类**：表现层 / 角色模型
- **作用**：提供角色 Mannequin 的完整模型资产链（骨架、网格、材质、贴图、IK/控制绑定、姿势）。通用动画通知与角色蓝图等逻辑资产在 `Content/PVP/`；武器相关角色动画放在 `Weapons/` 下。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Mannequins/Materials/` | 1 | `M_PVP_Mannequin` 角色母材质 |
| `Mannequins/Materials/Instances/` | 2 | `MI_PVP_Manny_01/02` 角色材质实例，对应两套角色外观 |
| `Mannequins/Meshes/` | 4 | `SKM_PVP_Manny*` 全身骨骼网格、`SKM_PVP_FP_Manny_Simple` 第一人称手臂网格、`SKEL_PVP_Mannequin` 骨架 |
| `Mannequins/Rigs/` | 6 | 角色绑定资产：3 个 ControlRig（基础足部 IK、身体、程序化）、`IK_PVP_Mannequin` IK Rig、`PA_PVP_Mannequin` 姿势资产、`RTG_PVP_Mannequin` 重定向器 |
| `Mannequins/Rigs/Poses/Manny/` | 28 | Manny 各骨骼部位（小腿、锁骨、脚等）的动画与姿势资产，供控制 Rig 与程序化 IK 采样使用 |
| `Mannequins/Textures/Manny/` | 6 | Manny 贴图：两套外观的基础色（D）、法线（N）与金属/高光/粗糙度遮罩（MSR_MSK） |

> **逻辑资产已迁出**：`BP_PVPBaseCharacter` 迁入 `Content/PVP/Characters/`；9 个通用动画通知蓝图（`ANS_PVP_*`、`AN_PVP_*`）迁入 `Content/PVP/Animation/Notifies/`；角色动画蓝图位于 `Content/PVP/Animation/AnimBP/`。本目录只放美术表现资产，新增蓝图/数据表/输入/地图一律放 `Content/PVP/`。

---

### 3.3 `Common/` — 共享模块

- **分类**：表现层 / 跨模块公共资源
- **作用**：存放被多个模块共同引用的母材质。各模块只保留自己的材质实例，母材质统一放在此处，便于集中修改与复用。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Materials/` | 7 | 环境、玻璃、辉光、准星、天空盒、文字、武器七类母材质（`M_PVP_*_Master`），供其他模块创建材质实例时继承 |

---

### 3.4 `Environment/` — 环境模块

- **分类**：表现层 / 场景与关卡美术
- **作用**：存放可布置到关卡中的场景物件（施工灯）与地面、墙面、天空盒等环境美术资产。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Animations/` | 3 | 施工灯（大/小）的姿势动画序列与姿势资产，用于灯具的摆动或状态表现 |
| `Materials/` | 4 | 地面、道具、天空盒、墙面的材质实例（`MI_PVP_*`），继承自 `Common/Materials/` 的母材质 |
| `Meshes/` | 30 | 施工灯大/小型号的骨架、骨骼网格、物理资产（带 `ConstructionLamp` 字样），以及场景静态网格 |
| `Textures/` | 11 | HDR 环境贴图（`TC_PVP_HDR`）与地面、道具等基础色/法线/ORM 贴图 |

---

### 3.5 `Props/` — 道具模块

- **分类**：表现层 / 可交互道具
- **作用**：存放治疗注射器这一可交互道具的完整资产链，供治疗玩法与道具交互使用。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Animations/` | 2 | 注射器治疗动作的 FP/TP 动画序列，分别用于第一人称手部与第三人称全身表现 |
| `Materials/` | 1 | `MI_PVP_Syringe` 注射器材质实例 |
| `Meshes/` | 5 | 注射器骨架、骨骼网格、物理资产、静态网格及瓶盖网格，支持物理抛掷与拾取 |
| `Textures/` | 3 | 注射器基础色、法线与 ORM 贴图 |

---

### 3.6 `UI/` — 界面模块

- **分类**：表现层 / UI 资源
- **作用**：存放界面字体资源。输入映射、控件蓝图等逻辑资产已被排除，不在本目录内。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Fonts/` | 2 | `Font_PVP_Bodycam` 与 `Font_Face_PVP_Bodycam` 字体资源，用于 Bodycam 风格界面文字渲染 |

---

### 3.7 `VFX/` — 特效模块

- **分类**：表现层 / Niagara 特效
- **作用**：存放枪口焰与烟雾特效的完整资源链（贴图 → 材质 → 发射器 → 系统），以及 Bodycam 鱼眼后处理材质。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Materials/` | 8 | 枪口焰火焰/烟雾、侧向烟雾、通用烟雾的材质实例，以及 `MI_PVP_BodycamFisheye` 鱼眼后处理材质 |
| `Textures/` | 5 | 火焰、烟雾的序列帧/遮罩贴图（含侧向烟雾与速度图 MV） |
| `Emitters/` | 2 | 枪口焰火焰与烟雾的 Niagara 发射器 |
| `Systems/` | 2 | `NS_PVP_MuzzleFlash` 枪口焰与 `NS_PVP_Smoke` 烟雾特效系统，供武器开火与烟雾效果调用 |

---

### 3.8 `Weapons/` — 武器模块

- **分类**：玩法表现 / 武器（资产量最大的模块）
- **作用**：存放武器网格、材质、贴图、动画、音效的完整资源。`AssaultRifle/` 为具体武器（突击步枪），遵循同一模式新增武器时只需增加一个同级子目录，不影响现有结构。散件按槽位归入 `Attachments/<Group>/`，槽位与插槽的权威对应关系见 `Docs/Equipment/Equipment-Attachments.md`。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Common/` | 2 | 跨武器通用的激光束网格与材质实例（`SM_PVP_LaserBeam` / `MI_PVP_LaserBeam`） |

#### 3.8.1 `Weapons/AssaultRifle/` — 突击步枪

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Attachments/Handguard/` | 3 | 护木：`SM_PVP_AR_Handguard_Default` / `_Extended` / `_Short`；三者自带 `SOCKET_Grip` / `SOCKET_Laser` / `SOCKET_Muzzle` / `SOCKET_Sight_Front` |
| `Attachments/Sight/` | 4 | 机瞄：前准星立起/折叠、后照门立起/折叠（`SM_PVP_AR_ATT_Sight_*`） |
| `Attachments/Optic/` | 1 | 光学瞄具 `SM_PVP_AR_ATT_Scope`（含镜片与准线材质） |
| `Attachments/Grip/` | 2 | 前握把 `SM_PVP_AR_ATT_Grip_Angled` / `_Vertical` |
| `Attachments/Muzzle/` | 1 | 枪口 `SM_PVP_AR_ATT_Silencer` |
| `Attachments/Laser/` | 1 | 激光指示器 `SM_PVP_AR_ATT_Laser`（自带 `SOCKET_LaserStart`） |
| `Attachments/Magazine/` | 8 | 骨骼弹匣（普通 / 透明）、弹匣骨架与物理资产、静态满/空/透明变体（`SM_PVP_AR_Magazine_*`） |
| `Materials/` | 12 | 枪身与配件的材质实例（`MI_PVP_AR_*`、附件 `ATT_*`） |
| `Meshes/` | 7 | 枪身本体 `SK_PVP_AR` / `SKEL_PVP_AR` / `PHYS_PVP_AR`；非装配件保留原路径：`SM_PVP_AR_Bullet`、`SM_PVP_AR_Bullet_Casing`、`SM_PVP_AR_GunCase`、`SM_PVP_AR_GunCase_Lid`（被 `L_PVP_Showcase` 引用，移动会牵连关卡重存） |
| `Textures/` | 31 | 枪身各部件与配件的基础色（D）、法线（N）、ORM 贴图 |
| `Animations/Character/FP/` | 90 | 第一人称角色持枪动画，细分为 5 类（见下表）；含瞄准射击 Montage `AM_PVP_FP_AR_Fire_Aimed`（本轮由 `A_PVP_FP_AR_Fire_Aimed` 生成） |
| `Animations/Character/TP/` | 56 | 第三人称角色持枪动画，细分为 5 类（见下表） |
| `Animations/Weapon/Common/` | 3 | 武器共用动画：弹匣耗尽、开火模式状态、参考姿势 |
| `Animations/Weapon/FP/` | 22 | 第一人称武器本体动画蒙太奇（开火、换弹、排障、检视、装备等） |
| `Animations/Weapon/TP/` | 14 | 第三人称武器本体动画蒙太奇 |
| `Audio/Firing/` | 33 | 开火、空仓、抛弹壳等射击音效及 Cue |
| `Audio/Handling/Bolt/` | 24 | 枪机拉柄开关音效 |
| `Audio/Handling/Foley/` | 31 | 持枪动作音效：瞄准进出、握持摩擦等 |
| `Audio/Handling/Magazine/` | 18 | 弹匣插入、退出、检查等音效 |
| `Audio/Impacts/` | 8 | 枪身磕碰、弹匣落地弹跳等撞击音效 |

**角色持枪动画的五个子类**（FP/TP 结构一致）：

| 子类 | 含义与作用 |
| --- | --- |
| `Combat/` | 战斗动作：开火（单发/连发/空仓）、排障、切换开火模式等 |
| `Interactions/` | 交互动作：检视、换握持方式、抓取、治疗、拳击等 |
| `Locomotion/` | 移动动作：站立/蹲伏/瞄准待机循环、跳跃起落等 |
| `Poses/` | 姿势资产：瞄准/待机在不同握持方式下的静态姿势，供混合与 IK 采样 |
| `Transitions/` | 过渡动作：装备/收起武器、快速切换、蹲伏与瞄准进出过渡 |

---

## 4. `Content/PVP/` — 逻辑资产根

- **分类**：逻辑资产 / 蓝图、关卡与数据
- **作用**：存放无法归入美术库的逻辑资产（蓝图、蓝图接口、数据资产、关卡），与美术根 `Content/PVPDemo/` 分离。本根由 `Content/Python/setup_pvp_game_framework.py` 与 `setup_pvp_input.py` 生成与维护，命名沿用 `BP_` / `BPI_` / `DA_` / `L_` 前缀。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Animation/` | 16 | 动画逻辑唯一落点，按资产类型再分子目录：`AnimBP/`（5 个角色动画蓝图，含 `ABP_PVP_FP_Locomotion` 与武器动画蓝图 `ABP_PVP_FP_Weapon_AR`）、`AnimBP/Environment/`（1 个环境灯具动画蓝图）、`Interfaces/`（1 个动画状态蓝图接口）、`Notifies/`（9 个通用动画通知蓝图）。动画蓝图 / 通知 / 接口不再散落在 `Actors/` 或 `Characters/` |
| `Characters/` | 2 | `BP_PVPBaseCharacter`（继承 `APVPBaseCharacter` 的玩家角色蓝图，`DefaultPawnClass` 指向它）、`BP_PVP_BaseCharacter`（从第三方迁移的完整角色逻辑蓝图副本，动画蓝图与通知内部引用它，当前未接入 GameMode） |
| `Game/` | 2 | `BP_PVPGameMode`（默认 Pawn 指向 `BP_PVPBaseCharacter`、玩家控制器指向 `BP_PVPPlayerController`）、`BP_PVPPlayerController`（继承 `APVPPlayerController`） |
| `Maps/` | 1 | `L_PVP_Showcase`：由第三方演示关卡复制而来的工程自有地图；无第三方角色实例，出生点为 `PlayerStart`，其余 Actor 全部保留且类引用改指向自有副本，WorldSettings 覆盖为 `BP_PVPGameMode` |
| `Actors/` | 20 | 19 个由第三方逻辑资产复制而来的 Actor 蓝图自有副本（墙体框架 7 个、施工灯基类、信息板、武器 / 弹匣 / 附件 / 配置、物理对象与注射器），加 1 个本轮新建的 `BP_PVP_Weapon_AR`（父类 `APVPWeaponActor`，配置 AR 网格 / 武器 ABP / 拾取半径 / 成对 Montage）。只放 Actor 蓝图；动画蓝图、动画通知、控件蓝图、枚举与结构体已迁出 |
| `Input/` | 31 | 29 个 `IA_PVP_*` 输入动作、`IMC_PVP_Default` 输入映射与 `DA_PVP_InputConfig` 输入配置数据资产。`DA_PVP_InputConfig` 把 `InputTag.Move/Look/Jump/Run/Aim/Fire` 映射到 6 个 IA 并挂到 `BP_PVPBaseCharacter` CDO；IMC 由 `APVPPlayerController` 在代码中默认添加。`IA_PVP_Look` 的 `Mouse2D` 映射带 **Negate(Y)**（俯仰方向修正，勿删除，详见 `Docs/Input/Input-Dev.md` 踩坑 1） |
| `Data/` | 2 | `DT_PVP_TextStyle` 文本样式数据表（UI 蓝图依赖）、`DA_PVP_WeaponAttachmentCatalog` 配件分类目录（8 个槽位定义 + `Basic` / `Tactical` 两套预设，被 `BP_PVP_Weapon_AR` 的 `AttachmentCatalog` 引用） |
| `Types/` | 5 | 4 个枚举（`E_PVP_CameraPerspectives`、`E_PVP_FireMode`、`E_PVP_GripAttachment`、`E_PVP_Stance`）与 `S_PVP_SpringData` 弹簧参数结构体；被角色动画蓝图、角色 / 武器蓝图与控件蓝图引用 |
| `UI/` | 2 | `WBP_PVP_BodycamOverlay`、`WBP_PVP_Demo` 两个控件蓝图（演示 HUD）。原 `Actors/` 下的同名重复副本已删除，此处为唯一一份，被 `BP_PVP_BaseCharacter` 引用 |

**归位规则（唯一事实来源：`Content/Python/pvp_logic_layout.py`）**：

| 资产名前缀 / 类型 | 落点 |
| --- | --- |
| `ABP_` | `Animation/AnimBP/`；环境类动画蓝图放 `Animation/AnimBP/Environment/` |
| `BPI_` | `Animation/Interfaces/` |
| `AN_` / `ANS_` | `Animation/Notifies/` |
| `E_` / `S_` | `Types/` |
| `WBP_` | `UI/` |
| `BP_`（Actor 蓝图） | `Actors/` |
| `BP_`（角色蓝图） | `Characters/` |
| `IA_` / `IMC_` / 输入配置 `DA_` | `Input/` |
| `DT_` 等数据表 | `Data/` |
| `L_` | `Maps/` |

新增逻辑资产按上表放置；`reorg_pvp_logic_assets.py`（整理）与 `setup_pvp_game_framework.py`（框架重建）共用该路由，重跑不会把动画资产搬回 `Actors/`。

**引用方向**：逻辑根可引用美术根与 `/Script/PVPDemo.*` 原生类；美术根**不得**反向引用逻辑根（既有的 `SKM_PVP_Manny` → `ABP_PVP_Manny_PostProcess` 为骨骼网格体的 `PostProcessAnimBlueprint` 属性，属引擎属性固有形态的例外）。两个根与新地图均不得再引用 `/Game/InfimaGames`。

---

## 5. 与源码分层的关系

`Content/PVPDemo/`（美术）与 `Content/PVP/`（逻辑）的模块划分与 `Source/PVPDemo/` 分层相互对应，但不是严格一对一（第三方包按美术资源组织，源码按逻辑职责组织）：

| Content 模块 | 对应源码层 | 关系说明 |
| --- | --- | --- |
| `PVP/Game/` | `Source/PVPDemo/Game/` | 游戏模式与玩家控制器蓝图，继承 `APVPGameMode` / `APVPPlayerController` |
| `PVP/Maps/` | 关卡 / 玩法 | `L_PVP_Showcase` 的 WorldSettings 指向 `BP_PVPGameMode`，出生点由 `PlayerStart` 提供 |
| `PVP/Characters/` | `Source/PVPDemo/Character/` | `BP_PVPBaseCharacter` 继承 `APVPBaseCharacter`，是指向 `DefaultPawnClass` 的蓝图，CDO 引用 `DA_PVP_InputConfig`；`BP_PVP_BaseCharacter` 为第三方完整角色逻辑副本，动画蓝图与通知内部引用它，当前未接入 GameMode |
| `PVP/Animation/` | `Source/PVPDemo/Animation/`、`Source/PVPDemoEditor/PVPAnimBlueprintBuilder` | `AnimBP/` 承接迁移后的角色动画蓝图（其中 `ABP_PVP_FP_BaseCharacter` 也是 `APVPBaseCharacter` 的第一人称 AnimClass 默认值）、`ABP_PVP_FP_Locomotion`（父类 `UPVPAnimInstanceBase`，已接到 `BP_PVPBaseCharacter` 的 Mesh AnimClass，并原地插入 `DefaultSlot`）与武器 ABP `ABP_PVP_FP_Weapon_AR`（`A_PVP_WEP_AR_Reference` + `DefaultSlot`）；`Interfaces/` 提供动画状态契约，`Notifies/` 被武器蒙太奇与武器蓝图引用 |
| `PVP/Types/` | `Source/PVPDemo/Character/`、`Game/` | 枚举与结构体承载角色视角 / 开火模式 / 握持方式 / 站姿与弹簧参数语义，供动画蓝图、角色与武器蓝图及 UI 读取 |
| `PVP/Input/` | `Source/PVPDemo/Input/`、`GAS/PVPGameplayTags` | IA / IMC / `DA_PVP_InputConfig` 提供动作与语义映射，供角色按 Tag 绑定、控制器注入 IMC |
| `Characters/` | `Source/PVPDemo/Character/`、`Animation/` | 提供角色网格、姿势与动画素材；角色逻辑在源码层实现 |
| `Weapons/` | `Source/PVPDemo/Equipment/`、`Animation/`、`PVPDemoEditor/` | 提供武器网格与动画素材；`Attachments/<Group>/` 是配件槽位分类的物理落点，槽位 / 插槽 / 组件名契约由 `Equipment/PVPWeaponAttachmentTypes.h` 定义，目录数据资产在 `PVP/Data/`，蓝图内装配由编辑器构建器写入 |
| `Audio/` `VFX/` | `Source/PVPDemo/GAS/` | 音效与特效资源作为 GameplayCue 的表现素材 |
| `Common/` | 全局 | 母材质被各模块材质实例继承 |
| `UI/` | 后续 UI 层 | 字体资源供界面使用 |
| `Environment/` `Props/` | 关卡/玩法 | 场景物件与可交互道具的美术资源 |

> 说明：源码目录 `Source/PVPDemo/` 当前已落地 `Character/PVPBaseCharacter.h/.cpp`（含 `EquipmentComponent`）、`Equipment/PVPWeaponActor.h/.cpp`、`Equipment/PVPEquipmentComponent.h/.cpp`、`Game/PVPGameMode.h/.cpp`、`Game/PVPPlayerController.h/.cpp`、`Input/PVPInputConfig.h/.cpp`、`Input/PVPInputComponent.h/.cpp`、`GAS/PVPGameplayTags.h/.cpp`（仅输入 Tag）、`Animation/PVPAnimInstanceBase.h/.cpp`（动画实例基类）与 `PVPDemoEditor/PVPAnimBlueprintBuilder.h/.cpp`（动画蓝图构建器，含武器 ABP 与 Slot 插入）；逻辑资产 `Animation/AnimBP/ABP_PVP_FP_Locomotion`（由 `Content/Python/setup_pvp_anim_blueprint.py` 生成，已接到 `BP_PVPBaseCharacter` 的 Mesh AnimClass，并已原地插入 `DefaultSlot`；C++ 构造期默认仍是 `ABP_PVP_FP_BaseCharacter`）、`Animation/AnimBP/ABP_PVP_FP_Weapon_AR` 与 `Actors/BP_PVP_Weapon_AR`（由 `Content/Python/setup_pvp_weapon.py` 生成）；`Camera/` 仍为规划中的空目录（见 `Docs/Source-Structure.md`）；编辑器期工具在 `Source/PVPDemoEditor/`。

---

## 6. 使用约定与注意事项

1. **已自包含**：两个资产根与新地图已由 `Content/Python/setup_pvp_game_framework.py` 完成引用重定向，不存在指向第三方包的活引用，删除本地 `Content/InfimaGames/` 不影响工程打开与运行；原包仅作开发期素材参考，且**不得修改**。
2. **命名规范**：新增资产沿用 `类型前缀 + PVP + 描述` 格式（如 `T_PVP_Floor_D`），不要引入 `TFA` 前缀。
3. **母材质与实例**：修改共享外观优先改 `Common/Materials/` 的母材质；模块专属参数在该模块的材质实例上调整。
4. **FP / TP 成对**：第一人称与第三人称动画通常成对存在，新增动作时建议两套一并制作，避免表现不一致。
5. **重复执行迁移**：迁移脚本 `Content/Python/migrate_pvp_assets.py` 为幂等实现，已存在的目标资产会跳过，不会覆盖手动修改；可安全重复运行以同步原包新增的美术资产。
6. **编辑器刷新**：资产由命令行编辑器写入，若当前打开的编辑器未显示，请在 Content Browser 按 F5 刷新或重启编辑器。
7. **两根分离**：`Content/PVPDemo/` 只放美术表现资产，`Content/PVP/` 只放逻辑资产；美术资产不得反向引用逻辑资产，逻辑资产可引用美术资产与 `/Script/PVPDemo.*` 原生类。
8. **脚本登记**：`Content/Python/` 下的脚本与用途——`migrate_pvp_assets.py`（第三方美术迁移，幂等）、`dump_tfa_character.py`（参考角色组件 Dump）、`compare_tfa_character.py`（1P 对照校验，派生蓝图已迁至 `/Game/PVP/Characters`）、`setup_pvp_game_framework.py`（框架蓝图/地图生成 + 引用重定向收敛，路由复用 `pvp_logic_layout.py`）、`verify_pvp_game_framework.py`（零依赖与地图状态断言，允许一把 `PVP_WeaponPickup_AR`）、`setup_pvp_input.py`（校验 IA ValueType / IMC 按键映射与 Look Y 轴极性，生成并挂接 `DA_PVP_InputConfig`，输出输入报告）、`setup_pvp_anim_blueprint.py`（生成 / 重建 `Animation/AnimBP/ABP_PVP_FP_Locomotion`，编译并读回状态机 / 过渡 / `DefaultSlot`，输出动画蓝图报告）、`setup_pvp_weapon.py`（生成 `BP_PVP_Weapon_AR` 与 `ABP_PVP_FP_Weapon_AR`、给角色 ABP 原地加 `DefaultSlot`、在 `L_PVP_Showcase` 放置 / 更新武器拾取物，输出武器报告）、`reorg_pvp_weapon_attachments.py`（把突击步枪散件按槽位归入 `Weapons/AssaultRifle/Attachments/<Group>/`，默认 dry-run，`PVP_ATTACHMENT_REORG_APPLY=1` 执行，输出归位报告）、`setup_pvp_weapon_assembly.py`（生成 / 更新 `DA_PVP_WeaponAttachmentCatalog`，调用 `PVPWeaponAssemblyBuilder` 在 `BP_PVP_Weapon_AR` 内装配并读回，支持 `PVP_WEAPON_LOADOUT=Basic|Tactical`）、`pvp_logic_layout.py`（逻辑资产布局单一事实来源：前缀路由、移动/删除清单、期望数量）、`reorg_pvp_logic_assets.py`（逻辑资产归位执行器，默认 dry-run，`PVP_REORG_APPLY=1` 执行移动 + 备份 + 重定向清理 + 报告）、`verify_pvp_asset_layout.py`（离线校验目录布局、旧路径零残留与 C++ 默认资产路径；`Data` 期望 2、逻辑总数 81）。
9. **地图约定**：`L_PVP_Showcase` 由 `L_TFA_AR_Showcase` 复制而来，**只删除了第三方角色实例**（改为 `PlayerStart`），其余 Actor 全部保留并将类引用改为自有副本；本轮另外在 PlayerStart 前方放置 1 个带 `PVP_WeaponPickup_AR` 标签的 `BP_PVP_Weapon_AR` 拾取物（由 `setup_pvp_weapon.py` 幂等维护），因此目标地图比源地图多 1 个 Actor，`verify_pvp_game_framework.py` 按 `source + weapon_pickups` 校验。不复制 `_BuiltData`（工程 `r.AllowStaticLighting=False`），地图内 `LevelBuildData` 保持对源包的只读引用。源地图与 `_BuiltData` 的 SHA256 记录在 `Saved/PVPGameFrameworkReport/source_hashes.json`，脚本执行前后必须一致。
10. **打包注意**：`Config/DefaultEngine.ini` 的 `GameDefaultMap` 未改动，打包时若未启用"全量 Cook"需手动把 `L_PVP_Showcase` 加入 `MapsToCook`。

---

## 7. 相关文档

- `AGENTS.md` — 上下文入口与文档地图
- `Docs/AI-Workflow.md` — 协作流程、文档同步判定与输出要求
- `Docs/Source-Structure.md` — 源码目录结构与职责说明（FPS 3C 技术方案分层）
- `Docs/Input/README.md` — 输入模块导航、已实现动作 / 绑定事件 / 回调函数速查
- `Docs/Input/Input-Dev.md` — 输入模块开发流程、新增动作 SOP、踩坑清单（含 Look 俯仰极性约定）与校验流程
- `Docs/Input/Input-Integration.md` — 输入模块依赖关系、数据流与对外接口契约
- `Docs/Equipment/README.md` — 装备模块导航、武器 / 装备组件 / 成对 Montage / 配件权威表
- `Docs/Equipment/Equipment-Dev.md` — 武器资产生成与配件装配 SOP、踩坑清单与 PIE 冒烟
- `Docs/Equipment/Equipment-Integration.md` — 装备依赖方向、C++ / 资产接口契约
- `Docs/Equipment/Equipment-Attachments.md` — 配件分类、插槽映射、基础 / 战术两套装配方案与换装 SOP
- `Saved/MigrationReports/migration_manifest.json` — 资产迁移清单（源路径 → 目标路径 → 新名 → 类型）
- `Saved/MigrationReports/migration_verify.json` — 迁移校验结果
- `Saved/PVPGameFrameworkReport/report.json|md` — 框架蓝图/地图生成与重定向收敛报告
- `Saved/PVPGameFrameworkReport/verify.json|md` — 零依赖、地图接入与源关卡指纹的校验结果
- `Saved/PVPGameFrameworkReport/source_hashes.json` — 源地图与 `_BuiltData` 的基线 SHA256
- `Saved/PVPInputReport/report.json|md` — 输入资产校验结果、`DA_PVP_InputConfig` 生成与角色蓝图挂接状态
- `Saved/PVPAssetReorgReport/report.json|md` — 逻辑资产归位报告（移动表 / 删除表 / 备份 SHA256 / 重定向清理 / 校验结果）
- `Saved/PVPAssetReorgReport/verify.json|md` — 离线布局校验结果（目录数量、旧路径零残留、C++ 默认资产路径）
- `Saved/PVPWeaponReport/report.json|md` — 武器 BP / 武器 ABP / 角色 `DefaultSlot` / 关卡拾取物生成与读回报告
- `Saved/PVPWeaponReport/weapon_abp_builder.json`、`slot_builder.json` — 武器 ABP 构建与角色 Slot 插入的 C++ 构建器报告
- `Saved/PVPAttachmentReport/reorg.json|md` — 配件网格归位报告（分类表 / 移动表 / 备份 SHA256 / 校验结果）
- `Saved/PVPAttachmentReport/report.json|md`、`report_tactical.json|md` — 基础型 / 战术型装配与读回报告
- `Saved/PVPAttachmentReport/assembly_builder.json`、`readback_builder.json` — 配件装配构建器的写入与读回报告
- `Saved/AssetBackup/<timestamp>/` — 删除重复控件蓝图 / 归位配件网格前的文件级备份，可回滚
