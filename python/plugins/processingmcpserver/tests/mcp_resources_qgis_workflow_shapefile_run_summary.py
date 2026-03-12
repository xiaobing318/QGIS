from __future__ import annotations

import json

from processingmcpserver.mcp_resources import register_resources

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class ResourceQgisWorkflowShapefileRunSummaryTest(ProcessingMCPTestBase):
    def test_resource_registered(self):
        """验证 resource registered 场景。"""
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())
        self.assertIn("qgis://workflow/shapefile/run-summary", mcp.resource_uris)

    def test_resource_envelope_ok(self):
        """验证 resource envelope ok 场景。"""
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())

        payload = json.loads(mcp.resource_funcs["qgis://workflow/shapefile/run-summary"]())
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["uri"], "qgis://workflow/shapefile/run-summary")
        self.assertIn("task_name", payload["data"])
        self.assertIn("status", payload["data"])
