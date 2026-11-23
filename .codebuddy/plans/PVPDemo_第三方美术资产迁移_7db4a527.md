---
name: PVPDemo 第三方美术资产迁移
overview: 将第三方包 InfimaGames/TacticalFPSAnimations 中的美术表现类资产通过 UE 5.9 编辑器 Python 脚本复制并重命名为 PVP 前缀，按功能模块重组到 Content/PVPDemo/ 目录。
todos:
  - id: scan-assets
    content: 用 [skill:UE Python Scripting] 编写扫描脚本，枚举并分类包内资产，输出清单
    status: completed
  - id: mapping-rules
    content: 定义源目录到功能模块映射与 PVP 命名规则表，处理非 TFA 与 Manny 资产
    status: completed
    dependencies:
      - scan-assets
  - id: migration-script
    content: 实现幂等 Duplicate+Rename 迁移脚本，含事务、进度与冲突跳过
    status: completed
    dependencies:
      - scan-assets
      - mapping-rules
  - id: execute-migration
    content: 在 UE5.9 编辑器运行迁移脚本并生成迁移报告
    status: completed
    dependencies:
      - migration-script
  - id: verify-migration
    content: 校验目标资产数量、命名与原包完整性，修正异常项
    status: completed
    dependencies:
      - execute-migration
---

## 产品概述

把第三方 FPS 动画包中的美术表现类资产，规范迁移到工程自有的 `Content/PVPDemo/` 目录，按功能模块分层存放，并统一改为项目名前缀命名。属于纯资产整理，无界面视觉变更，原第三方包保持不动。

## 核心功能

- **资产筛选**：仅迁移贴图、静态/骨骼网格体、材质与材质实例、音频、骨骼动画与蒙太奇、特效、字体等美术表现类资产；排除蓝图、输入、枚举、数据表、地图等逻辑资产。
- **命名规范化**：将厂商前缀 `TFA` 替换为 `PVP`（如 `T_TFA_Weapon → T_PVP_Weapon`），非 `TFA` 资产补齐 `PVP` 前缀，保留各类型的官方类型前缀。
- **目录重组**：按 Characters、Weapons、Environment、Props、VFX、Audio 等功能模块建立子目录，武器相关材质/网格/动画/音频归入 Weapons 模块。
- **安全复制**：以复制而非移动方式迁移，保证资产被正确识别、引用不断裂，原始包完整保留。
- **可核验**：产出迁移清单，重复执行不产生重复资产。

## Tech Stack Selection

- 引擎：Unreal Engine 5.9（编辑器当前正在运行）
- 脚本：Unreal Python，使用 `EditorAssetLibrary`（枚举/复制/重命名/建目录）、`AssetRegistry`（仅读元数据做分类）、`EditorAssetSubsystem`（重命名）、`ScopedSlowTask`（进度）、`ScopedEditorTransaction`（撤销）
- 脚本载体：`Content/Python/migrate_pvp_assets.py`，通过编辑器 Output Log 的 `py` 命令直接执行，无需重启
- 清单产物：`Saved/MigrationReports/` 下的 JSON/CSV（源路径 → 目标路径 → 新名 → 类型）

## Implementation Approach

采用「扫描分类 → 映射命名 → 复制重命名 → 校验」四阶段流水线，全部规则由数据表驱动，便于审阅与回滚。

关键决策与取舍：

1. **用编辑器 API 复制+重命名，而非文件系统改名**：编辑器会同步注册包名与资产名，避免直接从磁盘改名造成的引用断裂；原包只读访问，天然保留。代价是需编辑器在运行（已满足）。
2. **白名单 + 黑名单双重过滤**：按类型前缀白名单纳入美术类（`T/SM/SK/SKM/M/MI/A/AM/AN/ANS/BS/PHYS/SKEL/NS/NE/T_/Font/PA/CR/IK/RTG/SC/SEF/TC/S` 等），按前缀与目录黑名单排除逻辑资产（`BP/ABP/WBP/BPI/IA/IMC/E/DA/DT/L` 及所有 `.umap`）。特别说明 `ABP` 是 AnimBlueprint，属蓝图，予以排除。
3. **规则表驱动命名**：`TFA → PVP` 直接替换；无 `TFA` 的资产（如 `M_Mannequin`、`CR_Mannequin_*`、`IK_Mannequin`、`PA_Mannequin`、`RTG_Mannequin`、`SM_Syringe`）在类型前缀后补 `PVP_`；`Manny_*` 与 Manny 贴图依据 AssetRegistry 提供的资产类名补正确类型前缀。检测目标同名冲突并记录跳过。
4. **功能模块映射表**：源目录映射到目标模块（Characters / Weapons / Environment / Props / VFX / Audio / Common / UI），Weapons 子目录保留 AssaultRifle 下的材质、网格、动画、音频结构；通用音频归 Audio，武器音频归 Weapons。
5. **幂等**：以目标资产存在性判断跳过，脚本可安全重复运行。

