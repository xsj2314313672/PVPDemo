---
name: 模块双文档制度与Input文档拆分
overview: 在 Docs 协作流程中新增「每个功能模块固定产出导航 README + Dev + Integration 三份文档并随代码同步更新」的制度；同步把 Input 单文件文档拆分为三份，导航文档内维护已实现 Action / 绑定事件 / 回调函数速查表，并同步全部索引与引用。
todos:
  - id: workflow-policy
    content: 重写 AI-Workflow 模块文档制度，固定 README 导航与 Dev、Integration 模板及触发更新规则
    status: pending
  - id: input-nav
    content: 新建 Docs/Input/README.md，写入已实现 4 个 Action、6 条绑定与回调函数权威表
    status: pending
    dependencies:
      - workflow-policy
  - id: split-input-docs
    content: 拆分 Input 文档为 Dev 与 Integration，补齐四条踩坑并删除 Input-System.md
    status: pending
    dependencies:
      - workflow-policy
  - id: sync-indexes
    content: 同步 AGENTS 地图、RULE 导航、README 目录树及 Source、Content 结构文档引用
    status: pending
    dependencies:
      - input-nav
      - split-input-docs
  - id: verify-docs
    content: 校验引用零残留、模板一致、动作表与源码一致、交叉链接及行数合规
    status: pending
    dependencies:
      - sync-indexes
---

## 用户需求

在 `Docs/` 的 AI 协作规范中新增「模块文档制度」，并同步改造现有 Input 模块文档作为范例。

## 产品概述

本次为纯文档与规则改造，不涉及界面与视觉变化。目标是把「开发一个功能模块后留下可复用经验与稳定契约」固化为流程，让后续 agent 进入模块时先读导航、再按需读开发文档与接口文档，减少重复试错和跨模块误用。

## 核心功能

- **模块导航文档**：每个模块目录固定使用 `README.md` 作为导航入口，包含模块定位、文档清单与阅读顺序、「我要做 X → 读哪份哪节」路由、当前已实现能力速查、校验入口与最后更新时间。
- **开发文档**：`Docs/{模块}/{模块}-Dev.md`，固定记录范围与当前状态、源码结构、设计决策、开发流程 SOP、踩坑清单、校验与冒烟、已知限制、相关文件。
- **接口文档**：`Docs/{模块}/{模块}-Integration.md`，固定记录模块边界、依赖关系、数据流、对外接口、其他模块调用方式、禁止依赖方向、变更影响面。
- **踩坑条目规范**：每条必须写清现象、根因、正确做法、验证方式，禁止只写「注意」。
- **触发与实时更新**：新建模块必须首次产出三份文档；已有模块发生对外接口、依赖方向、数据流或资产契约变化时，同轮更新导航能力表与接口文档；出现新踩坑当轮追加开发文档。
- **Input 范例改造**：把现有单文件 `Input-System.md` 拆为导航、开发、接口三份；导航文档维护当前 4 个已实现 Action、6 条绑定事件、回调函数与绑定位置的权威速查表；开发文档补齐四条可验证踩坑。
- **索引同步**：`AGENTS.md` 文档地图、常驻规则导航、根 `README.md`、源码与资产结构文档统一指向新文档，删除旧文件避免双份维护。

## 技术栈选择

- 文档格式：Markdown，沿用现有 `Docs/` 目录结构、中文写作风格与表格体例，不引入新工具或外部依赖。
- 规则正文：`Docs/AI-Workflow.md` 为唯一来源；`AGENTS.md` 维护文档地图与收尾要求；`.codebuddy/rules/doc-first/RULE.mdc` 仅做导航，不承载正文。
- 校验手段：`search_content`、`read_file` 做引用零残留、章节模板、表格事实、交叉链接与行数检查；不编译、不改资产。

## 实施方案

1. **保持章节编号稳定**：`AGENTS.md` 引用「第 3 节清单」、`RULE.mdc` 引用「第 2～4 节」，因此 `AI-Workflow.md` 第 1～5 节编号不变；新增第 6 节「模块文档制度」，原第 6 节「相关文档」顺延为第 7 节。
2. **三份文档单一事实来源分工**：导航 `README.md` 负责「模块是什么、有哪些文档、已实现什么能力」；`Dev` 负责「怎么实现、怎么开发、踩过什么坑」；`Integration` 负责「依赖谁、暴露什么接口、别人怎么调用」。同一事实只在一处定义，其余位置链接引用。
3. **易变数字去重**：资产总数、文件总数只保留在 `Content-Structure.md` / `Source-Structure.md`；模块文档只写语义、路径与契约，必要时链接全局文档。行为契约中的固定数量（如 4 个已实现动作、6 条绑定事件）可写在导航表内，变化时同轮更新。
4. **模板内嵌**：制度正文直接给出三份文档的固定章节骨架，`Docs/Input/` 三份文档作为活范例，不新增 `_Templates/` 目录，减少维护面。
5. **存量拆分**：按章节映射把 `Input-System.md` 的 17 节内容拆入三份文档，原文件删除，并同步 4 处既有引用。
6. **收尾闭环**：三份文档文首固定「维度 / 模块 / 适用工程 / 最后更新 / 上游规范 / 配套文档」；收尾时刷新日期并输出「文档同步」。

