from __future__ import annotations

from unittest.mock import patch

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class _FakeProcessingRegistry:
    def __init__(self, available_algorithms: list[str]):
        self._available_algorithms = set(available_algorithms)

    def algorithmById(self, algorithm_id: str):
        if algorithm_id in self._available_algorithms:
            return object()
        return None


class ToolsRasterStatsZonalTest(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "raster_stats_zonal")

    def test_success_raster_stats_zonal(self):
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon")
        raster_layer = self.add_sample_raster_layer("zonal_raster")

        result = tools.raster_stats_zonal(
            vector_layer_ref=vector_layer.id(),
            raster_layer_ref=raster_layer.id(),
            raster_band=1,
            in_place=False,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "raster_stats_zonal")
        self.assertIn("output_layer_id", result["summary"])
        self.assertEqual(result["summary"]["algorithm"], "native:zonalstatisticsfb")
        self.assertNotIn("algorithm_selected", result["summary"])
        self.assertNotIn("fallback_used", result["summary"])

    def test_failure_invalid_raster_ref(self):
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon2")

        with self.assertRaises(Exception) as ctx:
            tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref="missing-raster",
            )
        self.assertIn("Layer not found", str(ctx.exception))

    @patch("processingmcpserver.mcp_tools.processing.run")
    @patch("processingmcpserver.mcp_tools.QgsApplication.processingRegistry")
    def test_success_uses_latest_algorithm_only(
        self, mock_processing_registry, mock_run
    ):
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon_fb_only")
        raster_layer = self.add_sample_raster_layer("zonal_raster_fb_only")
        mock_processing_registry.return_value = _FakeProcessingRegistry(
            ["native:zonalstatisticsfb"]
        )
        mock_run.return_value = {"OUTPUT": "fb-output-layer"}

        result = tools.raster_stats_zonal(
            vector_layer_ref=vector_layer.id(),
            raster_layer_ref=raster_layer.id(),
            raster_band=1,
            in_place=False,
        )

        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["algorithm"], "native:zonalstatisticsfb")
        self.assertEqual(result["summary"]["output_layer_id"], "fb-output-layer")
        self.assertNotIn("algorithm_selected", result["summary"])
        self.assertNotIn("fallback_used", result["summary"])
        self.assertEqual(mock_run.call_args.args[0], "native:zonalstatisticsfb")
        called_parameters = mock_run.call_args.args[1]
        self.assertIn("INPUT", called_parameters)
        self.assertIn("OUTPUT", called_parameters)
        self.assertNotIn("INPUT_VECTOR", called_parameters)

    @patch("processingmcpserver.mcp_tools.QgsApplication.processingRegistry")
    def test_failure_when_zonalstatisticsfb_unavailable(self, mock_processing_registry):
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon_no_algorithm")
        raster_layer = self.add_sample_raster_layer("zonal_raster_no_algorithm")
        mock_processing_registry.return_value = _FakeProcessingRegistry([])

        with self.assertRaises(Exception) as ctx:
            tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref=raster_layer.id(),
            )
        self.assertIn("Algorithm not available: native:zonalstatisticsfb", str(ctx.exception))

