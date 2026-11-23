---
name: PVP-GameFramework-Integration
overview: 新增 C++ 游戏模式与玩家控制器类，生成对应蓝图，复制 L_TFA_AR_Showcase 为新地图并接入新 GameMode 与 BP_PVPBaseCharacter 作为 Pawn，最后同步更新 Docs 文档。
todos:
  - id: cpp-game-framework
    content: 新增 Game/PVPGameMode 与 Game/PVPPlayerController，实现默认类、日志与 Pawn 只读接口
    status: pending
  - id: build-module
    content: 关闭编辑器后用 UBT 编译 PVPDemoEditor，确认新增类零编译错误
    status: pending
    dependencies:
      - cpp-game-framework
  - id: python-setup-assets
    content: 编写 setup_pvp_game_framework.py：创建 GameMode/Controller 蓝图并复制地图为 L_PVP_Showcase
    status: pending
    dependencies:
      - build-module
  - id: map-wiring
    content: 脚本内设 GameMode 默认 Pawn/Controller、地图 GameMode 覆盖，PlayerStart 替换 TFA 角色并保存
    status: pending
    dependencies:
      - python-setup-assets
  - id: verify-assets
    content: 编写并运行 verify_pvp_game_framework.py，从磁盘断言资产与地图状态并出报告
    status: pending
    dependencies:
      - map-wiring
  - id: update-docs
    content: 更新 Docs/Source-Structure.md、Docs/Content-Structure.md 与 README 的模块、状态与资产数量
    status: pending
    dependencies:
      - verify-assets
---

## Product Overview

在既有 UE 第一人称 PVP 工程中补齐"游戏框架层"：新增自有 C++ 游戏模式与玩家控制器类，产出对应蓝图资产，并把第三方展示关卡复制为工程自有地图，接入新的游戏模式与角色 Pawn，最后同步更新工程文档。

## Core Features

- 新增 `APVPGameMode`（基于 `AGameModeBase`）：在 C++ 中声明默认 Pawn 类、玩家控制器类，并在对局初始化与开始阶段输出可核对的日志。
- 新增 `APVPPlayerController`（基于 `APlayerController`）：提供视角/输入模式等骨架能力，暴露"获取当前 PVP 角色"的只读接口；输入绑定本次不实现，仅留明确待办位。
- 在内容目录生成两个蓝图：`BP_PVPGameMode`、`BP_PVPPlayerController`，前者默认 Pawn 指向已存在的 `BP_PVPBaseCharacter`、控制器指向 `BP_PVPPlayerController`。
- 复制关卡资产生成新地图 `L_PVP_Showcase`（独立于源关卡，源关卡不被修改），并在该地图内完成：WorldSettings 的游戏模式覆盖替换为新蓝图、场景内原有第三方角色实例替换为 `PlayerStart`（位置/朝向与原角色一致），使游戏模式按 `BP_PVPBaseCharacter` 生成并附身玩家。
- 保持可验证性：由独立脚本重新从磁盘加载资产，核对"地图游戏模式、默认 Pawn、默认控制器、PlayerStart 数量、第三方角色是否已清除"，输出报告。
- 更新 `Docs/Source-Structure.md` 与 `Docs/Content-Structure.md`，反映新增的框架层源码、蓝图与地图模块，以及目录/资产数量的真实状态。

## 技术栈

- 引擎：UE 5.9（`D:/UE/UE_5.9/UE_5.9`，由 `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds\{A4291A6C-...}` 指向，与 `PVPDemo.uproject` 的 EngineAssociation 一致）。
- 运行时模块：`PVPDemo`（导出宏 `PVPDEMO_API`），UE C++（UCLASS 反射）+ UBT 构建。
- 资产自动化：Unreal Python（`UnrealEditor-Cmd.exe -run=PythonScript -Script=...`），延续 `Content/Python/dump_tfa_character.py`、`compare_tfa_character.py` 已验证的 headless 工作流。
- 不新增第三方依赖：`AGameModeBase`、`APlayerController` 均在 Engine 模块内，`PVPDemo.Build.cs` 现有依赖已足够，**无需修改**。

