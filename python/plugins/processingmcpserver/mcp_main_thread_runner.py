"""Qt main-thread runner used by MCP tools and server callbacks."""

from __future__ import annotations

import threading
from typing import Callable, TypeVar

from qgis.PyQt.QtCore import (
    QCoreApplication,
    QMetaObject,
    QObject,
    Qt,
    QThread,
    Q_ARG,
    pyqtSlot,
)

ResultT = TypeVar("ResultT")


class McpMainThreadRunner(QObject):
    """Run callbacks on the Qt main thread and return their results synchronously."""

    DEFAULT_WAIT_TIMEOUT_SECONDS = 30.0

    def __init__(
        self,
        wait_timeout_seconds: float = DEFAULT_WAIT_TIMEOUT_SECONDS,
        parent: QObject | None = None,
    ) -> None:
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在主线程调度流程中被调用，用于把 QGIS 相关操作切换到主线程执行。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `wait_timeout_seconds`（`float`）：数值控制参数，用于限制范围、数量或时限。 默认值为 `DEFAULT_WAIT_TIMEOUT_SECONDS`。
        - 参数 `parent`（`QObject | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 返回：无返回值。
        返回结果：无返回值。
        异常：可能显式抛出 `ValueError`。
        """
        super().__init__(parent)
        timeout = float(wait_timeout_seconds)
        if timeout <= 0:
            raise ValueError("wait_timeout_seconds must be > 0")
        self._wait_timeout_seconds = timeout

    def run(self, func: Callable[[], ResultT]) -> ResultT:
        """
        作用：实现 `run` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `run` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在主线程调度流程中被调用，用于把 QGIS 相关操作切换到主线程执行。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `func`（`Callable[[], ResultT]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回 `ResultT` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `ResultT` 类型结果，返回值语义遵循该函数实现约定。
        异常：可能显式抛出 `RuntimeError`, `TimeoutError`, `result`。
        """
        app = QCoreApplication.instance()
        if app is None or QThread.currentThread() == app.thread():
            return func()

        result: dict[str, object] = {}
        done = threading.Event()

        def wrapper() -> None:
            """
            作用：处理 `wrapper` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            用途：处理 `wrapper` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
            使用场景：在主线程调度流程中被调用，用于把 QGIS 相关操作切换到主线程执行。
            参数与返回：
            - 参数：无。
            - 返回：无返回值。
            返回结果：无返回值。
            """
            try:
                result["value"] = func()
            except Exception as exc:
                result["error"] = exc
            finally:
                done.set()

        invoke_ok = QMetaObject.invokeMethod(
            self, "_execute", Qt.ConnectionType.QueuedConnection, Q_ARG(object, wrapper)
        )
        # PyQt6 may return `None` here even when the callback was queued successfully.
        if invoke_ok is False:
            raise RuntimeError("Failed to schedule callback on Qt main thread.")

        if not done.wait(self._wait_timeout_seconds):
            raise TimeoutError(
                "Timed out while waiting for callback on Qt main thread "
                f"({self._wait_timeout_seconds:.1f}s)."
            )

        if "error" in result:
            raise result["error"]  # type: ignore[misc]
        return result.get("value")  # type: ignore[return-value]

    @pyqtSlot(object)
    def _execute(self, func: Callable[[], None]) -> None:
        """
        作用：封装内部辅助步骤 `_execute`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_execute`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在主线程调度流程中被调用，用于把 QGIS 相关操作切换到主线程执行。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `func`（`Callable[[], None]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        func()
