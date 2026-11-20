# QCopilots MCP Server Web Search

此插件将独立运行的 QCopilots-WebSearch 原生服务接入 QGIS 的 MCP Servers Manager。提供 `web_search`、`web_open`、`web_find`、`web_search_status` 四个工具，默认本地 MCP 端口为 48218。

插件不会启动或停止原生后端。原生服务离线时，插件的 MCP 初始化、工具发现和本地健康检查仍可工作。工具调用返回明确的后端错误。

插件默认通过 `https://web-search.vastarmorai.com` 调用原生服务。要切换后端，可在 QCopilots 用户目录的 `web_search/config.json` 中设置 `backend_url`。环境变量 `QCOPILOTS_WEBSEARCH_CONFIG` 可指定优先读取的配置文件绝对路径。已有配置继续生效。`backend_url` 只填写源地址，适配器会自行添加 `/v1/` 路径。

启动插件后，QGIS 内嵌页面的 Configure Server 会显示 Manager 管理的虚拟地址 `https://qcopilots.localmachine/mcp/qcopilots.mcp_server_web_search`。保留该地址及默认授权设置。适配器通过 `QCOPILOTS_WEBSEARCH_BACKEND_TOKEN` 或配置中的 `auth.token_file` 单独读取原生服务的业务令牌。后端令牌必须独立于本地 MCP 令牌。配置不保存明文令牌，日志不记录令牌、查询或网页正文。

远程后端必须使用 HTTPS，可指定 `ca_file`。默认的 `compatible` 模式支持公网后端。`formal_restricted` 模式只允许已批准的回环地址。适配器不使用代理或跟随重定向，配置的后端地址不受工具参数影响。

每个请求默认 25 秒截止时间，最多五个业务请求并发，状态查询使用独立容量。超时调用会关闭已建立的连接。操作系统 DNS 查询若尚未结束，其工作线程继续占用原有容量，防止不断生成后台线程。修改最大并发数后需重启适配服务。

在 Manager 中手动启用此插件。它不会改变用户已有的默认启动列表。后端恢复后，后续工具调用可直接恢复使用，无需重新注册工具。

接口模式 `web_*.schema.json` 和工具元数据 `tools.json` 与原生仓库 `contracts/v1` 中的同名文件保持一致。模式顶层不使用 `oneOf`，因此 `web_open` 的 `url` 与 `page_id` 二选一由适配器在调用后端前检查。网页内容是不可信外部资料。此适配代码按 GPL-2.0-or-later 分发。
