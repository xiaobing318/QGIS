from __future__ import annotations

from unittest.mock import patch

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class _FakeProcessingRegistry:
    def __init__(self, available_algorithms: list[str]):
        """Initialize the fake processing registry state."""
        self._available_algorithms = set(available_algorithms)

    def providers(self):
        """Return the test provider list."""
        return []

    def algorithmById(self, algorithm_id: str):
        """Return the test algorithm object for an ID."""
        if algorithm_id in self._available_algorithms:
            return object()
        return None


class ToolsRasterStatsZonalTest(ProcessingMCPTestBase):
    def test_registered(self):
        """Ensure the expected capability is registered."""
        assert_tool_registered(self, "raster_stats_zonal")

    def test_success_raster_stats_zonal(self):
        """Verify the successful path for zonal raster statistics."""
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon")
        raster_layer = self.add_sample_raster_layer("zonal_raster")
        original_fields = self.vector_field_names(vector_layer)

        result = tools.raster_stats_zonal(
            vector_layer_ref=vector_layer.id(),
            raster_layer_ref=raster_layer.id(),
            raster_band=1,
            in_place=False,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["tool"], "raster_stats_zonal")
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertIn("output_layer_id", result["summary"])
        self.assertNotEqual(result["summary"]["output_layer_id"], vector_layer.id())
        self.assertEqual(result["summary"]["algorithm"], "native:zonalstatisticsfb")
        self.assertNotIn("algorithm_selected", result["summary"])
        self.assertNotIn("fallback_used", result["summary"])
        self.assertEqual(self.vector_field_names(vector_layer), original_fields)

    def test_failure_invalid_raster_ref(self):
        """Verify the failure path for an invalid raster reference."""
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon2")

        with self.assertRaises(Exception) as ctx:
            tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref="missing-raster",
            )
        self.assertIn("Layer not found", str(ctx.exception))

    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.processing.run")
    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.QgsApplication.processingRegistry")
    def test_success_uses_latest_algorithm_only(
        self, mock_processing_registry, mock_run
    ):
        """Verify the successful path for uses the latest algorithm only."""
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon_fb_only")
        raster_layer = self.add_sample_raster_layer("zonal_raster_fb_only")
        mock_processing_registry.return_value = _FakeProcessingRegistry(
            ["native:zonalstatisticsfb"]
        )
        mock_run.return_value = {"OUTPUT": "fb-output-layer"}

        with patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal._PROCESSING_INITIALIZED", True):
            result = tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref=raster_layer.id(),
                raster_band=1,
                in_place=False,
            )

        self.assertTrue(result["ok"])
        self.assertEqual(result["summary"]["mode"], "copy")
        self.assertEqual(result["summary"]["algorithm"], "native:zonalstatisticsfb")
        self.assertEqual(result["summary"]["output_layer_id"], "fb-output-layer")
        self.assertNotIn("algorithm_selected", result["summary"])
        self.assertNotIn("fallback_used", result["summary"])
        self.assertEqual(mock_run.call_args.args[0], "native:zonalstatisticsfb")
        called_parameters = mock_run.call_args.args[1]
        self.assertIn("INPUT", called_parameters)
        self.assertIn("OUTPUT", called_parameters)
        self.assertNotIn("INPUT_VECTOR", called_parameters)

    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.processing.run")
    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.QgsApplication.processingRegistry")
    def test_success_in_place_normalizes_parameters_and_output_layer_id(
        self, mock_processing_registry, mock_run
    ):
        """Verify the successful path for in-place mode normalizes parameters and the output layer ID."""
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon_in_place")
        raster_layer = self.add_sample_raster_layer("zonal_raster_in_place")
        mock_processing_registry.return_value = _FakeProcessingRegistry(
            ["native:zonalstatisticsfb"]
        )
        mock_run.return_value = {}

        with patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal._PROCESSING_INITIALIZED", True):
            result = tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref=raster_layer.id(),
                raster_band=0,
                column_prefix="",
                in_place=True,
            )

        called_parameters = mock_run.call_args.args[1]
        self.assertEqual(called_parameters["RASTER_BAND"], 1)
        self.assertEqual(called_parameters["COLUMN_PREFIX"], "z_")
        self.assertEqual(called_parameters["OUTPUT"], vector_layer)
        self.assertEqual(result["summary"]["mode"], "in_place")
        self.assertEqual(result["summary"]["output_layer_id"], vector_layer.id())

    @patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal.QgsApplication.processingRegistry")
    def test_failure_when_zonalstatisticsfb_unavailable(self, mock_processing_registry):
        """Verify the failure path for when `zonalstatisticsfb` is unavailable."""
        tools = self.build_tools()
        vector_layer = self.add_sample_polygon_layer("zonal_polygon_no_algorithm")
        raster_layer = self.add_sample_raster_layer("zonal_raster_no_algorithm")
        mock_processing_registry.return_value = _FakeProcessingRegistry([])

        with (
            patch("processingmcpserver.mcp_tools.mcp_tools_raster_stats_zonal._PROCESSING_INITIALIZED", True),
            self.assertRaises(Exception) as ctx,
        ):
            tools.raster_stats_zonal(
                vector_layer_ref=vector_layer.id(),
                raster_layer_ref=raster_layer.id(),
            )
        self.assertIn("Algorithm not available: native:zonalstatisticsfb", str(ctx.exception))
