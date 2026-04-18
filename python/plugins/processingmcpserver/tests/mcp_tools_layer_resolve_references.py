from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsLayerResolveReferencesTest(ProcessingMCPTestBase):
    def test_registered(self):
        """
        作用：执行测试用例 `registered`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `registered`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        assert_tool_registered(self, "mcp_tools_layer_resolve_references")

    def test_success_reports_ambiguous_names(self):
        """
        作用：执行测试用例 `success reports ambiguous names`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success reports ambiguous names`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer_a = self.add_sample_vector_layer("duplicate-layer-name")
        layer_b = self.add_sample_vector_layer("duplicate-layer-name")

        result = tools.mcp_tools_layer_resolve_references(
            refs=["duplicate-layer-name", layer_a.id()],
            strict=False,
        )

        self.assertEqual(result["resolved"][layer_a.id()], layer_a.id())
        self.assertEqual(
            sorted(result["ambiguous"]["duplicate-layer-name"]),
            sorted([layer_a.id(), layer_b.id()]),
        )
        self.assertNotIn("duplicate-layer-name", result["resolved"])

    def test_success_non_strict_reports_missing(self):
        """
        作用：执行测试用例 `success non strict reports missing`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `success non strict reports missing`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("resolve_ref_vector")

        result = tools.mcp_tools_layer_resolve_references(
            refs=[layer.id(), "missing-layer"],
            strict=False,
        )
        self.assertEqual(result["resolved"][layer.id()], layer.id())
        self.assertIn("missing-layer", result["missing"])

    def test_failure_strict_with_missing(self):
        """
        作用：执行测试用例 `failure strict with missing`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure strict with missing`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_layer_resolve_references(refs=["missing-layer"], strict=True)
        self.assertIn("Layer reference resolve failed", str(ctx.exception))

    def test_failure_strict_with_ambiguous_name(self):
        """
        作用：执行测试用例 `failure strict with ambiguous name`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `failure strict with ambiguous name`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        self.add_sample_vector_layer("duplicate-layer-strict")
        self.add_sample_vector_layer("duplicate-layer-strict")

        with self.assertRaises(Exception) as ctx:
            tools.mcp_tools_layer_resolve_references(
                refs=["duplicate-layer-strict"],
                strict=True,
            )
        self.assertIn("ambiguous", str(ctx.exception))

