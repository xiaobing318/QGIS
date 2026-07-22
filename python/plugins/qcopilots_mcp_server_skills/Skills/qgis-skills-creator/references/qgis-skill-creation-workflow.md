# QGIS-Skills-Creator 创建工作流

## 目录

1. 协议边界
2. 根目录和入口发现
3. tool discovery 与能力集合
4. requirement decomposition
5. detailed plan 和 revision loop
6. explicit approval
7. containment、no-overwrite 与 collision
8. resources-first、SKILL.md last 创建
9. 创建后验证与 forward-test
10. 故障停止条件

## 1. 协议边界

本协议用于创建新的、可复用的 QGIS 地理数据技能。它不负责一次性处理数据，不负责修改已存在技能，也不授予任何工具权限。

严格执行：

```text
DISCOVER → ANALYZE → DRAFT_PLAN → AWAIT_EXPLICIT_APPROVAL
→ CREATE → VALIDATE → FORWARD_TEST → DELIVER
```

批准前不得创建目标目录、staging tree 或任何目标技能文件。批准必须唯一绑定 current/exact `plan_id + revision`。只要用户修改需求、文件字节、工具、算法、参数、目标、副作用或验证，就递增 revision，旧批准失效。

计划批准与工具调用权限是两件事。计划描述“准备调用什么”；宿主决定“本次是否允许调用”。任何流程 cannot bypass host/llama-ui permission、沙箱或用户确认。

## 2. 根目录和入口发现

### 2.1 Creator 根

优先读取技能激活结果或 `read_skill` 返回的 `skill_root`。将其规范化为 `creator_skill_root`，并验证：

- 末级目录为 `qgis-skills-creator`。
- 根目录内存在 `SKILL.md`、`references/qgis-tools.catalog.json` 和 `scripts/plan_gate.py`。
- lexical path 和 resolved path 的差异已记录；任何 reparse/junction/symlink 歧义都会停止写入。

不得从进程当前目录、历史安装路径或固定盘符推断 Creator 根。

### 2.2 服务 manifest 和 Skills 根

从 `creator_skill_root` 向上有限搜索 `qcopilots_service.json`。对每个候选严格读取 JSON，并要求：

1. 顶层 `service_id` 是当前 Skills 服务。
2. 顶层 `skillsRoot` 是非空相对路径，不含盘符、根路径、空段、`.` 或 `..`。
3. 保存 `manifest.parent / skillsRoot` 的 lexical 路径；在任何 resolve 前检查该 leaf 及每个 existing ancestor，发现 symlink、junction 或 reparse 即失败。
4. 上述路径 resolve 后恰好等于 `creator_skill_root.parent`，并继续执行 containment 核对。
5. 只允许一个候选；零个或多个候选都停止并请求明确选择。

该规范化目录是 `qcopilots_skills_root`。新技能目标只能是：

```text
<qcopilots_skills_root>/<new-skill-name>
```

因此新技能与 Creator 是 sibling，绝不能位于 Creator 内部。不要从 `CODEX_HOME`、环境变量或其它技能目录推断这个根。

### 2.3 QGIS 发布根与 wrapper

读取 bundled catalog 的 `path_policy.qgis_root_discovery`，从 `creator_skill_root` 按其有限祖先层数向上搜索。候选必须同时包含 catalog 当前列出的全部相对 marker；不得把 marker 的观测结果写成永久事实。

- 唯一候选才成为 `qgis_package_root`。
- 没有候选时，请用户提供候选根并用同一 marker 集验证。
- 多个候选时，让用户选择后重新验证。
- 不使用固定 `../../..` 层数。

CLI 相对路径先来自 catalog，再与 live 文件系统核对。`bin/qgis_process-qgis-qt6.bat` 只是当前 catalog 的观测入口，必须在每次任务实时验证存在、位于发布根内、没有危险 reparse chain，并确认其真实参数。不得写入本机绝对路径到计划生成的技能资源。

如果实时安装不存在该 wrapper，或宿主没有可批准的进程执行入口，就把 CLI 标为 unavailable；不要猜测 `qgis-qt6-bin.bat` 或其它相似名称。

