# PVP 装备模块开发文档

> 维度：装备（武器 Actor / 拾取附着 / 成对 Montage）
> 模块：Equipment
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/Equipment/README.md`（导航与权威表）、`Docs/Equipment/Equipment-Integration.md`（依赖与接口契约）

---

## 1. 目的与范围

本文记录装备模块的实现方式、资产生成 SOP、踩坑与校验步骤，覆盖：

- `Source/PVPDemo/Equipment/PVPWeaponActor.h/.cpp`
- `Source/PVPDemo/Equipment/PVPEquipmentComponent.h/.cpp`
- `Source/PVPDemo/Character/PVPBaseCharacter` 的 `EquipmentComponent` 挂载
- `Source/PVPDemoEditor/PVPAnimBlueprintBuilder` 的武器 ABP 构建与原地 Slot 插入
- `Content/Python/setup_pvp_weapon.py` 与相关布局 / 框架校验脚本

不覆盖开火伤害、弹药、换弹状态机、3P 角色、网络复制与 GAS 技能（见第 7 节）。

---

## 2. 当前状态与源码结构

### 2.1 源码

| 文件 | 职责 |
| --- | --- |
| `Equipment/PVPWeaponActor.h/.cpp` | `APVPWeaponActor`：`SceneRoot` + `Mesh1P` / `Mesh3P` + `PickupSphere`；Actor 级配置在 `OnConstruction` 应用；自动拾取、附着 / 卸下 / 丢弃、成对 Montage、枪口查询、1P/3P 切换；扫描 `PVP.Attachment.<Slot>` 标签建配件槽位表，提供查询 / 显隐 / 换网格 / 应用预设 |
| `Equipment/PVPEquipmentComponent.h/.cpp` | `UPVPEquipmentComponent`：`CurrentWeapon`、"一次一把"、`EquipWeapon` / `UnequipCurrentWeapon` / `DropCurrentWeapon`；只依赖 `ACharacter` 与武器 Actor |
| `Equipment/PVPWeaponAttachmentTypes.h` | 配件槽位契约：`E_PVPWeaponAttachmentSlot`、`FPVPWeaponAttachmentSpec` / `Option` / `SlotDefinition` / `Loadout`，以及槽位名 / 组件名 / 标签 / 默认 socket / 默认父级映射 |
| `Equipment/PVPWeaponAttachmentCatalog.h/.cpp` | `UPVPWeaponAttachmentCatalog : UDataAsset`：槽位定义 + 候选网格 + `Basic` / `Tactical` 预设，提供 `BuildLoadoutAssembly` / `BuildDefaultAssembly` |
| `Character/PVPBaseCharacter.h/.cpp` | 新增 `EquipmentComponent` 属性与构造期 `CreateDefaultSubobject`；不改现有 Mesh / CameraFP / 输入 / 走跑逻辑 |
| `PVPDemoEditor/PVPAnimBlueprintBuilder.h/.cpp` | `BuildLocomotionAnimBlueprint` 增加 `DefaultSlot`；新增 `BuildWeaponAnimBlueprint` 与 `EnsureMontageSlot` |
| `PVPDemoEditor/PVPWeaponAssemblyBuilder.h/.cpp` | `AssembleAttachments` / `ReadbackAttachments`：在武器蓝图 SCS 中增改 `ATT_<Slot>` 组件、写 `USCS_Node::AttachToName`、设标签与网格、编译保存并出报告 |

运行时模块 `PVPDemo.Build.cs` 无新增依赖（骨骼网格、AnimInstance、SphereComponent、AnimMontage、StaticMeshComponent、DataAsset 均属 `Engine`）。编辑器模块沿用现有 `AnimGraph` / `AnimGraphRuntime` / `BlueprintGraph` / `AssetTools`，并在 `PublicDependencyModuleNames` 新增 `PVPDemo`（共享配件槽类型，Editor → Runtime 单向）。

### 2.2 资产

| 资产 | 路径 | 说明 |
| --- | --- | --- |
| `BP_PVP_Weapon_AR` | `/Game/PVP/Actors/` | 父类 `APVPWeaponActor`；配置 `SK_PVP_AR`、`ABP_PVP_FP_Weapon_AR_C`、`ik_hand_gun`、`SOCKET_Muzzle`、拾取半径与三组 Montage；组件树含 `ATT_Handguard` / `ATT_FrontSight` / `ATT_RearSight` / `ATT_Magazine`（基础型），`AttachmentCatalog` 指向目录资产 |
| `DA_PVP_WeaponAttachmentCatalog` | `/Game/PVP/Data/` | 配件目录：8 个槽位定义（父级 / socket / 组件名 / 候选网格）+ `Basic` / `Tactical` 两套预设 |
| 配件网格 | `/Game/PVPDemo/Weapons/AssaultRifle/Attachments/` | 7 个槽位子目录共 20 个网格；枪身 3 个与非装配件 4 个留在 `Meshes/` |
| `ABP_PVP_FP_Weapon_AR` | `/Game/PVP/Animation/AnimBP/` | 父类 `UAnimInstance`；`SequencePlayer(A_PVP_WEP_AR_Reference)` → `DefaultSlot` → `Root` |
| `ABP_PVP_FP_Locomotion` | `/Game/PVP/Animation/AnimBP/` | 原地在 StateMachine 与 Root 之间插入 `DefaultSlot`；未删除重建 |
| 地图拾取物 | `/Game/PVP/Maps/L_PVP_Showcase` | 标签与 Actor Label 均为 `PVP_WeaponPickup_AR`；PlayerStart 前方约 200cm、向下 80cm |

### 2.3 脚本与报告

| 入口 | 作用 |
| --- | --- |
| `Content/Python/setup_pvp_weapon.py` | 校验依赖 → 重建武器 ABP → 原地确保角色 ABP Slot → 复用 / 创建武器 BP 并设 CDO → 放置 / 更新地图拾取物 → 读回并写报告 |
| `Content/Python/reorg_pvp_weapon_attachments.py` | 配件网格按槽位归位（默认 dry-run，`PVP_ATTACHMENT_REORG_APPLY=1` 执行）：pre-flight → SHA256 备份 → `rename_asset` → 重定向清理 → 保存 → 校验 → 报告 |
| `Content/Python/setup_pvp_weapon_assembly.py` | 生成 / 更新配件目录 → 调用 `PVPWeaponAssemblyBuilder` 装配（`PVP_WEAPON_LOADOUT=Basic\|Tactical`）→ 把目录挂到 BP CDO → 逐项读回 → 报告 |
| `Saved/PVPWeaponReport/report.{json,md}` | Python 汇总报告（检查项、BP 默认值、地图实例、C++ 构建器报告） |
| `Saved/PVPWeaponReport/weapon_abp_builder.json` | `BuildWeaponAnimBlueprint` 的 C++ 报告 |
| `Saved/PVPWeaponReport/slot_builder.json` | `EnsureMontageSlot` 的 C++ 报告 |
| `Saved/PVPAttachmentReport/reorg.{json,md}` | 配件归位报告（分类表、移动表、备份 SHA256、校验结果） |
| `Saved/PVPAttachmentReport/report.{json,md}` | 当前装配报告（分类、检查项、读回组件）；`report_tactical.{json,md}` 为战术型归档 |
| `Saved/PVPAttachmentReport/assembly_builder.json` | `AssembleAttachments` 写入 + 编译 + 保存的 C++ 报告 |
| `Saved/PVPAttachmentReport/readback_builder.json` | `ReadbackAttachments` 的 C++ 读回报告 |
| `Content/Python/pvp_logic_layout.py` | 资产清单与数量：`Actors` 20、`Animation/AnimBP` 5、`Data` 2、总数 81 |
| `Content/Python/verify_pvp_game_framework.py` | 地图 Actor 数允许一把 `PVP_WeaponPickup_AR`，并单独断言数量为 1 |

---

## 3. 设计决策与理由

| 决策 | 理由 |
| --- | --- |
| 武器配置放 Actor 级 `UPROPERTY(EditDefaultsOnly)`，`OnConstruction` 应用到组件 | 避免 Python 直接修改 BP 继承组件的模板；CDO 上的普通属性可以稳定保存与读回 |
| `FPVPWeaponMontagePair` 承载成对 Montage | 角色手臂与武器本体是两个骨架，必须成对播放；结构体避免三组零散字段 |
| 拾取用 `PickupSphere` Overlap，不新增输入 | 用户确认"靠近自动拾取"；不改 Input 模块与 `DA_PVP_InputConfig` |
| 装备组件只依赖 `ACharacter` 与武器 Actor | `Character → Equipment` 单向；Equipment 不反向包含角色具体类，也不依赖 Game / GAS |
| `EnsureMontageSlot` 原地插入而不是删除重建角色 ABP | `BP_PVPBaseCharacter` 的 Mesh AnimClass 覆盖指向 `ABP_PVP_FP_Locomotion`，删除重建会带来引用丢失风险 |
| 武器 ABP 使用 `A_PVP_WEP_AR_Reference` + `DefaultSlot` | 无 Montage 时保持武器参考姿势，有 Montage 时由 Slot 覆盖；与迁移 Montage 的 Slot 名一致 |
| 拾取后默认播放 Equip，Fire / Reload 仅暴露接口 | 用户确认先跑通三类示例；未经输入系统接入前不擅自新增动作 |
| 武器 Actor `bReplicates=false` | 当前无装备网络方案，先做本地表现；后续复制化或 GAS 接管 |
| 配件槽位枚举 / 结构体定义在 C++（`PVPWeaponAttachmentTypes.h`） | 编辑器构建器与运行时接口都要按槽位传参；蓝图枚举无法被 C++ 引用，C++ 与蓝图各定义一份必然漂移。`UENUM/USTRUCT` 对蓝图完全可见，可编辑部分放 `DA_PVP_WeaponAttachmentCatalog` |
| 配件组件放 `BP_PVP_Weapon_AR` 组件树，而不是运行时动态创建 | 用户要求"在已创建的蓝图里组装"：组件在编辑器可见、可手动微调；运行时只按标签建表，不做创建 |
| socket 由编辑器 C++ 构建器写入 | `AttachSocketName` / `AttachToName` 无 `CPF_Edit`，Python 无法写；`SubobjectDataSubsystem` 的 `SetupAttachment` 还会把 `AttachToName` 清空，因此直接写 `USCS_Node::AttachToName` |
| 槽位表按 `ComponentTags` 扫描，不依赖蓝图变量名 | 蓝图组件可被改名；标签 `PVP.Attachment.<Slot>` 是稳定契约，扫描 O(N) 且无 Tick |
| 装配用静态满弹匣 | 保留骨骼弹匣资产给换弹动画阶段；静态版无需额外 AnimClass，先保证外观完整 |
| 弹药 / 枪箱不移动目录 | 被 `L_PVP_Showcase` 引用，移动会要求重存关卡包；它们也不参与装配 |

---

## 4. 开发流程 SOP

### 4.1 修改武器配置

1. 优先在 `BP_PVP_Weapon_AR` 的 Class Defaults 修改 `Mesh1PAsset` / AnimClass / `AttachSocketName` / `MuzzleSocketName` / `PickupRadius` / Montage 对。
2. 若需要脚本可重复生成，改 `Content/Python/setup_pvp_weapon.py` 顶部常量或 CDO 设置段，不要只手改资产。
3. 修改后重跑生成脚本，确认 `Saved/PVPWeaponReport/report.md` 对应检查项为 PASS。

### 4.2 修改拾取 / 附着行为

1. C++ 只改 `Equipment/PVPWeaponActor` 与 `Equipment/PVPEquipmentComponent`，不要把武器逻辑写进 `Character/`。
2. 附着点仍在角色 Mesh 的 `ik_hand_gun`；需要偏移时设置 `AttachOffset`（BP CDO 或实例），不要在代码里写死。
3. 丢弃后恢复拾取依赖 `PickupCooldown` 与 `SetPickupSuppressed`；不要在 Tick 中扫描。
4. 同步 `Docs/Equipment/README.md` 第 3 / 4 节与 `Equipment-Integration.md` 第 4 / 5 节。

### 4.3 生成 / 重建武器资产与关卡拾取物

前置：关闭编辑器，编译 `PVPDemoEditor`。

```text
Build.bat PVPDemoEditor Win64 Development -Project="<project>/PVPDemo.uproject" -WaitMutex -NoHotReload

UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/setup_pvp_weapon.py" ^
    -unattended -nosplash -nullrhi
```

脚本阶段：

1. 校验 `SKEL_PVP_AR`、`SK_PVP_AR`、`A_PVP_WEP_AR_Reference`、角色 / 武器 Montage、`BP_PVPBaseCharacter`。
2. 删除 `ABP_PVP_FP_Weapon_AR` 并调用 `BuildWeaponAnimBlueprint` 重建，编译 / 保存 / 读回 Slot 与接线。
3. 调用 `EnsureMontageSlot` 给 `ABP_PVP_FP_Locomotion` 加 `DefaultSlot`；已存在且接线正确时保持原样。
4. 复用 / 创建 `BP_PVP_Weapon_AR`，设置 CDO 网格、AnimClass、挂点、拾取半径与三组 Montage；编译保存后逐项读回。
5. 加载 `L_PVP_Showcase`，按标签 `PVP_WeaponPickup_AR` 复用 / 清理 / 生成一把武器；保存并重新加载读回数量。
6. 检查 `BP_PVPBaseCharacter` 包内仍引用 `ABP_PVP_FP_Locomotion`。

### 4.4 修改成对动画

1. 角色 Montage 必须使用 `SKEL_PVP_Mannequin`，武器 Montage 必须使用 `SKEL_PVP_AR`，Slot 均为 `DefaultSlot`。
2. 在 `BP_PVP_Weapon_AR` 的 `EquipMontage` / `FireMontage` / `ReloadMontage` 中成对替换。
3. 通过 `PlayEquipMontage` / `PlayFireMontage` / `PlayReloadMontage` 触发；不要直接对角色 Mesh 单独播放。
4. 重跑生成脚本读回三组 Montage 路径。

### 4.5 配件归位与蓝图内装配

前置：关闭编辑器、编译 `PVPDemoEditor`（构建器与运行时类型都需要）。

```text
# 1) 归位（默认 dry-run，确认分类与移动表后再 apply）
set PVP_ATTACHMENT_REORG_APPLY=1
UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/reorg_pvp_weapon_attachments.py" -unattended -nosplash -nullrhi

