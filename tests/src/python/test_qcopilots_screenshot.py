"""Tests for the QCopilots capture_qgis_screenshot tool.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "Ethan.Yang"
__date__ = "2026-11-02"
__copyright__ = "Copyright 2026, The QGIS Project"

import base64
import os
import random
import re
import sys
import threading
import unittest
from pathlib import Path
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))

_QGIS_DLL_DIRECTORY_HANDLES = []


def _prepare_windows_qgis_dll_search_path():
    """Add the active multi-config QGIS build directory before QGIS imports."""

    if sys.platform != "win32":
        return

    prefix_path = os.environ.get("QGIS_PREFIX_PATH", "")
    bin_root = Path(prefix_path).parent if prefix_path else None
    if bin_root is None or not bin_root.is_dir():
        return

    for candidate in sorted(bin_root.iterdir()):
        if not (candidate / "qgis_core.dll").is_file():
            continue
        os.environ["PATH"] = f"{candidate}{os.pathsep}{os.environ.get('PATH', '')}"
        if hasattr(os, "add_dll_directory"):
            _QGIS_DLL_DIRECTORY_HANDLES.append(os.add_dll_directory(str(candidate)))
        return


_prepare_windows_qgis_dll_search_path()

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
JPEG_MAGIC = b"\xff\xd8\xff"
# A 1 x 1 PNG for result formatting tests that do not need Qt.
TINY_PNG_BASE64 = (
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
)
OPERATION = "interactive_capture_screenshot"


def _qgis_bindings_available():
    try:
        import qgis.core  # NOQA
        import qgis.gui  # NOQA

        return True
    except Exception:
        return False


def _assert_plain_text(test, text):
    for line in text.splitlines():
        test.assertIsNone(
            re.match(r"\s*(#|[-*+]\s|\d+[.)]\s|>)", line),
            f"Markdown line start in: {line}",
        )
    for marker in ("**", "__", "`", "|"):
        test.assertNotIn(marker, text)
    test.assertIsNone(re.search(r"\[[^\]]*\]\([^)]*\)", text))


def _sample_bridge_result(**overrides):
    result = {
        "image_base64": TINY_PNG_BASE64,
        "mime_type": "image/png",
        "target": "map_canvas",
        "capture_method": "widget",
        "window_title": "**Draft** [notes](x) `germany__areas` | QGIS",
        "logical_size": [800, 600],
        "device_pixel_ratio": 1.0,
        "image_size": [800, 600],
        "scale": 1.0,
        "format": "png",
        "bytes": 68,
        "qcopilots_panel": "not_visible",
        "region": None,
        "warnings": [],
        "visible_windows": [
            {"title": "# Heading", "class": "QgisApp", "modal": False, "active": True},
            {"title": "Layer Properties", "class": "QgsVectorLayerProperties", "modal": True, "active": False},
        ],
        "canvas": {
            "crs": "EPSG:3035",
            "extent": [4031000.5, 2684000.25, 4672000.0, 3551000.75],
            "scale": 250000.4,
            "rotation": 0.0,
            "is_drawing": False,
        },
    }
    result.update(overrides)
    return result


class _FakeClock:
    def __init__(self):
        self.now = 100.0
        self.sleeps = []

    def __call__(self):
        return self.now

    def sleep(self, seconds):
        self.sleeps.append(seconds)
        self.now += seconds


class TestQCopilotsScreenshotResult(unittest.TestCase):
    """Schema, wrapper and MCP result checks that need no Qt."""

    def _tools(self, bridge_class):
        import qcopilots_common.interactive_layer_tools as interactive_layer_tools
        import qcopilots_common.processing_tools as processing_tools

        with mock.patch.object(processing_tools, "BridgeClient", bridge_class), mock.patch.object(
            interactive_layer_tools, "BridgeClient", bridge_class
        ):
            return processing_tools.build_interactive_tools()

    def test_tool_is_registered_after_export_map_image_with_flat_schema(self):
        from qcopilots_common.mcp_http import ToolError, _validate_tool_arguments

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                return {"tool": tool}

        tools = self._tools(FakeBridgeClient)
        names = [tool.name for tool in tools]
        self.assertEqual(len(names), 34)
        index = names.index("capture_qgis_screenshot")
        self.assertEqual(names[index - 1], "export_map_image")
        self.assertEqual(names[index + 1], "describe_layer_sources")

        tool = tools[index]
        schema = tool.input_schema
        self.assertEqual(tool.descriptor()["inputSchema"], schema)
        self.assertFalse(schema["additionalProperties"])
        self.assertNotIn("required", schema)
        serialized = repr(schema)
        for keyword in ("oneOf", "anyOf", "allOf", "if", "then"):
            self.assertNotIn(f"'{keyword}'", serialized)
        properties = schema["properties"]
        self.assertEqual(
            list(properties),
            [
                "target",
                "layout_name",
                "region",
                "include_qcopilots_panel",
                "capture_method",
                "max_dimension",
                "format",
                "jpeg_quality",
                "wait_for_render_ms",
            ],
        )
        self.assertEqual(
            properties["target"]["enum"],
            ["map_canvas", "main_window", "active_window", "layout_designer"],
        )
        self.assertEqual(properties["target"]["default"], "map_canvas")
        self.assertTrue(properties["include_qcopilots_panel"]["default"])
        self.assertEqual(properties["capture_method"]["enum"], ["widget", "screen"])
        self.assertEqual(properties["capture_method"]["default"], "widget")
        self.assertEqual(properties["region"]["items"]["type"], "integer")
        self.assertEqual(properties["region"]["minItems"], 4)
        self.assertEqual(properties["region"]["maxItems"], 4)
        # The handler needs width and height of at least 1, so does the schema.
        self.assertEqual(
            [item["minimum"] for item in properties["region"]["prefixItems"]],
            [0, 0, 1, 1],
        )
        _validate_tool_arguments({"region": [0, 0, 1, 1]}, schema)
        for region in ([0, 0, 0, 1], [0, 0, 1, 0]):
            with self.subTest(region=region), self.assertRaisesRegex(
                ToolError, "Invalid tool arguments"
            ):
                _validate_tool_arguments({"region": region}, schema)
        self.assertEqual(
            (
                properties["max_dimension"]["minimum"],
                properties["max_dimension"]["maximum"],
                properties["max_dimension"]["default"],
            ),
            (256, 2560, 1568),
        )
        self.assertEqual(properties["format"]["enum"], ["auto", "png", "jpeg"])
        self.assertEqual(
            (
                properties["jpeg_quality"]["minimum"],
                properties["jpeg_quality"]["maximum"],
                properties["jpeg_quality"]["default"],
            ),
            (40, 95, 85),
        )
        self.assertEqual(
            (
                properties["wait_for_render_ms"]["minimum"],
                properties["wait_for_render_ms"]["maximum"],
                properties["wait_for_render_ms"]["default"],
            ),
            (0, 10000, 3000),
        )
        self.assertIn("explicitly asks", tool.description)
        self.assertIn("map_canvas (default", tool.description)

        for arguments in (
            {},
            {"target": "layout_designer", "layout_name": "Atlas"},
            {"region": [0, 0, 10, 10], "include_qcopilots_panel": False},
            {"capture_method": "screen", "format": "jpeg", "jpeg_quality": 40},
            {"max_dimension": 256, "wait_for_render_ms": 0},
        ):
            with self.subTest(arguments=arguments):
                _validate_tool_arguments(arguments, schema)
        for arguments in (
            {"target": "desktop"},
            {"region": [0, 0, 10]},
            {"region": [0, 0, 10.5, 10]},
            {"region": [-1, 0, 10, 10]},
            {"max_dimension": 100},
            {"jpeg_quality": 99},
            {"wait_for_render_ms": 20000},
            {"format": "webp"},
            {"layout_name": ""},
            {"window": "main"},
        ):
            with self.subTest(arguments=arguments):
                with self.assertRaisesRegex(ToolError, "Invalid tool arguments"):
                    _validate_tool_arguments(arguments, schema)

    def test_results_without_image_pass_through_with_unchanged_arguments(self):
        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool}

        tool = {tool.name: tool for tool in self._tools(FakeBridgeClient)}[
            "capture_qgis_screenshot"
        ]
        self.assertEqual(tool.handler({}), {"tool": OPERATION})
        arguments = {"target": "main_window", "region": [1, 2, 3, 4]}
        self.assertEqual(tool.handler(arguments), {"tool": OPERATION})
        self.assertEqual(
            calls,
            [(OPERATION, {}), (OPERATION, {"target": "main_window", "region": [1, 2, 3, 4]})],
        )

    def test_image_result_has_plain_text_and_image_content(self):
        from qcopilots_common.screenshot import capture_screenshot_via_bridge

        bridge_result = _sample_bridge_result()
        result = capture_screenshot_via_bridge(
            lambda operation, arguments: dict(bridge_result),
            {},
        )

        self.assertEqual([item["type"] for item in result["content"]], ["text", "image"])
        image = result["content"][1]
        self.assertEqual(image["mimeType"], "image/png")
        self.assertEqual(image["data"], TINY_PNG_BASE64)
        self.assertTrue(base64.b64decode(image["data"], validate=True).startswith(PNG_MAGIC))

        structured = result["structuredContent"]
        self.assertNotIn("image_base64", structured)
        self.assertNotIn(TINY_PNG_BASE64, repr(structured))
        self.assertEqual(structured["canvas"]["crs"], "EPSG:3035")
        self.assertEqual(structured["warnings"], [])

        text = result["content"][0]["text"]
        self.assertNotIn(TINY_PNG_BASE64, text)
        _assert_plain_text(self, text)
        for expected in (
            "map canvas",
            "Image 800 x 600 pixels, PNG",
            "Scale 1 image pixels per logical pixel",
            "Map canvas CRS EPSG:3035",
            "xmin 4031000.5, ymin 2684000.25, xmax 4672000, ymax 3551000.75",
            "scale 1:250000",
            "rotation 0 degrees",
            "QCopilots chat panel is not visible",
            "Capture method widget",
            "Warnings: none.",
            '"Layer Properties" (QgsVectorLayerProperties, modal)',
            '"No. Heading" (QgisApp, active)',
            "Call capture_qgis_screenshot again after changes to see a fresh view.",
        ):
            self.assertIn(expected, text)

    def test_summary_mentions_region_masking_and_layout(self):
        from qcopilots_common.screenshot import screenshot_tool_result

        result = screenshot_tool_result(
            _sample_bridge_result(
                target="layout_designer",
                layout_name="Atlas > 1",
                region=[10, 20, 300, 200],
                qcopilots_panel="masked",
                format="jpeg",
                mime_type="image/jpeg",
                warnings=["downscaled_to_fit_size_budget"],
                canvas=None,
            ),
            ["canvas_still_rendering"],
        )
        text = result["content"][0]["text"]
        _assert_plain_text(self, text)
        self.assertEqual(result["content"][1]["mimeType"], "image/jpeg")
        self.assertIn("layout designer", text)
        self.assertIn('Print layout "Atlas ) 1"', text)
        self.assertIn("Region x 10, y 20, width 300, height 200", text)
        self.assertIn("covered by a grey block", text)
        self.assertIn("downscaled_to_fit_size_budget, canvas_still_rendering", text)
        self.assertNotIn("Map canvas CRS", text)
        self.assertEqual(
            result["structuredContent"]["warnings"],
            ["downscaled_to_fit_size_budget", "canvas_still_rendering"],
        )

    def test_pending_render_is_polled_until_the_canvas_finishes(self):
        from qcopilots_common.screenshot import capture_screenshot_via_bridge

        clock = _FakeClock()
        calls = []
        responses = [
            {"pending_render": True, "target": "map_canvas"},
            {"pending_render": True, "target": "map_canvas"},
            _sample_bridge_result(),
        ]

        def call(operation, arguments):
            calls.append((operation, dict(arguments)))
            return responses.pop(0)

        arguments = {"target": "map_canvas", "wait_for_render_ms": 3000}
        result = capture_screenshot_via_bridge(call, arguments, clock=clock, sleep=clock.sleep)

        self.assertEqual(calls, [(OPERATION, arguments)] * 3)
        self.assertEqual(clock.sleeps, [0.2, 0.2])
        self.assertEqual(result["structuredContent"]["warnings"], [])

    def test_pending_render_timeout_forces_a_final_capture_with_warning(self):
        from qcopilots_common.screenshot import capture_screenshot_via_bridge

        clock = _FakeClock()
        calls = []

        def call(operation, arguments):
            calls.append((operation, dict(arguments)))
            if arguments.get("wait_for_render_ms", 3000) > 0:
                return {"pending_render": True, "target": "map_canvas"}
            return _sample_bridge_result(warnings=["canvas_still_rendering"])

        arguments = {"wait_for_render_ms": 500}
        result = capture_screenshot_via_bridge(call, arguments, clock=clock, sleep=clock.sleep)

        self.assertEqual(calls[0], (OPERATION, {"wait_for_render_ms": 500}))
        self.assertEqual(calls[-1], (OPERATION, {"wait_for_render_ms": 0}))
        self.assertTrue(all(call == calls[0] for call in calls[:-1]))
        self.assertAlmostEqual(sum(clock.sleeps), 0.5)
        self.assertEqual(result["structuredContent"]["warnings"], ["canvas_still_rendering"])
        self.assertIn("canvas_still_rendering", result["content"][0]["text"])

        default_clock = _FakeClock()
        default_calls = []

        def default_call(operation, arguments):
            default_calls.append(dict(arguments))
            if arguments.get("wait_for_render_ms", 3000) > 0:
                return {"pending_render": True}
            return _sample_bridge_result()

        capture_screenshot_via_bridge(default_call, {}, clock=default_clock, sleep=default_clock.sleep)
        self.assertEqual(default_calls[0], {})
        self.assertEqual(default_calls[-1], {"wait_for_render_ms": 0})
        self.assertAlmostEqual(sum(default_clock.sleeps), 3.0)

    def test_still_pending_after_final_capture_is_a_tool_error(self):
        from qcopilots_common.mcp_http import ToolError
        from qcopilots_common.screenshot import capture_screenshot_via_bridge

        clock = _FakeClock()
        with self.assertRaisesRegex(ToolError, "still rendering"):
            capture_screenshot_via_bridge(
                lambda operation, arguments: {"pending_render": True},
                {"wait_for_render_ms": 0},
                clock=clock,
                sleep=clock.sleep,
            )

    def test_mcp_server_returns_the_image_content_unchanged(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return _sample_bridge_result()

        tools = self._tools(FakeBridgeClient)
        server = McpJsonRpcServer(
            server_name="qcopilots-screenshot-test",
            server_version="1.0.0",
            tools=tools,
        )
        listed = server.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}}
        )
        descriptors = {tool["name"]: tool for tool in listed["result"]["tools"]}
        self.assertIn("capture_qgis_screenshot", descriptors)

        response = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "tools/call",
                "params": {
                    "name": "capture_qgis_screenshot",
                    "arguments": {"target": "map_canvas", "max_dimension": 512},
                },
            }
        )
        result = response["result"]
        self.assertNotIn("isError", result)
        self.assertEqual([item["type"] for item in result["content"]], ["text", "image"])
        self.assertEqual(result["content"][1]["data"], TINY_PNG_BASE64)
        self.assertNotIn("image_base64", result["structuredContent"])
        _assert_plain_text(self, result["content"][0]["text"])
        self.assertEqual(calls, [(OPERATION, {"target": "map_canvas", "max_dimension": 512})])

        invalid = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {
                    "name": "capture_qgis_screenshot",
                    "arguments": {"region": [0, 0, 1]},
                },
            }
        )
        self.assertIn("error", invalid)
        self.assertEqual(len(calls), 1)

    def test_bridge_argument_validation(self):
        from qcopilots_common.screenshot import normalize_screenshot_arguments

        self.assertEqual(
            normalize_screenshot_arguments({}),
            {
                "target": "map_canvas",
                "layout_name": None,
                "region": None,
                "include_qcopilots_panel": True,
                "capture_method": "widget",
                "max_dimension": 1568,
                "format": "auto",
                "jpeg_quality": 85,
                "wait_for_render_ms": 3000,
            },
        )
        for arguments, message in (
            ({"target": "layout_designer"}, "layout_name is required"),
            ({"target": "desktop"}, "target must be one of"),
            ({"layout_name": " "}, "layout_name must be a non-empty string"),
            ({"region": [0, 0, 0, 10]}, "width and height must be 1 or more"),
            ({"region": [0, 0, True, 10]}, "4 integers"),
            ({"max_dimension": True}, "max_dimension must be an integer"),
            ({"jpeg_quality": 30}, "jpeg_quality must be an integer from 40 to 95"),
            ({"capture_method": "window"}, "capture_method must be one of"),
            ({"include_qcopilots_panel": "no"}, "include_qcopilots_panel must be a boolean"),
            ({"window": "x"}, "Unknown screenshot arguments: window"),
        ):
            with self.subTest(arguments=arguments):
                with self.assertRaisesRegex(ValueError, message):
                    normalize_screenshot_arguments(arguments)


@unittest.skipUnless(_qgis_bindings_available(), "QGIS bindings are not available")
class TestQCopilotsScreenshotCapture(unittest.TestCase):
    """Real offscreen Qt captures through the QGIS bridge method."""

    @classmethod
    def setUpClass(cls):
        from qgis.testing import start_app

        start_app()

    def _iface(self, canvas=None, main_window=None, designers=()):
        class FakeIface:
            def mapCanvas(self):
                return canvas

            def mainWindow(self):
                return main_window

            def openLayoutDesigners(self):
                return list(designers)

        return FakeIface()

    def _dispose(self, widget):
        def cleanup():
            widget.hide()
            widget.deleteLater()

        self.addCleanup(cleanup)
        return widget

    def _canvas(self, width=400, height=200, canvas_class=None):
        from qgis.core import QgsCoordinateReferenceSystem, QgsRectangle
        from qgis.gui import QgsMapCanvas
        from qgis.PyQt.QtCore import QCoreApplication
        from qgis.PyQt.QtGui import QColor

        canvas = self._dispose((canvas_class or QgsMapCanvas)())
        canvas.resize(width, height)
        canvas.setCanvasColor(QColor(10, 200, 30))
        canvas.setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3035"))
        canvas.setExtent(QgsRectangle(4000000, 2700000, 4400000, 2900000))
        QCoreApplication.processEvents()
        canvas.waitWhileRendering()
        return canvas

    def _capture(self, iface, arguments):
        from qcopilots_common.bridge import QgisBridgeTools

        return QgisBridgeTools(iface).dispatch(OPERATION, arguments)

    def _decode(self, result):
        from qgis.PyQt.QtGui import QImage

        self.assertNotIn("\n", result["image_base64"])
        data = base64.b64decode(result["image_base64"], validate=True)
        self.assertEqual(len(data), result["bytes"])
        if result["mime_type"] == "image/png":
            self.assertTrue(data.startswith(PNG_MAGIC))
        else:
            self.assertEqual(result["mime_type"], "image/jpeg")
            self.assertTrue(data.startswith(JPEG_MAGIC))
        image = QImage.fromData(data)
        self.assertFalse(image.isNull())
        self.assertEqual([image.width(), image.height()], result["image_size"])
        return image

    def _rgb(self, image, x, y):
        from qgis.PyQt.QtGui import QColor

        color = QColor(image.pixel(x, y))
        return (color.red(), color.green(), color.blue())

    def _rgb_at(self, image, result, x, y):
        """Sample a logical point. Display scaling puts it at a larger image pixel."""

        ratio = result["device_pixel_ratio"]
        return self._rgb(image, int(round(x * ratio)), int(round(y * ratio)))

    def _device_span(self, result, start, length):
        """Image pixels covered by a logical span, rounded at both edges."""

        ratio = result["device_pixel_ratio"]
        return int(round((start + length) * ratio)) - int(round(start * ratio))

    def _image_widget(self, image, logical_width, logical_height, title="Synthetic"):
        """A widget whose grab returns a prepared image, for DPR and size tests."""

        from qgis.PyQt.QtGui import QPixmap
        from qgis.PyQt.QtWidgets import QWidget

        class ImageWidget(QWidget):
            def grab(self, *args):
                return QPixmap.fromImage(image)

        widget = self._dispose(ImageWidget())
        widget.resize(logical_width, logical_height)
        widget.setWindowTitle(title)
        return widget

    def _noise_image(self, width, height, seed=7):
        from qgis.PyQt.QtGui import QImage

        data = random.Random(seed).randbytes(width * height * 4)
        image = QImage(data, width, height, width * 4, QImage.Format.Format_RGB32)
        return image.copy()

    def test_map_canvas_capture_keeps_aspect_and_canvas_colour(self):
        canvas = self._canvas()
        result = self._capture(self._iface(canvas=canvas), {})

        image = self._decode(result)
        self.assertEqual(result["target"], "map_canvas")
        self.assertEqual(result["capture_method"], "widget")
        self.assertEqual(result["format"], "png")
        self.assertEqual(result["logical_size"], [400, 200])
        # ctest runs on the real display, where scaling such as 125% applies.
        ratio = result["device_pixel_ratio"]
        self.assertGreaterEqual(ratio, 1.0)
        self.assertEqual(result["image_size"], [int(round(400 * ratio)), int(round(200 * ratio))])
        self.assertEqual(result["scale"], ratio)
        self.assertEqual(result["qcopilots_panel"], "not_visible")
        self.assertIsNone(result["region"])
        self.assertEqual(result["warnings"], [])
        self.assertIsInstance(result["visible_windows"], list)
        self.assertAlmostEqual(image.width() / image.height(), 2.0)
        self.assertEqual(self._rgb_at(image, result, 200, 100), (10, 200, 30))
        self.assertEqual(result["canvas"]["crs"], "EPSG:3035")
        self.assertEqual(len(result["canvas"]["extent"]), 4)
        xmin, ymin, xmax, ymax = result["canvas"]["extent"]
        self.assertLess(xmin, xmax)
        self.assertLess(ymin, ymax)
        self.assertGreater(result["canvas"]["scale"], 0)
        self.assertEqual(result["canvas"]["rotation"], 0)
        self.assertFalse(result["canvas"]["is_drawing"])

    def test_main_window_includes_the_panel_by_default_and_masks_it_on_request(self):
        from qgis.PyQt.QtCore import QCoreApplication, QPoint, Qt
        from qgis.PyQt.QtGui import QColor, QPalette
        from qgis.PyQt.QtWidgets import QDockWidget, QMainWindow, QWidget
        from qcopilots_common.screenshot import QCOPILOTS_MASK_COLOR

        def filled(color):
            widget = QWidget()
            widget.setAutoFillBackground(True)
            palette = widget.palette()
            palette.setColor(QPalette.ColorRole.Window, QColor(color))
            widget.setPalette(palette)
            return widget

        window = self._dispose(QMainWindow())
        window.setWindowTitle("QCopilots screenshot test")
        window.setCentralWidget(filled("#ff0000"))
        dock = QDockWidget("QCopilots", window)
        dock.setObjectName("QCopilotsDock")
        panel = filled("#0000ff")
        dock.setWidget(panel)
        window.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, dock)
        window.resize(640, 400)
        window.show()
        QCoreApplication.processEvents()

        iface = self._iface(main_window=window)
        panel_center = panel.mapTo(window, QPoint(panel.width() // 2, panel.height() // 2))
        dock_corner = dock.mapTo(window, QPoint(3, 3))
        central = window.centralWidget()
        central_center = central.mapTo(window, QPoint(central.width() // 2, central.height() // 2))

        included = self._capture(iface, {"target": "main_window"})
        image = self._decode(included)
        self.assertEqual(included["qcopilots_panel"], "included")
        self.assertEqual(included["window_title"], "QCopilots screenshot test")
        self.assertEqual(included["logical_size"], [window.width(), window.height()])
        self.assertNotIn("canvas", included)
        self.assertEqual(
            self._rgb_at(image, included, panel_center.x(), panel_center.y()), (0, 0, 255)
        )
        self.assertEqual(
            self._rgb_at(image, included, central_center.x(), central_center.y()), (255, 0, 0)
        )
        listed = [
            window
            for window in included["visible_windows"]
            if window["title"] == "QCopilots screenshot test"
        ]
        self.assertEqual(len(listed), 1)
        self.assertEqual(listed[0]["class"], "QMainWindow")
        self.assertFalse(listed[0]["modal"])
        self.assertIsInstance(listed[0]["active"], bool)

        masked = self._capture(
            iface,
            {"target": "main_window", "include_qcopilots_panel": False},
        )
        image = self._decode(masked)
        mask = QColor(QCOPILOTS_MASK_COLOR)
        self.assertEqual(masked["qcopilots_panel"], "masked")
        self.assertEqual(
            self._rgb_at(image, masked, dock_corner.x(), dock_corner.y()),
            (mask.red(), mask.green(), mask.blue()),
        )
        self.assertNotEqual(
            self._rgb_at(image, masked, panel_center.x(), panel_center.y()), (0, 0, 255)
        )
        self.assertEqual(
            self._rgb_at(image, masked, central_center.x(), central_center.y()), (255, 0, 0)
        )

        # A region that misses the docked panel reports it as not visible.
        outside = self._capture(
            iface,
            {
                "target": "main_window",
                "include_qcopilots_panel": False,
                "region": [central_center.x() - 5, central_center.y() - 5, 10, 10],
            },
        )
        self.assertEqual(outside["qcopilots_panel"], "not_visible")
        self.assertEqual(
            outside["image_size"],
            [
                self._device_span(outside, central_center.x() - 5, 10),
                self._device_span(outside, central_center.y() - 5, 10),
            ],
        )

        dock.hide()
        hidden = self._capture(iface, {"target": "main_window"})
        self.assertEqual(hidden["qcopilots_panel"], "not_visible")

    def test_region_crop_and_out_of_bounds_error(self):
        canvas = self._canvas()
        iface = self._iface(canvas=canvas)

        result = self._capture(iface, {"region": [50, 20, 100, 60]})
        image = self._decode(result)
        self.assertEqual(result["region"], [50, 20, 100, 60])
        self.assertEqual(
            result["image_size"],
            [self._device_span(result, 50, 100), self._device_span(result, 20, 60)],
        )
        self.assertEqual(result["logical_size"], [400, 200])
        self.assertAlmostEqual(result["scale"], result["device_pixel_ratio"], delta=0.02)
        self.assertEqual(self._rgb_at(image, result, 50, 30), (10, 200, 30))

        for region in ([350, 0, 100, 10], [0, 150, 10, 51]):
            with self.subTest(region=region):
                with self.assertRaisesRegex(ValueError, "outside the captured target, which is 400 x 200"):
                    self._capture(iface, {"region": region})

    def test_max_dimension_downscale_and_device_pixel_ratio(self):
        from qgis.PyQt.QtGui import QColor, QImage, QPainter

        canvas = self._canvas()
        result = self._capture(self._iface(canvas=canvas), {"max_dimension": 256})
        image = self._decode(result)
        self.assertEqual(result["image_size"], [256, 128])
        self.assertEqual(result["scale"], 0.64)
        self.assertEqual(self._rgb(image, 128, 64), (10, 200, 30))

        # 800 x 400 device pixels at a device pixel ratio of 2, left half red
        # and right half blue, behind a 400 x 200 logical widget.
        source = QImage(800, 400, QImage.Format.Format_RGB32)
        source.fill(QColor("#ff0000"))
        painter = QPainter(source)
        painter.fillRect(400, 0, 400, 400, QColor("#0000ff"))
        painter.end()
        source.setDevicePixelRatio(2.0)
        widget = self._image_widget(source, 400, 200)
        iface = self._iface(main_window=widget)

        full = self._capture(iface, {"target": "main_window"})
        image = self._decode(full)
        self.assertEqual(full["device_pixel_ratio"], 2.0)
        self.assertEqual(full["logical_size"], [400, 200])
        self.assertEqual(full["image_size"], [800, 400])
        self.assertEqual(full["scale"], 2.0)

        cropped = self._capture(
            iface,
            {"target": "main_window", "region": [200, 0, 200, 200], "max_dimension": 256},
        )
        image = self._decode(cropped)
        self.assertEqual(cropped["image_size"], [256, 256])
        self.assertEqual(cropped["scale"], 1.28)
        self.assertEqual(self._rgb(image, 5, 5), (0, 0, 255))
        self.assertEqual(self._rgb(image, 250, 250), (0, 0, 255))

    def test_png_jpeg_and_auto_size_budget(self):
        from qgis.PyQt.QtCore import Qt
        from qgis.PyQt.QtGui import QImage
        from qcopilots_common.screenshot import encode_image

        canvas = self._canvas()
        iface = self._iface(canvas=canvas)
        png = self._capture(iface, {"format": "png"})
        self._decode(png)
        self.assertEqual((png["format"], png["mime_type"]), ("png", "image/png"))
        jpeg = self._capture(iface, {"format": "jpeg", "jpeg_quality": 60})
        image = self._decode(jpeg)
        self.assertEqual((jpeg["format"], jpeg["mime_type"]), ("jpeg", "image/jpeg"))
        red, green, blue = self._rgb(image, 200, 100)
        self.assertLess(abs(red - 10) + abs(green - 200) + abs(blue - 30), 30)

        # Noise does not compress, so the PNG exceeds 1 MiB and auto uses JPEG.
        noise = self._noise_image(1200, 900)
        noisy = self._capture(
            self._iface(main_window=self._image_widget(noise, 1200, 900)),
            {"target": "main_window", "max_dimension": 2560},
        )
        self._decode(noisy)
        self.assertEqual(noisy["format"], "jpeg")
        self.assertEqual(noisy["image_size"], [1200, 900])
        self.assertLessEqual(noisy["bytes"], 3 * 1024 * 1024)

        warnings = []
        data, image_format, final = encode_image(
            self._noise_image(400, 300),
            "auto",
            85,
            warnings,
            png_budget=100,
            max_bytes=6000,
        )
        self.assertEqual(image_format, "jpeg")
        self.assertLessEqual(len(data), 6000)
        self.assertTrue(data.startswith(JPEG_MAGIC))
        self.assertLess(final.width(), 400)
        self.assertEqual(
            warnings,
            ["jpeg_quality_lowered_to_70", "downscaled_to_fit_size_budget"],
        )

        warnings = []
        data, image_format, final = encode_image(
            self._noise_image(400, 300), "png", 85, warnings, max_bytes=120000
        )
        self.assertEqual(image_format, "png")
        self.assertTrue(data.startswith(PNG_MAGIC))
        self.assertLessEqual(len(data), 120000)
        self.assertEqual(warnings, ["downscaled_to_fit_size_budget"])

        with self.assertRaisesRegex(RuntimeError, "could not be encoded within 10 bytes"):
            encode_image(self._noise_image(400, 300), "jpeg", 85, [], max_bytes=10)

        # JPEG flattens transparency onto white.
        transparent = QImage(64, 64, QImage.Format.Format_ARGB32)
        transparent.fill(Qt.GlobalColor.transparent)
        data, image_format, _final = encode_image(transparent, "jpeg", 90, [])
        decoded = QImage.fromData(data)
        self.assertEqual(image_format, "jpeg")
        self.assertTrue(all(channel >= 250 for channel in self._rgb(decoded, 32, 32)))

    def test_layout_designer_requires_an_open_designer(self):
        from qgis.PyQt.QtWidgets import QMainWindow

        class FakeLayout:
            def __init__(self, name):
                self._name = name

            def name(self):
                return self._name

        class FakeDesigner:
            def __init__(self, name, window):
                self._layout = FakeLayout(name)
                self._window = window

            def masterLayout(self):
                return self._layout

            def window(self):
                return self._window

        designer_window = self._dispose(QMainWindow())
        designer_window.setWindowTitle("Atlas")
        designer_window.resize(320, 240)
        iface = self._iface(designers=[FakeDesigner("Atlas", designer_window)])

        with self.assertRaisesRegex(ValueError, "layout_name is required"):
            self._capture(iface, {"target": "layout_designer"})
        with self.assertRaisesRegex(RuntimeError, 'No open layout designer shows the layout "Missing"') as caught:
            self._capture(iface, {"target": "layout_designer", "layout_name": "Missing"})
        self.assertIn('Open layout designers: "Atlas"', str(caught.exception))
        self.assertIn("list_print_layouts", str(caught.exception))

        result = self._capture(iface, {"target": "layout_designer", "layout_name": "Atlas"})
        self._decode(result)
        self.assertEqual(result["layout_name"], "Atlas")
        self.assertEqual(result["window_title"], "Atlas")
        self.assertEqual(result["logical_size"], [320, 240])
        self.assertNotIn("canvas", result)

    def test_active_window_prefers_dialogs_and_falls_back_to_the_main_window(self):
        from qgis.PyQt.QtWidgets import QDialog, QDockWidget, QMainWindow
        from qcopilots_common.screenshot import capture_qgis_screenshot

        main_window = self._dispose(QMainWindow())
        main_window.setWindowTitle("QGIS main")
        main_window.resize(300, 200)
        dialog = self._dispose(QDialog(main_window))
        dialog.setWindowTitle("Layer Properties")
        dialog.resize(200, 120)
        dock = QDockWidget("QCopilots", main_window)
        dock.setObjectName("QCopilotsDock")
        dock.setFloating(True)
        iface = self._iface(main_window=main_window)

        class FakeApplication:
            modal = None
            popup = None
            active = None

            @classmethod
            def activeModalWidget(cls):
                return cls.modal

            @classmethod
            def activePopupWidget(cls):
                return cls.popup

            @classmethod
            def activeWindow(cls):
                return cls.active

            @classmethod
            def topLevelWidgets(cls):
                return []

        def capture():
            return capture_qgis_screenshot(
                iface,
                {"target": "active_window"},
                application=FakeApplication,
            )

        FakeApplication.modal = dialog
        FakeApplication.active = main_window
        result = capture()
        self.assertEqual(result["window_title"], "Layer Properties")
        self.assertEqual(result["logical_size"], [200, 120])
        self.assertEqual(result["warnings"], [])

        FakeApplication.modal = None
        FakeApplication.active = dock
        result = capture()
        self.assertEqual(result["window_title"], "QGIS main")
        self.assertEqual(
            result["warnings"],
            ["active_window_is_qcopilots_panel_used_main_window"],
        )

        FakeApplication.active = None
        result = capture()
        self.assertEqual(result["window_title"], "QGIS main")
        self.assertEqual(result["warnings"], ["no_active_window_used_main_window"])

    def test_pending_render_returns_at_once_and_the_wrapper_polls(self):
        from qgis.gui import QgsMapCanvas
        from qgis.PyQt.QtWidgets import QMainWindow
        from qcopilots_common.bridge import QgisBridgeTools
        from qcopilots_common.screenshot import capture_screenshot_via_bridge

        class DrawingCanvas(QgsMapCanvas):
            drawing = True

            def isDrawing(self):
                return self.drawing

        canvas = self._canvas(canvas_class=DrawingCanvas)
        iface = self._iface(canvas=canvas)

        pending = self._capture(iface, {})
        self.assertTrue(pending["pending_render"])
        self.assertNotIn("image_base64", pending)
        self.assertTrue(pending["canvas"]["is_drawing"])

        forced = self._capture(iface, {"wait_for_render_ms": 0})
        self._decode(forced)
        self.assertEqual(forced["warnings"], ["canvas_still_rendering"])
        self.assertTrue(forced["canvas"]["is_drawing"])

        window = self._dispose(QMainWindow())
        canvas.setParent(None)
        window.setCentralWidget(canvas)
        window.resize(500, 300)
        iface = self._iface(canvas=canvas, main_window=window)
        self.assertTrue(self._capture(iface, {"target": "main_window"})["pending_render"])

        tools = QgisBridgeTools(iface)
        clock = _FakeClock()
        calls = []

        def call(operation, arguments):
            calls.append(dict(arguments))
            if len(calls) == 3:
                canvas.drawing = False
            return tools.dispatch(operation, arguments)

        result = capture_screenshot_via_bridge(
            call,
            {"target": "main_window"},
            clock=clock,
            sleep=clock.sleep,
        )
        self.assertEqual(calls, [{"target": "main_window"}] * 3)
        self.assertEqual(result["structuredContent"]["warnings"], [])
        self.assertFalse(result["structuredContent"]["canvas"]["is_drawing"])
        self.assertEqual(result["content"][1]["type"], "image")
        _assert_plain_text(self, result["content"][0]["text"])

    def test_screen_capture_method_reports_the_privacy_warning(self):
        from qgis.PyQt.QtCore import QCoreApplication

        canvas = self._canvas(240, 160)
        canvas.move(10, 10)
        canvas.show()
        QCoreApplication.processEvents()
        canvas.waitWhileRendering()

        result = self._capture(
            self._iface(canvas=canvas),
            {"capture_method": "screen", "wait_for_render_ms": 0},
        )
        self._decode(result)
        self.assertEqual(result["capture_method"], "screen")
        self.assertIn("screen_capture_may_show_other_applications", result["warnings"])
        self.assertEqual(result["logical_size"], [240, 160])

        canvas.hide()
        with self.assertRaisesRegex(RuntimeError, "not visible on screen"):
            self._capture(self._iface(canvas=canvas), {"capture_method": "screen"})

    def test_bridge_requires_iface_and_the_gui_thread(self):
        from qcopilots_common.bridge import QgisBridgeTools
        from qcopilots_common.screenshot import assert_gui_thread

        with self.assertRaisesRegex(RuntimeError, "QGIS interface is not available"):
            QgisBridgeTools(None).dispatch(OPERATION, {})

        errors = []

        def worker():
            try:
                assert_gui_thread()
            except RuntimeError as err:
                errors.append(str(err))

        thread = threading.Thread(target=worker)
        thread.start()
        thread.join(timeout=10)
        self.assertEqual(errors, ["QGIS screenshots must be taken on the QGIS GUI thread"])


if __name__ == "__main__":
    unittest.main()
