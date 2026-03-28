from __future__ import annotations

from datetime import datetime, timezone
import subprocess

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config import (
    MCP_LOG_CATEGORY,
    ProcessingMCPServerDependencies,
)
from processingmcpserver.dependency_contract import _validate_mcp_runtime_contract
from processingmcpserver.dependency_evaluator import (
    _collect_versions_from_evaluations,
    _evaluate_requirement,
    _evaluate_requirements,
    _extract_unsatisfied,
    _parse_requirement,
)
from processingmcpserver.dependency_models import (
    DependencyCheckResult,
    PipDetectionResult,
    PythonEnvironmentSnapshot,
    RequirementEvaluation,
    RequirementsFileLoadResult,
)
from processingmcpserver.dependency_probe import (
    _collect_python_environment,
    _detect_pip,
    _load_requirements_from_file,
    _resolve_install_target_details,
    _resolve_pip_python_executable,
    processing_mcp_dependency_report_file_path,
    processing_mcp_requirements_file_path,
)
from processingmcpserver.dependency_reporting import (
    _log_result as _log_result_impl,
    _summarize_failure_reason,
    _truncate_text,
    _write_dependency_report as _write_dependency_report_impl,
)


def _write_dependency_report(result: DependencyCheckResult) -> None:
    """Write the dependency report JSON file."""
    report_path = processing_mcp_dependency_report_file_path()
    _write_dependency_report_impl(report_path, result)


def _log_result(result: DependencyCheckResult) -> None:
    """Log the dependency check summary and key runtime context."""
    report_path = processing_mcp_dependency_report_file_path()
    _log_result_impl(report_path, result)


