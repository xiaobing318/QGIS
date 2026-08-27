"""Tests for QCopilots interactive MCP tools.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-19"
__copyright__ = "Copyright 2026, The QGIS Project"

import gc
import os
import sys
import tempfile
import threading
import types
import unittest
from pathlib import Path
from unittest import mock
from urllib.parse import parse_qs, urlencode


TILE_URL = "http://127.0.0.1/tiles/{z}/{x}/{y}.png"
WCS_URL = "http://127.0.0.1/wcs"
WMS_URL = "http://127.0.0.1/wms"


def _qgis_bindings_available():
    try:
        import qgis.core  # NOQA

        return True
    except Exception:
        return False


class TestQCopilotsMcpServerInteractiveTools(unittest.TestCase):
    def test_build_interactive_tools_includes_layer_and_editing_tools(self):
        import qcopilots_common.interactive_layer_tools as interactive_layer_tools
        import qcopilots_common.processing_tools as processing_tools

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool, "arguments": arguments}

        original_processing_bridge_client = processing_tools.BridgeClient
        original_interactive_layer_bridge_client = interactive_layer_tools.BridgeClient
        processing_tools.BridgeClient = FakeBridgeClient
        interactive_layer_tools.BridgeClient = FakeBridgeClient
        try:
            tools = processing_tools.build_interactive_tools()
            tool_names = [tool.name for tool in tools]
            self.assertEqual(len(tool_names), 33)
            self.assertEqual(
                tool_names,
                [
                    "list_layers",
                    "set_layer_visibility",
                    "zoom_to_layer",
                    "zoom_to_extent",
                    "zoom_in",
                    "zoom_out",
                    "zoom_full",
                    "zoom_to_selection",
                    "zoom_to_native_resolution",
                    "zoom_to_last_extent",
                    "zoom_to_next_extent",
                    "save_project",
                    "refresh_canvas",
                    "export_map_image",
                    "describe_layer_sources",
                    "list_map_layers",
                    "load_map_layer",
                    "remove_map_layers",
                    "get_layer_metadata",
                    "query_vector_features",
                    "set_vector_selection",
                    "delete_vector_features",
                    "get_project_crs",
                    "set_project_crs",
                    "get_raster_statistics",
                    "apply_layer_style",
                    "configure_vector_labels",
                    "create_print_layout",
                    "list_print_layouts",
                    "export_print_layout",
                    "create_vector_layer",
                    "add_vector_features",
                    "update_vector_features",
                ],
            )
            self.assertTrue(all("requires_auth" not in tool.descriptor() for tool in tools))
            tools_by_name = {tool.name: tool for tool in tools}
            self.assertIn("xyz", tools_by_name["load_map_layer"].input_schema["properties"]["layer_type"]["enum"])
            self.assertIn("wcs", tools_by_name["load_map_layer"].input_schema["properties"]["layer_type"]["enum"])
            self.assertIn(
                "point_cloud",
                tools_by_name["load_map_layer"].input_schema["properties"]["layer_type"]["enum"],
            )
            self.assertIn("features", tools_by_name["add_vector_features"].input_schema["properties"])
            self.assertEqual(
                tools_by_name["add_vector_features"].input_schema["properties"]["features"]["minItems"],
                1,
            )
            self.assertIn("updates", tools_by_name["update_vector_features"].input_schema["properties"])
            self.assertEqual(
                tools_by_name["update_vector_features"].input_schema["properties"]["updates"]["minItems"],
                1,
            )
            self.assertEqual(
                tools_by_name["query_vector_features"].input_schema["properties"]["limit"]["maximum"],
                500,
            )
            self.assertEqual(
                tools_by_name["query_vector_features"].input_schema["properties"]["offset"]["maximum"],
                10000,
            )
            self.assertEqual(
                tools_by_name["delete_vector_features"].input_schema["properties"]["feature_ids"]["maxItems"],
                10000,
            )
            self.assertEqual(
                tools_by_name["get_raster_statistics"].input_schema["properties"]["sample_size"]["maximum"],
                250000,
            )
            self.assertEqual(
                tools_by_name["get_raster_statistics"].input_schema["properties"]["sample_size"]["default"],
                100000,
            )
            self.assertEqual(
                tools_by_name["zoom_to_extent"].input_schema["properties"]["extent"]["minItems"],
                4,
            )
            self.assertEqual(
                tools_by_name["zoom_to_extent"].input_schema["properties"]["extent"]["maxItems"],
                4,
            )
            self.assertFalse(
                tools_by_name["export_print_layout"].input_schema["properties"]["overwrite"]["default"]
            )
            self.assertFalse(
                tools_by_name["create_vector_layer"].input_schema["properties"]["overwrite"]["default"]
            )
            for tool_name in (
                "save_project",
                "export_map_image",
                "export_print_layout",
                "create_vector_layer",
            ):
                self.assertIn(
                    "overwrite_confirmation_token",
                    tools_by_name[tool_name].input_schema["properties"],
                )
            self.assertIn(
                "confirmation_token",
                tools_by_name["remove_map_layers"].input_schema["properties"],
            )
            editable_changes = tools_by_name["remove_map_layers"].input_schema[
                "properties"
            ]["editable_changes"]
            self.assertEqual(
                editable_changes["enum"],
                ["reject", "save", "discard"],
            )
            self.assertEqual(editable_changes["default"], "reject")
            self.assertIn("oneOf", tools_by_name["remove_map_layers"].input_schema)
            self.assertIn("oneOf", tools_by_name["delete_vector_features"].input_schema)

            tools_by_name["list_layers"].handler({})
            tools_by_name["set_layer_visibility"].handler({"layer_id": "abc", "visible": True})
            tools_by_name["describe_layer_sources"].handler({})
            tools_by_name["load_map_layer"].handler(
                {
                    "layer_type": "xyz",
                    "source": TILE_URL,
                    "name": "Tiles",
                }
            )
            tools_by_name["get_layer_metadata"].handler({"layer_id": "abc"})
            tools_by_name["delete_vector_features"].handler(
                {"layer_id": "abc", "feature_ids": [1]}
            )
            tools_by_name["export_print_layout"].handler(
                {"layout_name": "Atlas", "path": "atlas.pdf"}
            )
            tools_by_name["create_vector_layer"].handler({"name": "Memory Points"})
            tools_by_name["update_vector_features"].handler(
                {
                    "layer_id": "abc",
                    "updates": [{"feature_id": 1, "attributes": {"name": "Roads"}}],
                }
            )
        finally:
            processing_tools.BridgeClient = original_processing_bridge_client
            interactive_layer_tools.BridgeClient = original_interactive_layer_bridge_client

        self.assertEqual(
            calls,
            [
                ("list_layers", {}),
                ("set_layer_visibility", {"layer_id": "abc", "visible": True}),
                ("interactive_layer_describe_sources", {}),
                (
                    "interactive_layer_load_layer",
                    {
                        "layer_type": "xyz",
                        "source": TILE_URL,
                        "name": "Tiles",
                    },
                ),
                ("interactive_layer_get_metadata", {"layer_id": "abc"}),
                (
                    "interactive_layer_delete_features",
                    {"layer_id": "abc", "feature_ids": [1]},
                ),
                (
                    "interactive_layout_export",
                    {"layout_name": "Atlas", "path": "atlas.pdf"},
                ),
                ("create_vector_layer", {"name": "Memory Points"}),
                (
                    "update_vector_features",
                    {
                        "layer_id": "abc",
                        "updates": [{"feature_id": 1, "attributes": {"name": "Roads"}}],
                    },
                ),
            ],
        )

    def test_all_33_interactive_tools_route_to_the_declared_bridge_operations(self):
        import qcopilots_common.interactive_layer_tools as interactive_layer_tools
        import qcopilots_common.processing_tools as processing_tools

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool}

        expected_operations = [
            "list_layers",
            "set_layer_visibility",
            "zoom_to_layer",
            "zoom_to_extent",
            "zoom_in",
            "zoom_out",
            "zoom_full",
            "zoom_to_selection",
            "zoom_to_native_resolution",
            "zoom_to_last_extent",
            "zoom_to_next_extent",
            "save_project",
            "refresh_canvas",
            "export_map_image",
            "interactive_layer_describe_sources",
            "interactive_layer_list_layers",
            "interactive_layer_load_layer",
            "interactive_layer_remove_layers",
            "interactive_layer_get_metadata",
            "interactive_layer_query_features",
            "interactive_layer_set_selection",
            "interactive_layer_delete_features",
            "interactive_project_get_crs",
            "interactive_project_set_crs",
            "interactive_raster_statistics",
            "interactive_layer_apply_style",
            "interactive_layer_configure_labels",
            "interactive_layout_create",
            "interactive_layout_list",
            "interactive_layout_export",
            "create_vector_layer",
            "add_vector_features",
            "update_vector_features",
        ]

        with mock.patch.object(processing_tools, "BridgeClient", FakeBridgeClient), mock.patch.object(
            interactive_layer_tools,
            "BridgeClient",
            FakeBridgeClient,
        ):
            tools = processing_tools.build_interactive_tools()
            for tool in tools:
                tool.handler({})

        self.assertEqual(len(tools), 33)
        self.assertEqual([operation for operation, _arguments in calls], expected_operations)
        self.assertTrue(all(arguments == {} for _operation, arguments in calls))

    def test_interactive_schemas_enforce_extent_and_confirmation_branches(self):
        import qcopilots_common.processing_tools as processing_tools
        from qcopilots_common.mcp_http import ToolError, _validate_tool_arguments

        tools = {
            tool.name: tool for tool in processing_tools.build_interactive_tools()
        }

        def assert_valid(tool_name, arguments):
            _validate_tool_arguments(arguments, tools[tool_name].input_schema)

        def assert_invalid(tool_name, arguments):
            with self.assertRaisesRegex(ToolError, "Invalid tool arguments"):
                _validate_tool_arguments(arguments, tools[tool_name].input_schema)

        assert_valid("zoom_to_extent", {"extent": [0, 1, 2, 3]})
        assert_invalid("zoom_to_extent", {"extent": [0, 1, 2]})
        assert_invalid("zoom_to_extent", {"extent": [0, 1, 2, 3, 4]})

        token = "confirmation-token"
        assert_valid("remove_map_layers", {"layer_id": "layer-a"})
        for editable_changes in ("reject", "save", "discard"):
            assert_valid(
                "remove_map_layers",
                {
                    "layer_id": "layer-a",
                    "editable_changes": editable_changes,
                },
            )
        assert_valid("remove_map_layers", {"confirmation_token": token})
        assert_invalid("remove_map_layers", {})
        assert_invalid(
            "remove_map_layers",
            {"layer_id": "layer-a", "editable_changes": "prompt"},
        )
        assert_invalid(
            "remove_map_layers",
            {"layer_id": "layer-a", "confirmation_token": token},
        )
        assert_invalid(
            "remove_map_layers",
            {
                "confirmation_token": token,
                "editable_changes": "discard",
            },
        )
        assert_invalid("remove_map_layers", {"layer_ids": []})

        assert_valid(
            "delete_vector_features",
            {"layer_id": "layer-a", "feature_ids": [1]},
        )
        assert_valid(
            "delete_vector_features",
            {"layer_id": "layer-a", "confirmation_token": token},
        )
        assert_invalid("delete_vector_features", {"layer_id": "layer-a"})
        assert_invalid(
            "delete_vector_features",
            {
                "layer_id": "layer-a",
                "feature_ids": [1],
                "confirmation_token": token,
            },
        )

        assert_valid(
            "set_vector_selection",
            {"layer_id": "layer-a", "feature_ids": [1]},
        )
        assert_valid(
            "set_vector_selection",
            {"layer_id": "layer-a", "mode": "clear"},
        )
        assert_invalid("set_vector_selection", {"layer_id": "layer-a"})
        assert_invalid(
            "set_vector_selection",
            {"layer_id": "layer-a", "mode": "add"},
        )
        assert_invalid(
            "set_vector_selection",
            {"layer_id": "layer-a", "mode": "clear", "feature_ids": [1]},
        )

        assert_valid(
            "configure_vector_labels",
            {"layer_id": "layer-a", "field_or_expression": "name"},
        )
        assert_valid(
            "configure_vector_labels",
            {"layer_id": "layer-a", "enabled": False},
        )
        assert_invalid("configure_vector_labels", {"layer_id": "layer-a"})
        assert_invalid(
            "configure_vector_labels",
            {
                "layer_id": "layer-a",
                "enabled": False,
                "field_or_expression": "name",
            },
        )

        for tool_name, property_name in (
            ("set_layer_visibility", "layer_id"),
            ("load_map_layer", "source"),
            ("add_vector_features", "layer_id"),
            ("update_vector_features", "layer_id"),
        ):
            self.assertEqual(
                tools[tool_name].input_schema["properties"][property_name][
                    "minLength"
                ],
                1,
            )

    @unittest.skipUnless(_qgis_bindings_available(), "QGIS bindings are unavailable")
    def test_create_vector_layer_rejects_provider_field_failure(self):
        import qcopilots_common.bridge as bridge

        provider = mock.Mock()
        provider.addAttributes.return_value = False
        layer = mock.Mock()
        layer.isValid.return_value = True
        layer.dataProvider.return_value = provider

        with mock.patch("qgis.core.QgsVectorLayer", return_value=layer), mock.patch.object(
            bridge,
            "_vector_geometry_type",
            return_value="Point",
        ), mock.patch.object(bridge, "_qgs_field", return_value=object()):
            tools = bridge.QgisBridgeTools(None)
            with self.assertRaisesRegex(
                RuntimeError,
                "Could not add vector layer fields",
            ):
                tools.create_vector_layer(
                    {
                        "name": "Rejected fields",
                        "fields": [{"name": "broken", "type": "string"}],
                        "add_to_project": False,
                    }
                )

        provider.addAttributes.assert_called_once()
        layer.updateFields.assert_not_called()

    @unittest.skipUnless(_qgis_bindings_available(), "QGIS bindings are unavailable")
    def test_raster_statistics_handler_uses_schema_default_sample_size(self):
        import qcopilots_common.bridge as bridge

        statistics = types.SimpleNamespace(
            elementCount=1,
            minimumValue=1.0,
            maximumValue=1.0,
            range=0.0,
            sum=1.0,
            mean=1.0,
            stdDev=0.0,
            sumOfSquares=1.0,
            width=1,
            height=1,
            extent=object(),
        )
        provider = mock.Mock()
        provider.bandStatistics.return_value = statistics
        layer = mock.Mock()
        layer.bandCount.return_value = 1
        layer.dataProvider.return_value = provider
        layer.extent.return_value = object()

        with mock.patch.object(
            bridge,
            "_raster_layer_from_id",
            return_value=layer,
        ), mock.patch.object(
            bridge,
            "_layer_summary",
            return_value={"id": "raster-a"},
        ), mock.patch.object(
            bridge,
            "_extent_values",
            return_value=[0, 0, 1, 1],
        ):
            result = bridge.QgisBridgeTools(None).interactive_raster_statistics(
                {"layer_id": "raster-a"}
            )

        self.assertEqual(result["sample_size"], 100000)
        self.assertEqual(provider.bandStatistics.call_args.args[3], 100000)

    def test_delete_confirmation_store_is_one_use_expiring_and_layer_bound(self):
        from qcopilots_common.bridge import FeatureDeleteConfirmationStore

        now = [100.0]
        store = FeatureDeleteConfirmationStore(
            ttl_seconds=5,
            clock=lambda: now[0],
            token_factory=lambda: "confirmation-token-0001",
        )
        token = store.issue("layer-a", [1, 2], "fingerprint")
        with self.assertRaisesRegex(RuntimeError, "does not match"):
            store.consume(token, "layer-b")
        with self.assertRaisesRegex(RuntimeError, "already used"):
            store.consume(token, "layer-a")

        token = store.issue("layer-a", [1, 2], "fingerprint")
        now[0] = 106.0
        with self.assertRaisesRegex(RuntimeError, "expired"):
            store.consume(token, "layer-a")

    def test_interactive_output_path_rejects_implicit_overwrite(self):
        from qcopilots_common.bridge import _require_available_output_path

        with tempfile.TemporaryDirectory() as temporary_directory:
            output_path = Path(temporary_directory) / "existing-output.pdf"
            output_path.write_text("preserve", encoding="utf-8")

            with self.assertRaisesRegex(FileExistsError, "already exists"):
                _require_available_output_path(output_path, False, "Layout output")
            self.assertEqual(output_path.read_text(encoding="utf-8"), "preserve")

            _require_available_output_path(output_path, True, "Layout output")

    def test_formal_layer_sources_allow_local_paths_and_require_approved_origin(self):
        from qcopilots_common.bridge import (
            QgisBridgeTools,
            _validate_formal_layer_uri_options,
        )
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            read_root = root / "read"
            outside_root = root / "outside"
            for directory in (read_root, outside_root):
                directory.mkdir()
            inside = read_root / "inside.gpkg"
            outside = outside_root / "outside.gpkg"
            inside.write_bytes(b"inside")
            outside.write_bytes(b"outside")
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {
                        "enabled": True,
                        "allowed_origins": ["http://127.0.0.1:49180"],
                    },
                }
            )
            bridge = QgisBridgeTools(None, filesystem_policy=policy)

            self.assertEqual(
                bridge._validate_layer_source_path(str(inside)),
                str(inside.resolve()),
            )
            self.assertEqual(
                bridge._validate_layer_source_path(str(outside)),
                str(outside.resolve()),
            )
            local_provider_uri = f"dbname='{inside}' table=roads"
            self.assertEqual(
                bridge._validate_layer_source_path(local_provider_uri),
                local_provider_uri,
            )
            outside_provider_uri = f"dbname='{outside}' table=roads"
            self.assertEqual(
                bridge._validate_layer_source_path(outside_provider_uri),
                outside_provider_uri,
            )
            allowed_url = "http://127.0.0.1:49180/wms?service=WMS"
            self.assertEqual(
                bridge._validate_layer_source_path(allowed_url),
                allowed_url,
            )
            for rejected in (
                "http://127.0.0.1:49181/wms",
                "https://127.0.0.1:49180/wms",
                "http://localhost:49180/wms",
                allowed_url + "|layers=roads",
                allowed_url + "|host=example.invalid",
                allowed_url + "|path=" + str(inside),
                "postgresql://127.0.0.1/database",
                "host=127.0.0.1 dbname=qgis",
                "missing-layer-id",
                f"dbname='{inside}' host=example.invalid port=5432 table=roads",
                f"dbname='{inside}' service=external_service table=roads",
                f"path='{inside}' connection=http://example.invalid",
                f"DBNAME='{inside}' HOST=example.invalid",
                "dbname%3D"
                + str(inside).replace("\\", "%5C")
                + "%26host%3Dexample.invalid",
                f"dbname='{inside}' dbname='{inside}'",
                "url=http%3A%2F%2F127.0.0.1%3A49180%2Fwms%26host%3Dexample.invalid",
                "x" * 17000 + "=value",
            ):
                with self.subTest(rejected=rejected), self.assertRaises(
                    (PermissionError, FileNotFoundError)
                ):
                    bridge._validate_layer_source_path(rejected)

            for uri_options in (
                {"host": "example.invalid"},
                {"port": 5432},
                {"service": "external_service"},
                {"connection": "http://example.invalid"},
                {"auth": "secret"},
                {"HOST": "example.invalid"},
                {"%68ost": "example.invalid"},
                {"HOST": "one", "host": "two"},
                {"path": str(inside)},
            ):
                with self.subTest(uri_options=uri_options), self.assertRaises(
                    PermissionError
                ):
                    _validate_formal_layer_uri_options(
                        allowed_url,
                        uri_options,
                        layer_type="wms",
                        provider="wms",
                        policy=policy,
                    )
            _validate_formal_layer_uri_options(
                allowed_url,
                {"layers": ["roads"], "styles": [""]},
                layer_type="wms",
                provider="wms",
                policy=policy,
            )

            disabled_policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )
            with self.assertRaisesRegex(PermissionError, "network sources"):
                QgisBridgeTools(
                    None, filesystem_policy=disabled_policy
                )._validate_layer_source_path(allowed_url)

    def test_compatible_layer_sources_validate_embedded_local_paths(self):
        from qcopilots_common.bridge import QgisBridgeTools
        from qcopilots_common.security_policy import FilesystemPolicy

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "data=roads.gpkg"
            source.write_bytes(b"data")
            bridge = QgisBridgeTools(
                None,
                filesystem_policy=FilesystemPolicy(),
            )

            self.assertEqual(
                bridge._validate_layer_source_path(str(source)),
                str(source.resolve()),
            )
            self.assertEqual(
                bridge._validate_layer_source_path(source.as_uri()),
                str(source.resolve()),
            )
            for local_url in (str(source), source.as_uri()):
                with self.subTest(local_url=local_url):
                    normalized_options = bridge._validate_layer_uri_options(
                        {"url": local_url},
                        layer_type="vector",
                        provider="ogr",
                    )
                    self.assertEqual(
                        normalized_options["url"],
                        str(source.resolve()),
                    )
            network_options = {"url": "http://example.test/roads.gpkg"}
            self.assertEqual(
                bridge._validate_layer_uri_options(
                    network_options,
                    layer_type="vector",
                    provider="ogr",
                ),
                network_options,
            )
            local_provider = f"dbname='{source}' table=roads"
            self.assertEqual(
                bridge._validate_layer_source_path(local_provider),
                local_provider,
            )
            local_provider_options = {"dbname": str(source)}
            self.assertEqual(
                bridge._validate_layer_uri_options(
                    local_provider_options,
                    layer_type="vector",
                    provider="spatialite",
                ),
                local_provider_options,
            )
            database_provider = "host=127.0.0.1 dbname=qgis table=roads"
            self.assertEqual(
                bridge._validate_layer_source_path(database_provider),
                database_provider,
            )
            for connection_provider in (
                "host=127.0.0.1 dbname='tenant:2026' table=roads",
                "service=qgis dbname=qgis.prod table=roads",
            ):
                self.assertEqual(
                    bridge._validate_layer_source_path(connection_provider),
                    connection_provider,
                )
            connection_options = {"host": "127.0.0.1", "dbname": "tenant:2026"}
            self.assertEqual(
                bridge._validate_layer_uri_options(
                    connection_options,
                    layer_type="vector",
                    provider="postgres",
                ),
                connection_options,
            )
            provider_bound_options = {"dbname": "tenant:2026"}
            self.assertEqual(
                bridge._validate_layer_uri_options(
                    provider_bound_options,
                    layer_type="vector",
                    provider="postgres",
                ),
                provider_bound_options,
            )
            provider_bound_source = "dbname='tenant:2026' table=roads"
            self.assertEqual(
                bridge._validate_layer_source_path(
                    provider_bound_source,
                    layer_type="vector",
                    provider="postgres",
                ),
                provider_bound_source,
            )
            local_datasource = f"GPKG:{source}"
            self.assertEqual(
                bridge._validate_layer_source_path(local_datasource),
                local_datasource,
            )
            nested = root / "layers"
            nested.mkdir()
            equals_path = nested / "dbname=data.gpkg"
            equals_path.write_bytes(b"data")
            previous_directory = Path.cwd()
            try:
                os.chdir(root)
                self.assertEqual(
                    bridge._validate_layer_source_path("layers/dbname=data.gpkg"),
                    str(equals_path.resolve()),
                )
            finally:
                os.chdir(previous_directory)

            blocked_sources = (
                r"dbname='\\server.invalid\share\roads.gpkg' table=roads",
                r"dbname='\\?\C:\roads.gpkg' table=roads",
                f"dbname='{source}:stream' table=roads",
                r"GPKG:\\server.invalid\share\roads.gpkg",
                r"SQLite:\\?\C:\roads.sqlite",
                f"GPKG:{source}:stream",
            )
            for blocked in blocked_sources:
                with self.subTest(blocked=blocked), self.assertRaises(PermissionError):
                    bridge._validate_layer_source_path(blocked)
            blocked_options = (
                {"url": r"\\server.invalid\share\roads.gpkg"},
                {"url": r"\\?\C:\roads.gpkg"},
                {"url": f"{source}:stream"},
                {"dbname": r"\\server.invalid\share\roads.gpkg"},
                {"DBNAME": r"\\?\C:\roads.gpkg"},
                {"%64bname": f"{source}:stream"},
            )
            for options in blocked_options:
                with self.subTest(options=options), self.assertRaises(PermissionError):
                    bridge._validate_layer_uri_options(
                        options,
                        layer_type="vector",
                        provider="ogr",
                    )

    def test_interactive_overwrite_preview_is_bound_one_use_and_rechecked(self):
        from qcopilots_common.bridge import (
            DestructiveActionConfirmationStore,
            _authorize_interactive_overwrite,
            _publish_staged_output,
        )

        tokens = iter(("overwrite-token-0001", "overwrite-token-0002"))
        store = DestructiveActionConfirmationStore(
            token_factory=lambda: next(tokens)
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            target = root / "map.png"
            target.write_bytes(b"original")
            arguments = {"overwrite": True}
            preview, _ = _authorize_interactive_overwrite(
                store,
                arguments,
                target,
                "export_map_image",
                "Map output",
                binding={"path": str(target), "dpi": 96},
            )
            token = preview["overwrite_confirmation_token"]
            with self.assertRaisesRegex(RuntimeError, "changed after preview"):
                _authorize_interactive_overwrite(
                    store,
                    {**arguments, "overwrite_confirmation_token": token},
                    target,
                    "export_map_image",
                    "Map output",
                    binding={"path": str(target), "dpi": 300},
                )
            with self.assertRaisesRegex(RuntimeError, "already used"):
                _authorize_interactive_overwrite(
                    store,
                    {**arguments, "overwrite_confirmation_token": token},
                    target,
                    "export_map_image",
                    "Map output",
                    binding={"path": str(target), "dpi": 96},
                )

            preview, _ = _authorize_interactive_overwrite(
                store,
                arguments,
                target,
                "export_map_image",
                "Map output",
                binding={"path": str(target), "dpi": 96},
            )
            _, expected = _authorize_interactive_overwrite(
                store,
                {
                    **arguments,
                    "overwrite_confirmation_token": preview[
                        "overwrite_confirmation_token"
                    ],
                },
                target,
                "export_map_image",
                "Map output",
                binding={"path": str(target), "dpi": 96},
            )
            staged = root / "staged.png"
            staged.write_bytes(b"replacement")
            target.write_bytes(b"concurrent")
            with self.assertRaisesRegex(RuntimeError, "changed after overwrite"):
                _publish_staged_output(
                    staged,
                    target,
                    overwrite=True,
                    expected_versions=expected,
                )
            self.assertEqual(target.read_bytes(), b"concurrent")
            self.assertEqual(staged.read_bytes(), b"replacement")

    def test_interactive_family_publish_rolls_back_partial_overwrite(self):
        import qcopilots_common.bridge as bridge
        import qcopilots_common.processing_jobs as processing_jobs

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            target = root / "map.png"
            world = root / "map.pgw"
            target.write_bytes(b"original-image")
            world.write_bytes(b"original-world")
            store = bridge.DestructiveActionConfirmationStore()
            preview, _ = bridge._authorize_interactive_overwrite(
                store,
                {"overwrite": True},
                target,
                "export_map_image",
                "Map output",
                binding={"path": str(target), "encoder": "PNG"},
                family_paths=[target, world],
            )
            _, expected = bridge._authorize_interactive_overwrite(
                store,
                {
                    "overwrite": True,
                    "overwrite_confirmation_token": preview[
                        "overwrite_confirmation_token"
                    ],
                },
                target,
                "export_map_image",
                "Map output",
                binding={"path": str(target), "encoder": "PNG"},
                family_paths=[target, world],
            )
            staging_root = root / "stage"
            staging_root.mkdir()
            (staging_root / target.name).write_bytes(b"new-image")
            (staging_root / world.name).write_bytes(b"new-world")
            real_link = processing_jobs._link_processing_file_no_clobber
            calls = 0

            def fail_second_publish(source, destination):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise PermissionError("simulated family publish failure")
                return real_link(source, destination)

            with mock.patch.object(
                processing_jobs,
                "_link_processing_file_no_clobber",
                side_effect=fail_second_publish,
            ), self.assertRaisesRegex(PermissionError, "simulated family"):
                bridge._publish_staged_output_family(
                    staging_root,
                    target,
                    [target, world],
                    overwrite=True,
                    expected_versions=expected,
                )
            self.assertEqual(target.read_bytes(), b"original-image")
            self.assertEqual(world.read_bytes(), b"original-world")
            self.assertEqual(list(root.glob(".*.qcopilots-backup-*")), [])

    def test_single_output_publish_preserves_concurrent_intruder(self):
        import qcopilots_common.bridge as bridge
        import qcopilots_common.processing_jobs as processing_jobs

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            target = root / "project.qgz"
            staged = root / "staged.qgz"
            staged.write_bytes(b"job-output")
            _, expected = bridge._authorize_interactive_overwrite(
                bridge.DestructiveActionConfirmationStore(),
                {},
                target,
                "save_project",
                "Project output",
                binding={"path": str(target)},
            )
            real_link = processing_jobs._link_processing_file_no_clobber

            def inject_intruder(source, destination):
                Path(destination).write_bytes(b"intruder")
                return real_link(source, destination)

            with mock.patch.object(
                processing_jobs,
                "_link_processing_file_no_clobber",
                side_effect=inject_intruder,
            ), self.assertRaisesRegex(FileExistsError, "appeared concurrently"):
                bridge._publish_staged_output(
                    staged,
                    target,
                    overwrite=False,
                    expected_versions=expected,
                )
            self.assertEqual(target.read_bytes(), b"intruder")
            self.assertEqual(staged.read_bytes(), b"job-output")
            self.assertEqual(list(root.glob(".*.qcopilots-publish-*")), [])

    def test_interactive_overwrite_rejects_large_file_before_hashing(self):
        import qcopilots_common.bridge as bridge

        with tempfile.TemporaryDirectory() as temporary_directory:
            target = Path(temporary_directory) / "large.qgz"
            with target.open("wb") as handle:
                handle.seek(bridge.MAX_INTERACTIVE_OVERWRITE_HASH_BYTES)
                handle.write(b"x")
            with mock.patch.object(
                bridge,
                "_interactive_file_version",
            ) as version, self.assertRaisesRegex(RuntimeError, "larger than"):
                bridge._authorize_interactive_overwrite(
                    bridge.DestructiveActionConfirmationStore(),
                    {"overwrite": True},
                    target,
                    "save_project",
                    "Project output",
                    binding={"path": str(target)},
                )
            version.assert_not_called()

    def test_map_export_uses_suffix_encoder_and_publishes_world_file(self):
        import qcopilots_common.bridge as bridge

        class Canvas:
            calls = []

            def saveAsImage(self, path, pixmap, encoder):
                self.calls.append((pixmap, encoder))
                Path(path).write_bytes(b"jpeg-image")
                bridge._map_image_world_file_path(Path(path)).write_bytes(
                    b"world-file"
                )

        canvas = Canvas()
        tools = bridge.QgisBridgeTools(
            types.SimpleNamespace(mapCanvas=lambda: canvas)
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            target = root / "map.jpg"
            result = tools.export_map_image({"path": str(target)})
            self.assertEqual(canvas.calls, [(None, "JPEG")])
            self.assertEqual(target.read_bytes(), b"jpeg-image")
            self.assertEqual((root / "map.jgw").read_bytes(), b"world-file")
            self.assertEqual(result["world_file_path"], str(root / "map.jgw"))
            self.assertTrue(result["cleanup"]["complete"])
            self.assertEqual(list(root.glob(".*.qcopilots-stage-*")), [])

    def test_map_export_reports_staging_cleanup_residual(self):
        import qcopilots_common.bridge as bridge

        class Canvas:
            def saveAsImage(self, path, pixmap, encoder):
                del pixmap, encoder
                Path(path).write_bytes(b"image")
                bridge._map_image_world_file_path(Path(path)).write_bytes(b"world")

        tools = bridge.QgisBridgeTools(
            types.SimpleNamespace(mapCanvas=lambda: Canvas())
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            target = root / "map.png"
            real_rmtree = bridge.shutil.rmtree
            with mock.patch.object(
                bridge.shutil,
                "rmtree",
                side_effect=PermissionError("locked staging directory"),
            ):
                result = tools.export_map_image({"path": str(target)})
            self.assertFalse(result["cleanup"]["complete"])
            self.assertTrue(result["cleanup"]["retry_recommended"])
            self.assertEqual(len(result["cleanup"]["residual_paths"]), 1)
            residual = Path(result["cleanup"]["residual_paths"][0])
            self.assertTrue(residual.is_dir())
            real_rmtree(residual)
            self.assertFalse(residual.exists())

    def test_interactive_cleanup_verifies_removal_completed(self):
        import qcopilots_common.bridge as bridge

        with tempfile.TemporaryDirectory() as temporary_directory:
            residual = Path(temporary_directory) / "staging"
            residual.mkdir()
            (residual / "partial.bin").write_bytes(b"partial")
            with mock.patch.object(bridge.shutil, "rmtree"):
                paths = bridge._cleanup_interactive_paths([residual])
            self.assertEqual(paths, [str(residual)])
            self.assertTrue(residual.exists())

    def test_atomic_vector_edit_rolls_back_failed_operation(self):
        from qcopilots_common.bridge import _atomic_vector_layer_edit

        class FakeLayer:
            def __init__(self):
                self.editable = False
                self.value = "original"
                self.snapshot = None
                self.destroyed = 0
                self.rolled_back = 0

            def isEditable(self):
                return self.editable

            def startEditing(self):
                self.editable = True
                return True

            def beginEditCommand(self, label):
                del label
                self.snapshot = self.value

            def endEditCommand(self):
                pass

            def destroyEditCommand(self):
                self.value = self.snapshot
                self.destroyed += 1

            def rollBack(self):
                self.value = self.snapshot
                self.editable = False
                self.rolled_back += 1
                return True

        layer = FakeLayer()

        def fail_after_change():
            layer.value = "partial"
            raise RuntimeError("provider rejected edit")

        with self.assertRaisesRegex(RuntimeError, "provider rejected"):
            _atomic_vector_layer_edit(layer, "atomic test", fail_after_change)
        self.assertEqual(layer.value, "original")
        self.assertEqual(layer.destroyed, 1)
        self.assertEqual(layer.rolled_back, 1)

    def test_atomic_vector_edit_rolls_back_partial_add_update_and_delete(self):
        from qcopilots_common.bridge import _atomic_vector_layer_edit

        class FakeLayer:
            def __init__(self, values):
                self.editable = False
                self.values = list(values)
                self.snapshot = None

            def isEditable(self):
                return self.editable

            def startEditing(self):
                self.editable = True
                return True

            def beginEditCommand(self, label):
                del label
                self.snapshot = list(self.values)

            def endEditCommand(self):
                pass

            def destroyEditCommand(self):
                self.values = list(self.snapshot)

            def rollBack(self):
                self.values = list(self.snapshot)
                self.editable = False
                return True

        cases = (
            ("add", ["one"], lambda values: values.append("partial")),
            ("update", ["one", "two"], lambda values: values.__setitem__(0, "partial")),
            ("delete", ["one", "two"], lambda values: values.pop(0)),
        )
        for label, original, partial_operation in cases:
            with self.subTest(operation=label):
                layer = FakeLayer(original)

                def fail_after_partial_change():
                    partial_operation(layer.values)
                    raise RuntimeError(f"partial {label} failed")

                with self.assertRaisesRegex(RuntimeError, f"partial {label}"):
                    _atomic_vector_layer_edit(
                        layer,
                        f"QCopilots {label}",
                        fail_after_partial_change,
                    )
                self.assertEqual(layer.values, original)
                self.assertFalse(layer.isEditable())

    def test_atomic_vector_edit_never_attempts_failing_provider_commit(self):
        from qcopilots_common.bridge import (
            _atomic_vector_layer_edit,
            _vector_edit_result_state,
        )

        class FakeLayer:
            def __init__(self):
                self.editable = False
                self.values = []
                self.commit_calls = 0

            def isEditable(self):
                return self.editable

            def startEditing(self):
                self.editable = True
                return True

            def beginEditCommand(self, label):
                del label

            def endEditCommand(self):
                pass

            def commitChanges(self):
                self.commit_calls += 1
                return False

        layer = FakeLayer()
        started = _atomic_vector_layer_edit(
            layer,
            "QCopilots staged edit",
            lambda: layer.values.append("staged"),
        )
        self.assertTrue(started)
        self.assertEqual(layer.values, ["staged"])
        self.assertTrue(layer.isEditable())
        self.assertEqual(layer.commit_calls, 0)
        self.assertEqual(
            _vector_edit_result_state(started),
            {
                "applied": True,
                "staged": True,
                "committed": False,
                "edit_state": "staged_in_new_edit_session",
                "edit_buffer_atomic": True,
                "provider_commit_attempted": False,
                "requires_user_commit": True,
            },
        )

    def test_layer_removal_fingerprint_enforces_layer_count_budget(self):
        import qcopilots_common.bridge as bridge

        with self.assertRaisesRegex(RuntimeError, "layer count limit"):
            bridge._layer_removal_fingerprint(
                [None] * (bridge.MAX_LAYER_REMOVAL_LAYERS + 1)
            )

    def test_layer_removal_edit_policy_rejects_saves_discards_and_reports_failures(self):
        import qcopilots_common.bridge as bridge

        class FakeLayer:
            def __init__(self, layer_id, *, commit_result=True, rollback_result=True):
                self.layer_id = layer_id
                self.editable = True
                self.modified = True
                self.commit_result = commit_result
                self.rollback_result = rollback_result
                self.commit_calls = 0
                self.rollback_calls = 0

            def id(self):
                return self.layer_id

            def isEditable(self):
                return self.editable

            def isModified(self):
                return self.modified

            @staticmethod
            def editBuffer():
                return None

            def commitChanges(self):
                self.commit_calls += 1
                if isinstance(self.commit_result, Exception):
                    raise self.commit_result
                if self.commit_result:
                    self.editable = False
                    self.modified = False
                return self.commit_result

            def rollBack(self):
                self.rollback_calls += 1
                if isinstance(self.rollback_result, Exception):
                    raise self.rollback_result
                if self.rollback_result:
                    self.editable = False
                    self.modified = False
                return self.rollback_result

        rejected = FakeLayer("reject-layer")
        with self.assertRaisesRegex(RuntimeError, "removal rejected"):
            bridge._apply_layer_removal_edit_policy([rejected], "reject")
        self.assertEqual(rejected.commit_calls, 0)
        self.assertEqual(rejected.rollback_calls, 0)
        self.assertTrue(rejected.isEditable())

        saved = FakeLayer("save-layer")
        self.assertEqual(
            bridge._apply_layer_removal_edit_policy([saved], "save"),
            ["save-layer"],
        )
        self.assertEqual(saved.commit_calls, 1)
        self.assertEqual(saved.rollback_calls, 0)
        self.assertFalse(saved.isEditable())

        discarded = FakeLayer("discard-layer")
        self.assertEqual(
            bridge._apply_layer_removal_edit_policy([discarded], "discard"),
            ["discard-layer"],
        )
        self.assertEqual(discarded.commit_calls, 0)
        self.assertEqual(discarded.rollback_calls, 1)
        self.assertFalse(discarded.isEditable())

        for multi_policy in ("save", "discard"):
            first = FakeLayer(f"{multi_policy}-first")
            second = FakeLayer(f"{multi_policy}-second")
            with self.subTest(multi_editable_policy=multi_policy), self.assertRaisesRegex(
                RuntimeError,
                "cannot safely process more than one editable layer",
            ):
                bridge._apply_layer_removal_edit_policy(
                    [first, second],
                    multi_policy,
                )
            self.assertEqual(first.commit_calls, 0)
            self.assertEqual(first.rollback_calls, 0)
            self.assertEqual(second.commit_calls, 0)
            self.assertEqual(second.rollback_calls, 0)

        save_failure = FakeLayer("save-failure", commit_result=False)
        with self.assertRaisesRegex(RuntimeError, "Could not save.*save-failure"):
            bridge._apply_layer_removal_edit_policy([save_failure], "save")
        self.assertTrue(save_failure.isEditable())

        discard_failure = FakeLayer(
            "discard-failure",
            rollback_result=RuntimeError("provider failure"),
        )
        with self.assertRaisesRegex(
            RuntimeError,
            "Could not discard.*discard-failure",
        ):
            bridge._apply_layer_removal_edit_policy(
                [discard_failure],
                "discard",
            )
        self.assertTrue(discard_failure.isEditable())

    def test_dispatch_timeout_after_start_reports_indeterminate_audit_id(self):
        import qcopilots_common.bridge as bridge

        dispatcher = bridge._QtMainThreadDispatcher.__new__(
            bridge._QtMainThreadDispatcher
        )
        dispatcher._closed = threading.Event()
        dispatcher._pending_lock = threading.Lock()
        dispatcher._pending = {}
        application_thread = object()
        worker_thread = object()
        dispatcher._app = types.SimpleNamespace(
            thread=lambda: application_thread
        )
        dispatcher._qthread = types.SimpleNamespace(
            currentThread=lambda: worker_thread
        )

        class RequestSignal:
            @staticmethod
            def emit(payload):
                payload["started"] = True

        dispatcher._object = types.SimpleNamespace(request=RequestSignal())
        with mock.patch.object(
            bridge.secrets,
            "token_hex",
            return_value="0123456789abcdef0123456789abcdef",
        ):
            with self.assertRaisesRegex(
                TimeoutError,
                "indeterminate_outcome=true.*audit_identifier=0123456789abcdef",
            ):
                dispatcher.call(lambda: None, timeout_seconds=0.001)
        self.assertEqual(dispatcher._pending, {})

    @unittest.skipUnless(_qgis_bindings_available(), "QGIS bindings are not available")
    def test_real_dirty_layer_removal_labels_and_custom_crs_round_trip(self):
        from qgis.core import (
            QgsCoordinateReferenceSystem,
            QgsFeature,
            QgsProject,
            QgsVectorLayer,
        )
        from qgis.testing import start_app
        import qcopilots_common.bridge as bridge
        from qcopilots_common.bridge import QgisBridgeTools

        start_app()
        project = QgsProject.instance()
        previous_crs = project.crs()
        layer = QgsVectorLayer(
            "Point?crs=EPSG:4326&field=name:string",
            "QCopilots dirty removal test",
            "memory",
        )
        project.addMapLayer(layer)
        layer_id = layer.id()
        clean_layer_id = None
        failure_layer_id = None
        save_layer_id = None
        second_edit_layer_id = None
        label_layer_id = None
        tools = QgisBridgeTools(None)
        try:
            clean_layer = QgsVectorLayer(
                "Point?crs=EPSG:4326&field=name:string",
                "QCopilots clean removal test",
                "memory",
            )
            project.addMapLayer(clean_layer)
            clean_layer_id = clean_layer.id()
            clean_preview = tools.interactive_layer_remove_layers(
                {"layer_id": clean_layer_id}
            )
            self.assertTrue(clean_preview["preview"])
            with self.assertRaisesRegex(RuntimeError, "only confirmation_token"):
                tools.interactive_layer_remove_layers(
                    {
                        "confirmation_token": clean_preview["confirmation_token"],
                        "allow_multiple": False,
                    }
                )
            clean_removed = tools.interactive_layer_remove_layers(
                {"confirmation_token": clean_preview["confirmation_token"]}
            )
            self.assertEqual(clean_removed["removed_count"], 1)
            self.assertEqual(clean_removed["editable_changes"], "reject")
            self.assertEqual(clean_removed["editable_layers_processed"], [])
            self.assertIsNone(project.mapLayer(clean_layer_id))

            self.assertTrue(layer.startEditing())
            feature = QgsFeature(layer.fields())
            feature.setAttribute("name", "first")
            self.assertTrue(layer.addFeature(feature))
            preview = tools.interactive_layer_remove_layers(
                {"layer_id": layer.id()}
            )
            self.assertTrue(preview["preview"])
            self.assertEqual(
                preview["layers"][0]["edit_state"]["added_feature_count"], 1
            )
            changed = QgsFeature(layer.fields())
            changed.setAttribute("name", "changed-after-preview")
            self.assertTrue(layer.addFeature(changed))
            with self.assertRaisesRegex(RuntimeError, "changed after removal preview"):
                tools.interactive_layer_remove_layers(
                    {"confirmation_token": preview["confirmation_token"]}
                )
            self.assertIsNotNone(project.mapLayer(layer.id()))

            second_edit_layer = QgsVectorLayer(
                "Point?crs=EPSG:4326&field=name:string",
                "QCopilots second edit policy test",
                "memory",
            )
            project.addMapLayer(second_edit_layer)
            second_edit_layer_id = second_edit_layer.id()
            self.assertTrue(second_edit_layer.startEditing())
            for multi_policy in ("save", "discard"):
                with self.subTest(
                    real_multi_editable_policy=multi_policy
                ), self.assertRaisesRegex(
                    RuntimeError,
                    "cannot safely process more than one editable layer",
                ):
                    tools.interactive_layer_remove_layers(
                        {
                            "layer_ids": [layer_id, second_edit_layer_id],
                            "editable_changes": multi_policy,
                        }
                    )
                self.assertTrue(layer.isEditable())
                self.assertTrue(second_edit_layer.isEditable())

            reject_preview = tools.interactive_layer_remove_layers(
                {"layer_id": layer.id()}
            )
            self.assertEqual(reject_preview["editable_changes"], "reject")
            with self.assertRaisesRegex(RuntimeError, "only confirmation_token"):
                tools.interactive_layer_remove_layers(
                    {
                        "confirmation_token": reject_preview["confirmation_token"],
                        "editable_changes": "discard",
                    }
                )
            with self.assertRaisesRegex(RuntimeError, "removal rejected"):
                tools.interactive_layer_remove_layers(
                    {"confirmation_token": reject_preview["confirmation_token"]}
                )
            self.assertIsNotNone(project.mapLayer(layer_id))
            self.assertTrue(layer.isEditable())
            with self.assertRaisesRegex(RuntimeError, "already used"):
                tools.interactive_layer_remove_layers(
                    {"confirmation_token": reject_preview["confirmation_token"]}
                )

            discard_preview = tools.interactive_layer_remove_layers(
                {
                    "layer_id": layer.id(),
                    "editable_changes": "discard",
                }
            )
            self.assertEqual(discard_preview["editable_changes"], "discard")
            removed = tools.interactive_layer_remove_layers(
                {"confirmation_token": discard_preview["confirmation_token"]}
            )
            self.assertEqual(removed["removed_count"], 1)
            self.assertEqual(removed["editable_changes"], "discard")
            self.assertEqual(removed["editable_layers_processed"], [layer_id])
            self.assertIsNone(project.mapLayer(layer_id))
            with self.assertRaisesRegex(RuntimeError, "already used"):
                tools.interactive_layer_remove_layers(
                    {"confirmation_token": discard_preview["confirmation_token"]}
                )

            failure_layer = QgsVectorLayer(
                "Point?crs=EPSG:4326&field=name:string",
                "QCopilots failed edit handling test",
                "memory",
            )
            project.addMapLayer(failure_layer)
            failure_layer_id = failure_layer.id()
            self.assertTrue(failure_layer.startEditing())
            failure_preview = tools.interactive_layer_remove_layers(
                {
                    "layer_id": failure_layer_id,
                    "editable_changes": "save",
                }
            )
            with mock.patch.object(
                bridge,
                "_apply_layer_removal_edit_policy",
                side_effect=RuntimeError("edit policy failed"),
            ), self.assertRaisesRegex(RuntimeError, "edit policy failed"):
                tools.interactive_layer_remove_layers(
                    {"confirmation_token": failure_preview["confirmation_token"]}
                )
            self.assertIsNotNone(project.mapLayer(failure_layer_id))
            self.assertTrue(failure_layer.isEditable())

            save_layer = QgsVectorLayer(
                "Point?crs=EPSG:4326&field=name:string",
                "QCopilots save before removal test",
                "memory",
            )
            project.addMapLayer(save_layer)
            save_layer_id = save_layer.id()
            self.assertTrue(save_layer.startEditing())
            saved_feature = QgsFeature(save_layer.fields())
            saved_feature.setAttribute("name", "saved")
            self.assertTrue(save_layer.addFeature(saved_feature))
            save_preview = tools.interactive_layer_remove_layers(
                {
                    "layer_id": save_layer_id,
                    "editable_changes": "save",
                }
            )
            save_removed = tools.interactive_layer_remove_layers(
                {"confirmation_token": save_preview["confirmation_token"]}
            )
            self.assertEqual(save_removed["removed_count"], 1)
            self.assertEqual(save_removed["editable_changes"], "save")
            self.assertEqual(
                save_removed["editable_layers_processed"],
                [save_layer_id],
            )
            self.assertIsNone(project.mapLayer(save_layer_id))

            label_layer = QgsVectorLayer(
                "Point?crs=EPSG:4326&field=name:string",
                "QCopilots labels test",
                "memory",
            )
            project.addMapLayer(label_layer)
            label_layer_id = label_layer.id()
            with self.assertRaisesRegex(RuntimeError, "Unknown fields"):
                tools.interactive_layer_configure_labels(
                    {
                        "layer_id": label_layer.id(),
                        "field_or_expression": '"missing_field"',
                        "is_expression": True,
                    }
                )
            configured = tools.interactive_layer_configure_labels(
                {
                    "layer_id": label_layer.id(),
                    "field_or_expression": 'upper("name")',
                    "is_expression": True,
                }
            )
            self.assertTrue(configured["labels_enabled"])

            custom = QgsCoordinateReferenceSystem.fromProj(
                "+proj=aeqd +lat_0=12.345 +lon_0=67.89 +datum=WGS84 "
                "+units=m +no_defs"
            )
            self.assertTrue(custom.isValid())
            changed_crs = tools.interactive_project_set_crs(
                {"crs": custom.toWkt()}
            )["crs"]
            observed_crs = tools.interactive_project_get_crs({})["crs"]
            self.assertTrue(changed_crs["wkt"])
            self.assertFalse(changed_crs["wkt_truncated"])
            self.assertEqual(observed_crs["wkt"], changed_crs["wkt"])
            self.assertTrue(
                QgsCoordinateReferenceSystem(observed_crs["wkt"]).isValid()
            )
        finally:
            project.setCrs(previous_crs)
            for candidate_id in (
                layer_id,
                clean_layer_id,
                failure_layer_id,
                save_layer_id,
                second_edit_layer_id,
                label_layer_id,
            ):
                candidate = project.mapLayer(candidate_id) if candidate_id else None
                if candidate is not None:
                    if candidate.isEditable():
                        candidate.rollBack()
                    project.removeMapLayer(candidate_id)

    @unittest.skipUnless(_qgis_bindings_available(), "QGIS bindings are not available")
    def test_real_layout_create_export_and_overwrite_failure_restore(self):
        from qgis.core import QgsProject
        from qgis.testing import start_app
        import qcopilots_common.bridge as bridge

        start_app()
        project = QgsProject.instance()
        manager = project.layoutManager()
        tools = bridge.QgisBridgeTools(None)
        layout_name = "QCopilots constrained layout test"
        old_layout = manager.layoutByName(layout_name)
        if old_layout is not None:
            manager.removeLayout(old_layout)
        try:
            created = tools.interactive_layout_create(
                {
                    "name": layout_name,
                    "title": "Verification layout",
                    "page_size": "A4",
                    "orientation": "landscape",
                    "map_extent": [0, 0, 10, 10],
                    "include_legend": False,
                    "include_scale_bar": False,
                }
            )
            self.assertTrue(created["created"])
            self.assertIn(
                layout_name,
                [item["name"] for item in tools.interactive_layout_list({})["layouts"]],
            )
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                output = root / "layout.pdf"
                first = tools.interactive_layout_export(
                    {
                        "layout_name": layout_name,
                        "path": str(output),
                        "format": "pdf",
                        "dpi": 96,
                    }
                )
                self.assertTrue(first["saved"])
                original = output.read_bytes()
                preview = tools.interactive_layout_export(
                    {
                        "layout_name": layout_name,
                        "path": str(output),
                        "format": "pdf",
                        "dpi": 96,
                        "overwrite": True,
                    }
                )
                with mock.patch.object(
                    bridge,
                    "_publish_staged_output",
                    side_effect=RuntimeError("publish failed"),
                ):
                    with self.assertRaisesRegex(RuntimeError, "publish failed"):
                        tools.interactive_layout_export(
                            {
                                "layout_name": layout_name,
                                "path": str(output),
                                "format": "pdf",
                                "dpi": 96,
                                "overwrite": True,
                                "overwrite_confirmation_token": preview[
                                    "overwrite_confirmation_token"
                                ],
                            }
                        )
                self.assertEqual(output.read_bytes(), original)
                self.assertEqual(list(root.glob(".*.qcopilots-*")), [])
                with self.assertRaisesRegex(RuntimeError, "format must match"):
                    tools.interactive_layout_export(
                        {
                            "layout_name": layout_name,
                            "path": str(root / "mismatch.jpg"),
                            "format": "png",
                        }
                    )
                with mock.patch.object(bridge, "MAX_LAYOUT_EXPORT_PIXELS", 1):
                    with self.assertRaisesRegex(RuntimeError, "pixel limit"):
                        tools.interactive_layout_export(
                            {
                                "layout_name": layout_name,
                                "path": str(root / "too-large.pdf"),
                                "dpi": 96,
                            }
                        )
                self.assertFalse((root / "too-large.pdf").exists())
        finally:
            layout = manager.layoutByName(layout_name)
            if layout is not None:
                manager.removeLayout(layout)

    @unittest.skipUnless(_qgis_bindings_available(), "QGIS bindings are not available")
    def test_real_project_map_and_vector_outputs_are_staged(self):
        from qgis.core import QgsProject
        from qgis.testing import start_app
        import qcopilots_common.bridge as bridge
        import qcopilots_common.processing_jobs as processing_jobs

        start_app()
        project = QgsProject.instance()
        previous_file_name = project.fileName()
        previous_dirty = bool(project.isDirty())

        class Canvas:
            encoder = None

            def saveAsImage(self, path, pixmap, encoder):
                self.encoder = encoder
                Path(path).write_bytes(b"rendered-map")
                bridge._map_image_world_file_path(Path(path)).write_bytes(
                    b"rendered-world"
                )

            @staticmethod
            def refresh():
                return None

        iface = types.SimpleNamespace(mapCanvas=lambda: Canvas())
        tools = bridge.QgisBridgeTools(iface)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                with self.assertRaisesRegex(RuntimeError, "must end in .qgz"):
                    tools.save_project({"path": str(root / "project.qgs")})
                project_path = root / "project.qgz"
                project_path.write_bytes(b"existing-project")
                project_preview = tools.save_project(
                    {"path": str(project_path), "overwrite": True}
                )
                with mock.patch.object(
                    bridge,
                    "_publish_staged_output",
                    side_effect=RuntimeError("project publish failed"),
                ):
                    with self.assertRaisesRegex(RuntimeError, "project publish failed"):
                        tools.save_project(
                            {
                                "path": str(project_path),
                                "overwrite": True,
                                "overwrite_confirmation_token": project_preview[
                                    "overwrite_confirmation_token"
                                ],
                            }
                        )
                self.assertEqual(project_path.read_bytes(), b"existing-project")
                self.assertEqual(project.fileName(), previous_file_name)
                self.assertEqual(bool(project.isDirty()), previous_dirty)

                map_path = root / "map.png"
                map_path.write_bytes(b"existing-map")
                map_preview = tools.export_map_image(
                    {"path": str(map_path), "overwrite": True}
                )
                exported = tools.export_map_image(
                    {
                        "path": str(map_path),
                        "overwrite": True,
                        "overwrite_confirmation_token": map_preview[
                            "overwrite_confirmation_token"
                        ],
                    }
                )
                self.assertTrue(exported["saved"])
                self.assertEqual(map_path.read_bytes(), b"rendered-map")
                self.assertEqual((root / "map.pgw").read_bytes(), b"rendered-world")
                self.assertEqual(exported["encoder"], "PNG")
                self.assertEqual(
                    exported["world_file_path"], str(root / "map.pgw")
                )
                with self.assertRaisesRegex(RuntimeError, "already used"):
                    tools.export_map_image(
                        {
                            "path": str(map_path),
                            "overwrite": True,
                            "overwrite_confirmation_token": map_preview[
                                "overwrite_confirmation_token"
                            ],
                        }
                    )

                vector_path = root / "points.gpkg"
                vector_arguments = {
                    "name": "QCopilots points",
                    "path": str(vector_path),
                    "geometry_type": "Point",
                    "fields": [{"name": "name", "type": "string"}],
                    "features": [
                        {
                            "attributes": {"name": "alpha"},
                            "geometry_wkt": "POINT (1 2)",
                        }
                    ],
                    "add_to_project": False,
                }
                created = tools.create_vector_layer(vector_arguments)
                self.assertEqual(created["feature_count"], 1)
                original_vector = vector_path.read_bytes()
                replacement_arguments = {
                    **vector_arguments,
                    "features": [
                        {
                            "attributes": {"name": "replacement"},
                            "geometry_wkt": "POINT (3 4)",
                        }
                    ],
                }
                vector_preview = tools.create_vector_layer(
                    {**replacement_arguments, "overwrite": True}
                )
                with mock.patch.object(
                    processing_jobs,
                    "_commit_processing_output_promotion",
                    side_effect=RuntimeError("commit cleanup failed"),
                ):
                    replaced = tools.create_vector_layer(
                        {
                            **replacement_arguments,
                            "overwrite": True,
                            "overwrite_confirmation_token": vector_preview[
                                "overwrite_confirmation_token"
                            ],
                        }
                    )
                self.assertNotEqual(vector_path.read_bytes(), original_vector)
                self.assertFalse(replaced["cleanup"]["complete"])
                self.assertTrue(replaced["cleanup"]["retry_recommended"])
                self.assertIn(
                    "commit cleanup failed",
                    replaced["cleanup"]["errors"][0],
                )
                for residual_path in replaced["cleanup"]["residual_paths"]:
                    residual = Path(residual_path)
                    if residual.is_dir():
                        bridge.shutil.rmtree(residual)
                    else:
                        residual.unlink(missing_ok=True)
                self.assertEqual(list(root.glob(".*.qcopilots-*-*")), [])
                gc.collect()
        finally:
            project.setFileName(previous_file_name)
            project.setDirty(previous_dirty)

    @unittest.skipUnless(_qgis_bindings_available(), "QGIS bindings are not available")
    def test_real_memory_layer_edit_query_selection_and_confirmed_delete(self):
        from qgis.core import QgsGeometry, QgsProject, QgsVectorLayer
        from qgis.testing import start_app
        import qcopilots_common.bridge as bridge
        from qcopilots_common.bridge import QgisBridgeTools

        start_app()
        project = QgsProject.instance()
        layer = QgsVectorLayer(
            "Point?crs=EPSG:4326&field=name:string",
            "QCopilots atomic edit test",
            "memory",
        )
        self.assertTrue(layer.isValid())
        project.addMapLayer(layer)
        try:
            tools = QgisBridgeTools(None)
            added = tools.add_vector_features(
                {
                    "layer_id": layer.id(),
                    "features": [
                        {
                            "attributes": {"name": "alpha"},
                            "geometry_wkt": "POINT (1 2)",
                        }
                    ],
                }
            )
            self.assertFalse(added["committed"])
            self.assertTrue(added["staged"])
            self.assertEqual(added["edit_state"], "staged_in_new_edit_session")
            self.assertFalse(added["provider_commit_attempted"])
            feature_id = added["added_feature_ids"][0]
            self.assertEqual(layer.featureCount(), 1)

            updated = tools.update_vector_features(
                {
                    "layer_id": layer.id(),
                    "updates": [
                        {
                            "feature_id": feature_id,
                            "attributes": {"name": "beta"},
                            "geometry_wkt": "POINT (3 4)",
                        }
                    ],
                }
            )
            self.assertFalse(updated["committed"])
            self.assertTrue(updated["staged"])
            self.assertEqual(updated["edit_state"], "staged_in_existing_edit_session")
            feature = layer.getFeature(feature_id)
            self.assertEqual(feature["name"], "beta")
            self.assertEqual(feature.geometry().asWkt(), QgsGeometry.fromWkt("POINT (3 4)").asWkt())

            queried = tools.interactive_layer_query_features(
                {
                    "layer_id": layer.id(),
                    "filter_expression": '"name" = \'beta\'',
                    "include_geometry": True,
                }
            )
            self.assertEqual(queried["returned_count"], 1)
            self.assertEqual(queried["features"][0]["feature_id"], feature_id)
            self.assertLessEqual(
                queried["response_bytes"], queried["response_byte_limit"]
            )
            with mock.patch.object(
                bridge, "MAX_VECTOR_QUERY_RESPONSE_BYTES", 8
            ), self.assertRaisesRegex(
                RuntimeError, "total feature payload"
            ) as error_context:
                tools.interactive_layer_query_features(
                    {"layer_id": layer.id(), "feature_ids": [feature_id]}
                )
            self.assertIn("code=query_response_too_large", str(error_context.exception))
            self.assertIn("response_byte_limit=8", str(error_context.exception))

            selected = tools.interactive_layer_set_selection(
                {
                    "layer_id": layer.id(),
                    "feature_ids": [feature_id],
                    "mode": "replace",
                }
            )
            self.assertEqual(selected["selected_feature_ids"], [feature_id])

            preview = tools.interactive_layer_delete_features(
                {"layer_id": layer.id(), "feature_ids": [feature_id]}
            )
            self.assertTrue(preview["preview"])
            self.assertEqual(preview["matched_count"], 1)
            with self.assertRaisesRegex(RuntimeError, "cannot be combined"):
                tools.interactive_layer_delete_features(
                    {
                        "layer_id": layer.id(),
                        "feature_ids": [feature_id],
                        "confirmation_token": preview["confirmation_token"],
                    }
                )
            deleted = tools.interactive_layer_delete_features(
                {
                    "layer_id": layer.id(),
                    "confirmation_token": preview["confirmation_token"],
                }
            )
            self.assertFalse(deleted["committed"])
            self.assertTrue(deleted["applied"])
            self.assertTrue(deleted["staged"])
            self.assertFalse(deleted["provider_commit_attempted"])
            self.assertEqual(layer.featureCount(), 0)
            with self.assertRaisesRegex(RuntimeError, "already used"):
                tools.interactive_layer_delete_features(
                    {
                        "layer_id": layer.id(),
                        "confirmation_token": preview["confirmation_token"],
                    }
                )

            staged = tools.add_vector_features(
                {
                    "layer_id": layer.id(),
                    "features": [
                        {
                            "attributes": {"name": "staged"},
                            "geometry_wkt": "POINT (5 6)",
                        }
                    ],
                }
            )
            self.assertTrue(staged["applied"])
            self.assertTrue(staged["staged"])
            self.assertFalse(staged["committed"])
            self.assertEqual(
                staged["edit_state"],
                "staged_in_existing_edit_session",
            )
            self.assertTrue(layer.rollBack())
            self.assertEqual(layer.featureCount(), 0)
        finally:
            if layer.isEditable():
                layer.rollBack()
            project.removeMapLayer(layer.id())

    def test_interactive_layer_bridge_helpers_prepare_common_layer_sources(self):
        import qcopilots_common.bridge as bridge

        xyz_uri = bridge._layer_source_uri("xyz", TILE_URL, "wms", {})
        vector_tile_uri = bridge._layer_source_uri(
            "vector_tile",
            TILE_URL.replace(".png", ".pbf"),
            "vectortile",
            {},
        )
        mbtiles_uri = bridge._layer_source_uri(
            "vector_tile",
            "data/world.mbtiles",
            "vectortile",
            {},
        )
        xyz_options_uri = bridge._layer_source_uri(
            "xyz",
            TILE_URL,
            "wms",
            {"zmax": 12},
        )

        self.assertEqual(parse_qs(xyz_uri)["type"], ["xyz"])
        self.assertEqual(parse_qs(xyz_uri)["url"], [TILE_URL])
        self.assertEqual(parse_qs(xyz_uri)["zmin"], ["0"])
        self.assertEqual(parse_qs(xyz_uri)["zmax"], ["19"])
        self.assertEqual(parse_qs(vector_tile_uri)["type"], ["xyz"])
        self.assertEqual(
            parse_qs(vector_tile_uri)["url"],
            [TILE_URL.replace(".png", ".pbf")],
        )
        self.assertEqual(parse_qs(mbtiles_uri)["type"], ["mbtiles"])
        mbtiles_path = parse_qs(mbtiles_uri)["url"][0].replace("\\", "/")
        self.assertTrue(mbtiles_path.endswith("data/world.mbtiles"))
        self.assertEqual(parse_qs(xyz_options_uri)["type"], ["xyz"])
        self.assertEqual(parse_qs(xyz_options_uri)["url"], [TILE_URL])
        self.assertEqual(parse_qs(xyz_options_uri)["zmin"], ["0"])
        self.assertEqual(parse_qs(xyz_options_uri)["zmax"], ["12"])
        self.assertEqual(
            bridge._normalize_layer_type("vector-tiles"),
            "vector_tile",
        )
        self.assertIn(
            "wcs",
            [
                item["layer_type"]
                for item in bridge._supported_layer_type_descriptions()
            ],
        )

    def test_interactive_layer_provider_aliases_resolve_to_qgis_provider_keys(self):
        import qcopilots_common.bridge as bridge

        self.assertEqual(
            bridge._resolve_layer_type_and_provider("xyz", "xyz", TILE_URL, {}),
            ("xyz", "wms"),
        )
        self.assertEqual(
            bridge._resolve_layer_type_and_provider("raster", "xyz", TILE_URL, {}),
            ("xyz", "wms"),
        )
        self.assertEqual(
            bridge._resolve_layer_type_and_provider("raster", "wms", TILE_URL, {}),
            ("xyz", "wms"),
        )
        self.assertEqual(
            bridge._resolve_layer_type_and_provider(
                "raster",
                "wms",
                WMS_URL,
                {"layers": ["roads"]},
            ),
            ("wms", "wms"),
        )
        self.assertEqual(
            bridge._resolve_layer_type_and_provider(
                "vector",
                "arcgis-feature-server",
                "http://127.0.0.1/arcgis/rest/services/Roads/FeatureServer/0",
                {},
            ),
            ("arcgis_feature_server", "arcgisfeatureserver"),
        )
        self.assertEqual(
            bridge._resolve_layer_type_and_provider("vector_tile", "xyz", TILE_URL, {}),
            ("vector_tile", "vectortile"),
        )
        self.assertEqual(
            bridge._resolve_layer_type_and_provider(
                "raster",
                "arcgis-map-server",
                "http://127.0.0.1/arcgis/rest/services/BaseMap/MapServer",
                {},
            ),
            ("arcgis_map_server", "arcgismapserver"),
        )

    def test_interactive_layer_create_qgis_layer_uses_normalized_provider_aliases(self):
        import qcopilots_common.bridge as bridge

        sentinel = object()
        original_qgis = sys.modules.get("qgis", sentinel)
        original_core = sys.modules.get("qgis.core", sentinel)

        class FakeRasterLayer:
            calls = []

            def __init__(self, source, name, provider):
                self.source = source
                self.name = name
                self.provider = provider
                self.calls.append(
                    {
                        "source": source,
                        "name": name,
                        "provider": provider,
                    }
                )

            def isValid(self):
                return True

        qgis_module = types.ModuleType("qgis")
        qgis_module.__path__ = []
        core_module = types.ModuleType("qgis.core")
        core_module.QgsRasterLayer = FakeRasterLayer
        sys.modules["qgis"] = qgis_module
        sys.modules["qgis.core"] = core_module
        try:
            layer, layer_type, provider, source = bridge._create_qgis_layer(
                "xyz",
                TILE_URL,
                "Tiles",
                "xyz",
                {},
            )
            self.assertIsInstance(layer, FakeRasterLayer)
            self.assertEqual(layer_type, "xyz")
            self.assertEqual(provider, "wms")
            self.assertEqual(FakeRasterLayer.calls[-1]["provider"], "wms")
            self.assertEqual(parse_qs(source)["url"], [TILE_URL])

            layer, layer_type, provider, source = bridge._create_qgis_layer(
                "raster",
                TILE_URL,
                "Tiles",
                "wms",
                {},
            )
            self.assertIsInstance(layer, FakeRasterLayer)
            self.assertEqual(layer_type, "xyz")
            self.assertEqual(provider, "wms")
            self.assertEqual(FakeRasterLayer.calls[-1]["provider"], "wms")
            self.assertEqual(parse_qs(source)["type"], ["xyz"])
        finally:
            if original_qgis is sentinel:
                sys.modules.pop("qgis", None)
            else:
                sys.modules["qgis"] = original_qgis
            if original_core is sentinel:
                sys.modules.pop("qgis.core", None)
            else:
                sys.modules["qgis.core"] = original_core

    def test_interactive_layer_bridge_helpers_prepare_service_url_sources(self):
        import qcopilots_common.bridge as bridge

        with self.assertRaisesRegex(RuntimeError, "uri_options.layers"):
            bridge._layer_source_uri("wms", WMS_URL, "wms", {})
        with self.assertRaisesRegex(RuntimeError, "uri_options.identifier"):
            bridge._layer_source_uri("wcs", WCS_URL, "wcs", {})

        wms_uri = bridge._layer_source_uri(
            "wms",
            WMS_URL,
            "wms",
            {"layers": ["roads"], "crs": "EPSG:3857"},
        )
        wcs_uri = bridge._layer_source_uri(
            "wcs",
            WCS_URL,
            "wcs",
            {"identifier": "elevation"},
        )
        arcgis_uri = bridge._layer_source_uri(
            "arcgis_map_server",
            "http://127.0.0.1/arcgis/rest/services/BaseMap/MapServer",
            "arcgismapserver",
            {},
        )

        self.assertEqual(parse_qs(wms_uri, keep_blank_values=True)["url"], [WMS_URL])
        self.assertEqual(parse_qs(wms_uri, keep_blank_values=True)["layers"], ["roads"])
        self.assertEqual(parse_qs(wms_uri, keep_blank_values=True)["styles"], [""])
        self.assertEqual(parse_qs(wcs_uri)["url"], [WCS_URL])
        self.assertEqual(parse_qs(wcs_uri)["identifier"], ["elevation"])
        self.assertEqual(
            parse_qs(arcgis_uri)["url"],
            ["http://127.0.0.1/arcgis/rest/services/BaseMap/MapServer"],
        )
        self.assertEqual(
            bridge._layer_source_uri(
                "wms",
                "layers=roads&url=http%3A%2F%2F127.0.0.1%2Fwms",
                "wms",
                {},
            ),
            "layers=roads&url=http%3A%2F%2F127.0.0.1%2Fwms",
        )

    def test_interactive_layer_auto_rejects_remote_sources(self):
        import qcopilots_common.bridge as bridge

        with self.assertRaisesRegex(RuntimeError, "auto layer_type"):
            bridge._create_qgis_layer("auto", TILE_URL, "Tiles", "", {})

    def test_interactive_layer_remove_selectors_match_layer_identity(self):
        import qcopilots_common.bridge as bridge

        class FakeLayer:
            def __init__(self, layer_id, name, source):
                self._id = layer_id
                self._name = name
                self._source = source

            def id(self):
                return self._id

            def name(self):
                return self._name

            def source(self):
                return self._source

        encoded_xyz_source = urlencode({"type": "xyz", "url": TILE_URL, "zmax": 19})
        xyz_layer = FakeLayer("xyz-1", "Tiles", encoded_xyz_source)
        layer = FakeLayer("layer-1", "Roads", "C:/Data/roads.gpkg")
        layer_with_same_name = FakeLayer("layer-2", "Roads", "C:/Data/roads-2.gpkg")
        self.assertTrue(
            bridge._layer_matches_selectors(
                xyz_layer,
                bridge._layer_remove_selectors({"source": TILE_URL}),
            )
        )
        self.assertTrue(
            bridge._layer_matches_selectors(
                FakeLayer("xyz-2", "Tiles", TILE_URL),
                bridge._layer_remove_selectors({"source": encoded_xyz_source}),
            )
        )
        selectors = bridge._layer_remove_selectors(
            {
                "layer_ids": ["layer-1"],
                "names": ["Roads"],
                "sources": ["C:/Data/other.gpkg"],
            }
        )

        self.assertTrue(bridge._layer_matches_selectors(layer, selectors))
        self.assertFalse(
            bridge._layer_matches_selectors(
                layer,
                bridge._layer_remove_selectors({"layer_id": "missing"}),
            )
        )
        self.assertTrue(
            bridge._layer_matches_selectors(
                layer,
                bridge._layer_remove_selectors(
                    {"name": "Roads", "source": "C:/Data/roads.gpkg"}
                ),
            )
        )
        self.assertFalse(
            bridge._layer_matches_selectors(
                layer,
                bridge._layer_remove_selectors(
                    {"name": "Roads", "source": "C:/Data/missing.gpkg"}
                ),
            )
        )
        with self.assertRaisesRegex(RuntimeError, "matched multiple layers"):
            bridge._layers_matching_selectors(
                [layer, layer_with_same_name],
                bridge._layer_remove_selectors({"name": "Roads"}),
            )
        self.assertEqual(
            [
                matched.id()
                for matched in bridge._layers_matching_selectors(
                    [layer, layer_with_same_name],
                    bridge._layer_remove_selectors({"name": "Roads"}),
                    allow_multiple=True,
                )
            ],
            ["layer-1", "layer-2"],
        )


if __name__ == "__main__":
    unittest.main()
