---
name: PVP-GameFramework-Integration
overview: 按 docs-first 规则落地游戏框架层：新增 Source/PVPDemo/Game 层（GameMode + PlayerController）、在 /Game/PVP 逻辑资产根生成蓝图并迁移既有 BP_PVPBaseCharacter、复制关卡为 L_PVP_Showcase 并接入新 GameMode 与 Pawn，同步修订 Docs、README 与 RULE.mdc。
todos:
  - id: cpp-game-layer
    content: 新增 Game 层 PVPGameMode 与 PVPPlayerController，用 [skill:UE5架构规划师] 校验层边界与依赖
    status: pending
  - id: build-module
    content: 关闭编辑器后用 UBT 编译 PVPDemoEditor，确认新增类零错误
    status: pending
    dependencies:
      - cpp-game-layer
  - id: logic-asset-root
    content: 用 [skill:UE Python Scripting] 写 setup 脚本：迁移 BP 至 /Game/PVP、建两蓝图、复制地图
    status: pending
    dependencies:
      - build-module
  - id: map-wiring
    content: 脚本设置 GameMode 默认 Pawn/Controller、地图 GameMode 覆盖与 PlayerStart 替换
    status: pending
    dependencies:
      - logic-asset-root
  - id: verify-assets
    content: 写并运行 verify 脚本，从磁盘断言资产与地图状态并输出报告
    status: pending
    dependencies:
      - map-wiring
  - id: docs-rule-sync
    content: 更新 Docs、README 与 RULE.mdc，按实测数量登记新层、新根与新前缀
    status: pending
    dependencies:
      - verify-assets
---

## 产品概述

在既有 UE5 第一人称 PVP 工程中补齐"游戏框架层"：新增自有游戏模式类与玩家控制器类（C++ 实现），产出对应蓝图资产，把第三方展示关卡复制为工程自有地图并接入新框架，同时把逻辑资产（蓝图/地图）从美术资产根中分离到独立目录，最后同步修订文档与项目规则。

## 核心功能

- 新增游戏模式类：声明默认 Pawn 类与玩家控制器类，在对局初始化与开始阶段输出可核对日志，为后续 GameState/PlayerState/对局流程保留扩展位但本次不实现。
- 新增玩家控制器类：提供光标与输入模式默认值、附身时的 Pawn 类型日志、"获取当前角色"的只读接口；输入绑定本次不实现，仅留明确待办位。
- 生成蓝图：`BP_PVPGameMode`（默认 Pawn 指向 `BP_PVPBaseCharacter`、玩家控制器指向 `BP_PVPPlayerController`）与 `BP_PVPPlayerController`。
- 资产归位：把既有 `BP_PVPBaseCharacter` 迁入独立逻辑资产根的角色目录，美术资产根恢复为纯美术；迁移后旧路径不留重定向器，且迁移前置引用审计门禁。
- 复制关卡：源关卡原样保留，新地图独立落盘，WorldSettings 的游戏模式覆盖指向新蓝图；场景内原有第三方角色实例替换为 `PlayerStart`（位置/朝向与原实例一致），使新游戏模式按 `BP_PVPBaseCharacter` 生成并附身玩家，避免双 Pawn 重叠。
- 可验证性：独立脚本重新从磁盘加载资产，核对蓝图**包路径**与父类、默认 Pawn/控制器类、地图游戏模式、PlayerStart 数量、第三方角色残留数、旧包路径已清空，失败即以非零退出码结束并落盘报告。
- 规范同步：文档补充新源码层、新资产根、新命名前缀、脚本清单与真实数量；项目规则同步修订，使后续任务不再复现同类冲突。

## 视觉与体验效果

打开新地图直接 Play 即可进入第一人称视角，玩家由新游戏模式生成的 `BP_PVPBaseCharacter` 接管，出生点位于原演示关卡角色的位置与朝向；场景表现与源关卡一致，无关卡资产被修改。

## 技术栈

