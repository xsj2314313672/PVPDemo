# PVP 武器配件分类与装配

> 维度：装备 / 配件分类与蓝图内装配
> 模块：Equipment
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Equipment/README.md`（导航与权威表）、`Docs/Equipment/Equipment-Dev.md`（SOP 与踩坑）、`Docs/Equipment/Equipment-Integration.md`（接口契约）

本文件是**配件分类、槽位 / 插槽映射、装配方案与装配过程**的权威表。其他文档只链接，不复制。

---

## 1. 定位与边界

**职责**：把突击步枪的散件网格（护木、机瞄、瞄具、握把、枪口、激光、弹匣）按槽位分类、归位到独立子目录，并在 `BP_PVP_Weapon_AR` 的组件树里装配成一把完整步枪。

**非职责**：

- 开火 / 弹药 / 换弹玩法：只做到"外观装配 + 槽位可切换"，不接输入与伤害。
- 配件 UI：目录数据资产已备好，界面属于后续阶段。
- 第三人称配件：`Mesh3P` 仍不带配件（3P 角色未实现）。
- 网络复制：武器与配件均为本地表现。

**类型落点**：槽位枚举与数据结构定义在 `Source/PVPDemo/Equipment/PVPWeaponAttachmentTypes.h`（C++，BlueprintType 反射），`Content/PVP/Types/` **不新建同名蓝图枚举**，避免两份事实来源漂移；可编辑的分类与预设放在 `Content/PVP/Data/DA_PVP_WeaponAttachmentCatalog`。

---

## 2. 分类总表（权威）

`SK_PVP_AR` 只是枪身本体（仅引用 `MI_PVP_AR_01/02`），其余部件均为独立网格。

| 槽位（枚举） | 父级 | 插槽 | 蓝图组件 | 候选网格（`Attachments/<Group>/`） | 默认项 |
| --- | --- | --- | --- | --- | --- |
| `Handguard` | `Mesh1P` | `SOCKET_Handguard` | `ATT_Handguard` | `SM_PVP_AR_Handguard_Default` / `_Extended` / `_Short` | `_Default` |
| `FrontSight` | `ATT_Handguard` | `SOCKET_Sight_Front` | `ATT_FrontSight` | `SM_PVP_AR_ATT_Sight_Front` / `_Front_Folded` | `Sight_Front` |
| `RearSight` | `Mesh1P` | `SOCKET_Sight_Rear` | `ATT_RearSight` | `SM_PVP_AR_ATT_Sight_Rear` / `_Rear_Folded` | `Sight_Rear` |
| `Optic` | `Mesh1P` | `SOCKET_Scope` | `ATT_Optic` | `SM_PVP_AR_ATT_Scope` | `ATT_Scope` |
| `ForeGrip` | `ATT_Handguard` | `SOCKET_Grip` | `ATT_ForeGrip` | `SM_PVP_AR_ATT_Grip_Angled` / `_Vertical` | `Grip_Angled` |
| `Muzzle` | `ATT_Handguard` | `SOCKET_Muzzle` | `ATT_Muzzle` | `SM_PVP_AR_ATT_Silencer` | `ATT_Silencer` |
| `Laser` | `ATT_Handguard` | `SOCKET_Laser` | `ATT_Laser` | `SM_PVP_AR_ATT_Laser` | `ATT_Laser` |
| `Magazine` | `Mesh1P` | `SOCKET_Magazine` | `ATT_Magazine` | `SM_PVP_AR_Magazine_Full` / `_Empty` / `_Transparent_Full` / `_Transparent_Empty` | `_Full` |

**挂接链**：`Mesh1P → ATT_Handguard → ATT_FrontSight / ATT_ForeGrip / ATT_Muzzle / ATT_Laser`；`Mesh1P` 直接承载 `ATT_RearSight` / `ATT_Optic` / `ATT_Magazine`。

**为什么前准星、握把、激光、消音器必须挂护木**：`SOCKET_Sight_Front` / `SOCKET_Grip` / `SOCKET_Laser` / `SOCKET_Muzzle` **只存在于三个护木网格**（`SM_PVP_AR_Handguard_*`）；骨架 `SKEL_PVP_AR` 上的 `SOCKET_Grip` / `SOCKET_Laser` / `SOCKET_Muzzle` 是给无护木态或动画用的，挂到 `Mesh1P` 会落到网格根导致错位。

**骨架插槽全集**（`SKEL_PVP_AR`）：`SOCKET_Handguard`、`SOCKET_Grip`、`SOCKET_Laser`、`SOCKET_Muzzle`、`SOCKET_Scope`、`SOCKET_Sight_Rear`、`SOCKET_Magazine`、`SOCKET_Magazine_Reserve`、`SOCKET_Bullet_Chambered`、`SOCKET_Eject_Casing`、`SOCKET_Casing_Jammed`、`SOCKET_GunCamera`。

### 2.1 非装配件（分类登记，不装到武器）

| 资产 | 分类 | 说明 |
| --- | --- | --- |
| `SM_PVP_AR_Bullet` | 弹药 | 走 `SOCKET_Bullet_Chambered`，由动画通知生成，不常驻组件 |
| `SM_PVP_AR_Bullet_Casing` | 弹药 | 走 `SOCKET_Eject_Casing`；被 `L_PVP_Showcase` 引用，**保留在 `Meshes/`** |
| `SM_PVP_AR_GunCase` / `_Lid` | 道具 | 枪箱，关卡道具；被 `L_PVP_Showcase` 引用，**保留在 `Meshes/`** |
| `SK_PVP_AR_Magazine` / `_Transparent`、`SKEL_PVP_AR_Magazine`、`PHYS_PVP_AR_Magazine` | 弹匣（动画 / 物理变体） | 归入 `Attachments/Magazine/`，供后续换弹动画与掉落物使用；当前装配用静态满弹匣 |

> 弹药与枪箱虽然参与分类，但被地图引用；移动它们会要求重存关卡包，收益低于风险，因此保留原路径。归位脚本对它们只做分类登记与存在性校验。

---

## 3. 两套装配方案

| 方案 | 内容 | 状态 |
| --- | --- | --- |
| `Basic`（基础型） | 护木 `_Default` + 前准星 `Sight_Front` + 后照门 `Sight_Rear` + 满弹匣 | **蓝图默认装配**（4 个组件） |
| `Tactical`（战术型） | 护木 `_Extended` + 折叠前准星 + 折叠后照门 + 光学瞄具 + 垂直握把 + 消音器 + 激光 + 满弹匣 | 已跑通并归档读回（8 个组件），需要时按第 5 节换装 |

两套方案同时登记在 `DA_PVP_WeaponAttachmentCatalog` 的 `Loadouts` 中，`BuildLoadoutAssembly(Name)` 会按预设生成规格，缺失槽位回落到槽位默认项。

### 3.1 本轮实际装配结果（`Basic`，读回证据）

| 组件 | 父级 | 插槽 | 网格 |
| --- | --- | --- | --- |
| `ATT_Handguard` | `Mesh1P`（原生组件） | `SOCKET_Handguard` | `Attachments/Handguard/SM_PVP_AR_Handguard_Default` |
| `ATT_FrontSight` | `ATT_Handguard`（SCS 节点） | `SOCKET_Sight_Front` | `Attachments/Sight/SM_PVP_AR_ATT_Sight_Front` |
| `ATT_RearSight` | `Mesh1P` | `SOCKET_Sight_Rear` | `Attachments/Sight/SM_PVP_AR_ATT_Sight_Rear` |
| `ATT_Magazine` | `Mesh1P` | `SOCKET_Magazine` | `Attachments/Magazine/SM_PVP_AR_Magazine_Full` |

- 每个组件带标签 `PVP.Attachment.<Slot>`，相对变换 `Identity`（socket 自带对位）。
- 编译 0 error，保存成功，`BP_PVP_Weapon_AR` 的 `AttachmentCatalog` 指向 `DA_PVP_WeaponAttachmentCatalog`。
- **运行时冒烟**：脚本在编辑器世界临时 spawn 一次武器，断言每个配件的 `GetAttachSocketName()` / `GetAttachParent()` / 网格，确认 SCS 真的按 `AttachToName` 挂上了 socket（不是只写进了字段），断言后销毁实例。
- 报告：`Saved/PVPAttachmentReport/report.{json,md}`（全部检查 PASS，含 `runtime.*` 项）。

`Tactical` 读回（已归档 `report_tactical.{json,md}`）：8 个组件，`ATT_Optic` → `Mesh1P`/`SOCKET_Scope`，`ATT_ForeGrip` → `ATT_Handguard`/`SOCKET_Grip`，`ATT_Muzzle` → `ATT_Handguard`/`SOCKET_Muzzle`，`ATT_Laser` → `ATT_Handguard`/`SOCKET_Laser`，其余同基础型。

---

## 4. 装配机制

```
reorg_pvp_weapon_attachments.py   资产归位（Attachments/<Group>/）
        │
