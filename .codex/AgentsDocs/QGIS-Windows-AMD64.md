# QGIS-Windows-AMD64 平台分册

## 1. 作用与适用范围

本分册适用于 Windows 10/11 + AMD64/x86-64 平台上对 QGIS 分析、开发、测试、编译、安装、发布和排障任务。仓库根目录 AGENTS.md 始终是最高优先级规则，本文件只补充 Windows AMD64 的本地资料入口、脚本入口选择、平台注意事项和阻塞边界。

**适用场景：**

- Windows AMD64 平台资料阅读、构建链解释、测试链解释、发布链解释和运行时验证计划。
- 使用 `__QGISCompilationNavigation__\QGIS-Windows-AMD64\` 下的本地资料作为证据来源。
- 在用户未明确要求处理 VS2019 时，针对 Windows AMD64 平台后续资料定位默认优先使用 VS2022 构建流水线入口，执行前仍必须由用户明确提供或确认构建配置文件和具体构建配置方案。

**不适用场景：**

- 对于 Windows ARM64、Linux AMD64、Linux ARM64、macOS ARM64 这些平台必须读取对应 `.codex\AgentsDocs\*.md` 说明文档。
- 除非用户明确要求处理 VS2019，否则不得将 VS2019 历史资料、旧版归档资料或 codex-automations 目录中自动化提示当作当前 Windows AMD64 主构建入口。
- 在未核验平台和当前任务所需外部路径前执行依赖外部资料的命令。

## 2. 基础上下文

进入任何 Windows AMD64 任务前，主代理仍按 AGENTS.md 完成基础平台信息、占位符 `__QGISCompilationNavigation__` 真实路径、平台分册路径、任务等级和允许读写范围的锁定。本分册不重复维护 HelperScripts 的参数、输出字段和完整命令。需要上下文发现时，按 .codex\HelperScripts\README.md 文件中说明的 Windows 入口生成证据，再整理给用户审查。当前任务仅阅读或修改 Codex 说明文档时，一般只需要锁定基础上下文。只有任务实际涉及编译、测试、安装、打包、发布、插件调试、运行时验证，或需要解释和执行外部构建资料脚本时，才继续要求用户提供或确认以下上下文：

- 配置文件 `BuildPipeline.json` 的真实路径和 `Test-Path` 结果。
- 明确的构建配置方案名称，不能使用脚本默认选择代替用户选择。
- 目标 QGIS、Qt、工具链、构建类型、构建目录、安装目录、日志目录、发布目录和 OSGeo4W 依赖根等当前任务实际需要的信息。

若任务需要这些上下文，但用户未明确提供且当前可追溯上下文中也无法确认，主代理必须先向用户请求补充。用户无法提供、拒绝提供或补充后仍无法核验时，才将对应验证项标记为 `BLOCKED` 标签。

## 3. 资料入口

平台资料根目录是 `__QGISCompilationNavigation__\QGIS-Windows-AMD64\` 目录，主要资料块：

| 资料块 | 入口 | 使用场景 | 边界 |
| --- | --- | --- | --- |
| VS2022 构建流水线 | `QGISCompilationGuide-Win10-VS2022\BuildPipeline.ps1`、`BuildPipeline.md`、`BuildPipeline.json`、`BuildPipelineSchema.json` | 自动化构建、测试、安装、发布阶段解释或执行 | 执行前必须由用户提供或确认构建配置文件和具体构建配置方案 |
| VS2022 手工资料 | `QGISCompilationGuide-Win10-VS2022\ManualBuildGuide.md`、`msvc-env-setup4vs2022.bat` | 环境准备、手工复现、构建原理解释 | 历史示例只作背景，不得复用账号、密码、代理、路径或机器配置 |
| VS2019 历史资料 | `QGISCompilationGuide-Win10-VS2019\README.md` | VS2019 + Qt5 手工编译参考 | 仅作为用户明确要求 VS2019 时的历史或预留手工资料；未明确要求 VS2019 时不作为默认资料入口 |
| 旧版归档资料 | `OldQGISCompilationGuide-Win10-VS2019\` | 旧版图文资料和附带环境脚本参考 | 仅归档参考，不作为当前验证入口 |
| OSGeo4W MSI 打包 | `QGISPackagingGuide-Win10-VS2022\README.md`、`ScriptInvocationOrderGuide.md` | 在 OSGeo4W 工作树中构建包、生成索引和制作 MSI | 会影响 OSGeo4W 工作树、缓存和输出目录，执行前必须单独声明影响范围 |
| Python 插件资料 | `QGISPythonPlugins\README.md`、`QGISPythonPluginsDebugGuide.md` | 插件开发、插件同步、Python 环境和 debugpy 调试 | 示例路径、端口和环境变量必须按本轮构建配置方案重新核验 |
| 公共辅助资料 | `CommonMaterials\` | 环境变量、MSVC 版本、OSGeo4W 快照、包下载、换行符治理等辅助分析 | 执行辅助脚本前必须先读对应指南，核验配置和影响范围 |
| Python 编译错误归档 | `QGISCompilationPythonErrorsAnalysis\` | 定位类似 Python 绑定或 SIP 编译错误 | 不得把历史路径、旧工具链和旧日志当成本轮事实 |
| Codex 自动化资料 | `QGISCompilationGuide-Win10-VS2022\codex-automations\`、`codex-developer\` | 自动化提示、报告模板和人工配置入口参考 | 不是主构建入口，不替代用户本轮需求和构建配置方案 |

公共辅助资料中的常见入口：

- `CommonMaterials\EnvironmentVariablesGuide.md`：环境变量和代理变量说明。
- `CommonMaterials\OSGeo4WSnapshotGuide.md`：OSGeo4W 快照、依赖来源和版本资料。
- `CommonMaterials\MSVCVersioningSystemAndCompatibilityGuide.md`：MSVC 版本和兼容性资料。
- `CommonMaterials\GetQGISWindowsMSVCVersion\`：QGIS Windows MSVC 版本辅助判断资料。
- `CommonMaterials\GetOSGeo4WSnapshotRange\`：OSGeo4W 快照范围辅助判断资料。
- `CommonMaterials\DownloadOSGeo4WPackages\`：OSGeo4W 包下载与安装辅助资料。

## 4. 场景选择

### 4.1 只读分析和文档修改

读取 AGENTS.md 文档、本分册、目标源码或目标说明文件，并按需要读取外部资料目录中的 README/Guide/schema 或脚本源码。任务等级 R0 只读分析不得修改仓库文件，也不得执行会产生构建、测试、安装、打包、发布或运行时验证结论的流程。R1 文档修改只做静态核验，不执行完整构建入口。

### 4.2 构建、测试和安装

使用 VS2022 自动化构建链时，主入口是：

- `QGISCompilationGuide-Win10-VS2022\BuildPipeline.ps1`
- `QGISCompilationGuide-Win10-VS2022\BuildPipeline.md`
- `QGISCompilationGuide-Win10-VS2022\BuildPipeline.json`
- `QGISCompilationGuide-Win10-VS2022\BuildPipelineSchema.json`

执行或解释 `BuildPipeline.ps1` 配置文件之前必须先读 `BuildPipeline.md` 说明文档，并让用户提供或确认 `BuildPipeline.json` 和具体构建配置方案。若用户未明确提供且当前可追溯上下文中也没有可核验的构建配置方案，必须先向用户请求补充，不能直接把缺失 profile 写成最终阻塞结论。Codex 生成执行命令时，必须先使用已核验且 `Test-Path=True` 的 `__QGISCompilationNavigation__` 真实值拼接脚本和配置文件路径，再显式传入用户明确指定的构建配置方案，不能依赖脚本默认选择，也不能自由猜测 profile。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "<__QGISCompilationNavigation__>\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\BuildPipeline.ps1" `
  -ConfigPath "<__QGISCompilationNavigation__>\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\BuildPipeline.json" `
  -Profile <用户明确指定的构建配置方案>
```