- 引擎：UE 5.9（`D:/UE/UE_5.9/UE_5.9`，由 `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds\{A4291A6C-...}` 指向，与 `.uproject` 的 EngineAssociation 一致）。
- 运行时：模块 `PVPDemo`，导出宏 `PVPDEMO_API`；基类使用 `AGameModeBase` / `APlayerController`（均在 Engine 模块内，`PVPDemo.Build.cs` 无需改动）。
- 包含路径：模块无 `Public/Private` 拆分，UBT 把 `Source/` 作为公共包含根（已由 `Intermediate/.../PVPDemo.Shared.rsp` 实证），因此新文件 include 写作 `"PVPDemo/Game/PVPGameMode.h"`，与现有 Character 层一致。
- 资产自动化：Unreal Python headless（`UnrealEditor-Cmd.exe -run=PythonScript -Script=<abs> -unattended -nosplash -nullrhi`），沿用 `Content/Python/dump_tfa_character.py`、`compare_tfa_character.py` 已验证的写法（`log()` + 绝对资产路径 + 报告落盘 + 失败抛异常）。

## 实现方案

**分层与资产根（先登记再落地）**

- 源码新增 `Source/PVPDemo/Game/`（会话与玩家入口层），依赖方向 `Game → Character`；`Character/` 不感知 `Game/`。本次不新增第三方依赖，`Build.cs` 保持不动，并在文档中明确记录"无新增依赖"。
- **分层为约定而非编译期强制**：两层同属一个模块，UBT 无法阻止 `Character/` 反向包含 `Game/` 头文件；按 skill 约束接受该风险，在 `Docs/Source-Structure.md` 第 4 章显式标注"分层靠 include 方向约束，成环时再抽薄公共模块"，本次不引入公共模块。
- 资产拆两根：`Content/PVPDemo/<模块>/` 只放美术表现资产；`Content/PVP/<模块>/` 放逻辑资产（`Game/` 蓝图、`Maps/` 地图、`Characters/` 角色蓝图）。既有 `Content/PVPDemo/Characters/BP_PVPBaseCharacter` 迁入 `Content/PVP/Characters/`，使 `Content/PVPDemo/Characters` 只保留 56 个美术资产。
- 引用方向约定：`Content/PVP` 的逻辑资产可引用 `Content/PVPDemo` 的美术资产，反之禁止（美术资产不得引用蓝图/地图）。

**类设计**

- `APVPGameMode : AGameModeBase`：构造只设 C++ 侧默认（`PlayerControllerClass=APVPPlayerController`、`DefaultPawnClass=APVPBaseCharacter`、`bStartPlayersAsSpectators=false`）；不硬编码任何蓝图路径，避免缺资产时构造失败。`InitGame` / `StartPlay` 仅 `Super` + 一条日志。
- `APVPPlayerController : APlayerController`：构造设 `bShowMouseCursor=false`；`BeginPlay` 加 `FInputModeGameOnly` 与日志；`SetupInputComponent` 只 `Super` + `TODO(Input)` 注释（不新建 IA/IMC）；`OnPossess` 记录实际 Pawn 类名与包路径；`GetPVPCharacter()` 为 `BlueprintPure` 的 `Cast<APVPBaseCharacter>(GetPawn())`。
- 日志类别：**仅在 `.cpp` 顶部** `DEFINE_LOG_CATEGORY_STATIC(LogPVPGameMode, Log, All)` / `DEFINE_LOG_CATEGORY_STATIC(LogPVPPlayerController, Log, All)`，头文件只放类声明，不放日志类别（避免重复定义）。
- 头文件边界：`PVPGameMode.h` 包含 `GameFramework/GameModeBase.h`（基类必需）；`PVPPlayerController.h` 前置声明 `class APVPBaseCharacter;`，`Cast<>` 与角色相关 include 放 `.cpp`。

**资产流水线（幂等）**