setup_pvp_weapon_assembly.py ──► DA_PVP_WeaponAttachmentCatalog（槽位定义 + 两套预设）
        │
        └──► UPVPWeaponAssemblyBuilder（编辑器 C++）
                 ├─ CreateNode / AddChildNode / SetParent(native Mesh1P)
                 ├─ USCS_Node::AttachToName = socket
                 ├─ ComponentTags = PVP.Attachment.<Slot>
                 ├─ 编译 + 保存 BP
                 └─ 写 assembly_builder.json / readback_builder.json
```

**为什么装配必须走编辑器 C++**：

- `USceneComponent::AttachSocketName` 是 `UPROPERTY(ReplicatedUsing=...)`（无 `CPF_Edit`，且 setter `SetAttachSocketName` 为 private），`USCS_Node::AttachToName` 是裸 `UPROPERTY()`；Python 的 `set_editor_property` 对两者都会被拒。
- `USCS_Node::ExecuteNode` 用 `SetupAttachment(ParentComponent, AttachToName)` 建立父子关系，socket 只能写在 SCS 节点上。
- 因此 Python 负责"数据 + 读回断言"，C++ 负责"组件树 + socket 写入"。

**运行时接口**（`APVPWeaponActor`，见 `Equipment-Integration.md` 第 4 节）：`OnConstruction` / `BeginPlay` 扫描自身带 `PVP.Attachment.<Slot>` 标签的 `UStaticMeshComponent` 建槽位表（无 Tick），对外提供 `GetAttachmentComponent` / `GetAttachmentMesh` / `SetAttachmentEnabled` / `ApplyAttachmentMesh` / `ApplyAttachmentLoadout`。

---

## 5. 装配过程记录与换装 SOP

### 5.1 首次装配（已执行）

```text
# 1) 编译（编辑器关闭）
Build.bat PVPDemoEditor Win64 Development -Project="<project>/PVPDemo.uproject" -WaitMutex -NoHotReload

