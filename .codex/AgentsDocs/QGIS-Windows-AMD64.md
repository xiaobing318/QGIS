# QGIS-Windows-AMD64 平台分册

## 1. 作用与适用范围

本分册用于指导 Codex 在 `Windows 10/11 + AMD64/x86-64` 平台上分析、开发、测试、编译、安装、发布和排障 QGIS。仓库根目录 `AGENTS.md` 始终是最高优先级规则；本文件只补充 Windows AMD64 的本地资料入口、配置解析规则、脚本入口和验证检查点。

适用范围：

- 操作系统：Windows 10/11。
- 指令集架构：AMD64/x86-64，`Configure.json` 中通常写作 `x64`。
- 常用工具链：Visual Studio 2022、CMake、MSBuild、CTest、Git、Cygwin、OSGeo4W。
- Visual Studio 2019 资料目前只作为历史/预留手工资料；没有有效 VS2019 `Configure.json`、profile 和自动化入口时，不得套用 VS2022 命令。
- Qt 分支：Qt5 或 Qt6，以当前 profile 的 `cmake.definitions.BUILD_WITH_QT6` 和相关配置为准。
- 当前 QGIS 源码、构建目录、安装目录、OSGeo4W 根目录均以用户本轮明确提供/确认的 profile 为准，不得写成固定长期规则。

不适用范围：

- Windows ARM64、Linux AMD64、Linux ARM64、macOS ARM64；这些平台必须读取对应 `.codex/AgentsDocs/*.md`。
- `QGISCompilationGuide-Win10-VS2022\codex-automations\` 不是当前构建入口。该目录可作为历史自动化提示和日志资料参考，但当前已核验其下没有 `InvokeQGISBuild.ps1` 或 `Configure.json`。

## 2. 必须先锁定的上下文

任何涉及 Windows AMD64 外部资料、脚本、测试或构建的任务，主代理必须先锁定并在答复中说明：

- 平台分册：`.codex\AgentsDocs\QGIS-Windows-AMD64.md`。
- `__QGISCompilationNavigation__` 真实路径、来源和 `Test-Path` 结果。
- `Configure.json` 真实路径、来源和 `Test-Path` 结果。
- 目标 profile 名称、来源和是否存在于 `Configure.json.profiles`。
- `__OSGeo4WRoot__` 真实路径、来源和 `Test-Path` 结果。
- 目标 `OS/ISA/QGIS/Qt/工具链/构建类型`。
- 本任务 R 等级：`R0`、`R1` 或 `R2`。

`__OSGeo4WRoot__` 是本分册专属占位符，只适用于 Windows AMD64 + OSGeo4W 依赖场景。其真实值必须由用户明确提供的 `Configure.json` 与 profile 中的 `paths.osgeo4wRoot` 决定；用户未提供 `Configure.json` 或 profile 时必须停止并要求补充。

本分册不得保存某台机器的绝对路径、profile 名称、OSGeo4W 快照或 QGIS 版本作为长期规则。主代理应在每轮任务答复中记录本轮实际解析出的路径、来源和 `Test-Path` 结果。

> 用户必须明确提供或确认 `Configure.json` 和 profile。缺少任一项时不得继续执行依赖 Windows AMD64 外部路径、构建、测试、安装或发布的命令。

推荐先运行仓库内上下文发现入口，生成 JSON 后再整理给用户审查：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .codex\HelperScripts\Windows\ResolveCodexContext.ps1 `
  --config .codex\HelperScripts\Windows\ResolveCodexContext.json