1. **迁移门禁（skill 的"包路径迁移升级人工处理"项）**：迁移 `BP_PVPBaseCharacter` 前先 `unreal.EditorAssetLibrary.find_package_referencers_for_asset` 做引用审计并把结果写入报告；若存在 `Content/PVP` 之外的引用 → **立即中止脚本并输出报告**，不做自动迁移，交由人工处理。审计通过才 `rename_asset` 到 `/Game/PVP/Characters/`。
2. 迁移后：`save_assets` → 重新 `load_asset` 二次校验（避免内存缓存仍指向旧包）→ 若旧路径残留 `ObjectRedirector` 则 `delete_asset` 清理，并断言旧包已不存在。
3. 复用而非重建该蓝图，校验父类为 `unreal.PVPBaseCharacter` 且 `get_path_name()` 前缀为 `/Game/PVP/Characters/`。
4. `BlueprintFactory` 创建 `BP_PVPGameMode`、`BP_PVPPlayerController`；对 `BP_PVPGameMode` 取 `unreal.get_default_object(bp.generated_class())` 设置 `default_pawn_class`（取迁移后的新类对象，不按类名查找）/ `player_controller_class`，随后 `compile_blueprint` + `save_asset(only_if_is_dirty=False)`。
5. `duplicate_asset` 复制 `/Game/InfimaGames/.../L_TFA_AR_Showcase` → `/Game/PVP/Maps/L_PVP_Showcase`，源资产只读不改。**不复制 `_BuiltData`**：地图内 `LevelBuildData` 引用保持指向第三方包（工程 `r.AllowStaticLighting=False`，无静态光照数据需求），避免产生引用了外部的孤立副本；该结论写入文档。
6. `load_map` 后：筛选 `WorldSettings` 设 `default_game_mode`；枚举 `BP_TFA_BaseCharacter_C` 实例，缓存首个实例的世界变换并 `spawn_actor_from_class(unreal.PlayerStart, loc, rot)`，销毁全部旧实例（已有 PlayerStart 则不新建）；`save_map` 落盘。
7. 报告写 `Saved/PVPGameFrameworkReport/report.md|json`（审计结果、创建/复用/迁移的资产、CleanUp 的 Actor 数）。

**校验脚本（新增 `verify_pvp_game_framework.py`）**

新进程从磁盘重新加载，断言失败即抛异常（非零退出）：

- `BP_PVPBaseCharacter` 存在且 `get_path_name()` 前缀为 `/Game/PVP/Characters/`；`/Game/PVPDemo/Characters/BP_PVPBaseCharacter` 不存在、无残留 redirector。
- `BP_PVPGameMode` 父类为 `APVPGameMode`；CDO `default_pawn_class` 的**包路径**为 `/Game/PVP/Characters/`、`player_controller_class` 包路径为 `/Game/PVP/Game/`。
- `L_PVP_Showcase` 位于 `/Game/PVP/Maps/`，WorldSettings `default_game_mode` == `BP_PVPGameMode_C`（含包路径）。
- 地图内 `PlayerStart` 数量 ≥ 1，`BP_TFA_BaseCharacter` 实例数 == 0。
- 报告写 `Saved/PVPGameFrameworkReport/verify.md|json`。

**规则与文档同步**

- `Docs/Source-Structure.md`：改写文首"仅有空目录"表述为分层状态；目录总览加 `Game/`；新增 `Game/` 层职责表；`Character/` 表改为已落地文件；第 4 章补依赖方向 + "分层仅约定、编译期不可强制"说明；第 5 章拆分为 `Content/PVPDemo/`（美术）与 `Content/PVP/`（逻辑）两根。
- `Docs/Content-Structure.md`：文首资产总数按实测改写；目录总览补 `Content/PVP/` 树并更新各模块数量；第 2 节前缀表增补 `BP_`、`L_`；新增 `Content/PVP/` 章节（Maps/Game/Characters）；第 4 章映射表加 `Game/` 行；第 5 章补"两套资产根""引用方向""新地图独立于源关卡、源包只读""不复制 `_BuiltData` 的原因""打包时需全量 Cook 或补 MapsToCook"；新增 Python 脚本登记清单（migrate/dump/compare/setup/verify）。
- `README.md`：顶层目录结构代码块补 `Content/PVP/` 与 `Source/PVPDemo/Game`。
- `.codebuddy/rules/docs-first/RULE.mdc`：§1.2 层清单加 `Game/`；§2.1/§2.3 改为"美术进 `Content/PVPDemo`、逻辑进 `Content/PVP`"；§4 同步清单加入 `Content/PVP` 与两套根的一致性核对。

## 架构审查结论与风险控制

| # | 风险 | 严重度 | 处理位置 |
| --- | --- | --- | --- |
| 1 | 复制 `_BuiltData` 会产生引用外部的孤立资产 | 高 | 资产流水线第 5 步：不复制，文档注明理由 |
| 2 | 包路径迁移属"升级人工处理"项 | 高 | 流水线第 1 步引用审计 + fail-fast，报告落盘 |
| 3 | 仅断言类名无法区分新旧包路径 | 中 | verify 断言 `get_path_name()` 前缀 |
| 4 | 日志类别误写进头文件 | 中 | 类设计：仅 `.cpp` 定义 |
| 5 | 编辑器内存缓存/未保存改动 | 中 | 执行前置条件 + rename 后 save/reload 二次校验 |
| 6 | 同模块分层无法编译期强制 | 低 | Source-Structure 第 4 章标注约定性质 |
| 7 | 打包可能漏新地图 | 低 | Content-Structure 第 5 章记录"后续打包注意事项"，本次不改 config |


