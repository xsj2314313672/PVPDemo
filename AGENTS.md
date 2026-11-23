# AGENTS.md — PVPDemo 上下文入口

本文件是项目的上下文入口，客户端支持时于会话开始时自动加载（CodeBuddy 在项目根不存在 `CODEBUDDY.md` 时加载本文件）。常驻规则 `.codebuddy/rules/doc-first/RULE.mdc`（`alwaysApply: true`）会引导 AI 先读本文件；若两者都未进入上下文，请显式 `@AGENTS.md`。

只放稳定信息：项目定位、文档地图、按需拉取协议与硬约束。**不写资产数、文件数等易变数字**，数量与实现状态一律以 `Docs/` 对应文档文首为准。

---

## 1. 项目速览

- **定位**：Unreal Engine 5 第一人称 PVP 射击玩法 Demo，目标是跑通 FPS 3C（Character / Camera / Control），再逐步接入 GAS、装备与动画系统。
- **源码**：`Source/PVPDemo/`，运行时模块名 `PVPDemo`，导出宏 `PVPDEMO_API`。已落地 `Character/PVPBaseCharacter.h/.cpp`（基础 1P 角色 + `CameraFP`，已接 Move / Look / Jump / Run 输入与走跑速度切换）、`Input/PVPInputConfig` 与 `Input/PVPInputComponent`（Lyra 式 Tag ↔ InputAction 配置与绑定）、`GAS/PVPGameplayTags`（仅 `InputTag.*`）与 `Game/PVPGameMode`、`Game/PVPPlayerController`（会话与玩家入口，控制器在代码中默认注入 `IMC_PVP_Default`）；`Animation/PVPAnimInstanceBase`（`UAnimInstance` 基类，线程安全采集 Move / Run / Jump 基础 locomotion 变量）与逻辑资产 `ABP_PVP_FP_Locomotion`（四状态 locomotion 图，父类 `UPVPAnimInstanceBase`，已通过 `BP_PVPBaseCharacter` 的 Mesh AnimClass 覆盖接线；C++ 构造期默认仍是 `ABP_PVP_FP_BaseCharacter`）；`Equipment/PVPWeaponActor` 与 `Equipment/PVPEquipmentComponent`（1P/3P 双网格武器 Actor、靠近自动拾取、附着到角色 `ik_hand_gun`、角色 / 武器成对 Montage），`Equipment/PVPWeaponAttachmentTypes.h` 与 `Equipment/PVPWeaponAttachmentCatalog.h/.cpp`（配件槽位枚举 / 规格 / 预设与目录 DataAsset 类，运行时按 `PVP.Attachment.<Slot>` 标签建槽位表并提供查询 / 显隐 / 换网格 / 应用预设接口），`BP_PVPBaseCharacter` 继承的 C++ 角色构造期已创建 `EquipmentComponent`；另含编辑器期模块 `Source/PVPDemoEditor/`（资产引用重定向工具 + 动画蓝图构建器，只进编辑器、不进打包）。`Camera/`、`Camera/Modes/` 仍为规划中的空目录。
- **自有资产**：分两根。`Content/PVPDemo/` 只放美术表现资产（Audio / Characters / Common / Environment / Props / UI / VFX / Weapons），其中 `Weapons/AssaultRifle/Attachments/<Group>/` 是按槽位分类的武器散件（Handguard / Sight / Optic / Grip / Muzzle / Laser / Magazine），枪身与弹药 / 枪箱留在 `Meshes/`；`Content/PVP/` 放逻辑资产（Animation / Actors / Characters / Data / Game / Input / Maps / Types / UI），动画蓝图 / 动画接口 / 动画通知统一在 `Animation/`，蓝图侧枚举与结构体统一在 `Types/`，配件目录数据资产在 `Data/`。
- **第三方原包**：`Content/InfimaGames/TacticalFPSAnimations` 不随仓库分发。工程自有资产已完成引用重定向、不再依赖该包，克隆后无需导入原包即可打开工程并运行自有地图。
- **构建**：右键 `PVPDemo.uproject` → Generate Visual Studio project files → 编译 `PVPDemoEditor`。

## 2. 文档地图（按维度按需拉取）

