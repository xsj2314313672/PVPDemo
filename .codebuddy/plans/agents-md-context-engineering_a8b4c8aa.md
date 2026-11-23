---
name: agents-md-context-engineering
overview: 将项目从"always 规则承载全部约束"改为上下文工程：新增根目录 AGENTS.md 作为自动加载的上下文地图（按维度索引 Docs），新增 Docs/AI-Workflow.md 承载流程规范，删除 .codebuddy/rules/docs-first，并同步修正已发现的文档漂移（源码状态、资产数 576→577、蓝图例外）。
todos:
  - id: workflow-doc
    content: 新建 Docs/AI-Workflow.md，承载读取协议、同步判定与清单、输出要求
    status: completed
  - id: agents-entry
    content: 新建根目录 AGENTS.md，写入文档地图、硬约束与索引自维护规则
    status: completed
    dependencies:
      - workflow-doc
  - id: remove-rule
    content: 删除 .codebuddy/rules/docs-first 规则并确认 rules 目录清空
    status: completed
  - id: sync-docs
    content: 修正 Source/Content 结构文档与 README 的失真内容及索引
    status: completed
    dependencies:
      - agents-entry
  - id: verify-context
    content: 交叉校验无 CODEBUDDY.md、路径可达、577 与 57 数字一致，输出文档同步结果
    status: completed
    dependencies:
      - remove-rule
      - sync-docs
---

## 用户需求

把项目现有的"always 规则强制加载全部规范"改造成上下文工程：每次会话进入时先以根目录 `AGENTS.md` 作为统一入口，由它按不同维度概括 `Docs/` 下各文档，使 Agent 按需拉取相关文档，而不是每次对话都全文注入长规则。

## 产品概述

项目当前通过 `.codebuddy/rules/docs-first/RULE.mdc`（always 类型，55 行）强制要求每次对话读完全部结构文档，并承载代码/资产规范与文档同步清单。改造后：`AGENTS.md` 常驻上下文，只保留稳定的项目定位、文档地图、少量硬约束与收尾要求；流程细节独立为 `Docs/AI-Workflow.md`；原规则删除，`rules` 目录清空。同时把已发现的文档失真（源码状态、资产数量、蓝图例外）一并校准，保证入口索引指向的内容与代码真实情况一致。

## 核心功能

- 根目录新增 `AGENTS.md`，作为唯一上下文入口，包含项目速览、文档地图（文档 / 维度 / 覆盖内容 / 何时拉取）、按需拉取协议、不超过 8 条硬约束、收尾与自维护要求。
- 文档地图至少覆盖：源码结构、资产结构、协作流程、项目上手（README）、脚本工具（`Content/Python/`）、校验产物（`Saved/` 下迁移与对照报告）。
- 新增 `Docs/AI-Workflow.md`，承载读取顺序（列目录 → 读相关章节 → 1~3 行复述约束 → 动手 → 收尾同步）、文档同步判定标准与逐项清单、输出末尾「文档同步」小节、冲突与失真处理、数量按实测统计。
- 删除 `.codebuddy/rules/docs-first/RULE.mdc` 及目录，项目不再存在 always 规则。
- 修正文档失真：`Source-Structure.md` 的当前状态、目录树、Character 层规划表；`Content-Structure.md` 的资产总数与 Characters 模块数量、蓝图例外登记、源码状态说明；`README.md` 的目录树与「AI 协作规范」段落。
- 明确约束：`AGENTS.md` 不写易变数字；资产数量以 `Content-Structure.md` 文首为准；新增或改名文档、脚本时必须回写索引。

## 技术栈

- 纯 Markdown 文档与上下文配置改造，不改动 C++ 代码与 `.uasset` 资产，无新增依赖。
- 上下文机制：CodeBuddy 在项目根存在 `AGENTS.md` 且不存在 `CODEBUDDY.md` 时自动载入其全文；`.codebuddy/rules` 支持 always / Agent Requested / Manual，本方案不再保留 any always 规则。
- 事实来源：`Docs/Source-Structure.md`、`Docs/Content-Structure.md`、`Content/Python/` 三个脚本、`Saved/MigrationReports/` 与 `Saved/PVPCharacterDump/` 产物。

