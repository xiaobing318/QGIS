from __future__ import annotations

import inspect
import json
import re

from processingmcpserver.mcp_prompts import (
    REGISTERED_PROMPT_NAMES,
    _REGISTERED_PROMPT_DOCSTRINGS,
    register_prompts,
)
from processingmcpserver.mcp_resources import (
    REGISTERED_RESOURCE_URIS,
    _REGISTERED_RESOURCE_DOCSTRINGS,
    register_resources,
)
from processingmcpserver.mcp_tools import (
    REGISTERED_TOOL_NAMES,
    ProcessingMCPTools,
    register_tools,
)

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class RegistrationsIntegrityTest(ProcessingMCPTestBase):
    def test_register_tools_matches_expected_set(self):
        """
        作用：执行测试用例 `register tools matches expected set`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `register tools matches expected set`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        removed_render_tool = "render" + "_map"
        legacy_removed = {
            "ping",
            "load_project",
            "create_new_project",
            "get_project_info",
            "zoom_to_layer",
            "save_project",
            "execute_processing",
            "execute_code",
            removed_render_tool,
            "vector_add_layers",
            "raster_add_layers",
            "dataset_load_from_directory",
            "layer_get_panel_tree",
            "processing_execute_on_layers",
        }

        mcp = DummyMcp()
        register_tools(mcp, DummyTools(), enable_execute_code=False)
        self.assertEqual(set(mcp.tool_names), set(REGISTERED_TOOL_NAMES))
        self.assertTrue(legacy_removed.isdisjoint(set(mcp.tool_names)))

    def test_registered_tool_signatures_have_no_private_parameter_names(self):
        mcp = DummyMcp()
        register_tools(mcp, DummyTools(), enable_execute_code=False)
        self.assertEqual(set(mcp.tool_names), set(REGISTERED_TOOL_NAMES))

        for tool_name, func in mcp.tool_funcs.items():
            signature = inspect.signature(func)
            for parameter in signature.parameters.values():
                self.assertFalse(
                    parameter.name.startswith("_"),
                    msg=f"{tool_name} has private parameter name: {parameter.name}",
                )
                self.assertNotIn(
                    parameter.name,
                    {"args", "kwargs"},
                    msg=f"{tool_name} leaked variadic placeholder parameter: {parameter.name}",
                )
                self.assertNotIn(
                    parameter.kind,
                    {
                        inspect.Parameter.VAR_POSITIONAL,
                        inspect.Parameter.VAR_KEYWORD,
                    },
                    msg=(
                        f"{tool_name} uses variadic signature kind: "
                        f"{parameter.kind!r}"
                    ),
                )

    def test_registered_tool_names_follow_lowercase_pattern(self):
        pattern = re.compile(r"^mcp_tools_[a-z0-9]+_[a-z0-9_]+$")
        for tool_name in REGISTERED_TOOL_NAMES:
            self.assertRegex(tool_name, pattern)

    def test_register_prompts_and_resources_discoverable(self):
        """
        作用：执行测试用例 `register prompts and resources discoverable`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `register prompts and resources discoverable`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        tools = DummyTools()
        register_prompts(mcp, tools)
        register_resources(mcp, tools)

        self.assertEqual(set(mcp.prompt_names), set(REGISTERED_PROMPT_NAMES))
        self.assertEqual(set(mcp.resource_uris), set(REGISTERED_RESOURCE_URIS))
        self.assertEqual(set(mcp.prompt_descriptions), set(REGISTERED_PROMPT_NAMES))
        self.assertEqual(set(mcp.resource_descriptions), set(REGISTERED_RESOURCE_URIS))
        self.assertNotIn("qgis_shapefile_pipeline_planner", mcp.prompt_names)

    def test_registered_tools_have_non_placeholder_docstrings(self):
        """
        作用：执行测试用例 `registered tools have non placeholder docstrings`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `registered tools have non placeholder docstrings`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        for tool_name in REGISTERED_TOOL_NAMES:
            method = getattr(ProcessingMCPTools, tool_name, None)
            self.assertTrue(callable(method), msg=f"Missing tool method: {tool_name}")
            doc = inspect.getdoc(method)
            self.assertTrue(doc, msg=f"Empty docstring for tool: {tool_name}")
            self.assertIn("用途：", doc, msg=f"Missing purpose section: {tool_name}")
            self.assertIn("返回结果：", doc, msg=f"Missing returns section: {tool_name}")
            self.assertNotEqual(
                doc,
                f"MCP tool wrapper for {tool_name}.",
                msg=f"Placeholder docstring leaked into tool: {tool_name}",
            )

    def test_registered_prompts_have_non_empty_docstrings(self):
        """
        作用：执行测试用例 `registered prompts have non empty docstrings`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `registered prompts have non empty docstrings`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.assertEqual(set(_REGISTERED_PROMPT_DOCSTRINGS), set(REGISTERED_PROMPT_NAMES))

        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        for prompt_name in REGISTERED_PROMPT_NAMES:
            description = mcp.prompt_descriptions.get(prompt_name, "")
            self.assertTrue(description, msg=f"Empty prompt description: {prompt_name}")
            self.assertIn("用途：", description, msg=f"Missing purpose section: {prompt_name}")
            self.assertEqual(
                inspect.getdoc(mcp.prompt_funcs[prompt_name]),
                description,
                msg=f"Prompt docstring mismatch: {prompt_name}",
            )

    def test_registered_resources_have_non_empty_docstrings(self):
        """
        作用：执行测试用例 `registered resources have non empty docstrings`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `registered resources have non empty docstrings`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.assertEqual(set(_REGISTERED_RESOURCE_DOCSTRINGS), set(REGISTERED_RESOURCE_URIS))

        mcp = DummyMcp()
        register_resources(mcp, DummyTools())

        for resource_uri in REGISTERED_RESOURCE_URIS:
            description = mcp.resource_descriptions.get(resource_uri, "")
            self.assertTrue(description, msg=f"Empty resource description: {resource_uri}")
            self.assertIn("用途：", description, msg=f"Missing purpose section: {resource_uri}")
            self.assertEqual(
                inspect.getdoc(mcp.resource_funcs[resource_uri]),
                description,
                msg=f"Resource docstring mismatch: {resource_uri}",
            )

    def test_prompt_templates_exclude_removed_tool_names(self):
        """
        作用：执行测试用例 `prompt templates exclude removed tool names`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompt templates exclude removed tool names`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        removed_render_tool = "render" + "_map"
        legacy_removed = [
            "ping",
            "load_project",
            "create_new_project",
            "get_project_info",
            "save_project",
            "zoom_to_layer",
            "execute_processing(",
            "execute_code",
            removed_render_tool,
            "qgis_shapefile_quality_gate",
            "qgis_shapefile_export_review",
        ]
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())
        prompt_call_inputs = {
            "qgis_shapefile_quality_repair_export_workflow": {
                "task_name": "quality-check",
                "input_dir": "C:/input",
                "output_dir": "C:/output",
            },
            "qgis_shapefile_overlay_clip_stats_workflow": {
                "task_name": "overlay-check",
                "subject_shapefile": "C:/input/subject.shp",
                "overlay_shapefile": "C:/input/overlay.shp",
                "output_shapefile": "C:/output/overlay_out.shp",
            },
            "qgis_shapefile_buffer_join_workflow": {
                "task_name": "buffer-check",
                "line_shapefile": "C:/input/line.shp",
                "point_shapefile": "C:/input/point.shp",
                "output_shapefile": "C:/output/buffer_out.shp",
                "buffer_distance": 120.0,
            },
        }
        for prompt_name, prompt_fn in mcp.prompt_funcs.items():
            output = prompt_fn(**prompt_call_inputs[prompt_name])
            for removed in legacy_removed:
                self.assertNotIn(
                    removed,
                    output,
                    msg=f"{prompt_name} still contains removed tool name: {removed}",
                )

    def test_resource_envelope_for_kept_resources(self):
        """
        作用：执行测试用例 `resource envelope for kept resources`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `resource envelope for kept resources`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        tools = DummyTools()
        register_resources(mcp, tools)

        for resource_uri in REGISTERED_RESOURCE_URIS:
            payload = json.loads(mcp.resource_funcs[resource_uri]())
            self.assertTrue(payload["ok"])
            self.assertEqual(payload["uri"], resource_uri)
            self.assertIn("generated_at", payload)
            self.assertIn("project_id", payload)
            self.assertEqual(payload["schema_version"], "1.0.0")
            self.assertIn("data", payload)
