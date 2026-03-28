## 1. 必须遵守的注意事项

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

### 1.5 自动化编译触发条件

- 本节用于判定何种任务必须触发自动化编译与回归测试。凡命中本节条件的任务，必须继续执行 `3.4 回归测试与编译验证要求` 中规定的流程，不得仅以静态分析、口头说明或局部脚本检查替代。
- 自动化编译的触发判断以“是否可能影响 `clean/configure/build/install` 成功与否”为准；只要改动可能影响源码编译、代码生成、构建配置、依赖解析、打包安装或运行时装配，就视为命中本节条件。
- 结合当前仓库文件类型统计，需重点关注的高频文本类型包括 `*.py`、`*.cpp`、`*.h`、`*.in`、`*.ui`、`CMakeLists.txt`、`*.cmake`、`*.sh`、`*.sip` 等；这些类型在本仓库中大量存在，且多数直接参与构建、代码生成或依赖装配流程。
- 以下文件一旦发生 `新增`、`删除`、`修改`，必须触发自动化编译与回归测试，不得仅做静态检查：
- `C/C++` 源码与头文件：`*.c`、`*.cc`、`*.cpp`、`*.cxx`、`*.h`、`*.hh`、`*.hpp`、`*.hxx`。
- `Python/绑定/Qt 生成输入`：`*.py`、`*.sip`、`*.sip.in`、`*.ui`、`*.qrc`。
- `CMake/构建模板`：`CMakeLists.txt`、`*.cmake`、`*.ctest`、`*.in`。
- `构建/CI/打包脚本`：`*.sh`、`*.ps1`、`*.bat`、`*.cmd`、`*.pl`。
- `构建或依赖相关文本配置`：位于 `.ci/`、`cmake/`、`cmake_templates/`、`debian/`、`linux/`、`mac/`、`ms-windows/`、`vcpkg/`、`scripts/`、`python/` 等路径下，且被构建、依赖准备、打包、环境初始化、代码生成流程直接消费的 `*.json`、`*.yml`、`*.yaml`、`*.ini`、`*.conf`、`*.install` 等文本文件。
- 对扩展名本身不足以判断用途的文本文件，必须按“目录位置 + 实际作用”综合判断；只要该文件会被 `CMake`、构建脚本、打包脚本、依赖准备脚本或代码生成流程直接读取，就按“必须触发自动化编译与回归测试”处理，不得因为它是 `*.txt`、`*.json`、`*.xml` 等通用文本格式而跳过。
- 以下情况通常不需要触发自动化编译，可按 `3.4` 中的 `R0` 执行文档静态核验：
- 纯说明文档与纯展示文档：`*.md`、`*.rst`、`README*`、`CONTRIBUTING*`、`INSTALL*`、`NEWS*` 等。
- `doc/` 目录下仅用于说明、展示或排版的 `*.html`、`*.css`、`*.dox` 及作者/赞助者名单等文档内容。
- 不参与构建链路、不会被构建脚本或代码生成流程直接消费的纯文本说明文件。
- 若任务只是对上述编译链相关内容做 `查询`、`排查`、`解释`，即使没有改动文件，也不得只做静态阅读；仍需按 `3.4` 中的 `R1` 规则执行验证。若后续产生文件 `新增`、`删除`、`修改`，则转入 `R2`。

## 2. 总体目标

总体目标是对当前仓库内容进行深入、可验证分析，帮助提问者实现或者理解：
- 实现用户的需求，例如添加一个新的功能，修复存在的问题等。
- 项目整体架构，例如目录分工、平台分支、脚本职责。
- 不同场景的正确入口（编译、依赖下载、解包、运行、打包、插件调试）。
- `为什么这么做` 与 `具体怎么做`（含可复现步骤）。
- 版本差异、环境差异、依赖差异下的风险点与替代方案。

回答本仓库问题时，优先依据以下来源并给出可核验证据：
- 官方文档与官方仓库。
- 本仓库 README 与平台导航文档。
- 本仓库脚本真实参数与实际输出。
- 已验证的本地实践资料。
- `__QGISCompilationNavigation__` 路径下资料。
- `__OSGeo4WRoot__` 目录中的脚本与文档。