# 2) 配件归位（apply；默认 dry-run）
set PVP_ATTACHMENT_REORG_APPLY=1
UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/reorg_pvp_weapon_attachments.py" -unattended -nosplash -nullrhi

# 3) 目录数据资产 + 基础型装配 + 读回
UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/setup_pvp_weapon_assembly.py" -unattended -nosplash -nullrhi
```

阶段结果：

| 阶段 | 结果 | 证据 |
| --- | --- | --- |
| 配件归位 | 20 个网格移动到 7 个槽位子目录，0 失败；保留枪身 3 + 非装配件 4 于 `Meshes/` | `Saved/PVPAttachmentReport/reorg.{json,md}` |
| 目录数据资产 | 8 个槽位定义 + `Basic` / `Tactical` 两套预设，保存在 `/Game/PVP/Data/` | `report.md` 的 `catalog.*` |
| 蓝图装配（Basic） | 4 个配件组件，socket / 父级 / 网格 / 标签逐项读回 PASS | `report.md` 的 `assembly.*` |
| 装配（Tactical） | 8 个配件组件，读回 PASS；随后恢复 Basic 时 4 个战术件被清理 | `report_tactical.{json,md}`、`report.json` 的 `assembly.removed` |
| 编译 | `BP_PVP_Weapon_AR` 0 error，包已保存 | `report.md` 的 `assembly.compiled` / `saved` |
| 运行时 socket | spawn → 4 个组件的 socket / 父级 / 网格全部正确 → 销毁 | `report.md` 的 `runtime.*` |

### 5.2 换装（基础型 ↔ 战术型）

```text
# 切换到战术型
set PVP_WEAPON_LOADOUT=Tactical
UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/setup_pvp_weapon_assembly.py" -unattended -nosplash -nullrhi

