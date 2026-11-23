# PVP 装备模块接口与依赖契约

> 维度：装备（武器 Actor / 拾取附着 / 成对 Montage）
> 模块：Equipment
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Equipment/README.md`（导航与权威表）、`Docs/Equipment/Equipment-Dev.md`（开发与踩坑）、`Docs/Equipment/Equipment-Attachments.md`（配件分类与装配）

---

## 1. 模块定位与边界

**职责**：

- 武器 Actor `APVPWeaponActor` 的组件装配、配置应用、拾取触发、附着 / 卸下 / 丢弃、成对 Montage 播放。
- 角色装备组件 `UPVPEquipmentComponent` 的"一次一把"规则与拾取入口。
- 配件槽位契约（`E_PVPWeaponAttachmentSlot` / `FPVPWeaponAttachmentSpec`）、配件目录数据资产与运行时槽位查询 / 显隐 / 换网格 / 应用预设。
- 为动画 / UI / 后续 GAS 提供只读查询：当前武器、枪口位置、是否装备、当前配件。

**非职责**：

- 输入语义与键位：`Input/` 与 `DA_PVP_InputConfig`；本轮不新增动作。
- 开火 / 换弹玩法与伤害：GAS 技能与武器玩法。
- 相机：`CameraFP` 仍由 `Character/` 持有。
- 网络复制、存档、武器切换：后续装备系统。
- 配件 UI、配件数值加成、配件拾取物：后续阶段。

---

## 2. 模块与外部依赖

### 2.1 源码依赖

| 依赖 | 用法 | 结论 |
| --- | --- | --- |
| `Engine` | `AActor`、`USkeletalMeshComponent`、`USphereComponent`、`UAnimInstance`、`UAnimMontage`、`ACharacter`、`UStaticMeshComponent`、`UDataAsset` | `PVPDemo.Build.cs` 已声明，无新增依赖 |
| `Character/` | `APVPBaseCharacter` 构造期创建 `UPVPEquipmentComponent` | **单向** `Character → Equipment` |
| `Equipment/` | `APVPWeaponActor` ↔ `UPVPEquipmentComponent` ↔ 配件类型 / 目录 | 同模块内部引用 |
| `Animation/` | `ABP_PVP_FP_Locomotion` / `ABP_PVP_FP_Weapon_AR` 的 `DefaultSlot` 接收 `Montage_Play` | 只通过 `UAnimInstance` 引擎接口，不包含动画源码类 |
| `Game/` / `GAS/` | 未依赖 | 本轮不接入 |
| `PVPDemoEditor/` | `PVPWeaponAssemblyBuilder` 调用运行时配件类型并写武器蓝图 SCS | **单向** `Editor → Runtime`，运行时不感知编辑器 |

**关键约束**：`UPVPEquipmentComponent` 只包含 `ACharacter` 与 `APVPWeaponActor`，**不包含** `APVPBaseCharacter` 具体类；`APVPWeaponActor` 同样不包含 `Character/` 头文件。因此 `Equipment/` 不会反向依赖 `Character/`、`Game/`、`GAS/`。配件槽位枚举与结构体是 `Equipment/` 的公共契约，编辑器构建器与蓝图 / 数据资产都引用同一份定义。

### 2.2 编辑器依赖

`PVPDemoEditor` 新增武器 ABP 构建、Slot 插入与配件装配构建，依赖：

- `AnimGraph`（`UAnimGraphNode_Slot` / `UAnimGraphNode_SequencePlayer` / `UAnimGraphNode_Root`）
- `AnimGraphRuntime`（`FAnimNode_Slot` 等）
- `BlueprintGraph` / `UnrealEd` / `AssetTools` / `Json`
- `PVPDemo`（共享 `E_PVPWeaponAttachmentSlot` / `FPVPWeaponAttachmentSpec`；`PublicDependencyModuleNames`）
- 配件装配只用 `Engine` 的 `USimpleConstructionScript` / `USCS_Node` + `UnrealEd` 的 `FBlueprintEditorUtils` / `FKismetEditorUtilities`，不新增模块

无新增编辑器模块。

### 2.3 第三方资产

- 武器美术资产全部来自 `Content/PVPDemo/Weapons/AssaultRifle/`，不引用 `/Game/InfimaGames`。
- 配件网格已分类到 `Content/PVPDemo/Weapons/AssaultRifle/Attachments/<Group>/`；弹药与枪箱因被关卡引用保留在 `Meshes/`。
- 迁移后的第三方逻辑蓝图 `BP_PVP_BaseWeapon` / `BP_PVP_BaseCharacter` 仅被动画通知引用；本轮不修改、不继承它们。

---

## 3. 数据流与交互

```text
Pawn 走近
  └─ APVPWeaponActor::PickupSphere OnComponentBeginOverlap
       └─ Pawn->FindComponentByClass<UPVPEquipmentComponent>()
            └─ UPVPEquipmentComponent::EquipWeapon(Weapon)
                 └─ APVPWeaponActor::EquipTo(Character)
                      ├─ AttachToComponent(CharacterMesh, "ik_hand_gun")
                      ├─ 关闭 PickupSphere
                      └─ PlayEquipMontage()
                           ├─ CharacterMesh AnimInstance->Montage_Play(CharacterMontage)
                           └─ ActiveWeaponMesh AnimInstance->Montage_Play(WeaponMontage)
