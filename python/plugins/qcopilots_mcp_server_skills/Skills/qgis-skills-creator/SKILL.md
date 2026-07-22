---
name: qgis-skills-creator
description: "为可复用的 QGIS、GIS、矢量、栅格、点云、网格、三维和地理服务工作流拆解需求、发现当前工具、生成可多轮修订的详细计划，并在用户明确批准当前 plan_id 与 revision 后安全创建和验证严格规范技能。用于用户要求设计、新建或审查 QGIS 地理处理技能时，也可只产出或审查计划并停在等待批准状态；不用于只需一次性执行地理处理、仅咨询 QGIS 用法或修改现有技能的场景。"
---

# QGIS-Skills-Creator

## 核心契约

把可重复的 QGIS 地理数据需求转成最小、可移植、可审查的技能包。始终按以下状态机推进：

```text
DISCOVER → ANALYZE → DRAFT_PLAN → AWAIT_EXPLICIT_APPROVAL
→ CREATE → VALIDATE → FORWARD_TEST → DELIVER
```

把这些状态当作硬门禁。批准前只做只读发现、需求分析和计划渲染；不得创建、修改、移动或删除目标技能的任何文件。只有用户对 current/exact `plan_id + revision` 给出 canonical explicit approval 后才进入 `CREATE`。沉默、“继续”“不错”“可以试试”、批准旧 revision 或批准时附带修改都不算批准。

每次任务开始完整读取 [创建工作流](references/qgis-skill-creation-workflow.md)。按需再读取：

- 制定 tool discovery 快照或选择工具前，读取 [工具目录](references/qgis-tools.catalog.json) 及其 [Schema](references/qgis-tools.catalog.schema.json)。
- 识别数据域、设计输入输出契约或验收标准时，只读取 [地理数据域检查清单](references/geospatial-domain-checklists.md) 中与当前任务有关的章节；不要把无关数据域复制进目标技能。
- 创建或校验机器计划前，读取 [计划 Schema](references/qgis-skill-plan.schema.json)，并从 [计划模板](assets/qgis-skill-plan.template.json) 起草。
- 渲染目标技能正文时，读取 [目标 SKILL 模板](assets/generated-skill-SKILL.template.md)，但最终 frontmatter 只保留 `name` 与 `description`。
- 默认从 [目标 openai.yaml 模板](assets/generated-skill-openai.template.yaml) 生成 `agents/openai.yaml`；若目标运行面明确不使用 UI 元数据，才可在计划中说明理由后省略。
- 对计划执行确定性 review、render 或 verify-staged 时，运行 `scripts/plan_gate.py`；脚本只读，不负责批准或创建。

## DISCOVER：发现环境和完整工具面

从激活结果返回的 `skill_root` 解析本技能根，不从当前工作目录猜测。按工作流中的标记算法发现服务 manifest、唯一 Skills 根、发布根和相对 wrapper；不得保存机器绝对路径到本技能或生成技能。

枚举当前 session/host available tools，包括宿主实际提供的 Builtin、Skills、Interactive、Processing、其它 MCP 工具，以及宿主允许探测的 QGIS CLI。static/bundled catalog does not limit or restrict tool discovery：静态目录只是可移植的已知能力、相对路径和风险元数据快照，不是工具白名单。新出现但实际可用的工具也要记录并评估，不能仅因目录尚未列出而从发现结果中删除。

分别记录四个集合，禁止混用：

- `discovered`：从会话、服务、registry 或文件标记实际观察到。
- `eligible`：与需求、版本、路径、数据类型和副作用相容，可进入候选。
- `selected`：本 revision 计划实际准备调用的 exact 最小子集。
- `actually available`：当前会话此刻真实可调用或可执行；可用不等于已经获权。

在机器计划中用 `capability_snapshot.capabilities[]` 的 `discovered=true`、`eligible`、`eligibility_evidence`、`ineligibility_reason` 和 `available` 表达前三个候选状态与 actually available；只有出现在 `selected_tools[]` 的能力才属于 selected。不可把不可 eligible 或不可 available 的能力选入计划。每项能力还必须携带 live/manual reviewed risk、side effects、invocation policy、operation contract、authoritative nested-target map、algorithm parameter constraints、对应哈希和证据；未知项按 R3 fail closed 且不可 selected。CLI 能力额外绑定当前 resolved executable、文件 SHA-256 和探测证据。

计划中的 exact algorithm ID、参数和 `allowed_algorithm_ids` 只绑定本 revision 的审查对象，不建立技能级 allowlist，也不授予永久权限。

## ANALYZE：requirement decomposition

先判断用户是否真的需要可复用技能。若只是一次性处理或普通说明，解释维护成本并建议直接完成任务；用户仍明确要求复用时再继续。

保留用户原始需求，并把信息标为 `CONFIRMED`、`INFERRED` 或 `UNKNOWN`。至少拆解：

