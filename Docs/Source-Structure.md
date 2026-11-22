# PVPDemo 源码目录结构与职责说明

本文档描述 `Source/PVPDemo/` 下各文件夹的**分类**与**预计在项目中的作用**，对应 FPS 3C（Character / Camera / Control）技术方案的分层架构。

> 说明：文中的"FPS 3C"是技术方案名称（第一人称射击品类），**工程代码统一使用 `PVP` 作为类与文件前缀**（如 `APVPCharacter`），导出宏为 `PVPDEMO_API`。

> 当前状态：以下目录均已创建，但**仅为空目录**，尚未写入任何 `.h` / `.cpp` 源文件。本文描述的是规划中的职责，用于指导后续代码落地。

> **阶段说明**：项目处于初期，`Camera/` 与 `Camera/Modes/` 下规划的**相机模式栈仅为架构预留，当前阶段不实现、不接入**。初期先用简单相机方案（相机跟随角色眼高）跑通角色与输入，待基础稳定后再引入模式栈。其他规划文件也均为后续阶段的落点。

---

## 1. 目录总览

```
Source/PVPDemo/
├── PVPDemo.Build.cs                 模块依赖声明（已有）
├── PVPDemo.cpp / .h                 模块入口（已有）
├── Character/                       角色层：角色本体、移动、视角
├── Camera/                          相机层：相机组件（模式栈为预留，初期不实现）
│   └── Modes/                       相机模式子类（仅预留目录）
├── Animation/                       动画层：AnimInstance、动画层、程序化动画
├── Equipment/                       装备层：武器定义、武器 Actor、装备组件
├── Input/                           输入层：Tag 与 InputAction 的映射配置
└── GAS/                             技能层：GAS 基础、Montage 任务、GameplayTag
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
| `PVPCharacter.h/.cpp` | 角色本体：装配 1P 手臂 + 3P 全身双网格组件、输入绑定、视角目标（`BecomeViewTarget`）回调 |
| `PVPCharacterMovementComponent.h/.cpp` | 扩展 `UCharacterMovementComponent`：冲刺、探头的网络预测，移动状态只依赖 SavedMove 压缩标志 |
| `PVPPerspectiveComponent.h/.cpp` | 统一管理 1P/3P 可见性、阴影（隐藏身体保留投影）、第一人称渲染类型、不可见时 Tick 优化；预留可见下半身开关 |
| `PVPCameraTargetInterface.h` | 相机读取角色数据的纯接口，使相机不依赖具体角色类（载具、炮台、无人机亦可作为相机目标） |

**协作关系**：接收 `Input/` 的输入意图；将状态提供给 `Animation/` 与 `Camera/`；ASC 挂在 PlayerState 上由角色初始化。

---

### 2.2 `Camera/` — 相机层（表现）

- **分类**：表现层 / 相机系统
- **预计作用（规划）**：以"相机模式栈"管理所有视角（第一人称、ADS、第三人称、死亡、观战、技能镜头），按权重平滑混合，并处理镜头碰撞与防穿帮。
- **当前阶段**：项目处于初期，**模式栈仅为预留**，暂不实现。初期相机直接跟随角色眼高，仅实现基础的视角旋转、眼高与碰撞处理；`PVPCameraMode` 及 `Modes/` 下的具体模式待后续阶段引入。

| 规划文件 | 预计职责 | 阶段 |
| --- | --- | --- |
| `PVPCameraComponent.h/.cpp` | 相机组件：初期提供基础视角跟随；规划中持有模式栈并决定当前相机模式 | 初期实现基础版 |
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
- **预计作用**：双 AnimInstance 驱动 1P 手臂与 3P 全身，武器动画层按装备切换，程序化层在 C++ 中完成手臂手感处理。

| 规划文件 | 预计职责 |
| --- | --- |
| `PVPAnimInstanceBase.h/.cpp` | 基类：线程安全数据采集（`NativeThreadSafeUpdateAnimation`）、GameplayTag → 属性映射 |
| `PVPBodyAnimInstance.h/.cpp` | 3P 全身：AimOffset、原地转身、探头脊柱 Roll |
| `PVPArmsAnimInstance.h/.cpp` | 1P 手臂状态与武器层链接 |
| `PVPWeaponAnimLayerBase.h/.cpp` | 武器动画层父类：从武器 DataAsset 读取序列，新增武器无需改主 ABP |
| `PVPProceduralAnimComponent.h/.cpp` | 程序化第一人称：Sway / Bob / Recoil / ADS 对齐 / 贴墙收枪 |
| `PVPSpring.h` | 弹簧阻尼工具类，供程序化动画使用 |

**协作关系**：只读 `Character/` 与 `GAS/` 的状态；装备切换由 `Equipment/` 驱动动画层；相机位姿来自 `Camera/`。

---

### 2.4 `Equipment/` — 装备层（玩法数据）

- **分类**：玩法数据 / 装备与武器
- **预计作用**：以数据资产定义武器，同一把武器同时持有 1P/3P 网格，装备复制与动画层切换集中管理。

| 规划文件 | 预计职责 |
| --- | --- |
| `PVPWeaponDefinition.h` | DataAsset：网格、动画层、Montage、手感参数 |
| `PVPWeaponActor.h/.cpp` | 同时持有 `Mesh1P` 与 `Mesh3P` 的武器 Actor |
| `PVPEquipmentComponent.h/.cpp` | 装备复制（当前武器定义 + 实例）、切换动画层、挂接与卸载 |

**协作关系**：向 `Animation/` 提供当前武器数据与动画层；为 `GAS/` 技能提供枪口位置、Montage 与瞄准 Socket；武器 Actor 推荐不复制，各端本地生成。

---

### 2.5 `Input/` — 输入层（配置）

- **分类**：配置 / 输入映射
- **预计作用**：把 InputAction 与 GameplayTag 绑定，使角色原生输入与 GAS 技能输入共用一套配置。

| 规划文件 | 预计职责 |
| --- | --- |
| `PVPInputConfig.h/.cpp` | Tag ↔ InputAction 的数据资产映射（Lyra 风格），供角色与技能查询 |

**协作关系**：工程已在 `Config/DefaultInput.ini` 启用 Enhanced Input；`Content/InfimaGames/TacticalFPSAnimations` 中已有 `IMC_TFA_Default` 与 29 个 `IA_TFA_*`，后续可复用或按本层规范重建。

---

### 2.6 `GAS/` — 技能层（GAS 基础）

- **分类**：技能系统 / 网络权威逻辑
- **预计作用**：从零搭建 GAS 基础设施，修正多网格下的关键坑点，为开火、瞄准、换弹、冲刺等技能提供支撑。

| 规划文件 | 预计职责 |
| --- | --- |
| `PVPAbilitySystemGlobals.h/.cpp` | 自定义 Globals：修正 `AllocAbilityActorInfo` 使用的 SkeletalMeshComponent，避免 Montage 播到 1P 手臂 |
| `PVPAbilityTask_PlayDualMontage.h/.cpp` | 双网格 Montage 播放任务：本地 1P 表现 + 服务器 3P 复制 |
| `PVPGameplayTags.h/.cpp` | GameplayTag 声明（如 `State.ADS`、`State.Sprinting`、`GameplayCue.Weapon.Fire`） |

**协作关系**：ASC 挂在 PlayerState 上；驱动 `Animation/` 的 Tag 属性映射、规划中的 `Camera/` 技能镜头与后坐、`Equipment/` 的武器表现。**当前工程尚无 GAS 基础，此目录是后续搭建的起点。**

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
- **依赖方向**：`Input → Character/GAS → Animation/Camera`，表现层不得反向修改逻辑状态；`Camera/` 通过接口解耦角色，`GAS/` 不直接依赖具体相机类。
- **头文件**：优先前置声明，具体类型包含放在 `.cpp`；仅在需要反射处使用 `UCLASS/USTRUCT/UENUM`。

---

## 5. 与 Content 目录的关系

- `Content/PVPDemo/`：工程自有资产目录（蓝图、DataAsset、ABP、相机模式子类、GA 等），与源码分层一一对应。
- `Content/InfimaGames/TacticalFPSAnimations/`：已导入的第三方 FPS 动画包，提供骨骼、武器动画、IMC/IA 等资产；接入前需核查其骨骼挂点（`weapon_r` / `ik_hand_gun`）与武器 Socket（`Muzzle` / `Sight` / `LHIK`）是否满足方案要求。

---

## 6. 后续演进（当前不在范围内）

- 相机：当前先用简单相机；**相机模式栈属于中后期内容**，待角色、输入、动画稳定后再引入；后续可再迁移到 `Gameplay Cameras`（Camera Rig）。
- 动画：`Pose Search`（Motion Matching）+ `Chooser` 替换 Locomotion 状态机；`Control Rig` 用于程序化 IK。
- 移动：`Mover 2.0` 独立分支评估，主线仍用 CMC 子类。
- 网络：`Iris` 复制系统、回放与击杀回放。