```

- 装备状态由 `UPVPEquipmentComponent::CurrentWeapon` 持有；武器 Actor 只知道自己被谁装备。
- 动画层只接收 `Montage_Play` 请求，不反向修改装备状态。
- 拾取不使用 Tick / 逐帧扫描，只有 Overlap 事件。

**配件数据流**：

```text
编辑器期（一次）
  reorg_pvp_weapon_attachments.py  →  Attachments/<Group>/ 资产落位
  setup_pvp_weapon_assembly.py     →  DA_PVP_WeaponAttachmentCatalog（槽位定义 + 预设）
                                   →  PVPWeaponAssemblyBuilder：BP_PVP_Weapon_AR 组件树（ATT_<Slot> + socket + 标签）

运行时（无 Tick）
  APVPWeaponActor::OnConstruction / BeginPlay
    └─ RefreshAttachmentComponents()：扫描 PVP.Attachment.<Slot> 标签 → AttachmentComponents(Slot → 组件)
         ├─ GetAttachmentComponent / GetAttachmentMesh（只读）
         ├─ SetAttachmentEnabled / ApplyAttachmentMesh（单件）
         └─ ApplyAttachmentLoadout(预设)：读目录 → 换网格 + 显隐 + 关掉未列出的配件
```

- 配件组件挂在 `Mesh1P` 或 `ATT_Handguard` 下，随武器附着角色而移动，不单独复制。
- 槽位表只在构造 / BeginPlay 重建一次；换装只改网格与可见性，不重建组件。

---

## 4. 对外接口

### 4.1 C++：`APVPWeaponActor`

```cpp
UCLASS(Blueprintable)
class PVPDEMO_API APVPWeaponActor : public AActor
{
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Mesh1P;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Mesh3P;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> PickupSphere;

    UFUNCTION(BlueprintCallable) bool EquipTo(ACharacter* NewOwnerCharacter);
    UFUNCTION(BlueprintCallable) void Unequip(bool bDropWeapon);
    UFUNCTION(BlueprintCallable) void DropWeapon();

    UFUNCTION(BlueprintCallable) void PlayEquipMontage();
    UFUNCTION(BlueprintCallable) void PlayFireMontage();
    UFUNCTION(BlueprintCallable) void PlayReloadMontage();

    UFUNCTION(BlueprintPure) bool IsEquipped() const;
    UFUNCTION(BlueprintPure) USkeletalMeshComponent* GetActiveMesh() const;
    UFUNCTION(BlueprintPure) FVector GetMuzzleLocation() const;
    UFUNCTION(BlueprintPure) ACharacter* GetOwningCharacter() const;
    UFUNCTION(BlueprintCallable) void SetPerspective(bool bFirstPerson);

