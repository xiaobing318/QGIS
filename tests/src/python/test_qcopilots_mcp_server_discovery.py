"""QGIS unit tests for QCopilots MCP service discovery.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import ast
import json
import os
import re
import sys
import tempfile
import types
import unittest
from pathlib import Path


SERVICE_PLUGIN_DIR_NAMES = (
    "qcopilots_mcp_server_builtin_tools",
    "qcopilots_mcp_server_interactive_tools",
    "qcopilots_mcp_server_processing_raster",
    "qcopilots_mcp_server_processing_vector",
    "qcopilots_mcp_server_skills",
)

SERVICE_SCHEMA_DESCRIPTION_LOCALIZED_DIR_NAMES = (
    "qcopilots_mcp_server_builtin_tools",
    "qcopilots_mcp_server_interactive_tools",
    "qcopilots_mcp_server_processing_raster",
    "qcopilots_mcp_server_processing_vector",
    "qcopilots_mcp_server_skills",
)

DEFAULT_STARTUP_SERVICE_IDS = [
    "qcopilots.mcp_server_builtin_tools",
    "qcopilots.mcp_server_interactive_tools",
    "qcopilots.mcp_server_processing_vector",
    "qcopilots.mcp_server_processing_raster",
    "qcopilots.mcp_server_skills",
]

SERVICE_DESCRIPTION_REQUIRED_TERMS = {
    "qcopilots.mcp_server_builtin_tools": (
        "本机工具",
        "文件读取",
        "Shell 执行",
        "文件写入",
        "文本替换编辑",
        ("llama-ui", "MCP 客户端"),
        "QCopilots MCP Servers Manager",
    ),
    "qcopilots.mcp_server_interactive_tools": (
        "QGIS bridge",
        "图层",
        "地图画布",
        "数据导出",
        ("加载", "移除"),
        ("XYZ", "WMS", "WCS"),
        ("矢量数据创建", "要素更新"),
        ("provider", "本机数据访问权限"),
        ("llama-ui", "MCP 客户端"),
        ("工程状态", "桌面工程"),
    ),
    "qcopilots.mcp_server_processing_raster": (
        "QGIS bridge",
        "Processing",
        "栅格",
        ("DEM", "重采样"),
        ("provider", "本机数据访问权限"),
    ),
    "qcopilots.mcp_server_processing_vector": (
        "QGIS bridge",
        "Processing",
        "矢量",
        ("几何", "属性"),
        ("provider", "数据上下文"),
    ),
    "qcopilots.mcp_server_skills": (
        "Agent Skills",
        "只读",
        "技能",
        ("入口资源", "技能文件内容"),
        ("不负责修改", "不修改"),
    ),
}

MANAGER_CONFIG_SCHEMA_CANDIDATES = (
    ("qcopilots_manager_config.json", "qcopilots_manager_config.schema.json"),
    ("qcopilots_manager.json", "qcopilots_manager.schema.json"),
    ("qcopilots_mcp_servers_manager_config.json", "qcopilots_mcp_servers_manager_config.schema.json"),
)


def _write_manifest(directory, service_id, port, enabled=True):
    directory.mkdir(parents=True, exist_ok=True)
    manifest_path = directory / "qcopilots_service.json"
    manifest_path.write_text(
        json.dumps(
            {
                "service_id": service_id,
                "display_name": service_id.replace("_", " ").title(),
                "description": "Test MCP service",
                "entry_point": service_id + ".server:main",
                "transport": {
                    "type": "http",
                    "host": "127.0.0.1",
                    "port": port,
                    "path": "/mcp",
                },
                "capabilities": ["tools"],
                "enabled": enabled,
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    return manifest_path


class TestQCopilotsMcpServerDiscovery(unittest.TestCase):
    def test_manifest_loader_normalizes_http_endpoint(self):
        from qcopilots_common.manifest import ServiceManifest, load_service_manifest

        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = _write_manifest(
                Path(tmp) / "qcopilots_mcp_server_builtin_tools",
                "qcopilots.mcp_server_builtin_tools",
                18711,
            )

            manifest = load_service_manifest(manifest_path)

            self.assertIsInstance(manifest, ServiceManifest)
            self.assertEqual(manifest.service_id, "qcopilots.mcp_server_builtin_tools")
            self.assertEqual(manifest.display_name, "Qcopilots.Mcp Server Builtin Tools")
            self.assertEqual(manifest.entry_point, "qcopilots.mcp_server_builtin_tools.server:main")
            self.assertEqual(manifest.transport.type, "http")
            self.assertEqual(manifest.transport.host, "127.0.0.1")
            self.assertEqual(manifest.transport.port, 18711)
            self.assertEqual(manifest.transport.path, "/mcp")
            self.assertEqual(manifest.endpoint_url, "http://127.0.0.1:18711/mcp")
            self.assertEqual(manifest.capabilities, ["tools"])
            self.assertEqual(manifest.source_path, manifest_path)

    def test_manifest_loader_keeps_bind_and_advertised_hosts_separate(self):
        from qcopilots_common.manifest import load_service_manifest, manifest_to_dict

        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = _write_manifest(
                Path(tmp) / "qcopilots_mcp_server_builtin_tools",
                "qcopilots.mcp_server_builtin_tools",
                18711,
            )
            manifest_data = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest_data["transport"]["host"] = "0.0.0.0"
            manifest_data["transport"]["advertised_host"] = "qgis-client-01.local"
            manifest_path.write_text(json.dumps(manifest_data), encoding="utf-8")

            manifest = load_service_manifest(manifest_path)

            self.assertEqual(manifest.host, "0.0.0.0")
            self.assertEqual(manifest.advertised_host, "qgis-client-01.local")
            self.assertEqual(manifest.endpoint_url, "http://qgis-client-01.local:18711/mcp")
            self.assertEqual(manifest.url(18712), "http://qgis-client-01.local:18712/mcp")
            self.assertEqual(
                manifest_to_dict(manifest, 18712),
                {
                    "service_id": "qcopilots.mcp_server_builtin_tools",
                    "display_name": "Qcopilots.Mcp Server Builtin Tools",
                    "description": "Test MCP service",
                    "plugin_name": "qcopilots_mcp_server_builtin_tools",
                    "plugin_dir": str(manifest.plugin_dir),
                    "entry_point": str(manifest.entry_path),
                    "host": "0.0.0.0",
                    "bind_host": "0.0.0.0",
                    "advertised_host": "qgis-client-01.local",
                    "port": 18712,
                    "mcp_path": "/mcp",
                    "url": "http://qgis-client-01.local:18712/mcp",
                    "category": "qcopilots",
                    "capabilities": ["tools"],
                    "enabled": True,
                    "cors_origins": [
                        "http://127.0.0.1:8282",
                        "http://localhost:8282",
                    ],
                    "icon": "",
                    "icon_path": "",
                    "icons": [],
                },
            )

    def test_discovery_returns_enabled_services_in_stable_order(self):
        from qcopilots_common.discovery import discover_service_manifests

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_manifest(root / "z_service", "z_service", 18799)
            _write_manifest(root / "a_service", "a_service", 18701)
            _write_manifest(root / "disabled_service", "disabled_service", 18702, False)
            (root / "broken" / "qcopilots_service.json").parent.mkdir()
            (root / "broken" / "qcopilots_service.json").write_text(
                "{not json",
                encoding="utf-8",
            )

            discovered = discover_service_manifests(root)
            self.assertEqual(
                [manifest.service_id for manifest in discovered],
                ["a_service", "z_service"],
            )
            self.assertTrue(all(manifest.enabled for manifest in discovered))

            all_services = discover_service_manifests(root, include_disabled=True)
            self.assertEqual(
                [manifest.service_id for manifest in all_services],
                ["a_service", "disabled_service", "z_service"],
            )

    def test_real_qcopilots_service_manifests_are_discoverable(self):
        from qcopilots_common.discovery import discover_service_manifests
        from qcopilots_common.manifest import manifest_to_dict

        plugins_root = _plugins_root()
        discovered = discover_service_manifests(plugins_root)
        self.assertEqual(
            [manifest.service_id for manifest in discovered],
            [
                "qcopilots.mcp_server_builtin_tools",
                "qcopilots.mcp_server_interactive_tools",
                "qcopilots.mcp_server_processing_raster",
                "qcopilots.mcp_server_processing_vector",
                "qcopilots.mcp_server_skills",
            ],
        )
        self.assertEqual(
            [manifest.plugin_name for manifest in discovered],
            [
                "QCopilots MCP Server Builtin Tools",
                "QCopilots MCP Server Interactive Tools",
                "QCopilots MCP Server Processing Raster",
                "QCopilots MCP Server Processing Vector",
                "QCopilots MCP Server Skills",
            ],
        )
        self.assertTrue(all(manifest.enabled for manifest in discovered))
        self.assertEqual([manifest.mcp_path for manifest in discovered], ["/mcp"] * 5)
        self.assertEqual(
            [manifest.default_port for manifest in discovered],
            [48211, 48213, 48215, 48214, 48212],
        )
        for manifest in discovered:
            self.assertTrue(_contains_cjk(manifest.description), manifest.description)
            self.assertGreaterEqual(len(manifest.description), 45)
            _assert_text_contains_terms(
                self,
                manifest.description,
                f"manifest:{manifest.service_id}:description",
                SERVICE_DESCRIPTION_REQUIRED_TERMS[manifest.service_id],
            )
            self.assertEqual(manifest.icon, "icon.svg")
            self.assertTrue(manifest.icon_path.is_file())
            if manifest.service_id == "qcopilots.mcp_server_builtin_tools":
                self.assertIn("文本替换编辑", manifest.description)
                self.assertNotIn("补丁", manifest.description)
            icons = manifest.icon_descriptors()
            self.assertEqual(icons[0]["mimeType"], "image/svg+xml")
            self.assertTrue(icons[0]["src"].startswith("data:image/svg+xml;base64,"))
            self.assertEqual(manifest_to_dict(manifest)["icons"], icons)

    def test_real_qcopilots_service_bridge_urls_match_capabilities(self):
        _install_manager_import_stubs()

        from qcopilots_common.constants import BRIDGE_URL_ENV
        from qcopilots_common.discovery import discover_service_manifests
        from qcopilots_mcp_servers_manager.plugin import QCopilotsMCPServersManagerPlugin

        bridge_url = "http://127.0.0.1:48200"
        plugin = object.__new__(QCopilotsMCPServersManagerPlugin)
        plugin.bridge = types.SimpleNamespace(url=bridge_url)
        manifests = {
            manifest.service_id: manifest
            for manifest in discover_service_manifests(_plugins_root())
        }
        expected_bridge_services = {
            "qcopilots.mcp_server_builtin_tools": False,
            "qcopilots.mcp_server_interactive_tools": True,
            "qcopilots.mcp_server_processing_raster": True,
            "qcopilots.mcp_server_processing_vector": True,
            "qcopilots.mcp_server_skills": False,
        }

        self.assertEqual(set(manifests), set(expected_bridge_services))
        for service_id, uses_bridge in expected_bridge_services.items():
            with self.subTest(service_id=service_id):
                manifest = manifests[service_id]
                expected_url = bridge_url if uses_bridge else None
                self.assertEqual(plugin._bridge_url(manifest), expected_url)
                env = plugin._service_env(manifest)
                if uses_bridge:
                    self.assertEqual(env, {BRIDGE_URL_ENV: bridge_url})
                else:
                    self.assertEqual(env, {})

    def test_real_qcopilots_service_manifests_have_descriptive_schemas(self):
        plugins_root = _plugins_root()
        service_fields = (
            "service_id",
            "plugin_name",
            "display_name",
            "description",
            "entry_point",
            "host",
            "advertised_host",
            "default_port",
            "mcp_path",
            "category",
            "capabilities",
            "runtime",
            "cors_origins",
        )

        for plugin_dir_name in SERVICE_PLUGIN_DIR_NAMES:
            plugin_dir = plugins_root / plugin_dir_name
            manifest_path = plugin_dir / "qcopilots_service.json"
            schema_path = plugin_dir / "qcopilots_service.schema.json"

            self.assertTrue(manifest_path.is_file(), plugin_dir_name)
            self.assertTrue(schema_path.is_file(), plugin_dir_name)
            manifest = _read_json(manifest_path)
            if "$schema" in manifest:
                self.assertEqual(
                    manifest["$schema"],
                    "./qcopilots_service.schema.json",
                    plugin_dir_name,
                )
            schema = _read_json(schema_path)
            _assert_json_schema_header(self, schema, schema_path)
            _assert_json_matches_schema(self, manifest, schema, f"{plugin_dir_name}:manifest")
            future_manifest = dict(manifest)
            future_manifest["service_id"] = "qcopilots.future_service"
            _assert_json_matches_schema(
                self,
                future_manifest,
                schema,
                f"{plugin_dir_name}:future service_id",
            )
            if plugin_dir_name in SERVICE_SCHEMA_DESCRIPTION_LOCALIZED_DIR_NAMES:
                _assert_schema_descriptions_are_chinese(self, schema, plugin_dir_name)
                _assert_schema_descriptions_cover_prompting_contract(self, schema, plugin_dir_name)

            properties = schema["properties"]
            self.assertNotIn("command", properties, plugin_dir_name)
            self.assertNotIn("enum", properties["service_id"], plugin_dir_name)
            self.assertEqual(
                properties["service_id"]["pattern"],
                "^(?!\\.)(?!.*\\.\\.)(?!.*\\.$)[A-Za-z0-9_.-]+$",
                plugin_dir_name,
            )
            for field in service_fields:
                self.assertIn(field, properties, f"{plugin_dir_name}:{field}")
                _assert_described_property(self, properties, field, plugin_dir_name)

            _assert_described_property(
                self,
                properties,
                "capabilities",
                plugin_dir_name,
                required_terms=("tools", "qgis-bridge", "processing"),
            )
            _assert_described_property(
                self,
                properties,
                "runtime",
                plugin_dir_name,
                required_terms=("uv", ("requirements", "lock file")),
            )
            _assert_described_property(
                self,
                properties,
                "cors_origins",
                plugin_dir_name,
                required_terms=(("CORS", "origin", "origins"), ("跨域", "browser", "allowed")),
            )
            _assert_described_property(
                self,
                properties,
                "mcp_path",
                plugin_dir_name,
                required_terms=("/mcp", ("路径", "path")),
            )
            _assert_described_property(
                self,
                properties,
                "default_port",
                plugin_dir_name,
                required_terms=(("端口", "port"), ("启动", "start", "client", "reconnect")),
            )

    def test_manager_config_schema_documents_default_start_behavior(self):
        manager_dir = _plugins_root() / "qcopilots_mcp_servers_manager"
        existing_candidates = [
            (manager_dir / config_name, manager_dir / schema_name)
            for config_name, schema_name in MANAGER_CONFIG_SCHEMA_CANDIDATES
            if (manager_dir / config_name).is_file() or (manager_dir / schema_name).is_file()
        ]
        self.assertEqual(
            len(existing_candidates),
            1,
            "Expected exactly one manager config/schema pair using a known qcopilots manager name",
        )
        config_path, schema_path = existing_candidates[0]
        self.assertTrue(config_path.is_file(), str(config_path))
        self.assertTrue(schema_path.is_file(), str(schema_path))

        config = _read_json(config_path)
        schema = _read_json(schema_path)
        self.assertEqual(config.get("$schema"), f"./{schema_path.name}")
        _assert_json_schema_header(self, schema, schema_path)
        _assert_json_matches_schema(self, config, schema, "manager config")
        _assert_schema_descriptions_cover_prompting_contract(self, schema, "manager config")
        self.assertIn("default_startup", schema["properties"])
        self.assertIn("service_network", schema["properties"])
        self.assertNotIn("workspace_roots", schema["properties"])

        default_start = schema["properties"]["default_startup"]
        _assert_described_schema_node(
            self,
            default_start,
            "manager:default_startup",
            required_terms=(("默认", "default", "automatic"), ("启动", "start"), ("显式", "service_ids", "list")),
        )
        self.assertEqual(default_start["type"], "object")
        self.assertNotIn("required", schema)
        self.assertNotIn("required", default_start)
        self.assertIn("properties", default_start)
        default_start_properties = default_start["properties"]
        self.assertIn("enabled", default_start_properties)
        self.assertIn("service_ids", default_start_properties)
        self.assertNotIn("count", default_start_properties)
        self.assertNotIn("service_order", default_start_properties)
        _assert_described_property(
            self,
            default_start_properties,
            "enabled",
            "manager",
            required_terms=("true", "false", ("启动", "start")),
        )
        _assert_described_property(
            self,
            default_start_properties,
            "service_ids",
            "manager",
            required_terms=("service_id", ("顺序", "order"), ("空", "empty", "[]")),
        )
        service_id_items = default_start_properties["service_ids"]["items"]
        self.assertEqual(service_id_items["pattern"], "^(?!\\.)(?!.*\\.\\.)(?!.*\\.$)[A-Za-z0-9_.-]+$")
        self.assertNotIn("enum", service_id_items)
        _assert_described_schema_node(
            self,
            service_id_items,
            "manager:service_ids.items",
            required_terms=(("候选", "candidate", "known"), "Builtin Tools"),
        )

        self.assertIn("default_startup", config)
        self.assertNotIn("workspace_roots", config)
        self.assertIs(config["default_startup"]["enabled"], True)
        self.assertEqual(
            config["default_startup"]["service_ids"],
            DEFAULT_STARTUP_SERVICE_IDS,
        )
        self.assertEqual(
            default_start_properties["service_ids"]["default"],
            DEFAULT_STARTUP_SERVICE_IDS,
        )
        self.assertNotIn("default", service_id_items)

        service_network = schema["properties"]["service_network"]
        _assert_described_schema_node(
            self,
            service_network,
            "manager:service_network",
            required_terms=(("LAN", "局域网"), ("host", "绑定"), ("CORS", "cors_origins")),
        )
        service_network_properties = service_network["properties"]
        self.assertIn("enabled", service_network_properties)
        self.assertIn("host", service_network_properties)
        self.assertIn("advertised_host", service_network_properties)
        self.assertIn("cors_origins", service_network_properties)
        self.assertIs(config["service_network"]["enabled"], True)
        self.assertEqual(config["service_network"]["host"], "0.0.0.0")
        self.assertEqual(config["service_network"]["advertised_host"], "")
        self.assertEqual(
            config["service_network"]["cors_origins"],
            ["*"],
        )
        self.assertEqual(service_network_properties["enabled"]["default"], True)
        self.assertEqual(service_network_properties["host"]["default"], "0.0.0.0")
        self.assertEqual(
            service_network_properties["cors_origins"]["default"],
            ["*"],
        )
        for host in ("0.0.0.0", "127.0.0.1", "10.1.2.3", "192.168.1.50", "169.254.10.20"):
            candidate = json.loads(json.dumps(config))
            candidate["service_network"]["host"] = host
            _assert_json_matches_schema(self, candidate, schema, f"manager config host {host}")
        for host in ("8.8.8.8", "qgis-client-01.local", "127.0.0.999"):
            candidate = json.loads(json.dumps(config))
            candidate["service_network"]["host"] = host
            self.assertTrue(
                _json_schema_errors(candidate, schema, f"manager config invalid host {host}"),
                host,
            )
        for advertised_host in ("qgis-client-01", "qgis-client-01.local", "192.168.1.50", ""):
            candidate = json.loads(json.dumps(config))
            candidate["service_network"]["advertised_host"] = advertised_host
            _assert_json_matches_schema(
                self,
                candidate,
                schema,
                f"manager config advertised_host {advertised_host}",
            )
        for advertised_host in ("qgis..local", "http://qgis-client-01.local/mcp", "qgis-client-01:48200"):
            candidate = json.loads(json.dumps(config))
            candidate["service_network"]["advertised_host"] = advertised_host
            self.assertTrue(
                _json_schema_errors(
                    candidate,
                    schema,
                    f"manager config invalid advertised_host {advertised_host}",
                ),
                advertised_host,
            )
        for origin in ("*", "http://127.0.0.1:8282", "https://server-host/"):
            candidate = json.loads(json.dumps(config))
            candidate["service_network"]["cors_origins"] = [origin]
            _assert_json_matches_schema(self, candidate, schema, f"manager config CORS {origin}")
        for origin in ("::1", "https://example.invalid/path", "http://server-host:8282/?debug=1"):
            candidate = json.loads(json.dumps(config))
            candidate["service_network"]["cors_origins"] = [origin]
            self.assertTrue(
                _json_schema_errors(candidate, schema, f"manager config invalid CORS {origin}"),
                origin,
            )

    def test_manager_default_startup_config_orders_and_applies_explicit_list(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        self.assertTrue(
            hasattr(manager_plugin, "_normalize_manager_config"),
            "manager plugin must keep manager config normalization testable",
        )
        self.assertTrue(
            hasattr(manager_plugin, "_order_service_manifests"),
            "manager plugin must keep startup service ordering testable",
        )
        builtin_id = "qcopilots.mcp_server_builtin_tools"
        skills_id = "qcopilots.mcp_server_skills"
        interactive_id = "qcopilots.mcp_server_interactive_tools"
        vector_id = "qcopilots.mcp_server_processing_vector"
        raster_id = "qcopilots.mcp_server_processing_raster"
        manifests = [
            types.SimpleNamespace(service_id=raster_id),
            types.SimpleNamespace(service_id=interactive_id),
            types.SimpleNamespace(service_id=builtin_id),
            types.SimpleNamespace(service_id=vector_id),
            types.SimpleNamespace(service_id=skills_id),
        ]
        default_startup_ids = [builtin_id, interactive_id, vector_id, raster_id, skills_id]
        default_order = [builtin_id, interactive_id, vector_id, raster_id, skills_id]
        explicit_order = [skills_id, builtin_id, raster_id, interactive_id, vector_id]

        class StartupPlugin:
            def __init__(self, running_ids=None, deep_running_ids=None):
                self._default_startup_applied = False
                self.running_ids = set(running_ids or set())
                self.deep_running_ids = set(deep_running_ids if deep_running_ids is not None else self.running_ids)
                self.deep_checks = []
                self.logger = _FakeLogger()

            def service_status(self, manifest, deep=True):
                if deep:
                    self.deep_checks.append(manifest.service_id)
                    running = manifest.service_id in self.deep_running_ids
                else:
                    running = manifest.service_id in self.running_ids
                return _status(running=running, health="ok" if running else "stopped")

        def toggled_ids(config_data, running_ids=None, deep_running_ids=None):
            config = manager_plugin._normalize_manager_config(config_data)
            ordered = manager_plugin._order_service_manifests(
                manifests,
                manager_plugin._startup_service_ids(config),
            )
            cards = [
                _DefaultStartupCard(manifest, manifest.service_id in (running_ids or set()))
                for manifest in ordered
            ]
            dialog = object.__new__(manager_plugin.ManagerDialog)
            dialog.plugin = StartupPlugin(running_ids=running_ids, deep_running_ids=deep_running_ids)
            manager_plugin.ManagerDialog._apply_default_startup(dialog, cards, config)
            return [card.manifest.service_id for card in cards if card.toggle_values == [True]]

        config = manager_plugin._normalize_manager_config({})
        self.assertNotIn("workspace_roots", config)
        self.assertEqual(config["default_startup"]["service_ids"], default_startup_ids)
        self.assertEqual(
            config["service_network"],
            {
                "enabled": False,
                "host": "127.0.0.1",
                "advertised_host": "",
                "cors_origins": [
                    "http://127.0.0.1:8282",
                    "http://localhost:8282",
                ],
            },
        )
        ordered = manager_plugin._order_service_manifests(
            manifests,
            manager_plugin._startup_service_ids(config),
        )
        self.assertEqual([manifest.service_id for manifest in ordered], default_order)
        self.assertEqual(toggled_ids({}), default_startup_ids)
        config = manager_plugin._normalize_manager_config(
            {"default_startup": {"enabled": True, "service_ids": [skills_id, builtin_id]}}
        )
        ordered = manager_plugin._order_service_manifests(
            manifests,
            manager_plugin._startup_service_ids(config),
        )
        self.assertEqual([manifest.service_id for manifest in ordered], explicit_order)
        self.assertEqual(
            toggled_ids({"default_startup": {"enabled": True, "service_ids": [skills_id, builtin_id]}}),
            [skills_id, builtin_id],
        )
        self.assertEqual(
            toggled_ids(
                {
                    "default_startup": {
                        "enabled": True,
                        "service_ids": [builtin_id, skills_id, interactive_id, vector_id, raster_id],
                    }
                }
            ),
            [builtin_id, skills_id, interactive_id, vector_id, raster_id],
        )
        self.assertEqual(
            toggled_ids({"default_startup": {"enabled": False, "service_ids": [builtin_id, skills_id]}}),
            [],
        )
        self.assertEqual(
            toggled_ids({"default_startup": {"enabled": True, "service_ids": []}}),
            [],
        )
        self.assertEqual(
            toggled_ids(
                {"default_startup": {"enabled": True, "service_ids": [builtin_id]}},
                running_ids={builtin_id},
            ),
            [],
        )
        self.assertEqual(
            toggled_ids(
                {"default_startup": {"enabled": True, "service_ids": [builtin_id, skills_id]}},
                running_ids={builtin_id},
            ),
            [skills_id],
        )
        self.assertEqual(
            toggled_ids(
                {"default_startup": {"enabled": True, "service_ids": [builtin_id]}},
                running_ids={builtin_id},
                deep_running_ids=set(),
            ),
            [builtin_id],
        )
        self.assertEqual(
            toggled_ids(
                {
                    "default_startup": {
                        "enabled": True,
                        "service_ids": ["qcopilots.future_service", builtin_id],
                    }
                }
            ),
            [builtin_id],
        )

    def test_manager_normalizes_default_startup_service_ids(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        logger = _FakeLogger()
        config = manager_plugin._normalize_manager_config(
            {
                "default_startup": {
                    "enabled": "false",
                    "service_ids": [
                        "qcopilots.mcp_server_builtin_tools",
                        "",
                        "qcopilots.future_service",
                        "qcopilots.future_service",
                        "../bad",
                        42,
                    ],
                }
            },
            logger,
        )

        self.assertIs(config["default_startup"]["enabled"], True)
        self.assertEqual(
            config["default_startup"]["service_ids"],
            [
                "qcopilots.mcp_server_builtin_tools",
                "qcopilots.future_service",
            ],
        )
        _assert_logger_warning_contains(self, logger, "not a boolean")
        _assert_logger_warning_contains(self, logger, "blank")
        _assert_logger_warning_contains(self, logger, "unsafe")
        _assert_logger_warning_contains(self, logger, "not a string")
        _assert_logger_warning_contains(self, logger, "duplicate")

        warning_count = len(logger.messages)
        config = manager_plugin._normalize_manager_config(
            {"default_startup": {"service_ids": "qcopilots.mcp_server_builtin_tools"}},
            logger,
        )
        self.assertEqual(
            config["default_startup"]["service_ids"],
            DEFAULT_STARTUP_SERVICE_IDS,
        )
        self.assertGreater(len(logger.messages), warning_count)
        _assert_logger_warning_contains(self, logger, "not a list")

        warning_count = len(logger.messages)
        config = manager_plugin._normalize_manager_config({"default_startup": []}, logger)
        self.assertEqual(
            config["default_startup"]["service_ids"],
            DEFAULT_STARTUP_SERVICE_IDS,
        )
        self.assertGreater(len(logger.messages), warning_count)
        _assert_logger_warning_contains(self, logger, "not an object")
        _assert_logger_warning_contains(self, logger, "missing enabled")
        _assert_logger_warning_contains(self, logger, "missing service_ids")

    def test_manager_normalizes_service_network_host_and_origins(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        logger = _FakeLogger()
        config = manager_plugin._normalize_manager_config(
            {
                "service_network": {
                    "enabled": "yes",
                    "host": "8.8.8.8",
                    "advertised_host": "  qgis-client-01.local  ",
                    "cors_origins": [
                        "http://llama-server:8282",
                        "https://example.invalid/path",
                        "::1",
                        "*",
                        42,
                        "http://llama-server:8282",
                    ],
                }
            },
            logger,
        )

        self.assertEqual(
            config["service_network"],
            {
                "enabled": False,
                "host": "127.0.0.1",
                "advertised_host": "qgis-client-01.local",
                "cors_origins": ["http://llama-server:8282", "*"],
            },
        )
        _assert_logger_warning_contains(self, logger, "not a boolean")
        _assert_logger_warning_contains(self, logger, "outside loopback or LAN/private IPv4")
        _assert_logger_warning_contains(self, logger, "invalid")
        _assert_logger_warning_contains(self, logger, "not a string")

    def test_manager_config_load_failures_use_loopback_service_network_fallback(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        with tempfile.TemporaryDirectory() as tmp:
            missing_config = Path(tmp) / "missing.json"
            logger = _FakeLogger()

            self.assertEqual(
                manager_plugin._load_manager_config(missing_config, logger)["service_network"],
                {
                    "enabled": False,
                    "host": "127.0.0.1",
                    "advertised_host": "",
                    "cors_origins": [
                        "http://127.0.0.1:8282",
                        "http://localhost:8282",
                    ],
                },
            )
            _assert_logger_warning_contains(self, logger, "missing")

            bad_config = Path(tmp) / "bad.json"
            bad_config.write_text("{", encoding="utf-8")
            logger = _FakeLogger()

            self.assertEqual(
                manager_plugin._load_manager_config(bad_config, logger)["service_network"]["host"],
                "127.0.0.1",
            )
            _assert_logger_warning_contains(self, logger, "Could not read")

    def test_manager_service_network_overrides_manifest_endpoint(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        with tempfile.TemporaryDirectory() as tmp:
            manifest = _manifest(
                tmp,
                "qcopilots.mcp_server_builtin_tools",
                ["tools"],
            )
            config = manager_plugin._normalize_manager_config(
                {
                    "service_network": {
                        "enabled": True,
                        "host": "0.0.0.0",
                        "advertised_host": "qgis-client-01.local",
                        "cors_origins": ["http://llama-server:8282"],
                    }
                }
            )

            lan_manifest = manager_plugin._network_overrides_manifest(
                manifest,
                config["service_network"],
            )

            self.assertEqual(manifest.host, "127.0.0.1")
            self.assertEqual(lan_manifest.host, "0.0.0.0")
            self.assertEqual(lan_manifest.advertised_host, "qgis-client-01.local")
            self.assertEqual(lan_manifest.url(), "http://qgis-client-01.local:48211/mcp")
            self.assertEqual(lan_manifest.cors_origins, ["http://llama-server:8282"])

            disabled_config = manager_plugin._normalize_manager_config(
                {"service_network": {"enabled": False}}
            )
            self.assertIs(
                manager_plugin._network_overrides_manifest(
                    manifest,
                    disabled_config["service_network"],
                ),
                manifest,
            )
            self.assertIs(
                manager_plugin._network_overrides_manifest(
                    manifest,
                    {},
                ),
                manifest,
            )

    def test_manager_default_service_network_advertises_detected_lan_host(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_local_lan_ipv4_host = manager_plugin._local_lan_ipv4_host
        try:
            with tempfile.TemporaryDirectory() as tmp:
                manager_plugin._local_lan_ipv4_host = lambda: "192.168.1.50"
                manifest = _manifest(
                    tmp,
                    "qcopilots.mcp_server_builtin_tools",
                    ["tools"],
                )
                config = manager_plugin._normalize_manager_config(
                    {"service_network": {"enabled": True}}
                )

                lan_manifest = manager_plugin._network_overrides_manifest(
                    manifest,
                    config["service_network"],
                )

                self.assertEqual(config["service_network"]["host"], "0.0.0.0")
                self.assertEqual(config["service_network"]["advertised_host"], "")
                self.assertEqual(lan_manifest.host, "0.0.0.0")
                self.assertEqual(lan_manifest.advertised_host, "192.168.1.50")
                self.assertEqual(lan_manifest.url(), "http://192.168.1.50:48211/mcp")
                self.assertEqual(lan_manifest.cors_origins, ["*"])
        finally:
            manager_plugin._local_lan_ipv4_host = old_local_lan_ipv4_host

    def test_manager_default_advertised_host_falls_back_when_lan_detection_fails(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_local_lan_ipv4_host = manager_plugin._local_lan_ipv4_host
        old_gethostname = manager_plugin.socket.gethostname
        try:
            manager_plugin._local_lan_ipv4_host = lambda: ""
            manager_plugin.socket.gethostname = lambda: "qgis-client-01"

            self.assertEqual(
                manager_plugin._default_advertised_host("0.0.0.0"),
                "qgis-client-01",
            )

            def raise_gethostname():
                raise OSError("hostname unavailable")

            manager_plugin.socket.gethostname = raise_gethostname
            self.assertEqual(
                manager_plugin._default_advertised_host("0.0.0.0"),
                "127.0.0.1",
            )
        finally:
            manager_plugin._local_lan_ipv4_host = old_local_lan_ipv4_host
            manager_plugin.socket.gethostname = old_gethostname

    def test_manager_detects_lan_ipv4_from_routing_probe_before_hostname(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        class FakeSocket:
            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                del exc_type, exc_value, traceback
                return False

            def connect(self, address):
                self.address = address

            def getsockname(self):
                return ("192.168.1.50", 59312)

        old_socket = manager_plugin.socket.socket
        old_gethostbyname_ex = manager_plugin.socket.gethostbyname_ex
        old_getaddrinfo = manager_plugin.socket.getaddrinfo
        try:
            manager_plugin.socket.socket = lambda *args, **kwargs: FakeSocket()
            manager_plugin.socket.gethostbyname_ex = lambda hostname: (
                hostname,
                [],
                ["127.0.0.1", "192.168.56.1"],
            )
            manager_plugin.socket.getaddrinfo = lambda *args, **kwargs: []

            self.assertEqual(manager_plugin._local_lan_ipv4_host(), "192.168.1.50")
        finally:
            manager_plugin.socket.socket = old_socket
            manager_plugin.socket.gethostbyname_ex = old_gethostbyname_ex
            manager_plugin.socket.getaddrinfo = old_getaddrinfo

    def test_manager_lan_ipv4_detection_prefers_private_over_link_local(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        class FakeSocket:
            probe_index = 0

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                del exc_type, exc_value, traceback
                return False

            def connect(self, address):
                self.address = address

            def getsockname(self):
                FakeSocket.probe_index += 1
                if FakeSocket.probe_index == 1:
                    return ("169.254.10.20", 59312)
                return ("192.168.1.50", 59312)

        old_socket = manager_plugin.socket.socket
        old_gethostbyname_ex = manager_plugin.socket.gethostbyname_ex
        old_getaddrinfo = manager_plugin.socket.getaddrinfo
        try:
            manager_plugin.socket.socket = lambda *args, **kwargs: FakeSocket()
            manager_plugin.socket.gethostbyname_ex = lambda hostname: (hostname, [], [])
            manager_plugin.socket.getaddrinfo = lambda *args, **kwargs: []

            self.assertEqual(manager_plugin._local_lan_ipv4_host(), "192.168.1.50")
        finally:
            manager_plugin.socket.socket = old_socket
            manager_plugin.socket.gethostbyname_ex = old_gethostbyname_ex
            manager_plugin.socket.getaddrinfo = old_getaddrinfo

    def test_manager_rejects_invalid_advertised_host_values(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        logger = _FakeLogger()
        config = manager_plugin._normalize_manager_config(
            {
                "service_network": {
                    "enabled": True,
                    "advertised_host": "http://qgis-client-01.local/mcp",
                }
            },
            logger,
        )

        self.assertEqual(config["service_network"]["advertised_host"], "")
        _assert_logger_warning_contains(self, logger, "advertised_host")

    def test_manager_discovery_applies_service_network_to_all_services(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        service_ids = [
            "qcopilots.mcp_server_builtin_tools",
            "qcopilots.mcp_server_skills",
            "qcopilots.mcp_server_interactive_tools",
            "qcopilots.mcp_server_processing_vector",
            "qcopilots.mcp_server_processing_raster",
        ]
        old_discover = manager_plugin.discover_service_manifests
        try:
            with tempfile.TemporaryDirectory() as tmp:
                plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
                plugin.plugins_root = Path(tmp)
                plugin._manager_config = manager_plugin._normalize_manager_config(
                    {
                        "service_network": {
                            "enabled": True,
                            "host": "0.0.0.0",
                            "advertised_host": "qgis-client-01.local",
                            "cors_origins": ["http://llama-server:8282"],
                        }
                    }
                )
                ports = [48211, 48212, 48213, 48214, 48215]
                manifests = [
                    _manifest(tmp, service_id, ["tools"], port=port)
                    for service_id, port in zip(service_ids, ports, strict=True)
                ]
                manager_plugin.discover_service_manifests = (
                    lambda root, include_disabled=False: manifests
                )

                discovered = plugin._discover_services()

                self.assertEqual([manifest.service_id for manifest in discovered], service_ids)
                self.assertTrue(all(manifest.host == "0.0.0.0" for manifest in discovered))
                self.assertTrue(
                    all(manifest.advertised_host == "qgis-client-01.local" for manifest in discovered)
                )
                self.assertEqual(
                    [manifest.url() for manifest in discovered],
                    [
                        "http://qgis-client-01.local:48211/mcp",
                        "http://qgis-client-01.local:48212/mcp",
                        "http://qgis-client-01.local:48213/mcp",
                        "http://qgis-client-01.local:48214/mcp",
                        "http://qgis-client-01.local:48215/mcp",
                    ],
                )

                plugin._manager_config = manager_plugin._normalize_manager_config(
                    {
                        "service_network": {
                            "enabled": True,
                            "host": "0.0.0.0",
                            "advertised_host": "qgis-client-02.local",
                            "cors_origins": ["http://llama-server:8282"],
                        }
                    }
                )
                self.assertTrue(
                    all(
                        manifest.advertised_host == "qgis-client-02.local"
                        for manifest in plugin._discover_services()
                    )
                )
        finally:
            manager_plugin.discover_service_manifests = old_discover

    def test_manager_discovery_keeps_localhost_manifests_when_service_network_disabled(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_discover = manager_plugin.discover_service_manifests
        try:
            with tempfile.TemporaryDirectory() as tmp:
                plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
                plugin.plugins_root = Path(tmp)
                plugin._manager_config = manager_plugin._normalize_manager_config(
                    {"service_network": {"enabled": False}}
                )
                manifests = [
                    _manifest(tmp, "qcopilots.mcp_server_builtin_tools", ["tools"]),
                    _manifest(tmp, "qcopilots.mcp_server_skills", ["tools"], port=48212),
                ]
                manager_plugin.discover_service_manifests = (
                    lambda root, include_disabled=False: manifests
                )

                discovered = plugin._discover_services()

                self.assertEqual(discovered, manifests)
                self.assertTrue(all(manifest.host == "127.0.0.1" for manifest in discovered))
                self.assertTrue(all(manifest.advertised_host == "" for manifest in discovered))
                self.assertTrue(
                    all(
                        manifest.cors_origins
                        == ["http://127.0.0.1:8282", "http://localhost:8282"]
                        for manifest in discovered
                    )
                )
        finally:
            manager_plugin.discover_service_manifests = old_discover

    def test_manager_config_is_cached_per_plugin_instance(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_bridge = manager_plugin.QgisBridgeController
        old_configure_logger = manager_plugin.configure_logger
        old_core_application = manager_plugin.QCoreApplication
        old_load_manager_config = manager_plugin._load_manager_config
        old_process_controller = manager_plugin.ProcessController
        old_service_log_file = manager_plugin.service_log_file
        try:
            load_calls = []
            cached_config = {
                "default_startup": {
                    "enabled": True,
                    "service_ids": ["qcopilots.mcp_server_skills"],
                }
            }

            def fake_load_manager_config(path, logger):
                load_calls.append((path, logger))
                return cached_config

            manager_plugin.QgisBridgeController = lambda iface, port: types.SimpleNamespace(
                url=f"http://127.0.0.1:{port}"
            )
            manager_plugin.configure_logger = lambda *args, **kwargs: _FakeLogger()
            manager_plugin.QCoreApplication = types.SimpleNamespace(applicationFilePath=lambda: "")
            manager_plugin._load_manager_config = fake_load_manager_config
            manager_plugin.ProcessController = lambda **kwargs: types.SimpleNamespace()
            manager_plugin.service_log_file = lambda service_id: Path(f"{service_id}.log")

            plugin = manager_plugin.QCopilotsMCPServersManagerPlugin(types.SimpleNamespace())
            first_config = plugin.manager_config()
            first_config["default_startup"]["service_ids"].append("qcopilots.mutated")
            second_config = plugin.manager_config()

            self.assertEqual(len(load_calls), 1)
            self.assertEqual(
                second_config["service_network"],
                {
                    "enabled": False,
                    "host": "127.0.0.1",
                    "advertised_host": "",
                    "cors_origins": [
                        "http://127.0.0.1:8282",
                        "http://localhost:8282",
                    ],
                },
            )
            self.assertEqual(
                second_config["default_startup"]["service_ids"],
                ["qcopilots.mcp_server_skills"],
            )
        finally:
            manager_plugin.QgisBridgeController = old_bridge
            manager_plugin.configure_logger = old_configure_logger
            manager_plugin.QCoreApplication = old_core_application
            manager_plugin._load_manager_config = old_load_manager_config
            manager_plugin.ProcessController = old_process_controller
            manager_plugin.service_log_file = old_service_log_file

    def test_manager_ignores_legacy_workspace_root_config(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        config = manager_plugin._normalize_manager_config(
            {
                "workspace_roots": [
                    "C:/Data/QGISData/TestData",
                    42,
                ]
            }
        )

        self.assertNotIn("workspace_roots", config)
        self.assertEqual(
            config["default_startup"]["service_ids"],
            DEFAULT_STARTUP_SERVICE_IDS,
        )

    def test_manager_dialog_populate_services_applies_default_startup(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_service_card = manager_plugin.ServiceCard
        try:
            cards = []

            def fake_service_card(plugin, manifest, expanded=False, parent=None):
                del plugin, parent
                card = _DefaultStartupCard(manifest)
                card.expanded = expanded
                cards.append(card)
                return card

            manager_plugin.ServiceCard = fake_service_card
            plugin = _FakeManagerPlugin()
            plugin._discover_services = lambda include_disabled=False: [
                types.SimpleNamespace(service_id="qcopilots.mcp_server_processing_raster"),
                types.SimpleNamespace(service_id="qcopilots.mcp_server_builtin_tools"),
                types.SimpleNamespace(service_id="qcopilots.mcp_server_skills"),
            ]
            plugin.manager_config = lambda: {
                "default_startup": {
                    "enabled": True,
                    "service_ids": [
                        "qcopilots.mcp_server_builtin_tools",
                        "qcopilots.mcp_server_skills",
                    ],
                }
            }
            dialog = object.__new__(manager_plugin.ManagerDialog)
            dialog.plugin = plugin
            dialog.content_layout = _FakeLayout()
            dialog.footer_label = _FakeLabel()

            manager_plugin.ManagerDialog.populate_services(dialog)

            self.assertEqual(
                [card.manifest.service_id for card in cards],
                [
                    "qcopilots.mcp_server_builtin_tools",
                    "qcopilots.mcp_server_skills",
                    "qcopilots.mcp_server_processing_raster",
                ],
            )
            self.assertEqual([card.expanded for card in cards], [True, False, False])
            self.assertEqual(
                [card.manifest.service_id for card in cards if card.toggle_values == [True]],
                [
                    "qcopilots.mcp_server_builtin_tools",
                    "qcopilots.mcp_server_skills",
                ],
            )
            self.assertTrue(plugin._default_startup_applied)
        finally:
            manager_plugin.ServiceCard = old_service_card

    def test_manifest_rejects_missing_required_fields(self):
        from qcopilots_common.manifest import ManifestValidationError, load_service_manifest

        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = Path(tmp) / "qcopilots_service.json"
            manifest_path.write_text(
                json.dumps({"service_id": "incomplete"}),
                encoding="utf-8",
            )

            with self.assertRaises(ManifestValidationError):
                load_service_manifest(manifest_path)

    def test_discovery_skips_unsafe_service_ids(self):
        from qcopilots_common.discovery import discover_service_manifests
        from qcopilots_common.manifest import ManifestValidationError, load_service_manifest

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_manifest(root / "valid_service", "qcopilots.valid_service", 18701)
            unsafe_manifest_path = _write_manifest(root / "unsafe_service", "../outside", 18702)

            with self.assertRaises(ManifestValidationError):
                load_service_manifest(unsafe_manifest_path)

            discovered = discover_service_manifests(root)
            self.assertEqual(
                [manifest.service_id for manifest in discovered],
                ["qcopilots.valid_service"],
            )

    def test_manifest_local_icon_cannot_escape_plugin_directory(self):
        from qcopilots_common.manifest import load_service_manifest, manifest_to_dict

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            outside_icon = root / "outside.svg"
            outside_icon.write_text("<svg />", encoding="utf-8")
            manifest_path = _write_manifest(
                root / "qcopilots_mcp_server_builtin_tools",
                "qcopilots.mcp_server_builtin_tools",
                18711,
            )
            data = json.loads(manifest_path.read_text(encoding="utf-8"))
            data["icon"] = "../outside.svg"
            manifest_path.write_text(json.dumps(data), encoding="utf-8")

            manifest = load_service_manifest(manifest_path)

            self.assertIsNone(manifest.icon_path)
            self.assertEqual(manifest.icon_descriptors(), [])
            self.assertEqual(manifest_to_dict(manifest)["icon_path"], "")

    def test_manager_service_env_only_injects_bridge_url_for_bridge_services(self):
        _install_manager_import_stubs()

        from qcopilots_common.constants import BRIDGE_URL_ENV
        from qcopilots_mcp_servers_manager.plugin import QCopilotsMCPServersManagerPlugin

        with tempfile.TemporaryDirectory() as tmp:
            plugin = object.__new__(QCopilotsMCPServersManagerPlugin)
            plugin.bridge = types.SimpleNamespace(url="http://127.0.0.1:48200")

            builtin_manifest = _manifest(
                tmp,
                "qcopilots.mcp_server_builtin_tools",
                ["tools"],
            )
            interactive_manifest = _manifest(
                tmp,
                "qcopilots.mcp_server_interactive_tools",
                ["tools", "qgis-bridge"],
            )
            processing_manifest = _manifest(
                tmp,
                "qcopilots.mcp_server_processing_vector",
                ["tools", "processing", "vector"],
            )

            self.assertEqual(plugin._service_env(builtin_manifest), {})
            self.assertEqual(
                plugin._service_env(interactive_manifest),
                {BRIDGE_URL_ENV: "http://127.0.0.1:48200"},
            )
            self.assertEqual(
                plugin._service_env(processing_manifest),
                {BRIDGE_URL_ENV: "http://127.0.0.1:48200"},
            )

    def test_manager_service_env_does_not_inject_workspace_settings(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin
        from qcopilots_common.constants import BRIDGE_URL_ENV

        with tempfile.TemporaryDirectory() as tmp:
            plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
            plugin.bridge = types.SimpleNamespace(url="http://127.0.0.1:48200")
            manifest = _manifest(
                tmp,
                "qcopilots.mcp_server_interactive_tools",
                ["tools", "qgis-bridge"],
            )

            env = plugin._service_env(manifest)

            self.assertEqual(env, {BRIDGE_URL_ENV: "http://127.0.0.1:48200"})

    def test_manager_start_service_only_passes_bridge_url_to_bridge_services(self):
        _install_manager_import_stubs()

        from qcopilots_common.constants import BRIDGE_URL_ENV
        from qcopilots_mcp_servers_manager.plugin import QCopilotsMCPServersManagerPlugin

        with tempfile.TemporaryDirectory() as tmp:
            plugin = object.__new__(QCopilotsMCPServersManagerPlugin)
            plugin.bridge = types.SimpleNamespace(url="http://127.0.0.1:48200")
            plugin.controller = _FakeProcessController()

            builtin_manifest = _manifest(
                tmp,
                "qcopilots.mcp_server_builtin_tools",
                ["tools"],
            )
            plugin.start_service(builtin_manifest)
            self.assertIsNone(plugin.controller.bridge_url)
            self.assertEqual(plugin.controller.extra_env, {})

            processing_manifest = _manifest(
                tmp,
                "qcopilots.mcp_server_processing_vector",
                ["tools", "processing"],
            )
            plugin.start_service(processing_manifest)
            self.assertEqual(plugin.controller.bridge_url, "http://127.0.0.1:48200")
            self.assertEqual(
                plugin.controller.extra_env,
                {BRIDGE_URL_ENV: "http://127.0.0.1:48200"},
            )

    def test_manager_source_has_no_auth_or_workspace_environment_helpers(self):
        plugin_source = (
            Path(__file__).resolve().parents[3]
            / "python"
            / "plugins"
            / "qcopilots_mcp_servers_manager"
            / "plugin.py"
        ).read_text(encoding="utf-8")

        self.assertNotIn("_local_unauthenticated_mcp_ok", plugin_source)
        self.assertNotIn("auth" + "_token", plugin_source)
        self.assertNotIn("mcp_" + "auth" + "_token", plugin_source)
        self.assertNotIn("WORKSPACE" + "_ROOTS_ENV", plugin_source)

    def test_qgis_bridge_allows_external_paths(self):
        _install_manager_import_stubs()

        from qcopilots_common.bridge import QgisBridgeTools

        with tempfile.TemporaryDirectory() as outside_tmp:
            outside = Path(outside_tmp).resolve()
            tools = QgisBridgeTools(None)

            self.assertEqual(
                tools._safe_workspace_path(str(outside / "escape.shp")),
                str((outside / "escape.shp").resolve()),
            )

    def test_qgis_bridge_default_allows_external_paths_without_extra_environment(self):
        _install_manager_import_stubs()

        from qcopilots_common.bridge import QgisBridgeTools

        with tempfile.TemporaryDirectory() as outside_tmp:
            outside = Path(outside_tmp).resolve()
            tools = QgisBridgeTools(None)

            self.assertEqual(
                tools._safe_workspace_path(str(outside / "escape.shp")),
                str((outside / "escape.shp").resolve()),
            )

    def test_manager_card_exposes_only_user_facing_service_fields(self):
        plugin_source = (
            Path(__file__).resolve().parents[3]
            / "python"
            / "plugins"
            / "qcopilots_mcp_servers_manager"
            / "plugin.py"
        ).read_text(encoding="utf-8")

        self.assertIn("class ServiceCard", plugin_source)
        self.assertIn("self.name_label = QLabel(manifest.plugin_name", plugin_source)
        self.assertIn("self.inline_endpoint_label", plugin_source)
        self.assertIn("self.endpoint_label", plugin_source)
        self.assertIn("self.port_label", plugin_source)
        self.assertIn("self.running_label", plugin_source)
        self.assertIn("qcopilots_service_settings_placeholder", plugin_source)
        self.assertIn("def copy_endpoint_url", plugin_source)
        self.assertIn("qcopilots_endpoint_copy_button", plugin_source)
        self.assertIn("Endpoint URL copied", plugin_source)
        self.assertIn('button.setText("✓")', plugin_source)
        self.assertIn("button.setEnabled(False)", plugin_source)
        self.assertIn("button.setEnabled(True)", plugin_source)
        self.assertIn("self.copy_feedback_timer.start(1200)", plugin_source)
        self.assertNotIn("mcp_" + "auth" + "_token", plugin_source)
        self.assertIn("def _running_text", plugin_source)
        self.assertIn("expanded=index == 0", plugin_source)
        self.assertNotIn("self.auth_header_label", plugin_source)
        self.assertNotIn("def copy_auth_header", plugin_source)
        self.assertNotIn("qcopilots_auth_header_copy_button", plugin_source)
        self.assertNotIn("Auth header copied", plugin_source)
        self.assertNotIn("Copy MCP auth header", plugin_source)
        self.assertNotIn("AUTH" + "_HEADER_NAME", plugin_source)
        self.assertNotIn("processes:", plugin_source)
        self.assertNotIn("owner = ", plugin_source)
        self.assertNotIn("self.status.log_file", plugin_source)
        self.assertNotIn("runtime =", plugin_source)

    def test_manager_dialog_has_no_refresh_all_button_or_handler(self):
        plugin_source = (
            Path(__file__).resolve().parents[3]
            / "python"
            / "plugins"
            / "qcopilots_mcp_servers_manager"
            / "plugin.py"
        ).read_text(encoding="utf-8")

        self.assertNotIn("Refresh All", plugin_source)
        self.assertNotIn("refresh_button", plugin_source)
        self.assertNotIn("qcopilots_mcp_servers_refresh_all", plugin_source)
        self.assertNotIn("def refresh", plugin_source)
        self.assertIn("def populate_services", plugin_source)
        tree = ast.parse(plugin_source)
        manager_dialog = next(
            node
            for node in tree.body
            if isinstance(node, ast.ClassDef) and node.name == "ManagerDialog"
        )
        self.assertFalse(
            any(isinstance(node, ast.FunctionDef) and node.name == "refresh" for node in manager_dialog.body)
        )
        self.assertFalse(
            any(
                isinstance(node, ast.Name) and node.id == "QPushButton"
                for node in ast.walk(manager_dialog)
            )
        )
        self.assertFalse(
            any(
                isinstance(target, ast.Attribute) and target.attr == "refresh_button"
                for node in ast.walk(manager_dialog)
                if isinstance(node, ast.Assign)
                for target in node.targets
            )
        )

    def test_manager_copy_endpoint_feedback_changes_and_restores_button(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_application = manager_plugin.QApplication
        try:
            manager_plugin.QApplication = _FakeApplication
            _FakeApplication.clipboard_text = ""
            card = object.__new__(manager_plugin.ServiceCard)
            card.plugin = types.SimpleNamespace(tr=lambda message: message)
            card.status = _status(url="http://127.0.0.1:48211/mcp")
            card.copy_button = _FakeButton("⧉", "Copy endpoint URL")
            card.copy_feedback_timer = _FakeTimer()
            card.copy_feedback_timer.connect(card._restore_copy_button)
            card._copy_restore_text = ""
            card._copy_restore_tooltip = ""
            card._copy_feedback_button = None

            card.copy_endpoint_url()

            self.assertEqual(_FakeApplication.clipboard_text, "http://127.0.0.1:48211/mcp")
            self.assertEqual(card.copy_button.text(), "✓")
            self.assertEqual(card.copy_button.toolTip(), "Endpoint URL copied")
            self.assertFalse(card.copy_button.enabled)
            self.assertEqual(card.copy_feedback_timer.interval, 1200)

            card.copy_feedback_timer.fire()

            self.assertEqual(card.copy_button.text(), "⧉")
            self.assertEqual(card.copy_button.toolTip(), "Copy endpoint URL")
            self.assertTrue(card.copy_button.enabled)
        finally:
            manager_plugin.QApplication = old_application

    def test_manager_card_does_not_expose_auth_header_helpers(self):
        plugin_source = (
            Path(__file__).resolve().parents[3]
            / "python"
            / "plugins"
            / "qcopilots_mcp_servers_manager"
            / "plugin.py"
        ).read_text(encoding="utf-8")

        self.assertNotIn("auth_header_copy_button", plugin_source)
        self.assertNotIn("copy_auth_header", plugin_source)
        self.assertNotIn("_auth_header_value", plugin_source)
        self.assertNotIn("_masked_auth_header", plugin_source)
        self.assertNotIn("X-QCopilots" + "-Token:", plugin_source)

    def test_manager_toggle_worker_stops_unhealthy_start(self):
        _install_manager_import_stubs()

        from qcopilots_mcp_servers_manager.plugin import ServiceToggleWorker

        manifest = types.SimpleNamespace(service_id="qcopilots.test")
        unhealthy = _status(running=True, health="starting")
        stopped = _status(running=False, health="stopped")
        controller = _FakeProcessController(start_status=unhealthy, stop_status=stopped)
        worker = _worker(ServiceToggleWorker, controller, manifest, enabled=True, was_running=False)
        worker.finished = _CaptureSignal()
        worker.failed = _CaptureSignal()

        worker.run()

        self.assertEqual(controller.calls, ["start", "stop"])
        self.assertEqual(worker.finished.emissions, [])
        self.assertEqual(worker.failed.emissions[0], ("Service did not become healthy", stopped))

    def test_manager_toggle_worker_emits_finished_for_successful_start_and_stop(self):
        _install_manager_import_stubs()

        from qcopilots_mcp_servers_manager.plugin import ServiceToggleWorker

        manifest = types.SimpleNamespace(service_id="qcopilots.test")
        running = _status(running=True, health="ok")
        stopped = _status(running=False, health="stopped")
        start_controller = _FakeProcessController(start_status=running)
        start_worker = _worker(ServiceToggleWorker, start_controller, manifest, enabled=True, was_running=False)
        start_worker.finished = _CaptureSignal()
        start_worker.failed = _CaptureSignal()

        start_worker.run()

        self.assertEqual(start_controller.calls, ["start"])
        self.assertEqual(start_worker.finished.emissions[0], (running,))
        self.assertEqual(start_worker.failed.emissions, [])

        stop_controller = _FakeProcessController(stop_status=stopped)
        stop_worker = _worker(ServiceToggleWorker, stop_controller, manifest, enabled=False, was_running=True)
        stop_worker.finished = _CaptureSignal()
        stop_worker.failed = _CaptureSignal()

        stop_worker.run()

        self.assertEqual(stop_controller.calls, ["stop"])
        self.assertEqual(stop_worker.finished.emissions[0], (stopped,))
        self.assertEqual(stop_worker.failed.emissions, [])

    def test_manager_toggle_worker_stops_started_service_when_shutdown_requested(self):
        _install_manager_import_stubs()

        from qcopilots_mcp_servers_manager.plugin import ServiceToggleWorker

        manifest = types.SimpleNamespace(service_id="qcopilots.test")
        running = _status(running=True, health="ok")
        stopped = _status(running=False, health="stopped")
        controller = _FakeProcessController(start_status=running, stop_status=stopped)
        worker = ServiceToggleWorker(
            controller,
            _FakeLogger(),
            manifest,
            True,
            False,
            "http://127.0.0.1:48200",
            {"QCOPILOTS_TEST_ENV": "1"},
            "Service did not become healthy",
            "Unknown service state",
            shutdown_requested=lambda: True,
        )
        worker.finished = _CaptureSignal()
        worker.failed = _CaptureSignal()

        worker.run()

        self.assertEqual(controller.calls, ["start", "stop"])
        self.assertEqual(worker.finished.emissions[0], (stopped,))
        self.assertEqual(worker.failed.emissions, [])

    def test_manager_toggle_worker_reports_stop_failure_with_live_status(self):
        _install_manager_import_stubs()

        from qcopilots_mcp_servers_manager.plugin import ServiceToggleWorker

        manifest = types.SimpleNamespace(service_id="qcopilots.test")
        owner_mismatch = _status(running=True, health="owner-mismatch")
        controller = _FakeProcessController(stop_status=owner_mismatch)
        worker = _worker(ServiceToggleWorker, controller, manifest, enabled=False, was_running=True)
        worker.finished = _CaptureSignal()
        worker.failed = _CaptureSignal()

        worker.run()

        self.assertEqual(controller.calls, ["stop"])
        self.assertEqual(worker.finished.emissions, [])
        self.assertEqual(worker.failed.emissions[0], ("owner-mismatch", owner_mismatch))

    def test_manager_toggle_worker_reports_exception_with_current_status(self):
        _install_manager_import_stubs()

        from qcopilots_mcp_servers_manager.plugin import ServiceToggleWorker

        manifest = types.SimpleNamespace(service_id="qcopilots.test")
        stopped = _status(running=False, health="stopped")
        controller = _FakeProcessController(status=stopped, start_error=RuntimeError("boom"))
        worker = _worker(ServiceToggleWorker, controller, manifest, enabled=True, was_running=False)
        worker.finished = _CaptureSignal()
        worker.failed = _CaptureSignal()

        worker.run()

        self.assertEqual(controller.calls, ["start", "status"])
        self.assertEqual(worker.finished.emissions, [])
        self.assertEqual(worker.failed.emissions[0], ("boom", stopped))

    def test_manager_toggle_service_starts_worker_without_qgis_api_in_thread(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_thread = manager_plugin.QThread
        old_worker = manager_plugin.ServiceToggleWorker
        try:
            manager_plugin.QThread = _DeferredFakeThread
            manager_plugin.ServiceToggleWorker = _DeferredFakeServiceToggleWorker
            card = _fake_service_card(manager_plugin, running=False)
            card.plugin = _FakeManagerPlugin()
            card.plugin.status = _status(running=False, health="stopped")
            card.toggle_service(True)

            self.assertFalse(card.switch.enabled)
            self.assertEqual(card.running_label.text(), "Starting...")
            self.assertEqual(card.running_label.toolTip(), "Starting...")
            self.assertIsInstance(card.toggle_thread, _DeferredFakeThread)
            self.assertIsNotNone(card.toggle_worker)
            self.assertIs(card.toggle_worker.controller, card.plugin.controller)
            self.assertEqual(card.toggle_worker.extra_env["QCOPILOTS_TEST_ENV"], "1")
            self.assertEqual(card.plugin.calls, ["service_env"])
        finally:
            manager_plugin.QThread = old_thread
            manager_plugin.ServiceToggleWorker = old_worker

    def test_manager_toggle_service_env_failure_uses_lightweight_status(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        class FailingEnvPlugin(_FakeManagerPlugin):
            def _service_env(self, manifest):
                del manifest
                self.calls.append("service_env")
                raise RuntimeError("env failed")

        card = _fake_service_card(manager_plugin, running=False)
        card.plugin = FailingEnvPlugin(status=_status(running=False, health="stopped"))

        card.toggle_service(True)

        self.assertEqual(card.plugin.calls, ["service_env", "status"])
        self.assertEqual(card.plugin.controller.calls, ["status"])
        self.assertEqual(card.plugin.controller.status_deep_values, [False])
        self.assertIsNone(card.toggle_thread)
        self.assertTrue(card.switch.enabled)
        self.assertFalse(card.switch.checked)
        self.assertEqual(card.running_label.text(), "Start failed")
        self.assertEqual(card.running_label.toolTip(), "env failed")

    def test_manager_toggle_service_stops_worker_without_service_env(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_thread = manager_plugin.QThread
        old_worker = manager_plugin.ServiceToggleWorker
        try:
            manager_plugin.QThread = _DeferredFakeThread
            manager_plugin.ServiceToggleWorker = _DeferredFakeServiceToggleWorker
            card = _fake_service_card(manager_plugin, running=True)
            card.toggle_service(False)

            self.assertFalse(card.switch.enabled)
            self.assertEqual(card.running_label.text(), "Stopping...")
            self.assertEqual(card.running_label.toolTip(), "Stopping...")
            self.assertIsInstance(card.toggle_thread, _DeferredFakeThread)
            self.assertFalse(card.toggle_worker.enabled)
            self.assertTrue(card.toggle_worker.was_running)
            self.assertEqual(card.toggle_worker.extra_env, {})
            self.assertEqual(card.plugin.calls, [])
        finally:
            manager_plugin.QThread = old_thread
            manager_plugin.ServiceToggleWorker = old_worker

    def test_manager_dialog_populate_services_captures_only_first_card_expanded(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_service_card = manager_plugin.ServiceCard
        try:
            captured = []

            def fake_service_card(plugin, manifest, expanded=False, parent=None):
                del plugin, parent
                captured.append((manifest.service_id, expanded))
                return _FakeWidgetForLayout()

            manager_plugin.ServiceCard = fake_service_card
            dialog = object.__new__(manager_plugin.ManagerDialog)
            dialog.plugin = _FakeManagerPlugin()
            dialog.content_layout = _FakeLayout()
            dialog.footer_label = _FakeLabel()

            manager_plugin.ManagerDialog.populate_services(dialog)

            self.assertEqual(
                captured,
                [
                    ("qcopilots.first", True),
                    ("qcopilots.second", False),
                    ("qcopilots.third", False),
                ],
            )
        finally:
            manager_plugin.ServiceCard = old_service_card

    def test_manager_dialog_populate_services_only_reads_service_status(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_service_card = manager_plugin.ServiceCard
        try:
            captured = []

            def fake_service_card(plugin, manifest, expanded=False, parent=None):
                del expanded, parent
                captured.append((manifest.service_id, plugin.service_status(manifest, deep=False).running))
                return _FakeWidgetForLayout()

            manager_plugin.ServiceCard = fake_service_card
            plugin = _FakeManagerPlugin(status=_status(running=True, health="ok"))
            dialog = object.__new__(manager_plugin.ManagerDialog)
            dialog.plugin = plugin
            dialog.content_layout = _FakeLayout()
            dialog.footer_label = _FakeLabel()

            manager_plugin.ManagerDialog.populate_services(dialog)

            self.assertEqual(
                captured,
                [
                    ("qcopilots.first", True),
                    ("qcopilots.second", True),
                    ("qcopilots.third", True),
                ],
            )
            self.assertEqual(plugin.calls, ["status", "status", "status"])
            self.assertEqual(plugin.controller.calls, ["status", "status", "status"])
            self.assertEqual(plugin.controller.status_deep_values, [False, False, False])
        finally:
            manager_plugin.ServiceCard = old_service_card

    def test_manager_service_status_only_reads_controller_status(self):
        _install_manager_import_stubs()

        from qcopilots_mcp_servers_manager.plugin import QCopilotsMCPServersManagerPlugin

        plugin = object.__new__(QCopilotsMCPServersManagerPlugin)
        plugin.controller = _FakeProcessController(status=_status(running=True, health="ok"))
        manifest = types.SimpleNamespace(service_id="qcopilots.test")

        status = QCopilotsMCPServersManagerPlugin.service_status(plugin, manifest)

        self.assertTrue(status.running)
        self.assertEqual(plugin.controller.calls, ["status"])
        self.assertEqual(plugin.controller.status_deep_values, [True])

    def test_manager_service_card_initial_status_uses_lightweight_probe(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        class StopAfterStatus(RuntimeError):
            pass

        class FakePlugin(_FakeManagerPlugin):
            def service_status(self, manifest, deep=True):
                status = super().service_status(manifest, deep=deep)
                raise StopAfterStatus(status)

        plugin = FakePlugin(status=_status(running=True, health="running"))
        manifest = types.SimpleNamespace(
            service_id="qcopilots.test",
            plugin_name="QCopilots Test",
            description="Test service",
        )

        with self.assertRaises(StopAfterStatus):
            manager_plugin.ServiceCard(plugin, manifest)

        self.assertEqual(plugin.calls, ["status"])
        self.assertEqual(plugin.controller.calls, ["status"])
        self.assertEqual(plugin.controller.status_deep_values, [False])

    def test_manager_run_uses_internal_populate_services_without_refresh_button(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_dialog = manager_plugin.ManagerDialog
        try:
            created = []

            class FakeManagerDialog:
                def __init__(self, parent, plugin):
                    self.parent = parent
                    self.plugin = plugin
                    self.populate_count = 0
                    self.show_count = 0
                    self.raise_count = 0
                    self.activate_count = 0
                    created.append(self)

                def populate_services(self):
                    self.populate_count += 1

                def show(self):
                    self.show_count += 1

                def raise_(self):
                    self.raise_count += 1

                def activateWindow(self):
                    self.activate_count += 1

            manager_plugin.ManagerDialog = FakeManagerDialog
            plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
            plugin.dialog = None
            plugin.iface = types.SimpleNamespace(mainWindow=lambda: "main-window")
            plugin.controller = _FakeProcessController()

            manager_plugin.QCopilotsMCPServersManagerPlugin.run(plugin)
            manager_plugin.QCopilotsMCPServersManagerPlugin.run(plugin)

            self.assertEqual(len(created), 1)
            self.assertIs(plugin.dialog, created[0])
            self.assertFalse(hasattr(plugin.dialog, "refresh_button"))
            self.assertFalse(hasattr(plugin.dialog, "refresh"))
            self.assertEqual(plugin.dialog.populate_count, 2)
            self.assertEqual(plugin.dialog.show_count, 2)
            self.assertEqual(plugin.dialog.raise_count, 2)
            self.assertEqual(plugin.dialog.activate_count, 2)
            self.assertEqual(plugin.controller.calls, [])
        finally:
            manager_plugin.ManagerDialog = old_dialog

    def test_manager_unload_waits_for_toggle_action_before_teardown(self):
        _install_manager_import_stubs()

        from qcopilots_mcp_servers_manager.plugin import QCopilotsMCPServersManagerPlugin

        plugin = object.__new__(QCopilotsMCPServersManagerPlugin)
        dialog = _FakeDialog(prepare_close_result=True)
        plugin.dialog = dialog
        plugin.logger = _FakeLogger()
        plugin.action = None
        plugin.iface = _FakeIface()
        plugin.bridge = _FakeBridge()
        plugin.controller = _RecordingController()
        plugin._discover_services = lambda include_disabled=False: [
            types.SimpleNamespace(service_id="qcopilots.first"),
            types.SimpleNamespace(service_id="qcopilots.second"),
        ]

        QCopilotsMCPServersManagerPlugin.unload(plugin)

        self.assertEqual(dialog.prepare_close_wait_values, [True])
        self.assertTrue(dialog.closed)
        self.assertTrue(dialog.deleted)
        self.assertIsNone(plugin.dialog)
        self.assertEqual(plugin.controller.stopped_service_ids, ["qcopilots.first", "qcopilots.second"])
        self.assertTrue(plugin.bridge.stopped)
        self.assertEqual(plugin.bridge.stop_count, 1)

    def test_manager_about_to_quit_hook_shuts_down_services(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_core_application = manager_plugin.QCoreApplication
        app = types.SimpleNamespace(aboutToQuit=_CaptureSignal())

        class FakeCoreApplication:
            @staticmethod
            def instance():
                return app

        try:
            manager_plugin.QCoreApplication = FakeCoreApplication
            plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
            plugin._about_to_quit_connected = False
            plugin._shutdown_started = False
            plugin.dialog = _FakeDialog()
            plugin.logger = _FakeLogger()
            plugin.bridge = _FakeBridge()
            plugin.controller = _RecordingController()
            plugin._discover_services = lambda include_disabled=False: [
                types.SimpleNamespace(service_id="qcopilots.first"),
                types.SimpleNamespace(service_id="qcopilots.second"),
            ]

            plugin._connect_about_to_quit()
            app.aboutToQuit.emit()
            app.aboutToQuit.emit()

            self.assertTrue(plugin._about_to_quit_connected)
            self.assertEqual(plugin.dialog.prepare_close_wait_values, [True])
            self.assertEqual(plugin.controller.stopped_service_ids, ["qcopilots.first", "qcopilots.second"])
            self.assertEqual(plugin.bridge.stop_count, 1)
        finally:
            manager_plugin.QCoreApplication = old_core_application

    def test_manager_about_to_quit_then_unload_disconnects_and_stays_idempotent(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_core_application = manager_plugin.QCoreApplication
        app = types.SimpleNamespace(aboutToQuit=_CaptureSignal())

        class FakeCoreApplication:
            @staticmethod
            def instance():
                return app

        try:
            manager_plugin.QCoreApplication = FakeCoreApplication
            plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
            plugin._about_to_quit_connected = False
            plugin._shutdown_started = False
            plugin.dialog = _FakeDialog()
            plugin.logger = _FakeLogger()
            plugin.action = None
            plugin.iface = _FakeIface()
            plugin.bridge = _FakeBridge()
            plugin.controller = _RecordingController()
            plugin._discover_services = lambda include_disabled=False: [
                types.SimpleNamespace(service_id="qcopilots.first")
            ]

            plugin._connect_about_to_quit()
            app.aboutToQuit.emit()
            manager_plugin.QCopilotsMCPServersManagerPlugin.unload(plugin)
            app.aboutToQuit.emit()

            self.assertFalse(plugin._about_to_quit_connected)
            self.assertEqual(plugin.controller.stopped_service_ids, ["qcopilots.first"])
            self.assertEqual(plugin.bridge.stop_count, 1)
            self.assertTrue(plugin.dialog is None)
        finally:
            manager_plugin.QCoreApplication = old_core_application

    def test_manager_shutdown_continues_after_service_stop_failure(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
        plugin._shutdown_started = False
        plugin.dialog = _FakeDialog()
        plugin.logger = _FakeLogger()
        plugin.bridge = _FakeBridge()
        plugin.controller = _RecordingController(fail_service_ids={"qcopilots.first"})
        plugin._discover_services = lambda include_disabled=False: [
            types.SimpleNamespace(service_id="qcopilots.first"),
            types.SimpleNamespace(service_id="qcopilots.second"),
        ]

        plugin._shutdown_services()

        self.assertEqual(plugin.dialog.prepare_close_wait_values, [True])
        self.assertEqual(plugin.controller.stopped_service_ids, ["qcopilots.first", "qcopilots.second"])
        self.assertEqual(plugin.bridge.stop_count, 1)
        self.assertEqual(plugin.logger.messages[0][0], "Failed to stop %s: %s")
        self.assertEqual(plugin.logger.messages[0][1][0], "qcopilots.first")
        self.assertEqual(str(plugin.logger.messages[0][1][1]), "stop failed")

    def test_manager_shutdown_continues_cleanup_when_toggle_action_is_still_running(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
        plugin._shutdown_started = False
        plugin._shutdown_completed = False
        plugin.dialog = _FakeDialog(prepare_close_result=False)
        plugin.logger = _FakeLogger()
        plugin.bridge = _FakeBridge()
        plugin.controller = _RecordingController()
        plugin._discover_services = lambda include_disabled=False: [
            types.SimpleNamespace(service_id="qcopilots.first")
        ]

        plugin._shutdown_services()

        self.assertTrue(plugin._shutdown_started)
        self.assertTrue(plugin._shutdown_completed)
        self.assertEqual(plugin.dialog.prepare_close_wait_values, [True])
        self.assertEqual(plugin.controller.stopped_service_ids, ["qcopilots.first"])
        self.assertEqual(plugin.bridge.stop_count, 1)
        self.assertEqual(
            plugin.logger.messages[1],
            ("Continuing QCopilots MCP shutdown cleanup with a running service action", ()),
        )

    def test_manager_shutdown_uses_stored_manifests_when_discovery_misses_service(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        plugin = object.__new__(manager_plugin.QCopilotsMCPServersManagerPlugin)
        plugin._shutdown_started = False
        plugin._shutdown_completed = False
        plugin.dialog = _FakeDialog()
        plugin.logger = _FakeLogger()
        plugin.bridge = _FakeBridge()
        plugin.controller = _RecordingController(
            stored_manifests=[types.SimpleNamespace(service_id="qcopilots.stored")]
        )
        plugin._discover_services = lambda include_disabled=False: []

        plugin._shutdown_services()

        self.assertEqual(plugin.controller.stopped_service_ids, ["qcopilots.stored"])
        self.assertEqual(plugin.bridge.stop_count, 1)
        self.assertTrue(plugin._shutdown_completed)

    def test_manager_cleanup_toggle_thread_waits_only_on_unload_path(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        card = object.__new__(manager_plugin.ServiceCard)
        thread = _DeferredFakeThread()
        thread.running = True
        card.toggle_thread = thread
        card.toggle_worker = object()
        cleanup = types.MethodType(manager_plugin.ServiceCard.cleanup_toggle_thread, card)

        self.assertFalse(cleanup(wait=False))
        self.assertIs(card.toggle_thread, thread)

        self.assertTrue(cleanup(wait=True))
        self.assertEqual(thread.quit_count, 1)
        self.assertEqual(thread.wait_count, 1)
        self.assertIsNone(card.toggle_thread)
        self.assertIsNone(card.toggle_worker)

    def test_manager_cleanup_toggle_thread_timeout_keeps_running_references(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        card = object.__new__(manager_plugin.ServiceCard)
        thread = _DeferredFakeThread()
        thread.running = True
        thread.wait_result = False
        thread.emit_finished_on_quit = False
        worker = object()
        card.plugin = _FakeManagerPlugin()
        card.manifest = types.SimpleNamespace(service_id="qcopilots.test")
        card.toggle_thread = thread
        card.toggle_worker = worker
        cleanup = types.MethodType(manager_plugin.ServiceCard.cleanup_toggle_thread, card)

        self.assertFalse(cleanup(wait=True))
        self.assertEqual(thread.quit_count, 1)
        self.assertEqual(thread.wait_args, [(manager_plugin.TOGGLE_THREAD_WAIT_TIMEOUT_MS,)])
        self.assertIs(card.toggle_thread, thread)
        self.assertIs(card.toggle_worker, worker)
        self.assertEqual(
            card.plugin.logger.messages[0],
            ("Timed out waiting for %s MCP server action to finish", ("qcopilots.test",)),
        )

    def test_manager_worker_teardown_uses_stable_thread_and_worker_references(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        old_thread = manager_plugin.QThread
        old_worker = manager_plugin.ServiceToggleWorker
        try:
            manager_plugin.QThread = _DeferredFakeThread
            manager_plugin.ServiceToggleWorker = _DeferredFakeServiceToggleWorker
            card = _fake_service_card(manager_plugin, running=False)
            card.plugin = _FakeManagerPlugin()
            card.toggle_service(True)
            thread = card.toggle_thread
            worker = card.toggle_worker

            self.assertTrue(card.cleanup_toggle_thread(wait=True))
            self.assertIsNone(card.toggle_thread)
            self.assertIsNone(card.toggle_worker)

            worker.finished.emit(_status(running=True, health="ok"))

            self.assertEqual(thread.quit_count, 2)
            self.assertTrue(worker.deleted)
        finally:
            manager_plugin.QThread = old_thread
            manager_plugin.ServiceToggleWorker = old_worker

    def test_manager_failed_toggle_feedback_rolls_switch_to_real_status(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        card = object.__new__(manager_plugin.ServiceCard)
        card.plugin = _FakeManagerPlugin(status=_status(running=False, health="stopped"))
        card.manifest = types.SimpleNamespace(service_id="qcopilots.test")
        card.status = _status(running=False, health="stopped")
        card.switch = _FakeSwitch()
        card.port_label = _FakeLabel()
        card.running_label = _FakeLabel()
        card.inline_endpoint_label = _FakeLabel()
        card.endpoint_label = _FakeLabel()
        card.status_feedback_timer = _FakeTimer()
        card.status_feedback_timer.connect(card.update_status_widgets)
        card._update_card_style = lambda: None
        live_status = _status(running=True, health="owner-mismatch")

        card._fail_toggle_service("Stop failed", "owner-mismatch", live_status)

        self.assertTrue(card.switch.enabled)
        self.assertTrue(card.switch.checked)
        self.assertEqual(card.running_label.text(), "Stop failed")
        self.assertEqual(card.running_label.toolTip(), "owner-mismatch")
        self.assertEqual(card.status_feedback_timer.interval, 1600)

        card.status_feedback_timer.fire()

        self.assertEqual(card.running_label.text(), "Running")
        self.assertEqual(card.running_label.toolTip(), "")

    def test_manager_finished_start_stops_service_when_shutdown_is_pending(self):
        _install_manager_import_stubs()

        import qcopilots_mcp_servers_manager.plugin as manager_plugin

        card = object.__new__(manager_plugin.ServiceCard)
        card.plugin = _FakeManagerPlugin(stop_status=_status(running=False, health="stopped"))
        card.plugin._shutdown_started = True
        card.manifest = types.SimpleNamespace(service_id="qcopilots.test")
        card.status = _status(running=False, health="stopped")
        card.switch = _FakeSwitch()
        card.port_label = _FakeLabel()
        card.running_label = _FakeLabel()
        card.inline_endpoint_label = _FakeLabel()
        card.endpoint_label = _FakeLabel()
        card.status_feedback_timer = _FakeTimer()
        card._update_card_style = lambda: None

        card._finish_toggle_service(True, "Start failed", _status(running=True, health="ok"))

        self.assertEqual(card.plugin.calls, ["stop_service"])
        self.assertFalse(card.status.running)
        self.assertTrue(card.switch.enabled)
        self.assertFalse(card.switch.checked)
        self.assertEqual(card.running_label.text(), "Stopped")

    def test_common_plugin_has_metadata_and_noop_wrapper(self):
        common_root = Path(__file__).resolve().parents[3] / "python" / "plugins" / "qcopilots_common"
        metadata = (common_root / "metadata.txt").read_text(encoding="utf-8")
        init_py = (common_root / "__init__.py").read_text(encoding="utf-8")
        plugin_py = (common_root / "plugin.py").read_text(encoding="utf-8")
        cmake = (common_root / "CMakeLists.txt").read_text(encoding="utf-8")

        self.assertIn("name=QCopilots Common", metadata)
        self.assertIn("icon=icon.svg", metadata)
        self.assertTrue((common_root / "icon.svg").is_file())
        self.assertIn("classFactory", init_py)
        self.assertIn("QCopilotsCommonPlugin", plugin_py)
        self.assertIn("SVG_FILES", cmake)
        self.assertIn("TXT_FILES", cmake)

    def test_qcopilots_plugins_declare_installable_icons(self):
        plugins_root = Path(__file__).resolve().parents[3] / "python" / "plugins"
        plugins_cmake = (plugins_root / "CMakeLists.txt").read_text(encoding="utf-8")
        plugin_dirs = [
            "qcopilots_common",
            "qcopilots_mcp_servers_manager",
            "qcopilots_mcp_server_builtin_tools",
            "qcopilots_mcp_server_interactive_tools",
            "qcopilots_mcp_server_processing_raster",
            "qcopilots_mcp_server_processing_vector",
            "qcopilots_mcp_server_skills",
        ]

        for plugin_dir_name in plugin_dirs:
            plugin_dir = plugins_root / plugin_dir_name
            metadata = (plugin_dir / "metadata.txt").read_text(encoding="utf-8")
            cmake = (plugin_dir / "CMakeLists.txt").read_text(encoding="utf-8")
            icon_line = next(line for line in metadata.splitlines() if line.startswith("icon="))
            icon_name = icon_line.split("=", 1)[1].strip()

            self.assertEqual(icon_name, "icon.svg", plugin_dir_name)
            self.assertTrue((plugin_dir / icon_name).is_file(), plugin_dir_name)
            self.assertIn("file(GLOB SVG_FILES *.svg)", cmake, plugin_dir_name)
            self.assertIn("${SVG_FILES}", cmake, plugin_dir_name)
            if plugin_dir_name != "qcopilots_common":
                self.assertIn("file(GLOB JSON_FILES *.json)", cmake, plugin_dir_name)
                self.assertIn("${JSON_FILES}", cmake, plugin_dir_name)
            if plugin_dir_name == "qcopilots_mcp_servers_manager":
                self.assertIn("name=QCopilots MCP Servers Manager", metadata)
                self.assertIn("add_subdirectory(qcopilots_mcp_servers_manager)", plugins_cmake)

    def test_service_plugins_use_manifest_uv_entry_points(self):
        plugins_root = Path(__file__).resolve().parents[3] / "python" / "plugins"
        common_root = plugins_root / "qcopilots_common"
        common_cmake = (common_root / "CMakeLists.txt").read_text(encoding="utf-8")

        self.assertFalse((common_root / "mcp_service_launcher.ps1").exists())
        self.assertFalse((common_root / "MCPServerDirectLaunchGuide.md").exists())
        self.assertFalse((common_root / "mcp_service_launcher.cmd").exists())
        self.assertNotIn("file(GLOB PS1_FILES *.ps1)", common_cmake)
        self.assertNotIn("file(GLOB CMD_FILES *.cmd)", common_cmake)
        self.assertIn("file(GLOB MD_FILES *.md)", common_cmake)
        self.assertNotIn("${PS1_FILES}", common_cmake)
        self.assertNotIn("${CMD_FILES}", common_cmake)
        self.assertIn("${MD_FILES}", common_cmake)
        self.assertRegex(
            common_cmake,
            r"PLUGIN_INSTALL\(qcopilots_common \. .*?\$\{MD_FILES\}.*?\$\{PY_FILES\}",
        )

        expected_ports = {
            "qcopilots_mcp_server_builtin_tools": "48211",
            "qcopilots_mcp_server_skills": "48212",
            "qcopilots_mcp_server_interactive_tools": "48213",
            "qcopilots_mcp_server_processing_vector": "48214",
            "qcopilots_mcp_server_processing_raster": "48215",
        }

        for plugin_dir_name in SERVICE_PLUGIN_DIR_NAMES:
            plugin_dir = plugins_root / plugin_dir_name
            cmake = (plugin_dir / "CMakeLists.txt").read_text(encoding="utf-8")
            manifest = json.loads(
                (plugin_dir / "qcopilots_service.json").read_text(encoding="utf-8")
            )
            lock = (plugin_dir / "requirements.lock").read_text(encoding="utf-8")
            port = expected_ports[plugin_dir_name]

            self.assertFalse((plugin_dir / "start_mcp_server.ps1").exists(), plugin_dir_name)
            self.assertFalse((plugin_dir / "start_mcp_server.cmd").exists(), plugin_dir_name)
            self.assertNotIn("file(GLOB PS1_FILES *.ps1)", cmake, plugin_dir_name)
            self.assertNotIn("file(GLOB CMD_FILES *.cmd)", cmake, plugin_dir_name)
            self.assertNotIn("${PS1_FILES}", cmake, plugin_dir_name)
            self.assertNotIn("${CMD_FILES}", cmake, plugin_dir_name)
            self.assertRegex(
                cmake,
                rf"PLUGIN_INSTALL\({re.escape(plugin_dir_name)} \. "
                r".*?\$\{MD_FILES\}.*?\$\{PY_FILES\}",
                plugin_dir_name,
            )
            self.assertEqual(manifest["entry_point"], "server.py", plugin_dir_name)
            self.assertTrue((plugin_dir / manifest["entry_point"]).is_file(), plugin_dir_name)
            self.assertEqual(manifest["runtime"]["type"], "uv", plugin_dir_name)
            self.assertEqual(manifest["runtime"]["requirements"], "requirements.lock", plugin_dir_name)
            self.assertEqual(str(manifest["default_port"]), port, plugin_dir_name)
            self.assertNotIn("command", manifest, plugin_dir_name)
            if plugin_dir_name == "qcopilots_mcp_server_skills":
                self.assertIn("pyyaml==6.0.2", lock.lower())
            else:
                self.assertFalse(
                    [
                        line
                        for line in lock.splitlines()
                        if line.strip() and not line.lstrip().startswith("#")
                    ],
                    plugin_dir_name,
                )

    def test_manager_switch_matches_requested_visual_contract(self):
        plugin_source = (
            Path(__file__).resolve().parents[3]
            / "python"
            / "plugins"
            / "qcopilots_mcp_servers_manager"
            / "plugin.py"
        ).read_text(encoding="utf-8")

        self.assertIn("class QCopilotsSwitch", plugin_source)
        self.assertIn("setFixedSize(54, 30)", plugin_source)
        self.assertIn("def hitButton", plugin_source)
        self.assertIn("return self.rect().contains(pos)", plugin_source)
        self.assertIn('QColor("#8bdc9a")', plugin_source)
        self.assertIn('background = "#e9f9f4" if self.status.running else "#ffffff"', plugin_source)
        self.assertIn("self.switch.clicked.connect(self.toggle_service)", plugin_source)
        self.assertNotIn("from functools import partial", plugin_source)
        self.assertIn('self.plugin.tr("Starting...")', plugin_source)
        self.assertIn('self.plugin.tr("Stopping...")', plugin_source)
        self.assertIn('self.plugin.tr("Start failed")', plugin_source)
        self.assertIn('self.plugin.tr("Stop failed")', plugin_source)
        self.assertIn("self.switch.setEnabled(False)", plugin_source)
        self.assertIn("self.switch.setEnabled(True)", plugin_source)
        self.assertIn("class ServiceToggleWorker", plugin_source)
        self.assertIn("worker.moveToThread(thread)", plugin_source)
        self.assertIn("def update_status_widgets", plugin_source)
        self.assertIn("def _show_temporary_status", plugin_source)
        self.assertIn("self.status_feedback_timer.start(1600)", plugin_source)
        self.assertIn('status.health == "ok"', plugin_source)
        self.assertIn("self._show_temporary_status(failure_text", plugin_source)
        self.assertIn("qcopilots_service_card", plugin_source)
        self.assertIn("QCopilots MCP Servers Manager", plugin_source)
        self.assertIn("setIconVisibleInMenu(True)", plugin_source)
        self.assertIn("border-radius: 8px", plugin_source)

    def test_manager_switch_hit_button_accepts_full_visual_rect(self):
        _install_manager_import_stubs()

        from qcopilots_mcp_servers_manager.plugin import QCopilotsSwitch

        switch = QCopilotsSwitch()

        self.assertEqual(switch.fixed_size, (54, 30))
        self.assertTrue(switch.hitButton((0, 0)))
        self.assertTrue(switch.hitButton((53, 29)))
        self.assertFalse(switch.hitButton((54, 30)))


def _manifest(tmp, service_id, capabilities, host="127.0.0.1", port=48211):
    from qcopilots_common.manifest import ServiceManifest, ServiceTransport

    plugin_dir = Path(tmp) / service_id.replace(".", "_")
    plugin_dir.mkdir()
    manifest_path = plugin_dir / "qcopilots_service.json"
    return ServiceManifest(
        service_id=service_id,
        display_name=service_id,
        description="test service",
        plugin_name=plugin_dir.name,
        plugin_dir=plugin_dir,
        manifest_path=manifest_path,
        transport=ServiceTransport(host=host, port=port, path="/mcp"),
        capabilities=capabilities,
    )


def _plugins_root():
    return Path(__file__).resolve().parents[3] / "python" / "plugins"


def _read_json(path):
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def _assert_json_schema_header(testcase, schema, path):
    testcase.assertIsInstance(schema, dict, str(path))
    testcase.assertIn("$schema", schema, str(path))
    testcase.assertIn("json-schema", schema["$schema"], str(path))
    testcase.assertEqual(schema.get("type"), "object", str(path))
    testcase.assertIsInstance(schema.get("properties"), dict, str(path))


def _assert_json_matches_schema(testcase, value, schema, context):
    errors = _json_schema_errors(value, schema, context)
    testcase.assertEqual(errors, [], context)


def _json_schema_errors(value, schema, context):
    errors = []
    if "oneOf" in schema:
        if any(not _json_schema_errors(value, candidate, context) for candidate in schema["oneOf"]):
            return []
        return [f"{context}: did not match any oneOf schema"]

    expected_type = schema.get("type")
    if expected_type and not _json_type_matches(value, expected_type):
        return [f"{context}: expected {expected_type}, got {type(value).__name__}"]

    if "enum" in schema and value not in schema["enum"]:
        errors.append(f"{context}: {value!r} not in enum")

    if isinstance(value, (int, float)) and not isinstance(value, bool):
        if "minimum" in schema and value < schema["minimum"]:
            errors.append(f"{context}: {value!r} below minimum {schema['minimum']}")
        if "maximum" in schema and value > schema["maximum"]:
            errors.append(f"{context}: {value!r} above maximum {schema['maximum']}")

    if isinstance(value, str):
        if "minLength" in schema and len(value) < schema["minLength"]:
            errors.append(f"{context}: string shorter than minLength {schema['minLength']}")
        if "pattern" in schema and not re.search(schema["pattern"], value):
            errors.append(f"{context}: {value!r} does not match {schema['pattern']!r}")

    if isinstance(value, list):
        if "minItems" in schema and len(value) < schema["minItems"]:
            errors.append(f"{context}: array shorter than minItems {schema['minItems']}")
        if schema.get("uniqueItems") and len(value) != len({json.dumps(item, sort_keys=True) for item in value}):
            errors.append(f"{context}: array items are not unique")
        item_schema = schema.get("items")
        if isinstance(item_schema, dict):
            for index, item in enumerate(value):
                errors.extend(_json_schema_errors(item, item_schema, f"{context}[{index}]"))

    if isinstance(value, dict):
        properties = schema.get("properties", {})
        for name in schema.get("required", []):
            if name not in value:
                errors.append(f"{context}: missing required property {name!r}")
        if schema.get("additionalProperties") is False:
            for name in value:
                if name not in properties:
                    errors.append(f"{context}: unexpected property {name!r}")
        for name, item in value.items():
            if name in properties:
                errors.extend(_json_schema_errors(item, properties[name], f"{context}.{name}"))
    return errors


def _json_type_matches(value, expected_type):
    if expected_type == "object":
        return isinstance(value, dict)
    if expected_type == "array":
        return isinstance(value, list)
    if expected_type == "string":
        return isinstance(value, str)
    if expected_type == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected_type == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if expected_type == "boolean":
        return isinstance(value, bool)
    return True


def _assert_described_property(testcase, properties, name, context, required_terms=()):
    _assert_described_schema_node(testcase, properties[name], f"{context}:{name}", required_terms)


def _assert_described_schema_node(testcase, node, context, required_terms=()):
    description = node.get("description", "")
    testcase.assertIsInstance(description, str, context)
    testcase.assertGreaterEqual(len(description), 16, context)
    for term in required_terms:
        if isinstance(term, tuple):
            testcase.assertTrue(
                any(_description_contains(description, option) for option in term),
                f"{context}: {description}",
            )
        else:
            testcase.assertTrue(_description_contains(description, term), f"{context}: {description}")


def _assert_text_contains_terms(testcase, text, context, required_terms):
    testcase.assertIsInstance(text, str, context)
    for term in required_terms:
        if isinstance(term, tuple):
            testcase.assertTrue(
                any(_description_contains(text, option) for option in term),
                f"{context}: {text}",
            )
        else:
            testcase.assertTrue(_description_contains(text, term), f"{context}: {text}")


def _assert_schema_descriptions_cover_prompting_contract(testcase, schema, context):
    descriptions = list(_schema_descriptions(schema.get("properties", {})))
    testcase.assertGreater(len(descriptions), 0, context)
    for description in descriptions:
        testcase.assertIn("字段作用", description, f"{context}: {description}")
        testcase.assertIn("候选", description, f"{context}: {description}")
        testcase.assertTrue(
            any(term in description for term in ("影响", "不同", "会")),
            f"{context}: {description}",
        )


def _assert_schema_descriptions_are_chinese(testcase, schema, context):
    descriptions = list(_schema_descriptions(schema))
    testcase.assertGreater(len(descriptions), 0, context)
    for description in descriptions:
        testcase.assertTrue(_contains_cjk(description), f"{context}: {description}")


def _schema_descriptions(node):
    if isinstance(node, dict):
        description = node.get("description")
        if isinstance(description, str):
            yield description
        for value in node.values():
            yield from _schema_descriptions(value)
    elif isinstance(node, list):
        for item in node:
            yield from _schema_descriptions(item)


def _description_contains(description, term):
    return str(term).lower() in description.lower()


def _contains_cjk(text):
    return any("\u4e00" <= character <= "\u9fff" for character in text)


def _status(running=False, health="stopped", url="http://127.0.0.1:48211/mcp", port=48211):
    return types.SimpleNamespace(
        running=running,
        health=health,
        url=url,
        port=port,
    )


class _DefaultStartupCard:
    def __init__(self, manifest, running=False):
        self.manifest = manifest
        self.status = _status(running=running, health="ok" if running else "stopped")
        self.toggle_values = []
        self.status_widget_updates = 0

    def toggle_service(self, enabled):
        self.toggle_values.append(enabled)

    def update_status_widgets(self):
        self.status_widget_updates += 1


def _worker(worker_class, controller, manifest, enabled, was_running):
    return worker_class(
        controller,
        _FakeLogger(),
        manifest,
        enabled,
        was_running,
        "http://127.0.0.1:48200",
        {"QCOPILOTS_TEST_ENV": "1"},
        "Service did not become healthy",
        "Unknown service state",
    )


def _fake_service_card(manager_plugin, running=False):
    card = object.__new__(manager_plugin.ServiceCard)
    card.plugin = _FakeManagerPlugin(status=_status(running=running, health="ok" if running else "stopped"))
    card.manifest = types.SimpleNamespace(service_id="qcopilots.test")
    card.status = _status(running=running, health="ok" if running else "stopped")
    card.switch = _FakeSwitch()
    card.port_label = _FakeLabel()
    card.running_label = _FakeLabel()
    card.inline_endpoint_label = _FakeLabel()
    card.endpoint_label = _FakeLabel()
    card.status_feedback_timer = _FakeTimer()
    card.status_feedback_timer.connect(card.update_status_widgets)
    card._update_card_style = lambda: None
    card.toggle_thread = None
    card.toggle_worker = None
    card.toggle_expected_enabled = False
    card.toggle_failure_text = ""
    card._fail_toggle_service = types.MethodType(manager_plugin.ServiceCard._fail_toggle_service, card)
    card._start_toggle_worker = types.MethodType(manager_plugin.ServiceCard._start_toggle_worker, card)
    card.toggle_service = types.MethodType(manager_plugin.ServiceCard.toggle_service, card)
    return card


class _CaptureSignal:
    def __init__(self):
        self.emissions = []
        self.slots = []

    def connect(self, slot):
        self.slots.append(slot)

    def disconnect(self, slot):
        self.slots = [existing for existing in self.slots if existing != slot]

    def emit(self, *args):
        self.emissions.append(args)
        for slot in self.slots:
            slot(*args)


class _FakeTimer:
    def __init__(self):
        self.interval = None
        self.callback = None

    def setSingleShot(self, value):
        self.single_shot = value

    @property
    def timeout(self):
        return self

    def connect(self, callback):
        self.callback = callback

    def start(self, interval):
        self.interval = interval

    def fire(self):
        if self.callback:
            self.callback()


class _FakeSingleShotTimer:
    calls = []

    @staticmethod
    def singleShot(interval, callback):
        _FakeSingleShotTimer.calls.append((interval, callback))


class _FakeClipboard:
    def setText(self, text):
        _FakeApplication.clipboard_text = text


class _FakeApplication:
    clipboard_text = ""

    @staticmethod
    def clipboard():
        return _FakeClipboard()


class _FakeButton:
    def __init__(self, text="", tooltip=""):
        self._text = text
        self._tooltip = tooltip
        self.enabled = True

    def text(self):
        return self._text

    def setText(self, text):
        self._text = text

    def toolTip(self):
        return self._tooltip

    def setToolTip(self, tooltip):
        self._tooltip = tooltip

    def setEnabled(self, enabled):
        self.enabled = enabled


class _FakeLabel:
    def __init__(self):
        self._text = ""
        self._tooltip = ""

    def text(self):
        return self._text

    def setText(self, text):
        self._text = text

    def toolTip(self):
        return self._tooltip

    def setToolTip(self, tooltip):
        self._tooltip = tooltip


class _FakeSwitch:
    def __init__(self):
        self.checked = False
        self.enabled = True
        self.signals_blocked = False

    def isChecked(self):
        return self.checked

    def setChecked(self, checked):
        self.checked = checked

    def setEnabled(self, enabled):
        self.enabled = enabled

    def blockSignals(self, blocked):
        self.signals_blocked = blocked


class _FakeWidgetForLayout:
    def __init__(self):
        self.deleted = False

    def deleteLater(self, *args):
        del args
        self.deleted = True


class _FakeLayoutItem:
    def __init__(self, widget):
        self._widget = widget

    def widget(self):
        return self._widget


class _FakeLayout:
    def __init__(self, widgets=None):
        self.items = [_FakeLayoutItem(widget) for widget in widgets or []]
        self.stretches = 0

    def count(self):
        return len(self.items)

    def itemAt(self, index):
        return self.items[index]

    def takeAt(self, index):
        return self.items.pop(index)

    def addWidget(self, widget):
        self.items.append(_FakeLayoutItem(widget))

    def addStretch(self, stretch):
        self.stretches += stretch


class _FakeLogger:
    def __init__(self):
        self.messages = []

    def warning(self, message, *args):
        self.messages.append((message, args))


def _assert_logger_warning_contains(testcase, logger, text):
    testcase.assertTrue(
        any(text in message for message, _args in logger.messages),
        f"{text!r} not found in {logger.messages!r}",
    )


class _FakeDialog:
    def __init__(self, prepare_close_result=True):
        self.prepare_close_result = prepare_close_result
        self.prepare_close_wait_values = []
        self.closed = False
        self.deleted = False

    def prepare_close(self, wait=False):
        self.prepare_close_wait_values.append(wait)
        return self.prepare_close_result

    def close(self):
        self.closed = True

    def deleteLater(self, *args):
        del args
        self.deleted = True


class _FakeIface:
    def mainWindow(self):
        return None

    def addToolBarIcon(self, action):
        self.toolbar_actions = getattr(self, "toolbar_actions", [])
        self.toolbar_actions.append(action)

    def removeToolBarIcon(self, action):
        self.removed_toolbar_action = action


class _FakeBridge:
    def __init__(self):
        self.url = "http://127.0.0.1:48200"
        self.started = False
        self.start_count = 0
        self.stopped = False
        self.stop_count = 0

    def start(self):
        self.started = True
        self.start_count += 1

    def stop(self):
        self.stopped = True
        self.stop_count += 1


class _FakeManagerPlugin:
    def __init__(self, start_status=None, stop_status=None, status=None):
        self.start_status = start_status or _status(running=True, health="ok")
        self.stop_status = stop_status or _status(running=False, health="stopped")
        self.status = status or _status(running=False, health="stopped")
        self.controller = _FakeProcessController(self.start_status, self.stop_status, self.status)
        self.bridge = types.SimpleNamespace(url="http://127.0.0.1:48200")
        self._default_startup_applied = False
        self.calls = []
        self.logger = _FakeLogger()

    def tr(self, message):
        return message

    def _service_env(self, manifest):
        del manifest
        self.calls.append("service_env")
        return {"QCOPILOTS_TEST_ENV": "1"}

    def _discover_services(self):
        return [
            types.SimpleNamespace(service_id="qcopilots.first"),
            types.SimpleNamespace(service_id="qcopilots.second"),
            types.SimpleNamespace(service_id="qcopilots.third"),
        ]

    def manager_config(self):
        return {"default_startup": {"enabled": False, "service_ids": []}}

    def service_status(self, manifest, deep=True):
        self.calls.append("status")
        return self.controller.status(manifest, deep=deep)

    def stop_service(self, manifest):
        del manifest
        self.calls.append("stop_service")
        return self.controller.stop(types.SimpleNamespace(service_id="qcopilots.test"))

    def is_shutting_down(self):
        return getattr(self, "_shutdown_started", False)


class _FakeProcessController:
    def __init__(self, start_status=None, stop_status=None, status=None, start_error=None, stop_error=None):
        self.start_status = start_status or _status(running=True, health="ok")
        self.stop_status = stop_status or _status(running=False, health="stopped")
        self.status_value = status or _status(running=False, health="stopped")
        self.start_error = start_error
        self.stop_error = stop_error
        self.calls = []
        self.status_deep_values = []

    def start(self, manifest, bridge_url=None, extra_env=None):
        del manifest
        self.calls.append("start")
        self.bridge_url = bridge_url
        self.extra_env = extra_env
        if self.start_error:
            raise self.start_error
        return self.start_status

    def stop(self, manifest):
        del manifest
        self.calls.append("stop")
        if self.stop_error:
            raise self.stop_error
        return self.stop_status

    def status(self, manifest, deep=True):
        del manifest
        self.calls.append("status")
        self.status_deep_values.append(deep)
        return self.status_value


class _RecordingController:
    def __init__(self, fail_service_ids=None, stored_manifests=None):
        self.stopped_service_ids = []
        self.fail_service_ids = set(fail_service_ids or set())
        self._stored_manifests = list(stored_manifests or [])

    def stop(self, manifest):
        self.stopped_service_ids.append(manifest.service_id)
        if manifest.service_id in self.fail_service_ids:
            raise RuntimeError("stop failed")
        return _status(running=False, health="stopped")

    def stored_manifests(self):
        return list(self._stored_manifests)


class _DeferredFakeThread:
    def __init__(self, parent=None):
        self.parent = parent
        self.started = _CaptureSignal()
        self.finished = _CaptureSignal()
        self.started_count = 0
        self.quit_count = 0
        self.wait_count = 0
        self.wait_args = []
        self.wait_result = True
        self.emit_finished_on_quit = True
        self.deleted = False
        self.running = False

    def start(self):
        self.started_count += 1
        self.running = True

    def quit(self, *args):
        del args
        self.quit_count += 1
        self.running = False
        if self.emit_finished_on_quit:
            self.finished.emit()

    def isRunning(self):
        return self.running

    def wait(self, *args):
        self.wait_count += 1
        self.wait_args.append(args)
        if self.wait_result:
            self.running = False
        return self.wait_result

    def deleteLater(self, *args):
        del args
        self.deleted = True


class _DeferredFakeServiceToggleWorker:
    def __init__(
        self,
        controller,
        logger,
        manifest,
        enabled,
        was_running,
        bridge_url,
        extra_env,
        unhealthy_message,
        unknown_state_message,
        shutdown_requested=None,
    ):
        self.controller = controller
        self.logger = logger
        self.manifest = manifest
        self.enabled = enabled
        self.was_running = was_running
        self.bridge_url = bridge_url
        self.extra_env = extra_env
        self.unhealthy_message = unhealthy_message
        self.unknown_state_message = unknown_state_message
        self.shutdown_requested = shutdown_requested
        self.finished = _CaptureSignal()
        self.failed = _CaptureSignal()
        self.deleted = False
        self.thread = None

    def moveToThread(self, thread):
        self.thread = thread

    def run(self):
        return

    def deleteLater(self, *args):
        del args
        self.deleted = True


def _install_manager_import_stubs():
    try:
        from qgis.core import QgsProject  # noqa: F401
        from qgis.PyQt.QtWidgets import QAction  # noqa: F401
        return
    except Exception:
        pass

    if "qgis.core" in sys.modules and "qgis.PyQt.QtWidgets" in sys.modules:
        return

    qgis_module = sys.modules.setdefault("qgis", types.ModuleType("qgis"))
    qgis_module.__path__ = getattr(qgis_module, "__path__", [])

    core = types.ModuleType("qgis.core")

    class FakeQgsProject:
        @staticmethod
        def instance():
            return FakeQgsProject()

        def fileName(self):
            return ""

    core.QgsProject = FakeQgsProject
    qgis_module.core = core
    sys.modules.setdefault("qgis.core", core)

    pyqt = types.ModuleType("qgis.PyQt")
    qtcore = types.ModuleType("qgis.PyQt.QtCore")
    qtwidgets = types.ModuleType("qgis.PyQt.QtWidgets")

    class FakeQCoreApplication:
        @staticmethod
        def translate(_context, message):
            return message

    class FakeQt:
        class AlignmentFlag:
            AlignTop = 0
            AlignCenter = 1

        class CursorShape:
            PointingHandCursor = 0

        class FocusPolicy:
            StrongFocus = 0

        class PenStyle:
            NoPen = 0

    class FakeQSize:
        def __init__(self, width, height):
            self.width = width
            self.height = height

    class FakeSignal:
        def __init__(self, *args, **kwargs):
            self.slots = []

        def connect(self, slot):
            self.slots.append(slot)

        def emit(self, *args):
            for slot in self.slots:
                slot(*args)

    class FakeWidget:
        def __init__(self, *args, **kwargs):
            self.deleted = False

        def moveToThread(self, thread):
            self.thread = thread

        def deleteLater(self, *args):
            del args
            self.deleted = True

        @staticmethod
        def processEvents():
            pass

    class FakeRect:
        def __init__(self, width, height):
            self.width = width
            self.height = height

        def contains(self, pos):
            x, y = pos
            return 0 <= x < self.width and 0 <= y < self.height

    class FakeCheckBox(FakeWidget):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.clicked = FakeSignal()
            self.fixed_size = (0, 0)
            self._checked = False

        def setText(self, text):
            self.text_value = text

        def setCursor(self, cursor):
            self.cursor = cursor

        def setFixedSize(self, width, height):
            self.fixed_size = (width, height)

        def setFocusPolicy(self, policy):
            self.focus_policy = policy

        def setChecked(self, checked):
            self._checked = checked

        def isChecked(self):
            return self._checked

        def rect(self):
            return FakeRect(*self.fixed_size)

    class FakeThread(FakeWidget):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.started = FakeSignal()
            self.finished = FakeSignal()

        def start(self):
            self.started.emit()

        def quit(self, *args):
            del args
            self.finished.emit()

    qtcore.QCoreApplication = FakeQCoreApplication
    qtcore.QObject = FakeWidget
    qtcore.QThread = FakeThread
    qtcore.QTimer = FakeWidget
    qtcore.QSize = FakeQSize
    qtcore.Qt = FakeQt
    qtcore.pyqtSignal = lambda *args, **kwargs: FakeSignal()
    qtgui = types.ModuleType("qgis.PyQt.QtGui")
    qtgui.QColor = FakeWidget
    qtgui.QIcon = FakeWidget
    qtgui.QPainter = FakeWidget
    for name in (
        "QAction",
        "QApplication",
        "QDialog",
        "QFrame",
        "QHBoxLayout",
        "QLabel",
        "QPushButton",
        "QScrollArea",
        "QSizePolicy",
        "QToolButton",
        "QVBoxLayout",
        "QWidget",
    ):
        setattr(qtwidgets, name, FakeWidget)
    qtwidgets.QCheckBox = FakeCheckBox

    sys.modules.setdefault("qgis.PyQt", pyqt)
    sys.modules.setdefault("qgis.PyQt.QtCore", qtcore)
    sys.modules.setdefault("qgis.PyQt.QtGui", qtgui)
    sys.modules.setdefault("qgis.PyQt.QtWidgets", qtwidgets)
    qgis_module.PyQt = pyqt
    pyqt.QtCore = qtcore
    pyqt.QtGui = qtgui
    pyqt.QtWidgets = qtwidgets


if __name__ == "__main__":
    unittest.main()
