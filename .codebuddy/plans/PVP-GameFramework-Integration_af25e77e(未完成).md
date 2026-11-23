---
name: PVP-GameFramework-Integration
overview: 在 docs-first 规则下落地游戏框架层（Game 层 C++ + 编辑器重定向工具），生成 GameMode/Controller 蓝图，把既有 BP_PVPBaseCharacter 迁入 /Game/PVP，复制展示关卡为 L_PVP_Showcase 并通过全量引用重定向收敛为"零第三方依赖"，最后同步 Docs、README 与 RULE.mdc。
todos:
  - id: register-arch
    content: 用 [skill:UE5架构规划师] 在 Docs 与 RULE 中登记 Game 层、编辑器模块与 Content/PVP 资产根
    status: pending
  - id: cpp-game-layer
    content: 用 [skill:虚幻引擎Claude Code技能] 新增 Game 层 PVPGameMode 与 PVPPlayerController
    status: pending
    dependencies:
      - register-arch
  - id: editor-remap-module
    content: 新增 PVPDemoEditor 模块与 PVPAssetRemapLibrary，编译编辑器与运行时模块
    status: pending
    dependencies:
      - register-arch
  - id: asset-pipeline
    content: 用 [skill:UE Python Scripting] 写 setup 脚本：迁移蓝图、建两份蓝图、复制地图
    status: pending
    dependencies:
      - cpp-game-layer
      - editor-remap-module
  - id: map-rebuild
    content: 脚本重建地图 Actor、PlayerStart 与 GameMode 覆盖并保存
    status: pending
    dependencies:
      - asset-pipeline
  - id: zero-dependency-remap
    content: 跑收敛式全量重定向，清零自有资产与新地图的第三方引用
    status: pending
    dependencies:
      - map-rebuild
  - id: verify
    content: 写并运行 verify 脚本，从磁盘断言零依赖与地图状态并出报告
    status: pending
    dependencies:
      - zero-dependency-remap
  - id: docs-sync
    content: 同步 Docs、README、RULE.mdc 与 compare 脚本路径，按实测数量更新
    status: pending
    dependencies:
      - verify
---

## 用户需求

在现有 UE5 第一人称 PVP 工程中补齐"游戏框架层"：自行编写 C++ 游戏模式与玩家控制器，生成对应蓝图资产，把第三方展示关卡复制为工程自有地图并接入新框架；同时要求**复制出的地图改为引用工程内已迁移的资产副本，不得依赖第三方资源包**。

## 产品概述

以 Docs 中的分层与命名规范为准，新增"会话与玩家入口层"承载游戏模式与玩家控制器，产出可在内容目录中直接使用的蓝图；把展示关卡复制到工程自有地图目录，用自有资产重建场景内容并指定新的游戏模式与角色 Pawn，同时把工程内的美术副本与逻辑资产整理为互不依赖第三方包的自包含资产集合，最后同步全部相关文档与项目规则。

## 核心功能

- 新增游戏模式类：声明默认 Pawn 与玩家控制器，输出可核对的初始化与开场日志，为后续 GameState / PlayerState / 对局流程预留扩展位。
- 新增玩家控制器类：设定视角与输入模式默认值、记录实际附身的 Pawn、提供只读角色访问接口；本次不实现输入绑定，仅保留明确待办位。
- 生成蓝图：游戏模式蓝图（默认 Pawn 指向角色蓝图、玩家控制器指向控制器蓝图）与玩家控制器蓝图；同时把既有角色蓝图迁入独立逻辑资产目录并保留其原生父类。
- 资产归位：美术资产与逻辑资产分两个根存放，逻辑资产不再混入美术资产目录；迁移后旧路径不残留重定向器。
- 地图复制与重建：新地图独立于源关卡，保留原有场景布局与光照氛围；移除仅用于演示的第三方逻辑对象，墙体与灯具用自有美术资产按原位置朝向重建，出生点置于原演示角色处，地图内指定新的游戏模式。
- 自包含改造：把工程自有资产内部指向第三方包的引用全部改为指向自有副本，补齐缺失的接口、后处理动画与备用骨骼网格等资源副本，最终使自有美术根、逻辑根与新地图均不含任何第三方包引用。
- 验收与规范同步：独立脚本从磁盘重新加载并断言包路径、父类、默认 Pawn / 控制器、地图游戏模式、出生点数量与第三方引用计数，失败即非零退出并落盘报告；文档与规则按实测数量与真实状态更新。

