from __future__ import annotations

from pathlib import Path

from ._shared_case_base import ProcessingMCPTestBase


class _FakeLayer:
    def __init__(self, layer_id: str) -> None:
        self._layer_id = layer_id

    def id(self) -> str:
        return self._layer_id


class HighfreqProcessingContractsTest(ProcessingMCPTestBase):
    CASES = {
        "mcp_tools_vector_buffer": {"input_layer_ref": "v1", "distance": 10.0},
        "mcp_tools_vector_dissolve": {"input_layer_ref": "v1", "fields": ["name"]},
        "mcp_tools_vector_clip": {"input_layer_ref": "v1", "overlay_layer_ref": "v2"},
        "mcp_tools_vector_intersection": {"input_layer_ref": "v1", "overlay_layer_ref": "v2"},
        "mcp_tools_vector_union": {"input_layer_ref": "v1", "overlay_layer_ref": "v2"},
        "mcp_tools_vector_difference": {"input_layer_ref": "v1", "overlay_layer_ref": "v2"},
        "mcp_tools_vector_fix_geometries": {"input_layer_ref": "v1"},
        "mcp_tools_vector_reproject_layer": {"input_layer_ref": "v1", "target_crs": "EPSG:4326"},
        "mcp_tools_vector_join_attributes_by_location": {
            "input_layer_ref": "v1",
            "join_layer_ref": "v2",
            "predicate": [0],
        },
        "mcp_tools_raster_clip_by_mask": {"raster_layer_ref": "r1", "mask_layer_ref": "v2"},
        "mcp_tools_raster_warp_reproject": {"raster_layer_ref": "r1", "target_crs": "EPSG:3857"},
        "mcp_tools_raster_calculator": {"raster_layer_ref": "r1", "expression": "A*2", "band": 1},
        "mcp_tools_raster_merge": {"raster_layer_refs": ["r1", "r2"]},
        "mcp_tools_raster_reclassify_by_table": {"raster_layer_ref": "r1", "table": [0.0, 10.0, 1.0], "band": 1},
    }

    def _build_stubbed_tools(self):
        tools = self.build_tools()
        call_state: dict[str, object] = {}

        def fake_sanitize(parameters, *, allow_disk_write, allow_in_place_edit):
            call_state["allow_disk_write"] = allow_disk_write
            call_state["allow_in_place_edit"] = allow_in_place_edit
            return parameters, []

        def fake_execute(algorithm, parameters, load_results):
            call_state["algorithm"] = algorithm
            call_state["parameters"] = parameters
            call_state["load_results"] = load_results
            return {"OUTPUT": parameters.get("OUTPUT")}

        tools._sanitize_processing_parameters = fake_sanitize  # type: ignore[method-assign]
        tools._execute_processing_call = fake_execute  # type: ignore[method-assign]
        tools._resolve_layer_ref = lambda ref: _FakeLayer(str(ref))  # type: ignore[method-assign]
        return tools, call_state

    def _invoke(self, tools, tool_name: str, payload: dict[str, object]):
        return getattr(tools, tool_name)(**payload)

    def test_default_output_is_temporary_and_loaded(self):
        for tool_name, payload in self.CASES.items():
            with self.subTest(tool=tool_name):
                tools, state = self._build_stubbed_tools()
                result = self._invoke(tools, tool_name, dict(payload))
                self.assertTrue(result["ok"])
                self.assertEqual(result["tool"], tool_name)
                self.assertEqual(state["parameters"]["OUTPUT"], "TEMPORARY_OUTPUT")
                self.assertTrue(state["load_results"])

    def test_output_write_requires_confirm_write(self):
        for tool_name, payload in self.CASES.items():
            with self.subTest(tool=tool_name):
                tools, _ = self._build_stubbed_tools()
                out = self.make_temp_dir() / f"{tool_name}.gpkg"
                call_payload = dict(payload)
                call_payload["output_path"] = str(out)
                result = self._invoke(tools, tool_name, call_payload)
                self.assertFalse(result["ok"])
                self.assertIn("confirm_write", result["first_error"])

    def test_overwrite_requires_confirm_destructive(self):
        for tool_name, payload in self.CASES.items():
            with self.subTest(tool=tool_name):
                tools, _ = self._build_stubbed_tools()
                out = self.make_temp_dir() / f"{tool_name}.gpkg"
                out.write_text("x", encoding="utf-8")
                call_payload = dict(payload)
                call_payload.update(
                    {
                        "output_path": str(out),
                        "overwrite": True,
                        "confirm_write": True,
                    }
                )
                result = self._invoke(tools, tool_name, call_payload)
                self.assertFalse(result["ok"])
                self.assertIn("confirm_destructive", result["first_error"])

    def test_explicit_output_path_supports_disk_write_when_confirmed(self):
        for tool_name, payload in self.CASES.items():
            with self.subTest(tool=tool_name):
                tools, state = self._build_stubbed_tools()
                out = self.make_temp_dir() / f"{tool_name}.gpkg"
                call_payload = dict(payload)
                call_payload.update({"output_path": str(out), "confirm_write": True})
                result = self._invoke(tools, tool_name, call_payload)
                self.assertTrue(result["ok"])
                self.assertEqual(Path(state["parameters"]["OUTPUT"]), out)
                self.assertTrue(state["allow_disk_write"])

    def test_failure_contains_error_stage_and_first_error(self):
        for tool_name, payload in self.CASES.items():
            with self.subTest(tool=tool_name):
                tools, _ = self._build_stubbed_tools()

                def fake_execute_fail(_algorithm, _parameters, _load_results):
                    raise RuntimeError("boom")

                tools._execute_processing_call = fake_execute_fail  # type: ignore[method-assign]
                result = self._invoke(tools, tool_name, dict(payload))
                self.assertFalse(result["ok"])
                self.assertEqual(result["error_stage"], "execute_processing")
                self.assertIn("boom", result["first_error"])
