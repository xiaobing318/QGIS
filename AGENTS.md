## 1. 注意事项

### 1.1 必须遵守

**1. 占位符说明**
- 本地资料路径占位符统一使用 `__QGIS_MATERIALS_WIN_AMD64_PREFIX__`，例如 E:\github_repos\QGIS-Materials 路径仅用于说明格式，不代表固定盘符。
- OSGeo4W 路径占位符统一使用 `__OSGEO4W_ROOT__`，例如 E:\osgeo4w-setup\OSGeo4W34407 路径仅用于说明格式，不代表固定盘符。
- 未解析出 `__QGIS_MATERIALS_WIN_AMD64_PREFIX__` 或 `__OSGEO4W_ROOT__` 的真实有效值前，不得执行依赖外部路径的命令。占位符解析后，必须先检查路径存在性；仅在 `Test-Path=True` 时继续执行。若 `Test-Path=False`，必须停止执行并提示缺失路径。
  ```powershell
  $QGISMaterialsPath = "__QGIS_MATERIALS_WIN_AMD64_PREFIX__"
  $OSGeo4WRootPath = "__OSGEO4W_ROOT__"
  Test-Path $QGISMaterialsPath
  Test-Path $OSGeo4WRootPath
  ```
- 当上下文已得到占位符真实值且有效时，后续引用路径必须自动替换并拼接到真实目录。

**2. 文件权限说明**

- 对当前仓库内文件具备读取、执行权限；除临时创建文件/目录外，删除其他文件前必须明确提示。
- 对当前仓库外文件具备读取、执行权限；除临时创建文件/目录外，删除其他文件前必须明确提示。


### 1.2 通用注意事项

- 以当前仓库根目录 `AGENTS.md` 作为最高优先级指令。
- 可调用当前仓库目录中的 `*.sh`、`*.bat`、`*.pl`、`*.bash` 及模型自定义脚本获取实际结果。
- 可调用 `__QGIS_MATERIALS_WIN_AMD64_PREFIX__` 目录中的脚本获取实际结果。
- 可调用 `__OSGEO4W_ROOT__` 目录中的 `*.sh`、`*.bat`、`*.pl`、`*.bash` 及模型自定义脚本获取实际结果。
- 允许创建临时文件/目录用于复现与验证；验证完成后必须清理，不得残留。
- 文档中的账号、密码、提取码、代理地址等历史示例默认视为敏感/过期信息，仅用于背景理解，不可复用或扩散。
- 遇到不确定结论时，必须回到源码、脚本或官方文档二次核验，不能仅凭经验回答。
- 仓库包含跨平台内容（Windows/Linux/macOS + AMD64/ARM64），回答前必须先锁定目标平台与版本。
- 对会写入仓库目录的脚本，执行前必须声明影响范围，执行后必须清理新增产物。

### 1.3 文件编码注意事项

- 当前仓库文档与脚本按 UTF-8 读取。
- 在 PowerShell 读取文本时，优先使用 `Get-Content -Raw -Encoding UTF8 <path>`。
- 若出现乱码，先做编码核验，再继续分析。

## 2. 总体目标

总体目标是对当前仓库内容进行深入、可验证分析，帮助提问者理解：
- 项目整体架构（目录分工、平台分支、脚本职责）。
- 不同场景的正确入口（编译、依赖下载、解包、运行、打包、插件调试）。
- `为什么这么做` 与 `具体怎么做`（含可复现步骤）。
- 版本差异、环境差异、依赖差异下的风险点与替代方案。