```

该脚本会解析 Windows/AMD64 平台信息，并发现、校验 `AGENTS.md` 中使用的 `__QGISCompilationNavigation__` 占位符绝对路径。入口参数只允许 `--help`、`--config`、`--outputJsonPath`；`Configure.json`、profile、`__OSGeo4WRoot__`、Qt、工具链、构建类型、测试开关和 pipeline 信息仍必须由用户明确提供后按本分册继续核验。脚本输出只作为本轮上下文证据；用户修正路径或 profile 时，以用户修正值为准。

## 3. 资料目录与职责

基础目录：

- 平台资料根目录：`__QGISCompilationNavigation__\QGIS-Windows-AMD64\`
- VS2022 编译资料根目录：`__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\`
- VS2019 预留/历史手工资料目录：`__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2019\`
- 旧版 VS2019 归档资料目录：`__QGISCompilationNavigation__\QGIS-Windows-AMD64\OldQGISCompilationGuide-Win10-VS2019\`
- OSGeo4W MSI 打包资料目录：`__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISPackagingGuide-Win10-VS2022\`
- Python 插件资料目录：`__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISPythonPlugins\`
- Python 编译错误分析资料目录：`__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationPythonErrorsAnalysis\`
- Windows 公共资料目录：`__QGISCompilationNavigation__\QGIS-Windows-AMD64\CommonMaterials\`

VS2022 编译资料根目录中的关键文件：

| 文件或目录 | 用途 | 使用时机 |
| --- | --- | --- |
| `AutomatedBuildGuide.md` | 自动化构建、阶段、命令场景、日志、发布和退出码说明。 | 需要执行或解释 `InvokeQGISBuild.ps1` 时先读。 |
| `ManualBuildGuide.md` | 手工准备 VS2022、CMake、Cygwin、OSGeo4W，以及常见编译问题。 | 环境缺失、手工复现、解释构建原理时读。 |
| `Configure.json` | profile、路径、CMake 定义、pipeline、发布配置。 | 解析当前构建上下文和入口参数时必读。 |
| `ConfigureSchema.json` | `Configure.json` 字段、允许值和行为说明。 | 字段含义不清、修改配置、核验 profile 时读。 |
| `InvokeQGISBuild.ps1` | 统一构建流水线入口。 | R2 构建验证、环境体检、阶段执行、日志分析时使用。 |
| `InvokeQGISBuildTests.ps1` | `InvokeQGISBuild.ps1` 的脚本测试入口。 | 修改外部构建脚本后才使用；普通 QGIS 源码改动通常不使用。 |
| `DetectMSBuildGeneratedPathLength.ps1` | Visual Studio 生成路径长度检测。 | VS 生成器且执行 build/install 前由主脚本调用；路径失败时单独排查。 |
| `PublishPackage.md` | 发布脚本的参数、配置、校验、常见问题说明。 | 发布、打包、运行时目录验证前必读。 |
| `PublishPackage.ps1` | 拷贝版 QGIS 运行目录发布脚本。 | 直接发布或由 `InvokeQGISBuild.ps1` 的 `PublishPackage` 阶段转调。 |
| `PublishPackageValidation.json` | 发布输入树、OSGeo4W 包清单和可选缺失项校验规则。 | 发布前、发布失败或 OSGeo4W 快照变化时核验。 |

其它 Windows AMD64 资料块：

| 目录或文件 | 用途 | 边界 |
| --- | --- | --- |
| `QGISCompilationGuide-Win10-VS2019\README.md` | Visual Studio 2019 + Qt5 手工编译参考。 | 只作为预留/历史资料；未提供 VS2019 自动化配置时不得执行 VS2022 流水线。 |
| `OldQGISCompilationGuide-Win10-VS2019\` | 旧版 VS2019 图文资料和附带环境脚本。 | 仅归档参考；不得作为当前验证入口。 |
| `QGISPackagingGuide-Win10-VS2022\README.md` | 通过 OSGeo4W 仓库构建 qgis-qt6 包并制作 MSI 的资料。 | 会影响 OSGeo4W 工作树、包缓存和打包输出；执行前必须单独声明影响范围。 |
| `QGISPackagingGuide-Win10-VS2022\ScriptInvocationOrderGuide.md` | OSGeo4W `bootstrap.sh qgis-qt6` 调用链说明。 | 用于分析 OSGeo4W MSI 打包，不等同于 `PublishPackage.ps1`。 |
| `QGISPythonPlugins\README.md`、`QGISPythonPluginsDebugGuide.md` | QGIS Python 插件开发、同步和 debugpy 调试资料。 | 文中机器路径和端口均为示例，使用前必须按当前 profile 核验。 |
| `QGISCompilationPythonErrorsAnalysis\` | Python 相关编译错误和历史日志分析。 | 仅用于定位类似错误；不得把历史路径、工具链和日志当成本轮事实。 |
| `QGISCompilationGuide-Win10-VS2022\codex-automations\`、`codex-developer\` | 历史 Codex 提示、计划和日志资料。 | 不是构建入口；不得复用历史账号、代理、路径或 profile 示例。 |

公共资料目录中的常见入口：

- `CommonMaterials\EnvironmentVariablesGuide.md`：Windows/QGIS/OSGeo4W 环境变量说明。
- `CommonMaterials\OSGeo4WSnapshotGuide.md`：OSGeo4W 快照、依赖来源和版本资料。
- `CommonMaterials\MSVCVersioningSystemAndCompatibilityGuide.md`：MSVC 版本和兼容性资料。
- `CommonMaterials\GetQGISWindowsMSVCVersion\`：MSVC/QGIS 版本辅助脚本资料。
- `CommonMaterials\GetOSGeo4WSnapshotRange\`：OSGeo4W 快照范围辅助脚本资料。

Python 插件资料入口：

- `QGISPythonPlugins\README.md` 和 `QGISPythonPluginsDebugGuide.md`：插件调试、Python 环境和 Qt5/Qt6 Python 配置。

Windows AMD64 + OSGeo4W 参考链接：

- https://github.com/jef-n/OSGeo4W
- https://github.com/xiaobing318/OSGeo4W
- https://download.osgeo.org/osgeo4w/

## 4. Profile 与占位符解析规则

解析顺序：

1. 用户本轮或可追溯上下文中明确提供/确认的 `__QGISCompilationNavigation__`、`Configure.json` 和 profile。
2. 核验 `__QGISCompilationNavigation__`、`Configure.json` 和 profile 均存在且匹配当前 Windows AMD64 任务。
3. 从用户指定 profile 的 `paths.osgeo4wRoot` 解析 `__OSGeo4WRoot__`，并执行 `Test-Path`。
4. 任一必需配置缺失、字段缺失、profile 不存在或路径核验失败时，停止并询问用户。

有效 profile 必须满足：

- 名称存在于 `Configure.json.profiles`。
- `paths.sourceDir` 指向当前 QGIS 仓库，或用户明确允许使用其它源码目录。
- `paths.osgeo4wRoot` 存在，且是当前平台的 OSGeo4W 根目录；该字段就是本轮 `__OSGeo4WRoot__` 的唯一配置来源。
- `paths.gitCommand` 存在。
- `cmake.generator`、`cmake.architecture` 与本机 VS/CMake/OSGeo4W 架构一致。
- 任务涉及 `build/test/install/PublishPackage` 时，继续核验 `paths.buildDir`、`paths.installDir` 和 `publishPackage` 前置条件。

常用静态核验命令：

```powershell
$QGISMaterialsPath = "__QGISCompilationNavigation__"
$BuildEntry = "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1"
$ConfigPath = "<user-provided Configure.json>"
$Profile = "<selectedProfile>"

