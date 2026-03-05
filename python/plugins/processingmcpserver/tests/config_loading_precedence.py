from __future__ import annotations

import json

from processingmcpserver.config import (
    default_processing_mcp_json_document,
    load_processing_mcp_server_config,
)

from ._shared_case_base import ProcessingMCPTestBase


class ConfigLoadingPrecedenceTest(ProcessingMCPTestBase):
    def test_create_default_json_when_missing(self):
        self.assertFalse(self.config_path.exists())

        config = load_processing_mcp_server_config()

        self.assertTrue(self.config_path.exists())
        self.assertEqual(config.host, "127.0.0.1")
        self.assertEqual(config.transport, "streamable-http")
        self.assertEqual(config.port, 8000)

        payload = json.loads(self.config_path.read_text(encoding="utf-8"))
        self.assertIn("processing_mcp", payload)
        self.assertEqual(payload.get("version"), 1)
        self.assertNotIn("limits", payload["processing_mcp"])

    def test_json_overrides_settings_host(self):
        self.settings.setValue("Processing/MCP/host", "settings-host")
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["host"] = "json-host"
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertEqual(config.host, "json-host")
        self.assertEqual(config.config_sources.get("host"), "JSON")

    def test_json_missing_key_fallback_to_settings(self):
        self.settings.setValue("Processing/MCP/host", "settings-host")
        payload = default_processing_mcp_json_document()
        del payload["processing_mcp"]["host"]
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertEqual(config.host, "settings-host")
        self.assertEqual(config.config_sources.get("host"), "Settings")

    def test_invalid_json_value_fallback_to_settings(self):
        self.settings.setValue("Processing/MCP/port", 8123)
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["port"] = "bad-port"
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertEqual(config.port, 8123)
        self.assertEqual(config.config_sources.get("port"), "Settings")

    def test_invalid_json_and_settings_fallback_to_default(self):
        self.settings.setValue("Processing/MCP/port", 99999)
        self._write_raw_json_text("{invalid json")

        config = load_processing_mcp_server_config()
        self.assertEqual(config.port, 8000)
        self.assertEqual(config.config_sources.get("port"), "Default")

    def test_legacy_limits_json_is_ignored(self):
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["limits"] = {
            "max_feature_limit": 111,
            "max_algorithm_list_limit": 222,
            "max_render_width": 333,
            "max_render_height": 444,
            "max_render_pixels": 555666,
            "max_execute_code_chars": 777,
        }
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertEqual(config.host, "127.0.0.1")
        self.assertEqual(config.port, 8000)
        self.assertIsNone(config.config_sources.get("limits.max_feature_limit"))
