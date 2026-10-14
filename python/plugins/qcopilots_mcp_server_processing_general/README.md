# QCopilots General 文件传输

文件、目录和混合来源上传下载由 General 服务的 11 个文件传输工具提供，原来的 6 个 Processing 工具保持不变。目标客户端是 Windows 10/11 x64。文件传输命令统一使用 `file_transfer_` 分类前缀，例如 `file_transfer_start_upload` 和 `file_transfer_start_download`，实际通过 SSH/SFTP 传输。

文件、目录和混合源列表都可以上传或下载。任务在后台运行，不占用 QGIS 主线程。已有同名文件默认拒绝覆盖，只有明确指定 `overwrite: true` 才允许原子替换。服务器不支持原子替换时会拒绝覆盖，不会先删除原文件。

## 设置连接

使用 QCopilots 对话即可查询和修改设置，不需要额外设置窗口，例如：
- 查看当前文件传输设置和配置文件位置。
- 将文件上传下载服务器设置为 192.0.2.10，端口 22，账户 upload-user，并设置我提供的密码。默认上传目录是 C:/RemoteData/。

对应工具是 `file_transfer_get_config` 和 `file_transfer_update_config`。更新工具接收需要修改的字段，未提供的字段保持原值。密码按当前产品约定以明文保存在本机配置文件中，查询、错误和任务结果不会回显密码。省略 `password` 保留现有密码，显式传入空字符串清除密码。在对话中提供密码会将其作为该聊天请求的一部分提交。首次调用更新工具会建立用户配置。开发者也可以将插件目录中的 `file_transfer_config.json` 模板复制到以下位置并手动编辑：

```text
%APPDATA%/QGIS/QCopilots/processing_general/file_transfer_config.json
```

如果设置了 `QCOPILOTS_HOME`，则使用该目录下的 `processing_general/file_transfer_config.json`。安装目录中的文件只是模板，升级插件不会覆盖用户设置。工具每次读取最新 JSON，正在运行的任务保留启动时的配置快照。配置模板对应 `file_transfer_config.schema.json`。传输断点和历史保存在同一用户目录下的 `processing_general/file_transfers`，插件自己的主机信任文件是 `processing_general/file_transfer_known_hosts`。

首次从旧命名版本切换时，先用旧版本完成现有传输，或取消后显式放弃并确认清理完成，再停止 General 服务并清理其旧文件传输配置、信任记录和任务状态。新版本只识别 `file_transfer_` 命令和新命名文件，不兼容旧命令，也不读取或迁移旧数据。切换后重新配置连接、确认主机信任并刷新客户端工具列表。用户的 OpenSSH `known_hosts` 文件和已完成的数据文件无需清理，其中 JSON 示例中的地址、账户、路径和密码都需要替换：

```json
{
  "config_version": 1,
  "host": "192.0.2.10",
  "port": 22,
  "username": "upload-user",
  "password": "YOUR_PASSWORD",
  "upload_sources": ["C:/Data/result.tif", "C:/Data/vector-output"],
  "upload_destination": "C:/RemoteData/",
  "download_sources": ["C:/RemoteData/result.tif"],
  "download_destination": "C:/Data/downloads",
  "connect_timeout": 15,
  "io_timeout": 30,
  "known_hosts_file": "~/.ssh/known_hosts",
  "max_bytes_per_second": 0
}
```

| 字段                                        | 含义                                                                   |
| ------------------------------------------- | ---------------------------------------------------------------------- |
| `host` / `port`                             | SSH 主机名或 IP 与端口，不要把账户或路径拼进 `host`                    |
| `username` / `password`                     | 当前服务器的一组登录凭据                                               |
| `upload_sources` / `upload_destination`     | 默认本地源列表与远端目标目录                                           |
| `download_sources` / `download_destination` | 默认远端源列表与本地目标目录                                           |
| `connect_timeout` / `io_timeout`            | 连接和网络读写超时秒数，默认 15 和 30                                  |
| `known_hosts_file`                          | 额外只读的 OpenSSH 已知主机文件，空字符串使用默认 `~/.ssh/known_hosts` |
| `max_bytes_per_second`                      | 可选速率上限，单位为字节每秒，0 表示不限速                             |

源列表可以同时包含文件和目录，空数组表示调用时需要明确给出源列表。Windows 路径可以使用 `/`，JSON 中使用反斜杠时应写成 `\\` 符号，未知字段、非法端口、无效超时和损坏的 JSON 会被拒绝，不会自动覆盖损坏配置。

