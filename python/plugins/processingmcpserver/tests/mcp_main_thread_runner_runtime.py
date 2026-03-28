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
        """
        作用：封装内部辅助步骤 `_pump_events_until`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_pump_events_until`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `predicate`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `timeout_seconds`（`float`）：数值控制参数，用于限制范围、数量或时限。 默认值为 `2.0`。
        - 参数 `failure_message`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"Condition was not satisfied before timeout."`。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：执行测试用例 `constructor rejects non positive timeout`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `constructor rejects non positive timeout`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        with self.assertRaises(ValueError) as ctx:
            McpMainThreadRunner(wait_timeout_seconds=0)
        self.assertIn("wait_timeout_seconds must be > 0", str(ctx.exception))

    def test_run_executes_directly_on_main_thread(self):
        """
        作用：执行测试用例 `run executes directly on main thread`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `run executes directly on main thread`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        runner = McpMainThreadRunner()
        self.assertEqual(runner.run(lambda: "ok"), "ok")

    def test_qt_event_loop_executes_queued_callback_when_invoke_method_returns_none(self):
        """
        作用：执行测试用例 `qt event loop executes queued callback when invoke method returns none`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `qt event loop executes queued callback when invoke method returns none`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        runner = McpMainThreadRunner()
        state = {"called": False}

        def wrapper() -> None:
            """
            作用：处理 `wrapper` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            用途：处理 `wrapper` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
            参数与返回：
            - 参数：无。
            - 返回：无返回值。
            返回结果：无返回值。
            """
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
        """
        作用：执行测试用例 `run executes queued callback for worker thread`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `run executes queued callback for worker thread`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：执行测试用例 `run executes queued callback for real worker thread`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `run executes queued callback for real worker thread`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        异常：可能显式抛出 `result`。
        """
        runner = McpMainThreadRunner()
        result: dict[str, object] = {}
        finished = threading.Event()

        def worker() -> None:
            """
            作用：处理 `worker` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            用途：处理 `worker` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
            参数与返回：
            - 参数：无。
            - 返回：无返回值。
            返回结果：无返回值。
            """
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
        """
        作用：执行测试用例 `run propagates worker thread callback error`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `run propagates worker thread callback error`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：执行测试用例 `run raises when invoke method fails`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `run raises when invoke method fails`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：执行测试用例 `run raises when worker thread times out`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `run raises when worker thread times out`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
