---
name: logic-asset-reorg-animbp
overview: 重排 Content/PVP 逻辑资产：新建 Animation（AnimBP/Notifies/Interfaces）与 Types 目录，归位 4 个动画蓝图、9 个动画通知、动画接口、4 个枚举 + 1 个结构体、2 个重复控件蓝图，并把第三方角色蓝图移入 Characters；同步修正 C++ 中指向第三方包的第一人称网格与动画蓝图默认路径，最后更新文档并执行校验。
todos:
  - id: layout-source-verify
    content: 用 [skill:UE Python Scripting] 新建 pvp_logic_layout.py 与 verify_pvp_asset_layout.py，定义路由、移动删除清单与校验规则
    status: completed
  - id: reorg-tool
    content: 编写 reorg_pvp_logic_assets.py：预检、备份、引用者校验、改名移动、清理重定向器与报告
    status: completed
    dependencies:
      - layout-source-verify
  - id: run-reorg
    content: 关闭编辑器后先 dry-run 再执行整理，核对报告与 77 个逻辑资产
    status: completed
    dependencies:
      - reorg-tool
  - id: framework-script
    content: 更新 setup_pvp_game_framework.py 的 EXPLICIT_COPIES 与路由函数，防止重跑回退到 Actors
    status: completed
    dependencies:
      - layout-source-verify
  - id: cpp-defaults
    content: 修正 PVPBaseCharacter.cpp/.h 第三方网格与动画蓝图路径，编译 PVPDemoEditor
    status: completed
    dependencies:
      - run-reorg
  - id: docs-sync
    content: 用 [skill:UE5架构规划师] 与 [subagent:code-explorer] 同步 Content-Structure、Animation/Character 三件套、Source-Structure、AGENTS.md
    status: completed
    dependencies:
      - run-reorg
      - framework-script
      - cpp-defaults
  - id: final-verify
    content: 运行资产布局校验、verify_doc_index.py 与 PIE 冒烟，确认全绿并输出文档同步说明
    status: completed
    dependencies:
      - docs-sync
---

## 用户需求

对 `Content/PVP/` 逻辑资产做一次全面归位检查，重点整治动画蓝图放置混乱的问题；不正确的资产重新安放，必要时单独开设目录承载动画逻辑资产。

## 产品概述

整理后，内容浏览器中的逻辑资产按"职责单一"原则归类：动画类资产集中到独立的动画目录，类型定义集中到类型目录，Actor 蓝图目录不再混入动画与 UI 资产，控件蓝图只保留唯一一份。原有玩法与引用关系保持不变，角色第一人称网格与动画蓝图默认值改为指向工程自有资产，不再依赖第三方包。

## 核心功能

- 新建动画逻辑目录：承接 4 个动画蓝图（第一人称角色、第三人称角色、角色后处理、施工灯）、9 个动画通知蓝图与 1 个动画状态蓝图接口。
- 新建类型目录：承接 4 个枚举与 1 个弹簧参数结构体。
- 第三方角色蓝图副本从 Actor 目录移入角色目录，与自有角色蓝图并列。
- 重复的 2 个控件蓝图合并为一份，只保留被实际引用的版本，多余副本删除。
- Actor 目录只保留 19 个 Actor 蓝图，动画、UI、类型资产不再出现在该目录。
- 移动后所有既有引用自动改指新路径，关卡、角色、动画蒙太奇、材质与网格均不受影响。
- 角色第一人称网格与动画蓝图默认值改为工程自有资产，删除第三方包后角色表现仍完整。
- 整理过程可试运行、可重复执行，并输出资产清单与引用核对报告。

## 技术栈选择

- 引擎：Unreal Engine 5.9（`PVPDemo.uproject`，EngineAssociation GUID `{A4291A6C-4F9B-B09A-08FC-608CB0ABBA16}`）。
- 资产整理：UE Editor Python（`unreal.EditorAssetLibrary` 的 `rename_asset` / `delete_asset` / `make_directory` / `save_asset`，配合 `AssetRegistry` 做引用者查询）。
- 校验：纯 Python 标准库（文件存在性、`.uasset` 路径字符串扫描），无需启动编辑器。
- 源码：C++（`Source/PVPDemo/Character/PVPBaseCharacter.cpp/.h`），UBT 编译 `PVPDemoEditor`。
- 执行方式：`UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript --script="Content/Python/xxx.py"`，执行前关闭编辑器。

