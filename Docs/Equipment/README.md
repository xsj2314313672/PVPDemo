# PVP 装备模块导航

> 维度：装备（武器 Actor / 拾取附着 / 成对 Montage）
> 模块：Equipment
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Equipment/Equipment-Dev.md`（开发流程与踩坑）、`Docs/Equipment/Equipment-Integration.md`（依赖与接口契约）

本文件是装备模块的固定导航入口。进入本模块的任务先读本文件，再按第 2 节路由按需读取；本文第 3 节是**武器 Actor 默认值 / 拾取附着行为 / 成对 Montage 名称的唯一权威表**，其他文档不复制。

---

## 1. 模块定位与边界

**职责**：把"一把可拾取的突击步枪"从世界 Actor 变成角色手上的第一人称武器，并让角色手臂与武器本体播放同步 Montage。

- 武器 Actor：`APVPWeaponActor`（双网格、拾取球体、附着 / 卸下 / 丢弃、枪口查询、成对 Montage）。
- 角色装备组件：`UPVPEquipmentComponent`（当前武器、"一次一把"规则、拾取入口）。
- 角色挂载：`APVPBaseCharacter` 构造期创建 `EquipmentComponent`，不改动现有 Mesh / CameraFP / 输入 / 走跑逻辑。
- 动画接入：武器动画蓝图 `ABP_PVP_FP_Weapon_AR`（参考姿势 + `DefaultSlot`），角色动画蓝图 `ABP_PVP_FP_Locomotion` 原地增加 `DefaultSlot`。
- 关卡接入：`L_PVP_Showcase` 的 PlayerStart 前方放置一把带 `PVP_WeaponPickup_AR` 标签的武器实例。

**非职责**（不属于本模块，不要在这里实现）：

- 开火伤害、弹药、射速、换弹状态机、后坐力：后续 GAS / 武器玩法阶段；本轮只播放 Montage 本体。ADS 姿态 / FOV 由 `Character/` + `Animation/` 接入，武器侧只提供瞄准射击 Montage。
- 输入动作：本模块不新增 `IA_PVP_*` / `InputTag.*`；Aim / Fire 由 `Character/` 绑定，拾取由 Overlap 自动触发。
- 第三人称角色：`Mesh3P` 已建好并隐藏，3P 可见性切换与 3P 角色网格属于后续阶段。
- 网络复制：武器 Actor 本地生成，`bReplicates=false`；装备状态不复制。
- 相机：1P 视角仍由 `CameraFP` 承担，见 `Docs/Character/README.md`。

---

## 2. 文档清单与路由

| 文档 | 内容 |
| --- | --- |
| `Docs/Equipment/README.md` | 本导航：定位、路由、能力速查、启动入口、校验入口、维护规则 |
| `Docs/Equipment/Equipment-Dev.md` | 实现细节：源码结构、设计决策、开发 SOP、踩坑清单、冒烟、已知限制 |
| `Docs/Equipment/Equipment-Integration.md` | 契约：模块依赖、数据流、C++ / 资产接口、禁止依赖方向、变更影响面 |
| `Docs/Equipment/Equipment-Attachments.md` | 配件权威表：槽位 / 插槽 / 组件名 / 候选网格、基础与战术两套方案、装配过程记录与换装 SOP |

**按场景路由**：

| 我要做的事 | 读哪里 |
| --- | --- |
| 查武器组件、默认值、挂点、Montage 名称 | 本文第 3 节 |
| 查拾取 / 附着 / 丢弃的运行时顺序与日志 | 本文第 4 节 |
| 修改武器网格、动画蓝图、Montage、拾取半径 | 本文第 3.1 节 + `Equipment-Dev.md` 第 4 节 SOP |
| 生成 / 重建武器资产与关卡拾取物 | 本文第 6 节 → `Equipment-Dev.md` 第 4.3 节 |
| 排查拾取不到、挂点错位、Montage 不播 | `Equipment-Dev.md` 第 5 节踩坑清单 |
| 查配件槽位、插槽、候选网格、默认装配 | 本文第 3.4 节 → `Equipment-Attachments.md` 第 2 / 3 节 |
| 装配 / 换装配件、查看装配过程记录 | `Equipment-Attachments.md` 第 4 / 5 节 |
| 查 C++ 接口签名、组件契约、依赖方向 | `Equipment-Integration.md` 第 4 / 6 节 |
| 让动画 / UI / GAS 读取当前武器 | `Equipment-Integration.md` 第 5 节 |
| 改接口前评估影响面 | `Equipment-Integration.md` 第 7 节 |

---

## 3. 已实现能力速查（权威表）

### 3.1 武器 Actor（`APVPWeaponActor`）

| 项 | 当前值 / 行为 | 说明 |
| --- | --- | --- |
| 组件树 | `SceneRoot` → `Mesh1P` / `Mesh3P` / `PickupSphere` | 全部在 C++ 构造期创建 |
| `Mesh1PAsset` / `Mesh3PAsset` | `/Game/PVPDemo/Weapons/AssaultRifle/Meshes/SK_PVP_AR` | BP CDO 配置，`OnConstruction` 应用到组件 |
| `Mesh1PAnimClass` / `Mesh3PAnimClass` | `/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Weapon_AR_C` | 武器参考姿势 + `DefaultSlot` |
| `AttachSocketName` | `ik_hand_gun` | 角色 1P 网格可用的枪械 IK 目标；`weapon_r` 在 `SKM_PVP_FP_Manny_Simple` 上不存在（见 `Equipment-Dev.md` 踩坑 6） |
| `AttachOffset` | `FTransform::Identity` | 需要微调时在 BP 或实例上覆盖 |
| `MuzzleSocketName` | `SOCKET_Muzzle` | 武器骨架 `SKEL_PVP_AR` 上的枪口挂点 |
| `PickupRadius` | `120.0` | `PickupSphere` 只对 Pawn 产生 Overlap |
| `bAutoPickup` | `true` | 走近自动拾取；关闭后需要外部调用 `EquipTo` |
| `PickupCooldown` | `0.75` | 丢弃后再次可拾取的延迟 |
| `DropForwardOffset` / `DropHeightOffset` | `150.0` / `-40.0` | 丢弃位置相对角色；150cm 大于拾取半径 + 胶囊半径，避免冷却结束瞬间原角色再次拾取 |
| `bPlayEquipMontageOnPickup` | `true` | 拾取时自动播放 Equip 成对 Montage |
| `AttachmentCatalog` | `/Game/PVP/Data/DA_PVP_WeaponAttachmentCatalog` | 配件目录，供 `ApplyAttachmentLoadout` 使用；配件组件本身在 BP 组件树 |
| `Mesh3P` 可见性 | 默认隐藏、无碰撞 | 为后续 3P 预留，`SetPerspective(false)` 可切换 |
| 复制 | `bReplicates=false` | 本地表现 Actor，后续装备系统再决定网络方案 |

**成对 Montage（BP_PVP_Weapon_AR CDO）**：

| 属性 | 角色 Montage（`SKEL_PVP_Mannequin`） | 武器 Montage（`SKEL_PVP_AR`） | PlayRate |
| --- | --- | --- | --- |
| `EquipMontage` | `AM_PVP_FP_AR_Equip` | `AM_PVP_FP_WEP_AR_Equip` | `1.0` |
| `FireMontage` | `AM_PVP_FP_AR_Fire_Auto` | `AM_PVP_FP_WEP_AR_Fire` | `1.0` |
| `AimedFireMontage` | `AM_PVP_FP_AR_Fire_Aimed` | `AM_PVP_FP_WEP_AR_Fire` | `1.0` |
| `ReloadMontage` | `AM_PVP_FP_AR_Reload` | `AM_PVP_FP_WEP_AR_Reload` | `1.0` |

所有 Montage 的 Slot 名称均为 `DefaultSlot`。

**公开接口**：

| 接口 | 类型 | 行为 |
| --- | --- | --- |
| `EquipTo(ACharacter*)` | `BlueprintCallable` | 附着到角色 Mesh 的 `AttachSocketName`，关闭拾取球，返回是否成功 |
| `Unequip(bool bDropWeapon)` | `BlueprintCallable` | 脱离角色；`bDropWeapon=true` 时放到角色前方并进入拾取冷却 |
| `DropWeapon()` | `BlueprintCallable` | 等价于 `Unequip(true)` |
| `PlayEquipMontage()` / `PlayFireMontage()` / `PlayReloadMontage()` | `BlueprintCallable` | 同时播放角色 Mesh 与当前可见武器 Mesh 的成对 Montage |
| `PlayAimedFireMontage()` | `BlueprintCallable` | 瞄准状态下的成对射击；`AimedFireMontage` 为空时回退 `FireMontage` |
| `IsEquipped()` | `BlueprintPure` | 是否已被角色持有 |
| `GetActiveMesh()` | `BlueprintPure` | 当前可见的武器 Mesh（默认 `Mesh1P`） |
| `GetMuzzleLocation()` | `BlueprintPure` | `SOCKET_Muzzle` 的世界位置，缺失时回退 Actor 位置 |
| `SetPerspective(bool)` | `BlueprintCallable` | 切换 `Mesh1P` / `Mesh3P` 可见性（3P 预留） |
| `GetOwningCharacter()` | `BlueprintPure` | 当前持有者（可能为空） |

### 3.2 装备组件（`UPVPEquipmentComponent`）

| 项 | 当前值 / 行为 | 说明 |
| --- | --- | --- |
| 挂载位置 | `APVPBaseCharacter` 构造期 `CreateDefaultSubobject` | 属性 `EquipmentComponent`（`VisibleAnywhere` / `BlueprintReadOnly`） |
| `CurrentWeapon` | 当前武器或空 | 只读给动画 / UI 消费 |
| `EquipWeapon(APVPWeaponActor*)` | 无武器时调用 `Weapon->EquipTo(OwnerCharacter)` | 已有武器时拒绝并写日志；一次只持有一把 |
| `UnequipCurrentWeapon(bool)` | 清空指针并调用武器 `Unequip` | `true` 时丢弃 |
| `DropCurrentWeapon()` | 等价于 `UnequipCurrentWeapon(true)` | |
| `HasWeapon()` | 蓝图只读 | |
| 依赖 | 只用 `ACharacter` 与 `APVPWeaponActor` | 不包含 `APVPBaseCharacter` 具体类 |

### 3.3 角色侧装配

| 项 | 当前值 | 说明 |
| --- | --- | --- |
| `EquipmentComponent` | C++ 构造期创建 | 蓝图实例自动继承，无需在 BP 中手工添加 |
| 角色 Mesh | `SKM_PVP_FP_Manny_Simple` | 挂点 `ik_hand_gun` 来自 `SKEL_PVP_Mannequin` |
| 现有逻辑 | 未改 Move / Look / Jump / Run、`CameraFP`、走跑速度 | 本轮只新增组件与武器拾取 |

### 3.4 配件与装配（权威摘要）

配件网格已按槽位归位到 `Content/PVPDemo/Weapons/AssaultRifle/Attachments/<Group>/`，槽位 / 插槽 / 组件名的完整表在 `Equipment-Attachments.md` 第 2 节，此处只列装配现状。

| 槽位 | 父级 | 插槽 | 组件 | 蓝图默认（Basic） |
| --- | --- | --- | --- | --- |
| `Handguard` | `Mesh1P` | `SOCKET_Handguard` | `ATT_Handguard` | `SM_PVP_AR_Handguard_Default` |
| `FrontSight` | `ATT_Handguard` | `SOCKET_Sight_Front` | `ATT_FrontSight` | `SM_PVP_AR_ATT_Sight_Front` |
| `RearSight` | `Mesh1P` | `SOCKET_Sight_Rear` | `ATT_RearSight` | `SM_PVP_AR_ATT_Sight_Rear` |
| `Magazine` | `Mesh1P` | `SOCKET_Magazine` | `ATT_Magazine` | `SM_PVP_AR_Magazine_Full` |

- 方案：`Basic`（默认，4 件）/ `Tactical`（8 件：延长护木 + 瞄具 + 折叠机瞄 + 垂直握把 + 激光 + 消音器 + 满弹匣）。
- 目录数据资产：`/Game/PVP/Data/DA_PVP_WeaponAttachmentCatalog`（8 个槽位定义 + 两套预设），挂在 `BP_PVP_Weapon_AR` 的 `AttachmentCatalog`。
- 类型契约：`Source/PVPDemo/Equipment/PVPWeaponAttachmentTypes.h`（`E_PVPWeaponAttachmentSlot` 等），`Content/PVP/Types/` 不再镜像同名枚举。
- 组件命名 / 标签：`ATT_<Slot>` / `PVP.Attachment.<Slot>`；组件由 `PVPWeaponAssemblyBuilder` 写入并读回。

**配件接口（`APVPWeaponActor`）**：

| 接口 | 类型 | 行为 |
| --- | --- | --- |
| `RefreshAttachmentComponents()` | `BlueprintCallable` | 扫描自身 `PVP.Attachment.<Slot>` 标签重建槽位表（`OnConstruction` / `BeginPlay` 自动调用） |
| `GetAttachmentComponent(Slot)` / `GetAttachmentMesh(Slot)` | `BlueprintPure` | 取装配组件 / 当前网格，未装配返回空 |
| `SetAttachmentEnabled(Slot, bEnabled)` | `BlueprintCallable` | 显隐单件配件，保留网格 |
| `ApplyAttachmentMesh(Slot, Mesh)` | `BlueprintCallable` | 运行时换单件网格，socket 与变换不变 |
| `ApplyAttachmentLoadout(Name)` | `BlueprintCallable` | 按目录预设切换网格与显隐，返回更新的槽位数 |

---

## 4. 启动与运行时入口

| 阶段 | 执行者 | 行为 |
| --- | --- | --- |
| 地图加载 | `L_PVP_Showcase` | PlayerStart 前方约 200cm、向下 80cm 放置 `PVP_WeaponPickup_AR` |
| 武器生成 / 放置 | `APVPWeaponActor::OnConstruction` | 应用网格、AnimClass、拾取半径、1P/3P 可见性 |
| 接近武器 | `PickupSphere::OnComponentBeginOverlap` | 只对 Pawn 响应；找到 Pawn 上的 `UPVPEquipmentComponent` |
| 拾取 | `UPVPEquipmentComponent::EquipWeapon` | 当前无武器 → `APVPWeaponActor::EquipTo` |
| 附着 | `APVPWeaponActor::EquipTo` | `AttachToComponent(CharacterMesh, ..., "ik_hand_gun")`，应用 `AttachOffset`，关闭拾取球 |
| 装备表现 | `APVPWeaponActor::PlayEquipMontage` | 角色 Mesh 与武器 Mesh 同步 `Montage_Play` |
| 丢弃 | `DropWeapon` / `UnequipCurrentWeapon(true)` | `DetachFromActor`，放到角色前方，`PickupCooldown` 后恢复拾取 |
| 每帧 | 无 | 武器 Actor `PrimaryActorTick.bCanEverTick=false`，不使用逐帧扫描 |

**关键日志**：

```text
LogPVPWeaponActor: <weapon>: equipped by <character> (socket=ik_hand_gun).
LogPVPWeaponActor: <weapon>: unequipped (drop=true).
LogPVPEquipmentComponent: <component>: equipped <weapon> on <character>.
```

失败信号：拾取无反应先看 `LogPVPEquipmentComponent` 是否“already holding”，再确认 Pawn 上是否存在 `EquipmentComponent`；武器错位先检查 `ik_hand_gun` 挂点与 `AttachOffset`。

---

## 5. 未实现能力与扩展方向

- **开火 / 换弹玩法**：已由角色把 `IA_PVP_Fire` 接到 `PlayFireMontage` / `PlayAimedFireMontage`（单击一次，腰射 / 瞄准两套），但未扣弹药、无射速 / 后坐力 / 换弹状态机；`PlayReloadMontage` 仍只有接口。
- **动画通知副作用**：武器 / 角色 Montage 内的 `AN_PVP_*` / `ANS_PVP_*` 会 Cast 到迁移逻辑蓝图 `BP_PVP_BaseWeapon` / `BP_PVP_BaseCharacter`，新的 C++ 武器与 `BP_PVPBaseCharacter` 不是这些类，通知会失败（不生成弹壳 / 弹匣）；Montage 本体仍正常播放。
- **3P**：`Mesh3P` 组件已存在但隐藏，无 3P 角色网格与可见性管理。
- **网络**：无复制、无服务器权威装备状态。
- **武器切换 / 多武器槽**：`UPVPEquipmentComponent` 只维护一把当前武器。
- **武器 DataAsset**：当前用 Actor 级配置属性，尚未落地规划中的 `PVPWeaponDefinition`；配件侧已有 `DA_PVP_WeaponAttachmentCatalog`。
- **瞄准 / 后坐 / 枪口特效**：瞄准姿态与 FOV 由 `Character/` + `Animation/` 接入，武器侧只有瞄准射击 Montage；`GetMuzzleLocation` 已提供，但未接 Niagara / 音效。
- **配件玩法**：已实现分类、槽位切换接口与 `Basic` / `Tactical` 两套装配，未接配件 UI、数值加成与拾取式换装。
- **3P 配件**：`Mesh3P` 未挂配件，3P 角色落地时再补。
- **弹匣动画**：装配用静态满弹匣；换弹下坠需改用骨骼弹匣并单独接 AnimClass。

---

## 6. 校验与报告入口

| 入口 | 作用 | 产物 |
| --- | --- | --- |
| UBT 编译 | 编译 `PVPDemoEditor`，验证武器 Actor / 装备组件 / 构建器 | `UnrealEditor-PVPDemo.dll` |
| `Content/Python/setup_pvp_weapon.py` | 生成瞄准射击 Montage / 武器 ABP / 武器 BP、给角色 ABP 加 `DefaultSlot`、放置关卡拾取物、逐项读回 | `Saved/PVPWeaponReport/report.json`、`report.md`、`aimed_fire_montage_builder.json`、`weapon_abp_builder.json`、`slot_builder.json` |
| `Content/Python/reorg_pvp_weapon_attachments.py` | 把 20 个配件网格按槽位归入 `Attachments/<Group>/`；默认 dry-run，`PVP_ATTACHMENT_REORG_APPLY=1` 执行 | `Saved/PVPAttachmentReport/reorg.{json,md}` |
| `Content/Python/setup_pvp_weapon_assembly.py` | 生成 / 更新配件目录数据资产、调用构建器装配（`Basic` / `Tactical`）、逐项读回、spawn 冒烟断言 socket | `Saved/PVPAttachmentReport/report.{json,md}`、`report_tactical.{json,md}`、`assembly_builder.json`、`readback_builder.json` |
| `Content/Python/verify_pvp_asset_layout.py` | 校验 `Actors` 20、`Animation/AnimBP` 5、`Data` 2、逻辑总数 81、旧路径零残留 | `Saved/PVPAssetReorgReport/verify.{json,md}` |
| `Content/Python/verify_pvp_game_framework.py` | 校验零第三方引用、地图武器拾取物数量与 Actor 总数 | `Saved/PVPGameFrameworkReport/verify.{json,md}` |
| PIE 冒烟 | 走近 → 自动拾取 → 附着 → 移动 / 跳跃跟随 → Equip Montage | 无（手动，清单见 `Equipment-Dev.md` 第 6 节） |

生成命令（编辑器关闭、编译完成后）：

```text
UnrealEditor-Cmd.exe "D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -run=PythonScript ^
    -Script="D:/UE/Unreal Projects/PVPDemo/Content/Python/setup_pvp_weapon.py" ^
    -unattended -nosplash -nullrhi