## 实施方案

采用三层上下文架构：

- L0 常驻层：`AGENTS.md`（控制在 60~100 行），只放稳定信息——项目定位、文档地图、按需拉取协议、不超过 8 条硬约束、收尾要求，不放资产数、文件数等易变数据。
- L1 按需层：`Docs/AI-Workflow.md`（流程维度）、`Docs/Source-Structure.md`（源码维度）、`Docs/Content-Structure.md`（资产维度），由地图中的触发条件决定拉取哪份、读哪些章节。
- L2 佐证层：`README.md`（打开/编译/第三方包）、`Content/Python/*.py`（迁移与对照脚本）、`Saved/` 下报告（校验产物）。

关键决策与理由：

- 删除 always 规则而不是改为 Agent Requested：避免同一份约束在规则与文档两处维护产生漂移；`AGENTS.md` 本身已自动加载，无需重复注入。
- 流程独立成文：`AGENTS.md` 保持"索引"职责，符合用户要求的"不同维度、按需拉取"。
- 不把资产数与文件数写入 `AGENTS.md`：数字变动频繁，统一指向文档文首，降低常驻内容失真概率。
- 保留两份结构文档现有章节编号与表格结构，只替换失真条目，控制改动影响范围。

性能与可靠性：原方案每次会话注入 4.33 KB 规则并强制全文阅读两份长文档（合计 400+ 行）；新方案常驻内容约 1~2k tokens，其余按维度精确读取，初始上下文显著下降。风险点为未来若新增 `CODEBUDDY.md` 会导致 `AGENTS.md` 不再自动加载，需在 `AI-Workflow.md` 与 `README.md` 中写明该机制。

## 实施要点

- 所有被引用路径必须真实存在：`Content/Python/migrate_pvp_assets.py`、`dump_tfa_character.py`、`compare_tfa_character.py`；`Saved/MigrationReports/`、`Saved/PVPCharacterDump/`。
- 数字以实测为准：`Content/PVPDemo/` 共 577 个 `.uasset/.umap`；`Characters/` 57（原文档 56）。差异来自 `Content/PVPDemo/Characters/BP_PVPBaseCharacter.uasset`。
- `BP_PVPBaseCharacter` 是当前唯一逻辑资产例外（由 `compare_tfa_character.py` 创建/复用），与"该目录只放美术资产"冲突；本次只登记为已知例外，不移动、不修改资产。
- `Docs/Source-Structure.md` 需按实际实现改写 2.1 表：将规划中的 `PVPCharacter.h/.cpp` 更正为 `PVPBaseCharacter.h/.cpp`，说明 `CameraFP` 暂内置于角色类、`Camera/` 层尚未实现、`Camera/Modes/` 仍为预留；补充 `PVPBaseCharacter.cpp` 硬引用 `/Game/InfimaGames/.../SKM_FP_Manny_Simple` 与 `ABP_TFA_FP_BaseCharacter_C` 的依赖事实。
- 保持阶段约束不变：相机模式栈仍不实现。
- 不新建 `CODEBUDDY.md`，不改 `.codebuddy/plans/`，不新增其他配置文件。
- 收尾必须按新流程在回复末尾输出「文档同步」小节，列出改动文档与具体改动点。

## 架构设计

上下文流转：会话启动 → 自动载入 `AGENTS.md` → 按任务维度查地图 → 只读对应文档的相关章节 → 执行 → 依 `AI-Workflow.md` 判定并同步文档 → 回复末尾输出同步结果。`AGENTS.md` 只做索引与硬约束，不复制文档正文，避免双份事实源。

## 目录结构

