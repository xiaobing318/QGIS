"""Transport adapters for exposing FastMCP over HTTP, SSE, or stdio."""

from __future__ import annotations

import asyncio
import sys
import threading
import traceback
from typing import TYPE_CHECKING, Optional

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config import ProcessingMCPServerConfig
from processingmcpserver.config_types import MCP_LOG_CATEGORY

if TYPE_CHECKING:
    from mcp.server.fastmcp import FastMCP


class BaseMcpTransport:
    """Base MCP transport that unifies thread lifecycle and logging behavior."""

    def __init__(self, mcp: "FastMCP", config: ProcessingMCPServerConfig) -> None:
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mcp`（`"FastMCP"`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `config`（`ProcessingMCPServerConfig`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self._mcp = mcp
        self._config = config
        self._thread: Optional[threading.Thread] = None

    def start(self) -> bool:
        """
        作用：实现 `start` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `start` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        """
        if self.is_running():
            self._log(
                f"Processing MCP transport already running ({self._context_details()}).",
                Qgis.Warning,
            )
            return True
        self._log(
            f"Starting Processing MCP transport thread '{self.thread_name}' "
            f"({self._context_details()}).",
            Qgis.Info,
        )
        self._thread = threading.Thread(
            target=self._run_with_logging, name=self.thread_name, daemon=True
        )
        self._thread.start()
        return True

    def stop(self) -> None:
        """
        作用：实现 `stop` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `stop` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self._log(
            f"Stopping Processing MCP transport thread '{self.thread_name}' "
            f"({self._context_details()}).",
            Qgis.Info,
        )
        self._request_stop()
        if not self._thread:
            return
        self._thread.join(timeout=2)
        if not self._thread.is_alive():
            self._thread = None
            self._log(
                f"Processing MCP transport stopped ({self._context_details()}).",
                Qgis.Info,
            )
            return
        self._log(
            f"Processing MCP transport is still running after stop request "
            f"({self._context_details()}).",
            Qgis.Warning,
        )

    def is_running(self) -> bool:
        """
        作用：实现 `is_running` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `is_running` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        """
        return self._thread is not None and self._thread.is_alive()

    @property
    def thread_name(self) -> str:
        """
        作用：实现 `thread_name` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `thread_name` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return "ProcessingMCPTransport"

    def describe(self) -> str:
        """
        作用：实现 `describe` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `describe` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return "MCP transport"

    def _run(self) -> None:
        """
        作用：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        异常：可能显式抛出 `NotImplementedError`。
        """
        raise NotImplementedError

    def _run_with_logging(self) -> None:
        """
        作用：封装内部辅助步骤 `_run_with_logging`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_run_with_logging`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        try:
            self._ensure_stdio_streams()
            self._run()
        except Exception:
            self._log_exception(
                f"Processing MCP transport crashed ({self._context_details()})"
            )

    def _ensure_stdio_streams(self) -> None:
        """
        作用：确保 `_ensure_stdio_streams` 负责的前置状态可用，必要时执行初始化或修复动作。
        用途：确保 `_ensure_stdio_streams` 负责的前置状态可用，必要时执行初始化或修复动作。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        if sys.stdout is None and sys.__stdout__ is not None:
            sys.stdout = sys.__stdout__
        if sys.stderr is None and sys.__stderr__ is not None:
            sys.stderr = sys.__stderr__

    def _request_stop(self) -> None:
        """
        作用：封装内部辅助步骤 `_request_stop`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_request_stop`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        return

    def _context_details(self) -> str:
        """
        作用：封装内部辅助步骤 `_context_details`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_context_details`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        settings = getattr(self._mcp, "settings", None)
        details = [f"transport={self._config.transport or 'streamable-http'}"]
        host = getattr(settings, "host", None)
        port = getattr(settings, "port", None)
        if host:
            details.append(f"host={host}")
        if port:
            details.append(f"port={port}")
        streamable_path = getattr(settings, "streamable_http_path", None)
        if streamable_path:
            details.append(f"streamable_http_path={streamable_path}")
        sse_path = getattr(settings, "sse_path", None)
        if sse_path:
            details.append(f"sse_path={sse_path}")
        message_path = getattr(settings, "message_path", None)
        if message_path:
            details.append(f"message_path={message_path}")
        mount_path = getattr(settings, "mount_path", None)
        if mount_path:
            details.append(f"mount_path={mount_path}")
        return ", ".join(details)

    def _log(self, message: str, level=Qgis.Info) -> None:
        """
        作用：封装内部辅助步骤 `_log`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_log`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `message`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `level`：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `Qgis.Info`。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        QgsMessageLog.logMessage(message, MCP_LOG_CATEGORY, level)

    def _log_exception(self, message: str, level=Qgis.Critical) -> None:
        """
        作用：封装内部辅助步骤 `_log_exception`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_log_exception`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `message`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `level`：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `Qgis.Critical`。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        details = traceback.format_exc()
        self._log(f"{message}\n{details}", level)

    def _apply_cors(self, app):
        """
        作用：封装内部辅助步骤 `_apply_cors`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_apply_cors`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `app`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        if not self._config.cors_origins:
            return app
        try:
            from starlette.middleware.cors import CORSMiddleware
        except Exception:
            self._log_exception("Failed to import CORS middleware")
            return app

        return CORSMiddleware(
            app,
            allow_origins=self._config.cors_origins,
            allow_methods=["GET", "POST", "DELETE", "OPTIONS"],
            allow_headers=self._config.cors_allow_headers or [],
            expose_headers=["Mcp-Session-Id"],
        )