## 首次连接和主机指纹

先在对话中请求测试文件传输连接。`file_transfer_test_connection` 会校验已有 OpenSSH 信任记录和插件自己的记录。未知主机会返回 SHA256 主机指纹和一次性确认 token，此时不会认证或传输文件。核对返回的指纹后，在对话中明确确认该指纹。`file_transfer_trust_host_key` 必须使用这次测试返回的 `fingerprint` 和 `confirmation_token`。新信任记录只写入：

```text
processing_general/file_transfer_known_hosts
```

原有用户 `~/.ssh/known_hosts` 保持只读。已经可信的主机发生密钥变化时会拒绝连接，不会自动当作首次连接接受。主机确认 token 仅用于确认该次连接观察到的指纹。

## 上传、下载与目录映射

以下请求使用配置中的默认连接，调用参数也可以覆盖默认源与目标：
- 上传 C:/Data/result.tif 和 C:/Data/vector-output 到服务器的 C:/RemoteData/，完成后报告校验结果。
- 将服务器的 C:/RemoteData/vector-output 下载到本机 C:/Data/downloads/。

每个源保留自己的文件名或目录名。例如上传 `C:/Data/vector-output` 到 `C:/RemoteData/` 后，内容位于 `C:/RemoteData/vector-output/`。目录会递归复制，保留空目录和普通文件。目录合并时，同名文件仍遵循显式覆盖规则。上传和下载都会递归创建缺失的目标目录，从最近存在的普通祖先目录开始逐级检查和创建。例如 `C:/RemoteData/new-output/results/` 的多个父目录缺失时，也可以直接传输。磁盘根必须存在，路径仍受文件系统权限和 QCopilots 文件系统规则约束。

传输使用任务 ID 查询进度。工具返回启动成功不代表数据已经传输完成，应通过 `file_transfer_get_job` 查询至成功、失败或取消状态。完整文件执行 SHA256 校验，校验通过后才发布到最终名称。校验过程需要读取源和目标数据，耗时和网络流量可能超过仅复制一次。本地路径遵守 QCopilots 的文件系统规则，不跟随符号链接、junction 或不支持的特殊路径。UNC、映射网络盘和设备路径不受支持。传输保证普通文件内容及目录结构，不复制 ACL、所有者、NTFS ADS 或文件时间戳。`formal_restricted` 模式拒绝远程 SSH 传输，网络禁用时也拒绝连接。

## 中断、恢复和清理

主动取消、异常断线或服务异常停止都会保留可恢复的传输记录和部分文件。恢复前会检查服务器身份、路径、源文件、已发布文件和部分数据。恢复使用当前配置中的密码，连接身份和源目标不能悄悄改变。源文件改变或校验不一致时会拒绝继续使用旧断点。

工具 `file_transfer_cancel_job` 停止正在运行或排队的任务，状态经过 `cancelling` 到 `cancelled`。收到取消请求不等于工作线程已经停止，应查询到 `cancelled` 且 `resumable: true` 后再恢复。`file_transfer_resume_job` 为同一 `transfer_id` 返回新的 `job_id`，旧尝试的结果仍可查询。对终态任务重复取消只返回现有状态。取消和最终发布同时发生时，以实际发布结果为准。

要放弃断点，先取消活动任务并等它停止，再调用 `file_transfer_discard`。失败或中断的终态任务也可以直接放弃。放弃后不能恢复，只清理该任务拥有的部分文件、断点以及确认由任务创建的空目录。已发布完整文件和用户原有目录始终保留。目录创建响应丢失而无法确认归属时，会报告 `directory_ownership_unknown` 及残留路径，不自动删除该目录。

TCP 连接、SSH 握手、认证及 SFTP 读写支持取消。数字 IP 无需 DNS 解析。使用主机名时，系统 DNS 解析尚未返回期间可以保持 `cancelling`，解析返回后不会继续建立连接。传输完成或清理成功后会删除可恢复断点，并保留精简终态历史供查询。保留历史不表示该任务仍有可恢复数据。

网络不可用时，放弃操作可能暂时无法删除远端部分文件。结果会报告 `cleanup_pending: true`，保留清理所需的记录，任务仍然不可恢复。重新连接后使用 `file_transfer_discard` 重试清理。主动取消并保留断点时 `cleanup_pending` 为 false，是否可恢复应以 `resumable` 判断。断点使用 schema 2，固定清单、逐条目状态和目录归属分开原子保存，大量小文件不会在每次更新时重写完整清单。