```
PVPDemo/
├── AGENTS.md                                  # [NEW] 上下文入口：项目速览、文档地图（文档/维度/覆盖内容/何时拉取）、按需拉取协议、≤8 条硬约束、收尾与索引自维护；不写易变数字
├── README.md                                  # [MODIFY] 目录结构块补 AGENTS.md 与 Docs/AI-Workflow.md；「AI 协作规范」段改写为 AGENTS.md 入口 + 按需拉取 + 流程文档位置，删除 .codebuddy/rules 描述
├── Docs/
│   ├── AI-Workflow.md                         # [NEW] 协作流程维度：读取顺序、同步判定（增/删/结构变化）、逐项同步清单、输出要求、冲突与失真处理、数量实测规则、CODEBUDDY.md 优先级提示
│   ├── Source-Structure.md                    # [MODIFY] 文首当前状态、第 1 节目录树、2.1 Character 表（PVPBaseCharacter 实况、CameraFP 内置、Camera/ 未实现）、原包硬依赖说明
│   └── Content-Structure.md                   # [MODIFY] 资产总数 576→577、Characters 56→57、目录树与 3.2 表、BP_PVPBaseCharacter 例外条目、第 4 节源码状态、第 5 节约定补充
└── .codebuddy/
    └── rules/
        └── docs-first/
            └── RULE.mdc                       # [DELETE] always 规则整体删除，docs-first 目录一并移除；rules 目录清空，plans 目录不动
```

## 关键结构

`AGENTS.md` 文档地图表格格式（列固定，便于后续追加维度）：

| 文档 | 维度 | 覆盖内容 | 何时拉取 |
| --- | --- | --- | --- |
| `Docs/Source-Structure.md` | 源码 | C++ 分层职责、规划文件、命名与依赖方向、阶段约束、Build.cs 依赖 | 新增/改动 Source 文件、类或模块依赖 |
| `Docs/Content-Structure.md` | 资产 | 模块划分、命名前缀/后缀、各模块资产表与数量、第三方迁移约束 | 新增/移动/重命名资产、核对命名与数量 |
| `Docs/AI-Workflow.md` | 流程 | 读取协议、同步判定与清单、输出要求、冲突处理 | 每次任务开始与收尾 |
| `README.md` | 上手 | 打开方式、编译目标、第三方包、版本控制 | 环境、构建、克隆相关问题 |
| `Content/Python/` | 工具 | 迁移、角色 Dump、对照校验脚本 | 批量资产迁移、角色一致性校验 |
| `Saved/MigrationReports/`、`Saved/PVPCharacterDump/` | 产物 | 迁移清单与报告、对照报告 | 核对迁移结果与角色对齐情况 |

---

## 后续变更说明（2026-11-23）

本计划落地后，经实践校验对两处设计做了调整，并记录一次显式偏离：

1. **保留 always 规则并瘦身为纯导航**：原方案"删除 `.codebuddy/rules/doc-first/RULE.mdc`、rules 目录清空"未执行。实际保留该文件并保持 `alwaysApply: true`，理由是单靠 `AGENTS.md` 自动加载在部分会话中可能漏读，常驻导航是低成本兜底。同时删除其第 2 节"硬约束速查"（该段复制自 `AGENTS.md` 第 4 节，正是原方案想避免的双份维护漂移），使规则回归"只做导航、不承载规范正文"的单一职责。规范正文仍以 `AGENTS.md` 与 `Docs/` 为唯一来源。
2. **新增机械校验闭环**：为弥补"自维护全靠 AI 自觉"的弱点，新增 `Content/Python/verify_doc_index.py`（纯标准库、无需 UE 环境），校验模块三件套完整性、文档 `.md` 引用可达性、源码分层空 / 非空状态与 `Docs/Source-Structure.md` 目录树一致性；接入 `Docs/AI-Workflow.md` 收尾与 `RULE.mdc` 收尾，报告落 `Saved/DocIndexReport/`。
3. **显式偏离原"实施要点"**：原实施要点写明"不改 `.codebuddy/plans/`"。本次为留下上述决策痕迹，经用户明确要求在本文件末尾追加本变更说明，属于对该条的显式偏离；偏离仅限本节文字追加，不改变原计划的已完成状态与其余约束。

配套补齐：Animation 模块第三件套 `Docs/Animation/Animation-Integration.md`，并同步 `AGENTS.md`、`Docs/Source-Structure.md` 的模块状态；新增脚本与产物目录已登记到 `AGENTS.md` 文档地图。