## 视觉与体验效果

打开新地图直接 Play 即进入第一人称视角，玩家由新游戏模式生成的 `BP_PVPBaseCharacter` 接管，出生位置与朝向和原演示关卡一致；场景外观保持原有地面、墙面、立柱、轮胎道具、天空盒与施工灯氛围，移除仅用于演示说明的信息板与武器配置对象；关卡不再因第三方包缺失而出现引用警告或贴图丢失。

## 技术栈

- 引擎：UE 5.9（`D:/UE/UE_5.9/UE_5.9`，由注册表 EngineAssociation GUID 指向，与 `PVPDemo.uproject` 一致）。
- 运行时模块：`PVPDemo`（Type Runtime，导出宏 `PVPDEMO_API`）；本次新增 `Game/` 层，基类用 `AGameModeBase` / `APlayerController`，均在 Engine 模块，运行时 `Build.cs` 依赖不变。
- 编辑器模块：新增 `PVPDemoEditor`（Type Editor，LoadingPhase `PostEngineInit`），承载资产引用重定向工具；依赖 `Core / CoreUObject / Engine / UnrealEd / AssetTools / AssetRegistry / Projects`。
- 资产自动化：Unreal Python headless（`UnrealEditor-Cmd.exe -run=PythonScript -Script=... -unattended -nosplash -nullrhi`），沿用 `Content/Python/dump_tfa_character.py`、`compare_tfa_character.py` 的既有写法。
- 引用重定向：`ObjectTools::ForceReplaceReferences(TArrayView&lt;FReplaceRequest&gt;, TSet&lt;UObject*&gt;&amp;)`（`Engine/Source/Editor/UnrealEd/Public/ObjectTools.h`），Python 侧无对应 API，故必须由编辑器模块暴露 `UFUNCTION`。

## 现状核实

- `Source/PVPDemo/` 仅 `Character/PVPBaseCharacter.h|.cpp` 有实现；`Animation/ Camera/ Equipment/ GAS/ Input/` 为空目录；include 根为 `Source/`（`PVPDemo.Shared.rsp` 实证），故 `#include "PVPDemo/Game/PVPGameMode.h"` 与既有写法一致。
- `PVPDemo.uproject` 目前只有 1 个模块；`PVPDemoEditor.Target.cs` 已存在，可直接承载编辑器模块。
- `Content/PVPDemo/Characters/BP_PVPBaseCharacter.uasset` 父类为 `/Script/PVPDemo.PVPBaseCharacter`，必须复用不重建。
- 源地图非 World Partition、无 PlayerStart、无 GameMode 覆盖，内含 `BP_TFA_BaseCharacter` 实例与 35 条第三方引用（24 条美术有副本、10 条逻辑无副本）。
- `Content/PVPDemo` 中 357 个资产共 1531 条第三方引用，1514 条可由 `Saved/MigrationReports/migration_manifest.json`（576 条 TFA→PVP 映射）覆盖，8 个目标无副本。
- `Content/InfimaGames/` 为 git 忽略的只读第三方包；编辑器进程当前在运行，编译与命令行建资产前须先保存并关闭。

## 实现方案

### 1. 分层与类设计

- `Source/PVPDemo/Game/PVPGameMode.h|.cpp`：`APVPGameMode : AGameModeBase`，构造设 `PlayerControllerClass = APVPPlayerController::StaticClass()`、`DefaultPawnClass = APVPBaseCharacter::StaticClass()`、`bStartPlayersAsSpectators = false`；覆写 `InitGame` / `StartPlay` 仅 `Super` + 一条日志；不硬编码蓝图路径，蓝图层再覆盖为 `BP_PVPBaseCharacter`。
- `Source/PVPDemo/Game/PVPPlayerController.h|.cpp`：`APVPPlayerController : APlayerController`，构造设 `bShowMouseCursor = false`；`BeginPlay` 加 `FInputModeGameOnly` 与日志；`SetupInputComponent` 仅 `Super` + `TODO(Input)`；`OnPossess` 打印 Pawn 类名与包路径；`GetPVPCharacter()` 为 `BlueprintPure` 的 `Cast&lt;APVPBaseCharacter&gt;(GetPawn())`。
- 日志类别只在 `.cpp` 用 `DEFINE_LOG_CATEGORY_STATIC` 定义；头文件只放类声明，`APVPBaseCharacter` 用前置声明。
- 依赖方向 `Game → Character` 单向；同模块内属约定约束（UBT 无法编译期强制），在 Source-Structure 第 4 章显式标注。