## 实施要点

### AI-Workflow 改造细节

- 第 1 节「每次任务开始前」第 2 步补充：涉及功能模块时先读 `Docs/{模块}/README.md`，再按导航读 `Dev` / `Integration`。
- 第 2 节「必须同步文档的判定标准」新增三类触发：新模块首次实现、模块契约变化、新增踩坑。
- 第 3 节「文档同步清单」新增三行：模块 `README.md`、`{模块}-Dev.md`、`{模块}-Integration.md`。
- 第 5 节「上下文维护」补充：三份文档职责边界、数量去重、单文档约 500 行拆分规则。
- 新增第 6 节：6.1 触发与产出、6.2 固定命名与目录、6.3 职责与事实归属表、6.4 三份固定模板、6.5 更新时机、6.6 与全局文档分工、6.7 Input 范例。
- 第 7 节「相关文档」登记 Input 三份文档。

### Input 文档内容映射

- 导航 `README.md`：定位与边界、文档路由表、已实现动作与绑定权威表、IMC 注入与日志速查、未实现能力、校验入口、文档维护规则。
- `Input-Dev.md`：原 2.1/2.2/4/8/9/13/14/16 节 + 新增踩坑清单；原 10 节保留实现视角的事件与处理函数说明，动作契约链接导航表。
- `Input-Integration.md`：原 6/7/12/15 节 + 原 3 节资产契约 + 原 5.1 Tag 表；新增「其他模块调用方式」与「变更影响面」。
- 删除 `Input-System.md` 前逐节核对，确保无信息丢失。

### Input 已实现能力事实（已核实源码）

| 语义 | Tag | IA | ValueType | 键位 | 绑定事件 | 回调函数 |
| --- | --- | --- | --- | --- | --- | --- |
| 平面移动 | `InputTag.Move` | `IA_PVP_Move` | Axis2D | W/A/S/D | Triggered | `APVPBaseCharacter::Input_Move` |
| 视角 | `InputTag.Look` | `IA_PVP_Look` | Axis2D | Mouse2D + Negate(Y) | Triggered | `APVPBaseCharacter::Input_Look` |
| 跳跃 | `InputTag.Jump` | `IA_PVP_Jump` | Boolean | SpaceBar | Started / Completed | `Input_JumpStarted` / `Input_JumpCompleted` |
| 按住跑 | `InputTag.Run` | `IA_PVP_Run` | Boolean | LeftShift | Started / Completed | `Input_RunStarted` / `Input_RunCompleted` |


- 绑定调用：`UPVPInputComponent::BindNativeAction`；绑定点：`APVPBaseCharacter::SetupPlayerInputComponent`；共 4 动作 / 6 事件。
- InputComponent 覆盖：构造函数 `OverrideInputComponentClass = UPVPInputComponent::StaticClass()`。
- IMC 注入：`APVPPlayerController` 构造函数加载 `/Game/PVP/Input/IMC_PVP_Default`，`BeginPlay` 中 `ClearAllMappings` 后按优先级 `AddMappingContext`。
- 日志契约：`bound 6/6 native input bindings (config=DA_PVP_InputConfig)`、`applied 1 default input mapping context(s)`。
- 未绑定：Fire / ADS / Crouch / Sprint 等其余 IA 已在 IMC 中，代码未绑定；不写具体总数。

### 四条踩坑（写入 `Input-Dev.md`，均含四要素）

1. **Look 俯仰极性反向**：现象为上下相反；根因是 `bUsePawnControlRotation` 下 Controller Pitch 方向与鼠标约定相反；正确做法是仅在 IMC 的 `IA_PVP_Look` 上加 Negate(Y)，C++ 保持 `AddControllerPitchInput(LookAxisVector.Y)` 不取反；验证为脚本报告 `IA_PVP_Look Negate(Y): yes` 且 PIE 四向检查。
2. **DA 槽位顺序错位**：现象为脚本写入后 IA 与 Tag 不匹配；根因是 GameplayTag 不能由脚本构造，C++ 构造函数按 Move / Look / Jump / Run 预置槽位，脚本 `REQUIRED_ACTIONS` 按索引写入；正确做法是两处顺序必须一致，改顺序时同改 C++ 与脚本并重建资产；验证为报告槽位数与动作一一对应、绑定日志 6/6。
3. **InputComponent 类型不符**：现象为 Cast 失败并打印 `expected UPVPInputComponent`；根因是试图改全局 `DefaultInputComponentClass` 会影响所有 Pawn；正确做法是使用 `APawn::OverrideInputComponentClass`；验证为不再出现该警告且绑定成功。
4. **IMC 重复注入**：现象为映射上下文重复或切换 Pawn 后输入异常；根因是蓝图与 PlayerController 同时 `AddMappingContext`，且 `BeginPlay` 的 `ClearAllMappings` 会清掉蓝图注入；正确做法是仅由 `APVPPlayerController` 注入，蓝图不手动添加；验证为日志 `applied 1` 且 PIE 动作正常。

