# QCopilots MCP Server Web Search

此插件将独立运行的 QCopilots-WebSearch 原生服务接入 QGIS 的 MCP Servers Manager。提供 `web_search`、`web_open`、`web_find`、`web_search_status` 四个工具，默认本地 MCP 端口为 48218。

插件不会启动或停止原生后端。原生服务离线时，插件的 MCP 初始化、工具发现和本地健康检查仍可工作。工具调用返回明确的后端错误。

将后端配置写入 QCopilots 用户目录中的 `web_search/config.json`，或通过绝对路径环境变量 `QCOPILOTS_WEBSEARCH_CONFIG` 指定。未提供用户配置时读取插件内的示例配置，默认连接 `http://127.0.0.1:8088`。

后端令牌来自 `QCOPILOTS_WEBSEARCH_BACKEND_TOKEN`，也可在配置中选择受保护的 `token_file`。后端令牌必须独立于本地 MCP 令牌。配置不保存明文令牌，日志不记录令牌、查询或网页正文。

远程后端必须使用 HTTPS，可指定 `ca_file`。现有 `formal_restricted` 网络策略仍生效，必须显式允许相应本机地址。适配器不使用代理或跟随重定向，配置的后端地址不受工具参数影响。

每个请求默认 25 秒截止时间，最多五个业务请求并发，状态查询使用独立容量。超时调用会关闭已建立的连接。操作系统 DNS 查询若尚未结束，其工作线程继续占用原有容量，防止不断生成后台线程。修改最大并发数后需重启适配服务。

在 Manager 中手动启用此插件。它不会改变用户已有的默认启动列表。后端恢复后，后续工具调用可直接恢复使用，无需重新注册工具。

接口模式 `web_*.schema.json` 和工具元数据 `tools.json` 与原生仓库 `contracts/v1` 中的同名文件保持一致。模式顶层不使用 `oneOf`，因此 `web_open` 的 `url` 与 `page_id` 二选一由适配器在调用后端前检查。网页内容是不可信外部资料。此适配代码按 GPL-2.0-or-later 分发。