1. 精确触发场景、边界正例、负例和近失配。
2. 输入来源、格式/provider、数据域、图层/band/点维度/网格数据集组/服务集合、CRS、垂直基准、时间轴、单位、规模和只读边界。
3. 输出格式、目标、命名、schema/bands/维度/LOD/服务响应契约、质量阈值、工程加载和 fail-if-exists 策略。
4. provider、QGIS 工程、网络、数据库、凭据、Shell、临时目录和清理需求。
5. 单一职责 DAG 节点、前后条件、exact arguments、作用目标、失败、回滚和验收证据。

只询问会改变算法、参数、目标、副作用或验收的阻塞问题。不得猜测缺失 CRS，不得用“赋予 CRS”替代重投影，不得把输出指向输入的物理别名。

## DRAFT_PLAN：生成 detailed plan

从计划模板创建新的任务计划。为计划使用稳定 `plan_id`，从 revision 1 开始。完整填充场景、范围、事实、输入输出契约、适用数据域及其地理语义、环境、能力快照、selected tools、workflow、目标文件树、副作用与调用审计说明、验证矩阵、风险、未决项和修订日志。对未选数据域明确写 `not-applicable` 理由，不得用矢量或栅格检查冒充点云、网格、三维或服务验证。

每个选中 Processing 操作必须列 exact `provider:algorithm_id` 和完整参数；每个 CLI 操作必须列 live resolved executable、文件 SHA-256、operation、argv template、materialized argv、cwd、stdin 和对应 allowed roots。选中项是审查绑定，不是获权声明。

对每个节点让 gate 校验 authoritative arguments 同时满足 live input schema、binding constraint 和 capability-reviewed、hash-bound selected-tool parameter constraints；Processing 节点的 `arguments.algorithm_id` 必须精确等于 selected algorithm。capability 用 `reviewed_nested_target_kinds` 审查每个结构化 leaf 的 exact RFC 6901 pointer 与唯一 target kind，selected tool 精确复制该 map/hash；node `nested_target_bindings[]` 的 key 集必须与实际 structured arguments/stdin leaves 全等，且每个 kind 必须等于 authoritative map 并与实际 value family 相容。RFC 3986 scheme 分类必须可达且 fail closed：`file` 是 local path；`postgres`/`postgresql`/`mssql`/`mysql`/`mariadb`/`oracle`/`sqlite` 是 database；`http`/`https`/`ws`/`wss`/`ftp`/`ftps`/`s3` 及未知 absolute scheme 是 network。对应 kind 只能是 local file/path、`database`、`network-endpoint`；`no-network` 必须拒绝 network。即使某叶是 `none` 也必须覆盖，且 URI 或路径形值都不得标成 `none`。Processing `parameters` 和 `qgis_process` stdin 同样适用。从每个实际 leaf 派生 effect/permission target并做 input/output allowed-root containment。

让 gate 从 arguments、nested leaves、全部 canonical allowed roots、QGIS state、network policy，以及 CLI/Shell 的 executable、完整 argv、cwd、stdin 确定性构造节点的完整 expected effect set。`effect_targets[]` 必须与该集合全等；额外自报和缺失项都失败。catalog 外 live capability 仍开放发现，但 selected risk、side effects、invocation policy 和 operation contract 必须精确绑定 capability review；未知默认 R3/fail closed。catalog baseline 存在时不得低于它。R2/R3 permission record 必须按 effective risk exact coverage。这些记录只是宿主逐次调用的审计输入，不是工具 allowlist，也不授予权限。

若 selected capability 有 `known_issues`，为每个 exact issue 填写 `known_issue_acceptances[]`，绑定本 `plan_id`、当前 revision、capability ID 和 description hash，并给出明确 rationale 与 acceptance basis；缺项、旧 revision 或近似 issue 都不得提交批准。

在内存中确定性生成计划列出的每个目标文件，记录原始 UTF-8 字节的 SHA-256 和 byte size；批准前不得为此创建目标目录或 staging tree。执行：

```text
python scripts/plan_gate.py --mode review --plan <plan.json>
python scripts/plan_gate.py --mode render --plan <plan.json>
```

将 `render` 的完整 stdout 原样交给用户。不得另写一份近似计划作为批准对象。

## AWAIT_EXPLICIT_APPROVAL：revision loop

请求用户使用当前计划标识明确回复：

```text
批准计划 <plan_id> revision <revision>，按该计划创建。
```

用户要求任何会改变内容、工具、参数、路径、副作用或验证的修改时，保留 `plan_id`、递增 revision、更新全部受影响字段和哈希、展示差异并重新请求 explicit approval。旧批准不可复用。

只接受与渲染请求逐字一致的 canonical 语句；`<plan_id>` 和 `<revision>` 必须替换为当前值，句后不得附带修改：

```text
批准计划 <plan_id> revision <revision>，按该计划创建。
```

收到该语句后，先把原句和当前 revision、plan/capability/review hashes、target、`intent=create-current-plan`、aware `approved_at` 逐字段写入 `approval` 审计，把 `status` 设为 `approved`，并再次执行 `plan_gate.py --mode review`。只有返回 `ok: true`、时间顺序和全部 binding 均通过时才进入 `CREATE`；否则回到等待或新 revision。

