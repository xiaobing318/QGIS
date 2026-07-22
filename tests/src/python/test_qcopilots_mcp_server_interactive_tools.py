"""Tests for QCopilots interactive MCP tools.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-19"
__copyright__ = "Copyright 2026, The QGIS Project"

import sys
import types
import unittest
from urllib.parse import parse_qs, urlencode


TILE_URL = "http://127.0.0.1/tiles/{z}/{x}/{y}.png"
WCS_URL = "http://127.0.0.1/wcs"
WMS_URL = "http://127.0.0.1/wms"


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
