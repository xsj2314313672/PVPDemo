# PVPDemo

基于 Unreal Engine 5 的第一人称 PVP 射击玩法 Demo，包含 GAS 技能框架、移动/开镜、武器换弹与检视、角色动画与音效等完整实现。

## 目录结构

```
PVPDemo/
├── Config/             # DefaultEngine / DefaultGame / DefaultInput 等配置
├── Content/
│   ├── PVPDemo/        # 自有资产（Audio / Characters / Common / Environment / Props / UI / VFX / Weapons）
│   └── Python/         # 资产迁移与比对脚本
├── Docs/               # Content-Structure.md / Source-Structure.md
├── Source/
│   ├── PVPDemo/        # 运行时模块（Animation / Camera / Character / Equipment / GAS / Input）
│   └── PVPDemo*.Target.cs
└── PVPDemo.uproject
```

## 打开方式

1. 安装与项目匹配的 UE5 引擎版本。
2. 由于 `PVPDemo.uproject` 中的 `EngineAssociation` 记录的是作者本机引擎标识，首次打开时请右键 `.uproject` → `Switch Unreal Engine Version` 选择本地引擎，或直接双击由系统提示选择。
3. 右键 `.uproject` → `Generate Visual Studio project files` 生成工程文件，再编译 `PVPDemoEditor` 目标。

## 关于第三方资产

本仓库**不包含**第三方资产包 `Content/InfimaGames`（Infima Games *Tactical FPS Animations*）。项目自有资产已迁移并重命名至 `Content/PVPDemo` 下的 `PVP_` 前缀目录。

因此克隆后打开工程时，若出现与 `InfimaGames` 相关的缺失引用警告属正常现象：

1. 自行购买并导入该资产包至 `Content/InfimaGames`，或
2. 在内容浏览器中 Fix Up Redirectors，将缺失引用替换为 `Content/PVPDemo` 内的对应资产。

## 版本控制说明

- 资产以普通 git 对象存储，**未使用 Git LFS**，克隆即可直接获得全部 `.uasset`。
- `Binaries`、`Intermediate`、`Saved`、`DerivedDataCache` 等生成目录已在 `.gitignore` 中排除。
