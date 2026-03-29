from __future__ import annotations

import inspect

from processingmcpserver.mcp_prompts import register_prompts

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class PromptQgisShapefilePipelinePlannerTest(ProcessingMCPTestBase):
    def test_prompt_registered(self):
        """
        作用：执行测试用例 `prompt registered`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompt registered`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())
        self.assertIn("qgis_shapefile_pipeline_planner", mcp.prompt_names)

    def test_prompt_signature_matches_expected_arguments(self):
        """
        作用：执行测试用例 `prompt signature matches expected arguments`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompt signature matches expected arguments`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        prompt_fn = mcp.prompt_funcs["qgis_shapefile_pipeline_planner"]
        signature = inspect.signature(prompt_fn)

        expected_param_names = [
            "task_name",
            "input_dir",
            "output_dir",
            "quality_rule_resource",
            "deliverables",
        ]
        self.assertEqual(list(signature.parameters.keys()), expected_param_names)
        self.assertEqual(signature.parameters["task_name"].default, inspect.Signature.empty)
        self.assertEqual(signature.parameters["input_dir"].default, inspect.Signature.empty)
        self.assertEqual(signature.parameters["output_dir"].default, inspect.Signature.empty)

    def test_prompt_template_contains_six_stages(self):
        """
        作用：执行测试用例 `prompt template contains six stages`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompt template contains six stages`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        prompt_fn = mcp.prompt_funcs["qgis_shapefile_pipeline_planner"]
        output = prompt_fn(
            task_name="roads-cleanup",
            input_dir="C:/input",
            output_dir="C:/output",
            deliverables="final shapefile bundle",
        )

        self.assertIn("1) 路径检查与数据范围确认阶段 / Path Check & Geometry Filter", output)
        self.assertIn("2) 预检与基线统计阶段 / Precheck & Baseline Stats", output)
        self.assertIn("3) 数据标准化阶段 / CRS Standardization", output)
        self.assertIn("4) 数据处理阶段 / Data Processing", output)
        self.assertIn("5) 数据处理后导出阶段 / Export", output)
        self.assertIn("6) 数据处理后清理阶段 / Cleanup", output)
        self.assertIn("project_cleanup_work_layers, layer_remove_batch", output)
        self.assertIn("EPSG:4490", output)
        self.assertIn("final shapefile bundle", output)
        self.assertNotIn("project_cleanup_work_layers, layer_remove\n", output)
        self.assertNotIn("7) 数据处理后检查阶段 / Pre-Export Review", output)
        self.assertNotIn("10) 总结阶段 / Summary", output)
        self.assertNotIn("qgis_shapefile_quality_gate", output)
        self.assertNotIn("qgis_shapefile_export_review", output)

    def test_prompt_defaults_for_optional_arguments(self):
        """
        作用：执行测试用例 `prompt defaults for optional arguments`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompt defaults for optional arguments`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        prompt_fn = mcp.prompt_funcs["qgis_shapefile_pipeline_planner"]
        output = prompt_fn(
            task_name="roads-cleanup",
            input_dir="C:/input",
            output_dir="C:/output",
        )

        self.assertIn(
            "quality_rule_resource: qgis://workflow/shapefile/quality-profile/default",
            output,
        )
        self.assertIn(
            "deliverables: 标准交付：输出 shapefile bundle、质量结果、运行摘要。",
            output,
        )
