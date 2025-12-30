from __future__ import annotations

import logging
import sys

from qgis.core import Qgis, QgsMessageLog


class QgisLogHandler(logging.Handler):
    def __init__(self, category: str = "Processing MCP") -> None:
        super().__init__()
        self._category = category

    def emit(self, record: logging.LogRecord) -> None:
        try:
            message = self.format(record)
            level = self._map_level(record.levelno)
            QgsMessageLog.logMessage(message, self._category, level)
        except Exception:
            self._fallback_emit(record)

    @staticmethod
    def _map_level(levelno: int) -> Qgis.MessageLevel:
        if levelno >= logging.ERROR:
            return Qgis.Critical
        if levelno >= logging.WARNING:
            return Qgis.Warning
        return Qgis.Info

    def _fallback_emit(self, record: logging.LogRecord) -> None:
        try:
            message = self.format(record)
            if sys.__stderr__:
                sys.__stderr__.write(message + "\n")
                sys.__stderr__.flush()
        except Exception:
            return