批准只允许按该 revision 创建已经审查的文件。它不批准额外工具调用，cannot bypass host/llama-ui permission，也不能绕过沙箱、用户确认或宿主策略。遇到权限提示时，只请求当前 exact 调用所需范围；被拒绝就停止并报告。

## CREATE：受控创建

创建前重新发现实际能力和路径，确认计划没有漂移、未决项为空、用户批准仍绑定 current/exact `plan_id + revision`。再次执行 containment、no-overwrite 和 collision 检查：

- 从 manifest 相对 `skillsRoot` 拼出的 lexical 路径必须先检查 leaf 与每个 existing ancestor 的 symlink、junction/reparse，之后才能 resolve；目标必须是该已解析 Skills 根的直接子目录，且与 `qgis-skills-creator` 为 sibling，绝不能嵌套在 Creator 内。
- 目录名、frontmatter `name` 和计划技术名必须完全相同。
- 新建默认 `fail-no-overwrite`；目标已存在即停止，不能合并、覆盖或把旧批准改作更新批准。
- 对 executable、cwd 和全部 allowed roots 先检查 lexical leaf 与每个 existing ancestor 的 symlink、junction/reparse，再 resolve，并继续核对实体、containment 与 executable hash；所有写入仍必须位于精确目标根。
- 只创建计划列出的文件，每个文件小于等于 256 KiB，文本使用 UTF-8、LF 和末尾换行。

使用 resources-first、`SKILL.md` last 顺序：先创建并逐个核对 `references/`、`scripts/`、`assets/` 和必要的 `agents/` 资源，最后写 `SKILL.md`。默认生成 `agents/openai.yaml`，其中 `display_name`、25–64 字符的 `short_description` 和显式包含 `$<skill-name>` 的一句 `default_prompt` 必须与最终 `SKILL.md` 一致；不要添加用户未提供的图标或品牌字段。每次写入都显式 no-overwrite，并核对计划 hash/size。若中途失败，不得用宽泛清理命令；报告本次确切创建的路径和恢复选项。

Skills 服务只读。脚本只能由宿主已有执行工具显式运行，并继续接受权限审查。不要声称 Skills 服务会执行脚本，也不要在技能中加入内部工具权限列表。

## VALIDATE：完整验证

先确认 Creator 自身门禁健康；该自测只在隔离临时目录构造测试夹具，不修改 Creator 或目标技能：

```text
python scripts/plan_gate.py --self-test
```

创建后立即执行只读结构验证：

```text
python scripts/plan_gate.py --mode verify-staged --plan <plan.json> --staged-package-root <skill-root> --skills-service-manifest <qcopilots_service.json> --skills-root <skills-root>
```

然后通过 QCopilots 执行：

1. `list_skills` 刷新目录；确认目标 slug 出现、`conformance == "strict"`，且 diagnostics 无目标错误或 `skills.reload.failed`。
2. `read_skill` 核对 `skill_root`、frontmatter 和正文确实是新 revision，而不是 last-known-good 旧快照。
3. `list_skill_resources` 与批准文件树逐项比较。
4. 对每个必要文本资源调用 `read_skill_resource`，核对内容和相对路径。
5. 让客户端重新列举动态 tools、prompts 和 resources，确认激活入口更新。

校验 frontmatter 仅含 `name` 和 `description`；不得含 `allowed-tools` 或任何权限配置。若存在 `agents/openai.yaml`，校验核心 UI 字段、短描述长度以及 `default_prompt` 对 `$<skill-name>` 的显式引用。每个计划文件必须声明 `content_kind`；对所有声明为文本或可严格嗅探为 UTF-8 文本的实际 bytes（包括无扩展名文件）执行 LF、末尾换行、TODO/占位符、Markdown link 和机器绝对路径检查。UNC 必须按完整 server+share 语法识别并拒绝 normal/JSON-escaped、Unicode、extended、WSL、forward-slash 与非本机 `file://host/share`；`file://localhost`/loopback 视为本机 file URI，其他 URI authority 不冒充 UNC，普通 JSON escape/regex 文本不得误杀。另拒绝任意 drive 与常见 Unix 根。校验所有相对引用存在且不逃逸，且无未声明文件或绝对安装路径。

## FORWARD_TEST：forward-test

用新上下文和用户式任务做独立前向测试。只提供技能路径、原始任务和必要小型数据，不提供预期答案或已知缺陷。覆盖：

- 应触发、边界触发、明确不触发和近失配。
- 正常、边界、失败、能力漂移、输出 collision、幂等和清理。
- 与计划 `domains[]` 一致的内容语义（矢量、栅格、点云、网格、三维、服务或时空数据）、QGIS 状态、日志和验收证据。

测试可能写数据或改变 QGIS 状态时，先取得对应宿主权限并使用隔离数据。任一必需验证失败，停止交付，修复后重跑；若修复改变批准内容，回到新的 revision loop。

## DELIVER：交付

报告 plan ID、revision、目标 skill root、文件清单、实际工具与 exact algorithms、结构/QCopilots/资源/forward-test 结果、清理结果、未执行项和残余风险。只有所有必需检查都有实际证据时才声明成功；不得把未运行、无匹配、跳过或权限拒绝写成通过。
