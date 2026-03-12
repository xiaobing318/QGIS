# QCopilots 说明

目标是为了在 QGIS 内嵌加载 llama-server WebUI，并通过 WebUI 配置 MCP Server 服务。

## 1. 适用范围

- Operating System:Windows/Ubuntu/Debian/Kylin/UOS
- ISA:AMD64/ARM64
- QGIS Version:3.44.7
- Qt Version:Qt6

## 2. 当前定位

qcopilots 是 QGIS 内的 `QWebEngineView` 容器，用于打开 llama-server WebUI 页面：

- 默认 URL：`http://127.0.0.1:8080`
- 可在插件菜单中修改 URL
- 仅支持 WebUI URL（`http` / `https`）

## 3. 配置来源与优先级

qcopilots 仅使用 QGIS Settings 与默认值，不使用 JSON 配置文件：

- 优先级：`QGIS Settings > 默认值`
- 配置键：`QCopilots/QCopilotServerUrl`
- 回滚键：`QCopilots/QCopilotServerLastKnownGoodUrl`

加载与持久化策略：

1. 用户输入新 URL 后先尝试连接，不立即写入配置。
2. 仅在页面加载成功后才写入 `QCopilots/QCopilotServerUrl`。
3. 若加载失败且存在 `LastKnownGood`，自动回滚到最后一次成功 URL。

## 4. 与 MCP 的关系

`qcopilots` 本身不实现 MCP 协议客户端逻辑，MCP 服务的配置、连接和工具调用由 llama-server WebUI 处理。

对于 shapefile 自动化处理，推荐直接在 llama-server WebUI 中挂载 `processingmcpserver` 的 shapefile 稳定模板能力：

- resources
  - `qgis://workflow/shapefile/template`
  - `qgis://workflow/shapefile/quality-profile/default`
  - `qgis://workflow/shapefile/run-summary`
- prompts
  - `qgis_shapefile_pipeline_planner`
  - `qgis_shapefile_quality_gate`
  - `qgis_shapefile_export_review`
- tools
  - `dataset_inspect_shapefile_bundle`
  - `vector_check_validity_report`
  - `vector_prepare_work_layer`
  - `vector_export_shapefile`
  - `project_cleanup_work_layers`

## 5. 下载行为（MCP Resources）

QCopilots 通过 Qt WebEngine 下载链路处理 llama-server WebUI 触发的资源下载（例如 `qgis://workflow/shapefile/run-summary` 的 Download content 按钮）：

- 下载触发后每次弹出“另存为”对话框（不自动静默保存）。
- 用户取消保存时，下载请求会被取消并写入 `QCopilots` 日志。
- 用户确认保存后，插件调用 WebEngine 下载接受流程并写入下载状态日志（started/completed/cancelled/interrupted）。
- 最近一次成功保存目录会记忆到 `QCopilots/QCopilotDownloadLastDir`，下次作为默认目录。

## 6. 联调入口（本机）

典型场景：QGIS 在本机，llama-server 在本机。

- 在 QGIS 中启动 `processingmcpserver`（默认 `http://127.0.0.1:8000/mcp`）。
- 启动本机 `llama-server` 并打开 WebUI（如 `http://127.0.0.1:8080`）。
- 在 `qcopilots` 中打开该 WebUI。
- 在 llama-server WebUI 的 MCP 设置中添加服务：
  - URL：`http://127.0.0.1:8000/mcp`
  - `useProxy`：`false`

## 7. 联调入口（远端）

典型场景：QGIS 在本机，llama-server 在远端服务器。

- 在远端启动 llama-server WebUI（示例：`http://<remote-host>:8080`）。
- 在 `qcopilots` 中输入远端 WebUI 地址（仅 `http/https`）。
- 在 WebUI 的开发者工具执行 `window.location.origin`，确认真实源（如 `http://localhost:8282` 或 `https://<remote-webui-host>`）。
- 在 WebUI 内配置 MCP URL（可指向本机或远端 `processingmcpserver`）。
- 场景二主路径优先 `useProxy=false`，并在 `processingmcpserver` 放行第 3 步确认到的真实源。
- 若网络拓扑限制必须走服务器代连，再评估 `useProxy=true`。

注意：

- `useProxy=true` 依赖 llama-server 的 CORS 代理能力（`--webui-mcp-proxy`）。
- `useProxy=false` 时，`127.0.0.1` 由浏览器所在机器解释；`useProxy=true` 时，`127.0.0.1` 由远端 llama-server 进程解释。
- 该能力在 llama.cpp 侧标记为实验特性，避免暴露到不可信网络。
- `proxying GET ... google.com/s2/favicons?...` 日志通常来自 favicon 请求，不代表 MCP initialize 一定失败。

## 8. Shapefile 模板使用方式

本地或远端 WebUI 打开后，建议在 llama-server WebUI 中按下列顺序使用：

1. 读取 `qgis://workflow/shapefile/template` 与 `qgis://workflow/shapefile/quality-profile/default`。
2. 让模型调用 `qgis_shapefile_pipeline_planner` 生成本次任务步骤。
3. 先执行 `dataset_inspect_shapefile_bundle` 和 `vector_check_validity_report` 做预检。
4. 通过 `vector_prepare_work_layer` 生成带任务标签的临时工作层，再调用通用 Processing 工具完成业务处理。
5. 在最终写盘前调用 `qgis_shapefile_export_review` 和 `qgis_shapefile_quality_gate`。
6. 只有在确认写盘后才调用 `vector_export_shapefile`，并在完成后调用 `project_cleanup_work_layers`。

## 9. 诊断日志位置

所有诊断信息仅写入 QGIS 的 Log Messages 面板，分类页签为 `QCopilots`，日志包含：

- Configured URL：当前配置尝试地址
- Final URL：实际最终访问地址（可能重定向）
- HTTP：连通性探测状态
- TLS/Proxy：证书与代理相关错误
- Hints：排障提示

## 10. 常见错误与处理

- 输入了 `ws://` 或其他非 `http/https` 协议
  - 表现：URL 被拒绝，消息栏提示协议不支持
  - 处理：改为 WebUI 地址（通常 `http://...` 或 `https://...`）

- 页面加载失败后自动回滚
  - 表现：日志出现 `Rolling back to last known good URL`
  - 处理：检查新地址是否可达、端口是否开放、证书是否可信

- HTTP 401/403
  - 表现：HTTP status 显示 401/403，并给出鉴权提示
  - 处理：确认目标服务认证配置或反向代理访问策略

- 证书或代理问题
  - 表现：Hints 出现 TLS/Proxy 相关提示
  - 处理：检查系统代理、证书链、域名与证书匹配关系

- MCP 资源点击下载无弹窗
  - 表现：WebUI 的 Download content 按钮点击后无保存对话框
  - 处理：确认当前运行的是最新编译并已部署的 `plugin_qcopilots` 构建产物，而不是旧插件二进制
