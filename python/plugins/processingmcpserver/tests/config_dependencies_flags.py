from __future__ import annotations

from processingmcpserver.config import (
    default_processing_mcp_json_document,
    load_processing_mcp_server_config,
)
from processingmcpserver.mcp_tools import ProcessingMCPTools

from ._shared_case_base import ProcessingMCPTestBase


class ConfigDependenciesFlagsTest(ProcessingMCPTestBase):
    def test_dependencies_default_loaded(self):
        config = load_processing_mcp_server_config()
        self.assertTrue(config.dependencies.auto_install)
        self.assertEqual(config.config_sources.get("dependencies.auto_install"), "JSON")

    def test_dependencies_json_overrides_settings(self):
        self.settings.setValue("Processing/MCP/dependencies/auto_install", False)
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["dependencies"] = {
            "auto_install": True,
        }
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertTrue(config.dependencies.auto_install)
        self.assertEqual(config.config_sources.get("dependencies.auto_install"), "JSON")

    def test_dependencies_invalid_json_fallback_to_settings(self):
        self.settings.setValue("Processing/MCP/dependencies/auto_install", False)
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["dependencies"] = {
            "auto_install": "invalid",
        }
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertFalse(config.dependencies.auto_install)
        self.assertEqual(config.config_sources.get("dependencies.auto_install"), "Settings")

    def test_enable_execute_code_default_and_json_override(self):
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["enable_execute_code"] = True
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertTrue(config.enable_execute_code)

    def test_feature_limit_default_and_dynamic_override(self):
        tools = self.build_tools()
        requested, applied, capped = tools._normalize_feature_limit(None)
        self.assertEqual(requested, ProcessingMCPTools.DEFAULT_FEATURE_LIMIT)
        self.assertEqual(applied, ProcessingMCPTools.DEFAULT_FEATURE_LIMIT)
        self.assertFalse(capped)

        requested, applied, capped = tools._normalize_feature_limit(42)
        self.assertEqual(requested, 42)
        self.assertEqual(applied, 42)
        self.assertFalse(capped)

    def test_feature_limit_capped_when_overflow(self):
        tools = self.build_tools()
        requested, applied, capped = tools._normalize_feature_limit(
            ProcessingMCPTools.MAX_FEATURE_LIMIT + 1
        )
        self.assertEqual(requested, ProcessingMCPTools.MAX_FEATURE_LIMIT + 1)
        self.assertEqual(applied, ProcessingMCPTools.MAX_FEATURE_LIMIT)
        self.assertTrue(capped)

    def test_dataset_filter_normalizers(self):
        tools = self.build_tools()
        self.assertEqual(tools._normalize_dataset_kind("vector"), "vector")
        self.assertEqual(tools._normalize_dataset_kind("both"), "both")
        self.assertEqual(tools._normalize_geometry_type_filter("line"), "line")
        self.assertEqual(tools._normalize_layer_types_filter("raster"), "raster")
        with self.assertRaises(Exception):
            tools._normalize_dataset_kind("invalid-kind")
        with self.assertRaises(Exception):
            tools._normalize_geometry_type_filter("curve")

    def test_execute_processing_algorithm_on_layers_requires_bindings(self):
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools._processing_execute_on_layers_impl(
                algorithm="native:buffer",
                layer_bindings={},
                parameters={},
                load_results=False,
                batch_mode=False,
                allow_disk_write=False,
                allow_in_place_edit=False,
            )
        self.assertIn("layer_bindings is required", str(ctx.exception))

