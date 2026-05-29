# QGIS-Linux-ARM64 平台分册

## 适用范围

- 目标平台：Linux + ARM64/AArch64。
- 当前资料覆盖 Ubuntu 20.04 ARM64、Kylin V10 SP1 Desktop ARM64、QGIS LTR 3.22/3.28 示例和 QGIS 3.26.03 依赖下载/解包/运行脚本。
- 当前仓库根目录 `AGENTS.md` 仍是最高优先级规则；本文件只补充 Linux ARM64 平台资料入口和验证边界。
- 本平台不使用 Windows AMD64 的 OSGeo4W 依赖根；无需解析 Windows AMD64 专属外部根路径。

## 资料入口

- 平台资料根目录：`__QGISCompilationNavigation__\QGIS-Linux-ARM64\`
- 跨平台公共资料：`__QGISCompilationNavigation__\CommonMaterials\`，用于核验 OS/ISA/QGIS 和 Ubuntu/Debian/glibc 兼容关系。
- 必读资料：
  - `generalExplanation.md`：Ubuntu ARM64 镜像、桌面环境和安装限制说明。
  - `commands-explanation.md`：命令说明资料。
  - `QGIS-LTR-322-Ubuntu2004\QGIS-LTR-322-Ubuntu2004.md`：Ubuntu 20.04 ARM64 + QGIS LTR 3.22 资料。
  - `QGIS-LTR-328-Ubuntu2004\QGIS-LTR-328-Ubuntu2004.md`：Ubuntu 20.04 ARM64 + QGIS LTR 3.28 资料。
  - `Kylin-V10-SP1-Desktop-2403-ARM64\qgis32603-download-debs.sh`、`qgis32603-extract-debs.sh`、`qgis32603-setup-and-run.sh`。
  - `Kylin-V10-SP1-Desktop-2503-ARM64\qgis32603-download-debs.sh`、`qgis32603-extract-debs.sh`、`qgis32603-setup-and-run.sh`。

## 任务前置

- 必须先锁定发行版、版本、桌面/服务器环境、CPU 架构、QGIS 版本、Qt 版本、目标脚本目录、运行目标和本任务 R 等级。
- 不同子目录代表不同系统版本或 QGIS 版本，不能混用脚本和依赖包目录。
- 本平台没有统一构建配置方案；不得从 Windows AMD64 的 `BuildPipeline.json`、OSGeo4W 或其它发行版子目录推导当前任务上下文。
- 脚本执行前必须静态核验下载源、包名、输出目录、解包目录、环境变量和是否需要 root 权限。

推荐先运行仓库内上下文发现入口，生成 JSON 后再整理给用户审查：

```bash
bash .codex/HelperScripts/Linux/ResolveCodexContext.sh --config .codex/HelperScripts/Linux/ResolveCodexContext.json
```

该脚本只解析 Linux/ISA，并发现、校验 `AGENTS.md` 中使用的 `__QGISCompilationNavigation__` 占位符绝对路径；具体发行版子目录、QGIS 版本、目标脚本目录和运行目标仍必须按用户输入、本分册和资料目录继续核验。

## 构建和验证边界

- 当前资料中没有统一 Codex 构建入口，也没有跨发行版通用构建配置方案。
- 若任务达到 R2 且要求完整编译验证，但用户未补充当前 Linux ARM64 的构建入口、配置文件、构建目录和测试命令，必须判定为 `BLOCKED`。
- 可执行的替代核验包括：源码静态检查、Markdown 资料一致性检查、Bash 脚本参数/变量检查、语法检查和用户指定脚本的受控执行；任何下载、解包或启动命令执行前都必须声明影响范围。
- 不得把 Windows AMD64 的 OSGeo4W、VS2022、`BuildPipeline.ps1` 或 `BuildPipeline.json` 流程套用到本平台。

## 常见检查点

- Ubuntu ARM64 桌面镜像可能不可直接获得，需要先安装 server/minimal 再补桌面环境；以 `generalExplanation.md` 和官方资料为准。
- Kylin 2403 与 2503 目录必须按目标系统版本选择。
- QGIS 版本相关变量、包目录、解包目录和运行脚本必须保持一致。
- 运行脚本前确认代理、APT 源、网络和 ARM64 包仓库可用。
