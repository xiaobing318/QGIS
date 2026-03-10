from __future__ import annotations

import threading
import time
from unittest.mock import MagicMock, patch

from qgis.PyQt.QtCore import QCoreApplication, QMetaObject, Qt, Q_ARG

from processingmcpserver.mcp_main_thread_runner import McpMainThreadRunner

from ._shared_case_base import ProcessingMCPTestBase


class McpMainThreadRunnerRuntimeTest(ProcessingMCPTestBase):
    def _pump_events_until(
        self,
        predicate,
        timeout_seconds: float = 2.0,
        failure_message: str = "Condition was not satisfied before timeout.",
    ) -> None:
        app = QCoreApplication.instance()
        self.assertIsNotNone(app)

        deadline = time.monotonic() + timeout_seconds
        while time.monotonic() < deadline:
            app.processEvents()
            if predicate():
                return
            time.sleep(0.01)

        app.processEvents()
        self.assertTrue(predicate(), failure_message)

    def test_constructor_rejects_non_positive_timeout(self):
        with self.assertRaises(ValueError) as ctx:
            McpMainThreadRunner(wait_timeout_seconds=0)
        self.assertIn("wait_timeout_seconds must be > 0", str(ctx.exception))

    def test_run_executes_directly_on_main_thread(self):
        runner = McpMainThreadRunner()
        self.assertEqual(runner.run(lambda: "ok"), "ok")

    def test_qt_event_loop_executes_queued_callback_when_invoke_method_returns_none(self):
        runner = McpMainThreadRunner()
        state = {"called": False}

        def wrapper() -> None:
            state["called"] = True

        invoke_result = QMetaObject.invokeMethod(
            runner, "_execute", Qt.ConnectionType.QueuedConnection, Q_ARG(object, wrapper)
        )

        self.assertIsNone(invoke_result)
        self._pump_events_until(
            lambda: state["called"],
            failure_message="Queued callback was not executed by the Qt event loop.",
        )

    def test_run_executes_queued_callback_for_worker_thread(self):
        runner = McpMainThreadRunner()
        fake_app = MagicMock()
        fake_app.thread.return_value = object()

        with (
            patch(
                "processingmcpserver.mcp_main_thread_runner.QCoreApplication.instance",
                return_value=fake_app,
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.QThread.currentThread",
                return_value=object(),
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.Q_ARG",
                side_effect=lambda _type, func: func,
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.QMetaObject.invokeMethod"
            ) as mock_invoke,
        ):
            mock_invoke.side_effect = (
                lambda _obj, _method, connection_type, func: (
                    self.assertEqual(connection_type, Qt.ConnectionType.QueuedConnection),
                    func(),
                    True,
                )[-1]
            )
            self.assertEqual(runner.run(lambda: "queued"), "queued")

    def test_run_executes_queued_callback_for_real_worker_thread(self):
        runner = McpMainThreadRunner()
        result: dict[str, object] = {}
        finished = threading.Event()

        def worker() -> None:
            try:
                result["value"] = runner.run(lambda: "queued")
            except Exception as exc:  # pragma: no cover - failure path asserted below
                result["error"] = exc
            finally:
                finished.set()

        thread = threading.Thread(target=worker, name="processingmcpserver-test-worker")
        thread.start()

        try:
            self._pump_events_until(
                finished.is_set,
                failure_message="Worker thread did not finish after Qt events were processed.",
            )
        finally:
            thread.join(timeout=1.0)

        self.assertFalse(thread.is_alive(), "Worker thread did not exit cleanly.")
        if "error" in result:
            raise result["error"]  # type: ignore[misc]
        self.assertEqual(result.get("value"), "queued")

    def test_run_propagates_worker_thread_callback_error(self):
        runner = McpMainThreadRunner()
        fake_app = MagicMock()
        fake_app.thread.return_value = object()

        with (
            patch(
                "processingmcpserver.mcp_main_thread_runner.QCoreApplication.instance",
                return_value=fake_app,
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.QThread.currentThread",
                return_value=object(),
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.Q_ARG",
                side_effect=lambda _type, func: func,
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.QMetaObject.invokeMethod",
                side_effect=lambda _obj, _method, _connection_type, func: (func(), True)[1],
            ),
        ):
            with self.assertRaises(ValueError) as ctx:
                runner.run(lambda: (_ for _ in ()).throw(ValueError("boom")))
            self.assertIn("boom", str(ctx.exception))

    def test_run_raises_when_invoke_method_fails(self):
        runner = McpMainThreadRunner()
        fake_app = MagicMock()
        fake_app.thread.return_value = object()

        with (
            patch(
                "processingmcpserver.mcp_main_thread_runner.QCoreApplication.instance",
                return_value=fake_app,
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.QThread.currentThread",
                return_value=object(),
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.Q_ARG",
                side_effect=lambda _type, func: func,
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.QMetaObject.invokeMethod",
                return_value=False,
            ),
        ):
            with self.assertRaises(RuntimeError) as ctx:
                runner.run(lambda: "never-runs")
            self.assertIn("Failed to schedule callback", str(ctx.exception))

    def test_run_raises_when_worker_thread_times_out(self):
        runner = McpMainThreadRunner(wait_timeout_seconds=0.01)
        fake_app = MagicMock()
        fake_app.thread.return_value = object()

        with (
            patch(
                "processingmcpserver.mcp_main_thread_runner.QCoreApplication.instance",
                return_value=fake_app,
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.QThread.currentThread",
                return_value=object(),
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.Q_ARG",
                side_effect=lambda _type, func: func,
            ),
            patch(
                "processingmcpserver.mcp_main_thread_runner.QMetaObject.invokeMethod",
                return_value=True,
            ),
        ):
            with self.assertRaises(TimeoutError) as ctx:
                runner.run(lambda: "never-finishes")
            self.assertIn("Timed out while waiting", str(ctx.exception))
