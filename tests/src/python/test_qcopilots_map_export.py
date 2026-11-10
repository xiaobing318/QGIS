"""Map image export publication and asynchronous lifecycle regressions.

The runtime doubles deliberately finish independently of the HTTP caller. This
exercises deadline and staging ownership without requiring QGIS or Qt imports.
"""

import gc
import math
import tempfile
import threading
import time
import unittest
import weakref
from dataclasses import replace
from pathlib import Path
from unittest import mock


def _wait_until(predicate, timeout=1):
    deadline = time.monotonic() + timeout
    while not predicate():
        if time.monotonic() >= deadline:
            return False
        threading.Event().wait(0.005)
    return True


class _Dispatcher:
    def __init__(self):
        self.timeouts = []

    def call(self, function, timeout_seconds=60):
        self.timeouts.append(timeout_seconds)
        return function()


class _ExportHarness:
    def __init__(
        self,
        *,
        timeout=1,
        auto_complete=True,
        prepare=None,
        dispatcher=None,
        clock=time.monotonic,
    ):
        from qcopilots_common.bridge import QgisBridgeTools
        from qcopilots_common.map_export import MapImageExportCoordinator

        self.tools = QgisBridgeTools(None)
        self.prepare = prepare or self.tools.export_map_image
        self.dispatcher = dispatcher or _Dispatcher()
        self.started = threading.Event()
        self.cancelled = threading.Event()
        self.operations = []
        self.auto_complete = auto_complete
        self.after_finish = None
        self.image = b"image-from-current-settings"
        self.world = b"2\n0\n0\n-2\n101\n199\n"
        harness = self

        class Runtime:
            def __init__(self, coordinator):
                self.coordinator = coordinator

            def begin(self, operation, arguments):
                plan = harness.prepare(arguments)
                if isinstance(plan, dict):
                    self.coordinator.preview_finished(operation, plan)
                    return
                operation.plan = plan
                plan.path.parent.mkdir(parents=True, exist_ok=True)
                operation.staging_root = Path(
                    tempfile.mkdtemp(
                        prefix=f".{plan.path.stem}.qcopilots-stage-",
                        dir=str(plan.path.parent),
                    )
                )
                operation.world_file_bytes = harness.world
                (operation.staging_root / plan.path.name).write_bytes(harness.image)
                harness.operations.append(operation)
                harness.started.set()
                if harness.auto_complete:
                    self.coordinator.render_finished(operation)
                    if harness.after_finish is not None:
                        harness.after_finish()

            def cancel(self, operation):
                # The writer has not stopped just because cancellation was asked.
                harness.cancelled.set()

        self.coordinator = MapImageExportCoordinator(
            self.dispatcher,
            self.prepare,
            timeout_seconds=timeout,
            runtime_factory=Runtime,
            clock=clock,
        )

    def start(self, arguments):
        outcome = {}

        def export():
            try:
                outcome["result"] = self.coordinator.export(arguments)
            except Exception as error:
                outcome["error"] = error

        thread = threading.Thread(target=export, daemon=True)
        thread.start()
        return thread, outcome

    def finish(self, error=None):
        self.coordinator.render_finished(self.operations[-1], error=error)


