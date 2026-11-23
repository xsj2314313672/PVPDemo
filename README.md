# PVPDemo

基于 Unreal Engine 5 的第一人称 PVP 射击玩法 Demo。当前已落地基础第一人称角色与相机（`Character/PVPBaseCharacter`）、会话与玩家入口层（`Game/PVPGameMode`、`Game/PVPPlayerController`）、动画实例基类（`Animation/PVPAnimInstanceBase`）与四状态 locomotion 动画蓝图（`ABP_PVP_FP_Locomotion`，已在 `BP_PVPBaseCharacter` 上接线，视觉复核待做）、武器拾取与附着（`Equipment/PVPWeaponActor`、`Equipment/PVPEquipmentComponent`，武器蓝图 `BP_PVP_Weapon_AR` + 武器动画蓝图 `ABP_PVP_FP_Weapon_AR`，成对 Equip / Fire / Reload Montage 示例）以及一张工程自有关卡 `L_PVP_Showcase`；规划中逐步接入 GAS 技能框架、移动 / 开镜、武器换弹与检视玩法、动画蓝图分层与音效等模块。

## 目录结构

```
PVPDemo/
├── .codebuddy/
│   └── rules/          # 常驻规则（仅入口导航，规范正文在 AGENTS.md 与 Docs）
├── AGENTS.md           # 上下文入口：文档地图、按需拉取协议与硬约束
├── Config/             # DefaultEngine / DefaultGame / DefaultInput 等配置
├── Content/
│   ├── PVPDemo/        # 自有美术资产（Audio / Characters / Common / Environment / Props / UI / VFX / Weapons）
│   ├── PVP/            # 自有逻辑资产（Animation / Actors / Characters / Data / Game / Input / Maps / Types / UI）
│   └── Python/         # 资产迁移、框架生成与校验脚本
├── Docs/               # 全局规范（AI-Workflow / Content-Structure / Source-Structure）+ 模块文件夹（每模块固定 README / Dev / Integration）
├── Source/
│   ├── PVPDemo/        # 运行时模块（Animation / Camera / Character / Equipment / Game / GAS / Input）
│   ├── PVPDemoEditor/  # 编辑器期模块：资产引用重定向工具
│   └── PVPDemo*.Target.cs
└── PVPDemo.uproject
```

## 打开方式

1. 安装与项目匹配的 UE5 引擎版本。
2. 由于 `PVPDemo.uproject` 中的 `EngineAssociation` 记录的是作者本机引擎标识，首次打开时请右键 `.uproject` → `Switch Unreal Engine Version` 选择本地引擎，或直接双击由系统提示选择。
3. 右键 `.uproject` → `Generate Visual Studio project files` 生成工程文件，再编译 `PVPDemoEditor` 目标。

## 关于第三方资产

本仓库**不包含**第三方资产包 `Content/InfimaGames`（Infima Games *Tactical FPS Animations*）。

项目自有资产已迁移并重命名：美术资产在 `Content/PVPDemo/`，逻辑资产（蓝图、输入、数据表、地图）在 `Content/PVP/`。工程已通过 `Content/Python/setup_pvp_game_framework.py` 完成引用重定向，**不存在指向第三方包的活引用**（仅有一条指向原包中不存在资产的悬空软引用 `SKM_Quinn`），因此克隆后无需导入原包即可打开工程并运行 `L_PVP_Showcase`。

## AI 协作规范

项目采用上下文工程，规范正文集中在 `AGENTS.md` 与 `Docs/`：`AGENTS.md` 在客户端支持时于会话开始时自动加载（前提是项目未创建 `CODEBUDDY.md`），提供项目定位、`Docs/` 文档地图与少量硬约束；具体规范按任务维度按需拉取：

- 改源码 → `Docs/Source-Structure.md`
- 改资产 → `Docs/Content-Structure.md`
- 改某个功能模块 → 先读 `Docs/<模块>/README.md`（导航），再按其中路由读 `<模块>-Dev.md` / `<模块>-Integration.md`
- 环境 / 构建 → 本文件

每个进入实现阶段的功能模块在 `Docs/` 下有同名文件夹，固定三件套：`README.md`（导航 + 已实现能力速查）、`<模块>-Dev.md`（开发流程与踩坑）、`<模块>-Integration.md`（依赖与接口契约）；附加文档须在该模块 `README.md` 登记。当前已建 `Docs/Input/`、`Docs/Character/`、`Docs/Game/`、`Docs/GAS/`、`Docs/Animation/` 与 `Docs/Equipment/`。制度正文（触发条件、固定章节、踩坑四要素、读取协议）见 `Docs/AI-Workflow.md` 第 6 节。

为防止新会话漏读，`.codebuddy/rules/doc-first/RULE.mdc` 以 **Always**（`alwaysApply: true`）常驻，仅承担入口导航——先读 `AGENTS.md`，再按维度拉取对应文档，**不写规范正文**，规范仍以本仓库文档为唯一来源。规则只在会话开始时加载，修改后需新建对话才生效。流程细节（读取顺序、文档同步判定与清单、输出要求）见 `Docs/AI-Workflow.md`；新增或改名文档时需同步更新 `AGENTS.md` 的文档地图，增删规则时同步本段与 `Docs/AI-Workflow.md`。

## 版本控制说明

- 资产以普通 git 对象存储，**未使用 Git LFS**，克隆即可直接获得全部 `.uasset`。
- `Binaries`、`Intermediate`、`Saved`、`DerivedDataCache` 等生成目录已在 `.gitignore` 中排除。