### 2. 编辑器重定向工具

`Source/PVPDemoEditor/PVPAssetRemapLibrary.h|.cpp` 提供 `UPVPAssetRemapLibrary : UBlueprintFunctionLibrary`，暴露一个 `UFUNCTION(BlueprintCallable)` 重定向入口：

| 入参 | 含义 |
| --- | --- |
| `in_from_packages` | 旧包路径数组（第三方路径） |
| `in_to_packages` | 新包路径数组（自有副本路径），与前者一一对应 |
| `in_packages_to_scan` | 只在这些包路径前缀内替换（`/Game/PVPDemo`、`/Game/PVP`） |
| `in_report_path` | 报告输出路径 |


实现要点：先 `LoadPackage` 收集 `in_packages_to_scan` 内所有对象为 `ObjectsToReplaceWithin`，按映射构造 `TArray&lt;FReplaceRequest&gt;`（`Old` / `New` 对象），调用 `ObjectTools::ForceReplaceReferences(Requests, ObjectsToReplaceWithin)`，再 `MarkPackageDirty` + 保存（`UEditorLoadingAndSavingUtils::SavePackages`），最后写报告（替换条数、未命中映射、失败包）。工具只读第三方包，不写入 `Content/InfimaGames/`。

### 3. 零依赖收敛（fixpoint，最多 6 轮）

1. 种子映射 = `migration_manifest.json` 的 576 条 + 三个资源类副本（`BPI_PVP_AnimationState`、`ABP_PVP_Manny_PostProcess`、`SKM_PVP_Quinn`）+ 本次新建蓝图。
2. 每轮：调用重定向工具 → 磁盘扫描 `/Game/PVPDemo`、`/Game/PVP` 与新地图的 `/Game/InfimaGames/` 残留 → 对每个残留目标"复制资产 → 按 TFA→PVP 改名 → 按类型落根"加入映射 → 下一轮。
3. 落根规则：`Blueprint / AnimBlueprint / BlueprintInterface / DataAsset / World` → `/Game/PVP/&lt;模块&gt;`；`SkeletalMesh / StaticMesh / Texture / Material / MaterialInstance / Skeleton / PhysicsAsset / AnimSequence / AnimMontage / Niagara / Sound` → `/Game/PVPDemo/&lt;模块&gt;`；模块目录优先沿用 manifest 的 `target_folder`。
4. `BP_TFA_BaseCharacter` 的 4 处引用优先重定向到 `BP_PVPBaseCharacter`，随后编译 4 个动画通知蓝图；若编译失败则回退为"复制为 `/Game/PVP/Actors/BP_PVP_LegacyCharacter` 再重定向"，回退原因写入报告。
5. 终止条件：残留为 0 视为成功；一轮内无新增映射且仍有残留视为失败，输出报告并以非零退出，可从 git 回滚。

### 4. 资产与地图流水线（幂等）

1. 引用审计通过后将 `BP_PVPBaseCharacter` 迁至 `/Game/PVP/Characters/`，保存后重新加载校验，清理残留重定向器。
2. `BlueprintFactory` 创建 `BP_PVPGameMode`、`BP_PVPPlayerController`；对 `BP_PVPGameMode` 的 CDO 设 `default_pawn_class`（迁移后的蓝图生成类对象）与 `player_controller_class`，编译并保存。
3. `duplicate_asset` 复制地图为 `/Game/PVP/Maps/L_PVP_Showcase`；**不复制 `_BuiltData`**（工程 `r.AllowStaticLighting=False`）。
4. 加载新地图：缓存各 `BP_TFA_*` Actor 的世界变换后销毁；按坐标用 `StaticMeshActor` 重建墙体（`SM_PVP_Wall_Frame*`、`SM_PVP_Panel_Frame_Wall_*`）与道具，用 `SkeletalMeshActor` 重建施工灯（`SK_PVP_ConstructionLamp_Small`）；在原角色位置生成 `PlayerStart`；`WorldSettings.set_editor_property('default_game_mode', BP_PVPGameMode 类)`；`save_map`。
5. 报告落 `Saved/PVPGameFrameworkReport/report.md|json`。