## 现状确认（已核实）

- `Source/PVPDemo/` 目前只有 `Character/PVPBaseCharacter.h|.cpp`；`Camera/`、`Animation/`、`Equipment/`、`GAS/`、`Input/` 为空目录。模块已编译过（`Binaries/Win64/UnrealEditor-PVPDemo.dll`）。
- `Content/PVPDemo/Characters/BP_PVPBaseCharacter.uasset` 已存在，父类为 `/Script/PVPDemo.PVPBaseCharacter`，**必须复用，不得重建**。
- 源地图 `/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase`（另有 `_BuiltData`）：非 World Partition、无 PlayerStart、WorldSettings 无 GameMode Override、内含 `BP_TFA_BaseCharacter` 放置实例。
- `Content/InfimaGames/` 为 git 忽略的第三方包，脚本只读不写；`Content/__ExternalActors__/PVPDemo/Maps/TestMap/` 为历史 World Partition 遗留，本次不依赖。
- 编辑器进程正在运行，**编译与命令行建资产前必须先关闭**。

## 实现方案

### 1. C++ 框架层（新增 `Source/PVPDemo/Game/`）

用一个新层承载"会话与玩家控制入口"，与现有"角色层"解耦，符合文档既有的"按职责分层"约定：

- `APVPGameMode : AGameModeBase`
- 构造函数声明 CDO 默认：`PlayerControllerClass = APVPPlayerController::StaticClass()`、`DefaultPawnClass = APVPBaseCharacter::StaticClass()`、`bStartPlayersAsSpectators = false`。
- 覆写 `InitGame()` / `StartPlay()` 仅做 `Super` 调用 + 一次性日志（证明网络/单机入口走的是本类），不引入对局状态机等未要求内容。
- **不在 C++ 中硬编码蓝图路径**：把 `DefaultPawnClass` 指向 `BP_PVPBaseCharacter` 的职责交给 `BP_PVPGameMode` 的 CDO，避免 C++ 依赖蓝图资产（缺失即构造报错）。
- `APVPPlayerController : APlayerController`
- 构造函数设定查看/光标默认值（不显示鼠标、不绑定点击事件）。
- `BeginPlay()`：`Super` + 日志 + `SetInputMode(FInputModeGameOnly())`，保证后续接入输入时处于正确输入模式。
- `SetupInputComponent()`：仅 `Super` + `// TODO(Input): 由 Source/PVPDemo/Input/ 层统一绑定 Enhanced Input`，不新建 IA/IMC，不绑定任何 Action。
- `OnPossess(APawn*)`：`Super` + 日志（打印实际 Pawn 类名，便于核对是 `BP_PVPBaseCharacter_C`）。
- `GetPVPCharacter()`：`BlueprintPure`，`Cast<APVPBaseCharacter>(GetPawn())`，供蓝图/后续相机层只读访问。
- 日志使用文件内 `DEFINE_LOG_CATEGORY_STATIC`，仅入口各打一条，避免刷屏。

### 2. 资产生成脚本（新增 `Content/Python/setup_pvp_game_framework.py`）

按现有脚本风格（`log()` + 绝对资产路径常量 + 报告落盘 `Saved/`），**幂等**执行：

