# PVP 装备模块开发文档

> 维度：装备（武器 Actor / 拾取附着 / 成对 Montage）
> 模块：Equipment
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-23
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
| `Equipment/PVPWeaponActor.h/.cpp` | `APVPWeaponActor`：`SceneRoot` + `Mesh1P` / `Mesh3P` + `PickupSphere`；Actor 级配置在 `OnConstruction` 应用；自动拾取、附着 / 卸下 / 丢弃、成对 Montage、枪口查询、1P/3P 切换 |
| `Equipment/PVPEquipmentComponent.h/.cpp` | `UPVPEquipmentComponent`：`CurrentWeapon`、"一次一把"、`EquipWeapon` / `UnequipCurrentWeapon` / `DropCurrentWeapon`；只依赖 `ACharacter` 与武器 Actor |
| `Character/PVPBaseCharacter.h/.cpp` | 新增 `EquipmentComponent` 属性与构造期 `CreateDefaultSubobject`；不改现有 Mesh / CameraFP / 输入 / 走跑逻辑 |
| `PVPDemoEditor/PVPAnimBlueprintBuilder.h/.cpp` | `BuildLocomotionAnimBlueprint` 增加 `DefaultSlot`；新增 `BuildWeaponAnimBlueprint` 与 `EnsureMontageSlot` |

运行时模块 `PVPDemo.Build.cs` 无新增依赖（骨骼网格、AnimInstance、SphereComponent、AnimMontage 均属 `Engine`）。编辑器模块沿用现有 `AnimGraph` / `AnimGraphRuntime` / `BlueprintGraph` / `AssetTools`。

### 2.2 资产

| 资产 | 路径 | 说明 |
| --- | --- | --- |
| `BP_PVP_Weapon_AR` | `/Game/PVP/Actors/` | 父类 `APVPWeaponActor`；配置 `SK_PVP_AR`、`ABP_PVP_FP_Weapon_AR_C`、`ik_hand_gun`、`SOCKET_Muzzle`、拾取半径与三组 Montage |
| `ABP_PVP_FP_Weapon_AR` | `/Game/PVP/Animation/AnimBP/` | 父类 `UAnimInstance`；`SequencePlayer(A_PVP_WEP_AR_Reference)` → `DefaultSlot` → `Root` |
| `ABP_PVP_FP_Locomotion` | `/Game/PVP/Animation/AnimBP/` | 原地在 StateMachine 与 Root 之间插入 `DefaultSlot`；未删除重建 |
| 地图拾取物 | `/Game/PVP/Maps/L_PVP_Showcase` | 标签与 Actor Label 均为 `PVP_WeaponPickup_AR`；PlayerStart 前方约 200cm、向下 80cm |

### 2.3 脚本与报告

| 入口 | 作用 |
| --- | --- |
| `Content/Python/setup_pvp_weapon.py` | 校验依赖 → 重建武器 ABP → 原地确保角色 ABP Slot → 复用 / 创建武器 BP 并设 CDO → 放置 / 更新地图拾取物 → 读回并写报告 |
| `Saved/PVPWeaponReport/report.{json,md}` | Python 汇总报告（检查项、BP 默认值、地图实例、C++ 构建器报告） |
| `Saved/PVPWeaponReport/weapon_abp_builder.json` | `BuildWeaponAnimBlueprint` 的 C++ 报告 |
| `Saved/PVPWeaponReport/slot_builder.json` | `EnsureMontageSlot` 的 C++ 报告 |
| `Content/Python/pvp_logic_layout.py` | 资产清单与数量：`Actors` 20、`Animation/AnimBP` 5、总数 80 |
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

### 6.2 布局与框架校验

```text
python Content/Python/verify_pvp_asset_layout.py
UnrealEditor-Cmd.exe "<project>/PVPDemo.uproject" -run=PythonScript ^
    -Script="<project>/Content/Python/verify_pvp_game_framework.py" ^
    -unattended -nosplash -nullrhi
```

- 布局：`Actors` 20、`Animation/AnimBP` 5、总数 80、旧路径零残留。
- 框架：`assets.zero_live_third_party_refs`（live=0）、`map.weapon_pickup_count=1`、`map.actor_count_conserved` 通过；`map.source_matches_baseline` 若失败，先确认是否为历史基线问题（源地图 hash 与 `Saved/PVPGameFrameworkReport/source_hashes.json` 不一致），不要把武器改动误判为第三方引用。

### 6.3 PIE 冒烟清单（手动）

1. 打开 `L_PVP_Showcase`，PlayerStart 前方可见一把 AR。
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
- `UPVPEquipmentComponent::EndPlay` 仅清空当前武器指针，不做丢弃处理；角色销毁时武器可能仍挂在失效 Mesh 上，后续装备生命周期阶段再补。
- 拾取物位置为固定偏移，未做地面射线贴合。

---

## 8. 相关文件

- `Source/PVPDemo/Equipment/PVPWeaponActor.h/.cpp`
- `Source/PVPDemo/Equipment/PVPEquipmentComponent.h/.cpp`
- `Source/PVPDemo/Character/PVPBaseCharacter.h/.cpp`
- `Source/PVPDemoEditor/PVPAnimBlueprintBuilder.h/.cpp`
- `Content/Python/setup_pvp_weapon.py`
- `Content/Python/pvp_logic_layout.py`
- `Content/Python/verify_pvp_asset_layout.py`
- `Content/Python/verify_pvp_game_framework.py`
- `Docs/Equipment/README.md`
- `Docs/Equipment/Equipment-Integration.md`
- `Saved/PVPWeaponReport/report.md`