# 回到基础型（战术件会被移除，Basic 组件只更新不重建）
set PVP_WEAPON_LOADOUT=Basic
（同上命令）
```

- 脚本按预设**精确装配**：预设外的配件组件会被删除（`bRemoveUnlisted=true`），重跑同一预设只更新网格 / socket / 标签，不重复创建。
- 运行时也可不改蓝图：`ApplyAttachmentLoadout("Tactical")` 会按目录切换已有的槽位并显隐未列出的配件；缺失的槽位（例如当前没有 `ATT_Optic`）会记 Warning 并跳过，需要先跑一次战术型装配把组件建出来。
- 只换单件：`ApplyAttachmentMesh(E_PVPWeaponAttachmentSlot::Handguard, Mesh)`。

### 5.3 校验入口

| 入口 | 期望结果 |
| --- | --- |
| `Saved/PVPAttachmentReport/report.md` | `result: PASS`，无 Errors |
| `Saved/PVPAttachmentReport/reorg.md` | `verification failures: 0` |
| `report.md` 的 `runtime.*` | 运行时 spawn 后的 socket / 父级 / 网格全部 PASS（自动化的"装配真的生效"证据） |
| `python Content/Python/verify_pvp_asset_layout.py` | `Data` 2、逻辑总数 81、旧路径零残留全部 PASS |
| `verify_pvp_game_framework.py` | 零第三方活引用、地图拾取物 1；`map.source_matches_baseline` 为历史基线问题（见 `Equipment-Dev.md` 踩坑 10），本模块不修改第三方原包 |
| PIE | 拾取后武器带护木 / 前后机瞄 / 弹匣，移动与跳跃跟随正常 |

---

## 6. 已知限制

- 只有 1P 装配：`Mesh3P` 未挂配件（3P 角色未实现）。
- 弹匣使用静态满弹匣网格；换弹动画如需弹匣下坠，需改用骨骼弹匣 `SK_PVP_AR_Magazine` 并单独接 AnimClass（后续换弹阶段）。
- 配件不参与网络复制与存档；换装是编辑器期（脚本）或运行时手动调用，没有 UI。
- 装备 Montage 内的第三方动画通知（弹壳 / 弹匣掉落）仍 Cast 到旧蓝图，不会生效（见 `Equipment-Dev.md` 踩坑 4）。
- Lasers / 消音器只做外观，"开火特效 / 弹道"未接入。

---

## 7. 维护规则

| 变更 | 必须同步 |
| --- | --- |
| 新增 / 重命名配件网格 | 本文第 2 节 + `Content/Python/reorg_pvp_weapon_attachments.py` 的 `ATTACHMENT_GROUPS` + `Docs/Content-Structure.md` |
| 新增槽位 / 改插槽 | 本文第 2 节 + `PVPWeaponAttachmentTypes.h`（枚举、socket、父级、组件名）+ 数据资产重建 + `Equipment-Integration.md` 第 4 节 |
| 调整默认 / 战术方案 | 本文第 3 节 + `setup_pvp_weapon_assembly.py` 的 `LOADOUTS` + 重跑装配 |
| 装配流程或构建器行为变化 | 本文第 4 / 5 节 + `Equipment-Dev.md` 第 4 / 5 节 + `Equipment-Integration.md` 第 6 节 |
| 任何改动 | 本文与三件套文首「最后更新」刷新为当天日期 |
