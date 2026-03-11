# Processing MCP Server

## 1. 适用范围

- 平台：Windows 10/11 + AMD64
- QGIS：3.44.7
- 工具链：Qt6 + VS2022（v143）

## 2. 模块职责矩阵

下表用于说明当前插件各模块的稳定职责边界：

| 模块 | 实际职责 | 命名判断 | 本轮处理 |
|---|---|---|---|
| `log_handler.py` | 统一插件日志入口与日志级别桥接 | 一致 | 保持文件名不变 |
| `config.py` / `config_models.py` / `config_loader.py` | 对外配置入口、配置模型、JSON/Settings/Default 加载与归一化 | 一致 | 保持文件名不变 |
| `server.py` | MCP Server 生命周期编排、启动/停止、tools/prompts/resources 注册 | 一致 | 保持文件名不变 |
| `transports.py` | `streamable-http` / `sse` / `stdio` 传输层封装与工厂选择 | 一致 | 保持文件名不变 |
| `mcp_main_thread_runner.py` | Qt 主线程同步执行器，供 server/tools 跨线程调用 | 一致 | 保持文件名不变 |
| `dependency_*.py` | 依赖探测、评估、契约校验、安装报告输出 | 一致 | 保持文件名不变 |
| `mcp_prompts.py` / `mcp_resources.py` | prompts/resources 注册与统一 envelope 生成 | 一致 | 保持文件名不变 |
| `plugin.py` | QGIS 插件入口、GUI 生命周期与依赖预检触发 | 一致 | 保持文件名不变 |
| `mcp_tools.py` | 工具注册与 QGIS/Processing/文件系统能力实现 | 一致 | 本轮补安全策略、返回契约和测试覆盖 |
| `tests/suite_runner.py` | 规范测试聚合入口 | 一致 | 保持 canonical 入口 |

## 3. 配置来源与优先级

当前插件使用三类配置来源，优先级顺序是 `JSON > QGIS Settings > Default`：

1. JSON 配置文件（最高优先级）。
2. QGIS Settings（次优先级）。
3. 内置默认值（最低优先级）。

JSON 配置文件路径为 `<QGIS Profile>/processingmcpserver/config.json`，其中 `<QGIS Profile>` 对应 `QgsApplication.qgisSettingsDirPath()`。若文件不存在，插件启动时会自动生成默认文件。

## 4. JSON 结构（v1）

```json
{
  "version": 1,
  "processing_mcp": {
    "enabled": true,
    "transport": "streamable-http",
    "host": "127.0.0.1",
    "port": 8000,
    "mount_path": "/",
    "sse_path": "/sse",
    "message_path": "/messages/",
    "streamable_http_path": "/mcp",
    "stateless_http": true,
    "json_response": true,
    "log_level": "INFO",
    "cors_origins": ["http://127.0.0.1:8080", "http://localhost:8080", "http://127.0.0.1:8282", "http://localhost:8282"],
    "cors_allow_headers": ["mcp-session-id", "mcp-protocol-version", "last-event-id", "authorization"],
    "enable_execute_code": false,
    "dependencies": {
      "auto_install": true
    },
    "filesystem": {
      "allowed_roots": ["<QGIS_PROFILE>/processingmcpserver", "<SYSTEM_TEMP>"],
      "readonly_roots": [],
      "disable_filesystem_tools": false
    }
  }
}
```

说明：

- `enable_execute_code` 目前仅作为兼容字段保留，当前版本不会注册任意代码执行类工具。
- 默认配置会把 `filesystem.allowed_roots` 初始化为当前 `<QGIS Profile>/processingmcpserver` 与系统临时目录；`allowed_roots` 外路径会被拒绝。
- 远端 WebUI 场景建议把 `readonly_roots` 设为 `allowed_roots` 的同一组路径，这样保留只读探查能力，同时默认关闭 `filesystem_edit_*` 写入面。

## 5. 工具阈值策略

工具阈值统一在 `mcp_tools.py` 内部定义默认值和保护逻辑，该文件中不再固化具体上限数值，避免文档与代码常量漂移；请以 `ProcessingMCPTools` 中对应常量与归一化函数为准。行为说明：`vector_get_layer_features`、`dataset_list_files`、`dataset_load_from_directory`、`processing_get_algorithms` 超限会截断并返回裁剪标记字段。用户可在 llama-server WebUI 中通过自然语言触发带参调用（例如 `limit=42`）覆盖默认值。

