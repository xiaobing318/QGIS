## 1. 基础约束

### 1.1 通用注意事项

- 以当前仓库根目录 `AGENTS.md` 作为最高优先级指令。
- 可调用当前仓库目录中的 `*.sh`、`*.bat`、`*.pl`、`*.bash`、`*.cmd`、`*.ps1` 以及模型自定义临时脚本获取实际结果。
- 可调用 `__QGISCompilationNavigation__` 目录中的脚本获取实际结果。
- 可调用 `__OSGeo4WRoot__` 目录中的 `*.sh`、`*.bat`、`*.pl`、`*.bash`、`*.cmd`、`*.ps1` 及模型自定义脚本获取实际结果。
- 允许创建临时文件/目录用于复现与验证；验证完成后必须清理，不得残留。
- 文档中的账号、密码、提取码、代理地址等历史示例默认视为敏感/过期信息，仅用于背景理解，不可复用或扩散。
- 遇到不确定结论时，必须回到源码、脚本或官方文档二次核验，不能仅凭经验回答。
- 仓库包含跨平台内容（Windows/Linux/macOS + AMD64/ARM64），回答前必须先锁定目标平台与版本。
- 对会写入仓库目录的脚本，执行前必须声明影响范围，执行后必须清理新增产物。

### 1.2 占位符注意事项

- 使用 `__QGISCompilationNavigation__` 占位符表示本地资料所在目录，例如 E:\github_repos\QGIS-Materials\QGISCompilationNavigation 路径仅用于说明格式，不代表固定盘符。
- 使用 `__OSGeo4WRoot__` 占位符表示 OSGeo4W 依赖库所在目录位置，例如 E:\osgeo4w-setup\OSGeo4W34407 路径仅用于说明格式，不代表固定盘符。
- 占位符真实值解析顺序必须为：`已验证的上下文值` > `Configure.json` > `询问提问者`。若上下文中已有已验证值，必须直接复用，不得重复猜测。
- 当上下文未提供已验证值时，必须优先尝试从 `Configure.json` 获取：读取 `activeProfile` 对应的 `profiles.<activeProfile>.paths.osgeo4wRoot` 作为 `__OSGeo4WRoot__`，并由该 `Configure.json` 的目录层级反推出 `__QGISCompilationNavigation__`。若 `Configure.json` 不存在、字段缺失、无法反推路径，或 `Test-Path` 校验失败，必须立即中止后续执行并向提问者索取路径，不得跳过。
- 在全新对话中，必须在首个有效响应内明确说明 `__QGISCompilationNavigation__` 与 `__OSGeo4WRoot__` 的真实取值、取值来源（上下文/Configure.json/提问者）及 `Test-Path` 结果。
- 在没有解析出 `__QGISCompilationNavigation__` 或 `__OSGeo4WRoot__` 的真实有效值前，不得执行依赖外部路径的命令。占位符解析后，必须先检查路径存在性；仅在 `Test-Path=True` 时继续执行。若 `Test-Path=False`，必须停止执行并提示缺失路径。
  ```powershell
  # 优先：上下文已验证值；其次：Configure.json；最后：询问提问者
  $QGISMaterialsPath = "__QGISCompilationNavigation__"
  $OSGeo4WRootPath = "__OSGeo4WRoot__"
  $ConfigurePath = "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\codex-automations\Configure.json"
  Test-Path $QGISMaterialsPath
  Test-Path $OSGeo4WRootPath
  Test-Path $ConfigurePath
  ```
- 当上下文已得到占位符真实值且有效时，后续引用路径必须自动替换并拼接到真实目录。

### 1.3 文件权限注意事项

- 对当前仓库内所有文件具备读取、执行、修改、创建权限；除临时创建文件/目录外，对文件进行删除操作之前必须明确提示。
- 对当前仓库外路径默认仅允许读取、执行，不允许进行删除、修改、创建等写入操作。
- 仓库外写入唯一例外：仅可在已确认有效的 `__QGISCompilationNavigation__`、`__OSGeo4WRoot__` 及其编译输出目录下创建/修改/删除临时产物（如日志、缓存、中间文件）用于验证；执行前必须声明影响范围，验证完成后必须清理。

### 1.4 文件编码注意事项

- 所有文本文件按 UTF-8 字符集编码读取使用。
- 在使用 PowerShell 读取文本文件时，优先使用 `Get-Content -Raw -Encoding UTF8 <path>` 命令。
- 如果读取的内容出现乱码，应该先做字符集编码核验，然后再继续分析。

## 2. 任务判级

### 2.1 判级总则

- 回归等级按“当前任务的实际操作类型”判定，且三者互斥：`R0`（只读）、`R1`（仅改说明/解释性文档）、`R2`（改源码/构建链文件）。
- 同一任务中若同时出现多种操作，按最高等级执行（`R2 > R1 > R0`）。
- 若任务执行过程中操作类型发生变化，必须立即升级等级并按新等级重做对应验证。
- 判级以“当前任务实际处理对象与操作”为准，不因工作区中与任务无关的历史脏状态改变等级。

### 2.2 R0 等级（只读）

