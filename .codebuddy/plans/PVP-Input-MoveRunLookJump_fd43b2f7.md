---
name: PVP-Input-MoveRunLookJump
overview: 参考 Lyra 为角色接入 Move/Run/Look/Jump：在 Input 层落地 DataAsset 输入配置与 PVPInputComponent，GAS 层登记原生输入 Tag，角色绑定四个动作，PlayerController 在代码中加载并添加默认 IMC_PVP_Default；复用现有 IA/IMC 资产并通过脚本校验、生成 DA_PVP_InputConfig 后挂到角色蓝图。
todos:
  - id: input-layer
    content: 新增 GAS 原生输入 Tag、InputConfig 数据资产与 PVPInputComponent 绑定组件
    status: completed
  - id: character-bindings
    content: 在 PVPBaseCharacter 绑定 Move/Run/Look/Jump 并实现走跑速度切换
    status: completed
    dependencies:
      - input-layer
  - id: controller-imc
    content: 在 PVPPlayerController 代码中默认加载 IMC_PVP_Default 并在 BeginPlay 注入
    status: completed
    dependencies:
      - input-layer
  - id: asset-script
    content: 编译后用 [skill:UE Python Scripting] 运行 setup_pvp_input.py，校验 IA/IMC 并生成输入配置资产
    status: completed
    dependencies:
      - character-bindings
      - controller-imc
  - id: docs-sync
    content: 核对报告与日志，同步 Source-Structure、Content-Structure 与 AGENTS 文档地图
    status: completed
    dependencies:
      - asset-script
---

## 产品概述

在现有 UE5 第一人称 PVP Demo 中，参考 Lyra 的数据驱动输入方案，为玩家角色补齐 Move、Run、Look、Jump 四个基础动作的完整输入链路，使角色在自有地图中可直接移动、转视角、跳跃与按住加速跑。

## 核心功能

- **动作输入**：
- Move：按视角朝向的平面方向前后左右移动。
- Look：鼠标控制水平与垂直视角。
- Jump：按下起跳、松开停止继续上升。
- Run：按住加速、松开恢复普通速度，两种速度可在默认值中调整。
- **输入配置数据化**：以一份输入配置资产把四个动作语义与输入 Action 关联，角色按语义绑定，新增动作只扩展配置，不改绑定流程。
- **默认输入映射**：玩家控制器在代码中自带默认输入映射上下文，进入游戏自动生效，无需在蓝图里手动添加。
- **资产接入与校验**：复用现有 Move、Look、Jump、Run 输入动作与默认映射上下文，并校验其数值类型与按键映射完整性。
- **可扩展性**：后续 Sprint、Crouch 与 GAS 技能输入可直接复用同一套配置与绑定入口。
- **视觉表现**：不新增界面，沿用第一人称视角与现有动画表现；视角俯仰仍由角色头部骨骼动画驱动，输入层只负责写入控制旋转。

## 技术栈

- 引擎：UE 5.9（`D:/UE/UE_5.9/UE_5.9`）。
- 语言与模块：C++，运行时模块 `PVPDemo`；`EnhancedInput`、`GameplayTags`、`GameplayAbilities` 已在 `Source/PVPDemo/PVPDemo.Build.cs` 中声明，本轮无新增模块依赖。
- 输入系统：Enhanced Input（`IMC_PVP_Default` + 29 个 `IA_PVP_*` 已存在于 `Content/PVP/Input`）。
- 资产产出：`Content/Python/` 下的无头 Python 脚本，遵循既有 `setup_pvp_game_framework.py` 的幂等与报告模式。

## 实现方案

采用 Lyra 式三层结构：**输入配置数据资产（Tag 到 InputAction 映射）、EnhancedInputComponent 子类（按 Tag 绑定）和玩家入口注入默认 IMC**。

关键决策与理由：

