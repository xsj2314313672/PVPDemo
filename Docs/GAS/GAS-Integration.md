# PVP 技能层模块接口与依赖文档

> 维度：技能层（GameplayTag 原生声明；GAS 基础设施未搭建）
> 模块：GAS
> 适用工程：PVPDemo（UE 5.9）
> 最后更新：2026-11-23
> 上游规范：`AGENTS.md`、`Docs/AI-Workflow.md`、`Docs/Source-Structure.md`、`Docs/Content-Structure.md`
> 配套文档：`Docs/GAS/README.md`（导航与 Tag 速查）、`Docs/GAS/GAS-Dev.md`（开发流程与踩坑）

本文只写**依赖谁、暴露什么、别人怎么调用**。Tag 名称与语义的权威表在 `Docs/GAS/README.md` 第 3 节，动作与键位在 `Docs/Input/README.md` 第 3 节。

---

## 1. 模块定位与边界

**职责**：

- 暴露原生 GameplayTag 常量（当前仅 `InputTag.*`），作为工程内 Tag 的唯一声明位置。
- 承担未来 GAS 基础设施的落点（ASC 宿主协作、AbilitySystemGlobals、AbilityTask、Cue），**当前均未实现**。

**非职责**：

- 不定义输入动作、键位、IMC 或输入配置资产（`Input/`）。
- 不持有角色状态（`bIsRunning` / `MaxWalkSpeed` 仍在 `Character/`）。
- 不实现技能表现：Montage、音效、特效由 `Animation/`、`Equipment/`、内容资产承担。
- 不反向依赖任何具体相机 / 动画 / 角色类。

---

## 2. 模块与外部依赖

### 2.1 Build.cs 依赖

| 依赖 | 状态 | 用途 |
| --- | --- | --- |
| `GameplayTags` | **已使用** | `FNativeGameplayTag`、`UE_DEFINE_GAMEPLAY_TAG`、`FGameplayTag` |
| `GameplayAbilities` | 已声明，**未使用** | 为 ASC / GA / AttributeSet 预留；当前代码无引用 |
| `GameplayTasks` | 已声明，**未使用** | AbilityTask（含规划的 `PlayDualMontage`）预留 |

- 三个依赖都已在 `PVPDemo.Build.cs` 中，**本次无新增依赖**。
- 本模块当前没有新增第三方依赖。

### 2.2 配置依赖

- 无 `Config/*.ini` 依赖：原生 Tag 通过 C++ 注册，不读配置。
- 无资产依赖：本模块不引用任何 `.uasset`。
- Tag 的可见性由模块加载保证；若本模块被裁剪（模块不再被引用），Tag 也不会注册。

### 2.3 第三方依赖

- 无。

---

## 3. 数据流与交互

### 3.1 Tag 注册与消费链

```text
PVPGameplayTags.cpp
  UE_DEFINE_GAMEPLAY_TAG(InputTag_Move, "InputTag.Move")  ← 模块加载期注册到全局 Tag 管理器
        │
        ▼
UPVPInputConfig 构造函数（Input/）
  槽位 0..3：InputTag_Move / InputTag_Look / InputTag_Jump / InputTag_Run
        │  setup_pvp_input.py 按索引写入 IA（GameplayTag 无法从 Python 构造）
        ▼
DA_PVP_InputConfig（Content/PVP/Input/，挂 BP_PVPBaseCharacter CDO）
        │
        ▼
APVPBaseCharacter::SetupPlayerInputComponent
  UPVPInputComponent::BindNativeAction(Config, Tag, Event, ...)
        │  FindNativeInputActionForTag(Tag) → IA
        ▼
Enhanced Input 按键 → IA → 绑定事件 → APVPBaseCharacter 回调
```

### 3.2 依赖方向

```text
GAS（Tag 定义）
  ▲                ▲
  │                │
Input/ ────────────┘   Character/（通过 Input/ 间接依赖 Tag）
  ▲
  │
Game/（注入 IMC，不直接消费 Tag）
```

- `Input/` 与 `Character/` 单向依赖 `GAS/PVPGameplayTags.h`。
- `GAS/` 不依赖任何工程内其他分层；未来 GAS 实现可依赖 `Engine` / `GameplayAbilities` 的通用类型，但不得依赖 `Camera/`、`Animation/` 具体类。

