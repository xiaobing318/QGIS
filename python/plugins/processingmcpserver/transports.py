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
    """MCP 传输层基类，统一线程生命周期与日志行为。"""

    def __init__(self, mcp: "FastMCP", config: ProcessingMCPServerConfig) -> None:
        """保存 MCP 实例与配置，并初始化后台线程句柄。"""
        self._mcp = mcp
        self._config = config
        self._thread: Optional[threading.Thread] = None

    def start(self) -> bool:
        """启动传输线程，副作用是创建并运行后台服务线程。"""
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
        """请求停止并等待线程退出，副作用是触发服务关闭。"""
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
        """返回传输线程是否仍在运行。"""
        return self._thread is not None and self._thread.is_alive()

    @property
    def thread_name(self) -> str:
        """返回默认线程名，供子类覆盖。"""
        return "ProcessingMCPTransport"

    def describe(self) -> str:
        """返回传输描述文本，供日志和 UI 展示。"""
        return "MCP transport"

    def _run(self) -> None:
        """执行实际传输逻辑，必须由子类实现。"""
        raise NotImplementedError

    def _run_with_logging(self) -> None:
        """包装执行 `_run` 并统一异常日志。"""
        try:
            self._ensure_stdio_streams()
            self._run()
        except Exception:
            self._log_exception(
                f"Processing MCP transport crashed ({self._context_details()})"
            )

    def _ensure_stdio_streams(self) -> None:
        """在需要时补齐 `stdout/stderr`，避免空流导致服务异常。"""
        if sys.stdout is None and sys.__stdout__ is not None:
            sys.stdout = sys.__stdout__
        if sys.stderr is None and sys.__stderr__ is not None:
            sys.stderr = sys.__stderr__

    def _request_stop(self) -> None:
        """发起停止请求，子类按协议实现具体行为。"""
        return

    def _context_details(self) -> str:
        """汇总 host/port/path 等上下文用于启停日志。"""
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
        """写入 QGIS 消息日志。"""
        QgsMessageLog.logMessage(message, MCP_LOG_CATEGORY, level)

    def _log_exception(self, message: str, level=Qgis.Critical) -> None:
        """附带堆栈信息记录异常日志。"""
        details = traceback.format_exc()
        self._log(f"{message}\n{details}", level)

    def _apply_cors(self, app):
        """按配置为 ASGI 应用注入 CORS 中间件。"""
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
    """基于 uvicorn 的 HTTP/SSE 传输层实现。"""

    def __init__(self, mcp: "FastMCP", config: ProcessingMCPServerConfig) -> None:
        """初始化 uvicorn 服务器句柄。"""
        super().__init__(mcp, config)
        self._uvicorn_server = None

    def _run(self) -> None:
        """构建 ASGI 应用并启动 uvicorn 服务循环。"""
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
        """通知 uvicorn 服务退出主循环。"""
        if self._uvicorn_server:
            self._uvicorn_server.should_exit = True

    def _build_app(self):
        """返回 ASGI 应用对象，需由子类实现。"""
        raise NotImplementedError

    def _build_log_config(self) -> dict[str, object]:
        """构建 uvicorn 到 QGIS 日志桥接配置。"""
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
        """创建 uvicorn 配置，失败时自动降级为默认日志配置。"""
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
    """基于 Streamable HTTP 的 MCP 传输实现。"""

    @property
    def thread_name(self) -> str:
        """返回 Streamable HTTP 线程名。"""
        return "ProcessingMCPStreamableHTTP"

    def describe(self) -> str:
        """返回 Streamable HTTP 访问地址描述。"""
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        path = self._mcp.settings.streamable_http_path
        return f"Streamable HTTP on http://{host}:{port}{path}"

    def _build_app(self):
        """构建 Streamable HTTP ASGI 应用并应用 CORS。"""
        return self._apply_cors(self._mcp.streamable_http_app())


class SseTransport(UvicornTransport):
    """基于 SSE 的 MCP 传输实现。"""

    @property
    def thread_name(self) -> str:
        """返回 SSE 线程名。"""
        return "ProcessingMCPSSE"

    def describe(self) -> str:
        """返回 SSE 访问地址与消息路径描述。"""
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        sse_path = self._mcp.settings.sse_path
        message_path = self._mcp.settings.message_path
        return f"SSE on http://{host}:{port}{sse_path} (messages {message_path})"

    def _build_app(self):
        """构建 SSE ASGI 应用并应用 CORS。"""
        return self._apply_cors(self._mcp.sse_app(self._config.mount_path))


class StdioTransport(BaseMcpTransport):
    """基于标准输入输出的 MCP 传输实现。"""

    @property
    def thread_name(self) -> str:
        """返回 STDIO 线程名。"""
        return "ProcessingMCPStdio"

    def describe(self) -> str:
        """返回 STDIO 传输描述文本。"""
        return "STDIO (stdin/stdout)"

    def _run(self) -> None:
        """在 stdin/stdout 可用时运行 MCP stdio 模式。"""
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
        """记录 STDIO 停止限制后调用基类停止流程。"""
        if self.is_running():
            self._log(
                "STDIO transport does not support graceful shutdown; close QGIS or stdin.",
                Qgis.Warning,
            )
        super().stop()


def create_transport(
    mcp: "FastMCP", config: ProcessingMCPServerConfig
) -> BaseMcpTransport:
    """根据配置创建对应传输实现，非法值回退为 Streamable HTTP。"""
    transport = (config.transport or "streamable-http").lower()
    if transport == "stdio":
        return StdioTransport(mcp, config)
    if transport == "sse":
        return SseTransport(mcp, config)
    return StreamableHttpTransport(mcp, config)