1. **DataAsset 而非散落 UPROPERTY**：`UPVPInputConfig` 统一承载动作语义与 Action，角色只依赖抽象 Tag；后续 Sprint、Crouch 只需扩配置。相比在角色上直接写四个 Action 引用，多一个资产，但可复用、可扩展、可被蓝图覆盖。
2. **`UPVPInputComponent` 子类**：提供 `BindNativeAction` 模板，统一处理“找不到 Action 时跳过并告警”，避免在每个绑定处重复判空；返回指针仅用于判空，不长期持有，规避 TArray 重分配导致的悬垂。
3. **不改 `Config/DefaultInput.ini`**：UE 5.9 的 `APawn::OverrideInputComponentClass` 已在引擎中提供（`Pawn.cpp:793`），在角色构造函数里指定 `UPVPInputComponent` 即可，改动面最小。
4. **默认 IMC 写在代码**：`APVPPlayerController` 构造函数用 `ConstructorHelpers::FObjectFinder` 加载 `/Game/PVP/Input/IMC_PVP_Default` 填入可编辑数组，`BeginPlay` 通过 `UEnhancedInputLocalPlayerSubsystem` 执行 `ClearAllMappings` 后按优先级添加；资产缺失时只告警不崩溃，且蓝图仍可覆盖。
5. **Look 写 Controller 旋转**：`CameraFP` 的 `bUsePawnControlRotation=false`，视角俯仰由 `ABP_TFA_FP_BaseCharacter` 驱动 head 骨骼，因此 `Input_Look` 只调用 `AddControllerYawInput` 与 `AddControllerPitchInput`，不直接改相机组件。
6. **Run 暂不进 GAS**：当前 GAS 层尚未搭建，`Input_RunStarted/Completed` 直接切换 `MaxWalkSpeed`（Walk 300 / Run 600，均 `EditDefaultsOnly`），并标注 `TODO(Network/GAS)`；网络权威速度留给后续 CMC 子类或 GAS 属性。

性能与可靠性：

- 绑定只在 `SetupPlayerInputComponent` 执行一次；`Move/Look` 为每输入事件 O(1) 计算，不新增 Tick；速度写入仅在 Run 状态切换时发生。
- 所有资产引用判空并输出可定位日志（IMC 数量、InputConfig 缺失、InputComponent 类型不符），不含敏感信息。
- 影响范围控制：不改 `Build.cs`、不改 `DefaultInput.ini`、不改相机与动画层、不修改现有 IA/IMC 资产本体。

## 架构设计

数据流保持单向：

```
IMC_PVP_Default → EnhancedInput → PVPInputComponent → PVPBaseCharacter
                                                       ├─ Move  → CharacterMovement
                                                       ├─ Look  → Controller 旋转
                                                       ├─ Jump  → Jump/StopJumping
                                                       └─ Run   → MaxWalkSpeed
Game/PVPPlayerController ── 注入 IMC（入口层，只读角色状态）
Animation / Camera ── 只读角色与控制旋转
```

`Character/` 依赖 `Input/` 的配置与组件类型，符合 `Input → Character` 方向；`Game/` 的 PlayerController 依赖 `Input/` 类型注入映射，`Character/` 不感知 `Game/`。

## 执行细节

- **IA 数值类型**：Move、Look 必须为 `Axis2D`，Jump、Run 为 `Boolean`；绑定前由脚本读取 `value_type` 断言，不匹配时在报告中明确报错（必要时只修正自有副本的 ValueType，不动第三方原包）。
- **IMC 映射**：脚本读取 `mappings`，按 Action 分组输出按键（预期 Move=WASD、Look=Mouse2D、Jump=SpaceBar、Run=LeftShift），四个动作必须各有至少一条映射，否则报告失败并提示在编辑器补齐。
- **DataAsset 赋值**：脚本幂等创建 `/Game/PVP/Input/DA_PVP_InputConfig`，把四个 Tag 与 IA 写入 `NativeInputActions`，并通过 `unreal.get_default_object(BP_PVPBaseCharacter.generated_class())` 设置 `input_config` 后编译保存蓝图。
- **验证顺序**：先编译 `PVPDemoEditor`，再以 `-run=PythonScript -unattended -nosplash -nullrhi` 运行脚本并检查 `Saved/PVPInputReport/report.md`，最后在 PIE 冒烟（WASD、鼠标、空格、Shift 按住/松开）。
- **文档同步**：源码新增 Input、GAS 层文件与资产新增，必须更新 `Docs/Source-Structure.md`、`Docs/Content-Structure.md`（`Content/PVP` 78→79、Input 30→31，按执行后实际统计）与 `AGENTS.md` 文档地图（新增脚本与 `Saved/PVPInputReport`）；`README.md` 与 `Docs/AI-Workflow.md` 预计不变。

