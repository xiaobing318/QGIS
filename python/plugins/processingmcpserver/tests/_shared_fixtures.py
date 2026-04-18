"""Shared runtime fixtures and test doubles for processingmcpserver tests."""

from __future__ import annotations

import json
import os
from pathlib import Path


def _candidate_qgis_bin_dirs() -> list[str]:
    """
    作用：封装内部辅助步骤 `_candidate_qgis_bin_dirs`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_candidate_qgis_bin_dirs`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数：无。
    - 返回：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
    """
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
    """
    作用：封装内部辅助步骤 `_prepend_runtime_paths`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_prepend_runtime_paths`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    """
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
    """
    作用：确保 `_ensure_qgis_test_app` 负责的前置状态可用，必要时执行初始化或修复动作。
    用途：确保 `_ensure_qgis_test_app` 负责的前置状态可用，必要时执行初始化或修复动作。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    if QgsApplication.instance() is None:
        start_app()


_ensure_qgis_test_app()


class DummyMcp:
    """Minimal MCP mock for collecting registrations."""

    def __init__(self) -> None:
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.tool_names: list[str] = []
        self.tool_funcs: dict[str, object] = {}
        self.prompt_names: list[str] = []
        self.resource_uris: list[str] = []
        self.prompt_funcs: dict[str, object] = {}
        self.resource_funcs: dict[str, object] = {}
        self.prompt_descriptions: dict[str, str] = {}
        self.resource_descriptions: dict[str, str] = {}

    def tool(self):
        """
        作用：实现 `tool` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `tool` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        def decorator(func):
            """
            作用：处理 `decorator` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            用途：处理 `decorator` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
            参数与返回：
            - 参数 `func`：业务输入参数，由调用方提供以驱动当前函数逻辑。
            - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            """
            self.tool_names.append(func.__name__)
            self.tool_funcs[func.__name__] = func
            return func

        return decorator

    def prompt(
        self,
        name: str | None = None,
        title: str | None = None,
        description: str | None = None,
        icons=None,
    ):
        """
        作用：实现 `prompt` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `prompt` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `name`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `None`。
        - 参数 `title`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 参数 `description`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 参数 `icons`：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        _ = title, icons

        def decorator(func):
            """
            作用：处理 `decorator` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            用途：处理 `decorator` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
            参数与返回：
            - 参数 `func`：业务输入参数，由调用方提供以驱动当前函数逻辑。
            - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            """
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
        """
        作用：实现 `resource` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `resource` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `uri`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `name`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `None`。
        - 参数 `title`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 参数 `description`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 参数 `mime_type`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 参数 `icons`：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 参数 `annotations`：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 参数 `meta`：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        _ = name, title, mime_type, icons, annotations, meta

        def decorator(func):
            """
            作用：处理 `decorator` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            用途：处理 `decorator` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
            参数与返回：
            - 参数 `func`：业务输入参数，由调用方提供以驱动当前函数逻辑。
            - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
            """
            self.resource_uris.append(uri)
            self.resource_funcs[uri] = func
            if isinstance(description, str):
                self.resource_descriptions[uri] = description
            return func

        return decorator


class DummyTools:
    """Minimal tools stub used for register_* tests."""

    def get_project_snapshot(self):
        """
        作用：实现 `get_project_snapshot` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_project_snapshot` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return {"title": "dummy-project"}

    def get_shapefile_workflow_template(self):
        """
        作用：实现 `get_shapefile_workflow_template` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_shapefile_workflow_template` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
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
        """
        作用：实现 `get_shapefile_quality_profile` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_shapefile_quality_profile` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return {
            "quality_checks": [
                "crs_declared",
                "geometry_valid",
                "duplicates_checked",
                "field_name_length",
            ]
        }

    def get_shapefile_run_summary(self):
        """
        作用：实现 `get_shapefile_run_summary` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `get_shapefile_run_summary` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return {
            "task_name": "dummy-task",
            "status": "initialized",
            "steps": [],
            "warnings": [],
            "outputs": {},
        }

    def layer_list(self):
        """
        作用：实现 `layer_list` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `layer_list` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return [{"id": "layer-1", "name": "L1", "type": "vector_0", "visible": True}]

    def common_get_qgis_info(self):
        """
        作用：实现 `common_get_qgis_info` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `common_get_qgis_info` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return {"qgis_version": "test-version"}

    def _resource_json(self, payload):
        """
        作用：封装内部辅助步骤 `_resource_json`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_resource_json`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `payload`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return json.dumps(payload, ensure_ascii=False, indent=2)

    def __getattr__(self, _name):
        """
        作用：封装内部辅助步骤 `__getattr__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__getattr__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `_name`：标识或模式参数，用于指定目标对象或流程分支。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return lambda *args, **kwargs: {}


class DummyRunner:
    """Main-thread runner stub that executes callback synchronously."""

    def run(self, func):
        """
        作用：实现 `run` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `run` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `func`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return func()


def assert_tool_registered(testcase, tool_name: str) -> None:
    """
    作用：断言 `tool_registered`，完成当前函数负责的处理步骤并产出结果。
    用途：断言 `tool_registered`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数 `testcase`：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `tool_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    from processingmcpserver.mcp_tools import register_tools

    mcp = DummyMcp()
    register_tools(mcp, DummyTools(), enable_execute_code=False)
    testcase.assertIn(tool_name, mcp.tool_names)