例如本轮上下文发现 `__QGISCompilationNavigation__=C:\Dev\github-repos\QGIS-Materials\QGISCompilationNavigation`，且用户明确指定构建配置方案 `WPGWorkstation-Windows-x64-feature-final-3_44_7-mcp-Qt6-Release` 时，Codex 应生成：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "C:\Dev\github-repos\QGIS-Materials\QGISCompilationNavigation\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\BuildPipeline.ps1" `
  -ConfigPath "C:\Dev\github-repos\QGIS-Materials\QGISCompilationNavigation\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\BuildPipeline.json" `
  -Profile WPGWorkstation-Windows-x64-feature-final-3_44_7-mcp-Qt6-Release
```

阶段、参数、日志、测试筛选、退出码和诊断输出以 `BuildPipeline.md`、`BuildPipelineSchema.json`、脚本帮助和实际日志为准，本分册不重复维护这些细节。任务等级 R2 完整验证必须使用该入口闭环，且不得用 `-CheckOnly`、`-DryRun` 或小范围测试替代完整验证。

### 4.3 发布拷贝版运行目录

拷贝版运行目录发布使用：

- `QGISCompilationGuide-Win10-VS2022\PublishPackage.ps1`
- `QGISCompilationGuide-Win10-VS2022\PublishPackage.md`
- `QGISCompilationGuide-Win10-VS2022\PublishPackage.json`
- `QGISCompilationGuide-Win10-VS2022\PublishPackageSchema.json`
- `QGISCompilationGuide-Win10-VS2022\PublishPackageValidation.json`
- `QGISCompilationGuide-Win10-VS2022\PublishPackageValidationSchema.json`

发布可通过 `BuildPipeline.ps1` 的 `PublishPackage` 阶段进入，也可以在确认前置条件后直接运行 `PublishPackage.ps1`。直接运行发布脚本前必须先读 `PublishPackage.md` 文件，并核验发布配置、发布校验文件、目标目录清理行为、ZIP 配置和运行时验证设置。脚本 `PublishPackage.ps1` 只生成拷贝版 QGIS 运行目录，不生成 OSGeo4W MSI 文件。需要构建 OSGeo4W 包或 MSI 时，必须改读 `QGISPackagingGuide-Win10-VS2022\README.md` 和 `ScriptInvocationOrderGuide.md` 文件，并单独声明 OSGeo4W 工作树、缓存、本地 HTTP 服务和 MSI 输出目录的影响范围。

### 4.4 OSGeo4W 依赖和辅助脚本

分析 OSGeo4W 快照、包下载、包范围和依赖问题时，优先读取 `CommonMaterials` 下对应 Guide。辅助脚本只能在脚本存在、配置文件存在、参数已静态核验、输出位置和影响范围已声明后执行。涉及下载、安装、缓存、工作树或发布目录写入时，必须按 AGENTS.md 的外部路径写入例外和清理要求处理。

### 4.5 Python 插件调试

插件开发或调试任务优先读取 `QGISPythonPlugins\README.md` 和 `QGISPythonPluginsDebugGuide.md`。执行前必须确认当前实际运行的插件代码路径、QGIS Python 环境、Qt5 或 Qt6 分支、调试端口、VS Code 配置和重启要求。资料中的路径和端口均为示例，不得直接当成本轮事实。

### 4.6 脚本自身修改

修改外部构建脚本、发布脚本、schema 或校验规则时，任务会从普通文档修改升级到更高风险范围。修改后才使用对应测试脚本，例如 `BuildPipelineTests.ps1` 或 `PublishPackageTests.ps1`。本仓库内 QGIS 源码改动通常不运行这些外部脚本测试，除非任务实际修改了这些外部脚本或用户明确要求。

## 5. 任务等级验证边界

R0：

- 只做源码、文档、配置、脚本和外部资料的静态核验。
- 可使用 `rg`、`Get-Content -Encoding UTF8`、`Test-Path`、参数分支检查和脚本帮助。
- 不修改仓库文件，不执行完整构建、测试、安装、打包、发布或运行时验证入口。

R1：

- 只修改说明性 Markdown、README 或其它不被 QGIS 构建链直接消费的说明文件。
- 必须静态核验路径、链接、命令语法、平台范围、构建配置方案说明和前后文一致性。
- 不执行 `BuildPipeline.ps1` 完整构建，不把静态核验写成构建通过。

R2：

- 涉及源码、构建链、测试链、运行资源、安装、打包、发布或运行时验证时，按 AGENTS.md 自动创建验收标准。
- Windows AMD64 的完整构建验证入口是 `QGISCompilationGuide-Win10-VS2022\BuildPipeline.ps1`。
- 执行前必须由用户提供或确认构建配置文件和具体构建配置方案。若当前上下文缺少 BuildPipeline profile，必须先向用户请求补充，补充后仍无法核验时才标记为 `BLOCKED` 标签。
- 日志、测试发现、失败用例、发布输出和运行时验证结论必须来自实际执行证据。
- 任一必选验收项缺证据、失败或阻塞时，总判定必须为未通过或受控例外，不得写成通过。

## 6. 常见阻塞条件

已经向用户请求补充但仍必须停止并报告 `BLOCKED` 的情况：

- 未能锁定 Windows AMD64 平台或当前平台分册。
- `__QGISCompilationNavigation__` 不存在。
- Windows AMD64 资料根目录不存在。
- 任务需要 VS2022 构建流水线，但 `BuildPipeline.ps1`、`BuildPipeline.md`、`BuildPipeline.json` 或 `BuildPipelineSchema.json` 缺失。
- 任务需要执行 `BuildPipeline.ps1`，但用户在请求补充后仍没有提供或确认构建配置文件和具体构建配置方案。
- 用户要求 VS2019 自动化构建，但没有提供有效 VS2019 构建入口、配置文件、构建配置方案和验证边界。
- 任务需要发布或运行时验证，但安装目录、发布目录、OSGeo4W 依赖根、校验文件、清理行为或日志位置无法核验。
- 发布或打包会清理、覆盖或写入目标目录，但用户未确认影响范围。
- 当前任务达到 R2，但无法执行当前平台完整验证入口，也没有满足受控例外条件的阻塞证据。

不得做的事：

- 不得把未由用户明确提供或确认的构建配置方案当成本轮事实。
- 不得把脚本默认选择当成用户选择。
- 不得把 `codex-automations` 或 `codex-developer` 下的提示文件当作主构建入口。
- 不得把 `PublishPackage.ps1` 的拷贝版发布流程说成 OSGeo4W MSI 打包流程。
- 不得把 VS2019 历史资料说成当前已验证自动化构建入口。
- 不得复用外部资料中的历史账号、密码、提取码、代理地址、机器路径或日志路径。
- 不得在缺少 `Test-Path=True` 证据时执行外部路径命令。
- 不得用预检、预演、小范围测试或口头说明替代 R2 完整验证。
- 不得将失败、跳过、无匹配测试或未执行流程写成通过。

## 7. 子代理任务包补充

项目级子代理任务包的通用字段按 AGENTS.md 和 `.codex\agents\README.md` 执行。本分册只补充 Windows AMD64 任务需要额外写入的内容。

任何 Windows AMD64 子代理任务包都应包含：

- 当前平台分册路径：`.codex\AgentsDocs\QGIS-Windows-AMD64.md` 文件。
- 平台信息：Windows AMD64，以及上下文发现证据。
- 占位符 `__QGISCompilationNavigation__` 的真实路径、来源和 `Test-Path` 结果。
- 当前任务等级和验证边界。
- 是否依赖 Windows AMD64 外部构建资料，若不依赖，明确写“不适用”。

当子代理任务实际依赖编译、测试、安装、发布、打包、插件调试或运行时验证时，还必须补充：

- 构建配置文件真实路径、来源和 `Test-Path` 结果。
- 用户确认的具体构建配置方案名称。
- 目标 QGIS、Qt、工具链、构建类型和任务阶段。
- 构建目录、安装目录、发布目录、日志目录、OSGeo4W 依赖根和相关清理边界。
- 本轮要读取的外部指南、脚本、schema 和配置文件。

子代理没有收到任务实际依赖的这些额外上下文时，不得猜测构建配置方案、依赖根、构建目录、安装目录、日志目录或测试范围。发现缺失字段影响判断时，应停止对应动作并请求主代理补充证据。
