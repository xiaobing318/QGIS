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


# 在 Qt 主线程执行回调并返回结果的辅助类。
class MainThreadRunner(QObject):
    def run(self, func: Callable[[], ResultT]) -> ResultT:
        # 已在主线程（或无应用实例）时直接执行。
        app = QCoreApplication.instance()
        if app is None or QThread.currentThread() == app.thread():
            return func()

        # 用于跨线程保存结果，并阻塞等待完成。
        result: dict[str, object] = {}
        done = threading.Event()

        # 包装执行，捕获返回值或异常。
        def wrapper() -> None:
            try:
                result["value"] = func()
            except Exception as exc:
                result["error"] = exc
            finally:
                done.set()

        # 将调用排入 Qt 主事件循环。
        QMetaObject.invokeMethod(
            self, "_execute", Qt.ConnectionType.QueuedConnection, Q_ARG(object, wrapper)
        )
        # 等待排队调用完成。
        done.wait()

        # 重新抛出执行时捕获的异常。
        if "error" in result:
            raise result["error"]  # type: ignore[misc]

        # 返回主线程计算的结果。
        return result.get("value")  # type: ignore[return-value]

    @pyqtSlot(object)
    def _execute(self, func: Callable[[], None]) -> None:
        # 供队列调用的槽函数。
        func()
