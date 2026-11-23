# PVP 技能层模块开发文档

> 维度：技能层（GameplayTag 原生声明；GAS 基础设施未搭建）
> 模块：GAS
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-23
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/GAS/README.md`（导航与 Tag 速查）、`Docs/GAS/GAS-Integration.md`（依赖与接口契约）

本文只写**怎么实现、怎么开发、踩过什么坑**。Tag 名称、语义与消费方的权威表在 `Docs/GAS/README.md` 第 3 节，动作与键位在 `Docs/Input/README.md` 第 3 节。

---

## 1. 目的与范围

覆盖范围：

- `Source/PVPDemo/GAS/PVPGameplayTags.h/.cpp` 的原生 Tag 声明与定义；
- 新增 Tag 的标准流程（声明 → 定义 → 消费方 → 槽位 / 脚本 → 文档）；
- Tag 相关踩坑、校验与冒烟；
- GAS 基础设施（ASC / 属性 / GA / Globals / 双网格 Montage / Cue）的**规划边界**。

不覆盖：输入动作与键位（`Docs/Input/`）、角色状态实现（`Docs/Character/`）、相机与动画表现（预留模块）。

当前范围只覆盖 `InputTag.Move/Look/Jump/Run` 四个原生 Tag，没有技能逻辑。

---

## 2. 当前状态与源码结构

### 2.1 当前状态

- 只有一个头文件与一个实现文件，`namespace PVPGameplayTags` 导出 4 个 `FNativeGameplayTag`。
- Tag 不依赖任何资产：`.cpp` 中 `UE_DEFINE_GAMEPLAY_TAG` 在模块加载期注册。
- 唯一的消费链是输入：`UPVPInputConfig` 构造函数按 4 个 Tag 预置槽位 → `setup_pvp_input.py` 按索引写入 IA → `APVPBaseCharacter::SetupPlayerInputComponent` 按 Tag 查 IA 并绑定。
- **没有** ASC、AttributeSet、GameplayAbility、AbilitySystemGlobals、AbilityTask、GameplayCue、Subsystem。

### 2.2 源码结构

| 文件 | 职责 |
| --- | --- |
| `PVPGameplayTags.h` | `namespace PVPGameplayTags` + 4 个 `extern PVPDEMO_API FNativeGameplayTag` 声明，每个带语义注释 |
| `PVPGameplayTags.cpp` | `UE_DEFINE_GAMEPLAY_TAG(InputTag_Xxx, "InputTag.Xxx")` 四条定义，保证字符串与符号一一对应 |

### 2.3 本模块涉及的资产

- **无自有资产**。`Content/PVP/Input/DA_PVP_InputConfig` 与 `IA_PVP_*` 属于输入模块资产，本模块只提供 Tag 常量。
- 音效 / 特效资产（`Content/PVPDemo/Audio`、`Content/PVPDemo/VFX`）未来可能作为 `GameplayCue` 的表现素材，当前无 Cue 引用。
- 资产数量只看 `Docs/Content-Structure.md`，本文不复制总数。

---

## 3. 设计决策与理由

1. **只定义当前被消费的 Tag**。Tag 表是契约，不是待办清单；定义未被消费的 Tag 会让"哪些能力真的存在"变得不可读。新增必须以"有消费方"为前提。
2. **用 `FNativeGameplayTag` 而非字符串常量**。`FNativeGameplayTag` 在模块加载期注册并带编译期符号，避免运行时 `RequestGameplayTag` 拼字符串失败，也避免 Tag 拼写错误只在上线后暴露。
3. **声明与定义分离，符号名与 Tag 字符串一一对应**。`InputTag_Move` ↔ `"InputTag.Move"`，`UE_DEFINE_GAMEPLAY_TAG` 同时承担注册与定义，缺一即链接失败。
4. **Tag 只表达语义，不表达按键或资产**。键位在 IMC，动作在 IA，Tag 只做语义标签；因此换键、换 IA 都不需要改本模块。
5. **命名族按用途划分**。输入用 `InputTag.*`，状态计划用 `State.*`，表现计划用 `GameplayCue.*`，技能输入计划用 `InputTag.Ability.<Skill>`；不混族、不复用原生动作 Tag 承载技能。
6. **槽位由 C++ 预置而非脚本构造**。GameplayTag 无法从 Python 构造，`UPVPInputConfig` 构造函数按 Move / Look / Jump / Run 顺序建立槽位，脚本只按索引填 IA；因此 C++ 顺序与脚本 `REQUIRED_ACTIONS` 是硬约定（见踩坑 4）。
7. **GAS 基础设施不提前落地**。`Build.cs` 已声明 `GameplayAbilities` / `GameplayTasks`，但依赖声明不等于能力存在；ASC 宿主（规划为 `PlayerState`）、Globals 与双网格 Montage 需要角色 / 动画阶段稳定后再引入。

---

## 4. 开发流程 SOP（新增 Tag）

1. **确认消费方**：说明谁按这个 Tag 查表 / 绑定 / 驱动表现；没有消费方不新增。
2. **选命名族**：输入语义用 `InputTag.*`；状态用 `State.*`；Cue 用 `GameplayCue.*`；不确定时先停下来对齐 `Docs/Source-Structure.md` 2.6 节。
3. **声明**：在 `PVPGameplayTags.h` 的 `namespace PVPGameplayTags` 中加 `extern PVPDEMO_API FNativeGameplayTag <符号>;`，并按现有风格写一行语义注释。
4. **定义**：在 `PVPGameplayTags.cpp` 中加 `UE_DEFINE_GAMEPLAY_TAG(<符号>, "<Tag 字符串>");`，符号名与字符串必须一一对应。
5. **接消费方**：
   - 若是输入动作：扩展 `UPVPInputConfig` 构造函数槽位 → 同步 `setup_pvp_input.py` 的 `REQUIRED_ACTIONS` → 在绑定处调用 `BindNativeAction` → 更新 `ExpectedBindings` 日志期望；
   - 若是技能输入（未来）：用 `InputTag.Ability.<Skill>`，在 GAS / Hero 层绑定，不复用原生动作 Tag。
6. **编译并校验**：编译通过（无 `unresolved external symbol`）→ 跑 `setup_pvp_input.py`（输入类 Tag）→ PIE 检查绑定数量与行为。
7. **同步文档**（缺一视为任务未完成）：
   - `Docs/GAS/README.md` 第 3 节（Tag 权威表）与第 5 节（未实现能力）；
   - `Docs/GAS/GAS-Integration.md` 第 4 / 7 节（接口与影响面）；
   - 输入类 Tag 还要同步 `Docs/Input/README.md` 第 3 节、`Docs/Input/Input-Integration.md` 第 4.3 节；
   - 新增 Tag 族或 GAS 能力时同步 `Docs/Source-Structure.md` 2.6 节与 `AGENTS.md` 模块现状。

---

## 5. 踩坑清单

### 5.1 只加声明没加定义，链接失败

- **现象**：编译末期出现 `unresolved external symbol "FNativeGameplayTag PVPGameplayTags::InputTag_Xxx"`，或模块加载时崩溃。
- **根因**：`PVPGameplayTags.h` 中的 `extern` 只是声明；`UE_DEFINE_GAMEPLAY_TAG` 才提供定义与注册。只在头文件写一行 `extern` 不会产生符号。
- **正确做法**：每个 `extern` 在 `PVPGameplayTags.cpp` 中对应一条 `UE_DEFINE_GAMEPLAY_TAG(<符号>, "<Tag 字符串>")`；新增时成对提交，不要分开两次改。
- **验证方式**：重新编译 `PVPDemoEditor` 无链接错误；编辑器 Gameplay Tag 列表中能看到新 Tag。

### 5.2 Tag 字符串与 C++ 符号不对应，绑定少一条

- **现象**：编译通过，但 PIE 日志 `bound 5/6`（或其他 N/6），对应动作无响应，且没有明显报错。
- **根因**：`UPVPInputConfig` 按 Tag 相等匹配 IA。若 `UE_DEFINE_GAMEPLAY_TAG` 的字符串与 `PVPInputConfig` 槽位里的语义不一致（例如把 `InputTag.Jump` 写成 `InputTag.Jumping`），`FindNativeInputActionForTag` 返回 `nullptr`，`BindNativeAction` 返回空，绑定被跳过。
- **正确做法**：符号名与字符串严格一一对应（`InputTag_Move` ↔ `InputTag.Move`）；改名时同步检查 `UPVPInputConfig.cpp`、`APVPBaseCharacter::SetupPlayerInputComponent` 与 `setup_pvp_input.py`。
- **验证方式**：日志恢复 `bound 6/6`；`Saved/PVPInputReport/report.md` 中 4 个槽位均能解析 IA。

### 5.3 运行时用字符串拼 Tag，查不到或注册失败

- **现象**：`FGameplayTag::RequestGameplayTag(FName("InputTag.Move"))` 返回空 Tag，或 `RequestGameplayTag(..., ErrorIfNotFound=true)` 直接断言 / 报错。
- **根因**：原生 Tag 的注册发生在模块加载期，且 Tag 名称对 Tag 管理器不透明；运行期用字符串拼 Tag 容易与真实注册名不一致，也会绕过符号引用导致模块未被链接。
- **正确做法**：始终引用 `PVPGameplayTags::InputTag_*` 常量；需要按字符串查时只用于日志 / 调试，不用于绑定判定。跨模块传递用 `FGameplayTag` 值，不传裸字符串。
- **验证方式**：全局搜索 `RequestGameplayTag` 无运行期绑定相关用法；PIE 绑定正常。

### 5.4 新增输入 Tag 后槽位 / 脚本 / 期望数量未同步

- **现象**：新增第 5 个输入动作后，`DA_PVP_InputConfig` 里该槽位的 IA 为空或错位（按空格触发跑步之类），日志 `bound N/6` 的期望值仍是 6，或新动作完全不响应。
- **根因**：输入 Tag 的实际映射依赖三处同步——`UPVPInputConfig` 构造函数的预置槽位顺序、`setup_pvp_input.py` 的 `REQUIRED_ACTIONS` 顺序、`SetupPlayerInputComponent` 的绑定与 `ExpectedBindings`。只加 Tag 不改这三处，脚本按旧索引写入，绑定也查不到。
- **正确做法**：新增 `InputTag.*` 时按"Tag 声明 → 定义 → C++ 槽位 → 脚本 `REQUIRED_ACTIONS` → 角色绑定 + `ExpectedBindings` → 重跑脚本重建 DA"的顺序改；顺序必须逐项一致。
- **验证方式**：重跑 `setup_pvp_input.py` 后报告槽位与动作一一对应；PIE 中每个动作行为正确；日志绑定数与期望值一致。

---

## 6. 校验、报告与冒烟

### 6.1 编译与编辑器检查

```text
编译目标：PVPDemoEditor
编辑器路径：Project Settings → Gameplay Tags
```

- 编译无 `unresolved external symbol`，说明声明 / 定义成对。
- Gameplay Tag 列表中可见 `InputTag.Move`、`InputTag.Look`、`InputTag.Jump`、`InputTag.Run`，且无多余 `InputTag.*`（避免预造未使用 Tag）。

### 6.2 间接校验（输入链路）

```text
UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
    -Script="<project>/Content/Python/setup_pvp_input.py" -unattended -nosplash -nullrhi