## 实施方案

### 核心策略

以"数据驱动的移动清单 + 单一事实来源的路由表"替代手工拖拽：资产路径规则、22 条移动映射、2 条删除清单集中在一个纯 Python 模块中；UE 侧脚本只负责预检、执行与报告；纯 Python 校验脚本负责事后断言。这样避免"改一次、下次脚本重跑又搬回去"的回归。

### 现状确认为何必须改脚本（根因）

`Content/Python/setup_pvp_game_framework.py` 的 `logic_subfolder_for()` 对未登记类名一律返回 `'Actors'`，而 `AnimBlueprint`、动画通知蓝图（类名为 `Blueprint`）、`BlueprintInterface`、`UserDefinedEnum`、`UserDefinedStruct` 全部落到该兜底分支，这正是动画蓝图与通知堆在 `Actors/` 的根因。同时 `EXPLICIT_COPIES` 仍把 `BPI_PVP_AnimationState`、`ABP_PVP_Manny_PostProcess` 的目标硬编码在 `/Game/PVP/Characters/`，重跑会重新生成旧路径副本。两处必须同步修正，否则整理成果会被脚本回退。

### 目标布局与数量（核对基准）

- `Content/PVP/Animation/AnimBP/`：4 个（`ABP_PVP_FP_BaseCharacter`、`ABP_PVP_TP_BaseCharacter`、`ABP_PVP_Manny_PostProcess`、`Environment/ABP_PVP_ConstructionLamp_Large`）
- `Content/PVP/Animation/Interfaces/`：1 个（`BPI_PVP_AnimationState`）
- `Content/PVP/Animation/Notifies/`：9 个（`AN_PVP_*` 5 个、`ANS_PVP_*` 4 个）
- `Content/PVP/Types/`：5 个（4 个 `E_PVP_*` + `S_PVP_SpringData`）
- `Content/PVP/Characters/`：2 个（`BP_PVPBaseCharacter`、移入的 `BP_PVP_BaseCharacter`）
- `Content/PVP/Actors/`：19 个 Actor 蓝图
- `Content/PVP/UI/`：2 个（合并后唯一一份）
- 逻辑资产总数由 79 降为 77（删除 2 个无外部引用的重复控件蓝图）。

### 引用影响面（移动即触发自动改指）

- `ABP_PVP_FP_BaseCharacter` ← `BP_PVP_BaseCharacter`、`BP_PVPBaseCharacter`
- `ABP_PVP_TP_BaseCharacter`、`ABP_PVP_ConstructionLamp_Large` ← 各自宿主蓝图
- `ABP_PVP_Manny_PostProcess` ← 美术资产 `SKM_PVP_Manny`（`PostProcessAnimBlueprint`，美术→逻辑反向引用，属既有例外，文档需如实说明）
- 9 个通知 ← 30+ 动画蒙太奇与 `BP_PVP_BaseWeapon` 等
- 枚举/结构体 ← 两个角色动画蓝图、角色蓝图、武器蓝图、控件蓝图
- `BP_PVP_BaseCharacter` 移入 `Characters/` 后，引用者路径同步改指，类身份不变

### 性能与可靠性

- 移动 22 个资产会触发引用方包重存，其中 `SKM_PVP_Manny`（约 28MB）最重；一次性操作，可接受。校验脚本按文件单遍读取 + 子串匹配，避免逐个 `load_asset` 造成内存峰值。
- 删除重复控件蓝图前必须用 `AssetRegistry.get_referencers` 确认无外部引用；有意外引用者则中止而非强删。
- 所有删除目标先复制到 `Saved/AssetBackup/<timestamp>/` 并记录 SHA256，可回滚。

## 实施要点

1. **预检**：源资产存在、目标不存在（两个 WBP 目标因先删后移而豁免）、目标目录可创建；任一不符立即中止并输出原因。
2. **顺序**：先备份并删除 UI 下 2 个重复控件蓝图，再执行 22 条改名移动；改名使用 `EditorAssetLibrary.rename_asset`，由其完成引用 fixup。
3. **重定向器清理**：复用 `setup_pvp_game_framework.py` 中 `is_redirector()` 的做法（`load_asset(path).get_class().get_name() == 'ObjectRedirector'`），遍历旧目录删除残留重定向器，避免旧路径继续被解析。
4. **保存**：移动后对 `/Game/PVP` 与受影响美术包执行 `save_asset(..., only_if_is_dirty=False)`，并清理空目录。
5. **日志与报告**：统一前缀 `[PVP-REORG]`；只记录路径、类名、引用者数量，不打印资产内容；报告写入 `Saved/PVPAssetReorgReport/report.json|md`（含移动表、删除表、备份哈希、校验结果）。
6. **不做的事**：不改蓝图图表、不改动画蒙太奇内容、不触碰 `Content/InfimaGames/`、不新增或删除第三方武器动画蓝图。

