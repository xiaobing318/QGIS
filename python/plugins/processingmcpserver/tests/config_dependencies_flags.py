from __future__ import annotations

from processingmcpserver.config import (
    default_processing_mcp_json_document,
    load_processing_mcp_server_config,
)

from ._shared_case_base import ProcessingMCPTestBase


class ConfigDependenciesFlagsTest(ProcessingMCPTestBase):
    def test_dependencies_default_loaded(self):
        """Verify dependencies default loaded."""
        config = load_processing_mcp_server_config()
        self.assertTrue(config.dependencies.auto_install)
        self.assertEqual(config.config_sources.get("dependencies.auto_install"), "JSON")

    def test_dependencies_json_overrides_settings(self):
        """Verify dependencies JSON overrides settings."""
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
        """Verify dependencies invalid JSON fallback to settings."""
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
        """Verify enable execute code default and JSON override."""
        config = load_processing_mcp_server_config()
        self.assertFalse(config.enable_execute_code)

        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["enable_execute_code"] = True
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertTrue(config.enable_execute_code)

    def test_enable_execute_code_invalid_json_fallback_to_settings(self):
        """Verify enable execute code invalid JSON fallback to settings."""
        self.settings.setValue("Processing/MCP/enable_execute_code", True)
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["enable_execute_code"] = "invalid"
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()

        self.assertTrue(config.enable_execute_code)
        self.assertEqual(config.config_sources.get("enable_execute_code"), "Settings")

