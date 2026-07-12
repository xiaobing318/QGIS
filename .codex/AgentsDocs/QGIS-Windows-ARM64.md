# QGIS-Windows-ARM64 平台分册

## 适用范围

- 目标平台：Windows 10/11 + ARM64。
- 当前仓库根目录 `AGENTS.md` 仍是最高优先级规则；本文件只记录 Windows ARM64 平台资料现状和验证边界。

## 资料入口

- 平台资料根目录：`__QGISCompilationNavigation__\QGIS-Windows-ARM64\`
- 跨平台公共资料：`__QGISCompilationNavigation__\CommonMaterials\`，仅在需要核验 OS/ISA/QGIS 兼容关系时读取。
- 当前资料：`README.md`。
- 已知现状：该 README 说明目前没有 Windows + ARM64 平台需求，因此目录中没有相应编译、测试、安装或打包资料。

## 任务前置

- 必须先确认用户确实要求 Windows ARM64，而不是 Windows AMD64 或 macOS ARM64。
- 若用户要求 Windows ARM64 QGIS 开发、构建或验证，必须让用户补充目标 QGIS/Qt/工具链、依赖来源、构建入口、配置文件、构建配置方案、构建目录、测试命令和本任务 R 等级。
- 未补充资料前，不得复用 Windows AMD64 的 VS2022 + OSGeo4W 命令作为 Windows ARM64 构建入口。

推荐先运行仓库内上下文发现入口，生成 JSON 后再整理给用户审查：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .codex\HelperScripts\Windows\ResolveCodexContext.ps1 `
  --config .codex\HelperScripts\Windows\ResolveCodexContext.json
```

该脚本只解析 Windows 平台信息，并发现、校验 `AGENTS.md` 中使用的 `__QGISCompilationNavigation__` 占位符绝对路径；当前仍没有 Windows ARM64 构建配置方案或 OSGeo4W ARM64 自动化入口，必须等待用户补充资料。

## 构建和验证边界

- 当前没有可用的 Windows ARM64 `BuildPipeline.ps1`、`BuildPipeline.json`、构建配置方案或 OSGeo4W ARM64 资料。
- 若任务达到 R2 且要求完整编译验证，但用户未补充 Windows ARM64 构建入口、配置文件、构建目录和测试命令，必须判定为 `BLOCKED`。
- 可执行的替代核验包括：源码静态检查、官方 QGIS/Qt/Windows ARM64 资料核验、用户补充脚本的参数检查和文档一致性检查；任何安装、依赖下载或系统环境修改都必须等待用户补充明确入口与影响范围。

## 常见检查点

- 明确区分 Windows ARM64 原生构建、Windows ARM64 上运行 x64 仿真程序、以及交叉编译三种场景。
- OSGeo4W 是否提供目标 ARM64 依赖集合必须以官方资料和用户本地资料为准。
- 任何安装、依赖下载或系统环境修改都必须在用户补充明确脚本和影响范围后再执行。
