## 概述

该目录中的内容是 Codex 所需要的配置文件，是为了能够在多个机器上同步 Codex 配置从而减少琐碎的工作量，同时也能提高使用 Codex 的一致性，目前该目录中的内容是针对仓库而言的 Codex 配置而不是全局的 Codex 配置。Codex 本身是支持分层级的配置，例如全局级别、仓库级别、目录级别，越靠近工作目录的配置将会覆盖之前的配置，除此之外命令行参数传递、会话选择（相当于参数传递）优先级最高。如果在 VS Code 里的会话/线程中选择设置，那么相当于把 model 等选项钉死成了会话覆盖，等价于命令行参数，优先级高于项目配置，这时候修改项目配置和全局配置然后重新启动 Codex 都是不会起作用。

## 项目配置

这个步骤是必须手动处理的,在用户级配置 ~/.codex/config.toml 加上项目被信任配置，具体示例如下所示：

**Linux Platform**
```toml
################################################################################
# Projects (trust levels)
################################################################################
# Mark specific worktrees as trusted or untrusted.
[projects]
[projects."/home/ubuntu-noble-240404/github_repos/QGIS"]
trust_level = "trusted"
```

**macOS Platform**
```toml
################################################################################
# Projects (trust levels)
################################################################################
# Mark specific worktrees as trusted or untrusted.
[projects]
[projects."/Users/xbyang/dev/github_repos/QGIS"]
trust_level = "trusted"
```

**Windows Platform**
```toml
################################################################################
# Projects (trust levels)
################################################################################
# Mark specific worktrees as trusted or untrusted.
[projects]
[projects."e:\\github_repos\\QGIS"]
trust_level = "trusted"
```

> [!NOTE]
> 1. 单个会话中选择的优先级最高，会覆盖配置文件中的设置。
> 2. 在 Windows/Linux/macOS 平台上针对项目级别的配置需要在用户级别的配置文件中将项目路径设置为信任，并且在 Windows 平台上路径编写需要小写盘符，例如 `e:\\github_repos\\QGIS` 。
> 3. 在特定的机器中需要对 writable_roots 配置进行修改，可以采用的一种方式就是将路径追加到数组中，这样可以使得配置文件在不同的机器之间实现类似通用的效果。

## Subagents

本仓库在 `.codex/agents` 中配置项目级子代理。子代理用于把大型 QGIS 开发任务拆成相互解耦的需求分析、仓库分析、实现、测试、审查、验证和构建闭环工作。主代理仍然负责锁定上下文、整合结论、判定 R 等级和输出最终验收结果。

`.codex/config.toml` 已启用 `multi_agent`，并设置了项目级并行线程上限。子代理 TOML 不固定模型、不固定 QGIS 版本、不固定机器路径、不固定 profile；这些上下文必须由主代理在每次任务开始时按 `AGENTS.md` 锁定后传给子代理。

本轮验证曾使用 `WPGWorkstation-Windows-x64-feature-final-3_44_7-mcp-Qt6-RelWithDebInfo` 作为当前验证 profile 示例。它不是默认 profile，也不能写入子代理通用规则。后续任务应以主代理当轮核验出的 profile 为准。

子代理 TOML 应使用 Codex 官方字段，例如 `name`、`description`、`sandbox_mode`、`developer_instructions`、`nickname_candidates`。不要使用非官方的 `nickname` 字段；不要在 agent 文件中写入 `model` 或 `model_reasoning_effort`，除非以后明确决定让某个代理脱离主会话模型策略。