## 6. MCP 能力面

当前插件对外提供三类 MCP 能力：

- `tools`：QGIS 图层/目录数据/Processing 工具调用。
- `prompts`：任务规划与图层健康检查模板（2 个）。
- `resources`：工程信息与图层摘要资源（2 个）。
- 注册清单常量：
  - `REGISTERED_TOOL_NAMES`（定义于 `mcp_tools.py`）
  - `REGISTERED_PROMPT_NAMES`（定义于 `mcp_prompts.py`）
  - `REGISTERED_RESOURCE_URIS`（定义于 `mcp_resources.py`）
- docstring 常量：
  - `_REGISTERED_TOOL_DOCSTRINGS`（定义于 `mcp_tools.py`）
  - `_REGISTERED_PROMPT_DOCSTRINGS`（定义于 `mcp_prompts.py`）
  - `_REGISTERED_RESOURCE_DOCSTRINGS`（定义于 `mcp_resources.py`）

完整能力清单不再手写在本 README 中，而是通过脚本导出到同目录的 `MCP_CAPABILITIES.generated.md`。

### 6.0 在 QGIS Python Console 生成能力文档

先确认当前 QGIS 运行的是哪一份插件代码：

```python
import processingmcpserver
print(processingmcpserver.__file__)
```

使用默认输出路径导出能力文档：

```python
import processingmcpserver

output_path = processingmcpserver.write_mcp_capabilities_markdown()
print(output_path)
```

使用显式路径导出能力文档：

```python
from pathlib import Path
import processingmcpserver

output_path = processingmcpserver.write_mcp_capabilities_markdown(
    Path(r"I:\github_repos\QGIS\python\plugins\processingmcpserver\MCP_CAPABILITIES.generated.md")
)
print(output_path)
```

校验点：

- `processingmcpserver.__file__` 必须指向当前实际运行的插件目录。
- 不传参数时，输出文件默认写到导出模块同目录，即 `MCP_CAPABILITIES.generated.md`。
- 生成文件必须包含 `tools/prompts/resources` 三类能力数量和对应原始 docstring。
- 若当前运行的是编译输出目录中的插件副本，则默认输出也会落到编译输出目录，而不是源码目录。

### 6.1 在 llama-server WebUI 中使用 prompts/resources

1. 打开 `MCP Servers`，确认目标服务连接状态为可用。
2. 在输入框附件菜单点击 `MCP Prompt`，选择一个 prompt 并填写参数（可空）。
3. 提交后，prompt 结果会注入会话上下文，模型会按模板结构生成执行步骤。
4. 在附件菜单点击 `MCP Resources`，挂载资源（如 `qgis://project/layers/summary`）作为上下文。
5. 继续对话时要求模型“按上面 prompt + resources 调用 tools 执行”。

补充说明：

- llama-server WebUI 中看到的 tool descriptions 来自 `mcp_tools.py` 内置 tool docstring。
- llama-server WebUI 中看到的 prompt/resource descriptions 来自 `mcp_prompts.py` / `mcp_resources.py` 的 registry docstring。
- WebUI 中看到的 `Server instructions` 来自 `server.py` 内置说明串。
- `Server instructions` 会明确提示三件事：先调用 `common_get_qgis_info` 建上下文、浏览器直连 MCP 时使用 `useProxy=false`、以及 `filesystem_*` 受 `allowed_roots/readonly_roots` 约束。

### 6.2 常见误区

- 把 `resources` 当大数据导出接口：
  `resources` 只提供摘要/索引，大体量明细请走 `tools`（例如 `vector_get_layer_features`）。
- prompt 不写目标与约束：
  会导致步骤泛化，建议至少补充目标、输入图层、输出格式。
- 服务 capability 未启用：
  WebUI 不会显示 `MCP Prompt`/`MCP Resources` 入口。

### 6.3 安全写入策略

