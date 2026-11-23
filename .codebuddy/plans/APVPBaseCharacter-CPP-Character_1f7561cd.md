---
name: APVPBaseCharacter-CPP-Character
overview: 创建 C++ 角色类 APVPBaseCharacter，复刻 BP_TFA_BaseCharacter 的第一人称相机与组件树，并提供 Python 提取脚本与原蓝图逐项比对，确保派生蓝图的 1P 视角完全一致。
todos:
  - id: dump-tfa-character
    content: 使用 [skill:UE Python Scripting] 编写并执行脚本，导出 BP_TFA_BaseCharacter 组件树与相机参数到 Saved/PVPCharacterDump
    status: completed
  - id: implement-character
    content: 按导出数据新建 Character/PVPBaseCharacter.h 与 .cpp，复刻 1P 组件层级、相对变换与相机参数
    status: completed
    dependencies:
      - dump-tfa-character
  - id: build-module
    content: 关闭编辑器后编译 PVPDemoEditor，修复编译与链接错误，确认类可被蓝图继承
    status: completed
    dependencies:
      - implement-character
  - id: verify-parity
    content: 使用 [skill:UE Python Scripting] 创建派生测试蓝图并逐项比对 1P 相机数据，输出差异报告
    status: completed
    dependencies:
      - build-module
  - id: tune-and-document
    content: 依据差异报告微调 C++ 默认值至完全一致，并在代码中标注 3P 与输入扩展点
    status: completed
    dependencies:
      - verify-parity
---

## 产品概述

为 PVP 射击项目新增一个可复用的 C++ 第一人称角色基类，组件层级与相机设置完全沿用参考角色蓝图 BP_TFA_BaseCharacter，使基于该类派生出的蓝图在 1P 视角下与该参考角色的观察位置、朝向和视场完全重合。

## 核心功能

- 新增角色基类 APVPBaseCharacter（ACharacter 派生），放置于 Source/PVPDemo/Character
- 复刻参考角色的组件树：胶囊体、第一人称骨骼网格组件、相机枢轴点、1P 相机组件，父子挂接关系（含骨骼 Socket）与默认相对变换保持一致
- 复刻 1P 相机参数：视场角、相对偏移/旋转、是否跟随控制器旋转、激活状态、宽高比约束、后处理混合权重
- 对齐影响 1P 视角的角色级设置：控制器 Yaw/Pitch 使用、基础视高与蹲伏视高、移动组件朝向设置
- 本期仅实现 1P，第三人称弹簧臂与相机不创建，仅在代码中预留扩展位置
- 提供自动化的数据提取与对比校验流程：导出参考蓝图组件/相机数据，再对新派生蓝图逐项比对并输出差异报告，用于确认视角一致
- 编译通过后，可直接在编辑器中基于该 C++ 类创建蓝图子类并使用

## 技术栈

- 引擎：UE 5.9（`D:\UE\UE_5.9\UE_5.9`），模块 `PVPDemo`（Runtime），`IMPLEMENT_PRIMARY_GAME_MODULE` 已存在于 `Source/PVPDemo/PVPDemo.cpp`
- 语言/构建：C++（UE 反射 + UBT），构建命令 `Engine\Build\BatchFiles\Build.bat PVPDemoEditor Win64 Development -Project="D:/UE/Unreal Projects/PVPDemo/PVPDemo.uproject" -WaitMutex`
- 组件：`ACharacter` 自带 `UCapsuleComponent`/`USkeletalMeshComponent(CharacterMesh0)`/`UArrowComponent`/`UCharacterMovementComponent`，新增 `USkeletalMeshComponent`、`USceneComponent`、`UCameraComponent`
- 数据提取：编辑器 Python（`unreal` 模块）反射读取 `BP_TFA_BaseCharacter` 的 CDO/SCS，脚本放置于已存在的 `Content/Python/`（已有 `migrate_pvp_assets.py` 先例）
- 现有 `PVPDemo.Build.cs` 依赖已足够（Core/CoreUObject/Engine/InputCore/EnhancedInput/...），本期无需新增模块依赖

## 实现方案

1. **数据先行，禁止猜测**：先写 `dump_tfa_character.py`，读取 `/Game/InfimaGames/TacticalFPSAnimations/Common/Core/Characters/BP_TFA_BaseCharacter` 的 CDO，输出每个组件的类、变量名、父组件、挂接 Socket、RelativeLocation/Rotation/Scale3D、可见性标志，以及相机 FOV/宽高比/控制旋转/后处理参数、胶囊尺寸、BaseEyeHeight、控制器旋转开关，写入 `Saved/PVPCharacterDump/tfa_base_character.json` 与可读 `.md` 报告。
2. **组件复刻**：C++ 构造函数中按 dump 顺序 `CreateDefaultSubobject` + `SetupAttachment`，变量名与参考蓝图一致（`FP_Mesh`、`CameraPivotPoint`、`CameraFP` 等），确保派生蓝图与编辑器细节面板的对应关系清晰；已由 `ACharacter` 提供的组件不重复创建。
3. **1P 一致性保障**：相机相对变换、FOV、`bUsePawnControlRotation`、`bAutoActivate`、`bConstrainAspectRatio`、`PostProcessBlendWeight` 全部按 dump 填写；角色级 `bUseControllerRotationYaw/Pitch`、`BaseEyeHeight`、`CrouchedEyeHeight` 一并复刻（这些会直接改变 1P 相机高度与朝向）。UE5 为 LWC，`FVector` 用双精度，数值按序列化结果原样写入，不做四舍五入。
4. **闭环校验**：`compare_tfa_character.py` 对两个蓝图 CDO 做同样结构的递归 dump，按组件路径匹配，沿父链相乘得到相机相对根组件的世界变换后比对，容差 0.01 UU / 0.01 度，输出差异表；差异项回填到 C++ 默认值，直到无差异。
5. **扩展性**：不实现输入与视角切换逻辑，但预留 `CameraPivotPoint` 与注释标记的 SpringArm/CameraTP 插入点，便于后续补 3P 与 Enhanced Input 时不改动现有层级。

