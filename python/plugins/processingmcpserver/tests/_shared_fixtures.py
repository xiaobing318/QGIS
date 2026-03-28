"""Shared runtime fixtures and test doubles for processingmcpserver tests."""

from __future__ import annotations

import json
import os
from pathlib import Path


def _candidate_qgis_bin_dirs() -> list[str]:
    """Collect candidate QGIS bin directories."""
    candidates: list[str] = []

    qgis_prefix = os.environ.get("QGIS_PREFIX_PATH", "").strip()
    if qgis_prefix:
        for replacement in (qgis_prefix, qgis_prefix.replace("$<CONFIG>", "RelWithDebInfo"), qgis_prefix.replace("$<CONFIG>", "Release"), qgis_prefix.replace("$<CONFIG>", "Debug")):
            if replacement:
                candidates.append(replacement)

    for py_path in os.environ.get("PYTHONPATH", "").split(os.pathsep):
        path = Path(py_path)
        if path.name.lower() != "python":
            continue
        base = path.parent
        candidates.append(str(base / "bin"))
        for config in ("RelWithDebInfo", "Release", "Debug"):
            candidates.append(str(base / "bin" / config))

    osgeo_root = os.environ.get("OSGEO4W_ROOT", "").strip()
    if osgeo_root:
        candidates.append(str(Path(osgeo_root) / "bin"))
        candidates.append(str(Path(osgeo_root) / "apps" / "Qt6" / "bin"))

    return candidates


def _prepend_runtime_paths() -> None:
    """Prepend discovered runtime paths to PATH."""
    original = os.environ.get("PATH", "")
    existing = original.split(os.pathsep) if original else []
    prepended: list[str] = []

    for candidate in _candidate_qgis_bin_dirs():
        if not candidate:
            continue
        candidate_path = Path(candidate)
        if not candidate_path.exists() or not candidate_path.is_dir():
            continue
        candidate_text = str(candidate_path)
        if candidate_text in prepended or candidate_text in existing:
            continue
        prepended.append(candidate_text)

    if prepended:
        os.environ["PATH"] = os.pathsep.join(prepended + existing)


_prepend_runtime_paths()

from qgis.core import QgsApplication
from qgis.testing import start_app


def _ensure_qgis_test_app() -> None:
    """Initialize QgsApplication only when needed."""
    if QgsApplication.instance() is None:
        start_app()


_ensure_qgis_test_app()


class DummyMcp:
    """Minimal MCP mock for collecting registrations."""

    def __init__(self) -> None:
        """Initialize the DummyMcp instance state."""
        self.tool_names: list[str] = []
        self.prompt_names: list[str] = []
        self.resource_uris: list[str] = []
        self.prompt_funcs: dict[str, object] = {}
        self.resource_funcs: dict[str, object] = {}
        self.prompt_descriptions: dict[str, str] = {}
        self.resource_descriptions: dict[str, str] = {}

    def tool(self):
        """Return the tool registration decorator."""
        def decorator(func):
            """Return the decorator used by the registration flow."""
            self.tool_names.append(func.__name__)
            return func

        return decorator

    def prompt(
        self,
        name: str | None = None,
        title: str | None = None,
        description: str | None = None,
        icons=None,
    ):
        """Return the prompt registration decorator."""
        _ = title, icons

        def decorator(func):
            """Return the decorator used by the registration flow."""
            prompt_name = name or func.__name__
            self.prompt_names.append(prompt_name)
            self.prompt_funcs[prompt_name] = func
            if isinstance(description, str):
                self.prompt_descriptions[prompt_name] = description
            return func

        return decorator

    def resource(
        self,
        uri: str,
        *,
        name: str | None = None,
        title: str | None = None,
        description: str | None = None,
        mime_type: str | None = None,
        icons=None,
        annotations=None,
        meta=None,
    ):
        """Return the resource registration decorator."""
        _ = name, title, mime_type, icons, annotations, meta

        def decorator(func):
            """Return the decorator used by the registration flow."""
            self.resource_uris.append(uri)
            self.resource_funcs[uri] = func
            if isinstance(description, str):
                self.resource_descriptions[uri] = description
            return func

        return decorator


class DummyTools:
    """Minimal tools stub used for register_* tests."""

    def get_project_snapshot(self):
        """Return a project snapshot."""
        return {"title": "dummy-project"}

    def get_shapefile_workflow_template(self):
        """Return the shapefile workflow template."""
        return {
            "workflow_stages": [
                "path_check_and_geometry_filter",
                "precheck_and_pre_statistics",
                "standardize_crs",
                "data_processing",
                "export",
                "cleanup",
            ],
            "required_inputs": [
                "task_name",
                "input_dir",
                "output_dir",
                "quality_rule_resource",
                "deliverables",
            ],
            "defaults": {
                "default_target_crs": "EPSG:4490",
                "default_geometry_filter": "all",
            },
        }

    def get_shapefile_quality_profile(self):
        """Return the shapefile quality profile."""
        return {
            "quality_checks": [
                "crs_declared",
                "geometry_valid",
                "duplicates_checked",
                "field_name_length",
            ]
        }

    def get_shapefile_run_summary(self):
        """Return the shapefile run summary."""
        return {
            "task_name": "dummy-task",
            "status": "initialized",
            "steps": [],
            "warnings": [],
            "outputs": {},
        }

    def layer_list(self):
        """Return the layer list."""
        return [{"id": "layer-1", "name": "L1", "type": "vector_0", "visible": True}]

    def common_get_qgis_info(self):
        """Return QGIS info metadata."""
        return {"qgis_version": "test-version"}

    def _resource_json(self, payload):
        """Return the resource payload as formatted JSON."""
        return json.dumps(payload, ensure_ascii=False, indent=2)

    def __getattr__(self, _name):
        """Fallback to an empty callable for missing attributes."""
        return lambda *args, **kwargs: {}


class DummyRunner:
    """Main-thread runner stub that executes callback synchronously."""

    def run(self, func):
        """Execute the callback and return its result."""
        return func()


def assert_tool_registered(testcase, tool_name: str) -> None:
    """Assert specific tool is registered by register_tools."""
    from processingmcpserver.mcp_tools import register_tools

    mcp = DummyMcp()
    register_tools(mcp, DummyTools(), enable_execute_code=False)
    testcase.assertIn(tool_name, mcp.tool_names)