```

- 该脚本不直接读 C++ Tag 符号，但按槽位写入 IA；槽位顺序依赖 `UPVPInputConfig` 构造函数中的 Tag 预置顺序。
- 报告 `Saved/PVPInputReport/report.md`：4 个动作的 ValueType 与键位、`IA_PVP_Look Negate(Y): yes`、`Errors` 为空。

### 6.3 运行时冒烟

- 进入 `L_PVP_Showcase`，日志 `bound 6/6 native input bindings (config=DA_PVP_InputConfig)`。
- 4 个动作行为与 `Docs/Input/README.md` 第 3 节一致（移动 / 视角 / 跳跃 / 跑步）。
- 若新增 Tag 但未接消费方：编辑器中可见 Tag，日志绑定数不变——这属于"定义了未使用"，应在评审时拒绝。

---

## 7. 已知限制与后续路线

**当前限制**

- 无 ASC / AttributeSet / GameplayAbility：没有任何技能、属性、冷却、消耗。
- 无 `AbilitySystemGlobals`：未来 Montage 会默认播放到 Leader Pose / 主网格，1P 手臂需专门处理。
- 无双网格 Montage Task：1P 表现与 3P 复制未分离。
- 无 Cue：音效 / 特效资产未接为 GameplayCue。
- 无 `State.*`：跑步、死亡等状态仍用布尔量（如 `bIsRunning`）表达。
- 无网络：无预测键、无复制属性。

**后续路线（按依赖顺序）**

1. **ASC 宿主**：在 `PlayerState` 上挂 `UAbilitySystemComponent`，由角色初始化并缓存 `AbilityActorInfo`；同步 `Docs/Source-Structure.md` 2.6 节。
2. **属性集**：`UPVPAttributeSet`（血量 / 速度等），把 `WalkSpeed` / `RunSpeed` 的本地写入迁到属性（与 `Character-Dev.md` 第 7 节 `TODO(Network/GAS)` 对齐）。
3. **GameplayAbility 与输入**：新增 `InputTag.Ability.*`，在 `DA_PVP_InputConfig` 扩展技能输入槽位或在 Hero 层绑定；不覆用原生动作 Tag。
4. **Globals 修正**：`PVPAbilitySystemGlobals` 覆写 `AllocAbilityActorInfo`，使用正确的 SkeletalMeshComponent。
5. **双网格 Montage**：`PVPAbilityTask_PlayDualMontage`：本地 1P 表现 + 服务器 3P 复制。
6. **Cue 与 Tag 族**：补齐 `GameplayCue.*` 与 `State.*`，把音效 / 特效接入。

---

## 8. 相关文件

- 源码
  - `Source/PVPDemo/GAS/PVPGameplayTags.h/.cpp`
  - `Source/PVPDemo/Input/PVPInputConfig.h/.cpp`（Tag 槽位预置与查找）
  - `Source/PVPDemo/Character/PVPBaseCharacter.cpp`（按 Tag 绑定）
- 配置与工具
  - `PVPDemo.Build.cs`（`GameplayTags` 已用；`GameplayAbilities` / `GameplayTasks` 已声明未使用）
  - `Content/Python/setup_pvp_input.py`
  - `Saved/PVPInputReport/report.md`
- 相关文档
  - `Docs/GAS/README.md`、`Docs/GAS/GAS-Integration.md`
  - `Docs/Input/README.md` 第 3 节、`Docs/Input/Input-Dev.md` 踩坑 2
  - `Docs/Source-Structure.md` 2.6 节