| 文档 | 维度 | 覆盖内容 | 何时拉取 |
| --- | --- | --- | --- |
| `Docs/Source-Structure.md` | 源码 | C++ 分层职责、规划文件、命名与依赖方向、阶段约束、模块与 `Build.cs` 依赖、当前实现状态 | 新增 / 修改 `Source/` 下的文件、类、模块依赖 |
| `Docs/Content-Structure.md` | 资产 | 美术根与逻辑根的模块划分、命名前缀与后缀、各模块资产表与数量、第三方迁移与自包含约束 | 新增 / 移动 / 重命名资产，核对命名与数量 |
| `Docs/AI-Workflow.md` | 流程 | 读取顺序、文档同步判定与清单、输出要求、冲突处理、上下文维护 | 每次任务开始与收尾 |
| `Docs/<模块>/` | 模块 | 每个功能模块的固定三件套：`README.md`（导航 + 已实现能力速查）、`<模块>-Dev.md`（开发流程与踩坑）、`<模块>-Integration.md`（依赖与接口契约）；可含 `<模块>-<主题>.md` 附加文档 | 新增 / 修改 / 排查某个功能模块：先读其中的 `README.md`，再按导航按需读取 |
| `Docs/Equipment/` | 装备 | 武器 Actor / 装备组件、自动拾取与附着、成对 Montage、武器资产生成 SOP 与接口契约（固定三件套）；`Equipment-Attachments.md` 承载配件分类 / 插槽映射 / 两套装配方案与换装 SOP | 修改武器 / 拾取 / 装备组件，排查挂点或 Montage 问题；分类或装配配件、换装 |
| `README.md` | 上手 | 打开方式、编译目标、第三方包说明、版本控制 | 环境配置、构建、克隆相关问题 |
| `Content/PVP/` | 逻辑资产 | 蓝图、动画蓝图、动画通知、蓝图接口、枚举 / 结构体、输入映射、数据表、关卡（Animation / Actors / Characters / Data / Game / Input / Maps / Types / UI） | 新增 / 修改蓝图、动画蓝图、输入映射或地图，核对逻辑资产落点与归位规则（`pvp_logic_layout.py`） |
| `Content/Python/` | 工具 | `migrate_pvp_assets.py` 资产迁移、`dump_tfa_character.py` 角色 Dump、`compare_tfa_character.py` 对照校验、`setup_pvp_game_framework.py` 游戏框架落地（路由复用 `pvp_logic_layout.py`）、`setup_pvp_input.py` 输入资产校验（含 Look Y 极性）与输入配置生成、`setup_pvp_anim_blueprint.py` 生成 / 重建 `ABP_PVP_FP_Locomotion`（编译 + 状态机读回）、`setup_pvp_weapon.py` 生成武器 BP / 武器 ABP、给角色 ABP 原地加 `DefaultSlot` 并放置关卡武器拾取物、`reorg_pvp_weapon_attachments.py` 配件网格按槽位归位（默认 dry-run，`PVP_ATTACHMENT_REORG_APPLY=1` 执行）、`setup_pvp_weapon_assembly.py` 生成配件目录并调用编辑器构建器在 `BP_PVP_Weapon_AR` 装配（`PVP_WEAPON_LOADOUT=Basic\|Tactical`）、`verify_pvp_game_framework.py` 零依赖与地图校验、`verify_doc_index.py` 文档索引一致性校验（三件套 / 引用可达 / 源码状态）、`pvp_logic_layout.py` 逻辑资产布局单一事实来源、`reorg_pvp_logic_assets.py` 逻辑资产归位执行器（默认 dry-run，`PVP_REORG_APPLY=1` 执行）、`verify_pvp_asset_layout.py` 离线布局校验（`Data` 2、总数 81） | 批量资产迁移、逻辑资产与武器配件归位、布局校验、角色 / 相机一致性校验、重建游戏框架资产、生成并校验输入配置、生成配件目录并装配 / 换装武器、结构性改动后自检文档索引 |
| `Saved/MigrationReports/`、`Saved/PVPCharacterDump/`、`Saved/PVPGameFrameworkReport/`、`Saved/PVPInputReport/`、`Saved/PVPAssetReorgReport/`、`Saved/PVPAnimBlueprintReport/`、`Saved/PVPWeaponReport/`、`Saved/PVPAttachmentReport/`、`Saved/AssetBackup/`、`Saved/DocIndexReport/` | 产物 | 迁移清单与报告、角色对照报告、游戏框架落地与校验报告、输入资产校验与配置生成报告、逻辑资产归位与布局校验报告、动画蓝图生成 / 编译 / 状态机读回报告、武器 BP / 武器 ABP / 角色 Slot / 关卡拾取物生成与读回报告、配件归位与装配 / 换装读回报告（含战术型归档）、删除前资产文件备份、文档索引校验报告 | 核对迁移结果、角色对齐情况、框架落地情况与输入配置结果、资产归位结果与回滚、动画蓝图生成结果、武器与配件资产生成 / 装配结果、文档索引自检结果 |