Test-Path $QGISMaterialsPath
Test-Path $BuildEntry
Test-Path $ConfigPath

$config = Get-Content -Raw -Encoding UTF8 -LiteralPath $ConfigPath | ConvertFrom-Json
$profileObject = $config.profiles.$Profile
$null -ne $profileObject
Test-Path -LiteralPath $profileObject.paths.sourceDir
Test-Path -LiteralPath $profileObject.paths.osgeo4wRoot
Test-Path -LiteralPath $profileObject.paths.gitCommand
```

## 5. `InvokeQGISBuild.ps1` 行为摘要

脚本入口：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>"
```

脚本支持的关键参数：

| 参数 | 作用 |
| --- | --- |
| `-ConfigPath <path>` | 必填，指定 `Configure.json`。 |
| `-Profile <name>` | Codex 任务中必须显式传入用户明确提供/确认的 profile；不得省略。 |
| `-Stages <list>` | 限定阶段，允许值：`clean`、`configure`、`build`、`test`、`install`、`PublishPackage`。 |
| `-BuildTarget <target>` | 覆盖 `pipeline.buildTarget`，可用逗号分隔多个目标。 |
| `-Configuration <name>` | 构建类型：`Debug`、`Release`、`MinSizeRel`、`RelWithDebInfo`。 |
| `-BuildVerbosity <level>` | MSBuild 输出详细度：`quiet`、`minimal`、`normal`、`detailed`、`diagnostic` 等。 |
| `-Jobs <n|auto>` | 并行任务数。 |
| `-CheckOnly` | 只验证配置、环境和路径，不执行 clean/configure/build/test/install/PublishPackage。 |
| `-DryRun` | 打印将执行的命令，不执行破坏性或外部步骤。 |
| `-Diag` | 输出工具路径、版本、CMakeCache、命令预览等诊断信息。 |