# 2) 目录数据资产 + 装配 + 读回（默认 Basic；战术型用 PVP_WEAPON_LOADOUT=Tactical）
UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/setup_pvp_weapon_assembly.py" -unattended -nosplash -nullrhi

# 3) 布局 / 框架校验
python Content/Python/verify_pvp_asset_layout.py
```

要点：

1. **顺序不能反**：先归位再装配。装配脚本按归位后的新路径加载网格，顺序颠倒会拿到不存在的资产。
2. **分类常量单点维护**：槽位分组在 `reorg_pvp_weapon_attachments.py` 的 `ATTACHMENT_GROUPS` / `SUPPORT_ASSETS`，装配侧通过 `final_path()` 复用，不要在两处各写一份路径。
3. **新增槽位要改三处**：`PVPWeaponAttachmentTypes.h`（枚举 + socket + 父级 + 组件名）、`setup_pvp_weapon_assembly.py` 的 `SLOTS` / `LOADOUTS`、`reorg_pvp_weapon_attachments.py` 的 `ATTACHMENT_GROUPS`。
4. **幂等**：装配脚本按预设精确匹配，已存在的组件只更新网格 / socket / 标签；预设外的配件组件会被 `SCS->RemoveNode` 删除（只删叶子，带配件子节点的节点会保留并报错）。
5. **读回 + 运行时冒烟才算完成**：`report.md` 的 `assembly.<组件>.*`（socket / parent / mesh / tag）与 `runtime.<组件>.*`（spawn 后的 socket / 父级 / 网格）必须全 PASS；只看 `success=true` 不够。运行时项验证的是 SCS 真的按 `AttachToName` 挂上了 socket，而不是仅把字段写进资产。

---

## 5. 踩坑清单

### 踩坑 1：Python 修改 USTRUCT 的 `EditDefaultsOnly` 字段被拒绝

| 要素 | 内容 |
| --- | --- |
| 现象 | `setup_pvp_weapon.py` 报 `Property 'CharacterMontage' ... cannot be edited on instances`，武器 BP CDO 未写入 Montage |
| 根因 | `FPVPWeaponMontagePair` 的字段声明为 `EditDefaultsOnly`，UE Python 对结构体实例使用 `set_editor_property` 时按实例编辑权限拒绝 |
| 正确做法 | 结构体字段改为 `EditAnywhere`（外层 `EquipMontage` 等仍为 `EditDefaultsOnly`）；Python 逐字段构造 `PVPWeaponMontagePair` 后整体赋给 CDO |
| 验证方式 | 重跑脚本，`weapon_bp.equip_montage` / `fire_montage` / `reload_montage` 三项均为 PASS |

### 踩坑 2：`-Script` 使用反斜杠路径导致脚本“Could not load”

| 要素 | 内容 |
| --- | --- |
| 现象 | commandlet 日志报 `Could not load Python file 'D:\Unreal Projects\...'`，脚本完全没执行 |
| 根因 | `D:\UE\Unreal Projects` 中的 `\U` 序列在命令行 / 日志传递中被转义为制表符，路径解析失败 |
| 正确做法 | `-Script` 与项目路径一律使用正斜杠：`D:/UE/Unreal Projects/PVPDemo/...` |
| 验证方式 | 日志出现 `[PVPWeapon]` 前缀与 `Saved/PVPWeaponReport/report.md` |

### 踩坑 3：删除重建角色 ABP 会丢失蓝图 AnimClass 覆盖

| 要素 | 内容 |
| --- | --- |
| 现象 | 给角色 ABP 加 Slot 后，`BP_PVPBaseCharacter` 的 Mesh AnimClass 可能回到 C++ 默认 `ABP_PVP_FP_BaseCharacter`，角色不再使用 locomotion 图 |
| 根因 | 角色 ABP 的 AnimClass 是蓝图实例覆盖；删除并重建同名资产会让蓝图引用在重新加载时失效或回退 |
| 正确做法 | 用 `UPVPAnimBlueprintBuilder::EnsureMontageSlot` 原地插入 Slot；生成脚本读回 `BP_PVPBaseCharacter` 包内的 `ABP_PVP_FP_Locomotion` 引用；不要用 Python 改继承 Mesh 组件 |
| 验证方式 | `report.md` 的 `character.locomotion_reference` 为 PASS；打开 `BP_PVPBaseCharacter` 确认 Mesh AnimClass |

### 踩坑 4：Montage 内的第三方动画通知 Cast 失败

| 要素 | 内容 |
| --- | --- |
| 现象 | 播放武器 / 角色 Montage 时，弹壳、弹匣、治疗道具等不生成；日志可能出现 Cast 失败或空对象告警 |
| 根因 | `Content/PVP/Animation/Notifies/` 下的 `AN_PVP_*` / `ANS_PVP_*` 逻辑是 Cast 到迁移的 `BP_PVP_BaseWeapon` / `BP_PVP_BaseCharacter`，新的 `APVPWeaponActor` 与 `BP_PVPBaseCharacter` 不是这些类 |
| 正确做法 | 本轮接受"只播放 Montage 本体"；需要弹壳 / 弹匣时，按新类重写通知或把通知改为接口调用，不要直接改第三方原包 |
| 验证方式 | PIE 中 Montage 姿态正常播放；若必须验证弹壳，先接 `BP_PVP_BaseWeapon` 体系或补写通知 |

### 踩坑 5：框架校验的 Actor 数量把武器算成“多出来”

| 要素 | 内容 |
| --- | --- |
| 现象 | `verify_pvp_game_framework.py` 报 `map.actor_count_conserved target=154 source=153` |
| 根因 | 校验脚本按“目标地图 Actor 数 == 源地图 Actor 数”断言，武器拾取物是本次有意新增的 Actor |
| 正确做法 | 断言改为 `target == source + PVP_WeaponPickup_AR 数量`，并单独断言拾取物数量为 1；不要删除其他 Actor 来凑数 |
| 验证方式 | 重跑校验，`map.actor_count_conserved` 显示 `weapon_pickups=1`，`map.weapon_pickup_count` 为 PASS |

### 踩坑 6：`weapon_r` 在 1P 网格上不存在，实际挂点是 `ik_hand_gun`

| 要素 | 内容 |
| --- | --- |
| 现象 | 编辑器世界冒烟日志出现 `socket 'weapon_r' was not found on CharacterMesh0, falling back to the mesh root`；武器附着到角色根而不是手部 |
| 根因 | `SKM_PVP_FP_Manny_Simple` 使用 `SKEL_PVP_Mannequin`，但该 1P 网格调用 `DoesSocketExist('weapon_r')` 返回 false；实际可用的是 TFA 枪械 IK 目标 `ik_hand_gun`。`weapon_r` 只出现在骨架字符串表中，不能据此判定 Socket 存在 |
| 正确做法 | `APVPWeaponActor::AttachSocketName` 默认改为 `ik_hand_gun`；`setup_pvp_weapon.py` 把 `BP_PVP_Weapon_AR` 的 `attach_socket_name` 写为 `ik_hand_gun`；新增武器时用 `SkeletalMeshComponent::DoesSocketExist` 实测，不要凭文档 / 字符串表推断 |
| 验证方式 | 冒烟日志不再出现 `socket ... was not found`；`Saved/PVPWeaponReport/report.md` 的 `weapon_bp.attach_socket` 为 `ik_hand_gun` 且 PASS；PIE 中武器出现在手中 |

### 踩坑 7：武器挂点 / 偏移未在 PIE 校准

| 要素 | 内容 |
| --- | --- |
| 现象 | 武器拾取后可能偏离手部、朝前或穿模 |
| 根因 | `AttachOffset` 默认 `FTransform::Identity`，实际对齐依赖 `ik_hand_gun` 骨骼 / Socket 的编辑值；不同骨架 / 网格可能需要微调 |
| 正确做法 | 在 `BP_PVP_Weapon_AR` 的 `AttachOffset` 调整位置 / 旋转，不要改 C++ 写死偏移；调整后重跑脚本会保留 BP 默认值（脚本当前显式设置的上限字段不含 `AttachOffset`，不会被覆盖） |
| 验证方式 | PIE 拾取后观察武器与手部、准星、枪口位置；移动 / 跳跃时武器跟随手臂 |

### 踩坑 8：Python 无法写配件 socket，只能由编辑器 C++ 构建器写

| 要素 | 内容 |
| --- | --- |
| 现象 | 用 Python 给配件组件设 socket 时 `set_editor_property('attach_socket_name', ...)` 报属性不可编辑；改用 `SubobjectDataSubsystem` 的 `SetupAttachment` 后 socket 变成 `None` |
| 根因 | `USceneComponent::AttachSocketName` 是 `UPROPERTY(ReplicatedUsing=...)`（无 `CPF_Edit`）且 setter `SetAttachSocketName` 为 private；`USCS_Node::AttachToName` 是裸 `UPROPERTY()`；`FSubobjectData::SetupAttachment` 内部会把 `AttachToName` 重置为 `NAME_None` |
| 正确做法 | socket 由 `UPVPWeaponAssemblyBuilder` 在 C++ 里直接写 `Node->AttachToName = Socket`（`USCS_Node::ExecuteNode` 用它调 `SetupAttachment(Parent, AttachToName)`）；Python 只负责生成规格、调用构建器与读回 |
| 验证方式 | `Saved/PVPAttachmentReport/report.md` 的 `assembly.<组件>.socket` 与 `runtime.<组件>.socket` 一致（后者是 spawn 后 `GetAttachSocketName()` 的实测值）；PIE 中配件跟随护木 / 枪身骨骼运动 |

### 踩坑 9：前准星、握把、激光、消音器的 socket 只在护木网格上

| 要素 | 内容 |
| --- | --- |
| 现象 | 把这四类配件直接挂 `Mesh1P` 后，模型落在枪身原点附近而不是护木位置 |
| 根因 | `SOCKET_Sight_Front` / `SOCKET_Grip` / `SOCKET_Laser` / `SOCKET_Muzzle` 只存在于 `SM_PVP_AR_Handguard_*` 静态网格；骨架上的同名 socket 位置与目标不一致 |
| 正确做法 | `FPVPWeaponAttachmentSpec::ParentSlot` 设为 `Handguard`，构建器先确保 `ATT_Handguard` 存在并挂好，再把它作为父节点 `AddChildNode`；槽位顺序表 `SLOT_ORDER` 保证父级先写 |
| 验证方式 | `report.md` 中这四类组件的 `parent` 必须是 `ATT_Handguard`；`ATT_Handguard` 自身为 `Mesh1P` |

### 踩坑 10：框架校验的 `map.source_matches_baseline` 失败属历史基线

| 要素 | 内容 |
| --- | --- |
| 现象 | `verify_pvp_game_framework.py` 报 `map.source_matches_baseline` 失败（源地图 SHA256 与 `source_hashes.json` 不一致），其余 17 项 PASS |
| 根因 | 基线记录的是第三方源地图导入时的 hash，源文件在 2026-11-22 已被改动（早于配件工作）；本模块只移动 `Content/PVPDemo/Weapons` 下的资产，不修改 `Content/InfimaGames` |
| 正确做法 | 不改第三方原包，也不为了让校验通过而伪造基线；在报告与文档中标注为历史问题，需要时可评估是否接受当前 hash 并更新基线（需单独确认） |
| 验证方式 | `verify.json` 中 `assets.zero_live_third_party_refs` / `map.actor_count_conserved` / `map.weapon_pickup_count` 仍为 PASS；源地图 mtime 早于本轮改动 |

### 踩坑 11：`unreal` Python 对 `E_` 枚举与 `b` 布尔属性的命名规则

| 要素 | 内容 |
| --- | --- |
| 现象 | `unreal.PVPWeaponAttachmentSlot` 取不到；`set_editor_property('b_is_default_option', ...)` 报属性不存在 |
| 根因 | UE Python 的类型名 / 属性名会剥离部分前缀：带下划线的 `E_` 枚举不一定按 `PVPWeaponAttachmentSlot` 暴露，布尔 `bIsXxx` 暴露为 `is_xxx` |
| 正确做法 | 访问前用候选名解析：枚举按 `PVPWeaponAttachmentSlot` / `E_PVPWeaponAttachmentSlot` / `endswith` 扫描；布尔属性用 `is_default_option` 回退 `b_is_default_option`（`setup_pvp_weapon_assembly.py` 的 `set_property` / `slot_enum_type`） |
| 验证方式 | 脚本不再抛 `RuntimeError`，`catalog.slot_count = 8`、`catalog.loadout_count = 2` 为 PASS |

### 踩坑 12：SCS 父级在 `ParentComponentOrVariableName` 里读不到

| 要素 | 内容 |
| --- | --- |
| 现象 | 读回报告里 `ATT_FrontSight` 的 `parent` 为 `None`，但蓝图里父子关系正确 |
| 根因 | `USCS_Node::ParentComponentOrVariableName` 只记录**原生组件**父级；SCS 节点之间的父子关系只在节点树（`ChildNodes`）里，`AddChildNode` 不会写该字段 |
| 正确做法 | 读回时先取 `ParentComponentOrVariableName`，为空再用 `USimpleConstructionScript::FindParentNode(Node)` 取 SCS 父节点名 |
| 验证方式 | `report.md` 中 `ATT_FrontSight` / `ATT_ForeGrip` / `ATT_Muzzle` / `ATT_Laser` 的 `parent` 为 `ATT_Handguard` |

---

## 6. 校验、报告与冒烟

### 6.1 编译与生成

```text
Build.bat PVPDemoEditor Win64 Development -Project="<project>/PVPDemo.uproject" -WaitMutex -NoHotReload

UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/setup_pvp_weapon.py" ^
    -unattended -nosplash -nullrhi
```

期望：`Saved/PVPWeaponReport/report.md` 顶部 `result: PASS`，无 Errors / Warnings；关键检查项：

- `weapon_abp.compiled` / `weapon_abp.builder_slot` / `weapon_abp.wiring.*`
- `character_slot.ensured` / `character_slot.report_wired`
- `weapon_bp.mesh1p` / `anim1p` / `attach_socket` / `muzzle_socket` / 三组 Montage
- `map.pickup_count`（1） / `map.pickup_class`
- `character.locomotion_reference`

配件侧（`Saved/PVPAttachmentReport/report.md`）：

- `catalog.slot_count`（8） / `catalog.loadout_count`（2）
- `assembly.compiled` / `assembly.saved` / `assembly.component_names`
- `assembly.<ATT_Slot>.socket` / `.parent` / `.mesh` / `.tag` 逐项 PASS
- `runtime.<ATT_Slot>.socket` / `.parent` / `.mesh` 逐项 PASS（spawn 后实测）
- `weapon_bp.attachment_catalog` 指向 `DA_PVP_WeaponAttachmentCatalog`
- 归位报告 `reorg.md` 的 `verification failures: 0`

### 6.2 布局与框架校验

```text
python Content/Python/verify_pvp_asset_layout.py
UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/verify_pvp_game_framework.py" ^
    -unattended -nosplash -nullrhi