def ensure_processing_mcp_dependencies(
    config: ProcessingMCPServerDependencies,
) -> DependencyCheckResult:
    """Run dependency checks, optionally auto-install missing packages, and write the report."""
    environment = _collect_python_environment()
    (
        pip_python_executable,
        pip_python_source,
        pip_python_resolve_error,
    ) = _resolve_pip_python_executable(environment)
    (
        install_target_prefix,
        install_target_site_packages,
    ) = _resolve_install_target_details(pip_python_executable, environment)
    pip_result = _detect_pip(
        pip_python_executable, pip_python_source, pip_python_resolve_error
    )
    requirements_result = _load_requirements_from_file(
        processing_mcp_requirements_file_path()
    )
    requirements = requirements_result.requirements
    auto_install = bool(config.auto_install)

    fatal_requirements_error = ""
    if not requirements_result.read_success:
        fatal_requirements_error = (
            "Failed to read Processing MCP requirements file: "
            f"{requirements_result.error or 'unknown reason'}"
        )
    elif not requirements:
        fatal_requirements_error = (
            "Processing MCP requirements file does not contain any requirement entries."
        )

    evaluations_before = _evaluate_requirements(requirements) if requirements else {}
    unsatisfied_before, reasons_before = _extract_unsatisfied(evaluations_before)
    versions_before = _collect_versions_from_evaluations(evaluations_before)

    install_attempted = False
    install_command: list[str] = []
    install_return_code: int | None = None
    install_stdout = ""
    install_stderr = ""
    error = ""

    if fatal_requirements_error:
        error = fatal_requirements_error
    elif unsatisfied_before and auto_install:
        if not pip_result.available:
            error = (
                "Automatic dependency installation skipped because pip is unavailable: "
                + (pip_result.error or "unknown reason")
            )
        else:
            install_attempted = True
            install_command = [
                pip_result.python_executable,
                "-m",
                "pip",
                "install",
                "--disable-pip-version-check",
                "--no-input",
                *unsatisfied_before,
            ]
            QgsMessageLog.logMessage(
                "Missing Processing MCP dependencies detected. Attempting automatic "
                f"installation: {', '.join(unsatisfied_before)}",
                MCP_LOG_CATEGORY,
                Qgis.Warning,
            )

            try:
                completed = subprocess.run(
                    install_command,
                    check=False,
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                )
                install_return_code = completed.returncode
                install_stdout = _truncate_text(completed.stdout)
                install_stderr = _truncate_text(completed.stderr)
                if completed.returncode != 0:
                    reason = _summarize_failure_reason(install_stderr or install_stdout)
                    error = (
                        "Automatic dependency installation failed with exit code "
                        f"{completed.returncode}: {reason}"
                    )
            except Exception as exc:
                error = f"Automatic dependency installation crashed: {exc}"

    evaluations_after = _evaluate_requirements(requirements) if requirements else {}
    unsatisfied_after, reasons_after = _extract_unsatisfied(evaluations_after)
    versions_after = _collect_versions_from_evaluations(evaluations_after)

    if unsatisfied_after and not error:
        if auto_install:
            if install_attempted:
                error = (
                    "Some requirements are still unsatisfied after installation: "
                    + ", ".join(unsatisfied_after)
                )
            else:
                error = (
                    "Missing requirements detected, but installation was not attempted."
                )
        else:
            error = (
                "Missing dependencies detected and auto installation is disabled: "
                + ", ".join(unsatisfied_after)
            )

    mcp_runtime_contract_ok = False
    mcp_runtime_contract_error = ""
    if not error:
        (
            mcp_runtime_contract_ok,
            mcp_runtime_contract_error,
        ) = _validate_mcp_runtime_contract()
        if not mcp_runtime_contract_ok:
            error = (
                "MCP runtime contract validation failed: "
                f"{mcp_runtime_contract_error}"
            )

    result = DependencyCheckResult(
        report_schema_version=3,
        timestamp_utc=datetime.now(timezone.utc).isoformat(),
        platform_system=environment.platform_system,
        platform_release=environment.platform_release,
        platform_machine=environment.platform_machine,
        python_executable=environment.python_executable,
        python_version=environment.python_version,
        python_prefix=environment.python_prefix,
        python_base_prefix=environment.python_base_prefix,
        python_exec_prefix=environment.python_exec_prefix,
        is_virtual_environment=environment.is_virtual_environment,
        site_packages=environment.site_packages,
        user_site_packages=environment.user_site_packages,
        python_environment_error=environment.environment_error,
        pip_available=pip_result.available,
        pip_version=pip_result.version,
        pip_python_executable=pip_result.python_executable,
        pip_python_source=pip_result.python_source,
        pip_probe_command=pip_result.command,
        pip_probe_stdout=pip_result.stdout,
        pip_probe_stderr=pip_result.stderr,
        pip_error=pip_result.error,
        requirements_file_path=requirements_result.path,
        requirements_file_read=requirements_result.read_success,
        requirements_file_error=requirements_result.error,
        auto_install=auto_install,
        install_target_prefix=install_target_prefix,
        install_target_site_packages=install_target_site_packages,
        requested_requirements=requirements,
        installed_versions_before=versions_before,
        unsatisfied_before=unsatisfied_before,
        unsatisfied_reasons_before=reasons_before,
        install_attempted=install_attempted,
        install_command=install_command,
        install_return_code=install_return_code,
        install_stdout=install_stdout,
        install_stderr=install_stderr,
        installed_versions_after=versions_after,
        unsatisfied_after=unsatisfied_after,
        unsatisfied_reasons_after=reasons_after,
        mcp_runtime_contract_ok=mcp_runtime_contract_ok,
        mcp_runtime_contract_error=mcp_runtime_contract_error,
        success=not unsatisfied_after and not error,
        failure_reason=error,
        error=error,
    )
    _write_dependency_report(result)
    _log_result(result)
    return result


__all__ = [
    "PythonEnvironmentSnapshot",
    "PipDetectionResult",
    "RequirementEvaluation",
    "RequirementsFileLoadResult",
    "DependencyCheckResult",
    "processing_mcp_dependency_report_file_path",
    "processing_mcp_requirements_file_path",
    "ensure_processing_mcp_dependencies",
    "_parse_requirement",
    "_evaluate_requirement",
    "_load_requirements_from_file",
    "_detect_pip",
    "_write_dependency_report",
    "_log_result",
    "_validate_mcp_runtime_contract",
]
