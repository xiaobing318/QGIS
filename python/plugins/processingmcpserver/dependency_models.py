from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class PythonEnvironmentSnapshot:
    """记录当前解释器与平台环境快照。"""

    platform_system: str
    platform_release: str
    platform_machine: str
    python_executable: str
    python_version: str
    python_prefix: str
    python_base_prefix: str
    python_exec_prefix: str
    is_virtual_environment: bool
    site_packages: list[str]
    user_site_packages: str
    environment_error: str = ""


@dataclass(frozen=True)
class PipDetectionResult:
    """记录 pip 可用性探测结果。"""

    available: bool
    version: str
    python_executable: str = ""
    python_source: str = ""
    command: list[str] = field(default_factory=list)
    stdout: str = ""
    stderr: str = ""
    error: str = ""


@dataclass(frozen=True)
class RequirementEvaluation:
    """记录单个 requirement 的满足情况。"""

    requirement: str
    distribution_name: str
    installed_version: str
    satisfied: bool
    reason: str = ""


@dataclass(frozen=True)
class RequirementsFileLoadResult:
    """记录 requirements.txt 文件读取结果。"""

    path: str
    read_success: bool
    requirements: list[str]
    error: str = ""


@dataclass(frozen=True)
class DependencyCheckResult:
    """记录依赖检测、自动安装与报告输出的完整结果。"""

    report_schema_version: int
    timestamp_utc: str
    platform_system: str
    platform_release: str
    platform_machine: str
    python_executable: str
    python_version: str
    python_prefix: str
    python_base_prefix: str
    python_exec_prefix: str
    is_virtual_environment: bool
    site_packages: list[str]
    user_site_packages: str
    python_environment_error: str
    pip_available: bool
    pip_version: str
    pip_python_executable: str
    pip_python_source: str
    pip_probe_command: list[str]
    pip_probe_stdout: str
    pip_probe_stderr: str
    pip_error: str
    requirements_file_path: str
    requirements_file_read: bool
    requirements_file_error: str
    auto_install: bool
    install_target_prefix: str
    install_target_site_packages: str
    requested_requirements: list[str]
    installed_versions_before: dict[str, str]
    unsatisfied_before: list[str]
    unsatisfied_reasons_before: dict[str, str]
    install_attempted: bool
    install_command: list[str]
    install_return_code: int | None
    install_stdout: str
    install_stderr: str
    installed_versions_after: dict[str, str]
    unsatisfied_after: list[str]
    unsatisfied_reasons_after: dict[str, str]
    mcp_runtime_contract_ok: bool
    mcp_runtime_contract_error: str
    success: bool
    failure_reason: str = ""
    error: str = ""
