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
        """Initialize the runner state and validate the timeout."""
        super().__init__(parent)
        timeout = float(wait_timeout_seconds)
        if timeout <= 0:
            raise ValueError("wait_timeout_seconds must be > 0")
        self._wait_timeout_seconds = timeout

    def run(self, func: Callable[[], ResultT]) -> ResultT:
        """Execute a callback synchronously, marshalling it to the Qt main thread when needed."""
        app = QCoreApplication.instance()
        if app is None or QThread.currentThread() == app.thread():
            return func()

        result: dict[str, object] = {}
        done = threading.Event()

        def wrapper() -> None:
            """Run the callback and capture either its result or raised exception."""
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
        """Execute the queued callback on the Qt main thread."""
        func()