阶段行为：

- 执行前必须明确本轮阶段列表；阶段可来自用户明确指定，或来自用户指定 profile 中已核验并已向用户复述的 pipeline 阶段配置。
- 即使配置数组顺序不同，脚本实际执行顺序固定为 `clean -> configure -> build -> test -> install -> PublishPackage`。
- `build` 阶段执行用户指定 profile 中已核验的 `pipeline.buildTarget`；如果配置了 `pipeline.rebuildTarget`，会在 buildTarget 后执行重建目标。
- Visual Studio 生成器在 build/install 前会调用 `DetectMSBuildGeneratedPathLength.ps1` 做生成路径长度检测。
- `test` 阶段先用 `ctest -N` 发现测试；配置 `pipeline.testProjects` 时按关键字筛选测试，必须在日志中检查匹配数量。
- `PublishPackage` 阶段会读取当前 profile 的 `publishPackage` 对象，并转调同目录下的 `PublishPackage.ps1`。

退出码速查：

- `0`：成功。
- `2`：配置、参数或 schema 错误。
- `3`：前置条件、环境或路径错误。
- `4`：外部命令失败，例如 configure/build/test/install/PublishPackage。

## 6. 推荐执行流程

只做环境体检：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>" `
  -CheckOnly
```

诊断模式，适合排查工具链和缓存：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>" `
  -Diag `
  -CheckOnly
```

预演完整链路，适合确认将执行哪些命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>" `
  -DryRun
```

增量构建：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>" `
  -Stages build
```

只运行测试阶段：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>" `
  -Stages test
```

完整 R2 构建验证：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>"
```

详细编译输出，适合定位 MSBuild 问题：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>" `
  -Stages build,install `
  -BuildVerbosity diagnostic
```

## 7. R0/R1/R2 平台验证要求

R0 只读分析：

- 读取本分册、`AGENTS.md`、源码、配置、脚本和外部资料。
- 可执行静态核验命令，例如 `rg`、`Get-Content`、`Test-Path`、参数分支检查。
- 不执行完整构建，不修改仓库文件。

R1 文档修改：

- 仅修改说明性 Markdown/RST/README 等不被构建链直接消费的文档。
- 静态核验文档路径、链接、命令语法、平台范围、profile 说明和前后文一致性。
- 不执行 `InvokeQGISBuild.ps1` 完整构建。

R2 源码或构建链修改：

- 必须先创建 R2 验收标准。
- 必须执行本分册中的 `InvokeQGISBuild.ps1` 入口，且显式传入 `-ConfigPath` 和 `-Profile`。
- 必须分析 pipeline 日志和 CMake/MSBuild/CTest 原始输出。
- 任一必选验收项缺证据、失败或阻塞时，总判定为 `未通过` 或 `受控例外（未通过）`，不得写成通过。

