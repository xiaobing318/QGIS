# QCopilots MCP Server Skills

本目录是 QCopilots MCP Server Skills 的默认技能根。服务按 Agent Skills 规范发现目录技能，并通过 MCP 提供目录、完整说明和按需资源读取。服务本身只读取技能内容，不执行脚本，也不授予额外权限。

## 规范与兼容模式

严格模式要求每个技能目录包含唯一的 `SKILL.md`。YAML front matter 必须提供 `name` 和 `description`，并可使用规范字段 `license`、`compatibility`、`metadata` 和 `allowed-tools`。技能名称使用小写字母、数字和单连字符分段，并与目录名一致。

兼容模式保留上述规范字段，同时接受服务已有的兼容处理和诊断，例如部分非规范字段形式、`SKILL.md` 的大小写变体，以及 `.zip`、`.skill` 归档。归档技能是 QCopilots 的扩展，不属于 Agent Skills 目录格式。客户端应查看 `conformance` 和 `list_skills` 返回的诊断，不应把兼容加载等同于严格规范通过。

## 渐进披露与热加载

服务分三层披露内容：先由 `list_skills` 返回名称、描述、slug 和规范状态，再由 `read_skill` 或每技能工具返回完整说明，最后仅在说明需要时读取 `scripts/`、`references/`、`assets/` 等资源。

技能文件或根配置变化后，服务会在下一次相关 MCP 请求时尝试重载。同一会话中，`list_skills` 是稳定的目录刷新入口。刷新后可以用新 slug 调用 `read_skill`。动态生成的每技能工具、提示和资源目录也会更新，但客户端若缓存了 `tools/list`、`prompts/list` 或 `resources/list`，可能需要重新列举才能看到变化。

重载采用稳定快照。扫描或配置读取失败时，服务保留最近一次可用快照并报告诊断。修复文件后，再次调用相关 MCP 入口即可重试。首次启动没有可用快照时，配置或扫描错误会直接失败并给出类别明确的诊断。

## 技能根配置

清单中的 `skillsRoot` 是服务选择技能根目录的唯一入口。相对路径按 `qcopilots_service.json` 所在目录解析，默认值是同目录下的 `Skills` 目录。服务不会从环境变量、`CODEX_HOME/skills`、`skills_roots` 或 `skills.roots` 读取额外技能根。修改 `skillsRoot` 后，下一次调用 `list_skills`、`read_skill`、每技能工具、提示或资源列表时会触发热加载，并以新的单一根目录为准。

`qcopilots_service.schema.json` 描述当前推荐的清单形态，因此要求显式写出 `skillsRoot`，并把校验策略放在 `skills.validation_mode`。运行时为了兼容旧清单，仍会在缺少 `skillsRoot` 时回退到同目录 `Skills`，也会接受旧的顶层 `skills_validation_mode`；这些兼容入口只用于迁移，不会新增第二个技能根来源。

## 脚本、资源与权限

目录技能会返回 `skill_root`。当技能说明要求运行 `scripts/` 下的文件时，应由宿主提供的执行工具处理，例如 `exec_shell_command`，并在用户或宿主权限策略批准后以 `skill_root` 为工作目录运行。具体工具和解释器由宿主决定，Skills 服务不会新增脚本执行引擎。

`allowed-tools` 只是技能作者的声明，不会自动启用工具，也不会绕过宿主权限、沙箱或用户确认。普通资源通过 MCP 按需只读。归档技能没有可执行的 `skill_root`，其中脚本也只作为归档资源读取，不能直接执行。
