# SPDX-License-Identifier: GPL-2.0-or-later
"""Credential-free upstream configuration for the QCopilots WebSearch relay."""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))
from qcopilots_mcp_server_web_search.config_store import ConfigError, load_config


class TestQCopilotsWebSearchConfig(unittest.TestCase):
    def setUp(self):
        self.root = Path(self.enterContext(tempfile.TemporaryDirectory()))
        self.path = self.root / "config.json"
        self.enterContext(mock.patch.dict(os.environ, {
            "QCOPILOTS_HOME": str(self.root), "QCOPILOTS_SECURITY_POLICY": "",
        }))
        for key in ("QCOPILOTS_WEBSEARCH_CONFIG", "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN", "QCOPILOTS_WEBSEARCH_ADMIN_TOKEN"):
            os.environ.pop(key, None)

    def config(self, value):
        self.path.write_text(json.dumps(value), encoding="utf-8")
        return load_config(self.path)

    def test_public_default_requires_no_credential_environment_or_file(self):
        config = load_config()
        self.assertEqual(config.backend_url, "https://web-search.vastarmorai.com/mcp")
        self.assertFalse(hasattr(config, "token"))
        self.assertEqual(config.timeout_seconds, 25)
        self.assertEqual(config.max_response_bytes, 1048576)
        self.assertEqual(config.max_concurrency, 5)

    def test_user_configuration_and_explicit_environment_path_precedence(self):
        user_config = self.root / "web_search" / "config.json"
        user_config.parent.mkdir()
        user_config.write_text(json.dumps({"backend_url": "https://user.example.invalid"}), encoding="utf-8")
        self.assertEqual(load_config().backend_url, "https://user.example.invalid/mcp")
        self.path.write_text(json.dumps({"backend_url": "https://override.example.invalid/mcp"}), encoding="utf-8")
        os.environ["QCOPILOTS_WEBSEARCH_CONFIG"] = str(self.path)
        self.assertEqual(load_config().backend_url, "https://override.example.invalid/mcp")
        os.environ["QCOPILOTS_WEBSEARCH_CONFIG"] = "relative.json"
        with self.assertRaises(ConfigError):
            load_config()

    def test_legacy_origin_and_credential_fields_do_not_read_secrets(self):
        for origin, expected in (
            ("https://backend.example.invalid", "https://backend.example.invalid/mcp"),
            ("https://backend.example.invalid/", "https://backend.example.invalid/mcp"),
            ("https://backend.example.invalid/mcp", "https://backend.example.invalid/mcp"),
            ("http://127.0.0.1:3181", "http://127.0.0.1:3181/mcp"),
        ):
            with self.subTest(origin=origin):
                config = self.config({"backend_url": origin,
                    "auth": {"token_file": "does-not-exist.token"},
                    "settings": {"url": "http://127.0.0.1:8091", "admin_token_file": "missing-admin.token"}})
                self.assertEqual(config.backend_url, expected)
                self.assertFalse(hasattr(config, "token"))
        with mock.patch.dict(os.environ, {"QCOPILOTS_WEBSEARCH_BACKEND_TOKEN": "must-not-be-read"}):
            config = self.config({"backend_url": "https://backend.example.invalid/mcp",
                "auth": {"token_env": "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN"}})
            self.assertNotIn("must-not-be-read", repr(config))
            self.assertFalse(hasattr(config, "token"))

    def test_rejects_unsafe_urls_and_non_mcp_paths(self):
        urls = (
            "http://backend.example.invalid/mcp", "https://user:pass@example.invalid/mcp",
            "https://example.invalid/v1/search", "https://example.invalid/mcp?token=secret",
            "https://example.invalid/mcp#fragment", "https://example.invalid/mcp/../other",
            "https://example.invalid/%6dcp", "https://example.invalid/mcp\\other",
            " https://example.invalid/mcp", "https://example.invalid:0/mcp")
        for url in urls:
            with self.subTest(url=url), self.assertRaises(ConfigError):
                self.config({"backend_url": url})

    def test_existing_network_policy_still_applies_to_upstream(self):
        policy = {"mode": "formal_restricted", "shell": {"enabled": False, "executables": []},
            "network": {"enabled": True, "allowed_origins": ["http://127.0.0.1:3181"]}}
        with mock.patch.dict(os.environ, {"QCOPILOTS_SECURITY_POLICY": json.dumps(policy)}):
            with self.assertRaises(PermissionError):
                self.config({"backend_url": "https://blocked.example.invalid:443/mcp"})
            self.assertEqual(self.config({"backend_url": "http://127.0.0.1:3181/mcp"}).backend_url,
                             "http://127.0.0.1:3181/mcp")

    def test_limits_and_unknown_fields_are_validated(self):
        for key, value in (("timeout_seconds", 0), ("timeout_seconds", True),
                           ("max_response_bytes", 1048577), ("max_concurrency", 0), ("unexpected", "field")):
            with self.subTest(key=key, value=value), self.assertRaises(ConfigError):
                self.config({"backend_url": "https://backend.example.invalid/mcp", key: value})


if __name__ == "__main__":
    unittest.main()
