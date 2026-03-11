from __future__ import annotations

"""Qt main-thread runner used by MCP tools and server callbacks."""

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
    """将任意回调切换到 Qt 主线程执行并同步返回结果。"""

    DEFAULT_WAIT_TIMEOUT_SECONDS = 30.0

    def __init__(
        self,
        wait_timeout_seconds: float = DEFAULT_WAIT_TIMEOUT_SECONDS,
        parent: QObject | None = None,
    ) -> None:
        """初始化 McpMainThreadRunner 实例状态。"""
        super().__init__(parent)
        timeout = float(wait_timeout_seconds)
        if timeout <= 0:
            raise ValueError("wait_timeout_seconds must be > 0")
        self._wait_timeout_seconds = timeout

    def run(self, func: Callable[[], ResultT]) -> ResultT:
        """执行回调并在跨线程场景下阻塞等待执行完成。"""
        app = QCoreApplication.instance()
        if app is None or QThread.currentThread() == app.thread():
            return func()

        result: dict[str, object] = {}
        done = threading.Event()

        def wrapper() -> None:
            """包装回调并同步传递执行结果。"""
            try:
                result["value"] = func()
            except Exception as exc:
                result["error"] = exc
            finally:
                done.set()

        invoke_ok = QMetaObject.invokeMethod(
            self, "_execute", Qt.ConnectionType.QueuedConnection, Q_ARG(object, wrapper)
        )
        # PyQt6 may return None here even when the callback was queued successfully.
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
        """作为 Qt 槽函数执行排队回调，副作用是运行传入函数。"""
        func()