表中未覆盖的维度，先在 `Docs/` 与 `README.md` 中检索，不要凭经验猜测。

### 2.1 模块文档目录

每个**已进入实现阶段**的功能模块在 `Docs/` 下拥有一个同名文件夹，固定三件套，可追加维护文档：

| 模块 | 文件夹 | 导航（固定） | 开发 / 接口（固定） | 现状 |
| --- | --- | --- | --- | --- |
| Input | `Docs/Input/` | `README.md` | `Input-Dev.md`、`Input-Integration.md` | 已实现 Move / Look / Jump / Run；导航内含动作 / 绑定事件 / 回调函数权威表 |
| Character | `Docs/Character/` | `README.md` | `Character-Dev.md`、`Character-Integration.md` | 已实现 1P 角色本体（胶囊 / Mesh 对齐参考蓝图、`CameraFP` 挂 `head` 骨骼）、走跑速度切换与原生输入回调；导航内含组件默认值与移动状态权威表；相机模式栈仍预留 |
| Game | `Docs/Game/` | `README.md` | `Game-Dev.md`、`Game-Integration.md` | 已实现会话与玩家入口（默认 Pawn / 控制器、GameOnly 输入模式、默认 IMC 注入、`GetPVPCharacter`）；导航内含入口装配与启动时序权威表；GameState / PlayerState / 对局流程未实现 |
| GAS | `Docs/GAS/` | `README.md` | `GAS-Dev.md`、`GAS-Integration.md` | 仅落地原生 `InputTag.Move/Look/Jump/Run`；导航内含 Tag 命名与语义权威表；ASC / AttributeSet / GameplayAbility / AbilitySystemGlobals / 双网格 Montage Task / Cue 均未实现 |
| Animation | `Docs/Animation/` | `README.md` | `Animation-Dev.md`、`Animation-Integration.md` | 已实现 `UPVPAnimInstanceBase`（游戏线程采样 + 动画线程计算）与 `ABP_PVP_FP_Locomotion`（`Ground` / `JumpStart` / `Falling` / `Land` 四状态，Ground 内 BlendSpace + Run 混合，6 条过渡）；导航内含变量与状态机权威表；新 ABP 已在 `BP_PVPBaseCharacter` 上接线（C++ 默认 ABP 未改），视觉复核待做；`ABP_PVP_FP_Locomotion` 已原地增加 `DefaultSlot`，另有武器 ABP `ABP_PVP_FP_Weapon_AR`（参考姿势 + Slot）；Look / 蹲伏 / 瞄准 / 程序化动画均未提供 |
| Equipment | `Docs/Equipment/` | `README.md` | `Equipment-Dev.md`、`Equipment-Integration.md`、`Equipment-Attachments.md` | 已实现 `APVPWeaponActor` / `UPVPEquipmentComponent`、靠近自动拾取、附着到 `ik_hand_gun`、角色 / 武器成对 Equip / Fire / Reload Montage；`BP_PVP_Weapon_AR`、`ABP_PVP_FP_Weapon_AR` 与地图拾取物由 `setup_pvp_weapon.py` 生成；配件已按槽位分类到 `Weapons/AssaultRifle/Attachments/`，`DA_PVP_WeaponAttachmentCatalog` 与 `BP_PVP_Weapon_AR` 的 4 件基础型配件由 `setup_pvp_weapon_assembly.py` 生成并读回，战术型 8 件可换装；3P 配件、网络复制、开火 / 弹药 / 配件数值未实现 |

- 新建功能模块（源码分层或逻辑资产模块由空变实）时必须同步创建该文件夹并补齐三件套；预留模块（`Camera/`、`Camera/Modes/`）进入实现前不建空目录。
- 三件套命名固定；附加文档必须以 `<模块>-` 开头，并在该模块 `README.md` 的文档清单中登记，禁止孤儿文档。
- 制度正文（触发条件、固定章节、踩坑四要素、读取协议、模板）见 `Docs/AI-Workflow.md` 第 6 节。

## 3. 按需拉取协议