- 所有写操作默认 `in_place=false`，先创建安全副本再写入。
- 返回体统一包含 `summary.mode`：
  - `copy`：写入副本，返回 `output_layer_id` 指向新图层，源图层不变。
  - `in_place`：直接写入原图层。
- 默认副本模式下，写工具与 `raster_stats_zonal` 都保证 `output_layer_id != source_layer_id`，便于 WebUI 明确区分输出层与源图层。
- 危险删除操作必须显式确认：
  - `vector_table_delete_records(confirm_destructive=true)`
  - `vector_table_truncate(confirm_destructive=true)`
- 新增记录/更新记录支持 QGIS API 兜底，弥补 Processing 在“插入记录”上的短板。
- `processing_execute_algorithm` / `processing_execute_on_layers` 默认也启用安全策略：
  - `allow_disk_write=false`（默认）：若参数包含磁盘输出路径，自动改写为 `TEMPORARY_OUTPUT`。
  - `allow_in_place_edit=false`（默认）：自动阻止 `IN_PLACE=true` 等原地编辑参数。
  - 返回体会额外给出 `warnings`、`safety_policy`、`effective_parameters`，用于确认最终生效参数。
- `filesystem_*` 默认也启用路径沙箱：
  - `allowed_roots` 外路径一律拒绝。
  - `readonly_roots` 内仅允许查询，不允许 `filesystem_edit_*` 写操作。
  - `disable_filesystem_tools=true` 时，所有 `filesystem_*` 工具都会直接拒绝执行。
- `filesystem_edit_*` 中涉及删除/覆盖/覆盖式移动的操作都要求显式确认：
  - 删除必须 `confirm_destructive=true`
  - 覆盖复制/覆盖移动/覆盖写入必须 `overwrite=true` 且 `confirm_destructive=true`

具体的 tool/prompt/resource 原始 docstring、完整名称清单和导出后的 Markdown 正文，请以同目录的 `MCP_CAPABILITIES.generated.md` 为准。

## 7. 依赖预检与自动安装

- 启动时会读取插件目录 `requirements.txt` 作为唯一依赖来源。
- `requirements.txt` 使用区间版本约束（`>=,<`），便于开发者升级与回归。
- 当前建议区间：
  - `mcp>=1.13.0,<2.0.0`
  - `uvicorn>=0.31.1,<1.0.0`
  - `starlette>=0.27,<1.0.0`
- 若存在缺失且 `dependencies.auto_install=true`，会在当前运行环境中解析目标解释器并执行安装。
- 解释器解析顺序固定为：`sys.executable(仅当文件名为 python*)` -> `sys.prefix/python(.exe)` -> `sys.exec_prefix/python(.exe)` -> `sys.base_prefix/python(.exe)`。
- 安装命令固定为 `<resolved_python> -m pip install ...`，不会回退到 `--user` 目录。
- 若目标 Python 根环境不可写（如权限不足），安装会失败并给出明确错误，不会自动回退到用户目录。
- 依赖检查与安装报告写入 `<QGIS Profile>/processingmcpserver/dependency-report.json`。
- 依赖满足后会执行 FastMCP 运行时契约校验，要求：
  - `FastMCP.__init__` 支持当前插件使用的参数（`host/port/mount_path/sse_path/message_path/streamable_http_path/stateless_http/json_response/log_level`）。
  - `FastMCP` 提供可调用的 `streamable_http_app` 与 `sse_app`。
- 契约校验失败会写入 `failure_reason` 并阻止插件启动。

### 报告生成链路

- `plugin.initGui`（`plugin.py`）
- `ensure_processing_mcp_dependencies`（`dependency_manager.py`）
- `_write_dependency_report`（`dependency_reporting.py`）

依赖模块分工：

- `dependency_manager.py`：依赖检查主编排与对外入口。
- `dependency_models.py`：依赖检查相关 dataclass 模型。
- `dependency_probe.py`：环境采集、pip 探测、requirements 读取。
- `dependency_evaluator.py`：requirement 解析与满足性评估。
- `dependency_contract.py`：FastMCP 运行时契约校验。
- `dependency_reporting.py`：依赖报告输出与日志摘要。
- `mcp_main_thread_runner.py`：Qt 主线程同步执行器（供 `server.py`/`mcp_tools.py` 使用）。

说明：

