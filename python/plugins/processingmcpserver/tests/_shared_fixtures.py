from __future__ import annotations

import json
import os
from pathlib import Path


def _candidate_qgis_bin_dirs() -> list[str]:
    """执行 candidate QGIS bin dirs 相关逻辑。"""
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
    """执行 prepend runtime paths 相关逻辑。"""
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
        """初始化 DummyMcp 实例状态。"""
        self.tool_names: list[str] = []
        self.prompt_names: list[str] = []
        self.resource_uris: list[str] = []
        self.prompt_funcs: dict[str, object] = {}
        self.resource_funcs: dict[str, object] = {}
        self.prompt_descriptions: dict[str, str] = {}
        self.resource_descriptions: dict[str, str] = {}

    def tool(self):
        """执行 tool 相关逻辑。"""
        def decorator(func):
            """返回当前注册流程使用的装饰器。"""
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
        """执行 prompt 相关逻辑。"""
        _ = title, icons

        def decorator(func):
            """返回当前注册流程使用的装饰器。"""
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
        """执行 resource 相关逻辑。"""
        _ = name, title, mime_type, icons, annotations, meta

        def decorator(func):
            """返回当前注册流程使用的装饰器。"""
            self.resource_uris.append(uri)
            self.resource_funcs[uri] = func
            if isinstance(description, str):
                self.resource_descriptions[uri] = description
            return func

        return decorator


class DummyTools:
    """Minimal tools stub used for register_* tests."""

    def get_project_snapshot(self):
        """返回 project snapshot。"""
        return {"title": "dummy-project"}

    def get_shapefile_workflow_template(self):
        """返回 shapefile workflow template。"""
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
        """返回 shapefile quality profile。"""
        return {
            "quality_checks": [
                "crs_declared",
                "geometry_valid",
                "duplicates_checked",
                "field_name_length",
            ]
        }

    def get_shapefile_run_summary(self):
        """返回 shapefile run summary。"""
        return {
            "task_name": "dummy-task",
            "status": "initialized",
            "steps": [],
            "warnings": [],
            "outputs": {},
        }

    def layer_list(self):
        """执行图层相关的 list 逻辑。"""
        return [{"id": "layer-1", "name": "L1", "type": "vector_0", "visible": True}]

    def common_get_qgis_info(self):
        """返回 QGIS info 信息。"""
        return {"qgis_version": "test-version"}

    def _resource_json(self, payload):
        """执行 resource JSON 相关逻辑。"""
        return json.dumps(payload, ensure_ascii=False, indent=2)

    def __getattr__(self, _name):
        """执行 getattr 相关逻辑。"""
        return lambda *args, **kwargs: {}


class DummyRunner:
    """Main-thread runner stub that executes callback synchronously."""

    def run(self, func):
        """执行当前核心逻辑并返回结果。"""
        return func()


def assert_tool_registered(testcase, tool_name: str) -> None:
    """Assert specific tool is registered by register_tools."""
    from processingmcpserver.mcp_tools import register_tools

    mcp = DummyMcp()
    register_tools(mcp, DummyTools(), enable_execute_code=False)
    testcase.assertIn(tool_name, mcp.tool_names)