- 定义：只进行读取、理解、分析、排查，不对仓库文件做 `新增`、`删除`、`修改`。
- 典型场景：阅读源码理解架构、分析潜在问题、静态核验脚本参数、定位故障点、解释构建流程。
- 执行要求：仅做静态核验，不执行自动化编译。

### 2.3 R1 等级（仅说明/解释性文档修改）

- 定义：只对说明/解释性文档做 `新增`、`删除`、`修改`，且这些文档不被构建链或代码生成流程直接消费。
- 典型文件：`*.md`、`*.rst`、`README*`、`CONTRIBUTING*`、`INSTALL*`、`NEWS*`，以及 `doc/` 下纯说明展示类内容。
- 执行要求：执行文档静态核验，不执行自动化编译。

### 2.4 R2 等级（源码/构建链文件修改）

- 定义：对源码、构建脚本、构建模板、依赖与打包配置等构建链相关文件执行了 `新增`、`删除`、`修改`。
- 至少包含以下类型或等效影响对象：
  - `C/C++` 源码与头文件：`*.c`、`*.cc`、`*.cpp`、`*.cxx`、`*.h`、`*.hh`、`*.hpp`、`*.hxx`。
  - `Python/绑定/Qt 生成输入`：`*.py`、`*.sip`、`*.sip.in`、`*.ui`、`*.qrc`。
  - `CMake/构建模板`：`CMakeLists.txt`、`*.cmake`、`*.ctest`、`*.in`。
  - `构建/CI/打包脚本`：`*.sh`、`*.ps1`、`*.bat`、`*.cmd`、`*.pl`。
  - 位于 `.ci/`、`cmake/`、`cmake_templates/`、`debian/`、`linux/`、`mac/`、`ms-windows/`、`vcpkg/`、`scripts/`、`python/` 且被构建链直接消费的 `*.json`、`*.yml`、`*.yaml`、`*.ini`、`*.conf`、`*.install` 等配置文件。
  - 扩展名不典型但被 `CMake`、构建脚本、依赖准备脚本、打包脚本、代码生成流程直接读取的文本文件。
- 执行要求：必须调用 `InvokeQGISBuild.ps1` 执行编译验证，并输出详细日志分析；不允许仅描述手动回归流程而不实际调用脚本。

## 3. 执行规范

### 3.1 参数核验与前置条件

- 优先用静态方式确认脚本参数支持情况（如 `rg -n` 检查参数分支）。
- 仅执行已确认存在的参数，避免不存在参数触发默认执行。
- 对依赖固定输入的脚本，执行前先检查前置条件是否存在（如 `remote-setup.ini`、`x86_64/release`）。
- 对会修改目录内容的脚本，执行前声明影响范围，执行后给出回滚/清理方案。
- 静态核验命令参考：
  ```powershell
  # PowerShell
  rg -n -- '(?i)(if /I "%~1"|--help|usage|remote-setup\.ini|x86_64/release)' <script_path>
  ```
  ```bash
  # Bash/Cygwin
  rg -n -- 'if /I "%~1"|--help|usage|remote-setup\.ini|x86_64/release' <script_path>
  ```

### 3.2 执行环境与平台规范

- 先锁定并明确本次任务适用范围：`OS/ISA/QGIS/Qt/工具链`。
- Windows 批处理脚本优先在 `cmd/PowerShell + OSGeo4W` 环境执行；`*.sh`、`*.pl` 脚本优先在 `Cygwin bash` 执行。
- 无法实机执行时，必须说明验证边界与替代验证方式（语法检查、参数检查、静态审阅等）。

### 3.3 副作用控制与清理规范

- 对会改变系统状态的命令（安装、删除、覆盖、写注册表、改环境变量），执行前必须说明影响范围。
- 对会写入仓库目录或外部允许目录的脚本，执行后必须说明新增产物并给出清理结果。
- 允许创建临时文件/目录用于复现与验证；验证完成后必须清理，不得残留。

## 4. 回归与编译验证

### 4.1 执行总则

- 本章用于规定 `R0/R1/R2` 的验证流程与证据记录要求，判级依据见 `2. 任务判级`。
- 执行前必须锁定适用范围：`OS/ISA/QGIS/Qt/工具链`。
- 执行前必须明确 `__QGISCompilationNavigation__` 与 `__OSGeo4WRoot__` 的真实值并执行 `Test-Path`；任一为 `False` 必须中止并报告缺失项。
- 对存在写入行为的操作，执行前说明影响范围；验证完成后说明清理结果。

### 4.2 R0 流程（只读）

1. 确认当前任务仅包含读取、理解、分析、排查，不包含任何文件改动。
2. 仅执行静态核验（如源码阅读、参数分支检查、路径存在性检查、逻辑链路检查）。
3. 记录证据：核验对象、使用命令、关键发现、结论。
4. 明确说明本次为 `R0`，且未执行自动化编译。

### 4.3 R1 流程（仅说明/解释性文档修改）