## 实现说明（执行细节）

- **执行前置条件**：编辑器当前正在运行，需用户在编辑器内 `Ctrl+S` 保存全部改动后再关闭（未保存内容会在关闭时丢失）；关闭后确认 `UnrealEditor.exe` 进程退出。
- 构建命令：`D:/UE/UE_5.9/UE_5.9/Engine/Build/BatchFiles/Build.bat PVPDemoEditor Win64 Development -Project="D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -WaitMutex`。
- 建资产命令：`UnrealEditor-Cmd.exe "<uproject>" -run=PythonScript -Script="D:/UE/Unreal Projects/PVPDemo/Content/Python/setup_pvp_game_framework.py" -unattended -nosplash -nullrhi`；命中 `LogPython: Error` 或断言失败即回读日志定位。
- 顺序不可颠倒：先编译出原生类 → 再迁移/创建蓝图（蓝图父类是原生类）→ 最后写地图（地图引用蓝图生成类）。
- `compare_tfa_character.py` 中硬编码的 `'/Game/PVPDemo/Characters'` 必须同步改为 `'/Game/PVP/Characters'` 并更新 docstring，否则迁移后该对齐脚本失效。
- 双 Pawn 风险：不保留旧 `BP_TFA_BaseCharacter` 实例（其默认自动附身），统一改为 `PlayerStart` + 游戏模式生成 Pawn。
- 写操作只落在 `Content/PVP/`、`Saved/`、源码与文档；绝不修改 `Content/InfimaGames/`。数量统计以脚本实测结果写入文档，不沿用旧的 576。
- 性能与体量：一次性离线命令，无运行时热路径改动；仅新增 4 个源文件与 4 个资产，地图为单关卡复制（源地图非 World Partition，不产生外部 Actor 扩散）。

## 架构设计

- 运行时链路：`APVPGameMode`（规则/出生）→ `APVPPlayerController`（玩家入口）→ `APVPBaseCharacter`（角色与 1P 相机，已存在）。
- 资产链路：`L_PVP_Showcase` → `BP_PVPGameMode` → `BP_PVPPlayerController` + `BP_PVPBaseCharacter` → `/Script/PVPDemo.*` 原生类。
- 依赖方向：`Game → Character`，反向不成立（约定约束）；无循环依赖，`Build.cs` 不变。

## 目录结构