### 索引与引用同步

- `AGENTS.md` 第 24 行单行 Input 登记替换为三条通用登记：模块导航 `README.md`、`{模块}-Dev.md`、`{模块}-Integration.md`；第 5 节补「新模块或接口变化时三份文档缺失视为任务未完成」。
- `.codebuddy/rules/doc-first/RULE.mdc` 第 1 节增加一行「新增 / 修改功能模块 → 先读模块 `README.md`，再按导航读 Dev / Integration」，不写制度正文。
- `README.md` 第 17 行 Docs 目录树说明加入模块三份文档；第 37～45 行 AI 协作规范列表增加模块文档维度。
- `Docs/Source-Structure.md` 第 137 行改为同时指向导航、Dev、Integration。
- `Docs/Content-Structure.md` 第 222 行 Negate 说明改指 `Input-Dev.md` 踩坑；第 271 行相关文档列出三份。
- 全库仅这 4 处引用 `Input-System`，删除旧文件后必须零残留。

### 可靠性控制

- 不改 `Source/`、`Content/`、`Config/`，不生成或修改 `.uasset`。
- 文档描述必须与源码一致；导航表函数名、事件名、日志字符串逐项对照源码。
- 每份文档控制在约 500 行以内，超出时按维度继续拆分。
- 删除旧文件前先完成三份新文档并核对章节覆盖，避免信息丢失。

## 架构设计

- **入口层**：`AGENTS.md` 文档地图 + `RULE.mdc` 常驻导航，只回答「何时读哪份」。
- **规则层**：`Docs/AI-Workflow.md` 承载触发判定、同步清单、三文档制度与固定模板。
- **模块层**：`Docs/{模块}/README.md` 为固定导航入口；`{模块}-Dev.md` 记录实现与踩坑；`{模块}-Integration.md` 记录依赖与接口契约。
- **全局索引层**：`Source-Structure.md`、`Content-Structure.md` 保留目录、数量与实现状态；模块文档只链接不复述数字。
- **更新链路**：代码 / 资产变化 → 第 2 节判定触发 → 同轮更新对应模块文档 → 刷新「最后更新」→ 收尾输出「文档同步」。

## 目录结构

```text
PVPDemo/
├── AGENTS.md                                  [MODIFY] 第 24 行文档地图改为模块三文档通用登记；第 5 节补充缺失判定
├── README.md                                  [MODIFY] Docs 目录树加入模块三份文档；AI 协作规范增加模块文档维度
├── .codebuddy/
│   └── rules/
│       └── doc-first/
│           └── RULE.mdc                       [MODIFY] 按维度拉取列表新增模块导航行，保持只导航不写正文
└── Docs/
    ├── AI-Workflow.md                         [MODIFY] 第 1/2/3/5 节补充模块文档规则；新增第 6 节制度与模板；相关文档顺延第 7 节
    ├── Source-Structure.md                    [MODIFY] 第 137 行输入层引用改为导航 + Dev + Integration
    ├── Content-Structure.md                   [MODIFY] 第 222 行改指 Dev 踩坑；第 271 行列出三份文档
    └── Input/
        ├── README.md                          [NEW] 固定命名导航文档：模块定位、文档路由、已实现 Action / 绑定事件 / 回调函数权威表、注入与日志速查、未实现能力、校验入口、维护规则
        ├── Input-Dev.md                       [NEW] 开发文档：范围与状态、源码结构、设计决策、SOP、四条踩坑、校验与 PIE 冒烟、已知限制、相关文件
        ├── Input-Integration.md               [NEW] 接口文档：定位与边界、依赖、数据流、C++ / 资产 / 配置接口、其他模块调用方式、禁止依赖方向、变更影响面、相关文件
        └── Input-System.md                    [DELETE] 三份新文档核对无信息丢失后删除，避免双份维护
```

## 关键文档结构

```text
文首元信息（三份通用）
> 维度 / 模块 / 适用工程 / 最后更新 / 上游规范 / 配套文档

模块 README.md 固定章节
1 模块定位与边界
2 文档路由（场景 → 文档 → 章节）
3 已实现能力速查（Action / Tag / IA / 键位 / 绑定事件 / 回调函数 / 行为）
4 启动与运行时入口（注入位置、绑定位置、关键日志）
5 未实现能力与扩展方向
6 校验与报告入口
7 文档维护规则

{模块}-Dev.md 固定章节
1 目的与范围
2 当前状态与源码结构
3 设计决策与理由
4 开发流程 SOP
5 踩坑清单（现象 / 根因 / 正确做法 / 验证方式）
6 校验、报告与冒烟
7 已知限制与后续路线
8 相关文件

{模块}-Integration.md 固定章节
1 模块定位与边界（职责 / 非职责）
2 模块与外部依赖（Build.cs / 配置 / 第三方）
3 数据流与交互
4 对外接口（C++ / 资产 / 配置）
5 其他模块调用方式（顺序 / 前置条件 / 示例）
6 禁止的依赖方向
7 变更影响面
8 相关文件
```