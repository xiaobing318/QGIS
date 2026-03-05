from __future__ import annotations

import json
import platform
from pathlib import Path
import site
import subprocess
import sys

from qgis.core import QgsApplication

from processingmcpserver.dependency_models import (
    PipDetectionResult,
    PythonEnvironmentSnapshot,
    RequirementsFileLoadResult,
)
from processingmcpserver.dependency_reporting import (
    _summarize_failure_reason,
    _truncate_text,
)


def processing_mcp_dependency_report_file_path() -> Path:
    """返回依赖检测报告输出路径。"""
    return (
        Path(QgsApplication.qgisSettingsDirPath())
        / "processingmcpserver"
        / "dependency-report.json"
    )


def processing_mcp_requirements_file_path() -> Path:
    """返回插件 requirements.txt 文件路径。"""
    return Path(__file__).resolve().parent / "requirements.txt"


def _load_requirements_from_file(path: Path) -> RequirementsFileLoadResult:
    """从 requirements.txt 读取依赖清单，跳过空行与注释。"""
    normalized: list[str] = []
    seen: set[str] = set()

    if not path.exists():
        return RequirementsFileLoadResult(
            path=str(path),
            read_success=False,
            requirements=[],
            error=f"file not found: {path}",
        )

    try:
        content = path.read_text(encoding="utf-8")
    except Exception as exc:
        return RequirementsFileLoadResult(
            path=str(path),
            read_success=False,
            requirements=[],
            error=str(exc),
        )

    for raw_line in content.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("#"):
            continue
        if "#" in line:
            line = line.split("#", 1)[0].strip()
        if not line:
            continue

        key = line.lower()
        if key in seen:
            continue
        seen.add(key)
        normalized.append(line)

    return RequirementsFileLoadResult(
        path=str(path),
        read_success=True,
        requirements=normalized,
        error="",
    )


def _collect_python_environment() -> PythonEnvironmentSnapshot:
    """采集当前解释器与平台信息，异常时写入环境错误字段。"""
    environment_errors: list[str] = []

    site_packages: list[str] = []
    try:
        for item in site.getsitepackages():
            text = str(item).strip()
            if text:
                site_packages.append(text)
    except Exception as exc:
        environment_errors.append(f"getsitepackages failed: {exc}")

    user_site_packages = ""
    try:
        user_site_packages = str(site.getusersitepackages()).strip()
    except Exception as exc:
        environment_errors.append(f"getusersitepackages failed: {exc}")

    python_executable = str(Path(sys.executable).resolve())
    python_prefix = str(Path(sys.prefix).resolve())
    python_base_prefix = str(Path(getattr(sys, "base_prefix", sys.prefix)).resolve())
    python_exec_prefix = str(Path(sys.exec_prefix).resolve())

    return PythonEnvironmentSnapshot(
        platform_system=platform.system(),
        platform_release=platform.release(),
        platform_machine=platform.machine(),
        python_executable=python_executable,
        python_version=sys.version,
        python_prefix=python_prefix,
        python_base_prefix=python_base_prefix,
        python_exec_prefix=python_exec_prefix,
        is_virtual_environment=python_prefix != python_base_prefix,
        site_packages=site_packages,
        user_site_packages=user_site_packages,
        environment_error="; ".join(environment_errors),
    )


def _resolve_pip_python_executable(
    environment: PythonEnvironmentSnapshot,
) -> tuple[str, str, str]:
    """解析用于 pip 的 Python 可执行文件路径。"""
    candidates: list[tuple[str, Path]] = []

    try:
        current = Path(sys.executable).resolve()
    except Exception:
        current = Path(sys.executable)

    if current.name.lower().startswith("python"):
        candidates.append(("sys.executable", current))

    prefix_candidates = (
        ("sys.prefix", environment.python_prefix),
        ("sys.exec_prefix", environment.python_exec_prefix),
        ("sys.base_prefix", environment.python_base_prefix),
    )
    for source, prefix in prefix_candidates:
        prefix_path = Path(prefix)
        for filename in ("python.exe", "python"):
            candidates.append((source, prefix_path / filename))

    seen: set[str] = set()
    checked_paths: list[str] = []
    for source, candidate in candidates:
        try:
            resolved = candidate.resolve()
        except Exception:
            resolved = candidate
        normalized = str(resolved).lower()
        if normalized in seen:
            continue
        seen.add(normalized)
        checked_paths.append(str(resolved))
        if resolved.exists() and resolved.is_file():
            return str(resolved), source, ""

    checked_text = ", ".join(checked_paths) if checked_paths else "<none>"
    return "", "", f"No usable python executable found. Checked: {checked_text}"