| 文件 | 子代理名称 | 权限 | 主要作用 | 典型使用场景 |
| --- | --- | --- | --- | --- |
| `QGISAcceptanceAnalyst.toml` | `qgis_acceptance_analyst` | `read-only` | 提取需求目标、范围边界、确认项、验收标准和风险。 | 用户需求宽泛、验收口径不清、需要先形成 AC/TODO/确认项时。 |
| `QGISRepositoryCartographer.toml` | `qgis_repository_cartographer` | `read-only` | 分析源码结构、模块边界、测试入口、构建入口、跨平台影响、本地资料和官方资料。 | 陌生模块、跨平台影响不清楚、需要确认源码/测试/构建入口时。 |
| `QGISImplementationEngineer.toml` | `qgis_implementation_engineer` | `workspace-write` | 在主代理明确分配的范围内实现生产代码或生产资源改动，不改测试。 | 方案已确认，需要并行实现生产逻辑、插件、UI、Provider 或资源时。 |
| `QGISTestEngineer.toml` | `qgis_test_engineer` | `workspace-write` | 设计并实现测试、测试数据、测试夹具和必要测试注册，不改生产逻辑。 | 需求稳定后补测试、实现后补回归测试、测试覆盖不足或失败路径缺失时。 |
| `QGISRequirementImplementationReviewer.toml` | `qgis_requirement_implementation_reviewer` | `read-only` | 审查生产实现是否满足需求、验收标准和范围边界。 | 生产实现完成后，检查漏实现、错实现或范围漂移时。 |
| `QGISTestCoverageReviewer.toml` | `qgis_test_coverage_reviewer` | `read-only` | 审查测试矩阵、断言强度、失败路径、回归覆盖、测试注册和可执行性。 | 测试实现完成后，判断测试是否足以支撑验收时。 |
| `QGISCodeStyleReviewer.toml` | `qgis_code_style_reviewer` | `read-only` | 审查代码、测试、CMake、脚本和文档是否符合 QGIS 风格与格式规则。 | 提交前、大范围改动后、风格不确定或需要定位格式/命名/注释问题时。 |
| `QGISCodeQualityReviewer.toml` | `qgis_code_quality_reviewer` | `read-only` | 审查架构、兼容性、安全、性能、维护性、回归风险和测试缺口。 | 最终验证前，代码风险较高或需要资深 reviewer 把关时。 |
| `QGISVerificationEngineer.toml` | `qgis_verification_engineer` | `workspace-write` | 执行完整相关测试并回填证据，只允许测试日志、缓存和临时产物。 | 审查通过后，需要运行完整相关测试而不启动整体构建时。 |
| `QGISBuildVerifier.toml` | `qgis_build_verifier` | `workspace-write` | R2 时执行整体构建、测试、安装、打包与日志健康度分析。 | R2 任务、构建失败、测试失败、安装/打包/runtime validation 或日志根因分析时。 |

建议派发顺序对应 DevFlow。每次派发前，主代理需要把任务包讲清楚：目标、范围、禁止事项、当前 OS/ISA/QGIS/Qt/工具链/profile、两个占位符路径的 `Test-Path` 结果、R 等级、允许读写范围和期望输出。

1. 主代理前置检查：锁定平台、当前任务 profile、路径和 R 等级。
2. 并行启动 `qgis_acceptance_analyst` 与 `qgis_repository_cartographer`。
3. 主代理汇总完整实现方案、测试矩阵、TODO、验收标准和并行任务切分。
4. 并行启动 `qgis_implementation_engineer` 与 `qgis_test_engineer`。
5. 并行启动 `qgis_requirement_implementation_reviewer` 与 `qgis_test_coverage_reviewer`。
6. 并行启动 `qgis_code_style_reviewer` 与 `qgis_code_quality_reviewer`。
7. 审查通过后启动 `qgis_verification_engineer` 执行完整相关测试。
8. 若任务达到 R2，最后启动 `qgis_build_verifier` 执行整体构建验证和日志分析。
9. 主代理输出最终验收总结，逐项对比 AC、测试证据、构建日志和风险边界。

> [!NOTE]
> 1. 写代理只能在主代理明确分配的写范围内工作，不得恢复、覆盖或删除用户已有改动。
> 2. `qgis_implementation_engineer` 不改测试；`qgis_test_engineer` 不改生产逻辑。
> 3. `qgis_verification_engineer` 只执行完整相关测试；R2 整体构建必须交给主代理或 `qgis_build_verifier` 按 `AGENTS.md` 闭环执行。
> 4. `qgis_verification_engineer` 和 `qgis_build_verifier` 必须记录临时产物路径、用途和清理或保留理由。
> 5. 如果 Codex 无法正确识别某个自定义子代理，需要先做最小 smoke test，确认实际加载的是对应 TOML 而不是 default agent。