## 3. tool discovery 与能力集合

### 3.1 开放发现语义

枚举 session/host available tools 的完整集合：

- Builtin：读取、搜索、写入、编辑、执行和当前宿主实际注入的其它工具。
- Skills：list/read/activation/resource 工具。
- Interactive：当前 QGIS 图层、画布、工程、导出、加载、创建和要素修改工具。
- Processing：vector/raster 服务的 list/details/run，以及实时 registry 中的 algorithms/providers。
- 其它当前 MCP 服务和宿主已批准探测的地理 CLI。

static/bundled catalog does not limit or restrict tool discovery。catalog 是可移植基线，记录已知服务、相对入口、观测版本、风险和副作用；它不是技能级工具白名单。实际会话出现 catalog 未列工具时仍记录其 live 描述、input/output schema、版本、来源和副作用，再决定是否 eligible。不得为了让旧 catalog 看起来完整而隐藏新工具。

### 3.2 四个集合

对每项能力分别记录：

| 集合 | 判定 | 不代表 |
| --- | --- | --- |
| `discovered` | 从当前会话、服务、registry 或文件标记实际观察到 | 可安全调用 |
| `eligible` | 与任务、数据、版本、路径和风险相容 | 已获权限 |
| `selected` | 本 plan revision 审查的最小 exact 子集 | 永久 allowlist |
| `actually available` | 当前此刻真实可调用或可执行 | 宿主会批准本次调用 |

`selected_tools[].allowed_algorithm_ids` 是历史 schema 字段名。只把它用作本 revision 对 exact `algorithm_id` 的审查绑定；禁止通配符，不把它描述成全局权限列表。

机器计划从字段确定性派生四集合：`capability_snapshot.capabilities[]` 必须有 `discovered=true`；`eligible` 配合非空 `eligibility_evidence` 和 `ineligibility_reason` 表达资格判断；`available` 表达 actually available；被 `selected_tools[].capability_id` 引用才表达 selected。eligible 为 true 时 reason 必须为 null；eligible 为 false 时 reason 必须非空。selected 能力必须同时 discovered、eligible、available。

每项 capability descriptor 还要记录本次 live/manual review 的 `reviewed_risk_level`、完整六项 side effects、invocation policy、operation/argv/stdin/argument-target contract、`reviewed_nested_target_kinds`、`reviewed_parameter_constraints`、各自 SHA-256、review status 和 evidence。selected tool 必须精确复制 nested map、parameter schema 和 hash，不能自报更宽或不同 schema。catalog 外能力继续开放发现，但未知 review 必须按 R3 和全部 side effects fail closed，且不能 selected；明确审查后才可选择。写文件最低 R2，catalog 外 Shell、网络或潜在破坏能力最低 R3。catalog baseline 存在时，reviewed risk、true side effect 和 invocation restriction 只能相等或更严格，不能低报。

CLI capability 必须记录实时解析后的 exact executable、文件 SHA-256 和观测证据；selected tool 精确复制这些字段，并声明 executable 与 cwd 的独立 allowed roots。计划 review 对 executable、cwd 与所有 allowed roots 先逐项检查 lexical leaf 和每个 existing ancestor 的 reparse/junction/symlink，再 resolve 并核对实体、containment 与 executable bytes/hash；漂移、reparse、根外路径或不再存在均失败。

### 3.3 发现顺序

1. 严格解析 catalog 和 catalog schema。
2. 解析 Creator、manifest、Skills 和 QGIS package roots。
3. 枚举当前会话真实工具，而不是仅读取 manager 默认启动配置。
4. 对活动 MCP 服务刷新工具描述与 input/output schema。
5. 调用 Processing list，记录完整返回和 provider。
6. 对候选 exact algorithm 调用 details。
7. 若 CLI 实际可用且宿主允许只读探测，用结构化 argv 获取 `--version`、`--json list` 和 `--json help <algorithm_id>`；不拼接 shell 字符串。
8. 记录 deprecated、known issues、参数、defaults、enum、outputs、flags、cancel 能力和版本。
9. 规范化能力快照，分别计算 description、input schema、output schema 与 snapshot SHA-256。
10. 在用户批准后、创建前重复发现；发现漂移就回到新 revision。

