# QCopilots 外部浏览器手动连接

QGIS 内嵌的 QCopilots 页面会自动注册 MCP 服务，而 Chrome 等外部浏览器使用独立的网页配置，需要手动添加服务，外部浏览器访问默认关闭，可以通过环境变量设置开关来进行控制。

## 启用与关闭

在用于启动 QGIS 的 PowerShell 中设置环境变量，再执行原有 Launcher 命令：

```powershell
# 设置环境变量
$env:QCOPILOTS_EXTERNAL_BROWSER_ACCESS = "ON"
# 进入到指定目录中
cd C:\Dev\github-repos\QGIS-Materials\QGISCompilationNavigation\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022
# 启动 QGIS
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Launcher.ps1 `
  --Profile WPGWorkstation-Windows-x64-feature-final-4_2_0-qcopilots-Qt6-RelWithDebInfo
```

管理器初始化时只读取一次该变量，去掉首尾空白后，只有不区分大小写的 `ON` 表示开启，未设置、空值、`OFF` 和其他值均表示关闭，修改后需要正常退出并重新启动 QGIS 软件，不会改变已经运行的实例，关闭时先正常退出 QGIS，再设置：

```powershell
# 设置环境变量
$env:QCOPILOTS_EXTERNAL_BROWSER_ACCESS = "OFF"
# 进入到指定目录中
cd C:\Dev\github-repos\QGIS-Materials\QGISCompilationNavigation\QGIS-Windows-AMD64\QGISCompilationGuide-Win10-VS2022
# 启动 QGIS
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Launcher.ps1 `
  --Profile WPGWorkstation-Windows-x64-feature-final-4_2_0-qcopilots-Qt6-RelWithDebInfo
```

## 允许访问的网页

开启后只允许以下精确来源，重复项会合并：

- Configure QCopilots URL 的 Origin 链接，例如配置 `https://example.com/chat` 时允许 `https://example.com`，路径不属于 Origin 链接。
- `http://127.0.0.1:8282`。
- `http://localhost:8282`。

其他协议、主机或端口不自动获准。修改 Configure QCopilots URL 后，重新启动 QGIS 以更新外部浏览器允许来源。MCP 服务始终只监听本机 `127.0.0.1` 地址。

## 连接失败排查

| 表现                           | 检查内容                                                   |
| ------------------------------ | ---------------------------------------------------------- |
| 管理器提示环境开关关闭         | 在启动 QGIS 的 PowerShell 设置 ON，并重新启动 QGIS         |
| 管理器提示配置禁用             | 检查现有用户配置中的 `browser_access.enabled`              |
| 管理器提示 Token 保存失败      | 修复用户配置目录写入问题，重新启动后再复制 Token           |
| 管理器提示 URL 无效            | 修正 Configure QCopilots URL 并重新启动                    |
| 网页来源被拒绝或出现 CORS 错误 | 对照管理器的完整允许来源列表，核对协议、主机和端口         |
| 返回 401                       | 检查 `Authorization: Bearer <Token>`，Token 更新后重新配置 |
| 连接被拒绝或超时               | 确认 QGIS 和目标服务正常运行，使用当前实际 Endpoint URL    |
| Chrome 拒绝本地网络访问        | 检查浏览器对当前网站的本地网络访问权限                     |
| 使用代理时仍不可用             | 关闭 `Use llama-server proxy`，本机服务应由浏览器直连      |

OFF 模式下 Token 复制按钮不可用，这是预期行为。不要通过放宽监听地址、使用通配 CORS 或关闭认证来排查连接问题。
