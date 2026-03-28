"""Logging bridge that forwards Python records to the QGIS message log."""

from __future__ import annotations

import logging
import sys

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config_types import MCP_LOG_CATEGORY


class QgisLogHandler(logging.Handler):
    """Forward Python logging records to the QGIS message log."""

    def __init__(self, category: str = MCP_LOG_CATEGORY) -> None:
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 `QgisLogHandler` 类的对象协作流程中被调用，用于完成该类职责内的具体步骤。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `category`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `MCP_LOG_CATEGORY`。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        super().__init__()
        # Category shown in the QGIS message log.
        self._category = category
        self.setFormatter(
            logging.Formatter(
                fmt="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
                datefmt="%Y-%m-%d %H:%M:%S",
            )
        )

    def emit(self, record: logging.LogRecord) -> None:
        """
        作用：实现 `emit` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `emit` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 `QgisLogHandler` 类的对象协作流程中被调用，用于完成该类职责内的具体步骤。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `record`（`logging.LogRecord`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        # Format the record and map its level to QGIS.
        try:
            message = self.format(record)
            level = self._map_level(record.levelno)
            QgsMessageLog.logMessage(message, self._category, level)
        except Exception:
            # Fall back to stderr if QGIS logging fails.
            self._fallback_emit(record)

    @staticmethod
    def _map_level(levelno: int) -> Qgis.MessageLevel:
        """
        作用：封装内部辅助步骤 `_map_level`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_map_level`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 `QgisLogHandler` 类的对象协作流程中被调用，用于完成该类职责内的具体步骤。
        参数与返回：
        - 参数 `levelno`（`int`）：标识或模式参数，用于指定目标对象或流程分支。
        - 返回：返回 `Qgis.MessageLevel` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Qgis.MessageLevel` 类型结果，返回值语义遵循该函数实现约定。
        """
        # Convert Python logging levels to QGIS message levels.
        if levelno >= logging.ERROR:
            return Qgis.Critical
        if levelno >= logging.WARNING:
            return Qgis.Warning
        return Qgis.Info

    def _fallback_emit(self, record: logging.LogRecord) -> None:
        """
        作用：封装内部辅助步骤 `_fallback_emit`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_fallback_emit`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 `QgisLogHandler` 类的对象协作流程中被调用，用于完成该类职责内的具体步骤。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `record`（`logging.LogRecord`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        # Final fallback to avoid dropping the record.
        try:
            message = self.format(record)
            if sys.__stderr__:
                sys.__stderr__.write(message + "\n")
                sys.__stderr__.flush()
        except Exception:
            return