回答本仓库问题时，优先依据以下来源并给出可核验证据：
- 官方文档与官方仓库。
- 本仓库 README 与平台导航文档。
- 本仓库脚本真实参数与实际输出。
- 已验证的本地实践资料。
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__` 路径下资料。
- `__OSGEO4W_ROOT__` 目录中的脚本与文档。

## 3. 回答准则

- 使用中文回答，表达应通俗、直接、可执行。
- 涉及流程时必须给出前置条件、操作步骤、检查点、常见错误与处理办法。
- 未指定语种时：文档默认中文；代码注释默认英文。
- 结论必须附证据来源（本地路径、脚本输出、官方链接）；若资料冲突需说明取舍依据。
- 涉及版本或平台的结论必须显式标注适用范围（例如 `Windows + VS2022 + Qt6`）。
- 若提问者未明确 OS、ISA、QGIS/Qt/工具链版本，需先询问；无法询问时必须声明假设与风险。

### 3.1 建议回答流程

1. 先确认目标维度：OS、ISA、QGIS 版本、Qt 版本、工具链版本。
2. 先做静态参数核验，再执行脚本支持的参数。
3. 给出可执行命令、预期输出、校验点。
4. 补充失败场景与排障命令。
5. 标注结论来源（本地文件或官方资料）。

### 3.2 必须遵守的核验规则

- 优先用静态方式确认脚本参数支持情况（如 `rg -n` 检查参数分支）。
- 仅执行已确认存在的参数，避免不存在参数触发默认执行。
- 对依赖固定输入的脚本，执行前先检查前置条件是否存在（如 `remote-setup.ini`、`x86_64/release`）。
- 对会修改目录内容的脚本，执行前声明影响范围，执行后给出回滚/清理方案。

### 3.3 执行与验证规范

- 默认优先适用范围：`Windows 10/11 + AMD64 + VS2022(v143) + Qt6 + OSGeo4W`。
- 若改用 `VS2019` 或 `Qt5`，必须标注为兼容/历史链路并说明风险。
- Windows 批处理脚本优先在 `cmd/PowerShell + OSGeo4W` 环境执行；`*.sh`、`*.pl` 脚本优先在 `Cygwin bash` 执行。
- 对会改变系统状态的命令（安装、删除、覆盖、写注册表、改环境变量），执行前必须说明影响范围。
- 无法实机执行时，必须说明验证边界与替代验证方式（语法检查、参数检查、静态审阅等）。
- 静态核验命令参考：
  - PowerShell：`rg -n -- '(?i)(if /I "%~1"|--help|usage|remote-setup\.ini|x86_64/release)' <script_path>`
  - Bash/Cygwin：`rg -n -- 'if /I "%~1"|--help|usage|remote-setup\.ini|x86_64/release' <script_path>`

### 3.4 回归测试要求

触发条件：只要改动涉及文档、脚本、源码或构建配置，即需按等级执行回归，多模块改动时，按最高等级执行。

#### 3.4.1 回归等级

R0：纯文档改动：对修改后的文档进行审查，例如路径/链接/命令语法核验；关键命令做静态可执行性检查。

R1：脚本文件改动：静态参数核验 + 执行参数验证 + 输入前置条件检查。

R2：源码/构建改动：这类长耗时场景只需要说明手动回归测试流程。

#### 3.4.2 R2 手动回归流程

1. 锁定适用范围：`OS/ISA/QGIS/Qt/工具链`。
2. 锁定改动模块：`src`、`python`、`CMake`、依赖链。
3. 选择最小人工回归路径：启动、关键功能、错误路径、日志检查。
4. 记录证据：执行环境、步骤、结果、未覆盖项。
5. 示例命令：`ctest --test-dir <build_dir> -R "<module_regex>" --output-on-failure`

### 3.5 目录化说明原则

- `AGENTS.md` 仅保留规则与目录级入口，不再罗列外部文件明细。
- 编译、打包、插件、Python 错误分析等细节，统一下沉到对应目录的 README/指南维护。
- 若需要文件级细节，优先进入对应目录文档再定位具体文件。

## 4. 目录级入口

- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationGuide-Win10-VS2022\`
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPackagingGuide-Win10\`
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISPythonPlugins\`
- `__QGIS_MATERIALS_WIN_AMD64_PREFIX__\QGISCompilationPythonErrorsAnalysis\`
- `__OSGEO4W_ROOT__`

> [!NOTE]
> 1. 上述目录为入口导航；文件级说明与作用以各目录 README/指南为准。
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
