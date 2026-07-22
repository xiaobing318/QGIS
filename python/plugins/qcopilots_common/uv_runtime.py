"""uv-managed runtime helpers for QCopilots service processes.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

from qcopilots_common.constants import UV_EXECUTABLE_ENV, runtime_root
from qcopilots_common.manifest import ServiceManifest
from qcopilots_common.subprocess_utils import hidden_subprocess_kwargs


class UvRuntime:
    def __init__(self, root: str | Path | None = None):
        self.root = Path(root) if root else runtime_root()

    def ensure_runtime(self, manifest: ServiceManifest, python_executable: str | None = None) -> Path:
        if not python_executable:
            raise RuntimeError(
                "QCopilots isolated runtimes require the Python interpreter bundled with the active QGIS package."
            )
        python_path = Path(python_executable)
        uv_command = self._uv_command(python_path)
        if not uv_command:
            raise RuntimeError(
                "uv executable was not found. Set QCOPILOTS_UV or install uv for isolated QCopilots runtimes."
            )

        service_runtime = self.root / manifest.service_id.replace(".", "_")
        venv_path = service_runtime / ".venv"
        venv_python = _venv_python(venv_path)
        service_runtime.mkdir(parents=True, exist_ok=True)

        if not venv_python.exists():
            subprocess.run(
                [*uv_command, "venv", str(venv_path), "--python", str(python_path)],
                check=True,
                timeout=600,
                **hidden_subprocess_kwargs(),
            )

        if manifest.requirements_path.exists():
            subprocess.run(
                [*uv_command, "pip", "sync", "--python", str(venv_python), str(manifest.requirements_path)],
                check=True,
                timeout=600,
                **hidden_subprocess_kwargs(),
            )

        return venv_python

    def service_command(self, manifest: ServiceManifest, python_executable: str | Path) -> list[str]:
        python_path = Path(python_executable)
        uv_command = self._uv_command(python_path)
        if not uv_command:
            raise RuntimeError(
                "uv executable was not found. Set QCOPILOTS_UV or install uv for isolated QCopilots runtimes."
            )

        command = [
            *uv_command,
            "run",
            "--directory",
            str(manifest.plugin_dir),
            "--python",
            str(python_path),
        ]
        if _requirements_has_dependencies(manifest.requirements_path):
            command.extend(["--with-requirements", str(manifest.requirements_path)])
        command.append(str(manifest.entry_path))
        return command

    def _uv_command(self, python_path: Path) -> list[str] | None:
        configured = os.environ.get(UV_EXECUTABLE_ENV)
        if configured:
            configured_path = Path(configured)
            if configured_path.exists():
                return [str(configured_path)]
            return [configured]
        if _looks_like_python(python_path):
            return [str(python_path), "-m", "uv"]
        uv = shutil.which("uv")
        if uv:
            return [uv]
        return None


def _venv_python(venv_path: Path) -> Path:
    if os.name == "nt":
        return venv_path / "Scripts" / "python.exe"
    return venv_path / "bin" / "python"


def _looks_like_python(path: Path) -> bool:
    name = path.name.lower()
    return name in ("python", "python.exe", "pythonw.exe") or name.startswith("python")


def _requirements_has_dependencies(path: Path) -> bool:
    if not path.exists():
        return False
    try:
        for line in path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if stripped and not stripped.startswith("#"):
                return True
    except OSError:
        return False
    return False
