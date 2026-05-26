# QGIS-Linux-AMD64 平台分册

## 适用范围

- 目标平台：Linux + AMD64，重点覆盖 Ubuntu/Debian 及 Kylin/UOS/SinoVation/Deepin 等国产操作系统部署场景。
- 当前资料重点是依赖包下载、解包、隔离运行、多版本 glibc/binutils/gcc 兼容和运行环境准备；不等同于已经存在统一 Codex 构建流水线。
- 当前仓库根目录 `AGENTS.md` 仍是最高优先级规则；本文件只补充 Linux AMD64 平台资料入口和验证边界。
- 本平台不使用 Windows AMD64 的 OSGeo4W 依赖根；无需解析 Windows AMD64 专属外部根路径。

## 资料入口

- 平台资料根目录：`__QGISCompilationNavigation__\QGIS-Linux-AMD64\`
- 跨平台公共资料：`__QGISCompilationNavigation__\CommonMaterials\`，用于核验 OS/ISA/QGIS、Ubuntu/Debian 与 glibc 版本关系。
- 必读资料：
  - `README.md`：平台目标、QGIS 版本示例、文件说明和执行步骤。
  - `CommonBashScripts\qgis-download-debs.sh`：下载指定版本 QGIS 的直接和间接依赖。
  - `CommonBashScripts\qgis-download-debs-enhancement.sh`：增强版依赖下载脚本。
  - `CommonBashScripts\qgis-extract-debs.sh`：在目标系统解包依赖。
  - `CommonBashScripts\qgis-setup-and-run.sh`：配置运行环境并启动 QGIS。
  - `CommonBashScripts\qgis-setup-and-run-for-multi-glibc.sh`：低版本 glibc 系统上的隔离运行入口。
  - `CommonMaterials\CompileMultiVersionGLIBCGuide.md`、`MultiVersionGLIBCGuide.md`：多版本 glibc 资料。
  - `CommonMaterials\CompileMultiVersionGCCGuide.md`、`CompileMultiVersionBinutilsGuide.md`：GCC/binutils 构建资料。

## 任务前置

- 必须先锁定发行版、版本、CPU 架构、目标 QGIS 版本、Qt 版本、glibc 版本、部署目标和本任务 R 等级。
- 本平台没有统一 profile；不得从 Windows `Configure.json` 或 OSGeo4W profile 推导 Linux 构建上下文。
- 脚本执行前必须静态核验脚本内变量、下载源、输出目录、是否需要 root 权限、是否会写入系统目录或目标部署目录。
- 对会下载、解包、复制或修改运行环境的脚本，执行前必须声明影响范围；验证完成后清理本轮临时产物。

推荐先运行仓库内上下文发现入口，生成 JSON 后再整理给用户审查：

```bash
bash .codex/HelperScripts/Linux/ResolveCodexContext.sh --config .codex/HelperScripts/Linux/ResolveCodexContext.json
```

该脚本只解析 Linux/ISA，并发现、校验 `AGENTS.md` 中使用的 `__QGISCompilationNavigation__` 占位符绝对路径；发行版、glibc、QGIS/Qt、部署目标和具体脚本目录仍必须按用户输入、本分册和资料目录继续核验。

## 构建和验证边界

- 当前资料中没有与 Windows AMD64 `InvokeQGISBuild.ps1` 等价的统一 Codex 构建入口。
- 若任务达到 R2 且要求完整编译验证，但用户未补充当前 Linux AMD64 的构建入口、配置文件、构建目录和测试命令，必须判定为 `BLOCKED`。
- 可执行的替代核验包括：源码静态检查、脚本参数检查、README/Guide 一致性检查、Bash 语法检查和已明确脚本的 `--check-only`、`--dry-run` 或只读检查；参数存在性必须先静态核验。
- 不得把 Windows AMD64 的 OSGeo4W、VS2022 或 `InvokeQGISBuild.ps1` 命令套用到本平台。

## 常见检查点

- 依赖下载目录和解包目录是否与 `README.md` 示例变量一致。
- `DEB_PYTHON_INSTALL_LAYOUT=deb_system` 等发行版相关编译/运行问题是否适用于当前目标。
- 多版本 glibc 方案是否和目标系统 glibc、binutils、libstdc++ 版本兼容。
- 运行脚本前确认 `LD_LIBRARY_PATH`、`PATH`、Qt plugin path、Python path 等环境变量不会污染用户系统长期状态。