    // 配件（slot 类型见 4.5）
    UFUNCTION(BlueprintCallable) void RefreshAttachmentComponents();
    UFUNCTION(BlueprintPure) UStaticMeshComponent* GetAttachmentComponent(E_PVPWeaponAttachmentSlot Slot) const;
    UFUNCTION(BlueprintPure) UStaticMesh* GetAttachmentMesh(E_PVPWeaponAttachmentSlot Slot) const;
    UFUNCTION(BlueprintCallable) bool SetAttachmentEnabled(E_PVPWeaponAttachmentSlot Slot, bool bEnabled);
    UFUNCTION(BlueprintCallable) bool ApplyAttachmentMesh(E_PVPWeaponAttachmentSlot Slot, UStaticMesh* NewMesh);
    UFUNCTION(BlueprintCallable) int32 ApplyAttachmentLoadout(FName LoadoutName);
};
```

配置属性（`EditDefaultsOnly`，BP CDO 可覆盖）：`Mesh1PAsset` / `Mesh1PAnimClass` / `Mesh3PAsset` / `Mesh3PAnimClass` / `AttachSocketName` / `AttachOffset` / `MuzzleSocketName` / `AttachmentCatalog` / `PickupRadius` / `bAutoPickup` / `PickupCooldown` / `DropForwardOffset` / `DropHeightOffset` / `bPlayEquipMontageOnPickup` / `EquipMontage` / `FireMontage` / `ReloadMontage`。

`FPVPWeaponMontagePair`：`CharacterMontage` / `WeaponMontage` / `PlayRate`（均为 `EditAnywhere`，便于 BP CDO 与 Python 读回）。

### 4.2 C++：`UPVPEquipmentComponent`

```cpp
UCLASS(ClassGroup = (PVP), meta = (BlueprintSpawnableComponent))
class PVPDEMO_API UPVPEquipmentComponent : public UActorComponent
{
public:
    UPROPERTY(BlueprintReadOnly) TObjectPtr<APVPWeaponActor> CurrentWeapon;

    UFUNCTION(BlueprintCallable) bool EquipWeapon(APVPWeaponActor* Weapon);
    UFUNCTION(BlueprintCallable) void UnequipCurrentWeapon(bool bDropWeapon);
    UFUNCTION(BlueprintCallable) void DropCurrentWeapon();
    UFUNCTION(BlueprintPure) APVPWeaponActor* GetCurrentWeapon() const;
    UFUNCTION(BlueprintPure) bool HasWeapon() const;
};
```

### 4.3 C++：`APVPBaseCharacter`

- 新增 `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PVP|Equipment") TObjectPtr<UPVPEquipmentComponent> EquipmentComponent;`
- 构造期 `CreateDefaultSubobject<UPVPEquipmentComponent>(TEXT("EquipmentComponent"))`。
- 不新增输入动作、不改 Mesh / CameraFP / 走跑速度。

### 4.4 资产契约

| 资产 / 属性 | 约定 |
| --- | --- |
| `BP_PVP_Weapon_AR` | 父类 `APVPWeaponActor`；`Mesh1PAsset` / `Mesh3PAsset` = `SK_PVP_AR`；AnimClass = `ABP_PVP_FP_Weapon_AR_C`；`AttachSocketName=ik_hand_gun`；`MuzzleSocketName=SOCKET_Muzzle` |
| `ABP_PVP_FP_Weapon_AR` | 父类 `UAnimInstance`；骨骼 `SKEL_PVP_AR`；`A_PVP_WEP_AR_Reference` → `DefaultSlot` → Root |
| `ABP_PVP_FP_Locomotion` | StateMachine → `DefaultSlot` → Root；四状态 / 6 过渡保持不变 |
| 角色 Mesh | `SKM_PVP_FP_Manny_Simple`，骨架 `SKEL_PVP_Mannequin`，可用挂点/骨骼 `ik_hand_gun`（`weapon_r` 不存在） |
| 武器 Mesh | `SK_PVP_AR`，骨架 `SKEL_PVP_AR`，Socket `SOCKET_Muzzle` / `SOCKET_Sight_Rear` |
| 地图拾取物 | `L_PVP_Showcase` 中标签 / Label 为 `PVP_WeaponPickup_AR`，数量 1 |
| 配件组件 | `BP_PVP_Weapon_AR` 的 SCS 组件：`ATT_<Slot>`，父级 `Mesh1P` 或 `ATT_Handguard`，`AttachToName` 写 socket，标签 `PVP.Attachment.<Slot>`，相对变换 Identity |
| 配件目录 | `/Game/PVP/Data/DA_PVP_WeaponAttachmentCatalog`：8 个槽位定义 + `Basic` / `Tactical` 预设，`BP_PVP_Weapon_AR.AttachmentCatalog` 引用 |
| 配件网格 | `/Game/PVPDemo/Weapons/AssaultRifle/Attachments/<Group>/`；`Meshes/` 只留枪身 3 件与弹药 / 枪箱 4 件 |