- 依赖预检阶段只读取 `dependencies.auto_install`；其它配置字段不会参与依赖安装决策。
- `dependency-report.json` 是运行时代码生成，不是配置文件“创建”。

报告关键字段（schema v3）：

- `report_schema_version`
- `platform_system` / `platform_release` / `platform_machine`
- `python_executable` / `python_version`
- `python_prefix` / `python_base_prefix` / `python_exec_prefix`
- `is_virtual_environment`
- `site_packages` / `user_site_packages`
- `pip_available` / `pip_version` / `pip_error`
- `pip_python_executable` / `pip_python_source`
- `pip_probe_command` / `pip_probe_stdout` / `pip_probe_stderr`
- `requirements_file_path` / `requirements_file_read` / `requirements_file_error`
- `install_target_prefix` / `install_target_site_packages`
- `mcp_runtime_contract_ok` / `mcp_runtime_contract_error`
- `requested_requirements`
- `unsatisfied_before` / `unsatisfied_after`
- `unsatisfied_reasons_before` / `unsatisfied_reasons_after`
- `installed_versions_before` / `installed_versions_after`
- `install_command`
- `failure_reason`

### 7.1 日志页签与级别

- 所有插件日志写入 QGIS `Log Messages` 面板的 `Processing MCP Server` 页签。
- 默认 `log_level=INFO`，日常联调可直接使用。
- 需要更细粒度诊断时可改为 `DEBUG`，会看到更多 `uvicorn/mcp/starlette` 桥接日志。

## 8. QGIS Settings 回退键

当 JSON 未提供某个键时，会回退读取 `Processing/MCP/*`：

- `Processing/MCP/enabled`
- `Processing/MCP/transport`
- `Processing/MCP/host`
- `Processing/MCP/port`
- `Processing/MCP/mount_path`
- `Processing/MCP/sse_path`
- `Processing/MCP/message_path`
- `Processing/MCP/streamable_http_path`
- `Processing/MCP/stateless_http`
- `Processing/MCP/json_response`
- `Processing/MCP/log_level`
- `Processing/MCP/cors_origins`
- `Processing/MCP/cors_allow_headers`
- `Processing/MCP/enable_execute_code`
- `Processing/MCP/dependencies/auto_install`
- `Processing/MCP/filesystem/allowed_roots`
- `Processing/MCP/filesystem/readonly_roots`
- `Processing/MCP/filesystem/disable_filesystem_tools`

## 9. 架构图

### 9.1 组件架构图

```mermaid
flowchart LR
    A[QGIS Desktop] --> B[qcopilots Dock\nQWebEngineView]
    B --> C[llama-server WebUI]
    C --> D[processingmcpserver\nstreamable-http/sse]
    D --> E[QGIS Processing / Project / Layers]
```

### 9.2 场景时序图（场景一 + 场景二）

```mermaid
sequenceDiagram
    participant Q as QGIS(qcopilots)
    participant W as llama-server WebUI
    participant M as processingmcpserver

    rect rgb(236, 248, 255)
    note over Q,W: 场景一：本地 WebUI
    Q->>W: 打开 http://127.0.0.1:8080
    W->>M: 连接 http://127.0.0.1:8000/mcp
    M-->>W: 返回 tools/prompts/resources
    end

    rect rgb(245, 255, 238)
    note over Q,W: 场景二：远端 WebUI 直连本地 MCP
    Q->>W: 打开 https://<remote-webui-host>
    W->>M: 跨域连接 http://<xian-host>:8000/mcp
    M-->>W: 通过 CORS 校验后返回能力清单
    end
```

## 10. 场景一：本地 WebUI 联调步骤

1. 本地启动 `llama-server` 并启用 WebUI（默认 `http://127.0.0.1:8080`）。
2. 打开 QGIS，确认 `processingmcpserver` 已加载并监听 `http://127.0.0.1:8000/mcp`。
3. 在 `qcopilots` Dock 打开 `http://127.0.0.1:8080`。
4. 在 WebUI 的 MCP 设置中新增服务：
   - URL：`http://127.0.0.1:8000/mcp`
   - `useProxy=false`
