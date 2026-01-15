from __future__ import annotations

import logging
import sys

from qgis.core import Qgis, QgsMessageLog


# 将日志转发到 QGIS 消息日志的处理器。
class QgisLogHandler(logging.Handler):
    def __init__(self, category: str = "Processing MCP") -> None:
        super().__init__()
        # 在 QGIS 日志面板显示的分类名称。
        self._category = category

    def emit(self, record: logging.LogRecord) -> None:
        # 格式化日志并将级别映射到 QGIS 级别。
        try:
            message = self.format(record)
            level = self._map_level(record.levelno)
            QgsMessageLog.logMessage(message, self._category, level)
        except Exception:
            # QGIS 记录失败时回退到 stderr。
            self._fallback_emit(record)

    @staticmethod
    def _map_level(levelno: int) -> Qgis.MessageLevel:
        # 将 Python 日志级别转换为 QGIS 级别。
        if levelno >= logging.ERROR:
            return Qgis.Critical
        if levelno >= logging.WARNING:
            return Qgis.Warning
        return Qgis.Info

    def _fallback_emit(self, record: logging.LogRecord) -> None:
        # 最后的兜底写入，避免吞掉异常。
        try:
            message = self.format(record)
            if sys.__stderr__:
                sys.__stderr__.write(message + "\n")
                sys.__stderr__.flush()
        except Exception:
            return