性能与可靠性：

- 约 577 个资产属批处理；过滤阶段只读 AssetRegistry 元数据，不逐个 `load_asset`，避免加载完整资产的高开销；执行阶段用 `ScopedSlowTask` 显示进度。
- 单个资产操作置于 `ScopedEditorTransaction` 中支持撤销；异常资产记录日志后继续处理，不中断整批。

避免技术债：不使用新的自定义框架，全部基于 UE 原生 Python API；规则集中在映射表，便于后续复用或扩展到其他包。

## Implementation Notes

- PythonScriptPlugin 为 UE5.9 内置插件；若 Output Log 中 `py` 命令不可用，先在插件管理器启用该插件。编辑器已运行，脚本执行无需重启。
- 引用说明：`duplicate + rename` 保证资产注册正确、引用不断裂；资产之间的内部引用默认仍指向原包（原包保留故安全）。如需让副本互相引用副本，可在校验后追加一次引用重定向步骤。
- 同名冲突：执行前扫描目标目录，已存在则跳过并写入报告，不覆盖。
- 原包保护：脚本对 `/Game/InfimaGames` 仅做列举与读取，禁止任何写操作。
- 影响面控制：不改动 `.uproject`、`Build.cs`、`Config/*.ini` 与任何 C++ 文件；不创建新蓝图或逻辑资产；脚本文件不产生 `.uasset`。

## Architecture Design

```text
源包扫描 (AssetRegistry 元数据)
        ↓ 白名单/黑名单分类
迁移清单 manifest (源→目标→新名→类型)
        ↓ 功能模块映射 + PVP 命名规则
复制重命名 (duplicate_asset → rename_asset，事务+进度)
        ↓
校验报告 (数量/命名/原包完整性/冲突记录)
```

## Directory Structure

脚本与目标目录结构如下；所有目标子目录由脚本按需幂等创建。

```
d:/UE/Unreal Projects/PVPDemo/
├── Content/Python/
│   └── migrate_pvp_assets.py            # [NEW] 迁移主脚本：扫描枚举、美术类过滤、源目录到功能模块映射、TFA→PVP 命名、非 TFA/Manny 资产补前缀、duplicate+rename、事务与进度、冲突跳过、生成报告；对源包只读
├── Saved/MigrationReports/
│   ├── migration_manifest.json          # [NEW-产物] 迁移清单，记录源路径/目标路径/新名/资产类型
│   └── migration_report.json            # [NEW-产物] 执行结果：成功数、跳过数、失败项、冲突项
└── Content/PVPDemo/                     # [目标根] 迁移后的工程自有资产（均由脚本生成）
    ├── Characters/                      # 角色：Mannequins 材质/网格/骨架/姿势/贴图 + 通用角色动画
    │   ├── Mannequins/{Materials,Meshes,Rigs,Textures,Animations}
    │   └── Animations/
    ├── Weapons/                         # 武器：保留 AssaultRifle 细分结构
    │   └── AssaultRifle/
    │       ├── {Materials,Meshes,Textures}
    │       ├── Animations/Character/{FP,TP}/{Combat,Interactions,Locomotion,Poses,Transitions}
    │       ├── Animations/Weapon/{Common,FP,TP}
    │       └── Audio/{Firing,Handling/{Bolt,Foley,Magazine},Impacts}
    ├── Environment/{Materials,Meshes,Textures,Animations}
    ├── Props/{Materials,Meshes,Textures,Animations}
    ├── VFX/{Materials,Systems,Emitters,Textures}
    ├── Audio/{Common/{Foley,Heal,Melee},Classes}
    ├── Common/Materials/
    └── UI/Fonts/
```

## Agent Extensions

### Skill

- **UE Python Scripting**
- Purpose: 提供 UE5.x Python API（EditorAssetLibrary / AssetRegistry / EditorAssetSubsystem / ScopedSlowTask）的准确用法与批处理脚本模板，用于扫描、过滤、复制、重命名资产。
- Expected outcome: 产出可直接在 UE5.9 编辑器运行的迁移脚本，正确完成约 577 个美术资产的 duplicate + rename，并生成可核验的迁移清单与报告。