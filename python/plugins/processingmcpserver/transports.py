"""Transport adapters for exposing FastMCP over HTTP, SSE, or stdio."""

from __future__ import annotations

import asyncio
import socket
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
        if not self._preflight_start():
            return False
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

    def endpoint_url(self) -> Optional[str]:
        """
        作用：返回当前传输模式可用于客户端连接的主入口 URL。
        用途：为启动日志提供最终连接地址，便于定位配置错误与联调。
        使用场景：在服务启动完成后由上层调用，用于输出最终 MCP URL。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `Optional[str]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Optional[str]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return None

    def _preflight_start(self) -> bool:
        """
        作用：执行传输层启动前置校验，失败时阻止后续启动动作。
        用途：在真正创建线程前提前暴露可预见错误（如端口占用）。
        使用场景：在 `start` 中被调用，作为统一前置校验钩子。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        """
        return True

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

    def _preflight_start(self) -> bool:
        """
        作用：实现 `UvicornTransport` 的启动前置校验。
        用途：启动前检测 host/port 是否可绑定，避免线程启动后立即报错退出。
        使用场景：在 HTTP/SSE 传输模式启动前调用。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `bool` 类型结果，返回值语义遵循该函数实现约定。
        """
        settings = getattr(self._mcp, "settings", None)
        host = str(getattr(settings, "host", "") or self._config.host or "")
        port_raw = getattr(settings, "port", self._config.port)
        try:
            port = int(port_raw)
        except (TypeError, ValueError):
            self._log(
                f"Processing MCP port check failed: invalid port value {port_raw!r}.",
                Qgis.Critical,
            )
            return False

        if port <= 0 or port > 65535:
            self._log(
                f"Processing MCP port check failed: port {port} is out of range (1-65535).",
                Qgis.Critical,
            )
            return False

        ok, detail = _is_tcp_bind_available(host, port)
        if ok:
            return True

        fallback_port = _find_available_port(host, port)
        if fallback_port is not None:
            try:
                setattr(settings, "port", fallback_port)
            except Exception as exc:
                self._log(
                    (
                        "Processing MCP port check failed: "
                        f"configured {host}:{port} is unavailable ({detail}), and fallback "
                        f"port {fallback_port} cannot be applied to runtime settings ({exc})."
                    ),
                    Qgis.Critical,
                )
                return False
            self._log(
                (
                    "Processing MCP port check warning: "
                    f"configured {host}:{port} is unavailable ({detail}); "
                    f"automatically selected fallback port {fallback_port}."
                ),
                Qgis.Warning,
            )
            return True

        self._log(
            (
                "Processing MCP port check failed: "
                f"{host}:{port} is already in use or unavailable ({detail}). "
                "No fallback port was found. Please update processingmcpserver/config.json "
                "with an unused port."
            ),
            Qgis.Critical,
        )
        return False

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

    def endpoint_url(self) -> Optional[str]:
        """
        作用：返回 Streamable HTTP 模式对外提供的最终 MCP URL。
        用途：为启动成功日志输出可直接复制使用的连接地址。
        使用场景：上层服务启动成功后调用。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `Optional[str]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Optional[str]` 类型结果，返回值语义遵循该函数实现约定。
        """
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        path = self._mcp.settings.streamable_http_path
        return f"http://{host}:{port}{path}"

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

    def endpoint_url(self) -> Optional[str]:
        """
        作用：返回 SSE 模式主入口 URL（SSE 端点）。
        用途：为启动成功日志输出连接地址。
        使用场景：上层服务启动成功后调用。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `Optional[str]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Optional[str]` 类型结果，返回值语义遵循该函数实现约定。
        """
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        sse_path = self._mcp.settings.sse_path
        return f"http://{host}:{port}{sse_path}"

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


def _is_tcp_bind_available(host: str, port: int) -> tuple[bool, str]:
    """
    作用：检查给定 host/port 是否可用于 TCP bind。
    用途：在启动前识别端口冲突，避免 uvicorn 报 [WinError 10048] 后才失败。
    使用场景：仅供传输层启动前置校验调用。
    参数与返回：
    - 参数 `host`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `port`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 返回：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：`(True, "")` 表示可绑定，`(False, detail)` 表示不可绑定并返回错误摘要。
    """
    try:
        addr_infos = socket.getaddrinfo(
            host,
            port,
            type=socket.SOCK_STREAM,
            proto=socket.IPPROTO_TCP,
        )
    except OSError as exc:
        return False, str(exc)

    first_error: Optional[str] = None
    seen: set[tuple[int, int, int, tuple]] = set()
    for family, socktype, proto, _canonname, sockaddr in addr_infos:
        key = (family, socktype, proto, sockaddr)
        if key in seen:
            continue
        seen.add(key)
        try:
            with socket.socket(family, socktype, proto) as sock:
                sock.bind(sockaddr)
            return True, ""
        except OSError as exc:
            if first_error is None:
                first_error = str(exc)

    return False, first_error or "no bindable address candidates"


def _find_available_port(host: str, preferred_port: int) -> Optional[int]:
    """
    作用：在配置端口不可用时，查找可绑定的替代端口。
    用途：提升启动鲁棒性，避免端口冲突导致 MCP 直接不可用。
    使用场景：仅供 HTTP/SSE 传输启动前置校验调用。
    参数与返回：
    - 参数 `host`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `preferred_port`（`int`）：数值控制参数，用于限制范围、数量或时限。
    - 返回：返回 `Optional[int]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回首个可用端口；若未找到则返回 `None`。
    """
    candidates: list[int] = []
    for port in range(preferred_port + 1, min(preferred_port + 101, 65536)):
        candidates.append(port)
    for port in (18000, 19000, 20000, 29000):
        if 1 <= port <= 65535 and port not in candidates and port != preferred_port:
            candidates.append(port)
    for port in candidates:
        ok, _detail = _is_tcp_bind_available(host, port)
        if ok:
            return port
    return None