### 4.5 C++：配件类型契约

```cpp
UENUM(BlueprintType)
enum class E_PVPWeaponAttachmentSlot : uint8
{
    None, Handguard, FrontSight, RearSight, Optic, ForeGrip, Muzzle, Laser, Magazine
};

USTRUCT(BlueprintType)
struct FPVPWeaponAttachmentSpec
{
    E_PVPWeaponAttachmentSlot Slot;        // 自身槽位
    E_PVPWeaponAttachmentSlot ParentSlot;  // None=挂 Mesh1P；Handguard=挂护手组件
    FName SocketName;                      // 父级网格上的 socket
    TObjectPtr<UStaticMesh> Mesh;
    FName ComponentName;                   // ATT_<Slot>
    bool bEnabled;
};

UCLASS(BlueprintType)
class UPVPWeaponAttachmentCatalog : public UDataAsset
{
    TArray<FPVPWeaponAttachmentSlotDefinition> Slots;   // 槽位 + 候选网格
    TArray<FPVPWeaponAttachmentLoadout> Loadouts;       // Basic / Tactical
    bool FindSlotDefinition(E_PVPWeaponAttachmentSlot, FPVPWeaponAttachmentSlotDefinition&) const;
    bool FindLoadout(FName, FPVPWeaponAttachmentLoadout&) const;
    TArray<FPVPWeaponAttachmentSpec> BuildLoadoutAssembly(FName LoadoutName) const;
    TArray<FPVPWeaponAttachmentSpec> BuildDefaultAssembly() const;
};
```

- 命名 / 标签 / socket / 父级约定由 `PVPWeaponAttachment::{MakeComponentName, MakeComponentTag, DefaultSocketName, DefaultParentSlot}` 统一提供，不要在别处硬编码字符串。
- 编辑器构建器入口：`UPVPWeaponAssemblyBuilder::AssembleAttachments(BlueprintPath, Specs, bRemoveUnlisted, ReportPath)` / `ReadbackAttachments(BlueprintPath, ReportPath)`，只由 `Content/Python/setup_pvp_weapon_assembly.py` 调用。
- **禁止**在 `Content/PVP/Types/` 里再建一份镜像槽位枚举：蓝图侧直接使用 C++ `UENUM`，可编辑内容放在目录数据资产。

---

## 5. 其他模块调用方式

| 调用方 | 场景 | 方式 |
| --- | --- | --- |
| 角色 / 拾取 | 走近武器 | 由 `PickupSphere` Overlap 自动调用 `UPVPEquipmentComponent::EquipWeapon`，无需输入 |
| 蓝图 / 调试 | 测试开火 / 换弹 | 取 `BP_PVP_Weapon_AR` 实例或 `EquipmentComponent->GetCurrentWeapon()`，调用 `PlayFireMontage` / `PlayReloadMontage` |
| UI | 显示当前武器 | 读取 `UPVPEquipmentComponent::CurrentWeapon` / `HasWeapon` |
| 后续 GAS | 瞄准 / 后坐 / 弹道 | 通过 `GetMuzzleLocation()` 与 `GetActiveMesh()` 查询，不直接写装备状态 |
| 动画 | 播放 Montage | 只通过 `UAnimInstance::Montage_Play` 接收；`DefaultSlot` 必须存在 |
| UI / 调试 | 查询当前配件 | `GetAttachmentComponent(Slot)` / `GetAttachmentMesh(Slot)`，只读 |
| 调试 / 后续配件玩法 | 换装 | `ApplyAttachmentMesh` / `SetAttachmentEnabled` / `ApplyAttachmentLoadout`；缺失槽位会记 Warning 并跳过 |

**前置条件**：

- 武器 BP 必须配置 `AttachSocketName` 且角色骨架存在该 Socket。
- `ABP_PVP_FP_Locomotion` 与 `ABP_PVP_FP_Weapon_AR` 必须有 `DefaultSlot`，否则 Montage 不显示。
- 角色 Pawn 上必须有 `UPVPEquipmentComponent`（C++ 构造期已保证）。

---

## 6. 禁止的依赖方向

