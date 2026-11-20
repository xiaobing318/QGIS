# SPDX-License-Identifier: GPL-2.0-or-later
"""Test backend configuration precedence for the QCopilots Web Search service.

This program is free software. You can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or any later version.
"""

__copyright__ = "Copyright 2026, The QGIS Project"

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))
from qcopilots_mcp_server_web_search.config_store import load_config


class TestQCopilotsWebSearchConfig(unittest.TestCase):
    def test_public_default_and_backend_config_overrides(self):
        home = Path(self.enterContext(tempfile.TemporaryDirectory()))
        environment = {
            "QCOPILOTS_HOME": str(home),
            "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN": "test-backend",
            "QCOPILOTS_MCP_AUTH_TOKEN": "test-mcp",
            "QCOPILOTS_SECURITY_POLICY": "",
        }
        with mock.patch.dict(os.environ, environment):
            os.environ.pop("QCOPILOTS_WEBSEARCH_CONFIG", None)
            self.assertEqual(
                load_config().backend_url, "https://web-search.vastarmorai.com"
            )

            user_config = home / "web_search" / "config.json"
            user_config.parent.mkdir()
            config = {
                "backend_url": "http://127.0.0.1:8088",
                "auth": {"token_env": "QCOPILOTS_WEBSEARCH_BACKEND_TOKEN"},
            }
            user_config.write_text(json.dumps(config), encoding="utf-8")
            self.assertEqual(load_config().backend_url, config["backend_url"])

            config["backend_url"] = "https://custom.example.invalid"
            override = home / "override.json"
            override.write_text(json.dumps(config), encoding="utf-8")
            os.environ["QCOPILOTS_WEBSEARCH_CONFIG"] = str(override)
            self.assertEqual(load_config().backend_url, config["backend_url"])


if __name__ == "__main__":
    unittest.main()