## 目录结构

```
Source/PVPDemo/
├── GAS/
│   └── PVPGameplayTags.h/.cpp        # [NEW] 声明并定义 InputTag.Move/Look/Jump/Run 原生 Tag，使用 UE_DECLARE/DEFINE_GAMEPLAY_TAG，不预造未使用 Tag。
├── Input/
│   ├── PVPInputConfig.h/.cpp         # [NEW] FPVPInputAction（InputAction + InputTag）、UPVPInputConfig（NativeInputActions + FindNativeInputActionForTag）、FPVPInputMappingContextAndPriority（InputMapping + Priority）。
│   └── PVPInputComponent.h/.cpp      # [NEW] UEnhancedInputComponent 子类；BindNativeAction 模板按 Tag 查找 Action 并绑定，找不到返回 nullptr。
├── Character/
│   └── PVPBaseCharacter.h/.cpp       # [MODIFY] 构造设 OverrideInputComponentClass 与初始 MaxWalkSpeed；新增 InputConfig、WalkSpeed、RunSpeed、bIsRunning；实现 SetupPlayerInputComponent 与四个输入处理函数。
└── Game/
    └── PVPPlayerController.h/.cpp    # [MODIFY] 新增 DefaultMappingContexts 数组，构造加载 IMC_PVP_Default，BeginPlay 注入映射上下文并打日志。

Content/
├── Python/
│   └── setup_pvp_input.py            # [NEW] 校验 4 个 IA 的 ValueType 与 IMC 映射；幂等创建 DA_PVP_InputConfig；赋给 BP_PVPBaseCharacter CDO；输出 Saved/PVPInputReport/report.json|md。
└── PVP/Input/
    └── DA_PVP_InputConfig.uasset     # [NEW，仅由脚本产出] 四个 Tag 到 IA 的输入配置数据资产。

Docs/
├── Source-Structure.md               # [MODIFY] 更新 Input/GAS 由空变实、角色输入绑定、默认 IMC 注入与无新增依赖结论。
├── Content-Structure.md              # [MODIFY] 更新逻辑资产总数、Input 目录数量与脚本清单。
└── AGENTS.md                         # [MODIFY] 更新源码/资产摘要，文档地图登记新脚本与 Saved/PVPInputReport。
```

## 关键代码结构

```cpp
// Source/PVPDemo/Input/PVPInputConfig.h
USTRUCT(BlueprintType)
struct FPVPInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr&lt;UInputAction&gt; InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

UCLASS(BlueprintType)
class PVPDEMO_API UPVPInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray&lt;FPVPInputAction&gt; NativeInputActions;

	const UInputAction* FindNativeInputActionForTag(const FGameplayTag&amp; InputTag) const;
};
```

```cpp
// Source/PVPDemo/Input/PVPInputComponent.h
template &lt;class UserClass, typename FuncType&gt;
FEnhancedInputActionEventBinding* BindNativeAction(
	const UPVPInputConfig* InputConfig,
	const FGameplayTag&amp; InputTag,
	ETriggerEvent TriggerEvent,
	UserClass* Object,
	FuncType Func);
```

## Agent Extensions

### Skill

- **UE Python Scripting**
- Purpose: 按项目既有无头脚本风格编写并运行 `Content/Python/setup_pvp_input.py`，完成 IA ValueType 与 IMC 映射校验、`DA_PVP_InputConfig` 生成、`BP_PVPBaseCharacter` CDO 赋值以及 `Saved/PVPInputReport` 报告输出。
- Expected outcome: 生成并保存输入配置数据资产，角色蓝图默认引用该资产，报告中四个动作的数值类型与按键映射全部通过，可供编译后直接在 PIE 验证。