```

- 布局：`Actors` 20、`Animation/AnimBP` 5、`Data` 2、总数 81、旧路径零残留。
- 框架：`assets.zero_live_third_party_refs`（live=0）、`map.weapon_pickup_count=1`、`map.actor_count_conserved` 通过；`map.source_matches_baseline` 若失败，先确认是否为历史基线问题（源地图 hash 与 `Saved/PVPGameFrameworkReport/source_hashes.json` 不一致），不要把武器改动误判为第三方引用。

### 6.3 PIE 冒烟清单（手动）

> 配件 socket / 父级 / 网格已由 `setup_pvp_weapon_assembly.py` 的运行时冒烟自动断言（`report.md` 的 `runtime.*`）；下列步骤只需复核视觉与手感。

1. 打开 `L_PVP_Showcase`，PlayerStart 前方可见一把带护木、前后机瞄、弹匣的 AR。
2. 走近武器 → 自动拾取；角色手臂与武器同步播放 Equip Montage。
3. 前 / 后 / 左 / 右移动、跳跃、转身：武器始终跟随手部，不抖动、不脱手。
4. 观察枪口位置与准星关系；如偏位，调 `BP_PVP_Weapon_AR` 的 `AttachOffset`。
5. 通过蓝图或调试调用 `PlayFireMontage` / `PlayReloadMontage`，确认角色手臂与武器同步；弹壳 / 弹匣通知不生成属已知限制。
6. 丢弃（如接入调用 `DropWeapon`）后确认武器落在角色前方，冷却后可再次拾取。

> **编辑器世界冒烟（已通过）**：commandlet 加载 `L_PVP_Showcase` 后临时 spawn `BP_PVPBaseCharacter`，调用 `EquipmentComponent->EquipWeapon`、`PlayFireMontage` / `PlayReloadMontage`、`UnequipCurrentWeapon(true)`；武器成功附着到 `ik_hand_gun`、无 socket 警告、成对 Montage 无 anim instance 警告、卸下后 `IsEquipped=false`。该测试不保存地图；视觉对齐仍需 PIE 人工确认。

---

## 7. 已知限制与后续路线

- 无开火伤害、弹药、换弹状态机、后坐力、ADS。
- 无 3P 角色与 `Mesh3P` 可见性切换；`SetPerspective` 仅预留。
- 无网络复制与服务器权威装备状态。
- 无武器切换 / 多武器槽 / DataAsset 定义。
- Montage 内第三方通知不生成弹壳 / 弹匣（见踩坑 4）。
- 配件只有 1P 装配；`Mesh3P` 不带配件，3P 角色落地时再补。
- 装配用静态满弹匣；骨骼弹匣 `SK_PVP_AR_Magazine` 留待换弹动画阶段单独接 AnimClass。
- 配件切换只到"改网格 / 显隐"，没有配件 UI、数值加成与拾取式换装；换装靠重跑脚本或运行时调用接口。
- `UPVPEquipmentComponent::EndPlay` 仅清空当前武器指针，不做丢弃处理；角色销毁时武器可能仍挂在失效 Mesh 上，后续装备生命周期阶段再补。
- 拾取物位置为固定偏移，未做地面射线贴合。

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
- `Docs/Equipment/Equipment-Attachments.md`
- `Content/Python/pvp_logic_layout.py`
- `Content/Python/verify_pvp_asset_layout.py`
- `Content/Python/verify_pvp_game_framework.py`
- `Docs/Equipment/README.md`
- `Docs/Equipment/Equipment-Integration.md`
- `Saved/PVPWeaponReport/report.md`