class UvicornTransport(BaseMcpTransport):
    """Uvicorn-based HTTP/SSE transport implementation."""

    def __init__(self, mcp: "FastMCP", config: ProcessingMCPServerConfig) -> None:
        """
        作用：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `__init__`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `mcp`（`"FastMCP"`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `config`（`ProcessingMCPServerConfig`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        super().__init__(mcp, config)
        self._uvicorn_server = None

    def _run(self) -> None:
        """
        作用：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        try:
            import uvicorn
        except Exception as exc:
            self._log(f"Failed to import uvicorn: {exc}", Qgis.Critical)
            return

        try:
            app = self._build_app()
        except Exception:
            self._log_exception("Failed to build MCP ASGI app")
            return

        config = self._create_uvicorn_config(uvicorn, app)
        if config is None:
            return

        self._uvicorn_server = uvicorn.Server(config)

        try:
            asyncio.run(self._uvicorn_server.serve())
        except Exception:
            self._log_exception("Processing MCP server stopped with error")
        finally:
            self._uvicorn_server = None

    def _request_stop(self) -> None:
        """
        作用：封装内部辅助步骤 `_request_stop`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_request_stop`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        if self._uvicorn_server:
            self._uvicorn_server.should_exit = True

    def _build_app(self):
        """
        作用：构建 `_build_app` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_app` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        异常：可能显式抛出 `NotImplementedError`。
        """
        raise NotImplementedError

    def _build_log_config(self) -> dict[str, object]:
        """
        作用：构建 `_build_log_config` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_log_config` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, object]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, object]` 类型结果，返回值语义遵循该函数实现约定。
        """
        log_level = self._mcp.settings.log_level
        return {
            "version": 1,
            "disable_existing_loggers": False,
            "formatters": {
                "default": {
                    "format": "%(asctime)s [%(levelname)s] %(name)s: %(message)s",
                    "datefmt": "%Y-%m-%d %H:%M:%S",
                }
            },
            "handlers": {
                "qgis": {
                    "class": "processingmcpserver.log_handler.QgisLogHandler",
                    "formatter": "default",
                    "level": log_level,
                }
            },
            "loggers": {
                "uvicorn": {
                    "handlers": ["qgis"],
                    "level": log_level,
                    "propagate": False,
                },
                "uvicorn.error": {
                    "handlers": ["qgis"],
                    "level": log_level,
                    "propagate": False,
                },
                "uvicorn.access": {
                    "handlers": ["qgis"],
                    "level": log_level,
                    "propagate": False,
                },
                "mcp": {
                    "handlers": ["qgis"],
                    "level": log_level,
                    "propagate": False,
                },
                "mcp.server": {
                    "handlers": ["qgis"],
                    "level": log_level,
                    "propagate": False,
                },
                "mcp.server.streamable_http": {
                    "handlers": ["qgis"],
                    "level": log_level,
                    "propagate": False,
                },
                "mcp.server.streamable_http_manager": {
                    "handlers": ["qgis"],
                    "level": log_level,
                    "propagate": False,
                },
                "starlette": {
                    "handlers": ["qgis"],
                    "level": log_level,
                    "propagate": False,
                },
            },
        }

    def _create_uvicorn_config(self, uvicorn, app):
        """
        作用：封装内部辅助步骤 `_create_uvicorn_config`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_create_uvicorn_config`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `uvicorn`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `app`：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        log_config = None
        try:
            log_config = self._build_log_config()
        except Exception:
            self._log_exception("Failed to build uvicorn logging configuration")

        try:
            return uvicorn.Config(
                app,
                host=self._mcp.settings.host,
                port=self._mcp.settings.port,
                log_level=self._mcp.settings.log_level.lower(),
                log_config=log_config,
            )
        except Exception:
            self._log_exception("Failed to configure uvicorn server")

        if log_config is None:
            return None

        try:
            self._log(
                "Retrying uvicorn configuration without custom logging.",
                Qgis.Warning,
            )
            return uvicorn.Config(
                app,
                host=self._mcp.settings.host,
                port=self._mcp.settings.port,
                log_level=self._mcp.settings.log_level.lower(),
                log_config=None,
            )
        except Exception:
            self._log_exception("Failed to configure uvicorn server without custom logging")
            return None


