from __future__ import annotations

"""Logging bridge from Python handlers into the QGIS message log."""

import logging
import sys

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config_types import MCP_LOG_CATEGORY


# Handler that forwards logs to the QGIS message log.
class QgisLogHandler(logging.Handler):
    """Forward Python logging records to the QGIS log panel."""

    def __init__(self, category: str = MCP_LOG_CATEGORY) -> None:
        """Initialize the log category used for QGIS output."""
        super().__init__()
        # Category name shown in the QGIS log panel.
        self._category = category
        self.setFormatter(
            logging.Formatter(
                fmt="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
                datefmt="%Y-%m-%d %H:%M:%S",
            )
        )

    def emit(self, record: logging.LogRecord) -> None:
        """Emit one log record and fall back to stderr on failure."""
        # Format the log and map the level to QGIS.
        try:
            message = self.format(record)
            level = self._map_level(record.levelno)
            QgsMessageLog.logMessage(message, self._category, level)
        except Exception:
            # Fall back to stderr if QGIS logging fails.
            self._fallback_emit(record)

    @staticmethod
    def _map_level(levelno: int) -> Qgis.MessageLevel:
        """Map a logging level to a QGIS message level."""
        # Convert Python logging levels to QGIS levels.
        if levelno >= logging.ERROR:
            return Qgis.Critical
        if levelno >= logging.WARNING:
            return Qgis.Warning
        return Qgis.Info

    def _fallback_emit(self, record: logging.LogRecord) -> None:
        """Write the message to `stderr` when QGIS logging is unavailable."""
        # Final fallback to avoid swallowing the record.
        try:
            message = self.format(record)
            if sys.__stderr__:
                sys.__stderr__.write(message + "\n")
                sys.__stderr__.flush()
        except Exception:
            return