5. 按以下清单做严格验收：
   - `MCP Server` 卡片中必须能看到中文 tool descriptions，且 `Server instructions` 可展开查看。
   - `listPrompts` 返回数量必须为 `2`，且仅包含 `qgis_task_planner`、`qgis_layer_health_check`。
   - `listResources` 返回数量必须为 `2`，且仅包含 `qgis://project/info`、`qgis://project/layers/summary`。
   - `listTools` 必须包含 `processing_execute_algorithm`、`layer_get_details`、`vector_get_layer_features`、`vector_table_query_records`、`vector_table_update_records`、`vector_stats_basic`、`raster_stats_basic`，且不包含已移除旧名（如 `execute_processing`、`render_map`）。
   - 最少完成 1 次工具调用并成功（建议先 `common_get_qgis_info`，再 `layer_list` 复核）。

## 11. 场景二：远端 WebUI 直连本地 MCP

0. 先确认 WebUI 的真实源（不是猜测）：
   - 在 MCP 页开发者工具执行 `window.location.origin`。
   - 常见输出：`http://localhost:8282`（SSH 端口转发）或 `https://<remote-webui-host>`（直连远端域名）。
1. 在北京服务器启动 llama-server WebUI（例如 `https://<remote-webui-host>`）。
2. 在西安本机启动 QGIS 与 `processingmcpserver`。
3. 在本机配置 `processing_mcp.cors_origins`，放行上一步确认到的真实源（精确到协议+域名+端口）。
4. 在 WebUI 里配置 MCP Server：
   - URL：`http://127.0.0.1:8000/mcp`（SSH 隧道场景常用）或 `http://<xian-host>:8000/mcp`
   - `Use llama-server proxy`：关闭（`useProxy=false`）
5. 在西安本机执行 CORS 预检（示例为 SSH 隧道来源）：

```bash
curl -i -X OPTIONS http://127.0.0.1:8000/mcp \
  -H "Origin: http://localhost:8282" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: mcp-session-id,mcp-protocol-version"
```

期望结果：

- 状态码为 `2xx`。
- 响应头包含 `Access-Control-Allow-Origin: http://localhost:8282`。
- 响应头包含 `Access-Control-Allow-Headers` 且覆盖 `mcp-session-id,mcp-protocol-version`。

6. 在 WebUI MCP 面板点击重连，按以下清单做严格验收：
   - 连接日志从 `Sending initialize request...` 进入成功阶段（如 capability 交换/connected），不再出现 `Failed to fetch`。
   - `MCP Server` 卡片中必须能看到中文 tool descriptions，且 `Server instructions` 可展开查看。
   - `listPrompts` 返回数量必须为 `2`，且仅包含 `qgis_task_planner`、`qgis_layer_health_check`。
   - `listResources` 返回数量必须为 `2`，且仅包含 `qgis://project/info`、`qgis://project/layers/summary`。
   - `listTools` 必须包含 `processing_execute_algorithm`、`layer_get_details`、`vector_get_layer_features`、`vector_table_query_records`、`vector_table_update_records`、`vector_stats_basic`、`raster_stats_basic`，且不包含已移除旧名（如 `execute_processing`、`render_map`）。
   - 最少完成 1 次工具调用（建议 `common_get_qgis_info`）并在 MCP 面板看到成功返回。

### 11.1 场景二配置模板

- SSH 端口转发场景（`window.location.origin` 为 `http://localhost:8282`）：
  `examples/config.scene2.ssh-tunnel.v1.json`
- 远端 HTTPS 直连场景（`window.location.origin` 为 `https://<remote-webui-host>`）：
  `examples/config.scene2.remote-https.v1.json`

注意：

- 该路径是“远端 WebUI 直连本地 MCP”，主路径不依赖 `--webui-mcp-proxy`。
- `useProxy=false` 时，`127.0.0.1` 由浏览器所在机器解释。若你用 `http://localhost:8282`（SSH 转发）访问 WebUI，`127.0.0.1:8000` 指向西安本机。
- `useProxy=true` 时，请求改由 llama-server 进程发起，`127.0.0.1` 会变成远端 Ubuntu 本机回环地址，通常不符合本场景目标。
- 若网络拓扑不可达，需要先打通路由/NAT/防火墙。

## 12. 故障排查

