---
name: 模块双文档制度与Input文档拆分
overview: 在 Docs 协作流程中新增「每开发一个功能模块必须产出 Dev + Integration 两份文档并随代码同步更新」的制度，同时把现有 Input 单文件文档拆分为 Dev / Integration 两份作为首个范例，并同步所有引用与索引。
todos:
  - id: workflow-policy
    content: 在 AI-Workflow 新增模块双文档制度，补齐触发、同步清单与维护规则
    status: pending
  - id: split-input-docs
    content: 拆分 Input 文档为 Dev 与 Integration，补四条踩坑并删除原文件
    status: pending
    dependencies:
      - workflow-policy
  - id: sync-indexes-refs
    content: 同步 AGENTS 地图、常驻规则导航、README 及两份结构文档引用
    status: pending
    dependencies:
      - split-input-docs
  - id: verify-docs
    content: 全库校验引用零残留、模板一致、交叉链接与行数合规
    status: pending
    dependencies:
      - sync-indexes-refs
---

## User Requirements

- 在 `Docs/` 现有提示词与协作规范体系中新增「模块双文档」制度：每开发一个新功能模块（如输入系统），必须产出两份文档。
- 开发模块文档：写清开发流程、关键设计决策、踩过的坑及验证方式，避免后续 agent 重复试错。
- 依赖与接口文档：写清模块边界、依赖关系、数据流、对外接口、其他模块如何调用，以及禁止的依赖方向。
- 两份文档随代码实时更新：接口、依赖、数据流、资产契约变化时同轮同步；新踩坑当轮追加。
- 现有 Input 模块同步改造为首个范例，后续 Camera、GAS、Equipment 等模块照抄。

## Product Overview

- 产出为 Markdown 文档与索引更新，无界面视觉变化。
- 每个模块在 `Docs/{模块}/` 下形成一对文档：`{模块}-Dev.md` 与 `{模块}-Integration.md`；`AGENTS.md` 文档地图、常驻规则导航、README 与源码/资产结构文档统一指向新文件。
- 目标是让 agent 修改模块前先读历史踩坑与调用契约，减少重复探索、接口误用与跨模块回归。

## Core Features

- 触发判定：新建模块（预留目录转为实现）必须首次产出两份文档；已有模块发生对外接口、依赖方向、数据流或资产契约变化时必须同步更新。
- Dev 固定章节：目的与范围、当前状态与源码结构、设计决策、开发流程 SOP、踩坑清单（现象/根因/正确做法/验证）、校验与冒烟、已知限制、相关文件。
- Integration 固定章节：定位与边界、模块依赖、数据流与交互、对外接口（C++/资产/配置）、其他模块调用方式、禁止依赖方向、变更影响面、相关文件。
- 维护规则：同一轮改动内更新并刷新文首「最后更新」；同一事实只在一处定义，另一处链接；模块文档不重复资产/文件总数等易变数字。
- Input 范例：拆分现有单文件，补齐四条可验证踩坑（Look 俯仰极性、DA 槽位顺序、InputComponent 覆盖方式、IMC 重复注入）。

## Tech Stack Selection

- 文档格式：Markdown，沿用仓库现有 `Docs/` 目录与中文写作风格，不引入新工具或依赖。
- 规则正文：`Docs/AI-Workflow.md` 作为唯一来源；`AGENTS.md` 维护文档地图；`.codebuddy/rules/doc-first/RULE.mdc` 只做导航。
- 校验手段：`search_content` / `read_file` 做引用零残留、章节模板、交叉链接与行数检查；不编译、不改资产。

## Implementation Approach

1. 制度落在 `Docs/AI-Workflow.md`：第 1 节增加「改模块先读双文档」、第 2 节增加触发判定、第 3 节同步清单增加两行、第 5 节增加维护规则，并新增第 6 节承载完整制度与模板；原第 6 节「相关文档」顺延为第 7 节。
2. 编号稳定性优先：第 1～5 节编号不变，避免 `AGENTS.md` 的「第 3 节清单」与 `RULE.mdc` 的「第 2～4 节」引用失效；这是选择新增第 6 节而不是插入第 3 节的原因。
3. 双文档职责切分：Integration 定义契约（依赖、接口签名、资产路径、Tag 与 IA 映射、调用顺序、禁止方向）；Dev 定义实现（源码结构、绑定事件与处理函数、SOP、踩坑、校验）。同一事实只在一处定义，另一处用链接引用，避免两份文档漂移。
4. 数量去重：资产总数、文件数等易变统计只保留在 `Content-Structure.md` / `Source-Structure.md`；模块文档只写语义、路径与契约，必要时链接全局文档。行为契约中的固定数量（如 4 个原生动作、6 个绑定事件）可保留，变化时同轮更新。
5. 模板内嵌而非新增 `_Templates/`：制度正文直接给出固定章节骨架，Input 双文档作为活范例，减少新增文件维度与维护面。
6. 存量改造：按章节映射把 `Input-System.md` 的 17 节内容拆入两份文档，新增独立踩坑清单，删除原文件，并同步 4 处引用。
7. 可靠性：每份文档文首固定「维度 / 模块 / 适用工程 / 最后更新 / 上游规范 / 配套文档」，收尾时更新日期并输出「文档同步」。

## Implementation Notes