1. 复用而非重建 `BP_PVPBaseCharacter`：`load_asset` 命中则校验其父类为 `APVPBaseCharacter`（`unreal.PVPBaseCharacter`）。
2. 创建蓝图：`unreal.AssetToolsHelpers.get_asset_tools().create_asset(...)` + `unreal.BlueprintFactory`（`parent_class` 设为对应 C++ 类），路径 `/Game/PVPDemo/Game/BP_PVPGameMode`、`/Game/PVPDemo/Game/BP_PVPPlayerController`。
3. 设置 `BP_PVPGameMode` 的类默认值：取 `unreal.get_default_object(bp.generated_class())`，`set_editor_property('default_pawn_class', BP_PVPBaseCharacter 生成类)`、`set_editor_property('player_controller_class', BP_PVPPlayerController 生成类)`，随后 `unreal.BlueprintEditorLibrary.compile_blueprint` + `save_asset`。
4. 复制地图：`/Game/PVPDemo/Maps/L_PVP_Showcase`；优先 `unreal.EditorLoadingAndSavingUtils.duplicate_map(src, dst)`，不可用时回退 `unreal.EditorAssetLibrary.duplicate_asset(src, dst)`；源包若存在 `_BuiltData` 一并复制为 `L_PVP_Showcase_BuiltData`（静态光照已关闭，缺失不阻塞，但保持包完整）。
5. 加载新地图并改造：

- 找到 `WorldSettings`（`get_all_level_actors()` 中筛类型），`set_editor_property('default_game_mode', bp_game_mode 生成类)`。
- 枚举全部 `BP_TFA_BaseCharacter_C` 实例；对首个实例记录世界变换，按该变换生成 `unreal.PlayerStart`（朝向沿用原 Actor），并 `destroy_actor` 删除所有第三方角色实例；若地图已有 `PlayerStart` 则不再新建。避免"手放 Pawn + GameMode 再生成"导致的双 Pawn/AutoPossess 冲突。
- `unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)` 落盘。

6. 输出 `Saved/PVPGameFrameworkReport/report.md|json`（创建/复用了哪些资产、应用到哪张地图、替换了几个 Actor）。

### 3. 独立校验脚本（新增 `Content/Python/verify_pvp_game_framework.py`）

新进程从磁盘重新加载，断言并以非零退出码失败（沿用 `compare_tfa_character.py` 的失败策略）：

- `BP_PVPGameMode` 父类为 `APVPGameMode`，CDO 的 `default_pawn_class` 类名 == `BP_PVPBaseCharacter_C`，`player_controller_class` 类名 == `BP_PVPPlayerController_C`。
- `L_PVP_Showcase` 的 WorldSettings `default_game_mode` == `BP_PVPGameMode_C`。
- 地图中 `PlayerStart` 数量 ≥ 1，`BP_TFA_BaseCharacter` 实例数量 == 0。
- 报告写 `Saved/PVPGameFrameworkReport/verify.md|json`。

### 4. 文档同步

- `Docs/Source-Structure.md`：去掉"仅为空目录"表述，改为分层状态表；目录总览加 `Game/`；新增 `Game/` 层职责表（`PVPGameMode` / `PVPPlayerController`）；把 `Character/` 表格更新为已落地文件；同步第 3、4、5 章（命名/依赖/与 Content 关系）。
- `Docs/Content-Structure.md`：目录总览加 `Maps/`、`Game/`，`Characters/` 补充 `BP_PVPBaseCharacter`；新增 `Maps/` 与 `Game/` 小节；更新资产总数与 `Characters/` 数量（按脚本统计 `Content/PVPDemo` 下 `.uasset/.umap` 实数写入，不沿用旧的 576）；第 4 章映射表新增 `Maps/`、`Game/` 行；第 5 章补充"新地图独立于源地图、源包只读"的约定。
- `README.md`：同步目录树中的 `Content/PVPDemo`（加 Maps、Game）与 `Source/PVPDemo`（加 Game）模块列表，避免文档漂移。

## 架构与依赖方向

`GameMode`（规则/出生）→ `PlayerController`（玩家入口）→ `Character`（角色实体与 1P 相机）；`Game/` 依赖 `Character/`，反向不成立，符合既有"输入 → 权威状态 → 表现"的单向数据流。资产侧：`BP_PVPGameMode` → `BP_PVPPlayerController` + `BP_PVPBaseCharacter`；`L_PVP_Showcase` → `BP_PVPGameMode`。无新增编译依赖，无循环依赖。

## 目录结构

