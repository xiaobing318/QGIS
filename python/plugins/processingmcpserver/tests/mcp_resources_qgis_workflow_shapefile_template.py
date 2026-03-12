from __future__ import annotations

import json

from processingmcpserver.mcp_resources import register_resources

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class ResourceQgisWorkflowShapefileTemplateTest(ProcessingMCPTestBase):
    def test_resource_registered(self):
        """验证 resource registered 场景。"""
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())
        self.assertIn("qgis://workflow/shapefile/template", mcp.resource_uris)

    def test_resource_envelope_ok(self):
        """验证 resource envelope ok 场景。"""
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())

        payload = json.loads(mcp.resource_funcs["qgis://workflow/shapefile/template"]())
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["uri"], "qgis://workflow/shapefile/template")
        self.assertIn("workflow_stages", payload["data"])
        self.assertEqual(
            payload["data"]["workflow_stages"],
            [
                "path_check_and_geometry_filter",
                "precheck_and_pre_statistics",
                "standardize_crs",
                "data_processing",
                "export",
                "cleanup",
            ],
        )
        self.assertEqual(
            payload["data"]["required_inputs"],
            [
                "task_name",
                "input_dir",
                "output_dir",
                "quality_rule_resource",
                "deliverables",
            ],
        )
        defaults = payload["data"]["defaults"]
        self.assertEqual(defaults["default_target_crs"], "EPSG:4490")
        self.assertEqual(defaults["default_geometry_filter"], "all")