## 架构设计

- **布局定义层**（纯 Python，无 `unreal` 依赖）：路由规则、移动/删除清单、期望数量，被三个入口共享，是唯一事实来源。
- **执行层**（UE 编辑器内）：读取布局定义 → 预检 → 备份/删除 → 移动 → 清理重定向器 → 保存 → 写报告。
- **校验层**（纯 Python，可脱离 UE）：断言目标文件存在、旧路径字符串零残留、目录数量匹配、关键引用已改指。
- **脚本回归防护**：`setup_pvp_game_framework.py` 改为调用共享路由函数，重跑不再把资产搬回 `Actors/`。

## 目录结构

```
PVPDemo/
├── Content/
│   ├── PVP/
│   │   ├── Animation/                                      # [NEW] 动画逻辑根
│   │   │   ├── AnimBP/                                     # [NEW] 4 个动画蓝图
│   │   │   │   └── Environment/                            # [NEW] 施工灯动画蓝图
│   │   │   ├── Interfaces/                                 # [NEW] BPI_PVP_AnimationState
│   │   │   └── Notifies/                                   # [NEW] 9 个 AN_/ANS_
│   │   ├── Types/                                          # [NEW] 4 个枚举 + 1 个结构体
│   │   ├── Actors/                                         # [MODIFY] 收敛为 19 个 Actor 蓝图
│   │   ├── Characters/                                     # [MODIFY] 2 个角色蓝图
│   │   └── UI/                                             # [MODIFY] 合并后 2 个控件蓝图
│   └── Python/
│       ├── pvp_logic_layout.py                             # [NEW] 布局单一事实来源：前缀路由规则、22 条移动、2 条删除、期望数量；纯 Python，供执行与校验脚本共用
│       ├── reorg_pvp_logic_assets.py                       # [NEW] UE 执行器：--dry-run/--apply，预检、备份、引用者校验、rename、重定向器清理、保存、写 Saved/PVPAssetReorgReport
│       ├── verify_pvp_asset_layout.py                      # [NEW] 纯 Python 校验：目标存在、旧路径零残留、数量匹配、关键引用改指；失败非零退出
│       └── setup_pvp_game_framework.py                     # [MODIFY] EXPLICIT_COPIES 两条目标改新路径；logic_subfolder_for/target_path_for 改为调用共享路由，消除 Actors 兜底回归
├── Source/PVPDemo/Character/
│   ├── PVPBaseCharacter.cpp                                # [MODIFY] 第 34-38 行网格与 ABP 路径改为自有资产与新 ABP 路径
│   └── PVPBaseCharacter.h                                  # [MODIFY] 第 17-31 行注释改指自有资产，去除 TFA 路径
├── Docs/
│   ├── Content-Structure.md                                # [MODIFY] 第 1 节目录树与数量（79→77）、第 2 节补 ABP_/WBP_/E_/S_ 前缀、第 3.2 节说明、第 4 节 Actors/Characters/UI 表格与新增 Animation/Types 行、第 5 节映射表、第 6.8 节脚本登记
│   ├── Animation/README.md                                 # [MODIFY] 3.1 接线状态、5 未实现能力：默认 ABP 已改自有路径，自研 AnimInstance 仍未接线
│   ├── Animation/Animation-Dev.md                          # [MODIFY] 47/128 行接线描述；新增踩坑：ABP 内 Cast 目标为 BP_PVP_BaseCharacter 而实际 Pawn 是 BP_PVPBaseCharacter
│   ├── Animation/Animation-Integration.md                  # [MODIFY] 53/115/116/188 行资产路径与接线状态
│   ├── Character/Character-Dev.md                          # [MODIFY] 54/190 行 BPI/ABP 所在目录
│   ├── Character/Character-Integration.md                  # [MODIFY] 121/124 行资产路径表
│   ├── Source-Structure.md                                 # [MODIFY] 110 行接线状态描述
│   └── ../AGENTS.md                                        # [MODIFY] Content/PVP 模块列表加 Animation/Types，文档地图登记 3 个新脚本，Animation 模块状态描述更新
└── Saved/
    ├── PVPAssetReorgReport/report.json|md                  # [NEW] 整理与校验报告
    └── AssetBackup/<timestamp>/                            # [NEW] 删除前备份
```