```
PVPDemo/
├── Source/PVPDemo/
│   └── Game/
│       ├── PVPGameMode.h          # [NEW] APVPGameMode 声明：AGameModeBase 子类，CDO 设默认 PlayerController/Pawn 类，覆写 InitGame/StartPlay 做日志；文件内声明日志类别。需保留扩展位（GameState/PlayerState/对局流程）但本次不实现。
│       ├── PVPGameMode.cpp        # [NEW] 实现构造与两个覆写，include 写成 "PVPDemo/Game/PVPGameMode.h"，与现有 Character 层一致；不引用任何蓝图路径。
│       ├── PVPPlayerController.h  # [NEW] APVPPlayerController 声明：构造、BeginPlay、SetupInputComponent、OnPossess、BlueprintPure GetPVPCharacter()；前置声明 APVPBaseCharacter。
│       └── PVPPlayerController.cpp# [NEW] 实现上述方法；SetupInputComponent 只调 Super 并留 TODO(Input) 注释；GetPVPCharacter 用 Cast；日志每入口一条。
├── Content/Python/
│   ├── setup_pvp_game_framework.py    # [NEW] 幂等资产生成/改造脚本：复用 BP_PVPBaseCharacter、创建 BP_PVPGameMode 与 BP_PVPPlayerController、写 CDO 类默认值、复制地图、设 WorldSettings 游戏模式、以 PlayerStart 替换 TFA 角色实例、保存并出报告。使用 AssetTools/BlueprintFactory/CDO set_editor_property/EditorLoadingAndSavingUtils。
│   └── verify_pvp_game_framework.py   # [NEW] 独立断言脚本：重新加载资产核对父类、CDO 类引用、地图游戏模式、PlayerStart 数量、残留 TFA 角色数；失败抛异常返回非零。
├── Content/PVPDemo/
│   ├── Characters/BP_PVPBaseCharacter.uasset  # [REUSE] 已存在且父类为本工程 APVPBaseCharacter，只校验父类并编译保存，禁止重建。
│   ├── Game/BP_PVPGameMode.uasset             # [NEW] 父类 APVPGameMode；CDO DefaultPawnClass=BP_PVPBaseCharacter、PlayerControllerClass=BP_PVPPlayerController。
│   ├── Game/BP_PVPPlayerController.uasset     # [NEW] 父类 APVPPlayerController，无额外逻辑。
│   └── Maps/L_PVP_Showcase.umap                # [NEW] 源地图副本；WorldSettings GameMode Override=BP_PVPGameMode；以 PlayerStart 替代原 BP_TFA_BaseCharacter 实例（保留位置与朝向）。
├── Docs/Source-Structure.md   # [MODIFY] 更新分层现状、新增 Game/ 层、Character/ 实际文件、依赖与 Content 映射。
├── Docs/Content-Structure.md  # [MODIFY] 新增 Maps/ 与 Game/ 小节、Characters 蓝图说明、资产数量与目录树、映射表。
├── README.md                  # [MODIFY] 同步 Content/Source 模块列表（加 Maps、Game）。
├── Source/PVPDemo/PVPDemo.Build.cs # [不改动] Engine 类已可满足，无需新增依赖。
└── Config/DefaultEngine.ini        # [不改动] 按确认结果，仅在地图内覆盖游戏模式。
```

## 关键类型定义

```cpp
// Source/PVPDemo/Game/PVPGameMode.h
UCLASS(Blueprintable)
class PVPDEMO_API APVPGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	APVPGameMode();                                                        // 设 PlayerControllerClass / DefaultPawnClass(C++) / bStartPlayersAsSpectators
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;                                     // 仅 Super + 一条日志
};

// Source/PVPDemo/Game/PVPPlayerController.h
UCLASS(Blueprintable)
class PVPDEMO_API APVPPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	APVPPlayerController();
	virtual void BeginPlay() override;                                     // Super + GameOnly 输入模式 + 日志
	virtual void SetupInputComponent() override;                            // Super + TODO(Input) 占位，不绑定任何 Action
	virtual void OnPossess(APawn* InPawn) override;                        // Super + 打印实际 Pawn 类名
	UFUNCTION(BlueprintPure, Category = "PVP|Pawn")
	APVPBaseCharacter* GetPVPCharacter() const;                            // Cast<APVPBaseCharacter>(GetPawn())
};
```

