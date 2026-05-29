# 子代理目录维护规则

本目录用于存放项目级子代理 TOML 配置和本目录的说明文档。根目录 `AGENTS.md` 仍是最高优先级规则；本文件只补充 `.codex/agents` 目录内子代理文件的维护细则，不能覆盖 `AGENTS.md`、平台分册或用户本轮明确要求。

## 文件范围

- `*.toml` 是 Codex 子代理配置文件，文件内字段决定子代理名称、描述、权限和开发者指令。
- `README.md` 是目录级说明文档，不是子代理配置文件，不应被当作 agent 加载。
- 不在本目录保存机器专属配置方案、依赖快照、构建目录、发布目录、凭据、代理地址或其它只适用于单台机器的长期配置。

## TOML 字段规则

- 子代理 TOML 应使用 Codex 官方字段，例如 `name`、`description`、`sandbox_mode`、`developer_instructions`、`nickname_candidates`。
- 不得使用非官方 `nickname` 字段。
- 通常不在子代理 TOML 中固定 `model` 或 `model_reasoning_effort`，子代理继承主会话模型策略。除非后续任务明确要求按角色分级，并同步更新相关说明。
- 不把 QGIS 版本、Qt 版本、工具链、配置方案名称、机器绝对路径、平台依赖快照、构建目录或发布目录写成通用规则。这些上下文只能来自主代理当轮任务包中的已核验证据。

## 同步要求

- 更新子代理清单、名称、权限、职责、派发时机、禁止事项或输出要求时，必须同步核验 `.codex/agents/*.toml`、`.codex/README.md` 和 `AGENTS.md`。
- 新增、删除或重命名子代理 TOML 时，必须同步更新 `.codex/README.md` 的 Subagents 清单，并检查 `AGENTS.md` 第 5 章的子代理清单、编排流程图和任务包要求是否仍一致。
- 修改本目录规则后，应使用 `codex_conformity_reviewer` 或等效只读审查核对 Codex 配置域是否发生漂移。
