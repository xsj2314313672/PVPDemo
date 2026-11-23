---
name: 模块双文档制度与Input文档拆分
overview: 在 Docs 协作流程中固化「每个功能模块一个文件夹 + 固定三件套（导航 README / Dev / Integration）并随代码同步」的制度，明确 AGENTS.md 记录模块文件夹存在性、读取走导航优先按需加载且不无差别扫描模块代码；同步把 Input 单文件文档拆为三份并补齐 Action 绑定回调速查表。
todos:
  - id: workflow-policy
    content: 在 AI-Workflow 第 6 节新增模块文件夹与三件套制度，含读取协议、同步清单与三份模板
    status: completed
  - id: input-triplet
    content: 新建 Input 导航 README（Action 绑定回调权威表）、Input-Dev（四条踩坑）、Input-Integration，核对后删除原文件
    status: completed
    dependencies:
      - workflow-policy
  - id: agents-nav
    content: 在 AGENTS.md 登记模块文档目录层，更新按需拉取协议、硬约束与收尾判定
    status: completed
    dependencies:
      - input-triplet
  - id: sync-refs
    content: 同步 RULE 导航、根 README 目录树及 Source、Content 结构文档引用
    status: completed
    dependencies:
      - agents-nav
  - id: verify-docs
    content: 校验旧文件引用零残留、模板一致、动作表与源码一致、交叉链接与行数合规
    status: completed
    dependencies:
      - sync-refs
---

## 用户需求

在 `Docs/` 的 AI 协作规范中固化「模块文档体系」，并把现有 Input 模块改造为首个符合规范的范例。

## 产品概述

本次为纯文档与规则建设，不涉及界面与视觉。目标是让 agent 进入任一功能模块时，先通过固定入口了解模块全貌与契约，再按需定点读取，避免重复踩坑、接口误用和整目录盲扫。

## 核心功能

- **每模块一个文件夹**：新建功能模块时在 `Docs/` 下创建 `Docs/{模块}/`，内含固定三件套 `README.md`（导航）、`{模块}-Dev.md`（开发与踩坑）、`{模块}-Integration.md`（依赖与接口）；允许追加 `<模块>-<主题>.md` 等维护文档，但必须登记在导航中，不允许孤儿文档。
- **导航文档固定命名**：模块目录一律以 `README.md` 作为导航入口，内容为模块定位与边界、文档清单与阅读顺序、「我要做 X 读哪份哪节」路由、已实现能力速查、启动与校验入口、维护规则。
- **开发文档**：记录范围与当前状态、源码结构、设计决策、开发流程 SOP、踩坑清单（现象 / 根因 / 正确做法 / 验证方式）、校验与冒烟、已知限制。
- **接口文档**：记录模块边界、外部依赖、数据流与交互、对外接口（C++ / 资产 / 配置）、其他模块调用方式与顺序、禁止的依赖方向、变更影响面。
- **AGENTS.md 显式登记模块层**：除原有规范文档与架构说明外，新增「模块文档目录」区块，使 agent 知道可以按模块找文档；读取协议改为「先导航、再按需」，无特殊需求不得全量扫描模块源码或资产。
- **实时同步**：新模块首次实现必须一次性建夹建三件套；已有模块发生对外接口、依赖方向、数据流、资产契约变化时同轮更新导航能力表与接口文档；出现新踩坑当轮追加开发文档。
- **Input 范例**：原 `Input-System.md` 拆为三份；导航文档内维护当前 4 个已实现 Action、6 条绑定事件、回调函数、绑定位置与关键日志的权威速查表，其他文档不再复制该表。

## Tech Stack Selection

- 文档格式：Markdown，沿用 `Docs/` 现有中文写作风格与表格体例，不引入新工具、脚本或外部依赖。
- 规则正文：`Docs/AI-Workflow.md` 为唯一来源；`AGENTS.md` 承担文档地图与模块目录登记；`.codebuddy/rules/doc-first/RULE.mdc` 仅做导航，不写制度正文。
- 校验方式：`search_content` / `read_file` 完成引用零残留、章节模板一致性、表格事实与源码一致、交叉链接可达、单文档行数检查；不编译、不改资产。