1. 确认当前任务仅修改说明/解释性文档，且未改动构建链相关文件。
2. 对文档修改执行静态核验，至少覆盖：路径有效性、链接有效性、命令语法自洽、平台与版本说明一致性、前后文一致性。
3. 记录证据：改动文件清单、静态检查命令、发现问题与处理结论。
4. 明确说明本次为 `R1`，且不执行自动化编译。
5. 若执行中出现源码/构建链文件改动，必须立即升级到 `R2` 并执行 `4.4`。

### 4.4 R2 流程（源码/构建链文件修改）

1. 明确列出本次改动文件或模块，并说明为何归类为 `R2`。
2. 先做静态参数核验与前置条件检查；若入口脚本缺失、参数不匹配或前置条件不满足，先报告缺失项并停止默认编译流程。
3. 必须调用以下入口执行编译验证（`__QGISCompilationNavigation__` 需替换为已核验且 `Test-Path=True` 的真实路径）：
   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
     "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\codex-automations\InvokeQGISBuild.ps1" `
     -ConfigPath "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\codex-automations\Configure.json"
   ```
4. 对日志做详细、结构化分析，至少覆盖：`error`、`warning`、`fatal`、`CMake Warning`、`CMake Error`、`MSB`、`LNK`、`ninja: build stopped`，并明确失败阶段、对应目标/模块、首个有效错误、后续连锁错误、疑似根因。
5. 若当前 profile 启用了测试（例如 `.cmake.ENABLE_TESTS=ON`），必须在日志分析中单独说明测试阶段结果（通过/失败、关键失败用例、退出码）。
6. 记录证据：执行环境、真实路径、执行命令、关键日志、最终结论、未覆盖项。
7. 明确说明本次为 `R2`，且已实际调用 `InvokeQGISBuild.ps1`；不得仅描述手动回归流程。

## 5. 输出要求

### 5.1 总体目标

总体目标是对当前仓库内容进行深入、可验证分析，帮助提问者实现或者理解：
- 实现用户的需求，例如添加一个新的功能，修复存在的问题等。
- 项目整体架构，例如目录分工、平台分支、脚本职责。
- 不同场景的正确入口（编译、依赖下载、解包、运行、打包、插件调试）。
- `为什么这么做` 与 `具体怎么做`（含可复现步骤）。
- 版本差异、环境差异、依赖差异下的风险点与替代方案。

### 5.2 回答内容要求

- 使用中文回答，表达应通俗、直接、可执行。
- 涉及流程时必须给出前置条件、操作步骤、检查点、常见错误与处理办法。
- 未指定语种时：编写文档和代码注释默认使用中文。
- 结论必须附证据来源（本地路径、脚本输出、官方链接）；若资料冲突需说明取舍依据。
- 涉及版本或平台的结论必须显式标注适用范围（例如 `Windows 10/11 + VS2022 + Qt6`）。
- 若提问者未明确 OS、ISA、QGIS/Qt/工具链版本，需先询问；无法询问时必须声明假设与风险。
- 回答本仓库问题时，优先依据以下来源并给出可核验证据：
- 官方文档与官方仓库。
- 本仓库 README 与平台导航文档。
- 本仓库脚本真实参数与实际输出。
- 已验证的本地实践资料。
- `__QGISCompilationNavigation__` 路径下资料。
- `__OSGeo4WRoot__` 目录中的脚本与文档。

### 5.3 建议回答流程

1. 先确认目标维度：OS、ISA、QGIS 版本、Qt 版本、Visual Studio 工具链版本。
2. 在首个有效响应中明确给出 `__QGISCompilationNavigation__` 与 `__OSGeo4WRoot__` 的真实值及 `Test-Path` 结果；解析顺序固定为：`已验证上下文值` > `Configure.json` > `询问提问者`。
3. 先做静态参数核验，再执行脚本支持的参数。
4. 给出可执行命令、预期输出、校验点。
5. 补充失败场景与排障命令。
6. 标注结论来源（本地文件或官方资料）。

## 6. 目录导航与参考链接

### 6.1 目录级入口

- `__QGISCompilationNavigation__\CommonMaterials\`
- `__QGISCompilationNavigation__\QGIS-Linux-AMD64\`
- `__QGISCompilationNavigation__\QGIS-Linux-ARM64\`
- `__QGISCompilationNavigation__\QGIS-macOS-ARM64\`
- `__QGISCompilationNavigation__\QGIS-Windows-AMD64\`
- `__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\codex-automations\`
- `__QGISCompilationNavigation__\QGIS-Windows-ARM64\`
- `__OSGeo4WRoot__`

> [!NOTE]
> 1. 上述目录为入口导航；文件级说明与作用以各目录 README/Guide 为准。
> 2. 若目录文档与本文件存在冲突，优先遵循当前仓库根目录 `AGENTS.md` 的约束规则。

### 6.2 参考网址

- https://qgis.org/
- https://github.com/qgis/QGIS
- https://github.com/qgis/QGIS/blob/master/INSTALL.md
- https://github.com/jef-n/OSGeo4W
- https://github.com/xiaobing318/OSGeo4W
- https://download.osgeo.org/osgeo4w/
- https://debian.qgis.org/debian/
- https://ubuntu.qgis.org/ubuntu/