def _resolve_install_target_details(
    python_executable: str,
    environment: PythonEnvironmentSnapshot,
) -> tuple[str, str]:
    """解析依赖安装目标 prefix 和 site-packages 路径。"""
    fallback_prefix = environment.python_prefix
    fallback_site_packages = _default_site_packages_from_prefix(fallback_prefix)
    if environment.site_packages:
        fallback_site_packages = environment.site_packages[-1]

    if not python_executable:
        return fallback_prefix, fallback_site_packages

    command = [
        python_executable,
        "-c",
        (
            "import json, site, sys; "
            "payload={'prefix': sys.prefix, 'site_packages': []}; "
            "sp = site.getsitepackages(); "
            "payload['site_packages'] = sp if isinstance(sp, list) else [str(sp)]; "
            "print(json.dumps(payload))"
        ),
    ]
    try:
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except Exception:
        return fallback_prefix, fallback_site_packages

    if completed.returncode != 0:
        return fallback_prefix, fallback_site_packages

    try:
        payload = json.loads((completed.stdout or "").strip())
    except Exception:
        return fallback_prefix, fallback_site_packages

    prefix = str(payload.get("prefix") or "").strip() or fallback_prefix
    site_packages = payload.get("site_packages")
    resolved_site_packages = fallback_site_packages
    if isinstance(site_packages, list):
        normalized = [str(item).strip() for item in site_packages if str(item).strip()]
        if normalized:
            resolved_site_packages = normalized[-1]
    return prefix, resolved_site_packages


def _default_site_packages_from_prefix(prefix: str) -> str:
    """根据 prefix 推导 site-packages 目录。"""
    path = Path(prefix)
    if platform.system().lower() == "windows":
        return str(path / "Lib" / "site-packages")
    version_tag = f"python{sys.version_info.major}.{sys.version_info.minor}"
    return str(path / "lib" / version_tag / "site-packages")


def _detect_pip(
    python_executable: str, python_source: str, resolve_error: str
) -> PipDetectionResult:
    """探测指定解释器下 pip 是否可用并返回版本信息。"""
    if not python_executable:
        return PipDetectionResult(
            available=False,
            version="",
            python_executable="",
            python_source=python_source,
            command=[],
            stdout="",
            stderr="",
            error=(
                "pip probe skipped because python executable is unresolved: "
                + (resolve_error or "unknown reason")
            ),
        )

    command = [python_executable, "-m", "pip", "--version"]
    try:
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except Exception as exc:
        return PipDetectionResult(
            available=False,
            version="",
            python_executable=python_executable,
            python_source=python_source,
            command=command,
            stdout="",
            stderr="",
            error=f"pip probe crashed: {exc}",
        )

    stdout = _truncate_text(completed.stdout)
    stderr = _truncate_text(completed.stderr)
    if completed.returncode == 0:
        output = (completed.stdout or completed.stderr or "").strip()
        version = output.splitlines()[0].strip() if output else ""
        if "pip " not in output.lower():
            return PipDetectionResult(
                available=False,
                version="",
                python_executable=python_executable,
                python_source=python_source,
                command=command,
                stdout=stdout,
                stderr=stderr,
                error=(
                    "pip probe returned success but output does not contain "
                    f"'pip ': {version or '<empty>'}"
                ),
            )
        return PipDetectionResult(
            available=True,
            version=version,
            python_executable=python_executable,
            python_source=python_source,
            command=command,
            stdout=stdout,
            stderr=stderr,
            error="",
        )

    reason = _summarize_failure_reason(completed.stderr or completed.stdout)
    return PipDetectionResult(
        available=False,
        version="",
        python_executable=python_executable,
        python_source=python_source,
        command=command,
        stdout=stdout,
        stderr=stderr,
        error=f"pip probe failed with exit code {completed.returncode}: {reason}",
    )