- 固定章节骨架（写入 AI-Workflow 第 6 节，供后续模块复制）：
- Dev：1 目的与范围；2 当前状态与源码结构；3 设计决策与理由；4 开发流程 SOP；5 踩坑清单；6 校验、报告与冒烟；7 已知限制与后续路线；8 相关文件。
- Integration：1 定位与边界（职责/非职责）；2 模块与外部依赖（Build.cs/配置/第三方）；3 数据流与交互；4 对外接口（C++/资产/配置）；5 其他模块调用方式（顺序/前置条件/示例）；6 禁止的依赖方向；7 变更影响面；8 相关文件。
- 踩坑条目必须四要素齐全：现象、根因、正确做法、验证方式；禁止只写「注意」。Input 至少落 4 条：Look 俯仰极性（IMC Negate(Y) 且 C++ 不取反）、DA 槽位顺序（C++ 构造函数与 `setup_pvp_input.py` 的 `REQUIRED_ACTIONS` 一致）、InputComponent 覆盖（用 `APawn::OverrideInputComponentClass`，不改全局 `DefaultInputComponentClass`）、IMC 重复注入（仅由 `APVPPlayerController` 注入，蓝图不重复 AddMappingContext）。
- 章节迁移映射：原 2.1/2.2/4/8/9/13/14/16 归 Dev；原 6/7/12/15 归 Integration；原 3 拆为 Integration 资产契约与 Dev 资产准备步骤；原 5 的 Tag 表归 Integration、声明规则归 Dev；原 10 拆为 Integration 契约表（Tag/IA/ValueType/键位/触发事件）与 Dev 实现表（事件/处理函数/行为）；原 11 完整落 Dev 踩坑，Integration 保留一行约束链接；原 17 按源码/资产/报告分列两份。
- 引用同步：`AGENTS.md` 第 24 行单行改为两行通用登记（以 Input 为例）；`Source-Structure.md` 第 137 行改为同时指向两份；`Content-Structure.md` 第 222 行指向 Dev 踩坑、第 271 行列出两份。
- 索引与导航：`RULE.mdc` 只在按维度拉取列表增加一行「新增/修改功能模块 → 模块 Dev 与 Integration」，不写制度正文；`README.md` 更新 Docs 目录树与「AI 协作规范」列表。
- 收尾：`AGENTS.md` 第 5 节补一句「新模块首次实现或接口变化时两份文档缺失视为任务未完成」。
- 不改 `Source/`、`Content/`、`Config/`，不生成或修改 `.uasset`；删除原 `Input-System.md` 前逐节核对新文档已覆盖，避免信息丢失。

## Architecture Design

- 入口层：`AGENTS.md` 文档地图 + `RULE.mdc` 常驻导航，只回答「何时读哪份」。
- 规则层：`Docs/AI-Workflow.md` 承载触发判定、同步清单、模块文档制度与模板。
- 模块层：`Docs/{模块}/{模块}-Dev.md` 记录实现与踩坑；`{模块}-Integration.md` 记录依赖与接口契约。
- 全局索引层：`Source-Structure.md`、`Content-Structure.md` 保留目录、数量与实现状态，模块文档只链接不复述数字。
- 产物层：`Saved/` 报告作为校验证据，模块文档引用报告路径而不复制易变结果。
- 更新链路：代码/资产变化 → 第 2 节判定触发 → 同轮更新 Dev/Integration 对应章节 → 刷新「最后更新」→ 收尾「文档同步」。

## Directory Structure

```
PVPDemo/
├── AGENTS.md                          [MODIFY] 文档地图输入单行改为 Dev/Integration 两行通用登记；第 5 节收尾补模块文档缺失判定。
├── README.md                          [MODIFY] Docs 目录树说明与「AI 协作规范」按维度列表加入模块双文档。
├── .codebuddy/
│   └── rules/
│       └── doc-first/
│           └── RULE.mdc               [MODIFY] 按维度拉取列表新增模块 Dev/Integration 导航，不承载正文。
└── Docs/
    ├── AI-Workflow.md                 [MODIFY] 唯一规则正文。第 1 节增加模块文档读取；第 2 节增加触发判定；第 3 节表格增加 Dev/Integration 两行；第 5 节增加数量去重与 500 行拆分；新增第 6 节制度与模板；相关文档顺延第 7 节并登记新文件。
    ├── Source-Structure.md            [MODIFY] 第 137 行输入层「完整设计」引用改为同时指向 Input-Dev 与 Input-Integration。
    ├── Content-Structure.md           [MODIFY] 第 222 行 Negate(Y) 说明改指 Input-Dev 踩坑；第 271 行相关文档列出双文档。
    └── Input/
        ├── Input-Dev.md               [NEW] 输入模块开发文档：范围与状态、源码结构、设计决策、SOP、四条踩坑清单、校验报告与 PIE 冒烟、已知限制、相关文件。保留现有实现事实，不复制资产总数。
        ├── Input-Integration.md       [NEW] 输入模块接口文档：定位边界、EnhancedInput/GameplayTags/InputCore 依赖、IMC 注入与数据流、C++/资产接口表、其他模块调用顺序、禁止依赖方向、相机/动画边界、变更影响面。
        └── Input-System.md            [DELETE] 拆分完成并核对无信息丢失后删除，避免双份维护。
```

## Key Doc Structures

```
文首元信息（两份通用）
> 维度 / 模块 / 适用工程 / 最后更新 / 上游规范 / 配套文档

Dev 固定章节
1 目的与范围
2 当前状态与源码结构
3 设计决策与理由
4 开发流程 SOP
5 踩坑清单（现象 / 根因 / 正确做法 / 验证方式）
6 校验、报告与冒烟
7 已知限制与后续路线
8 相关文件

Integration 固定章节
1 模块定位与边界（职责 / 非职责）
2 模块与外部依赖（Build.cs / 配置 / 第三方）
3 数据流与交互
4 对外接口（C++ / 资产 / 配置）
5 其他模块调用方式（顺序 / 前置条件 / 示例）
6 禁止的依赖方向
7 变更影响面（改什么要同步谁）
8 相关文件
```