## 3. 回答准则

- 使用中文回答，表达应通俗、直接、可执行。
- 涉及流程时必须给出前置条件、操作步骤、检查点、常见错误与处理办法。
- 未指定语种时：编写文档和代码注释默认使用中文。
- 结论必须附证据来源（本地路径、脚本输出、官方链接）；若资料冲突需说明取舍依据。
- 涉及版本或平台的结论必须显式标注适用范围（例如 `Windows 10/11 + VS2022 + Qt6`）。
- 若提问者未明确 OS、ISA、QGIS/Qt/工具链版本，需先询问；无法询问时必须声明假设与风险。

### 3.1 建议回答流程

1. 先确认目标维度：OS、ISA、QGIS 版本、Qt 版本、Visual Studio 工具链版本。
2. 在首个有效响应中明确给出 `__QGISCompilationNavigation__` 与 `__OSGeo4WRoot__` 的真实值及 `Test-Path` 结果；解析顺序固定为：`已验证上下文值` > `Configure.json` > `询问提问者`。
3. 先做静态参数核验，再执行脚本支持的参数。
4. 给出可执行命令、预期输出、校验点。
5. 补充失败场景与排障命令。
6. 标注结论来源（本地文件或官方资料）。

### 3.2 必须遵守的核验规则

- 优先用静态方式确认脚本参数支持情况（如 `rg -n` 检查参数分支）。
- 仅执行已确认存在的参数，避免不存在参数触发默认执行。
- 对依赖固定输入的脚本，执行前先检查前置条件是否存在（如 `remote-setup.ini`、`x86_64/release`）。
- 对会修改目录内容的脚本，执行前声明影响范围，执行后给出回滚/清理方案。

### 3.3 执行与验证规范

- Windows 批处理脚本优先在 `cmd/PowerShell + OSGeo4W` 环境执行；`*.sh`、`*.pl` 脚本优先在 `Cygwin bash` 执行。
- 对会改变系统状态的命令（安装、删除、覆盖、写注册表、改环境变量），执行前必须说明影响范围。
- 无法实机执行时，必须说明验证边界与替代验证方式（语法检查、参数检查、静态审阅等）。
- 静态核验命令参考
  ```powershell
  # PowerShell
  rg -n -- '(?i)(if /I "%~1"|--help|usage|remote-setup\.ini|x86_64/release)' <script_path>
  ```
  ```bash
  # Bash/Cygwin
  rg -n -- 'if /I "%~1"|--help|usage|remote-setup\.ini|x86_64/release' <script_path>
  ```

### 3.4 回归测试与编译验证要求

- 本节用于规定自动化编译与回归测试的执行等级、执行入口、日志分析要求和结果判定标准。`1.5` 负责判定是否触发，本节负责规定触发后的完整验证流程。
- 回归等级必须按以下互斥顺序判定：先判断是否命中 `1.5`，未命中则为 `R0`；命中后再判断当前任务是否对 `1.5` 范围内文件产生 `新增`、`删除`、`修改`，无变更则为 `R1`，有变更则为 `R2`。同一任务不得同时归入多个等级；若执行过程中等级条件发生变化，必须立即升级并按更高等级重做验证。
- 判级以“当前任务实际处理的对象与操作”为准，不因工作区中与当前任务无关的历史脏状态而改变等级；若一个任务同时涉及多个模块或多种类型内容，默认按最高等级执行。

#### 3.4.1 回归等级

R0：未命中 `1.5` 触发条件。仅涉及说明文档、展示文档或未参与构建链路的说明性文本；不得调用自动化编译，只执行文档静态核验。

R1：已命中 `1.5` 触发条件，但当前任务未对 `1.5` 范围内文件执行 `新增`、`删除`、`修改`。典型场景是查询、排查、解释、环境核验；必须执行自动化编译验证与详细日志分析。

R2：已命中 `1.5` 触发条件，且当前任务对 `1.5` 范围内文件执行了 `新增`、`删除`、`修改`。必须调用 `InvokeQGISBuild.ps1` 执行编译验证，并输出详细日志分析，不允许只说明手动回归流程而不调用具体的脚本进行编译验证。

#### 3.4.2 R0/R1/R2 回归流程