## 关键代码结构

```python
# Content/Python/pvp_logic_layout.py —— 三个入口共享的唯一事实来源
LOGIC_ROUTE_RULES = (
    ("ANS_", "Animation/Notifies"),
    ("AN_",  "Animation/Notifies"),
    ("ABP_", "Animation/AnimBP"),
    ("BPI_", "Animation/Interfaces"),
    ("WBP_", "UI"),
    ("E_",   "Types"),
    ("S_",   "Types"),
    ("BP_",  "Actors"),
)

MOVE_PLAN = (
    ("/Game/PVP/Actors/ABP_PVP_FP_BaseCharacter",       "/Game/PVP/Animation/AnimBP"),
    ("/Game/PVP/Actors/ABP_PVP_TP_BaseCharacter",       "/Game/PVP/Animation/AnimBP"),
    ("/Game/PVP/Characters/ABP_PVP_Manny_PostProcess",  "/Game/PVP/Animation/AnimBP"),
    ("/Game/PVP/Actors/ABP_PVP_ConstructionLamp_Large", "/Game/PVP/Animation/AnimBP/Environment"),
    ("/Game/PVP/Characters/BPI_PVP_AnimationState",     "/Game/PVP/Animation/Interfaces"),
    ("/Game/PVP/Actors/BP_PVP_BaseCharacter",           "/Game/PVP/Characters"),
    ("/Game/PVP/Actors/WBP_PVP_BodycamOverlay",         "/Game/PVP/UI"),
    ("/Game/PVP/Actors/WBP_PVP_Demo",                   "/Game/PVP/UI"),
)  # 另有 9 个通知与 5 个类型资产按前缀规则批量归位

DELETE_PLAN = (
    "/Game/PVP/UI/WBP_PVP_BodycamOverlay",
    "/Game/PVP/UI/WBP_PVP_Demo",
)
```

```cpp
// Source/PVPDemo/Character/PVPBaseCharacter.cpp 第 34-38 行
static const TCHAR* FirstPersonMeshPath =
    TEXT("/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple.SKM_PVP_FP_Manny_Simple");
static const TCHAR* FirstPersonAnimBlueprintPath =
    TEXT("/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter.ABP_PVP_FP_BaseCharacter_C");
```

## 已知限制（本轮记录不修）

- 两个角色动画蓝图内部引用（Cast/变量访问）指向 `BP_PVP_BaseCharacter`，而 GameMode 默认 Pawn 是 `BP_PVPBaseCharacter`（兄弟类而非子类），动画变量可能取不到值；改图属独立任务，本轮只写入 `Animation-Dev.md` 踩坑清单。
- `SKEL_PVP_Mannequin` 内仍残留 `/Game/InfimaGames` 字符串（悬空引用），在报告中标注，不属本次移动范围。
- 美术根 `SKM_PVP_Manny` 对后处理动画蓝图的反向引用为引擎属性固有形态，文档记录为既有例外。

## Agent Extensions

### Skill

- **UE Python Scripting**
- Purpose: 编写并调优 `pvp_logic_layout.py`、`reorg_pvp_logic_assets.py`、`verify_pvp_asset_layout.py`，确保 `EditorAssetLibrary` 改名、引用 fixup、重定向器清理与幂等处理符合 UE 5.9 API 规范。
- Expected outcome: 脚本可 `--dry-run` 试运行、可重复执行，产出结构化报告并零残留重定向器。
- **UE5架构规划师**
- Purpose: 校验 `Content/PVP` 目标目录划分（Animation/Types/Actors/Characters/UI）的职责边界与命名规范，确认无循环归属与未登记分层。
- Expected outcome: 产出可落地的目录职责表，目录划分与 `Docs/Content-Structure.md` 和项目硬约束一致。

### SubAgent

- **code-explorer**
- Purpose: 整理完成后在仓库范围内全量复查旧路径字符串（`/Game/PVP/Actors/ABP_*`、`/Game/PVP/Characters/BPI_*` 等）在文档、脚本、配置中的残留。
- Expected outcome: 输出残留清单，确保文档与脚本引用全部改指新路径。