发现阶段只执行只读调用。即使某调用语义为只读，通用 Shell 仍可能触发宿主权限提示；不得绕过。

## 4. requirement decomposition

### 4.1 判断是否值得创建

确认工作流会重复、输入输出契约稳定、QGIS 专用知识值得固化，并且能形成一个连贯职责。若普通回答、一次性执行或一个小脚本已经足够，先提出更轻量方案。

### 4.2 场景模块

为目标技能写明：

- 技术名称、显示名称、一句话能力和可重复结果。
- 4–6 个明确正例、2–4 个边界正例。
- 4–6 个明确负例、4–6 个近失配负例。
- provider、QGIS bridge、GUI/headless、网络、凭据、规模和取消先决条件。

description 同时写“做什么”“何时触发”“数据/格式/操作词”和“近失配边界”。不得声称支持尚未 discovered 或无法验证的 provider、格式或算法。

### 4.3 数据与地理语义

先从 `vector`、`raster`、`point-cloud`、`mesh`、`three-dimensional`、`geospatial-service`、`spatiotemporal` 和 `tabular` 中选择实际 `domains[]`。只加载 `geospatial-domain-checklists.md` 中对应章节，并把域专属约束写入计划；不要让矢量/栅格默认项掩盖其它数据域。

为每个输入/输出建立稳定 ID。至少记录：来源或目标、格式/driver、图层/band/点维度/网格数据集组/服务集合、水平与适用的垂直 CRS、轴顺序、空间与时间单位、域专属类型、schema/bands/dimensions、NoData/缺测、extent、resolution/density/LOD、质量阈值和 overwrite policy。

- 缺失 CRS 不等于 WGS 84。
- “赋予 CRS”不等于重投影。
- 分类栅格与连续栅格不得盲用同一 resampling。
- 水平 CRS 转换不等于垂直基准转换；点云、网格与三维数据必须分别验证高度语义。
- 远程服务必须绑定协议版本、exact endpoint/collection 和分页、认证、限流、缓存策略。
- 多文件格式要包含 sidecars；多层容器要区分 dataset 与 layer collision。
- 默认输入只读、输出 fail-if-exists。

### 4.4 workflow 节点

一个节点只做一个可验证转换或检查。记录依赖、selected tool、operation、authoritative arguments、参数来源、effect targets、前后条件、输出、中间物、风险、超时/取消、幂等、fallback、验收和清理。

fallback 必须有精确触发条件。不能“失败后换工具试试”，不能暗加参数，也不能用一个节点批准去执行另一目标。

gate 必须把 node arguments 同时送入 live capability `input_schema`、每个 binding 的 `constraint_schema` 和 selected tool `parameter_constraints`；执行 required/omitted argument、CLI placeholder/stdin/target-kind 与风险/副作用规则。

任何 object/array argument 以及整个 structured stdin 都必须先在 capability 的 `reviewed_nested_target_kinds` 中以 exact RFC 6901 JSON Pointer 审查唯一 target kind，并由 selected tool 复制 map/hash。node `nested_target_bindings[]` 的 pointer 集、reviewed map key 集与实际 terminal leaf 集三者必须全等，每个 node kind 也必须精确等于 reviewed kind并与实际 value family 相容；普通数值以 `target_kind=none` 明确分类。按 RFC 3986 scheme 分类：`file` 只能使用 local file/path kind；`postgres`、`postgresql`、`mssql`、`mysql`、`mariadb`、`oracle`、`sqlite` 只能使用 `database`；`http`、`https`、`ws`、`wss`、`ftp`、`ftps`、`s3` 及未知 absolute scheme 只能使用 `network-endpoint`，且 `no-network` 拒绝它们。SQLite URI 保留 database 合同与其本地数据风险，不降格为无目标值。任何 URI 或绝对/疑似本地路径都不得伪装成 `none`。Processing 的 `parameters` 和 `qgis_process` 的 `stdin_payload.inputs` 都适用。路径 target 从实际 leaf 规范化，敏感显示值可哈希，但 containment 仍使用原始值完成。

