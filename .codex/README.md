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