class TestQCopilotsMapExport(unittest.TestCase):
    def test_all_formats_publish_image_and_snapshot_world_file(self):
        formats = (
            ("png", "png", "PNG", "pgw"),
            ("jpg", "jpeg", "JPEG", "jgw"),
            ("jpeg", "jpeg", "JPEG", "jgw"),
            ("bmp", "bmp", "BMP", "bpw"),
            ("tif", "tiff", "TIFF", "tfw"),
            ("tiff", "tiff", "TIFF", "tfw"),
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for suffix, image_format, encoder, world_suffix in formats:
                with self.subTest(suffix=suffix):
                    harness = _ExportHarness()
                    target = root / f"map-{suffix}.{suffix}"
                    result = harness.coordinator.export(
                        {
                            "path": str(target),
                            "format": image_format,
                        }
                    )
                    world = target.with_suffix(f".{world_suffix}")
                    self.assertEqual(target.read_bytes(), harness.image)
                    self.assertEqual(world.read_bytes(), harness.world)
                    self.assertEqual(result["encoder"], encoder)
                    self.assertEqual(result["world_file_path"], str(world))
                    self.assertEqual(result["bytes_written"], len(harness.image))
                    self.assertTrue(result["saved"])
                    self.assertTrue(result["cleanup"]["complete"])
                    self.assertFalse(harness.operations[0].staging_root.exists())

    def test_overwrite_preview_does_not_start_renderer_or_create_stage(self):
        harness = _ExportHarness()
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            target.write_bytes(b"keep")
            result = harness.coordinator.export(
                {
                    "path": str(target),
                    "format": "png",
                    "overwrite": True,
                }
            )
            self.assertIn("overwrite_confirmation_token", result)
            self.assertEqual(harness.operations, [])
            self.assertEqual(target.read_bytes(), b"keep")
            self.assertEqual(list(target.parent.glob(".*.qcopilots-stage-*")), [])

    def test_confirmed_overwrite_is_authorized_once_and_consumes_token(self):
        harness = _ExportHarness()
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            target.write_bytes(b"old")
            arguments = {"path": str(target), "format": "png", "overwrite": True}
            preview = harness.coordinator.export(arguments)
            arguments["overwrite_confirmation_token"] = preview[
                "overwrite_confirmation_token"
            ]
            with mock.patch.object(
                harness, "prepare", wraps=harness.prepare
            ) as prepare:
                result = harness.coordinator.export(arguments)
                self.assertEqual(prepare.call_count, 1)
            self.assertTrue(result["saved"])
            with self.assertRaisesRegex(RuntimeError, "already used"):
                harness.coordinator.export(arguments)

    def test_target_changed_while_rendering_is_preserved(self):
        harness = _ExportHarness(auto_complete=False)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            target.write_bytes(b"original")
            arguments = {"path": str(target), "format": "png", "overwrite": True}
            preview = harness.coordinator.export(arguments)
            arguments["overwrite_confirmation_token"] = preview[
                "overwrite_confirmation_token"
            ]
            thread, outcome = harness.start(arguments)
            self.assertTrue(harness.started.wait(1))
            target.write_bytes(b"concurrently-edited")
            harness.finish()
            thread.join(1)
            self.assertFalse(thread.is_alive())
            self.assertIn("changed after overwrite", str(outcome["error"]))
            self.assertEqual(target.read_bytes(), b"concurrently-edited")
            self.assertFalse(target.with_suffix(".pgw").exists())
            self.assertFalse(harness.operations[0].staging_root.exists())

    def test_timeout_retains_active_stage_and_late_success_never_publishes(self):
        harness = _ExportHarness(timeout=0.05, auto_complete=False)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            thread, outcome = harness.start({"path": str(target), "format": "png"})
            self.assertTrue(harness.started.wait(1))
            operation = harness.operations[0]
            thread.join(1)
            self.assertFalse(thread.is_alive())
            self.assertIsInstance(outcome.get("error"), TimeoutError)
            self.assertTrue(harness.cancelled.wait(1))
            self.assertTrue(operation.staging_root.is_dir())
            self.assertFalse(target.exists())
            harness.finish()
            self.assertFalse(target.exists())
            self.assertFalse(target.with_suffix(".pgw").exists())
            self.assertTrue(_wait_until(lambda: not operation.staging_root.exists()))

    def test_single_slot_is_held_until_timed_out_writer_really_finishes(self):
        harness = _ExportHarness(timeout=0.05, auto_complete=False)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first, first_outcome = harness.start(
                {"path": str(root / "one.png"), "format": "png"}
            )
            self.assertTrue(harness.started.wait(1))
            first.join(1)
            self.assertIsInstance(first_outcome.get("error"), TimeoutError)
            with self.assertRaises(TimeoutError):
                harness.coordinator.export(
                    {"path": str(root / "two.png"), "format": "png"}
                )
            self.assertEqual(len(harness.operations), 1)
            harness.finish()
            harness.auto_complete = True
            result = harness.coordinator.export(
                {"path": str(root / "three.png"), "format": "png"}
            )
            self.assertTrue(result["saved"])
            self.assertFalse((root / "one.png").exists())
            self.assertFalse((root / "two.png").exists())

    def test_render_error_releases_slot_and_cleans_only_its_stage(self):
        harness = _ExportHarness(auto_complete=False)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            unrelated = root / ".other.qcopilots-stage-keep"
            unrelated.mkdir()
            (unrelated / "keep").write_bytes(b"keep")
            target = root / "map.png"
            thread, outcome = harness.start({"path": str(target), "format": "png"})
            self.assertTrue(harness.started.wait(1))
            harness.finish(RuntimeError("renderer failed"))
            thread.join(1)
            self.assertIn("renderer failed", str(outcome["error"]))
            self.assertFalse(target.exists())
            self.assertFalse(harness.operations[0].staging_root.exists())
            self.assertEqual((unrelated / "keep").read_bytes(), b"keep")
            harness.auto_complete = True
            self.assertTrue(
                harness.coordinator.export({"path": str(target), "format": "png"})[
                    "saved"
                ]
            )

    def test_shutdown_wakes_waiter_and_keeps_active_stage_until_terminal(self):
        harness = _ExportHarness(timeout=5, auto_complete=False)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            thread, outcome = harness.start({"path": str(target), "format": "png"})
            self.assertTrue(harness.started.wait(1))
            operation = harness.operations[0]
            harness.coordinator.shutdown()
            thread.join(1)
            self.assertFalse(thread.is_alive())
            self.assertIn("stopping", str(outcome.get("error")))
            self.assertTrue(harness.cancelled.wait(1))
            self.assertTrue(operation.staging_root.exists())
            harness.finish()
            self.assertTrue(_wait_until(lambda: not operation.staging_root.exists()))
            self.assertFalse(target.exists())
            with self.assertRaisesRegex(RuntimeError, "stopping"):
                harness.coordinator.export({"path": str(target), "format": "png"})

    def test_empty_image_is_not_published(self):
        harness = _ExportHarness()
        harness.image = b""
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            with self.assertRaisesRegex(RuntimeError, "no output|empty"):
                harness.coordinator.export({"path": str(target), "format": "png"})
            self.assertFalse(target.exists())
            self.assertFalse(target.with_suffix(".pgw").exists())
            self.assertFalse(harness.operations[0].staging_root.exists())

    def test_revalidation_failure_after_render_prevents_publication(self):
        from qcopilots_common.bridge import QgisBridgeTools

        tools = QgisBridgeTools(None)

        def prepare(arguments):
            plan = tools.export_map_image(arguments)
            return replace(
                plan, revalidate=mock.Mock(side_effect=PermissionError("path changed"))
            )

        harness = _ExportHarness(prepare=prepare)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            with self.assertRaisesRegex(PermissionError, "path changed"):
                harness.coordinator.export({"path": str(target), "format": "png"})
            self.assertFalse(target.exists())
            self.assertFalse(harness.operations[0].staging_root.exists())

    def test_dispatch_uses_remaining_budget_instead_of_default_sixty_seconds(self):
        harness = _ExportHarness(timeout=0.25)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            harness.coordinator.export({"path": str(target), "format": "png"})
            self.assertTrue(harness.dispatcher.timeouts)
            for timeout in harness.dispatcher.timeouts:
                self.assertGreater(timeout, 0)
                self.assertLessEqual(timeout, 0.25)

    def test_rejected_gui_dispatch_does_not_poison_next_export(self):
        class RejectOnceDispatcher(_Dispatcher):
            def call(self, function, timeout_seconds=60):
                if not self.timeouts:
                    self.timeouts.append(timeout_seconds)
                    raise RuntimeError("GUI request rejected")
                return super().call(function, timeout_seconds)

        harness = _ExportHarness(dispatcher=RejectOnceDispatcher())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "first.png"
            with self.assertRaisesRegex(RuntimeError, "GUI request rejected"):
                harness.coordinator.export({"path": str(first), "format": "png"})
            self.assertFalse(first.exists())
            self.assertEqual(harness.operations, [])
            result = harness.coordinator.export(
                {
                    "path": str(root / "second.png"),
                    "format": "png",
                }
            )
            self.assertTrue(result["saved"])

    def test_completion_after_deadline_cannot_claim_publication(self):
        now = [10.0]
        harness = _ExportHarness(clock=lambda: now[0])
        harness.after_finish = lambda: now.__setitem__(0, 12.0)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            with self.assertRaises(TimeoutError):
                harness.coordinator.export({"path": str(target), "format": "png"})
            self.assertFalse(target.exists())
            self.assertFalse(target.with_suffix(".pgw").exists())
            self.assertFalse(harness.operations[0].staging_root.exists())

    def test_shutdown_after_render_completion_prevents_publication(self):
        harness = _ExportHarness()
        harness.after_finish = harness.coordinator.shutdown
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            with self.assertRaisesRegex(RuntimeError, "stopping"):
                harness.coordinator.export({"path": str(target), "format": "png"})
            self.assertFalse(target.exists())
            self.assertFalse(target.with_suffix(".pgw").exists())
            self.assertFalse(harness.operations[0].staging_root.exists())

    def test_failed_export_retains_primary_error_and_reports_cleanup_residual(self):
        import qcopilots_common.bridge as bridge

        harness = _ExportHarness(auto_complete=False)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            thread, outcome = harness.start({"path": str(target), "format": "png"})
            self.assertTrue(harness.started.wait(1))
            stage = harness.operations[0].staging_root
            with mock.patch.object(
                bridge, "_cleanup_interactive_paths", return_value=[str(stage)]
            ):
                harness.finish(RuntimeError("original renderer failure"))
                thread.join(1)
            self.assertFalse(thread.is_alive())
            message = str(outcome.get("error"))
            self.assertIn("original renderer failure", message)
            self.assertIn(str(stage), message)
            self.assertTrue(stage.exists())
            self.assertFalse(target.exists())

    def test_late_cleanup_logs_residual_and_keeps_a_bounded_history(self):
        import qcopilots_common.bridge as bridge

        harness = _ExportHarness(timeout=0.05, auto_complete=False)
        harness.coordinator.cleanup_residuals.extend(
            f"old-{index}" for index in range(64)
        )
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "map.png"
            thread, outcome = harness.start({"path": str(target), "format": "png"})
            self.assertTrue(harness.started.wait(1))
            stage = harness.operations[0].staging_root
            thread.join(1)
            self.assertFalse(thread.is_alive())
            self.assertIsInstance(outcome.get("error"), TimeoutError)
            with (
                mock.patch.object(
                    bridge, "_cleanup_interactive_paths", return_value=[str(stage)]
                ),
                self.assertLogs(
                    "qcopilots_common.map_export", level="WARNING"
                ) as messages,
            ):
                harness.finish()
                self.assertTrue(_wait_until(lambda: bool(messages.output)))
            self.assertTrue(any(str(stage) in message for message in messages.output))
            self.assertIn(str(stage), harness.coordinator.cleanup_residuals)
            self.assertLessEqual(len(harness.coordinator.cleanup_residuals), 64)
            self.assertNotIn("old-0", harness.coordinator.cleanup_residuals)
            self.assertTrue(stage.exists())
            self.assertFalse(target.exists())


# Reuse the configuration-aware bootstrap used by the other native suites.
import test_qcopilots_mcp_server_interactive_tools  # noqa: F401


def _qgis_bindings_available():
    try:
        import qgis.core  # NOQA
        import qgis.gui  # NOQA

        return True
    except Exception:
        return False


@unittest.skipUnless(_qgis_bindings_available(), "QGIS bindings are not available")
class TestQCopilotsNativeMapExport(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from qgis.testing import start_app

        cls.app = start_app()

    def _canvas(self):
        from qgis.core import (
            Qgis,
            QgsCoordinateReferenceSystem,
            QgsMapSettings,
            QgsRectangle,
        )
        from qgis.PyQt.QtCore import QSize, QThread
        from qgis.PyQt.QtGui import QColor, QImage

        test = self
        settings = QgsMapSettings()
        settings.setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3857"))
        settings.setOutputSize(QSize(320, 240))
        settings.setOutputDpi(120)
        settings.setDevicePixelRatio(2)
        settings.setExtent(QgsRectangle(100, 200, 900, 800))
        settings.setRotation(37)
        settings.setLayers([])
        settings.setBackgroundColor(QColor("blue"))
        settings.setFlag(Qgis.MapSettingsFlag.UseRenderingOptimization, True)
        settings.setFlag(Qgis.MapSettingsFlag.RenderPartialOutput, True)

        class Canvas:
            def __init__(self):
                self.current = settings
                self.theme_name = ""
                self.temporal_controller = None
                self.old_frame = QImage(640, 480, QImage.Format.Format_RGB32)
                self.old_frame.fill(QColor("red"))
                self.save_as_image_calls = 0
                self.gui_calls = 0

            def assert_gui(self):
                test.assertEqual(QThread.currentThread(), test.app.thread())
                self.gui_calls += 1

            def mapSettings(self):
                self.assert_gui()
                return QgsMapSettings(self.current)

            def layers(self):
                self.assert_gui()
                return self.current.layers()

            def canvasColor(self):
                self.assert_gui()
                return self.current.backgroundColor()

            def rotation(self):
                self.assert_gui()
                return self.current.rotation()

            def theme(self):
                self.assert_gui()
                return self.theme_name

            def temporalController(self):
                self.assert_gui()
                return self.temporal_controller

            def annotationsVisible(self):
                self.assert_gui()
                return True

            def createExpressionContext(self):
                from qgis.core import (
                    QgsExpressionContext,
                    QgsExpressionContextScope,
                    QgsExpressionContextUtils,
                    QgsProject,
                )

                self.assert_gui()
                context = QgsExpressionContext()
                context.appendScope(QgsExpressionContextUtils.globalScope())
                context.appendScope(
                    QgsExpressionContextUtils.projectScope(QgsProject.instance())
                )
                context.appendScope(QgsExpressionContextUtils.atlasScope(None))
                context.appendScope(
                    QgsExpressionContextUtils.mapSettingsScope(self.current)
                )
                if self.temporal_controller is not None:
                    context.appendScope(
                        self.temporal_controller.createExpressionContextScope()
                    )
                context.appendScope(QgsExpressionContextScope("Map Canvas"))
                context.appendScope(QgsExpressionContextScope("Canvas Custom"))
                return context

            def saveAsImage(self, path, pixmap, encoder):
                self.assert_gui()
                self.save_as_image_calls += 1
                self.old_frame.save(path, encoder)

        return Canvas()

    def _assert_world_file(self, content, dpr, rotation):
        # Independent analytic oracle: extent (100, 200, 900, 800), logical
        # 320 x 240 pixels, and a rotation about the fixed center (500, 500).
        # Each physical pixel spans 2.5 / DPR map units before rotation.
        self.assertEqual(content.count(b"\r\n"), 6)
        values = [float(value) for value in content.decode("utf-8").splitlines()]
        self.assertEqual(len(values), 6)
        a, d, b, e, c, f = values
        step = 2.5 / dpr
        cosine = math.cos(math.radians(rotation))
        sine = math.sin(math.radians(rotation))
        for actual, expected in zip(
            (a, d, b, e),
            (step * cosine, step * sine, step * sine, -step * cosine),
        ):
            self.assertAlmostEqual(actual, expected, places=10)
        width, height = int(320 * dpr), int(240 * dpr)
        for column, row in (
            (0, 0),
            (width - 1, 0),
            (0, height - 1),
            (width - 1, height - 1),
        ):
            # Rotate physical-pixel centers, not logical-pixel centers.
            x = 100 + (column + 0.5) * step - 500
            y = 800 - (row + 0.5) * step - 500
            expected_x = 500 + cosine * x - sine * y
            expected_y = 500 + sine * x + cosine * y
            self.assertAlmostEqual(c + a * column + b * row, expected_x, places=9)
            self.assertAlmostEqual(f + d * column + e * row, expected_y, places=9)

    def test_world_file_physical_pixel_centers_and_steps_at_all_pixel_ratios(self):
        from qcopilots_common.map_export import _export_settings, _world_file_content
        from qgis.core import QgsMapSettingsUtils

        for dpr in (1, 2, 1.5):
            for rotation in (0, 37):
                with self.subTest(dpr=dpr, rotation=rotation):
                    canvas = self._canvas()
                    canvas.current.setDevicePixelRatio(dpr)
                    canvas.current.setRotation(rotation)
                    settings = _export_settings(canvas)
                    actual = _world_file_content(settings)
                    native = QgsMapSettingsUtils.worldFileContent(settings).encode(
                        "utf-8"
                    )
                    self._assert_world_file(actual, dpr, rotation)
                    self.assertEqual(actual.splitlines()[:4], native.splitlines()[:4])
                    if dpr == 1:
                        self.assertEqual(actual, native)

    def test_export_settings_use_native_rotation_extent_and_export_flags(self):
        from qcopilots_common.map_export import _export_settings
        from qgis.core import Qgis, QgsMapSettings, QgsMapSettingsUtils
        from qgis.PyQt.QtCore import QSize
        from qgis.PyQt.QtGui import QColor

        canvas = self._canvas()
        unrotated = QgsMapSettings(canvas.current)
        unrotated.setRotation(0)
        result = _export_settings(canvas)
        self.assertEqual(result.extent(), unrotated.visibleExtent())
        self.assertEqual(result.rotation(), 37)
        self.assertEqual(result.outputSize(), QSize(320, 240))
        self.assertEqual(result.outputDpi(), 120)
        self.assertEqual(result.devicePixelRatio(), 2)
        self.assertEqual(result.backgroundColor(), QColor("blue"))
        self.assertEqual(result.destinationCrs(), canvas.current.destinationCrs())
        self.assertEqual(result.rendererUsage(), Qgis.RendererUsage.Export)
        self.assertEqual(
            result.rasterizedRenderingPolicy(),
            Qgis.RasterizedRenderingPolicy.PreferVector,
        )
        self.assertFalse(result.testFlag(Qgis.MapSettingsFlag.UseRenderingOptimization))
        self.assertFalse(result.testFlag(Qgis.MapSettingsFlag.RenderPartialOutput))
        self.assertFalse(result.testFlag(Qgis.MapSettingsFlag.DrawEditingInfo))
        self.assertTrue(result.testFlag(Qgis.MapSettingsFlag.DrawSelection))
        # The six world-file coefficients must all come from this same snapshot.
        world = QgsMapSettingsUtils.worldFileContent(result).encode("utf-8")
        coefficients = [float(value) for value in world.decode("utf-8").splitlines()]
        self.assertEqual(len(coefficients), 6)
        self.assertNotEqual(coefficients[1], 0)
        self.assertNotEqual(coefficients[2], 0)

    def _run_native_export(self, canvas, target, image_format, capture_runtime=None):
        from qcopilots_common.bridge import QgisBridgeTools, _QtMainThreadDispatcher
        from qcopilots_common.map_export import MapImageExportCoordinator
        from qgis.PyQt.QtCore import QEventLoop, QThread, QTimer

        self.assertEqual(QThread.currentThread(), self.app.thread())
        gui_thread = self.app.thread()

        class Iface:
            def mapCanvas(self):
                return canvas

            def activeDecorations(self):
                return []

        iface = Iface()
        tools = QgisBridgeTools(iface)
        dispatcher = _QtMainThreadDispatcher()
        coordinator = MapImageExportCoordinator(
            dispatcher,
            tools.export_map_image,
            iface=iface,
            timeout_seconds=5,
        )
        outcome = {}
        heartbeat = []
        loop = QEventLoop()
        poll = QTimer()
        watchdog = QTimer()
        watchdog.setSingleShot(True)
        watchdog.timeout.connect(loop.quit)

        def export():
            try:
                self.assertNotEqual(QThread.currentThread(), gui_thread)
                outcome["result"] = coordinator.export(
                    {
                        "path": str(target),
                        "format": image_format,
                    }
                )
            except Exception as error:
                outcome["error"] = error

        worker = threading.Thread(target=export, daemon=True)

        def tick():
            if worker.is_alive():
                heartbeat.append(True)
            else:
                loop.quit()

        poll.timeout.connect(tick)
        worker.start()
        poll.start(0)
        watchdog.start(10000)
        try:
            loop.exec()
        finally:
            poll.stop()
            watchdog.stop()
            if capture_runtime is not None:
                capture_runtime.extend(
                    (
                        weakref.ref(coordinator),
                        weakref.ref(iface),
                        weakref.ref(coordinator._runtime),
                    )
                )
            coordinator.shutdown()
            dispatcher.close()
            worker.join(1)
        self.assertFalse(worker.is_alive(), "Native export did not stop")
        if "error" in outcome:
            raise outcome["error"]
        self.assertIn("result", outcome)
        self.assertTrue(heartbeat, "GUI event loop did not run while HTTP waited")
        if hasattr(canvas, "gui_calls"):
            self.assertGreater(canvas.gui_calls, 0)
            self.assertEqual(canvas.save_as_image_calls, 0)
        self.assertTrue(outcome["result"]["saved"])
        self.assertEqual(outcome["result"]["bytes_written"], target.stat().st_size)
        self.assertEqual(list(target.parent.glob(".*.qcopilots-stage-*")), [])
        return outcome["result"]

    def test_native_task_exports_new_blue_settings_and_matching_world_file(self):
        from qgis.PyQt.QtGui import QColor, QImage

        formats = (
            ("png", "png", "pgw"),
            ("jpeg", "jpg", "jgw"),
            ("bmp", "bmp", "bpw"),
            ("tiff", "tif", "tfw"),
        )
        with tempfile.TemporaryDirectory() as directory:
            for image_format, suffix, world_suffix in formats:
                with self.subTest(image_format=image_format):
                    canvas = self._canvas()
                    target = Path(directory) / f"current.{suffix}"
                    self._run_native_export(canvas, target, image_format)
                    self.assertEqual(
                        canvas.old_frame.pixelColor(320, 240), QColor("red")
                    )
                    image = QImage(str(target))
                    self.assertFalse(image.isNull())
                    self.assertEqual((image.width(), image.height()), (640, 480))
                    color = image.pixelColor(320, 240)
                    if image_format == "jpeg":
                        self.assertLessEqual(color.red(), 2)
                        self.assertLessEqual(color.green(), 2)
                        self.assertGreaterEqual(color.blue(), 253)
                    else:
                        self.assertEqual(color, QColor("blue"))
                    self._assert_world_file(
                        target.with_suffix(f".{world_suffix}").read_bytes(), 2, 37
                    )
                    if image_format == "tiff":
                        from osgeo import gdal

                        # Explicitly ignore the world file while checking that
                        # the encoder did not embed its own georeferencing.
                        dataset = gdal.OpenEx(
                            str(target),
                            gdal.OF_RASTER,
                            open_options=["GEOREF_SOURCES=INTERNAL"],
                        )
                        self.assertIsNotNone(dataset)
                        self.assertIsNone(dataset.GetGeoTransform(can_return_null=True))
                        self.assertEqual(dataset.GetProjection(), "")
                        dataset = None

    def test_native_theme_uses_current_style_and_immediate_selection(self):
        from qgis.core import (
            QgsFeature,
            QgsFillSymbol,
            QgsGeometry,
            QgsMapThemeCollection,
            QgsProject,
            QgsSingleSymbolRenderer,
            QgsVectorLayer,
        )
        from qgis.PyQt.QtGui import QColor, QImage

        project = QgsProject.instance()
        previous_dirty = project.isDirty()
        layer = QgsVectorLayer("Polygon?crs=EPSG:3857", "export theme", "memory")
        self.assertTrue(layer.isValid())
        feature = QgsFeature(layer.fields())
        feature.setGeometry(
            QgsGeometry.fromWkt(
                "POLYGON ((-10000 -10000, 10000 -10000, 10000 10000, "
                "-10000 10000, -10000 -10000))"
            )
        )
        self.assertTrue(layer.dataProvider().addFeatures([feature])[0])
        layer.updateExtents()
        layer.setRenderer(
            QgsSingleSymbolRenderer(
                QgsFillSymbol.createSimple(
                    {
                        "color": "255,0,0",
                        "outline_style": "no",
                    }
                )
            )
        )
        project.addMapLayer(layer)
        collection = project.mapThemeCollection()
        theme_name = "QCopilots export current style regression"
        try:
            self.assertTrue(layer.styleManager().addStyleFromLayer("export-style"))
            self.assertTrue(layer.styleManager().setCurrentStyle("export-style"))
            record = QgsMapThemeCollection.MapThemeLayerRecord(layer)
            record.isVisible = True
            record.usingCurrentStyle = True
            record.currentStyle = "export-style"
            theme = QgsMapThemeCollection.MapThemeRecord()
            theme.setLayerRecords([record])
            collection.insert(theme_name, theme)
            canvas = self._canvas()
            canvas.theme_name = theme_name
            canvas.current.setLayers([layer])
            # These XML overrides deliberately describe the old red style.
            canvas.current.setLayerStyleOverrides(
                collection.mapThemeStyleOverrides(theme_name)
            )
            layer.renderer().symbol().setColor(QColor(0, 255, 0))
            with tempfile.TemporaryDirectory() as directory:
                target = Path(directory) / "theme.png"
                self._run_native_export(canvas, target, "png")
                self.assertEqual(
                    QImage(str(target)).pixelColor(320, 240), QColor(0, 255, 0)
                )
                layer.selectByIds([next(layer.getFeatures()).id()])
                canvas.current.setSelectionColor(QColor("magenta"))
                selected = Path(directory) / "selected.png"
                self._run_native_export(canvas, selected, "png")
                self.assertEqual(
                    QImage(str(selected)).pixelColor(320, 240), QColor("magenta")
                )
        finally:
            collection.removeMapTheme(theme_name)
            project.removeMapLayer(layer.id())
            project.setDirty(previous_dirty)

    def test_export_settings_filter_invalid_and_hidden_linked_annotation_layers(self):
        from qcopilots_common.map_export import _export_settings
        from qgis.core import QgsAnnotationLayer, QgsProject, QgsVectorLayer

        project = QgsProject.instance()
        visible = QgsVectorLayer("Point?crs=EPSG:3857", "visible", "memory")
        linked = QgsVectorLayer("Point?crs=EPSG:3857", "linked", "memory")
        invalid = QgsVectorLayer("", "invalid", "ogr")
        annotation = QgsAnnotationLayer(
            "linked annotation",
            QgsAnnotationLayer.LayerOptions(project.transformContext()),
        )
        annotation.setLinkedVisibilityLayer(linked)
        self.assertTrue(visible.isValid())
        self.assertTrue(linked.isValid())
        self.assertTrue(annotation.isValid())
        self.assertFalse(invalid.isValid())
        canvas = self._canvas()
        canvas.current.setLayers([annotation, visible, invalid])
        self.assertEqual(_export_settings(canvas).layers(), [visible])
        canvas.current.setLayers([annotation, linked, visible, invalid])
        self.assertEqual(
            _export_settings(canvas).layers(), [annotation, linked, visible]
        )

    def test_export_settings_snapshot_animation_frame_before_expression_context(self):
        from qcopilots_common.map_export import _export_settings
        from qgis.core import (
            Qgis,
            QgsDateTimeRange,
            QgsInterval,
            QgsTemporalNavigationObject,
        )
        from qgis.PyQt.QtCore import QDateTime, Qt

        controller = QgsTemporalNavigationObject()
        controller.setTemporalExtents(
            QgsDateTimeRange(
                QDateTime.fromString("2020-01-01T00:00:00Z", Qt.DateFormat.ISODate),
                QDateTime.fromString("2020-01-01T00:10:00Z", Qt.DateFormat.ISODate),
            )
        )
        controller.setFrameDuration(QgsInterval(60))
        controller.setFramesPerSecond(12.5)
        canvas = self._canvas()
        canvas.temporal_controller = controller
        # The old canvas frame is deliberately stale: export must consult the
        # controller before building the expression context used by renderers.
        canvas.current.setFrameRate(2)
        canvas.current.setCurrentFrame(-1)
        for mode in (
            Qgis.TemporalNavigationMode.Animated,
            Qgis.TemporalNavigationMode.Movie,
        ):
            with self.subTest(mode=mode):
                controller.setNavigationMode(mode)
                controller.setCurrentFrameNumber(4)
                result = _export_settings(canvas)
                self.assertEqual(result.currentFrame(), 4)
                self.assertEqual(result.frameRate(), 12.5)
                self.assertEqual(result.expressionContext().variable("frame_number"), 4)
                self.assertEqual(
                    result.expressionContext().variable("frame_rate"), 12.5
                )

    def test_native_atlas_scope_drives_exported_symbol_color(self):
        from qgis.core import (
            QgsCoordinateReferenceSystem,
            QgsFeature,
            QgsFillSymbol,
            QgsGeometry,
            QgsProject,
            QgsProperty,
            QgsRectangle,
            QgsSingleSymbolRenderer,
            QgsSymbolLayer,
            QgsVectorLayer,
        )
        from qgis.gui import QgsMapCanvas
        from qgis.PyQt.QtGui import QColor, QImage

        project = QgsProject.instance()
        previous_dirty = project.isDirty()
        layer = QgsVectorLayer("Polygon?crs=EPSG:3857", "atlas color", "memory")
        feature = QgsFeature(layer.fields())
        feature.setGeometry(
            QgsGeometry.fromWkt(
                "POLYGON ((-10000 -10000, 10000 -10000, 10000 10000, "
                "-10000 10000, -10000 -10000))"
            )
        )
        self.assertTrue(layer.dataProvider().addFeatures([feature])[0])
        layer.updateExtents()
        symbol = QgsFillSymbol.createSimple({"color": "255,0,0", "outline_style": "no"})
        symbol.symbolLayer(0).setDataDefinedProperty(
            QgsSymbolLayer.Property.FillColor,
            QgsProperty.fromExpression(
                "if(@atlas_featureid = 17, '0,255,0', '255,0,0')"
            ),
        )
        layer.setRenderer(QgsSingleSymbolRenderer(symbol))
        project.addMapLayer(layer)
        canvas = QgsMapCanvas()
        canvas.freeze(True)
        try:
            canvas.resize(320, 240)
            canvas.setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3857"))
            canvas.setLayers([layer])
            canvas.setExtent(QgsRectangle(100, 200, 900, 800))
            canvas.expressionContextScope().setVariable("atlas_featureid", 17)
            self.assertEqual(
                canvas.createExpressionContext().variable("atlas_featureid"), 17
            )
            with tempfile.TemporaryDirectory() as directory:
                target = Path(directory) / "atlas.png"
                self._run_native_export(canvas, target, "png")
                image = QImage(str(target))
                self.assertFalse(image.isNull())
                self.assertEqual(
                    image.pixelColor(image.width() // 2, image.height() // 2),
                    QColor(0, 255, 0),
                )
        finally:
            canvas.setLayers([])
            canvas.stopRendering()
            canvas.deleteLater()
            project.removeMapLayer(layer.id())
            project.setDirty(previous_dirty)

    def test_native_temporal_and_canvas_scopes_survive_fresh_export_snapshot(self):
        from qcopilots_common.map_export import _export_settings
        from qgis.core import (
            Qgis,
            QgsCoordinateReferenceSystem,
            QgsDateTimeRange,
            QgsInterval,
            QgsRectangle,
            QgsTemporalNavigationObject,
        )
        from qgis.gui import QgsMapCanvas
        from qgis.PyQt.QtCore import QDateTime, Qt

        canvas = QgsMapCanvas()
        canvas.freeze(True)
        controller = QgsTemporalNavigationObject()
        try:
            canvas.resize(320, 240)
            canvas.setDestinationCrs(QgsCoordinateReferenceSystem("EPSG:3857"))
            canvas.setExtent(QgsRectangle(0, 0, 40, 30))
            old_extent = canvas.createExpressionContext().variable("map_extent")
            controller.setTemporalExtents(
                QgsDateTimeRange(
                    QDateTime.fromString("2020-01-01T00:00:00Z", Qt.DateFormat.ISODate),
                    QDateTime.fromString("2020-01-01T00:10:00Z", Qt.DateFormat.ISODate),
                )
            )
            controller.setFrameDuration(QgsInterval(60))
            controller.setFramesPerSecond(12.5)
            controller.setNavigationMode(Qgis.TemporalNavigationMode.Animated)
            controller.setCurrentFrameNumber(4)
            canvas.setTemporalController(controller)
            canvas.expressionContextScope().setVariable(
                "export_context_note", "keep me"
            )
            canvas.expressionContextScope().setVariable("atlas_featureid", 17)
            canvas.setExtent(QgsRectangle(100, 200, 900, 800))
            canvas.setRotation(37)
            snapshot = _export_settings(canvas)
            context = snapshot.expressionContext()
            self.assertEqual(context.variable("export_context_note"), "keep me")
            self.assertEqual(context.variable("atlas_featureid"), 17)
            self.assertTrue(context.hasVariable("canvas_cursor_point"))
            duration = context.variable("frame_duration")
            self.assertIsInstance(duration, QgsInterval)
            self.assertEqual(duration.seconds(), 60)
            self.assertEqual(
                context.variable("total_frame_count"), controller.totalFrameCount()
            )
            self.assertEqual(context.variable("frame_number"), 4)
            self.assertEqual(context.variable("frame_rate"), 12.5)
            self.assertEqual(context.variable("map_rotation"), 37)
            self.assertEqual(
                context.variable("map_extent").boundingBox(), snapshot.visibleExtent()
            )
            self.assertNotEqual(
                context.variable("map_extent").boundingBox(), old_extent.boundingBox()
            )
        finally:
            canvas.setTemporalController(None)
            canvas.stopRendering()
            canvas.deleteLater()

    def test_shutdown_releases_native_runtime_controller_and_interface(self):
        from qgis.PyQt import sip
        from qgis.PyQt.QtCore import QCoreApplication, QEvent

        references = []
        with tempfile.TemporaryDirectory() as directory:
            self._run_native_export(
                self._canvas(),
                Path(directory) / "runtime.png",
                "png",
                capture_runtime=references,
            )
        # QObject destruction is deferred, so allow the normal Qt deletion
        # boundary before checking that the app parent retained no closure.
        QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
        gc.collect()
        coordinator_ref, iface_ref, runtime_ref = references
        self.assertIsNone(coordinator_ref())
        self.assertIsNone(iface_ref())
        runtime = runtime_ref()
        self.assertTrue(runtime is None or sip.isdeleted(runtime))


if __name__ == "__main__":
    unittest.main()
