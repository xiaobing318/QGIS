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


class MainThreadRunner(QObject):
    def run(self, func: Callable[[], ResultT]) -> ResultT:
        app = QCoreApplication.instance()
        if app is None or QThread.currentThread() == app.thread():
            return func()

        result: dict[str, object] = {}
        done = threading.Event()

        def wrapper() -> None:
            try:
                result["value"] = func()
            except Exception as exc:
                result["error"] = exc
            finally:
                done.set()

        QMetaObject.invokeMethod(
            self, "_execute", Qt.ConnectionType.QueuedConnection, Q_ARG(object, wrapper)
        )
        done.wait()

        if "error" in result:
            raise result["error"]  # type: ignore[misc]

        return result.get("value")  # type: ignore[return-value]

    @pyqtSlot(object)
    def _execute(self, func: Callable[[], None]) -> None:
        func()
