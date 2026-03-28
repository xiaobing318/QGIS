from __future__ import annotations

import json
from dataclasses import asdict
from pathlib import Path

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config import MCP_LOG_CATEGORY
from processingmcpserver.dependency_models import DependencyCheckResult


def _write_dependency_report(path: Path, result: DependencyCheckResult) -> None:
    """
    作用：封装内部辅助步骤 `_write_dependency_report`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_write_dependency_report`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在依赖检查结果落盘与日志展示流程中被调用，用于输出结构化报告。
    参数与返回：
    - 参数 `path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `result`（`DependencyCheckResult`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(asdict(result), ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
    except Exception as exc:
        QgsMessageLog.logMessage(
            f"Failed to write dependency report {path}: {exc}",
            MCP_LOG_CATEGORY,
            Qgis.Warning,
        )


def _log_result(report_path: Path, result: DependencyCheckResult) -> None:
    """
    作用：封装内部辅助步骤 `_log_result`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_log_result`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在依赖检查结果落盘与日志展示流程中被调用，用于输出结构化报告。
    参数与返回：
    - 参数 `report_path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
    - 参数 `result`（`DependencyCheckResult`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    QgsMessageLog.logMessage(
        f"Processing MCP dependency report: {report_path}",
        MCP_LOG_CATEGORY,
        Qgis.Info,
    )
    QgsMessageLog.logMessage(
        (
            "Processing MCP runtime environment: "
            f"platform={result.platform_system} {result.platform_release}, "
            f"machine={result.platform_machine}, "
            f"python={result.python_executable}"
        ),
        MCP_LOG_CATEGORY,
        Qgis.Info,
    )
    QgsMessageLog.logMessage(
        (
            "Processing MCP pip probe: "
            f"available={result.pip_available}, "
            f"version={result.pip_version or '<unknown>'}, "
            f"python={result.pip_python_executable or '<unknown>'}, "
            f"source={result.pip_python_source or '<unknown>'}, "
            f"error={result.pip_error or '<none>'}"
        ),
        MCP_LOG_CATEGORY,
        Qgis.Info if result.pip_available else Qgis.Warning,
    )
    QgsMessageLog.logMessage(
        (
            "Processing MCP pip probe command: "
            + (" ".join(result.pip_probe_command) if result.pip_probe_command else "<none>")
        ),
        MCP_LOG_CATEGORY,
        Qgis.Info,
    )
    QgsMessageLog.logMessage(
        (
            "Processing MCP pip probe output: "
            f"stdout={_first_line(result.pip_probe_stdout)}, "
            f"stderr={_first_line(result.pip_probe_stderr)}"
        ),
        MCP_LOG_CATEGORY,
        Qgis.Info if result.pip_available else Qgis.Warning,
    )
    QgsMessageLog.logMessage(
        (
            "Processing MCP install target: "
            f"prefix={result.install_target_prefix or '<unknown>'}, "
            f"site_packages={result.install_target_site_packages or '<unknown>'}"
        ),
        MCP_LOG_CATEGORY,
        Qgis.Info,
    )
    QgsMessageLog.logMessage(
        (
            "Processing MCP requirements source: "
            f"path={result.requirements_file_path}, "
            f"read_success={result.requirements_file_read}, "
            f"error={result.requirements_file_error or '<none>'}"
        ),
        MCP_LOG_CATEGORY,
        Qgis.Info if result.requirements_file_read else Qgis.Warning,
    )
    QgsMessageLog.logMessage(
        (
            "Processing MCP runtime contract: "
            f"ok={result.mcp_runtime_contract_ok}, "
            f"error={result.mcp_runtime_contract_error or '<none>'}"
        ),
        MCP_LOG_CATEGORY,
        Qgis.Info if result.mcp_runtime_contract_ok else Qgis.Warning,
    )
    if result.install_attempted:
        QgsMessageLog.logMessage(
            (
                "Processing MCP dependency install command: "
                + " ".join(result.install_command)
            ),
            MCP_LOG_CATEGORY,
            Qgis.Info,
        )
        if result.install_return_code not in (None, 0):
            summary = _summarize_failure_reason(
                result.install_stderr or result.install_stdout
            )
            QgsMessageLog.logMessage(
                (
                    "Processing MCP dependency installation failed: "
                    f"{summary}. Report: {report_path}"
                ),
                MCP_LOG_CATEGORY,
                Qgis.Critical,
            )

    if result.success:
        QgsMessageLog.logMessage(
            "Processing MCP dependency check passed.",
            MCP_LOG_CATEGORY,
            Qgis.Info,
        )
        return

    message = result.failure_reason or result.error or "Dependency check failed."
    QgsMessageLog.logMessage(message, MCP_LOG_CATEGORY, Qgis.Critical)


def _truncate_text(text: str, max_length: int = 20_000) -> str:
    """
    作用：封装内部辅助步骤 `_truncate_text`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_truncate_text`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在依赖检查结果落盘与日志展示流程中被调用，用于输出结构化报告。
    参数与返回：
    - 参数 `text`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `max_length`（`int`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `20_000`。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    if len(text) <= max_length:
        return text
    return text[:max_length] + "\n...[truncated]..."


def _first_line(text: str) -> str:
    """
    作用：封装内部辅助步骤 `_first_line`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_first_line`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在依赖检查结果落盘与日志展示流程中被调用，用于输出结构化报告。
    参数与返回：
    - 参数 `text`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    content = (text or "").strip()
    if not content:
        return "<empty>"
    line = content.splitlines()[0].strip()
    return line or "<empty>"


def _summarize_failure_reason(stderr_text: str) -> str:
    """
    作用：封装内部辅助步骤 `_summarize_failure_reason`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_summarize_failure_reason`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在依赖检查结果落盘与日志展示流程中被调用，用于输出结构化报告。
    参数与返回：
    - 参数 `stderr_text`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    text = (stderr_text or "").strip()
    if not text:
        return "no stderr output"
    first_line = text.splitlines()[0].strip()
    if not first_line:
        return "stderr output is empty after trimming"
    return first_line
