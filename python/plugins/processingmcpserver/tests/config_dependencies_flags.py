from __future__ import annotations

from processingmcpserver.config import (
    default_processing_mcp_json_document,
    load_processing_mcp_server_config,
)

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
        config = load_processing_mcp_server_config()
        self.assertFalse(config.enable_execute_code)

        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["enable_execute_code"] = True
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertTrue(config.enable_execute_code)

    def test_enable_execute_code_invalid_json_fallback_to_settings(self):
        self.settings.setValue("Processing/MCP/enable_execute_code", True)
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["enable_execute_code"] = "invalid"
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()

        self.assertTrue(config.enable_execute_code)
        self.assertEqual(config.config_sources.get("enable_execute_code"), "Settings")

    def test_filesystem_defaults_and_json_override(self):
        config = load_processing_mcp_server_config()
        self.assertFalse(config.filesystem.disable_filesystem_tools)
        self.assertGreaterEqual(len(config.filesystem.allowed_roots), 1)

        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["filesystem"] = {
            "allowed_roots": ["C:/allowed-a", "D:/allowed-b"],
            "readonly_roots": ["D:/allowed-b"],
            "disable_filesystem_tools": True,
        }
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()

        self.assertEqual(config.filesystem.allowed_roots, ["C:/allowed-a", "D:/allowed-b"])
        self.assertEqual(config.filesystem.readonly_roots, ["D:/allowed-b"])
        self.assertTrue(config.filesystem.disable_filesystem_tools)
        self.assertEqual(config.config_sources.get("filesystem.allowed_roots"), "JSON")
        self.assertEqual(config.config_sources.get("filesystem.readonly_roots"), "JSON")
        self.assertEqual(
            config.config_sources.get("filesystem.disable_filesystem_tools"),
            "JSON",
        )

    def test_filesystem_invalid_json_fallback_to_settings(self):
        self.settings.setValue(
            "Processing/MCP/filesystem/allowed_roots", "C:/settings-allowed"
        )
        self.settings.setValue(
            "Processing/MCP/filesystem/disable_filesystem_tools", True
        )
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["filesystem"] = {
            "allowed_roots": 123,
            "readonly_roots": "C:/readonly",
            "disable_filesystem_tools": "invalid",
        }
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()

        self.assertEqual(config.filesystem.allowed_roots, ["C:/settings-allowed"])
        self.assertEqual(config.filesystem.readonly_roots, ["C:/readonly"])
        self.assertTrue(config.filesystem.disable_filesystem_tools)
        self.assertEqual(
            config.config_sources.get("filesystem.allowed_roots"), "Settings"
        )
        self.assertEqual(
            config.config_sources.get("filesystem.readonly_roots"), "JSON"
        )
        self.assertEqual(
            config.config_sources.get("filesystem.disable_filesystem_tools"),
            "Settings",
        )