capability 的 `reviewed_parameter_constraints` 必须绑定 exact algorithm argument schema 及 hash/evidence，selected `parameter_constraints` 与 hash 必须逐字节语义等同。Processing 和 CLI 的 schema 必须用 required `algorithm_id` const 绑定 exact selected algorithm；每个 Processing node 的实际 `arguments.algorithm_id` 还必须再次精确相等。

gate 从实际 arguments、顶层与 nested target、canonical input/output/executable/cwd roots、QGIS state、network deny/endpoint，以及 executable、精确物化 argv、cwd 和 stdin hash 构造完整 expected effect set。计划的 `effect_targets[]` 必须集合全等，不能额外自报或漏报。CLI `materialized_argv` 必须逐 token 从 reviewed `argv_template` 替换独占 placeholder 得到；不进行 shell parse 或字符串拼接。每个 R2/R3 tool 和 node 必须有 effective risk 与 exact effect targets 均覆盖的 `permissions[]` 记录。该记录只供宿主逐次 permission audit，不形成全局技能 allowlist，也不授予或绕过任何调用权限。

selected capability 只要有 `known_issues`，就必须为每个 exact issue 提供一个 `known_issue_acceptances[]` 记录，精确绑定当前 `plan_id`、revision、capability ID、description hash、issue、rationale 和 acceptance basis。旧 revision、近似文本、泛化接受或漏项均失败。

## 5. detailed plan 和 revision loop

复制 `../assets/qgis-skill-plan.template.json` 的结构，使用 `../references/qgis-skill-plan.schema.json` 校验。模板内 `$schema` 从 asset 自身闭包解析到该 package schema；复制为外部工作计划后，不得假定同一相对位置仍然成立。模板只是起点；提交审查前替换全部 placeholder、示例时间和零 hash。

目标文件必须在内存中生成最终 bytes，再把每个相对路径、来源、required、SHA-256 和 byte size 写入计划。此时仍不得写目标目录或 staging tree。

计划至少覆盖：

1. 原始需求、解释目标、事实等级、范围和非目标。
2. 场景正负例、先决条件和 frontmatter description。
3. 输入输出、`domains[]` 及 CRS/垂直基准/单位/时间和域专属语义。
4. discovered/eligible/selected/actually available 工具证据。
5. exact service/tool/algorithm/operation/arguments、nested JSON Pointer targets、风险、副作用和 fallback。
6. DAG、完整 expected effect set、CLI executable/materialized argv/cwd、timeout、cancel、rollback 和清理。
7. 目标树、每个文件最终 hash/size 和加载条件。
8. `resources-first-skill-md-last`、containment、no-overwrite、collision 和权限说明。
9. 结构、trigger、功能、失败、漂移、QGIS 集成、资源和清理验证。
10. 风险、替代、未决问题和 revision 差异。

运行：

```text
python ../scripts/plan_gate.py --mode review --plan <plan.json>
python ../scripts/plan_gate.py --mode render --plan <plan.json>
```

路径示例是相对本 reference 的说明；实际调用应从激活的 `skill_root` 解析 `scripts/plan_gate.py`。校验复制到 package 外的计划时，必须使用 gate 从同一技能根解析的默认 schema，或显式传入 `--plan-schema`、`--catalog`、`--catalog-schema`；不得依赖外部 plan 的 `$schema` 相对位置。

`review` 返回 JSON 诊断与 canonical hashes；`render` 只在 review 通过时输出确定性 Markdown。脚本只读，不创建技能、不授予批准、不执行工具。

用户提出修改后保持 plan ID、revision 加一、更新所有受影响的内容/hash/风险/验证，展示差异并重新 render。这就是 revision loop；任何旧批准都失效。

## 6. explicit approval

只接受能唯一绑定当前计划的明确语句，例如：

```text
批准计划 qgis-skill-plan-example revision 2，按该计划创建。
```

拒绝沉默、超时、“继续”“不错”“可以试试”、只批准某次工具调用、批准旧 revision，或“批准但顺便修改”的回复。最后一种属于变更请求，必须先生成下一 revision。

canonical approval 必须逐字等于：