- `config.json` 语法错误：插件会记录 warning，并回退到 Settings/默认值。
- 配置值非法（如端口越界）：该键降级到低优先级来源并记录 warning。
- 场景二跨域失败：优先检查 `cors_origins` 是否包含远端源。
- 地址不可达：检查防火墙、端口暴露、NAT 映射是否正确。
- 错误使用 `useProxy`：远端直连本地 MCP 场景应使用 `useProxy=false`。
- MCP 卡片出现 `Failed to fetch` 且日志停在 `Sending initialize request...`：优先排查 CORS（看 `window.location.origin` 与 `cors_origins` 是否严格匹配）。
- llama-server 日志出现 `proxying GET ... google.com/s2/favicons?...`：这是 favicon 拉取请求，不等价于 MCP initialize 失败根因。
- 依赖自动安装失败：查看 `dependency-report.json` 的 `pip_error`、`install_return_code`、`failure_reason`、`install_stdout`、`install_stderr`。

### 12.1 MetaSearch / jinja2 全局修复（独立于 processingmcpserver）

说明：

- `MetaSearch` 对 `jinja2` 的依赖属于 QGIS 插件全局运行时问题，不在 `processingmcpserver` 自动依赖管理范围内。
- 请使用对应安装目录下的 `python.exe` 执行安装，确保安装到目标 Python 根环境。

示例 A（`E:\osgeo4w-setup\OSGeo4W34407`）：

```powershell
E:\osgeo4w-setup\OSGeo4W34407\apps\Python312\python.exe -m pip install --disable-pip-version-check --no-input "jinja2>=3.1,<4.0"
E:\osgeo4w-setup\OSGeo4W34407\apps\Python312\python.exe -c "import jinja2,markupsafe;print(jinja2.__version__);print(markupsafe.__version__)"
```

示例 B（`I:\data\QGISInstall\qgis34407`）：

```powershell
I:\data\QGISInstall\qgis34407\apps\Python312\python.exe -m pip install --disable-pip-version-check --no-input "jinja2>=3.1,<4.0"
I:\data\QGISInstall\qgis34407\apps\Python312\python.exe -c "import jinja2,markupsafe;print(jinja2.__version__);print(markupsafe.__version__)"
```

回滚（按需）：

```powershell
<QGIS_PYTHON_EXE> -m pip uninstall -y jinja2 markupsafe
```

## 13. 在 QGIS Python Console 运行测试

可直接在 QGIS Python Console 执行：

```python
from processingmcpserver.tests.suite_runner import run_from_qgis_console
run_from_qgis_console(verbosity=2)
```

说明：

- 该入口会先检测 `QgsApplication.instance()`，避免重复初始化冲突。
- 失败堆栈会直接回显到 QGIS Python Console，便于联调。
- 工具测试使用固定样本目录：`python/plugins/processingmcpserver/tests/data`。

### 13.1 通过命令行运行全量单测（PowerShell）

不启动 QGIS GUI、直接命令行运行测试时，可在 `PowerShell` 执行：

```powershell
$env:PYTHONHOME='<OSGEO4W_ROOT>\apps\Python312'; `
$env:QGIS_PREFIX_PATH='<QGIS_BUILD_DIR>\output\bin\<BUILD_CONFIG>'; `
$env:PATH='<QGIS_BUILD_DIR>\output\bin\<BUILD_CONFIG>;<OSGEO4W_ROOT>\apps\Qt6\bin;<OSGEO4W_ROOT>\bin;' + $env:PATH; `
$env:PYTHONPATH='<QGIS_SOURCE_DIR>\python\plugins;<QGIS_BUILD_DIR>\output\python;<QGIS_BUILD_DIR>\output\python\plugins;<QGIS_SOURCE_DIR>\tests\src\python'; `
<OSGEO4W_ROOT>\apps\Python312\python.exe -B -m processingmcpserver.tests.suite_runner
```

占位符说明：

- `<OSGEO4W_ROOT>`：OSGeo4W 根目录（例如 `C:\OSGeo4W`）。
- `<QGIS_SOURCE_DIR>`：QGIS 源码根目录（例如 `C:\OSGeo4W64\QGIS`）。
- `<QGIS_BUILD_DIR>`：QGIS 构建目录（例如 `I:\QGISCompilations\VisualStudio2022\feature-final-3_44_7-mcp\qgis-project`）。
- `<BUILD_CONFIG>`：构建配置（常见值：`RelWithDebInfo`、`Release`、`Debug`）。