## 8. 日志与验收检查点

日志配置检查：

- 必须先读取用户指定 profile 的 `logging` 配置；未配置时，不得把日志位置写成已知结论，必须以脚本实际输出或文档核验结果为准。
- 日志目录必须来自用户指定 profile、脚本文档或实际执行输出，并在答复中记录证据。
- pipeline 日志文件名通常为 `pipeline-<profile>-<timestamp>.log`，以实际输出为准。
- 若启用原始输出日志，会生成类似 `cmake-<profile>-<timestamp>.log` 的文件。

构建日志必须检查：

- 是否出现 `Using config` 和 `Using profile`，且路径/profile 与本轮锁定值一致。
- `Effective stages`、`Effective build targets`、`Effective configuration`、`Effective parallel jobs`、`Effective build verbosity` 是否符合预期。
- `Check-only mode completed. No clean/configure/build/test/install/PublishPackage commands were executed.` 是否只出现在预检场景。
- Visual Studio 路径长度检测是否通过；若失败，按提示缩短 `paths.buildDir`，同时保持 `paths.installDir` 足够短。
- 搜索 `error`、`warning`、`fatal`、`CMake Warning`、`CMake Error`、`MSB`、`LNK`、`ninja: build stopped`。
- 若失败，记录失败阶段、目标/模块、首个有效错误、后续连锁错误和疑似根因。

测试阶段必须检查：

- profile 的 `.cmake.definitions.ENABLE_TESTS` 是否为 `ON`。
- `ctest -N` 发现的总测试数量。
- `pipeline.testProjects` 的筛选关键字。
- `Filter matched tests` 数量。
- 是否存在 `Unmatched testProjects keywords`。
- 是否出现 `No tests were discovered` 或 `No tests matched testProjects`；出现时不能把测试阶段写成通过。
- 失败用例名称、退出码和 `--output-on-failure` 输出。

成功标志：

- 主流程末尾出现 `Pipeline completed successfully in ...`。
- 对应 R2 验收标准中的功能、回归、构建、日志和测试项都有证据并为 `PASS`。

## 9. 发布与运行时验证

发布方式：

- 推荐通过 `InvokeQGISBuild.ps1 -Stages PublishPackage` 或 `-Stages install,PublishPackage` 进入发布阶段。
- 直接运行 `PublishPackage.ps1` 前必须先读 `PublishPackage.md`，并核验 `PublishPackageValidation.json` 和 `PublishPackageValidationSchema.json`。
- `PublishPackage.ps1` 生成的是拷贝版 QGIS 运行目录，不生成 OSGeo4W MSI。
- 需要构建 OSGeo4W 包或 MSI 时，应读取 `QGISPackagingGuide-Win10-VS2022\README.md` 和 `ScriptInvocationOrderGuide.md`，并按 OSGeo4W 工作树、缓存、`x86_64\release`、本地 HTTP 服务和 MSI 输出目录单独声明影响范围。

发布前置条件：

- `paths.installDir` 已由 install 阶段生成，并满足 `PublishPackageValidation.json` 的 `installDir` 规则。
- `paths.osgeo4wRoot` 是完整 OSGeo4W 根目录，并满足 OSGeo4W 包清单、备用包和可选缺失项校验规则。
- `publishPackage.destinationRoot` 的清理行为已确认；若启用 `cleanDestination`，必须声明会清理目标发布目录。
- 启用 ZIP 时，`sevenzip.executablePath` 必须存在。
- 启用运行时验证时，发布日志应能看到 `Runtime validation main window detected` 和 `Runtime validation stable window passed.`。

发布预演：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\InvokeQGISBuild.ps1" `
  -ConfigPath "<user-provided Configure.json>" `
  -Profile "<selectedProfile>" `
  -Stages PublishPackage `
  -DryRun
```

发布常见错误：