## Implementation Approach

1. **章节编号稳定**：`AGENTS.md` 第 5 节引用「第 3 节清单」、`RULE.mdc` 第 3 节引用「第 2～4 节」，因此 `AI-Workflow.md` 第 1～5 节编号保持不变；新增第 6 节「模块文档制度」，原第 6 节「相关文档」顺延为第 7 节。
2. **三件套职责切分（单一事实来源）**：导航 `README.md` 回答「模块是什么、有哪些文档、已实现什么能力、去哪校验」；`Dev` 回答「怎么实现、怎么开发、踩过什么坑」；`Integration` 回答「依赖谁、暴露什么、别人怎么调」。同一事实只定义一次，其余位置用链接与节号引用。
3. **导航优先的读取协议**：进入模块任务先读该模块 `README.md`，由路由表决定读哪一节；仅当文档未覆盖、疑似与代码不一致、或需要精确签名时，才用 `search_content` / 精准 `read_file` 定点核对。禁止为「熟悉模块」而整目录通读源码或资产，这是本次制度的核心收益点。
4. **易变数字去重**：资产总数、文件总数只保留在 `Content-Structure.md` / `Source-Structure.md`；模块文档只写语义、路径与契约。行为契约中的固定数量（4 个已实现动作、6 条绑定事件）写在导航表内，变化时同轮更新。
5. **模板内嵌**：制度正文直接给出三份固定章节骨架，Input 三份文档作为活范例，不新增 `_Templates/` 目录，减少新增维度与维护面。
6. **存量改造**：按章节映射把 `Input-System.md` 的 17 节拆入三份文档，补齐独立踩坑清单，核对无信息丢失后删除原文件，并同步全部引用。
7. **可靠性**：三份文档文首固定元信息（维度 / 模块 / 适用工程 / 最后更新 / 上游规范 / 配套文档），收尾刷新日期并输出「文档同步」。

## Implementation Notes

- **固定命名**：`Docs/{模块}/README.md`、`Docs/{模块}/{模块}-Dev.md`、`Docs/{模块}/{模块}-Integration.md`；附加文档命名 `{模块}-<主题>.md` 并登记到 README 的文档清单。
- **建夹时机**：模块首次进入实现阶段（源码分层由空变实或新增已登记分层）时才创建 `Docs/{模块}/`，未进入实现的预留模块（Camera / Animation / Equipment 等）不提前建空目录，与现有阶段约束一致。
- **三份固定章节**：见「Key Doc Structures」，后续模块直接复制骨架。
- **踩坑四要素**：现象、根因、正确做法、验证方式，缺一不成立；禁止只写「注意」。
- **AGENTS.md 精确改动**：第 2 节文档地图保留原有四行并更新 Input 指向，新增「模块文档目录」子块（模块 / 文件夹 / 导航 / 固定文档 / 现状）；第 3 节按需拉取协议新增「定位模块 → 先读模块 README」与「禁止无差别全量扫描」；第 4 节硬约束补一条模块文档产出义务；第 5 节收尾补「新建模块或接口变化时三件套缺失视为任务未完成」。
- **引用同步点**：`AGENTS.md` 第 24 行原 Input 单行、`Source-Structure.md` 第 137 行、`Content-Structure.md` 第 222 行与第 271 行、根 `README.md` 第 17 行目录树与第 37～45 行 AI 协作规范、`RULE.mdc` 第 1 节维度列表。
- **输入事实核对**：导航表函数名、事件名、日志字符串必须逐项对照 `PVPBaseCharacter` / `PVPPlayerController` / `PVPInputComponent` 真实代码，不得沿用文档旧描述。
- **边界控制**：不改 `Source/`、`Content/`、`Config/`，不生成或修改 `.uasset`；每份文档控制在约 500 行以内。

## Architecture Design