1. **定位维度与模块**：按上表判断本次任务涉及哪些维度；若涉及某个功能模块，先定位到 `Docs/<模块>/`。
2. **模块先读导航**：进入模块任务先读 `Docs/<模块>/README.md`，按其中的「文档清单与路由」决定读 `<模块>-Dev.md`、`<模块>-Integration.md` 的哪几节。
3. **读相关章节**：优先读与改动直接相关的章节；单个文档超过约 500 行时，先看标题结构再精读。禁止只看文件名就推测规范。
4. **不做全量扫描**：导航与模块文档已覆盖的内容不必再通读源码 / 资产；仅当文档未覆盖、与代码疑似不一致或需要精确签名时，才用 `search_content` / 精准 `read_file` 定点核对。禁止为"熟悉模块"整目录扫描。
5. **复述约束**：用 1～3 行说明本轮要遵守的关键约束（目录、命名、阶段、依赖方向），确认与 Docs 无冲突。
6. **冲突处理**：Docs 未覆盖或与需求冲突时，以 Docs 为准并说明冲突点；确需偏离时必须显式标注"拟偏离 Docs 的 X 条，理由为……"，等用户确认后再动手。
7. **收尾同步**：按 `Docs/AI-Workflow.md` 第 2～4 节判定并同步文档，回复末尾输出「文档同步」小节。

文档与代码 / 资产不一致时，以真实情况为准并主动修正文档，不静默按错误文档执行。

## 4. 硬约束（始终生效）

1. **命名**：类与文件统一 `PVP` 前缀（如 `APVPBaseCharacter`），导出宏 `PVPDEMO_API`；遵守 UE 的 `U` / `A` / `F` / `E` / `I` 类型前缀。
2. **位置**：源码分层只能落在已登记层 `Character/` `Game/` `Camera/` `Animation/` `Equipment/` `Input/` `GAS/`，不新建未登记的分层目录。编辑器专用代码走独立模块 `Source/PVPDemoEditor/`（模块目录，不是新分层），不得把编辑器依赖引入运行时模块。
3. **数据流**：`Input → Character/GAS → Animation/Camera` 单向流动；表现层只读逻辑状态，不得反向修改。`Game/` 依赖 `Character/` 单向，`Character/` 不感知 `Game/`；`Character/` 依赖 `Equipment/` 单向，`Equipment/` 只使用 `ACharacter` 与武器 Actor，不包含 `Character/` 具体类，也不依赖 `Game/` / `GAS/`。
4. **阶段约束**：`Camera/Modes/` 仅为预留目录，未到阶段不得提前实现相机模式栈；`CameraFP` 当前内置于 `Character/PVPBaseCharacter`。
5. **资产位置与命名**：美术表现资产只放 `Content/PVPDemo/<模块>/`；逻辑资产（蓝图、动画蓝图、动画通知、蓝图接口、枚举 / 结构体、数据资产、地图）只放 `Content/PVP/<模块>/`，其中动画类进 `Animation/`、**蓝图侧**类型资产进 `Types/`、数据资产进 `Data/`（路由见 `Content/Python/pvp_logic_layout.py`）。命名遵循 `Docs/Content-Structure.md` 第 2 节前缀 / 后缀。**边界澄清**：与 C++ 契约绑定的类型定义随源码层（如配件槽位 `Equipment/PVPWeaponAttachmentTypes.h`），`Content/PVP/Types/` 只放蓝图自身的 `E_*` / `S_*` 资产；不得为同一枚举在 C++ 与蓝图各定义一份。
6. **资产生成方式**：`.uasset` / `.umap` 不可手写或伪造，只能通过 UE 编辑器或 `Content/Python/` 下的脚本产出；新增脚本需登记到文档与本文件地图。
7. **依赖变更**：新增模块或第三方依赖时同步更新对应模块的 `Build.cs`，并写入 `Docs/Source-Structure.md`；运行时模块无新增依赖时也要显式记录结论。
8. **文档真实性**：文档描述的实现状态必须与代码 / 资产真实情况一致，不允许"文档说未实现而代码已存在"（或相反）。
9. **模块文档**：新建功能模块，或已有模块的对外接口 / 依赖方向 / 数据流 / 资产契约变化时，必须同步 `Docs/<模块>/` 三件套；开发中出现新坑点当轮写入 `<模块>-Dev.md` 踩坑清单，不得只留在会话上下文里。

## 5. 收尾要求

- 按 `Docs/AI-Workflow.md` 第 3 节清单同步文档，回复末尾固定追加「文档同步」小节；本次未改变结构时写明"本次未改变结构，无需同步文档"并给出依据。
- 模块相关改动需在「文档同步」中列出更新了 `README.md` / `<模块>-Dev.md` / `<模块>-Integration.md` 中的哪几份、哪几节；新建模块或接口变化时三件套缺失视为任务未完成。
- 新增、改名或删除 `Docs/` 文档、`Content/Python/` 脚本时，同步更新本文件「文档地图」与「模块文档目录」，保持索引与磁盘一致。