class StreamableHttpTransport(UvicornTransport):
    """MCP transport implementation based on Streamable HTTP."""

    @property
    def thread_name(self) -> str:
        """
        作用：实现 `thread_name` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `thread_name` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return "ProcessingMCPStreamableHTTP"

    def describe(self) -> str:
        """
        作用：实现 `describe` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `describe` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        path = self._mcp.settings.streamable_http_path
        return f"Streamable HTTP on http://{host}:{port}{path}"

    def _build_app(self):
        """
        作用：构建 `_build_app` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_app` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return self._apply_cors(self._mcp.streamable_http_app())


class SseTransport(UvicornTransport):
    """MCP transport implementation based on SSE."""

    @property
    def thread_name(self) -> str:
        """
        作用：实现 `thread_name` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `thread_name` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return "ProcessingMCPSSE"

    def describe(self) -> str:
        """
        作用：实现 `describe` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `describe` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        sse_path = self._mcp.settings.sse_path
        message_path = self._mcp.settings.message_path
        return f"SSE on http://{host}:{port}{sse_path} (messages {message_path})"

    def _build_app(self):
        """
        作用：构建 `_build_app` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_app` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        返回结果：返回执行结果对象或状态值，具体结构以当前实现生成的数据为准。
        """
        return self._apply_cors(self._mcp.sse_app(self._config.mount_path))


class StdioTransport(BaseMcpTransport):
    """MCP transport implementation based on stdin/stdout."""

    @property
    def thread_name(self) -> str:
        """
        作用：实现 `thread_name` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `thread_name` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return "ProcessingMCPStdio"

    def describe(self) -> str:
        """
        作用：实现 `describe` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `describe` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return "STDIO (stdin/stdout)"

    def _run(self) -> None:
        """
        作用：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_run`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        if (
            sys.stdin is None
            or sys.stdout is None
            or not hasattr(sys.stdin, "buffer")
            or not hasattr(sys.stdout, "buffer")
        ):
            self._log(
                "STDIO transport unavailable: stdin/stdout is not attached.",
                Qgis.Critical,
            )
            return
        try:
            self._mcp.run(transport="stdio")
        except Exception:
            self._log_exception("Processing MCP stdio server stopped with error")

    def stop(self) -> None:
        """
        作用：实现 `stop` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `stop` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        if self.is_running():
            self._log(
                "STDIO transport does not support graceful shutdown; close QGIS or stdin.",
                Qgis.Warning,
            )
        super().stop()


def create_transport(
    mcp: "FastMCP", config: ProcessingMCPServerConfig
) -> BaseMcpTransport:
    """
    作用：创建 `transport`，完成当前函数负责的处理步骤并产出结果。
    用途：创建 `transport`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 传输层启动与停止流程中被调用，用于管理 stdio/http/sse 运行线程与日志。
    参数与返回：
    - 参数 `mcp`（`"FastMCP"`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `config`（`ProcessingMCPServerConfig`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `BaseMcpTransport` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `BaseMcpTransport` 类型结果，返回值语义遵循该函数实现约定。
    """
    transport = (config.transport or "streamable-http").lower()
    if transport == "stdio":
        return StdioTransport(mcp, config)
    if transport == "sse":
        return SseTransport(mcp, config)
    return StreamableHttpTransport(mcp, config)