```text
批准计划 <plan_id> revision <revision>，按该计划创建。
```

收到该句后仍不能直接创建。先把 `approval.state=approved`、`approved_revision`、当前 plan/capability/review hashes、`approved_target_relative_path`、`intent=create-current-plan`、原始 `user_statement` 和 aware `approved_at` 逐字段回填，把 `status=approved`，并保证 `created_at <= updated_at <= approved_at`。然后重新运行 `plan_gate.py --mode review`；只有输出 `ok: true` 才进入 CREATE。模糊回复、旧 revision、附带修改或时间倒序均返回等待或新的 revision。

计划 JSON 中的 `approval` 对象是模板兼容的审计字段，不是权限来源。不要自行补造批准数据，也不要把 `plan_gate.py` 输出当作用户批准。当前任务的 authority 是本轮可追溯对话中用户对 exact plan ID/revision 的明确语句；具体写入或执行仍需宿主允许。

## 7. containment、no-overwrite 与 collision

创建前和每次写入前都执行：

1. 从 manifest 重新解析 Skills 根。
2. 规范化目标，确认它是 Skills 根的 direct child 和 Creator 的 sibling。
3. 检查目标及其 lexical/resolved ancestor chain 没有不可信 reparse、junction 或 symlink。
4. 确认 target 完全不存在；存在即 collision，停止。
5. 对计划中每个相对路径拒绝盘符、UNC、根路径、反斜杠、空段、`.`、`..`、Windows 保留名和非法字符。
6. 合成候选路径并确认仍在目标根内。
7. 每个文件显式 no-overwrite，绝不使用默认覆盖。
8. 每个文件声明 `content_kind=text-utf8|binary`；写后立即读取原始 bytes，核对 size 和 SHA-256。

输入输出数据也要规范化物理 identity。解析 file URI、provider 前缀、container layer suffix、symlink/junction/reparse、8.3 name；现有文件还要比较 file identity，防止 hardlink 让输出覆盖只读输入。任何 endpoint 无法规范化时 fail closed。

若目标已存在，不自动更新、合并、删除或改名。读取必要结构后提交独立更新计划；该计划不属于本技能的新建流程，除非用户另行授权扩展。

## 8. resources-first、SKILL.md last 创建

确认 plan/current capability/target 未漂移后，只写计划列出的 exact bytes：

1. 建立最小目录，不创建 README、CHANGELOG、过程记录或大型地理数据。
2. 写 `references/`，逐文件校验。
3. 写必要 `scripts/`，逐文件校验；脚本存在不代表可执行。
4. 写必要 `assets/` 和 `agents/`，逐文件校验。默认用 `generated-skill-openai.template.yaml` 生成 `agents/openai.yaml`，核心字段必须与最终 skill name、显示名和用途一致。
5. 复核当前目录树与计划一致。
6. 最后写 `SKILL.md`，使不完整目录不会过早注册为技能。
7. 用 `verify-staged` 对最终目标树做只读精确校验。

`SKILL.md` 必须 UTF-8、LF、无 BOM、有末尾换行且少于 500 行。frontmatter 只允许：

```yaml
---
name: exact-lowercase-skill-name
description: "What it does and exactly when to use it."
---
```

不得增加 `allowed-tools`、license、compatibility、metadata 或权限配置。正文用动作指令，资源从技能根解析，reference 从 `SKILL.md` 一层可达。

`agents/openai.yaml` 默认只生成：

```yaml
interface:
  display_name: "Human-facing name"
  short_description: "A 25 to 64 character UI description"
  default_prompt: "Use $exact-skill-name to perform the intended workflow."
```

所有字符串加引号；`default_prompt` 必须显式提到 `$<skill-name>`。仅当用户实际提供图标或品牌值时加入对应 interface 字段。该文件是 UI 元数据，不是权限或工具声明。

Skills 服务只读，只公开内容和资源。执行 `scripts/` 必须使用宿主已有工具，以返回的 `skill_root` 为基准，并接受当前权限提示。脚本不得把自己位于技能包内当作执行授权。

失败时停止后续写入。只报告本次确切创建的路径；没有精确且获准的清理调用就保留不完整目录并说明，不使用递归宽泛删除。