### 5. 验收

`verify_pvp_game_framework.py` 新进程从磁盘断言，失败抛异常：

- `Content/PVP`、`Content/PVPDemo`、`L_PVP_Showcase.umap` 二进制中 `/Game/InfimaGames/` 计数均为 0（白名单为空）。
- `BP_PVPBaseCharacter` 位于 `/Game/PVP/Characters/` 且父类正确；旧路径不存在、无残留重定向器。
- `BP_PVPGameMode` 父类为 `APVPGameMode`，CDO 的 `default_pawn_class` 包路径在 `/Game/PVP/Characters/`、`player_controller_class` 在 `/Game/PVP/Game/`。
- 地图 WorldSettings GameMode 覆盖为 `BP_PVPGameMode_C`；`PlayerStart` 数量 ≥ 1；`BP_TFA_*` 实例数为 0。
- 报告落 `Saved/PVPGameFrameworkReport/verify.md|json`。

## 架构设计

- 模块：`PVPDemo`（Runtime，Game → Character 单向） + `PVPDemoEditor`（Editor，仅编辑器期重定向工具，不进入打包）。
- 资产链路：`L_PVP_Showcase` → `BP_PVPGameMode` → `BP_PVPPlayerController` + `BP_PVPBaseCharacter` → `/Script/PVPDemo.*`；美术资产全部落在 `/Game/PVPDemo`，逻辑资产全部落在 `/Game/PVP`。
- 反向依赖禁止：`/Game/PVPDemo` 与 `/Game/PVP` 不得再引用 `/Game/InfimaGames`；第三方包仅作开发期参考。

## 目录结构

```
PVPDemo/
├── Source/
│   ├── PVPDemo/
│   │   ├── Game/PVPGameMode.h|.cpp          # [NEW] 游戏模式：默认类、InitGame/StartPlay 日志，cpp 内定义日志类别
│   │   ├── Game/PVPPlayerController.h|.cpp  # [NEW] 玩家控制器骨架：GameOnly 输入模式、OnPossess 日志、GetPVPCharacter
│   │   ├── Character/PVPBaseCharacter.h|.cpp# [不改动] 既有 1P 角色
│   │   └── PVPDemo.Build.cs                 # [不改动] 无新增依赖
│   └── PVPDemoEditor/
│       ├── PVPDemoEditor.Build.cs           # [NEW] Editor 模块：UnrealEd/AssetTools/AssetRegistry
│       ├── PVPDemoEditor.h|.cpp             # [NEW] IMPLEMENT_GAME_MODULE(FDefaultGameModuleImpl, PVPDemoEditor)
│       └── PVPAssetRemapLibrary.h|.cpp      # [NEW] ForceReplaceReferences 批量重定向 + 报告
├── Content/
│   ├── Python/
│   │   ├── setup_pvp_game_framework.py      # [NEW] 迁移/建蓝图/复制地图/重定向收敛/重建 Actor，幂等
│   │   ├── verify_pvp_game_framework.py     # [NEW] 零依赖与地图状态断言，失败非零退出
│   │   └── compare_tfa_character.py         # [MODIFY] 派生蓝图路径改为 /Game/PVP/Characters
│   ├── PVP/
│   │   ├── Characters/BP_PVPBaseCharacter.uasset   # [MOVE] 由 Content/PVPDemo/Characters 迁入
│   │   ├── Game/BP_PVPGameMode.uasset              # [NEW] Pawn=BP_PVPBaseCharacter，Controller=BP_PVPPlayerController
│   │   ├── Game/BP_PVPPlayerController.uasset      # [NEW] 父类 APVPPlayerController
│   │   ├── Actors/                                 # [NEW] 收敛循环补齐的逻辑资产副本（BP_PVP_BaseWeapon、注射器系列、LegacyCharacter 等）
│   │   └── Maps/L_PVP_Showcase.umap                # [NEW] 源关卡副本，GameMode 覆盖 + PlayerStart + 自有资产重建
│   └── PVPDemo/                            # [MODIFY] 357 个资产引用改为指向自有副本；补 BPI/ABP/SKM 三个副本
├── Docs/Source-Structure.md                # [MODIFY] Game/ 层、PVPDemoEditor 模块、分层约定、两根关系
├── Docs/Content-Structure.md               # [MODIFY] Content/PVP 章节、前缀 BP_/L_、数量实测、§5.1 自包含结论、脚本登记
├── README.md                               # [MODIFY] 顶层目录补 Content/PVP、Source/PVPDemo/Game、Source/PVPDemoEditor
├── .codebuddy/rules/docs-first/RULE.mdc     # [MODIFY] §1.2 层清单、§2.1/§2.3 两根规则、§4 同步清单
└── PVPDemo.uproject                        # [MODIFY] 增加 PVPDemoEditor 模块条目
```

