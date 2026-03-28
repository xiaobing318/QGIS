from __future__ import annotations

import json

from processingmcpserver.mcp_resources import register_resources

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class ResourceQgisWorkflowShapefileQualityProfileDefaultTest(ProcessingMCPTestBase):
    def test_resource_registered(self):
        """Verify resource registered."""
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())
        self.assertIn("qgis://workflow/shapefile/quality-profile/default", mcp.resource_uris)

    def test_resource_envelope_ok(self):
        """Verify resource envelope ok."""
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())

        payload = json.loads(
            mcp.resource_funcs["qgis://workflow/shapefile/quality-profile/default"]()
        )
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["uri"], "qgis://workflow/shapefile/quality-profile/default")
        self.assertIn("quality_checks", payload["data"])
