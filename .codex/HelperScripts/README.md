## 1. 目录用途

目录 HelperScripts 是 Codex 在进入分析、实现、测试或构建前使用的上下文发现入口，它只负责生成可审查的 UTF-8 JSON 证据，不负责构建、测试、安装、发布、打包、下载依赖。入口脚本可以按 ResolveCodexContext.json 配置文件中的 EnabledScripts 数组选择性执行发现步骤：解析当前平台信息，解析 __QGISCompilationNavigation__ 占位符绝对路径。入口脚本只支持 3 个参数，名称固定如下：

| 参数 | 作用 | 默认行为 |
| --- | --- | --- |
| --help | 输出帮助信息后退出。 | 不与其它参数组合使用。 |
| --config <path> | 指定 JSON 配置文件。 | 读取入口脚本同目录的 ResolveCodexContext.json。 |
| --outputJsonPath <path> | 指定 JSON 写出文件；若目标文件已存在则自动覆盖。 | 不写文件，只把 JSON 输出到终端。 |

默认成功输出只包含 `Status/PlatformInfo/Placeholders.__QGISCompilationNavigation__` 字段。若某个发现步骤被 EnabledScripts 禁用，则不输出对应字段。当资料根不存在、候选路径歧义或输入无效时，输出可能额外包含 Candidates 或 Messages，用于让用户审查和修正。

## 2. 注意事项

- 脚本 ResolveCodexContext.* 可能会因为扫描磁盘而耗时较长。在执行入口脚本时应在工具调用层显式设置执行超时时间，例如 `timeout_ms=120000` 到 `timeout_ms=300000` 之间，当配置文件中的 DefaultSearchMode 字段会扫描多个磁盘或文件系统时，优先使用 `timeout_ms=300000` 设置。


## 3. 配置文件

每个平台入口默认读取同目录的 `ResolveCodexContext.json`。顶层 `EnabledScripts` 必须是字符串数组，字段值使用当前平台目录中的真实脚本文件名并包含后缀。

Windows:

```json
{
  "EnabledScripts": [
    "ResolvePlatformInfo.ps1",
    "ResolveQGISCompilationNavigation.ps1"
  ]
}
```

Linux/macOS:

```json
{
  "EnabledScripts": [
    "ResolvePlatformInfo.sh",
    "ResolveQGISCompilationNavigation.sh"
  ]
}
```

## 4. 使用场景与完整命令

### 4.1 查看帮助

Windows:

```powershell
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 查看帮助信息
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Windows\ResolveCodexContext.ps1 --help
```

Linux:

```bash
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 查看帮助信息
bash ./Linux/ResolveCodexContext.sh --help
```

macOS:

```bash
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 查看帮助信息
bash ./macOS/ResolveCodexContext.sh --help
```

### 4.2 使用默认配置并输出到终端

适用于本轮尚未锁定平台信息或 `__QGISCompilationNavigation__`，需要先生成 JSON 结果供用户审查的场景。该模式不写文件。

Windows:

```powershell
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 使用默认配置并输出到终端
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Windows\ResolveCodexContext.ps1
```

Linux:

```bash
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 使用默认配置并输出到终端
bash ./Linux/ResolveCodexContext.sh
```

macOS:

```bash
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 使用默认配置并输出到终端
bash ./macOS/ResolveCodexContext.sh
```

### 4.3 指定配置文件并输出到终端

适用于需要切换配置文件、选择发现步骤或限制搜索根的场景。发现步骤、搜索根、默认搜索模式和剪枝目录等发现策略应写入对应 `ResolveCodexContext.json`，不要通过入口参数传递。

Windows:

```powershell
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 指定配置文件并输出到终端
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Windows\ResolveCodexContext.ps1 --config .\Windows\ResolveCodexContext.json
```

Linux:

```bash
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 指定配置文件并输出到终端
bash ./Linux/ResolveCodexContext.sh --config ./Linux/ResolveCodexContext.json
```

macOS:

```bash
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 指定配置文件并输出到终端
bash ./macOS/ResolveCodexContext.sh --config ./macOS/ResolveCodexContext.json
```

### 4.4 指定 JSON 写出文件

适用于需要保留本轮上下文发现证据的场景。执行前应说明写入范围；若目标 JSON 已存在，入口脚本会自动覆盖。验证完成后，临时输出文件应按任务要求清理。

Windows:

```powershell
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 指定 JSON 写出文件
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Windows\ResolveCodexContext.ps1 --config .\Windows\ResolveCodexContext.json --outputJsonPath .\Output\CodexContext.json
```

Linux:

```bash
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 指定 JSON 写出文件
bash ./Linux/ResolveCodexContext.sh --config ./Linux/ResolveCodexContext.json --outputJsonPath ./Output/CodexContext.json
```

macOS:

```bash
# 进入到指定目录中
cd Path2Repo/.codex/HelperScripts
# 指定 JSON 写出文件
bash ./macOS/ResolveCodexContext.sh --config ./macOS/ResolveCodexContext.json --outputJsonPath ./Output/CodexContext.json
```
