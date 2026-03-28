from __future__ import annotations

import io
import logging
from unittest.mock import patch

from qgis.core import Qgis

from processingmcpserver.config import MCP_LOG_CATEGORY
from processingmcpserver.log_handler import QgisLogHandler

from ._shared_case_base import ProcessingMCPTestBase


class LogHandlerRuntimeTest(ProcessingMCPTestBase):
    @patch("processingmcpserver.log_handler.QgsMessageLog.logMessage")
    def test_default_category_and_message_format(self, mock_log_message):
        """
        作用：执行测试用例 `default category and message format`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `default category and message format`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mock_log_message`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        handler = QgisLogHandler()
        record = logging.LogRecord(
            name="mcp.server.streamable_http",
            level=logging.INFO,
            pathname=__file__,
            lineno=18,
            msg="startup ready",
            args=(),
            exc_info=None,
        )

        handler.emit(record)

        self.assertEqual(mock_log_message.call_count, 1)
        message, category, level = mock_log_message.call_args[0]
        self.assertEqual(category, MCP_LOG_CATEGORY)
        self.assertEqual(level, Qgis.Info)
        self.assertIn("[INFO]", message)
        self.assertIn("mcp.server.streamable_http", message)
        self.assertIn("startup ready", message)

    def test_level_mapping(self):
        """
        作用：执行测试用例 `level mapping`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `level mapping`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.assertEqual(QgisLogHandler._map_level(logging.INFO), Qgis.Info)
        self.assertEqual(QgisLogHandler._map_level(logging.WARNING), Qgis.Warning)
        self.assertEqual(QgisLogHandler._map_level(logging.ERROR), Qgis.Critical)

    def test_emit_falls_back_to_stderr_when_qgis_logging_fails(self):
        """
        作用：执行测试用例 `emit falls back to stderr when qgis logging fails`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `emit falls back to stderr when qgis logging fails`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        handler = QgisLogHandler()
        record = logging.LogRecord(
            name="mcp.server.streamable_http",
            level=logging.ERROR,
            pathname=__file__,
            lineno=48,
            msg="fallback ready",
            args=(),
            exc_info=None,
        )
        stderr = io.StringIO()

        with (
            patch(
                "processingmcpserver.log_handler.QgsMessageLog.logMessage",
                side_effect=RuntimeError("log boom"),
            ),
            patch.object(
                __import__("processingmcpserver.log_handler").log_handler.sys,
                "__stderr__",
                stderr,
            ),
        ):
            handler.emit(record)

        self.assertIn("fallback ready", stderr.getvalue())
