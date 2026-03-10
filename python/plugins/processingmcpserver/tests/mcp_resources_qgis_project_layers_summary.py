from __future__ import annotations

import json

from processingmcpserver.mcp_resources import register_resources

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class ResourceQgisProjectLayersSummaryTest(ProcessingMCPTestBase):
    def test_resource_registered(self):
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())
        self.assertIn("qgis://project/layers/summary", mcp.resource_uris)

    def test_resource_envelope_ok(self):
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())

        payload = json.loads(mcp.resource_funcs["qgis://project/layers/summary"]())
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["uri"], "qgis://project/layers/summary")
        self.assertEqual(payload["schema_version"], "1.0.0")
        self.assertIn("generated_at", payload)
        self.assertIn("project_id", payload)
        self.assertIn("data", payload)

    def test_resource_envelope_error_when_supplier_fails(self):
        class BrokenTools(DummyTools):
            def get_layers_summary(self):
                raise RuntimeError("layers summary failed")

        mcp = DummyMcp()
        register_resources(mcp, BrokenTools())

        payload = json.loads(mcp.resource_funcs["qgis://project/layers/summary"]())
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["uri"], "qgis://project/layers/summary")
        self.assertIn("error", payload)
        self.assertIn("layers summary failed", payload["error"]["message"])

