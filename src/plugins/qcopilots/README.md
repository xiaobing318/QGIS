# QCopilots 说明

## 1. 适用范围

- 平台：Windows 10/11 + AMD64/ARM64 + Qt6（`WITH_QTWEBENGINE=ON`）
- QGIS：3.44.7 及同类构建
- 目标：在 QGIS 内嵌加载 llama-server WebUI，并通过 WebUI 配置 MCP 服务

## 2. 当前定位

`qcopilots` 是 QGIS 内的 `QWebEngineView` 容器，用于打开 llama-server WebUI。

- 默认 URL：`http://127.0.0.1:8080`
- 可在插件菜单中修改 URL
- 仅支持 WebUI URL（`http` / `https`）

## 3. 配置来源与优先级

`qcopilots` 仅使用 QGIS Settings（`QGIS3.ini`）与默认值，不使用 JSON 配置文件。

- 优先级：`QGIS Settings(QGIS3.ini) > 默认值`
- 配置键：`QCopilots/QCopilotServerUrl`
- 回滚键：`QCopilots/QCopilotServerLastKnownGoodUrl`

加载与持久化策略：

1. 用户输入新 URL 后先尝试连接，不立即写入配置。
2. 仅在页面加载成功后才写入 `QCopilots/QCopilotServerUrl`。
3. 若加载失败且存在 `LastKnownGood`，自动回滚到最后一次成功 URL。

## 4. 与 MCP 的关系

`qcopilots` 本身不实现 MCP 协议客户端逻辑。
MCP 服务的配置、连接和工具调用由 llama-server WebUI 处理。

## 5. 联调入口（本机）

1. 在 QGIS 中启动 `processingmcpserver`（默认 `http://127.0.0.1:8000/mcp`）。
2. 启动本机 `llama-server` 并打开 WebUI（如 `http://127.0.0.1:8080`）。
3. 在 `qcopilots` 中打开该 WebUI。
4. 在 llama-server WebUI 的 MCP 设置中添加服务：
   - URL：`http://127.0.0.1:8000/mcp`
   - `useProxy`：`false`

## 6. 联调入口（远端）

典型场景：QGIS 在本机，llama-server 在远端服务器。

1. 在远端启动 llama-server WebUI（示例：`http://<remote-host>:8080`）。
2. 在 `qcopilots` 中输入远端 WebUI 地址（仅 `http/https`）。
3. 在 WebUI 内配置 MCP URL（可指向本机或远端 `processingmcpserver`）。
4. 若跨域受限，可在 WebUI 的 MCP 服务项中评估启用 `useProxy`。

注意：

- `useProxy=true` 依赖 llama-server 的 CORS 代理能力（`--webui-mcp-proxy`）。
- 该能力在 llama.cpp 侧标记为实验特性，避免暴露到不可信网络。

## 7. 诊断日志位置

`qcopilots` 不再在 Dock 底部显示诊断面板。
所有诊断信息仅写入 QGIS 的 `Log Messages` 面板，分类页签为 `QCopilots`。

日志包含：

- Configured URL：当前配置尝试地址
- Final URL：实际最终访问地址（可能重定向）
- HTTP：连通性探测状态
- TLS/Proxy：证书与代理相关错误
- Hints：排障提示

## 8. 常见错误与处理

1. 输入了 `ws://` 或其他非 `http/https` 协议
   - 表现：URL 被拒绝，消息栏提示协议不支持
   - 处理：改为 WebUI 地址（通常 `http://...` 或 `https://...`）

2. 页面加载失败后自动回滚
   - 表现：日志出现 `Rolling back to last known good URL`
   - 处理：检查新地址是否可达、端口是否开放、证书是否可信

3. HTTP 401/403
   - 表现：HTTP status 显示 401/403，并给出鉴权提示
   - 处理：确认目标服务认证配置或反向代理访问策略

4. 证书或代理问题
   - 表现：Hints 出现 TLS/Proxy 相关提示
   - 处理：检查系统代理、证书链、域名与证书匹配关系
