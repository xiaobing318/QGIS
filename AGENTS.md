## 1. 注意事项

### 1.1 通用注意事项

- 以当前仓库根目录中的 `AGENTS.md` 为最高优先级指令。
- 模型可以调用仓库中的 `*.sh`、`*.bat`、`*.pl`、`*.bash` 及模型自定义脚本，以获取实际运行结果。
- 允许创建临时文件/目录用于复现、排障、验证；验证结束后必须删除，不能残留在仓库中。
- 对文档中的账号、密码、提取码、代理地址等历史示例信息，默认视为敏感/过期信息：仅用于理解背景，不可复用，不可在回答中扩散。
- 遇到不确定结论时，必须回到源码/脚本/官方文档进行二次核验，不能只凭经验回答。
- 当前仓库包含跨平台内容（Windows/Linux/macOS + AMD64/ARM64），回答时必须先锁定目标平台与版本再给步骤。
- 对会写入仓库目录的脚本（例如自动生成 `logs` 目录），需先声明影响范围，再执行，并在验证后清理新增产物。

### 1.2 文件编码注意事项

- 当前仓库目录中的文档与脚本按 UTF-8 读取。
- 在 PowerShell 中读取文本时，优先使用命令 `Get-Content -Raw -Encoding UTF8 <path>`。
- 若出现乱码，不得直接下结论，首先做编码核验然后再继续分析。

### 1.3 路径占位符替换（必须遵守）

为了在当前仓库中稳定引用 `QGIS-Materials\QGISCompilationNavigation\QGIS-Windows-AMD64` 资料，统一使用 `__QGIS_MATERIALS_WIN_AMD64_PREFIX__` 前缀占位符，该占位符在后续手动替换为真实路径时，例如：`I:\github_repos\QGIS-Materials\QGISCompilationNavigation\QGIS-Windows-AMD64`。如果上下文中已经获取得到 `__QGIS_MATERIALS_WIN_AMD64_PREFIX__` 占位符的具体值并且有效，则在后续引用资料的时候需要自动替换、拼接成对应的路径。

### 删除权限（必须遵守）

不得整体删除该仓库中的所有文件和编译 QGIS 所依赖的第三方库 OSGeo4W 根目录中的所有文件。

## 2. 总体目标

总体目标是对当前仓库内容进行深入、可验证的分析，帮助提问者理解：
- 项目整体架构，即目录分工、平台分支、脚本职责。
- 不同场景的正确入口，例如编译、依赖下载、解包、运行、打包、插件调试。
- `为什么这么做` 和 `具体怎么做`，不仅给结论，还给可复现步骤。
- 在版本差异、环境差异、依赖差异下的风险点和替代方案。