## 关键代码结构

```cpp
// Source/PVPDemo/Game/PVPGameMode.h
UCLASS(Blueprintable)
class PVPDEMO_API APVPGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    APVPGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void StartPlay() override;
};

// Source/PVPDemo/Game/PVPPlayerController.h
UCLASS(Blueprintable)
class PVPDEMO_API APVPPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    APVPPlayerController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    UFUNCTION(BlueprintPure, Category = "PVP|Pawn")
    APVPBaseCharacter* GetPVPCharacter() const;
};

// Source/PVPDemoEditor/PVPAssetRemapLibrary.h
UCLASS()
class UPVPAssetRemapLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "PVP|Tools")
    static bool RemapAssetReferences(const TArray&lt;FString&gt;& InFromPackages,
                                     const TArray&lt;FString&gt;& InToPackages,
                                     const TArray&lt;FString&gt;& InPackagesToScan,
                                     const FString& InReportPath);
};
```

## 执行注意事项

- 执行前置条件：请用户在编辑器中保存全部改动后关闭编辑器，确认 `UnrealEditor.exe` 退出，再执行 UBT 编译（`Build.bat PVPDemoEditor Win64 Development -Project=...`），否则 DLL 占用与资产双写。
- 顺序不可颠倒：先登记文档 → 编译运行时层与编辑器模块 → 迁移/创建蓝图 → 复制地图并重建 → 收敛重定向 → 校验 → 收尾同步文档。
- 体积与风险：重定向会重写 300 余个 `.uasset`，git diff 很大；建议执行前做基线提交或分支快照（需用户明确同意后才可 commit）；失败可从 git 回滚并保留报告。
- 性能：重定向为一次性离线批处理，按包批量加载与保存，避免逐资产重复打开；收敛轮次上限 6 轮防止无限循环。
- 保护：`Content/InfimaGames/` 只读；`DefaultEngine.ini` 不改；`BP_PVPBaseCharacter` 不重建；数量统计以脚本实测写入文档。

## Agent Extensions

### Skill

- **UE5架构规划师**
- 目的：确定 `Game/` 运行时层与 `PVPDemoEditor` 编辑器模块的归属、`Build.cs` 依赖最小集与 `Game → Character` 单向边界；按技能要求把"包路径迁移/资产重定向"作为升级人工处理项，产出引用审计结论。
- 预期结果：模块职责与依赖方向表、`PVPDemo.Build.cs` 无需改动的结论、编辑器模块依赖清单、迁移审计报告要点，写回 `Docs/Source-Structure.md`。
- **虚幻引擎Claude Code技能**
- 目的：约束新增 C++ 类符合 UE 编码与反射规范（`UCLASS` / `GENERATED_BODY`、include 顺序、日志类别只在 `.cpp`、构造内设 CDO 默认值、前置声明）。
- 预期结果：`PVPGameMode`、`PVPPlayerController`、`PVPAssetRemapLibrary` 共 6 个源文件通过 UBT 编译，零错误。
- **UE Python Scripting**
- 目的：编写 headless 幂等脚本，覆盖资产迁移、蓝图创建与 CDO 设值、地图复制与 Actor 重建、收敛式重定向调用与磁盘零依赖校验。
- 预期结果：`setup_pvp_game_framework.py` 与 `verify_pvp_game_framework.py` 无人值守执行成功，`Saved/PVPGameFrameworkReport/` 下报告全 PASS 且退出码为 0。