### 3.3 与输入模块的契约切分

| 层 | 负责 | 不负责 |
| --- | --- | --- |
| `GAS/` | Tag 字符串、C++ 符号、命名族与语义 | IA 资产、槽位顺序、绑定事件 |
| `Input/` | Tag → IA 配置、绑定组件、槽位顺序 | Tag 定义、语义命名 |
| `Character/` | 按 Tag 查 IA 并绑定回调 | Tag 定义、IA 资产 |

槽位顺序与键位的完整契约见 `Docs/Input/Input-Integration.md` 第 3 / 4.3 节。

---

## 4. 对外接口

### 4.1 C++ 接口（当前唯一）

| 接口 | 位置 | 类型 | 用途 |
| --- | --- | --- | --- |
| `PVPGameplayTags::InputTag_Move` | `GAS/PVPGameplayTags.h` | `extern PVPDEMO_API FNativeGameplayTag` | 平面移动语义，供配置槽位与绑定查询 |
| `PVPGameplayTags::InputTag_Look` | 同上 | 同上 | 视角 Yaw / Pitch 语义 |
| `PVPGameplayTags::InputTag_Jump` | 同上 | 同上 | 起跳 / 停止起跳语义 |
| `PVPGameplayTags::InputTag_Run` | 同上 | 同上 | 按住跑语义 |

**规则**：

1. Tag 只表达语义：不写按键名、不写资产路径、不写数值。
2. 原生 Tag 一律用 `extern PVPDEMO_API FNativeGameplayTag` 声明 + `UE_DEFINE_GAMEPLAY_TAG` 定义；不用 `FGameplayTag` 静态常量，不在运行期拼字符串。
3. 对外传递使用 `FGameplayTag`（`FNativeGameplayTag` 可隐式转换），接收方不应假设 Tag 来自哪个命名族。
4. 新增 Tag 只增加实际被消费的条目；未被消费的 Tag 在评审时移除。

### 4.2 规划中的接口（未实现，不得提前调用）

| 规划接口 | 预定位置 | 状态 |
| --- | --- | --- |
| `UPVPAbilitySystemComponent` / ASC 宿主 | `PlayerState` 上挂载，由角色初始化 | 未实现 |
| `UPVPAttributeSet`（血量 / 速度等） | `GAS/` | 未实现 |
| `UPVPAbilitySystemGlobals`（修正 `AllocAbilityActorInfo` 的网格） | `GAS/` | 未实现 |
| `UPVPAbilityTask_PlayDualMontage`（1P 表现 + 3P 复制） | `GAS/` | 未实现 |
| `InputTag.Ability.<Skill>` | `GAS/PVPGameplayTags` | 未定义 |
| `GameplayCue.*` / `State.*` | `GAS/PVPGameplayTags` | 未定义 |

- 上表仅用于规划对齐；当前工程内不存在这些符号，引用即编译失败。

### 4.3 资产与配置接口

| 项 | 是否由本模块提供 | 说明 |
| --- | --- | --- |
| `DA_PVP_InputConfig` | 否 | `Input/` 资产，槽位由 `UPVPInputConfig` 构造函数按 Tag 预置 |
| `IA_PVP_*` / `IMC_PVP_Default` | 否 | `Input/` 资产 |
| GameplayCue 资产 | 否 | 规划中，音效 / 特效素材位于 `Content/PVPDemo/Audio`、`Content/PVPDemo/VFX` |

---

## 5. 其他模块调用方式

### 5.1 `Input/` 如何消费

- `UPVPInputConfig::UPVPInputConfig()` 直接引用 4 个 Tag 建立槽位，顺序为 Move / Look / Jump / Run。
- `UPVPInputConfig::FindNativeInputActionForTag(const FGameplayTag&)` 以 Tag 相等匹配 IA。
- 新增 Tag 时本模块要先提供符号，`Input/` 才能编译；顺序上 Tag 定义先行。

### 5.2 `Character/` 如何消费

```cpp
PVPInputComponent->BindNativeAction(InputConfig, PVPGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
```