- **入口层**：`AGENTS.md` 文档地图 + 模块文档目录 + `RULE.mdc` 常驻导航，只回答「何时读哪份」。
- **规则层**：`Docs/AI-Workflow.md` 承载触发判定、同步清单、三件套制度、读取协议与模板。
- **模块层**：`Docs/{模块}/README.md` 为固定入口；`{模块}-Dev.md` 记录实现与踩坑；`{模块}-Integration.md` 记录依赖与接口契约；附加文档由 README 登记。
- **全局索引层**：`Source-Structure.md`、`Content-Structure.md` 保留目录、数量与实现状态；模块文档只链接不复述数字。
- **更新链路**：代码 / 资产变化 → 第 2 节判定触发 → 同轮更新对应模块文档 → 刷新「最后更新」→ 收尾输出「文档同步」。

## Directory Structure

```text
PVPDemo/
├── AGENTS.md                                  [MODIFY] 第 2 节新增「模块文档目录」登记块并更新 Input 指向；第 3 节增加模块导航优先与禁止全量扫描；第 4 节补模块文档义务；第 5 节补缺失即未完成判定
├── README.md                                  [MODIFY] Docs 目录树体现模块文件夹与三件套；AI 协作规范增加「按模块查找」维度说明
├── .codebuddy/
│   └── rules/
│       └── doc-first/
│           └── RULE.mdc                       [MODIFY] 第 1 节维度列表新增「改某功能模块 → 先读 Docs/{模块}/README.md，再按路由按需读取」，保持只导航不写正文
└── Docs/
    ├── AI-Workflow.md                         [MODIFY] 第 1 节补模块读取顺序；第 2 节补三类触发；第 3 节清单增三行；第 5 节补职责边界与行数规则；新增第 6 节制度与模板；原第 6 节顺延为第 7 节并登记新文档
    ├── Source-Structure.md                    [MODIFY] 第 137 行改为指向 Input 导航 + Dev + Integration
    ├── Content-Structure.md                   [MODIFY] 第 222 行 Negate 说明改指 Input-Dev 踩坑；第 271 行相关文档列出三份
    └── Input/
        ├── README.md                          [NEW] 固定导航：模块定位与边界、文档路由表、已实现 4 个 Action / 6 条绑定 / 回调函数 / 绑定位置 / 行为权威表、IMC 注入与日志速查、未实现能力、校验入口、维护规则
        ├── Input-Dev.md                       [NEW] 开发文档：范围与状态、源码结构、设计决策、SOP、四条踩坑（四要素齐全）、校验报告与 PIE 冒烟、已知限制、相关文件
        ├── Input-Integration.md               [NEW] 接口文档：定位与边界、模块与外部依赖、数据流与交互、对外接口（C++ / 资产 / 配置）、其他模块调用方式、禁止依赖方向、变更影响面、相关文件
        └── Input-System.md                    [DELETE] 三份新文档核对无信息丢失后删除，避免双份维护
```

## Key Doc Structures

```text
文首元信息（三份通用）
> 维度 / 模块 / 适用工程 / 最后更新 / 上游规范 / 配套文档

模块 README.md
1 模块定位与边界（职责 / 非职责）
2 文档清单与路由（场景 → 文档 → 章节）
3 已实现能力速查（Action / Tag / IA / ValueType / 键位 / 绑定事件 / 回调函数 / 行为）
4 启动与运行时入口（注入位置、绑定点、关键日志）
5 未实现能力与扩展方向
6 校验与报告入口
7 文档维护规则（新增或修改时更新哪几处）

{模块}-Dev.md
1 目的与范围
2 当前状态与源码结构
3 设计决策与理由
4 开发流程 SOP
5 踩坑清单（现象 / 根因 / 正确做法 / 验证方式）
6 校验、报告与冒烟
7 已知限制与后续路线
8 相关文件

{模块}-Integration.md
1 模块定位与边界（职责 / 非职责）
2 模块与外部依赖（Build.cs / 配置 / 第三方）
3 数据流与交互
4 对外接口（C++ / 资产 / 配置）
5 其他模块调用方式（顺序 / 前置条件 / 示例）
6 禁止的依赖方向
7 变更影响面（改什么要同步谁）
8 相关文件
```