```

脚本可重复执行：武器 ABP 会删除重建，角色 ABP 的 `DefaultSlot` 原地检测 / 插入（不删除资产），武器 BP 复用并重设 CDO，地图拾取物按标签复用 / 清理 / 生成。

---

## 7. 文档维护规则

| 变更 | 必须同步 |
| --- | --- |
| 武器组件默认值、挂点、Montage 名称变化 | 本文第 3.1 节 + `Equipment-Dev.md` 第 3 / 4 节 + `setup_pvp_weapon.py` 的读回项（含 `AimedFireMontage`） |
| 拾取 / 附着 / 丢弃行为变化 | 本文第 4 节 + `Equipment-Dev.md` 第 3 / 6 节 + `Equipment-Integration.md` 第 3 / 5 节 |
| C++ 接口签名变化 | 本文第 3 节 + `Equipment-Integration.md` 第 4 / 7 节 |
| 武器资产新增 / 改名 / 移动 | `Docs/Content-Structure.md` + `Content/Python/pvp_logic_layout.py` + 本文第 3 节 |
| 配件槽位 / 插槽 / 候选网格 / 默认方案变化 | 本文第 3.4 节 + `Equipment-Attachments.md` 第 2 / 3 节 + `PVPWeaponAttachmentTypes.h` + `reorg_pvp_weapon_attachments.py` / `setup_pvp_weapon_assembly.py` |
| 配件装配接口变化 | 本文第 3.4 节 + `Equipment-Integration.md` 第 4 / 7 节 |
| 出现新坑点或校验步骤 | `Equipment-Dev.md` 第 5 / 6 节（踩坑四要素齐全） |
| 新增本模块维护文档 | 本文第 2 节登记，命名 `Equipment-<主题>.md` |
| 任何改动 | 三份文档文首「最后更新」刷新为当天日期 |

- 武器默认值与成对 Montage 的权威表只存在于本文第 3 节；其他文档只链接，不复制。
- 资产总数只写在 `Docs/Content-Structure.md` / `Docs/Source-Structure.md`，本文不复制总数。
- 本模块文档与代码 / 资产不一致时，以真实情况为准并当轮修正文档。