## 执行注意事项

- **必须先关闭编辑器**（当前 `UnrealEditor.exe` 正在运行）：否则 DLL 被占用导致 UBT 链接失败、资产被双写覆盖。构建命令模板：`D:/UE/UE_5.9/UE_5.9/Engine/Build/BatchFiles/Build.bat PVPDemoEditor Win64 Development -Project="D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -WaitMutex`。
- 建资产命令模板：`D:/UE/UE_5.9/UE_5.9/Engine/Binaries/Win64/UnrealEditor-Cmd.exe "D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -run=PythonScript -Script="D:/UE/Unreal Projects/PVPDemo/Content/Python/setup_pvp_game_framework.py" -unattended -nosplash -nullrhi`；命中 `LogPython: Error` 或断言失败即视为失败并回读日志定位。
- 幂等与保护：脚本对已存在蓝图走 `load_asset` 复用（不重建，避免打断引用）；地图已存在则跳过复制只重设 WorldSettings/PlayerStart；写操作只落在 `Content/PVPDemo/` 与 `Saved/`，绝不修改 `Content/InfimaGames/`。
- 双 Pawn 风险：不要保留原 `BP_TFA_BaseCharacter` 实例（其蓝图默认自动附身），统一改为 `PlayerStart` + GameMode 生成 `BP_PVPBaseCharacter`，否则 PIE 会出现两个重叠角色。
- 删除/替换 Actor 时先读取并缓存世界变换，再 `spawn_actor_from_class(unreal.PlayerStart, loc, rot)`，避免位置偏移（PlayerStart 的位置即 Pawn 胶囊中心，与原角色 Actor 位置语义一致）。
- 地图保存用 `EditorLoadingAndSavingUtils.save_map(world, path)`；资产保存用 `save_asset(path, only_if_is_dirty=False)`；报告与日志不写敏感信息。
- 性能与体量：本次仅新增 4 个源文件与 3~4 个资产，地图为单关卡复制（无 World Partition 外部 Actor 扩散），无运行时热路径改动；命令行为一次性离线操作，不引入编辑器 Tick 开销。
- 影响面控制：不重命名任何既有类/资产，不改 `BP_PVPBaseCharacter` 的默认值，不改工程默认地图与全局 GameMode 配置；变更可回退（删除新增文件/资产即可）。

## Agent Extensions

### Skill

- **虚幻引擎Claude Code技能**
- Purpose: 约束本次 UE5 C++ 实现符合 Epic 编码规范与反射/模块约定（UCLASS/GENERATED_BODY、include 顺序、日志类别、构造函数中设置 CDO 默认值）。
- Expected outcome: `Game/PVPGameMode.*`、`Game/PVPPlayerController.*` 可直接通过 UBT 编译且零警告级错误。
- **UE5架构规划师**
- Purpose: 规划新增 `Game/` 层的模块归属、Build.cs 依赖与 Public/Private 暴露边界，确保与既有 `Character/` 层单向依赖、不产生循环依赖。
- Expected outcome: 输出明确的层职责与依赖方向，`PVPDemo.Build.cs` 维持无需改动并给出可扩展的边界结论。
- **UE Python Scripting**
- Purpose: 编写幂等的 headless 资产自动化脚本（BlueprintFactory 建蓝图、CDO 设默认类、duplicate_map 复制关卡、WorldSettings 覆盖游戏模式、PlayerStart 替换 Actor、独立校验脚本）。
- Expected outcome: `setup_pvp_game_framework.py` 与 `verify_pvp_game_framework.py` 可无人值守执行并产出 `Saved/PVPGameFrameworkReport/` 下的通过/失败报告。