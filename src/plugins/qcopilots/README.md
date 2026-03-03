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

## 5. 联调入口（本机）

典型场景：QGIS 在本机，llama-server 在本机。

- 在 QGIS 中启动 `processingmcpserver`（默认 `http://127.0.0.1:8000/mcp`）。
- 启动本机 `llama-server` 并打开 WebUI（如 `http://127.0.0.1:8080`）。
- 在 `qcopilots` 中打开该 WebUI。
- 在 llama-server WebUI 的 MCP 设置中添加服务：
  - URL：`http://127.0.0.1:8000/mcp`
  - `useProxy`：`false`

## 6. 联调入口（远端）

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

## 7. 诊断日志位置

所有诊断信息仅写入 QGIS 的 Log Messages 面板，分类页签为 `QCopilots`，日志包含：

- Configured URL：当前配置尝试地址
- Final URL：实际最终访问地址（可能重定向）
- HTTP：连通性探测状态
- TLS/Proxy：证书与代理相关错误
- Hints：排障提示

## 8. 常见错误与处理

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