```
PVPDemo/
├── Source/PVPDemo/
│   ├── Game/
│   │   ├── PVPGameMode.h            # [NEW] APVPGameMode 声明：AGameModeBase 子类；构造设默认 Controller/Pawn 类；覆写 InitGame/StartPlay 仅 Super+日志；头文件不含日志类别
│   │   ├── PVPGameMode.cpp          # [NEW] 实现 + DEFINE_LOG_CATEGORY_STATIC；include 写成 "PVPDemo/Game/PVPGameMode.h"；不引用任何蓝图路径
│   │   ├── PVPPlayerController.h    # [NEW] APVPPlayerController 声明：构造、BeginPlay、SetupInputComponent、OnPossess、BlueprintPure GetPVPCharacter()；前置声明 APVPBaseCharacter
│   │   └── PVPPlayerController.cpp  # [NEW] 实现 + DEFINE_LOG_CATEGORY_STATIC；SetupInputComponent 只调 Super 并留 TODO(Input)；OnPossess 打印 Pawn 类名与包路径
│   ├── Character/PVPBaseCharacter.h|.cpp   # [不改动] 已有 1P 角色与 CameraFP 布局
│   └── PVPDemo.Build.cs                    # [不改动] 无新增依赖，需在文档中记录该结论
├── Content/
│   ├── Python/
│   │   ├── setup_pvp_game_framework.py     # [NEW] 幂等脚本：引用审计门禁 → 迁移 BP → 创建两蓝图 → 写 GameMode CDO 默认类 → 复制地图（不复制 BuiltData）→ 设 WorldSettings 覆盖 → PlayerStart 替换旧角色 → 保存并出报告
│   │   ├── verify_pvp_game_framework.py    # [NEW] 独立断言脚本：包路径、父类、CDO 类引用、地图游戏模式、PlayerStart 数量、旧角色残留数、旧包已清空；失败非零退出
│   │   └── compare_tfa_character.py        # [MODIFY] 派生蓝图路径改到 /Game/PVP/Characters，同步 docstring
│   ├── PVP/                                # [NEW] 逻辑资产根
│   │   ├── Characters/BP_PVPBaseCharacter.uasset  # [MOVE] 由 Content/PVPDemo/Characters 迁入，内容不改
│   │   ├── Game/BP_PVPGameMode.uasset             # [NEW] 父类 APVPGameMode；DefaultPawnClass=BP_PVPBaseCharacter、PlayerControllerClass=BP_PVPPlayerController
│   │   ├── Game/BP_PVPPlayerController.uasset     # [NEW] 父类 APVPPlayerController
│   │   └── Maps/L_PVP_Showcase.umap               # [NEW] 源关卡副本（不含 _BuiltData）；GameMode Override=BP_PVPGameMode；PlayerStart 替代 TFA 角色实例
│   └── PVPDemo/Characters/                 # [MODIFY] 迁移后仅剩 56 个美术资产
├── Docs/Source-Structure.md   # [MODIFY] 新增 Game/ 层、更新 Character/ 实际文件、文首状态、依赖方向与分层约定说明、Content 两根关系
├── Docs/Content-Structure.md  # [MODIFY] 新增 Content/PVP 章节、前缀表补 BP_/L_、更新数量与映射表、登记 Python 脚本、打包注意事项
├── README.md                  # [MODIFY] 顶层目录结构补 Content/PVP 与 Source/PVPDemo/Game
├── .codebuddy/rules/docs-first/RULE.mdc # [MODIFY] §1.2 层清单、§2.1/§2.3 资产根规则、§4 同步清单
└── Config/DefaultEngine.ini   # [不改动] 仅地图内设 GameMode Override
```

## 关键代码结构

```cpp
// Source/PVPDemo/Game/PVPGameMode.h
UCLASS(Blueprintable)
class PVPDEMO_API APVPGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	APVPGameMode();                                                   // 设 PlayerControllerClass / DefaultPawnClass(C++) / bStartPlayersAsSpectators
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;                                // 仅 Super + 一条日志
};

// Source/PVPDemo/Game/PVPPlayerController.h
UCLASS(Blueprintable)
class PVPDEMO_API APVPPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	APVPPlayerController();                                           // bShowMouseCursor=false 等查看默认值
	virtual void BeginPlay() override;                                // Super + GameOnly 输入模式 + 日志
	virtual void SetupInputComponent() override;                       // Super + TODO(Input) 占位，不绑定 Action
	virtual void OnPossess(APawn* InPawn) override;                   // Super + 打印实际 Pawn 类名与包路径
	UFUNCTION(BlueprintPure, Category = "PVP|Pawn")
	APVPBaseCharacter* GetPVPCharacter() const;                       // Cast<APVPBaseCharacter>(GetPawn())
};
```

## Agent Extensions

### Skill

- **UE5架构规划师**
- Purpose: 校验新增 `Game/` 层的归属、`Build.cs` 依赖边界与 `Game → Character` 单向依赖，确认不引入循环依赖；对包路径迁移执行"升级人工处理"，产出引用审计结论。
- Expected outcome: 层职责与依赖方向结论、`PVPDemo.Build.cs` 无需改动的结论、引用审计通过报告，写回 `Docs/Source-Structure.md`。
- **虚幻引擎Claude Code技能**
- Purpose: 保证 `PVPGameMode` / `PVPPlayerController` 符合 Epic 编码规范与反射约定（UCLASS/GENERATED_BODY、include 顺序、日志类别放置、构造内设 CDO 默认值）。
- Expected outcome: 新增 4 个源文件零编译错误通过 UBT `PVPDemoEditor Win64 Development` 构建。
- **UE Python Scripting**
- Purpose: 编写 headless 幂等资产脚本（引用审计、rename_asset 迁移并清理重定向器、BlueprintFactory 建蓝图、CDO 设默认类、duplicate_asset 复制关卡、WorldSettings 覆盖游戏模式、PlayerStart 替换 Actor）与独立校验脚本。
- Expected outcome: `Saved/PVPGameFrameworkReport/` 下产出通过报告，`verify_pvp_game_framework.py` 断言全部 PASS 且退出码为 0。