- **禁止** `Equipment/` 包含 `Character/PVPBaseCharacter.h` 或 `Game/`、`GAS/` 头文件；需要角色信息时只用 `ACharacter` / Engine 类型。
- **禁止** 动画层（`Animation/`）反向修改 `CurrentWeapon`、附着状态或武器 Actor。
- **禁止** 在 `Character/` 中实现武器拾取 / Montage 配对逻辑；角色只持有组件。
- **禁止** 通过 Python 修改 `BP_PVPBaseCharacter` 的继承 Mesh 组件 AnimClass（会破坏蓝图覆盖；用 `EnsureMontageSlot` 原地处理）。
- **禁止** 让武器资产引用 `/Game/InfimaGames`；美术资产只从 `Content/PVPDemo` 取。
- **禁止** 在 `Content/PVP/Types/` 新建槽位镜像枚举 / 结构体：槽位契约只有 `PVPWeaponAttachmentTypes.h` 一份。
- **禁止** 由 Python / 蓝图直接写 `USceneComponent::AttachSocketName` 或 `USCS_Node::AttachToName`：装配写回只能走 `PVPWeaponAssemblyBuilder`。

---

## 7. 变更影响面

| 变更 | 影响 |
| --- | --- |
| 修改 `APVPWeaponActor` 组件 / 默认值 | `BP_PVP_Weapon_AR`、`Saved/PVPWeaponReport` 读回项、`Docs/Equipment/README.md` 第 3.1 节 |
| 修改 `UPVPEquipmentComponent` 接口 | 角色蓝图、UI / GAS 调用方、`Docs/Equipment/README.md` 第 3.2 节、本文第 4.2 / 5 节 |
| 修改 Montage 对 / Slot 名 | `ABP_PVP_FP_Locomotion`、`ABP_PVP_FP_Weapon_AR`、`setup_pvp_weapon.py` 读回项、`Docs/Animation/` |
| 修改角色骨架挂点 | `AttachSocketName`、PIE 校准、`Docs/Character/` |
| 新增武器资产 | `pvp_logic_layout.py` 清单 / 数量、`Docs/Content-Structure.md`、`setup_pvp_weapon.py` |
| 新增武器拾取物 | `verify_pvp_game_framework.py` 的 Actor 数量断言、地图保存、`Docs/Content-Structure.md` |
| 修改编辑器构建器 | `PVPDemoEditor` 编译、`setup_pvp_weapon.py`、`Saved/PVPWeaponReport/*_builder.json`、`Docs/Animation/Animation-Dev.md` |
| 修改配件槽位 / 插槽 / 组件名 | `PVPWeaponAttachmentTypes.h`、`DA_PVP_WeaponAttachmentCatalog` 重建、`reorg_pvp_weapon_attachments.py` 分组、`setup_pvp_weapon_assembly.py` 的 `SLOTS` / `LOADOUTS`、`Docs/Equipment/Equipment-Attachments.md` 第 2 节 |
| 修改配件装配 / 换装接口 | `APVPWeaponActor`、`PVPWeaponAssemblyBuilder`、`Saved/PVPAttachmentReport/*`、本文第 4.1 / 4.5 / 5 节 |
| 配件资产移动 / 改名 | `reorg_pvp_weapon_attachments.py`、`pvp_logic_layout.py`（仅逻辑资产相关）、`Docs/Content-Structure.md`、`Saved/PVPAttachmentReport/reorg.md` |

---

## 8. 相关文件

- `Source/PVPDemo/Equipment/PVPWeaponActor.h/.cpp`
- `Source/PVPDemo/Equipment/PVPEquipmentComponent.h/.cpp`
- `Source/PVPDemo/Equipment/PVPWeaponAttachmentTypes.h`
- `Source/PVPDemo/Equipment/PVPWeaponAttachmentCatalog.h/.cpp`
- `Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp`
- `Source/PVPDemoEditor/PVPAnimBlueprintBuilder.h/.cpp`
- `Source/PVPDemoEditor/PVPWeaponAssemblyBuilder.h/.cpp`
- `Content/Python/setup_pvp_weapon.py`
- `Content/Python/reorg_pvp_weapon_attachments.py`
- `Content/Python/setup_pvp_weapon_assembly.py`
- `Content/Python/pvp_logic_layout.py`
- `Content/Python/verify_pvp_asset_layout.py`
- `Content/Python/verify_pvp_game_framework.py`
- `Docs/Equipment/README.md`
- `Docs/Equipment/Equipment-Dev.md`
- `Docs/Equipment/Equipment-Attachments.md`
- `Saved/PVPWeaponReport/report.md`
- `Saved/PVPAttachmentReport/report.md`