- `installDir not found`：install 阶段未成功完成，或 profile 指向错误安装目录。
- `osgeo4wRoot is missing required file/directory/package`：OSGeo4W 根目录不完整，或依赖快照与当前 profile 不匹配。
- `OSGeo4W package list not found`：`osgeo4wRoot\etc\setup` 缺少包清单。
- `Publish package validation file not found`：`PublishPackageValidation.json` 不在发布脚本同目录。
- 运行时验证失败：优先检查 `bin\qgis-qt6-bin.exe`、环境文件、Qt plugin path、Python path、GDAL/PROJ/Qt DLL 和 GUI 启动日志。

## 10. 常见阻塞条件

必须停止并报告 `BLOCKED` 的情况：

- 未能锁定 Windows AMD64 平台或当前平台分册。
- `__QGISCompilationNavigation__` 不存在。
- VS2022 编译资料根目录不存在。
- `Configure.json` 不存在或 JSON/schema 校验失败。
- 用户指定 profile 不存在。
- 用户要求 VS2019，但未提供有效 VS2019 `Configure.json`、profile、构建入口和验证边界。
- profile 的 `paths.sourceDir` 不是当前仓库，且用户未明确允许。
- `paths.osgeo4wRoot`、`paths.gitCommand` 或必要工具不存在。
- 当前任务达到 R2，但没有可执行的构建入口、日志路径或验收标准。
- 测试启用但没有发现测试、筛选不到目标测试，或测试证据不可追溯。
- 发布阶段会清理/覆盖目标目录，但用户未确认影响范围。

不得做的事：

- 不得把任何未由用户明确提供/确认的 profile 当成用户显式指定 profile 的替代。
- 不得把 `codex-automations` 目录下的历史提示文件当作构建入口。
- 不得把 `PublishPackage.ps1` 拷贝版发布流程说成 OSGeo4W MSI 打包流程。
- 不得把 VS2019 历史手工资料说成当前已验证自动化构建入口。
- 不得在缺少 `Test-Path=True` 证据时执行外部路径命令。
- 不得用 `-CheckOnly`、`-DryRun` 或小范围测试替代 R2 完整构建验证。
- 不得将失败、跳过、无匹配测试写成通过。

## 11. 子代理任务包要求

主代理派发任何 QGIS 子代理前，任务包必须先包含本轮基础上下文：

- 当前平台分册路径：`.codex\AgentsDocs\QGIS-Windows-AMD64.md`。
- 本轮目标：需求、范围、禁止事项和期望输出。
- 锁定上下文：`OS/ISA/R 等级`。
- `__QGISCompilationNavigation__` 的真实路径、来源和 `Test-Path` 结果。
- 允许读写范围：仓库内、已核验外部资料目录、构建输出目录、日志目录、临时产物目录；不适用的范围必须明确标为不适用。
- 验证要求：R0/R1 静态核验，或 R2 验收标准与构建日志分析要求。

只有当子代理任务实际依赖编译、测试、安装、打包、发布、插件调试、运行时验证，或依赖 Windows AMD64 构建资料中的 profile/依赖根/工具链上下文时，任务包才必须继续补充以下额外上下文：

- QGIS/Qt/工具链/profile/构建类型等目标上下文的来源和核验结果。
- `Configure.json`、`__OSGeo4WRoot__` 的真实路径、来源和 `Test-Path` 结果。
- 当前 profile 的关键字段：`paths.sourceDir`、`paths.buildDir`、`paths.installDir`、`paths.osgeo4wRoot`、`cmake.generator`、`cmake.architecture`、`cmake.definitions.ENABLE_TESTS`、pipeline 阶段配置、`pipeline.testProjects`。

子代理如果没有收到任务实际依赖的上述额外上下文，不得自行猜测 profile、OSGeo4W 根目录、构建目录、安装目录、日志目录或测试范围；当前任务不依赖这些上下文时，任务包应明确记录“不适用”，且子代理不得执行依赖这些外部构建上下文的命令。