- 绑定只按 Tag 查 IA，不关心 Tag 字符串内容；Tag 改名后重新编译即可，不需要改绑定调用点。
- 角色不得自行定义 Tag 或写死字符串。

### 5.3 脚本如何间接依赖

- `Content/Python/setup_pvp_input.py` **无法构造 GameplayTag**（Tag 名称对 Python 不透明），只能通过 `SetNativeInputActionByIndex` 按索引写 IA。
- 因此脚本的 `REQUIRED_ACTIONS` 顺序必须与 `UPVPInputConfig` 构造函数中的 Tag 顺序一致；新增 / 调整槽位时两侧同改。

### 5.4 未来 GAS 能力的调用方式（未实现）

1. ASC 挂在 `PlayerState`，角色 `Possess` 后初始化 `AbilityActorInfo`。
2. 技能授予与输入绑定在 Hero / ASC 层完成，使用 `InputTag.Ability.*`。
3. 表现层（Animation / Camera）通过 Tag 或属性只读驱动，不反向调用 GAS 逻辑。
4. 速度、血量等属性替换当前 `Character/` 的本地写入（`TODO(Network/GAS)`）。

---

## 6. 禁止的依赖方向

- `GAS/` 不得包含 `Input/`、`Character/`、`Game/`、`Camera/`、`Animation/`、`Equipment/` 的头文件；Tag 模块保持零工程内依赖。
- 任何模块不得自建 GameplayTag 常量或运行期拼字符串代替 `PVPGameplayTags`。
- `Input/`、`Character/` 不得把 Tag 字符串写进绑定逻辑（只引用符号）。
- 表现层不得通过 Tag 反向驱动输入或角色状态；Tag 只作为只读的语义标识。
- 不允许提前引入 ASC / GA 代码"占位"：未实现的 GAS 规划不得以空类或伪接口形式落地。

---

## 7. 变更影响面

| 改动 | 必须同步 |
| --- | --- |
| Tag 重命名 / 字符串变化 | `PVPGameplayTags.cpp`、`UPVPInputConfig` 槽位、所有绑定调用点、`setup_pvp_input.py`、`Docs/GAS/README.md` 第 3 节、`Docs/Input/README.md` 第 3 节 |
| 新增 `InputTag.*` | C++ 槽位顺序、脚本 `REQUIRED_ACTIONS`、`ExpectedBindings`、`DA_PVP_InputConfig` 重建、`Docs/Input/Input-Integration.md` 第 4.3 节 |
| 新增 Tag 族（`State.*` / `GameplayCue.*` / `InputTag.Ability.*`） | `Docs/GAS/README.md` 第 3 / 5 节、`Docs/Source-Structure.md` 2.6 节、`AGENTS.md` 现状描述 |
| 引入 ASC / AttributeSet / GA | 本文第 4.2 / 5.4 节、`GAS-Dev.md` 第 3 / 7 节、`Docs/Source-Structure.md` 2.6 节、角色速度与状态文档（`Docs/Character/`） |
| `PVPDemo.Build.cs` 依赖调整 | `Docs/Source-Structure.md` 第 3 节依赖结论、本文第 2.1 节 |
| 模块被裁剪 / 改名 | Tag 注册失效、绑定全断；同步 `Docs/Source-Structure.md` 与 `AGENTS.md` |

- 任何一项改动完成后，同时刷新 `Docs/GAS/` 三份文档文首的「最后更新」。
- 源码文件变化同步 `Docs/Source-Structure.md`；资产变化同步 `Docs/Content-Structure.md`。

---

## 8. 相关文件

- 源码
  - `Source/PVPDemo/GAS/PVPGameplayTags.h/.cpp`
  - `Source/PVPDemo/Input/PVPInputConfig.h/.cpp`（槽位与查找）
  - `Source/PVPDemo/Character/PVPBaseCharacter.cpp`（绑定调用点）
- 配置与工具
  - `PVPDemo.Build.cs`
  - `Content/Python/setup_pvp_input.py`
- 相关文档
  - `Docs/GAS/README.md`、`Docs/GAS/GAS-Dev.md`
  - `Docs/Input/Input-Integration.md` 第 4.3 节
  - `Docs/Source-Structure.md` 2.6 节