## 工具接口

以下命令统一归入文件传输分类。上传和下载均支持单个文件、目录及其混合列表，目录会递归处理。

| 工具                            | 参数和用途                                                                 |
| ------------------------------- | -------------------------------------------------------------------------- |
| `file_transfer_get_config`      | 无参数，查询脱敏配置及其文件路径                                           |
| `file_transfer_update_config`   | 扁平配置字段，保存指定变更                                                 |
| `file_transfer_test_connection` | 无参数，测试连接或获取首次主机确认信息                                     |
| `file_transfer_trust_host_key`  | `fingerprint`、`confirmation_token`，记录用户明确确认的新主机              |
| `file_transfer_start_upload`    | 可选 `sources`、`destination`、`overwrite`、`client_request_id`            |
| `file_transfer_start_download`  | 参数同上传，方向为服务器到本地                                             |
| `file_transfer_get_job`         | `job_id`，可选 `files_offset`、`files_limit`，查询进度、文件结果和恢复信息 |
| `file_transfer_list_jobs`       | 可选 `states`、`limit`，列出最新任务摘要，不展开文件明细                   |
| `file_transfer_cancel_job`      | `job_id`，停止活动任务并保留断点                                           |
| `file_transfer_resume_job`      | `job_id`，可选 `client_request_id`，恢复取消或异常中断的传输               |
| `file_transfer_discard`         | `transfer_id`，清理放弃的传输或重试挂账清理                                |

`client_request_id` 是可选幂等键。相同键用于同一请求可避免重试时重复启动任务。错误使用稳定的 `code`、说明、是否可重试和受控详情返回，不返回底层异常中的凭据。工具 `file_transfer_get_job` 的 `files_offset` 默认 0，`files_limit` 默认 100，允许 1 至 200。返回 `files_total` 和 `next_files_offset`，继续使用下一页偏移查询，直到 `next_files_offset` 为 null。每页都返回任务进度及状态，不需要为了查询进度读取全部明细。旧尝试的终态文件结果不会被后续恢复尝试改写。其中 `resumable` 表示当前传输是否仍能通过该任务 ID 恢复。后续尝试运行中、传输成功或已放弃时，旧尝试也返回 false，但其历史状态和文件结果保持不变。

## 运行依赖和测试

SSH 后端使用 Paramiko 包，完整依赖版本在本插件的 `requirements.lock` 中锁定。服务管理器沿用现有 `uv run --with-requirements` 启动机制。活动 QGIS 随包 Python 已完整安装锁文件中的精确版本且依赖可正常导入时，服务可直接使用这些包，首次启动也可离线完成，依赖未满足时，工具 uv 可能准备隔离依赖环境并使用缓存或下载依赖。若包元数据满足版本要求但模块文件损坏，也可能直接复用后在导入模块时失败。Windows 发布应通过 QGIS-Materials 构建方案的 `pipeline.extraPythonPackages` 预装全部锁定依赖，并验证无缓存离线启动。依赖缺失、损坏或版本不符时，应修复发布包，保留主机指纹确认、认证和文件完整性校验。

传输适配层在单个工作线程中批量发送有界请求，每批最多 32 个读取请求或 8 个写入请求。写入必须收到全部成功响应后才能推进断点，读取保持顺序和完整校验，不创建预读线程。该实现依赖已审查的 Paramiko 5.0.0 请求分发接口，升级依赖时必须同步审查适配层并运行协议和真实回环测试。不匹配的版本会明确拒绝传输，避免静默使用未经验证的接口。

配置、工具列表和假后端测试不导入 Paramiko。实际连接时才加载 SSH 后端依赖。General 服务只在首次文件传输调用时初始化传输状态，传输目录不可写不会阻止原有 Processing 工具启动。锁文件已针对 Windows x64 的 QGIS Python 3.12 解析。其中 `tests/src/python/test_qcopilots_mcp_server_file_transfer_config.py` 覆盖配置默认值、明文持久化与脱敏输出、手动编辑重载、非法配置、原子保存失败、临时文件自动清理、并发变更和 11 个 MCP 工具的调用契约。测试使用临时目录和假后端，结束后自动删除产物，不连接外部服务器。