说明：

- 上述命令是 PowerShell 语法（`$env:...`），不适用于 `cmd`。
- 该方式用于“命令行直接跑测试”；QGIS Python Console 方式仍适用于交互调试。
- 若环境中 Processing 算法提供者未完整可用，部分用例可能因 `Algorithm not found` 失败。

### 13.2 tests/data 固化测试数据

- 当前目录至少包含：
  - `dem.tif`（栅格测试样本）
  - `sample_vector.geojson`（矢量测试样本）
- 为保证 source/output 行为一致，`tests/CMakeLists.txt` 已安装 `tests/data/*` 到插件测试目录。

安装后可在 output 目录检查：

```bat
dir <QGIS_BUILD_DIR>\output\python\plugins\processingmcpserver\tests\data
```

校验点：

- 必须能看到 `dem.tif` 与 `sample_vector.geojson`。
- 若缺失，先执行构建/安装同步，再从 QGIS Python Console 重跑测试。

## 14. 语法校验命令详解（Qt6 主链路）

你给出的命令：

```bat
cmd /c "call C:\OSGeo4W64\bin\o4w_env.bat && call C:\OSGeo4W64\bin\py3_env.bat && python -m py_compile ..."
```

命令结构说明：

- `cmd /c`：在独立 CMD 会话执行整串命令并退出。
- `call ...\o4w_env.bat`：初始化 OSGeo4W 基础环境变量（PATH/GDAL/PROJ 等）。
- `call ...\py3_env.bat` 或 `call ...\python3.bat`：绑定 Python 解释器路径。
- `python -m py_compile`：仅做 Python 语法编译检查，不运行业务逻辑。

为什么该命令不适用于 Qt6 主链路：

- `C:\OSGeo4W64\bin\py3_env.bat` 绑定的是 `Python39 + Qt5` 套件。
- Qt6 主链路应使用 `C:\OSGeo4W` 套件，并显式切换到 Qt6 环境。
- 结论：`C:\OSGeo4W64\bin\py3_env.bat` 不作为 Qt6 主链路命令模板。

Qt6 推荐命令（Windows 10/11 + AMD64）：

```bat
cmd /c "call C:\OSGeo4W\bin\o4w_env.bat && call C:\OSGeo4W\etc\ini\python3.bat && call C:\OSGeo4W\bin\qt6_env.bat && python -m py_compile C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\mcp_tools.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\mcp_prompts.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\mcp_resources.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\dependency_manager.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\dependency_models.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\dependency_probe.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\dependency_evaluator.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\dependency_contract.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\dependency_reporting.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\mcp_main_thread_runner.py C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\tests\suite_runner.py"
```

或直接对 tests 目录做整体验证：

```bat
cmd /c "call C:\OSGeo4W\bin\o4w_env.bat && call C:\OSGeo4W\etc\ini\python3.bat && call C:\OSGeo4W\bin\qt6_env.bat && python -m compileall C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver\tests"
```

环境校验命令（建议紧接着执行）：

```bat
call C:\OSGeo4W\bin\o4w_env.bat
call C:\OSGeo4W\etc\ini\python3.bat
call C:\OSGeo4W\bin\qt6_env.bat
python -c "import os,sys;print(sys.executable);print(sys.version);print(os.environ.get('PYTHONHOME',''));print(os.environ.get('PATH','').split(';')[0])"
```

校验点：

- `sys.executable` 应来自目标环境的 `...\apps\Python312\python.exe`（例如 `E:\osgeo4w-setup\OSGeo4W34407\apps\Python312\python.exe`）。
- `sys.version` 应为 `3.12.x`。
- PATH 前部应包含 `Qt6\bin`。

副作用与清理：

- `py_compile` 或 `compileall` 会生成 `__pycache__`。
- 清理命令：

```bat
for /r C:\OSGeo4W64\QGIS\python\plugins\processingmcpserver %d in (__pycache__) do @if exist "%d" rd /s /q "%d"
```