## 9. 创建后验证与 forward-test

### 9.1 本地严格验证

先运行 Creator 自测，确认安装包、schema 和 gate 内部正常/失败夹具都可用：

```text
python scripts/plan_gate.py --self-test
```

自测只使用隔离临时目录，不修改 Creator 或目标技能。失败时先修复 Creator，不继续创建。

执行：

```text
python scripts/plan_gate.py --mode verify-staged --plan <plan.json> --staged-package-root <created-skill-root> --skills-service-manifest <qcopilots_service.json> --skills-root <qcopilots-skills-root>
```

`verify-staged` 严格读取实际 manifest，核对 `service_id`、相对 `skillsRoot`、manifest hash、root identity/path；manifest 派生 Skills root 必须在 resolve 前扫描 lexical leaf 与 existing ancestors，再证明目标是该 root 的 direct child 和 Creator sibling。它还只读检查 exact tree、无 reparse、每个原始 hash/size、256 KiB 上限。对声明文本和实际可严格解码的 UTF-8 文本一律检查 BOM、LF、末尾换行、TODO/模板占位符、Markdown 相对链接和机器绝对路径；无扩展名文本不能逃逸。UNC 检测要求完整 server+share，拒绝 normal/escaped、Unicode、extended、WSL、forward-slash 和远端 `file://host/share`，但不把普通 JSON escape/regex、非-file URI authority 或 `file://localhost`/loopback 误判为远端 UNC；同时拒绝 Windows 任意 drive 以及常见 Unix 安装、home、data、source 和 workspace 根。`SKILL.md` 还必须少于 500 行且仅含 name/description；存在 `agents/openai.yaml` 时还检查核心 UI 字段、短描述长度和 `$<skill-name>` prompt 引用。它不是创建授权。

如当前环境提供官方 Agent Skills validator 或 Codex quick validator，再运行它们；说明每个 validator 的覆盖边界。

### 9.2 QCopilots 验证

1. 调用 `list_skills` 触发刷新。
2. 确认目标 slug、`conformance == "strict"`，diagnostics 无目标错误和 `skills.reload.failed`。
3. 调用 `read_skill`，确认 `skill_root` 和正文对应新 revision。
4. 调用 `list_skill_resources`，与计划逐项比较。
5. 调用 `read_skill_resource` 读取每个必要文本资源。
6. 重新列举动态 tools/prompts/resources 并实际激活新技能。

热加载使用 last-known-good。单独 `read_skill` 成功不能证明已加载新内容，必须同时检查 diagnostics 和具体 revision 内容。

### 9.3 forward-test

用独立上下文提交真实用户式提示，仅提供技能路径、任务和隔离输入。至少覆盖：

- 正例、边界正例、负例、近失配。
- 正常数据、空数据以及 `domains[]` 对应的 CRS/垂直基准/geometry/NoData/dimensions/topology/LOD/service/time 边界。
- provider/algorithm 缺失、schema drift、权限拒绝、输出 collision。
- 重跑幂等、成功/失败清理、QGIS 工程状态和内容级输出验收。

测试写文件、运行 Processing 或改变 QGIS 状态前再次取得宿主权限。记录实际 tool call、参数、结果、日志、QGIS 状态、文件和清理证据。不得用“文件存在”代替地理内容校验。

## 10. 故障停止条件

出现下列任一项就停止相应阶段：

- 无法唯一解析 Creator、manifest、Skills root 或 QGIS package root。
- target 不是 Skills root direct child，或与 Creator 路径相交。
- 当前实际工具、provider、algorithm、description/schema 或 wrapper 与计划漂移。
- 未决问题会改变工具、参数、目标、副作用或验收。
- 用户没有明确批准 current/exact plan ID/revision。
- target collision、containment 无法证明或 no-overwrite 不可保证。
- 宿主拒绝权限，或实际调用无法精确表达计划 operation/arguments/targets。
- `verify-staged`、strict conformance、resource read 或 forward-test 失败。

交付时逐项报告实际结果、未运行项、证据和残余风险。任何失败、跳过、无匹配或未执行都不能写成通过。
