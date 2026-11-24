# PVP 技能层模块导航

> 维度：技能层（GameplayTag 原生声明；GAS 基础设施未搭建）
> 模块：GAS
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-24
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/GAS/GAS-Dev.md`（开发流程与踩坑）、`Docs/GAS/GAS-Integration.md`（依赖与接口契约）

本文件是技能层模块的固定导航入口。进入本模块的任务先读本文件，再按第 2 节路由按需读取；本文第 3 节是**原生 GameplayTag 命名与语义的唯一权威表**，其他文档不复制。

> **阶段声明**：`GAS/` 目前**只有原生 Tag 声明**，`ASC`、`AttributeSet`、`GameplayAbility`、`AbilitySystemGlobals`、双网格 Montage Task、`GameplayCue` 全部未实现。本文档描述的是"Tag 契约现状"，不是"GAS 能力现状"。

---

## 1. 模块定位与边界

**职责**：

- 作为工程内原生 GameplayTag 的**唯一声明位置**（`FNativeGameplayTag` + `UE_DEFINE_GAMEPLAY_TAG`）。
- 定义 Tag 的命名规范与语义边界：Tag 只表达语义，不表达按键、不表达资产路径。
- 为 `Input/`（`UPVPInputConfig` 的槽位与 `APVPBaseCharacter` 的绑定）提供 Tag 常量。

**非职责**（不属于本模块，当前也不要在这里实现）：

- 输入动作、键位与 IMC：属于 `Input/`，权威表在 `Docs/Input/README.md` 第 3 节。
- 角色状态（速度、姿态、血量）：当前由 `Character/` 直接持有，未来才考虑迁到 GAS 属性。
- 相机与动画表现：属于 `Camera/`（预留）与 `Animation/`（已落地 `UPVPAnimInstanceBase` 基础 locomotion 变量，见 `Docs/Animation/`）；GAS 只能通过 Tag / 属性驱动，不反向依赖具体类。
- `State.*`、`GameplayCue.*`、`InputTag.Ability.*`：尚未定义，不要预先占位。

---

## 2. 文档清单与路由

| 文档 | 内容 |
| --- | --- |
| `Docs/GAS/README.md` | 本导航：定位、路由、原生 Tag 速查、注册入口、未实现能力、校验入口、维护规则 |
| `Docs/GAS/GAS-Dev.md` | 实现细节：源码结构、设计决策、新增 Tag SOP、踩坑清单、校验与冒烟、GAS 后续路线 |
| `Docs/GAS/GAS-Integration.md` | 契约：模块依赖、数据流、对外接口、其他模块调用方式、禁止依赖方向、变更影响面 |

**按场景路由**：

| 我要做的事 | 读哪里 |
| --- | --- |
| 查已定义哪些原生 Tag、分别被谁消费 | 本文第 3 节 |
| 查某个输入动作绑到哪个函数 | `Docs/Input/README.md` 第 3 节 |
| 新增一个 Tag（含声明 / 定义 / 消费方登记） | `GAS-Dev.md` 第 4 节（SOP） |
| 排查链接错误 `unresolved external symbol` | `GAS-Dev.md` 第 5 节踩坑 1 |
| 排查"绑定数少一条 / Tag 找不到" | `GAS-Dev.md` 第 5 节踩坑 2、3 |
| 查 C++ 接口签名与 Tag 类型 | `GAS-Integration.md` 第 4 节 |
| 让其他模块消费 Tag / 判断依赖方向 | `GAS-Integration.md` 第 5 / 6 节 |
| 改 Tag 名称前评估影响面 | `GAS-Integration.md` 第 7 节 |
| 确认 ASC / 技能何时接入 | 本文第 5 节 → `GAS-Dev.md` 第 7 节 |

---

## 3. 已实现能力速查（权威表：原生 Tag）

当前只定义 **6 个原生 Tag**，全部属于 `InputTag` 族：

| Tag 字符串 | C++ 符号 | 语义 | 消费方 |
| --- | --- | --- | --- |
| `InputTag.Move` | `PVPGameplayTags::InputTag_Move` | 平面移动意图（相对 `ControlRotation.Yaw`） | `UPVPInputConfig` 槽位 0 → `APVPBaseCharacter::Input_Move` |
| `InputTag.Look` | `PVPGameplayTags::InputTag_Look` | 视角 Yaw / Pitch 意图 | 槽位 1 → `Input_Look` |
| `InputTag.Jump` | `PVPGameplayTags::InputTag_Jump` | 起跳 / 停止起跳 | 槽位 2 → `Input_JumpStarted` / `Input_JumpCompleted` |
| `InputTag.Run` | `PVPGameplayTags::InputTag_Run` | 按住跑 | 槽位 3 → `Input_RunStarted` / `Input_RunCompleted` |
| `InputTag.Aim` | `PVPGameplayTags::InputTag_Aim` | 按住瞄准（ADS） | 槽位 4 → `Input_AimStarted` / `Input_AimCompleted` |
| `InputTag.Fire` | `PVPGameplayTags::InputTag_Fire` | 射击一次 | 槽位 5 → `Input_FireStarted` |

**代码位置**：

| 项 | 位置 |
| --- | --- |
| 声明 | `Source/PVPDemo/GAS/PVPGameplayTags.h`（`extern PVPDEMO_API FNativeGameplayTag`） |
| 定义 | `Source/PVPDemo/GAS/PVPGameplayTags.cpp`（`UE_DEFINE_GAMEPLAY_TAG(<符号>, "<Tag 字符串>")`） |
| 槽位预置 | `Source/PVPDemo/Input/PVPInputConfig.cpp` 构造函数按 Move / Look / Jump / Run / Aim / Fire 顺序预置；`EnsureNativeInputActionSlots` 给旧 4 槽资产补槽 |
| 绑定点 | `APVPBaseCharacter::SetupPlayerInputComponent`（`Source/PVPDemo/Character/PVPBaseCharacter.cpp`） |
| 键位与 ValueType | `Docs/Input/README.md` 第 3 节（本文不复写） |

**未定义的 Tag 族**：`State.*`、`GameplayCue.*`、`InputTag.Ability.*`、`Ability.*` 均不存在于代码中。新增前必须先有实际消费方，不预造。

---

## 4. 启动与运行时入口

- **无运行时入口**：本模块没有 Actor / Component / Subsystem，也没有 `BeginPlay` 或 Tick。
- **注册时机**：`UE_DEFINE_GAMEPLAY_TAG` 在模块加载期把原生 Tag 注册进全局 Tag 管理器；因此模块被引用即注册，不依赖任何资产或配置。
- **生效判据**：`DA_PVP_InputConfig` 的槽位能找到对应 IA（见 `Docs/Input/README.md` 第 3 节），角色日志为 `bound 9/9 native input bindings`。
- **日志**：本模块没有独立日志类别；Tag 相关失败表现为 `UPVPInputComponent::BindNativeAction` 返回 `nullptr`，由 `APVPBaseCharacter` 的 `bound N/9` 日志暴露。

---

## 5. 未实现能力与扩展方向

- **ASC 与属性**：无 `AbilitySystemComponent`、`AttributeSet`、血量 / 伤害 / 速度属性；规划中 ASC 挂在 `PlayerState`（见 `Docs/Source-Structure.md` 2.6 节）。
- **GameplayAbility**：无任何 GA / 技能输入；`InputTag.Ability.*` 未定义。
- **AbilitySystemGlobals**：无自定义 Globals；规划中的 `PVPAbilitySystemGlobals` 用于修正 `AllocAbilityActorInfo` 使用的 SkeletalMeshComponent，避免 Montage 播到 1P 手臂。
- **双网格 Montage**：无 `PVPAbilityTask_PlayDualMontage`；1P 表现与 3P 复制的分离开销尚未处理。
- **GameplayCue**：无 Cue Tag、无 Cue Notify；音效 / 特效资产（`Content/PVPDemo/Audio`、`VFX`）尚未接入为 Cue。
- **Tag 族**：`State.*`（如 `State.Dead`、`State.Running`）未定义；当前跑步状态仍是 `bIsRunning` 布尔量。
- **网络**：无预测键、无复制属性；技能层与网络相关的设计全部未开始。

落地顺序与依赖关系（ASC 宿主 → 属性集 → GA 与技能输入 → Globals 修正 → 双网格 Montage → Cue 与 Tag 族）见 `GAS-Dev.md` 第 7 节。

---

## 6. 校验与报告入口

本模块**没有专用 Python 脚本**（`Content/Python/` 下无 GAS 相关工具）。当前校验方式是：

| 方式 | 检查内容 |
| --- | --- |
| 编译 | 声明与定义齐全（缺定义会链接失败，见 `GAS-Dev.md` 踩坑 1） |
| 编辑器 Gameplay Tag 列表 | `Project Settings → Gameplay Tags` 中可见 `InputTag.Move/Look/Jump/Run/Aim/Fire` |
| 运行日志 | 进入 `L_PVP_Showcase` 后 `bound 9/9 native input bindings`，说明 6 个 Tag 均能查到 IA |
| 输入资产校验 | `Content/Python/setup_pvp_input.py` 间接覆盖：它按槽位写入 IA，槽位依赖 C++ 预置的 Tag 顺序 |

详细步骤与冒烟清单见 `GAS-Dev.md` 第 6 节。

---

## 7. 文档维护规则

| 变更 | 必须同步 |
| --- | --- |
| 新增 / 重命名 / 删除原生 Tag | 本文第 3 节 + `GAS-Integration.md` 第 4 / 7 节 + 消费方（`UPVPInputConfig` 槽位、`PVPBaseCharacter` 绑定）+ `Docs/Input/README.md` 第 3 节 + `setup_pvp_input.py` |
| Tag 语义变化（同名不同义） | 本文第 3 节 + 消费该 Tag 的行为文档（`Docs/Input/`、`Docs/Character/`） |
| 引入 ASC / GA / AttributeSet / Cue | 本文第 5 节 + `GAS-Dev.md` 第 3 / 7 节 + `GAS-Integration.md` 第 2 / 4 / 5 节 + `Docs/Source-Structure.md` 2.6 节 + `AGENTS.md` 模块现状 |
| 新增本模块维护文档 | 本文第 2 节文档清单登记，命名 `GAS-<主题>.md` |
| 任何改动 | 三份文档文首「最后更新」刷新为当天日期 |

- 动作 / 键位 / 回调的权威表只存在于 `Docs/Input/README.md` 第 3 节；本文只定义 Tag 语义。
- 资产总数、文件数只写在 `Docs/Content-Structure.md` / `Docs/Source-Structure.md`，本文不复制。
- 本模块文档与代码不一致时，以代码为准并当轮修正文档。
