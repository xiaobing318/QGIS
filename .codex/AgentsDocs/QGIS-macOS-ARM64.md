# QGIS-macOS-ARM64 平台分册

## 适用范围

- 目标平台：macOS + ARM64，重点是 Apple Silicon 宿主机相关资料。
- 当前本地资料主要覆盖 macOS + Parallels Desktop + Ubuntu 20.04 ARM64 虚拟机代理配置，不覆盖完整原生 macOS QGIS 编译流水线。
- 当前仓库根目录 `AGENTS.md` 仍是最高优先级规则；本文件只补充 macOS ARM64 平台资料入口和验证边界。
- 本平台不使用 Windows AMD64 的 OSGeo4W 依赖根；无需解析 Windows AMD64 专属外部根路径。

## 资料入口

- 平台资料根目录：`__QGISCompilationNavigation__\QGIS-macOS-ARM64\`
- 跨平台公共资料：`__QGISCompilationNavigation__\CommonMaterials\`，仅在需要核验 OS/ISA/QGIS 兼容关系时读取。
- 现有资料：
  - `ParallelsDesktopProxy\NOTES.md`：macOS 宿主机代理给 Parallels Ubuntu ARM64 虚拟机复用的说明。
  - `ParallelsDesktopProxy\set-env.bash`：代理环境变量辅助脚本。

## 任务前置

- 必须先确认任务目标是原生 macOS ARM64 QGIS 开发，还是 macOS 宿主机上的 Ubuntu ARM64 虚拟机开发，并锁定 QGIS/Qt/工具链、运行目标和本任务 R 等级。
- 若目标是 Parallels Ubuntu ARM64 虚拟机，应同时读取 `.codex/AgentsDocs/QGIS-Linux-ARM64.md`，本分册只作为代理和网络辅助资料。
- 涉及代理、系统环境变量、APT 代理或 shell 配置时，必须声明影响范围，并避免扩散历史示例中的代理地址、账号、密码或提取码。

推荐先运行仓库内上下文发现入口，生成 JSON 后再整理给用户审查：

```bash
bash .codex/HelperScripts/macOS/ResolveCodexContext.sh --config .codex/HelperScripts/macOS/ResolveCodexContext.json
```

该脚本只解析 macOS/ISA，并发现、校验 `AGENTS.md` 中使用的 `__QGISCompilationNavigation__` 占位符绝对路径；原生 macOS 构建入口或 Parallels Ubuntu ARM64 虚拟机上下文仍必须按用户输入、本分册和 Linux ARM64 分册继续核验。

## 构建和验证边界

- 当前资料没有原生 macOS ARM64 QGIS 编译、测试、安装或打包的统一入口。
- 若任务达到 R2 且要求原生 macOS ARM64 完整编译验证，但用户未补充对应构建入口、配置文件、构建目录和测试命令，必须判定为 `BLOCKED`。
- 可执行的替代核验包括：源码静态检查、官方 QGIS macOS 构建资料核验、代理脚本语法检查、Parallels 网络配置静态核验。
- 不得把 Windows AMD64 的 OSGeo4W、VS2022 或 `InvokeQGISBuild.ps1` 命令套用到本平台。

## 常见检查点

- Parallels Shared Network 常见宿主机地址和代理端口必须以当前机器实际命令输出为准。
- Ubuntu 20.04 ARM64 里配置 APT 或环境变量代理前，先区分临时会话和系统级持久化写入。
- Clash/Clash Verge、SSR 等代理工具的包依赖和端口监听状态需要按实际环境核验。
