# PVPDemo Content 资产目录结构与职责说明

本文档描述 `Content/PVPDemo/` 下各**功能模块文件夹**及其子文件夹中资产的含义与作用。该目录是工程自有的美术资产库，按 FPS 玩法功能模块组织，与 `Source/PVPDemo/` 的源码分层相互呼应。

> **来源说明**：本目录资产从第三方包 `Content/InfimaGames/TacticalFPSAnimations` 复制而来，原始包完整保留且未被修改。迁移时仅纳入美术表现类资产（贴图、网格体、材质、动画、特效、音频、字体等），排除了蓝图、输入、枚举、数据表、地图等逻辑资产。

> **命名说明**：资产统一改为 `PVP` 前缀并遵循 Unreal 官方类型前缀规范，例如 `T_TFA_Weapon → T_PVP_Weapon`。原包中的 `TFA` 前缀已被全部替换。

> **当前状态**：目录内共 **576 个资产**，均已落盘并完成引用注册校验；副本内部引用仍指向原包，因此使用这些资产时需保留原第三方包。

---

## 1. 目录总览

```
Content/PVPDemo/
├── Audio/             65   音频：声音类、通用动作音效
│   ├── Classes/        2   声音类与混音效果预设
│   └── Common/        63   通用音效（Foley / Heal / Melee）
├── Characters/        56   角色：Mannequin 骨架/网格/材质/姿势 + 通用动画通知
│   ├── Animations/     9   动画通知与动画通知状态
│   └── Mannequins/    47   角色模型全套资产
├── Common/             7   跨模块共享的母材质
│   └── Materials/      7
├── Environment/       48   场景环境：施工灯、地面/墙面/天空盒
├── Props/             11   可交互道具：注射器
├── UI/                 2   界面字体
│   └── Fonts/          2
├── VFX/               17   特效：枪口焰、烟雾
└── Weapons/          370   武器：突击步枪全套 + 通用激光
    ├── Common/         2   通用武器配件
    └── AssaultRifle/ 368   突击步枪全套资产
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

- **分类**：表现层 / 角色模型与通用动画逻辑
- **作用**：提供角色 Mannequin 的完整模型资产链（骨架、网格、材质、贴图、IK/控制绑定、姿势），以及不依赖具体武器的通用动画通知逻辑。武器相关角色动画放在 `Weapons/` 下。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Animations/` | 9 | 通用动画通知：`ANS_PVP_*` 用于 ADS 遮挡、隐藏主弹匣、左手握持、显示备用弹匣等状态切换；`AN_PVP_*` 用于抛弹匣、抛弹壳、生成附着物体、抛出物理物体、解锁动作等时间点触发 |
| `Mannequins/Materials/` | 1 | `M_PVP_Mannequin` 角色母材质 |
| `Mannequins/Materials/Instances/` | 2 | `MI_PVP_Manny_01/02` 角色材质实例，对应两套角色外观 |
| `Mannequins/Meshes/` | 4 | `SKM_PVP_Manny*` 全身骨骼网格、`SKM_PVP_FP_Manny_Simple` 第一人称手臂网格、`SKEL_PVP_Mannequin` 骨架 |
| `Mannequins/Rigs/` | 6 | 角色绑定资产：3 个 ControlRig（基础足部 IK、身体、程序化）、`IK_PVP_Mannequin` IK Rig、`PA_PVP_Mannequin` 姿势资产、`RTG_PVP_Mannequin` 重定向器 |
| `Mannequins/Rigs/Poses/Manny/` | 28 | Manny 各骨骼部位（小腿、锁骨、脚等）的动画与姿势资产，供控制 Rig 与程序化 IK 采样使用 |
| `Mannequins/Textures/Manny/` | 6 | Manny 贴图：两套外观的基础色（D）、法线（N）与金属/高光/粗糙度遮罩（MSR_MSK） |

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
- **作用**：存放武器网格、材质、贴图、动画、音效的完整资源。`AssaultRifle/` 为具体武器（突击步枪），遵循同一模式新增武器时只需增加一个同级子目录，不影响现有结构。

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Common/` | 2 | 跨武器通用的激光束网格与材质实例（`SM_PVP_LaserBeam` / `MI_PVP_LaserBeam`） |

#### 3.8.1 `Weapons/AssaultRifle/` — 突击步枪

| 子文件夹 | 数量 | 资产含义与作用 |
| --- | --- | --- |
| `Materials/` | 12 | 枪身与配件的材质实例（`MI_PVP_AR_*`、附件 `ATT_*`） |
| `Meshes/` | 27 | 步枪骨架/骨骼网格、弹匣网格（满/空/透明）、枪箱、枪口/照门/握把/消音器/瞄具等配件网格、子弹与弹壳 |
| `Textures/` | 31 | 枪身各部件与配件的基础色（D）、法线（N）、ORM 贴图 |
| `Animations/Character/FP/` | 89 | 第一人称角色持枪动画，细分为 5 类（见下表） |
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

## 4. 与源码分层的关系

`Content/PVPDemo/` 的模块划分与 `Source/PVPDemo/` 分层相互对应，但不是严格一对一（第三方包按美术资源组织，源码按逻辑职责组织）：

| Content 模块 | 对应源码层 | 关系说明 |
| --- | --- | --- |
| `Characters/` | `Source/PVPDemo/Character/`、`Animation/` | 提供角色网格、姿势与动画通知；角色逻辑在源码层实现 |
| `Weapons/` | `Source/PVPDemo/Equipment/`、`Animation/` | 提供武器网格与动画素材，供武器 DataAsset 与动画层引用 |
| `Audio/` `VFX/` | `Source/PVPDemo/GAS/` | 音效与特效资源作为 GameplayCue 的表现素材 |
| `Common/` | 全局 | 母材质被各模块材质实例继承 |
| `UI/` | 后续 UI 层 | 字体资源供界面使用 |
| `Environment/` `Props/` | 关卡/玩法 | 场景物件与可交互道具的美术资源 |

> 说明：源码目录 `Source/PVPDemo/` 当前为规划状态（仅空目录），上表描述的是资源与设计的预期对应关系。

---

## 5. 使用约定与注意事项

1. **保留原包**：副本内部引用仍指向 `/Game/InfimaGames/TacticalFPSAnimations/`，删除或移动原包会导致引用丢失。如需完全独立，应执行引用重定向后再移除原包。
2. **命名规范**：新增资产沿用 `类型前缀 + PVP + 描述` 格式（如 `T_PVP_Floor_D`），不要引入 `TFA` 前缀。
3. **母材质与实例**：修改共享外观优先改 `Common/Materials/` 的母材质；模块专属参数在该模块的材质实例上调整。
4. **FP / TP 成对**：第一人称与第三人称动画通常成对存在，新增动作时建议两套一并制作，避免表现不一致。
5. **重复执行迁移**：迁移脚本 `Content/Python/migrate_pvp_assets.py` 为幂等实现，已存在的目标资产会跳过，不会覆盖手动修改；可安全重复运行以同步原包新增的美术资产。
6. **编辑器刷新**：资产由命令行编辑器写入，若当前打开的编辑器未显示，请在 Content Browser 按 F5 刷新或重启编辑器。

---

## 6. 相关文档

- `Docs/Source-Structure.md` — 源码目录结构与职责说明（FPS 3C 技术方案分层）
- `Saved/MigrationReports/migration_manifest.json` — 资产迁移清单（源路径 → 目标路径 → 新名 → 类型）
- `Saved/MigrationReports/migration_verify.json` — 迁移校验结果