独立真实服务器脚本位于 `tests/src/python/qcopilots_file_transfer_live_acceptance.py`。它直接使用同一套传输管理器和真实 SSH 后端，不修改用户的文件传输配置。密码只从环境变量 `QCOPILOTS_FILE_TRANSFER_TEST_PASSWORD` 读取，既不接受密码命令行参数，也不在结果中回显密码。以下 PowerShell 示例中的解释器、服务器、账户和测试根目录需要替换为实际值，两个测试根目录必须事先存在：

```powershell
$fileTransferPython = "C:\Path\To\python.exe"
$fileTransferRepository = "C:\Dev\github-repos\QGIS"
$fileTransferTestCredential = Get-Credential -UserName "upload-user" -Message "输入独立 SSH 测试账户的密码"
$env:QCOPILOTS_FILE_TRANSFER_TEST_PASSWORD = $fileTransferTestCredential.GetNetworkCredential().Password
try {
    & $fileTransferPython -m uv run --python "$fileTransferPython" --with-requirements `
        "$fileTransferRepository\python\plugins\qcopilots_mcp_server_processing_general\requirements.lock" `
        "$fileTransferRepository\tests\src\python\qcopilots_file_transfer_live_acceptance.py" `
        --host "192.0.2.10" --port 22 --username "upload-user" `
        --remote-base "C:/RemoteData/TestData/" `
        --workspace-base "C:\Data\TestData" `
        --report "C:\TestReports\qcopilots-file-transfer-live.json"
    if ($LASTEXITCODE -ne 0) { throw "文件传输真实验收失败，请检查场景结果和清理状态。" }
}
finally {
    Remove-Item Env:QCOPILOTS_FILE_TRANSFER_TEST_PASSWORD -ErrorAction SilentlyContinue
    $fileTransferTestCredential = $null
}
```

脚本默认只接受用户 `~/.ssh/known_hosts` 中已有的主机信任，可以通过 `--known-hosts` 指定另一个只读文件。未知主机必须提供已独立核实的 `--host-key-fingerprint SHA256:...`，脚本仅在实际指纹完全匹配时写入本轮临时信任文件。不要把尚未确认的探测结果当作已确认参数传入。

默认生成 256 MiB 的确定性二进制文件用于取消和恢复测试。开发时可以添加 `--large-mib 8` 快速检查，正式验收保留默认值。覆盖场景包括文件与目录混合双向传输、中文与空格名称、嵌套空目录、空文件、目录清单及 SHA256 校验、默认同名冲突、显式原子覆盖、错误密码脱敏、双向取消后恢复、真实连接中断续传，以及新建管理器后的断点恢复。放弃与清理重试单独验证。结果明确记录服务器实际支持原子覆盖还是拒绝覆盖并保留原文件，续传必须证明复用了已传字节。

提供 `--corpus-root` 可以将预先准备的持久样本作为只读来源执行完整双向校验。样本目录与 `--workspace-base` 必须互不包含。包含大量小文件时，脚本还比较 512 个和 4096 个文件的断点写入量，并检查文件明细分页不遗漏、不重复及查询不会改写断点。超过 4 GiB 的样本可以验证大文件计数与传输。耗时较长的验收可以用 `--timeout` 指定每个传输的等待上限。`--scenario-group` 默认为 `all`。选择 `baseline` 只运行基本功能和生命周期场景，选择 `corpus` 只运行持久样本及小文件规模测试，后者必须同时指定 `--corpus-root`。分组运行时每轮仍使用独立 UUID 目录和清理流程，应分别指定报告路径。

本地和远端只在本轮 UUID 子目录内创建数据，以所有权标记验证清理范围。脚本在 `finally` 中停止工作线程并清理本轮数据、临时配置和断点，保留根目录中原有的数据。清理失败返回非零退出码，远端不可达时会有界重试，仍失败则保留必要清理记录并输出本轮安全标识。可选 `--report` 保存各场景耗时、文件大小、校验摘要、覆盖分支、恢复复用字节及最终清理结果，不保存凭据或源目标路径。该报告应放在测试 UUID 目录之外。

持续离线时保留的清理记录不包含密码，重连后可通过 `--retry-cleanup` 指向该记录再次清理。仍需提供当前测试密码和已核实的主机信任信息。清理记录不能替代对目标目录所有权的验证。此脚本的管理器重建验证不能代替 QGIS 实机重启验收。对话验收仍需要在 QGIS 的 QCopilots 中发起配置、连接、上传、下载、取消与恢复操作，再检查任务结果、SHA256 和测试产物清理情况。