## 架构设计

组件层级（父子关系与 Socket 挂接以 dump 结果为准，下列为预期结构）：

```text
APVPBaseCharacter (ACharacter)
├── CapsuleComponent        (继承，根组件)
├── ArrowComponent          (继承)
├── CharacterMesh0          (继承 Mesh，TP 身体，可见性按原蓝图设置)
├── FP_Mesh                 (新增，1P 网格，按 dump 决定挂胶囊或相机)
├── CameraPivotPoint        (新增，相机枢轴)
└── CameraFP                (新增，1P 相机，按 dump 决定是否挂骨骼 Socket/跟随控制旋转)
```

数据流：Python 读取参考蓝图 CDO → JSON 基线 → C++ 构造函数默认值 → 派生蓝图 CDO → 对比脚本校验，形成单向可重复的标定链路，不产生运行时开销（无 Tick、仅一个激活相机）。

## 目录结构

```text
Source/PVPDemo/Character/PVPBaseCharacter.h    # [NEW] APVPBaseCharacter 声明。继承 ACharacter，声明 FP_Mesh/CameraPivotPoint/CameraFP 等 UPROPERTY(VisibleAnywhere, BlueprintReadOnly)，按参考蓝图分类命名；预留 3P 扩展注释；不实现输入与视角切换逻辑。
Source/PVPDemo/Character/PVPBaseCharacter.cpp  # [NEW] 构造函数实现。CreateDefaultSubobject + SetupAttachment 复刻层级与 Socket；写入 dump 得到的相对变换、FOV、控制旋转、可见性与角色级视高/旋转开关；设置 PrimaryActorTick 为不需要。
Content/Python/dump_tfa_character.py           # [NEW] 编辑器脚本。加载 BP_TFA_BaseCharacter，遍历 CDO 组件与 Character 关键属性，输出 JSON + Markdown 报告到 Saved/PVPCharacterDump。
Content/Python/compare_tfa_character.py        # [NEW] 校验脚本。创建/编译派生测试蓝图 /Game/PVPDemo/Characters/BP_PVPBaseCharacter，对两个 CDO 递归比对组件树、变换、相机与角色参数，输出 diff 表与退出码。
Saved/PVPCharacterDump/*.json|*.md             # [GENERATED] 基线数据与校验报告，不纳入源码管理。
```

不修改任何 `.uasset` 资源，不修改 `PVPDemo.Build.cs`、`PVPDemo.h/.cpp`。

## 关键代码结构

```cpp
UCLASS(Blueprintable)
class PVPDemo_API APVPBaseCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    APVPBaseCharacter();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Camera")
    TObjectPtr&lt;USceneComponent&gt; CameraPivotPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Camera")
    TObjectPtr&lt;UCameraComponent&gt; CameraFP;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Mesh")
    TObjectPtr&lt;USkeletalMeshComponent&gt; FP_Mesh;

    // TODO(3P): SpringArm + CameraTP 扩展位，本期不创建
};
```

## 实现注意

- **不要将原 BP_TFA_BaseCharacter 直接改为继承新 C++ 类**：会与其内部同名组件（FP_Mesh/CameraFP 等）冲突产生重复组件；本期验证使用新建的派生蓝图。
- 编译前必须关闭编辑器，否则 DLL 被占用导致链接失败；UBT 失败时保留完整报错并逐条修复。
- Python 脚本以 `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="..." -unattended -nosplash -nullrhi` 执行；若 CDO 组件枚举为空，回退为读取 Blueprint 的 `simple_construction_script` 节点树（脚本内实现两条路径）。
- 数值来源唯一：C++ 默认值必须来自 dump JSON，禁止凭经验填 1P 相机偏移/FOV；后续调整也只改 JSON 对应值并重新校准。
- 相机仅一个且 `bAutoActivate` 为真，`AActor::CalcCamera` 直接采用该相机，避免额外 ViewTarget 逻辑与每帧开销。
- 日志：脚本与编译输出使用控制台/Output Log，不新增日志宏，不打印大段二进制或敏感路径以外信息。
- 影响面控制：仅新增 4 个文件，不动现有模块、配置与内容资产；派生测试蓝图放在 `Content/PVPDemo/Characters/`，可随时删除重建。

## Agent Extensions

### Skill

- **UE Python Scripting**
- Purpose: 编写符合 UE 5.x 规范的编辑器 Python 脚本，完成 BP_TFA_BaseCharacter 的 CDO/SCS 组件树与相机参数提取、派生蓝图创建与逐项比对校验。
- Expected outcome: 生成 `Content/Python/dump_tfa_character.py` 与 `compare_tfa_character.py`，成功产出 `Saved/PVPCharacterDump` 下的 JSON/Markdown 基线与差异报告，报告显示新旧角色 1P 相机无差异。