回答关于本仓库的问题时，应优先基于下列要求，始终对结论保持审慎，必要时给出核验命令、预期输出和回滚/纠错方法，确保回答真实、有效、可复现：
- 官方文档与官方仓库。
- 本仓库 README 与各平台导航文档。
- 本仓库脚本的真实参数与实际输出。
- 已验证的本地实践资料。
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__` 路径下的 Windows AMD64 专项资料。

## 3. 回答准则

- 使用中文回答。
- 采用通俗、直接、可执行的表达方式，避免抽象空话。
- 涉及流程时必须给出完整步骤、前置条件、检查点、常见错误与处理办法。
- 文档语种：若提问者未指定，创建/修改文档默认使用中文。
- 代码注释语种：按提问者要求；未指定则默认英文注释。
- 结论必须带证据来源，例如本地文件路径、脚本输出、官方链接。若资料冲突，需要显式指出冲突并说明取舍依据。
- 涉及版本/平台的结论，必须显式写明适用范围，例如 `Windows + VS2022 + Qt6`、`Ubuntu 22.04 + glibc 2.35` 等。
- 若提问者未明确 OS、ISA、QGIS/Qt/工具链版本，需先补充询问；若无法询问，必须显式声明当前假设及其风险。

### 3.1 建议的回答流程

1. 先确认目标维度：OS、ISA、QGIS 版本、Qt 版本、工具链版本。
2. 先做静态参数核验，例如脚本内参数解析、配置区域、默认路径；仅在脚本明确支持时，再执行例如 `--help`、`--check-only`、`--dry-run`、`--diag` 等参数。
3. 给出可执行步骤即命令 + 预期输出 + 校验点。
4. 补充失败场景和排障命令。
5. 标注结论来自哪个本地文件或官方资料。

### 3.2 必须遵守的核验规则

- 先用静态方式确认脚本参数支持情况，优先 `rg -n` 检查脚本中的参数分支，例如 `--help`、`--QGISInstallDir`、`--qt6`、`--build-dir` 等参数。
- 仅对确认支持的参数做实际执行核验，避免用不存在参数触发默认执行。
- 对依赖固定输入文件/目录的脚本，先检查前置条件是否存在，再执行；例如 `remote-setup.ini`、`x86_64/release` 等。
- 对会修改目录内容的脚本，执行前必须声明影响范围，执行后给出回滚/清理方案。

### 3.3 执行与验证规范

- 默认优先适用范围：`Windows 10/11 + AMD64 + VS2022(v143) + Qt6 + OSGeo4W`。
- 若改为 `VS2019` 或 `Qt5`，必须显式标注为兼容/历史链路，并说明差异风险。
- Windows 批处理脚本优先在 `cmd/PowerShell + OSGeo4W` 环境执行；`*.sh`、`*.pl` 脚本优先在 `Cygwin bash` 环境执行。
- 对会改系统状态的命令（安装、删除、覆盖、写注册表、改环境变量）需要先说明影响范围，再执行。
- 无法在当前环境执行时，必须明确说明 `验证边界` 和 `替代验证方式`，例如语法检查、参数检查、静态审阅。

### 3.4 Windows AMD64 场景入口

- 编译与打包总览：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\CommonMaterials\QGISCompilationAndPackagingGuide.md`
- 工具链兼容性：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\CommonMaterials\MSVC版本体系与兼容性指南.md`
- 环境变量基础：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\CommonMaterials\环境变量指南.md`
- VS2022 编译主线：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationGuide-Win10-VS2022\README.md`
- VS2022 环境脚本：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationGuide-Win10-VS2022\msvc-env-setup4vs2022.bat`
- VS2019 兼容链路：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationGuide-Win10-VS2019\README.md`
- 打包流程：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPackagingGuide-Win10\QGISPackagingGuide.md`
- setup.hint 生成脚本：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPackagingGuide-Win10\GenerateNestedHintsOptimized.pl`
- 包完整性检查脚本：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPackagingGuide-Win10\VerifyNestedPackages.sh`
- Python 插件目录说明：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPythonPlugins\README.md`
- Python 插件调试指南：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPythonPlugins\QGISPythonPluginsDebugGuide.md`
- Python 插件环境脚本（Qt6）：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPythonPlugins\QGISQt6PythonEnvConfig.bat`
- Python 插件环境脚本（Qt5）：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPythonPlugins\QGISQt5PythonEnvConfig.bat`
- 编译阶段 Python 错误分析：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationPythonErrorsAnalysis\QGISCompilationPythonErrorsAnalysis.md`
- 附带环境模板：`__QGIS_MATERIALS_WIN_AMD64_PREFIX__\附带文件\`

### 3.5 脚本参数静态核验清单

- `msvc-env-setup4vs2022.bat`：支持 `x86|x86_64`、`--qt5`、`--qt6`、`--no-o4w`、`--osgeo4w-root`、`--sdk`、`--cygwin-root`、`--no-cygwin`、`--build-dir`、`--cmake-gui`、`--help`。
- `msvc-env-setup4vs2019.bat`、`msvc-env-setup4vs2022-qt5-deprecated.bat`、`msvc-env-setup4vs2022-qt6-deprecated.bat`：按脚本分支仅支持位置参数 `x86|x86_64`（无完整长参数集合）。
- `QGISQt6PythonEnvConfig.bat`、`QGISQt5PythonEnvConfig.bat`：支持 `--help` 和 `--QGISInstallDir <path>`。
- `GenerateNestedHints.pl`、`GenerateNestedHintsOptimized.pl`：依赖 `remote-setup.ini` 与 `x86_64/release` 目录。
- `VerifyNestedPackages.sh`：按脚本内固定路径检查 `x86_64/release` 下关键包目录、`*.tar.bz2` 与 `setup.hint`。

> [!NOTE]
> 1. 建议先执行静态核验命令：`rg -n -- "if /I \"%~1\"|--help|usage|remote-setup.ini|x86_64/release" <script_path>`。
> 2. 仅当静态核验确认参数存在时，再进行实机执行验证。

## 4. 外部资料目录

- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\CommonMaterials\`：跨主题通用知识（MSVC 兼容性、环境变量、编译打包总览）。
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationGuide-Win10-VS2022\`：Windows + VS2022 主编译流程与环境脚本。
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationGuide-Win10-VS2019\`：VS2019 兼容流程（历史链路）。
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPackagingGuide-Win10\`：OSGeo4W 打包流程与辅助脚本。
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPythonPlugins\`：插件调试与开发环境配置。
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationPythonErrorsAnalysis\`：编译阶段 Python 模块错误分析。
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\附带文件\`：环境脚本模板与说明文件。

## 5. 参考网址

- https://qgis.org/
- https://github.com/qgis/QGIS
- https://github.com/qgis/QGIS/blob/master/INSTALL.md
- https://github.com/jef-n/OSGeo4W
- https://github.com/xiaobing318/OSGeo4W
- https://download.osgeo.org/osgeo4w/
- https://debian.qgis.org/debian/
- https://ubuntu.qgis.org/ubuntu/
