from __future__ import annotations

import json

from processingmcpserver.mcp_resources import register_resources

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class ResourceQgisWorkflowShapefileRunSummaryTest(ProcessingMCPTestBase):
    def test_resource_registered(self):
        """
        作用：执行测试用例 `resource registered`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `resource registered`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())
        self.assertIn("qgis://workflow/shapefile/run-summary", mcp.resource_uris)

    def test_resource_envelope_ok(self):
        """
        作用：执行测试用例 `resource envelope ok`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `resource envelope ok`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())

        payload = json.loads(mcp.resource_funcs["qgis://workflow/shapefile/run-summary"]())
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["uri"], "qgis://workflow/shapefile/run-summary")
        self.assertIn("task_name", payload["data"])
        self.assertIn("status", payload["data"])
