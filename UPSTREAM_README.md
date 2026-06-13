# jamony（基于 jamulus 的二次开发基线）

本仓库是 [jamulussoftware/jamulus](https://github.com/jamulussoftware/jamulus) 的私有镜像，托管在 `jamonyapp` 组织下，用作 jamony 项目二次开发的稳定基线。

## 上游来源

- **官方仓库**：https://github.com/jamulussoftware/jamulus
- **官方网站**：https://jamulus.io/
- **许可证**：AGPL 3.0+（GNU Affero General Public License v3.0 or later）
  - 2026-6-2 由上游正式从 GPLv2 升级为 AGPL 3.0+（commit `d289bb6` by pljones）
  - 分水岭：3.12.1dev（commit `eb172d47`）之前的代码为 GPL 3.0+，之后的新贡献为 AGPL 3.0+
  - 关键条款：AGPL 第 13 条（网络服务条款）要求，若以网络服务形式提供修改版，须向所有使用该服务的用户提供完整源码
- **导入时间**：2026 年 6 月 5 日
- **导入方式**：GitHub Importer 一次性快照（非持续同步）

## ⚠️ 上游仓库辨析

`jamulus` 由 Volker Fischer（GitHub: `corrados`）于 2005 年创立。
2020 年 3 月后，项目移交给 `jamulussoftware` 组织维护。

- **当前唯一活跃官方仓库 = `jamulussoftware/jamulus`**
- `corrados/jamulus` 是历史路径，已停止维护（GitHub 自动重定向到新地址）
- 未来同步上游、提交 PR、查看 Issues，一律以 `jamulussoftware/jamulus` 为准

## 为什么开私有镜像

- **基线稳定**：二次开发需要稳定起点，不被上游强制更新打乱节奏
- **私有探索**：商业化形态、品牌定制、未公开功能在私有仓库迭代
- **AGPL 3.0+ 合规**：AGPL 允许私有修改和内部使用（仅组织内部、未对外提供服务时无公开义务）；⚠️ 注意 AGPL 第 13 条的特殊性 —— 一旦以网络服务形式提供给外部用户使用（即便不"分发"二进制），就触发源码公开义务，必须向所有使用该服务的用户提供完整源码下载入口

## 上游同步策略

**当前**：手动按需同步，暂未自动化。

**未来可选**：

- 本地添加 upstream remote：
  ```bash
  git remote add upstream https://github.com/jamulussoftware/jamulus.git
  ```
- 同步命令：
  ```bash
  git fetch upstream && git merge upstream/main
  ```
- 或配置 GitHub Actions 自动 weekly 同步

## 协作者须知

- **上游原始代码必须保留各文件原有的许可证协议头和 `COPYING`/`CONTRIBUTING` 等许可证文件**
  - 注意：分水岭 commit `eb172d47` 之前的文件协议头为 GPL 3.0+，之后的为 AGPL 3.0+，请勿擅自修改或统一
- **修改文件前建议先看上游同名文件的最新状态**
- **二次开发代码若未来对外分发或部署为网络服务，需评估 AGPL 3.0+ 的传染性影响**（重点关注第 13 条网络服务条款）