`R0` 流程：
1. 确认当前任务未命中 `1.5`；若审查过程中发现文件或脚本实际属于编译链相关内容，必须停止 `R0` 并重新判级。
2. 锁定本次文档核验对象，例如说明文档、展示文档、帮助文本、路径说明、命令示例、版本说明。
3. 对修改内容做静态审查，至少检查：路径是否真实存在、链接是否有效、命令语法是否自洽、平台与版本说明是否一致、前后文是否冲突。
4. 对文档中涉及的关键命令，仅做静态可执行性检查，例如参数存在性检查、帮助信息检查、路径存在性检查；不得调用自动化编译。
5. 记录证据：核验文件、静态检查命令、发现的问题、未调用自动化编译的原因与结论。

`R1/R2` 通用执行要求：
1. 锁定适用范围：`OS/ISA/QGIS/Qt/工具链`。
2. 明确 `__QGISCompilationNavigation__` 与 `__OSGeo4WRoot__` 的真实值并执行 `Test-Path`；优先使用已验证上下文值，其次从 `Configure.json` 解析，若仍无法通过校验则询问提问者；任一为 `False` 必须中止并报告缺失项。
3. 锁定排查对象或改动模块：包括但不限于 `src`、`python`、`CMake`、依赖链、代码生成链、打包安装链这些内容。
4. 先做静态参数核验与前置条件检查；若入口脚本缺失、参数不匹配或前置条件不满足，必须停止默认编译流程并先给出缺失项与修复建议，不得盲目回退到未知脚本。
5. 编译必须使用 `__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\codex-automations\` 作为单一入口，并按该目录 README/Guide 的入口脚本执行。标准调用示例如下，其中 `__QGISCompilationNavigation__` 需要替换为已核验通过 `Test-Path=True` 的真实路径：
   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
     "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\codex-automations\InvokeQGISBuild.ps1" `
     -ConfigPath "__QGISCompilationNavigation__\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022\codex-automations\Configure.json"
   ```
6. 对日志做详细、结构化分析，不能只做关键词命中统计；至少要分别检查并说明是否存在：编译错误、严重编译警告、构建系统错误、链接错误、打包/安装阶段错误，以及首个有效错误引发的连锁报错。日志分析时至少覆盖：`error`、`warning`、`fatal`、`CMake Warning`、`CMake Error`、`MSB`、`LNK`、`ninja: build stopped` 等信号，并给出失败阶段、对应目标或模块、首个有效错误、后续连锁错误与疑似根因。
7. 记录证据：执行环境、真实路径、执行命令、关键日志、结果、未覆盖项。

`R1` 流程：
1. 确认当前任务已命中 `1.5`，但当前任务未对 `1.5` 范围内文件执行 `新增`、`删除`、`修改`。
2. 执行上文“`R1/R2` 通用执行要求”全部步骤，验证当前仓库状态、构建环境或待排查对象是否正常。
3. 在结论中明确说明本次等级为 `R1` 的原因，即“命中编译链，但无当前任务文件变更”；若执行过程中产生文件变更，必须立即升级为 `R2` 并重走 `R2` 流程。

`R2` 流程：
1. 确认当前任务已命中 `1.5`，且当前任务对 `1.5` 范围内文件执行了 `新增`、`删除`、`修改`。
2. 在执行编译前，明确列出本次改动文件或改动模块，并说明这些改动为何命中 `1.5`。
3. 执行上文“`R1/R2` 通用执行要求”全部步骤，验证改动后的构建链路是否仍能通过。
4. 在结论中明确说明本次等级为 `R2` 的原因，即“命中编译链且存在当前任务文件变更”；除通用日志分析外，还必须说明改动是否直接导致新增错误、严重警告、构建失败、链接失败或安装失败。

## 4. 目录级入口

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

## 5. 参考网址

- https://qgis.org/
- https://github.com/qgis/QGIS
- https://github.com/qgis/QGIS/blob/master/INSTALL.md
- https://github.com/jef-n/OSGeo4W
- https://github.com/xiaobing318/OSGeo4W
- https://download.osgeo.org/osgeo4w/
- https://debian.qgis.org/debian/
- https://ubuntu.qgis.org/ubuntu/
