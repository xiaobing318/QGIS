from __future__ import annotations

import asyncio
import sys
import threading
import traceback
from typing import Optional

from mcp.server.fastmcp import FastMCP
from qgis.core import Qgis, QgsMessageLog

from processing.mcpserver.config import McpServerConfig


class BaseMcpTransport:
    def __init__(self, mcp: FastMCP, config: McpServerConfig) -> None:
        self._mcp = mcp
        self._config = config
        self._thread: Optional[threading.Thread] = None

    def start(self) -> bool:
        if self.is_running():
            return True
        self._thread = threading.Thread(
            target=self._run_with_logging, name=self.thread_name, daemon=True
        )
        self._thread.start()
        return True

    def stop(self) -> None:
        self._request_stop()
        if not self._thread:
            return
        self._thread.join(timeout=2)
        if not self._thread.is_alive():
            self._thread = None

    def is_running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    @property
    def thread_name(self) -> str:
        return "ProcessingMCPTransport"

    def describe(self) -> str:
        return "MCP transport"

    def _run(self) -> None:
        raise NotImplementedError

    def _run_with_logging(self) -> None:
        try:
            self._ensure_stdio_streams()
            self._run()
        except Exception:
            self._log_exception("Processing MCP transport crashed")

    def _ensure_stdio_streams(self) -> None:
        if sys.stdout is None and sys.__stdout__ is not None:
            sys.stdout = sys.__stdout__
        if sys.stderr is None and sys.__stderr__ is not None:
            sys.stderr = sys.__stderr__

    def _request_stop(self) -> None:
        return

    def _log(self, message: str, level=Qgis.Info) -> None:
        QgsMessageLog.logMessage(message, "Processing MCP", level)

    def _log_exception(self, message: str, level=Qgis.Critical) -> None:
        details = traceback.format_exc()
        self._log(f"{message}\n{details}", level)

    def _apply_cors(self, app):
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
    def __init__(self, mcp: FastMCP, config: McpServerConfig) -> None:
        super().__init__(mcp, config)
        self._uvicorn_server = None

    def _run(self) -> None:
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
        if self._uvicorn_server:
            self._uvicorn_server.should_exit = True

    def _build_app(self):
        raise NotImplementedError

    def _build_log_config(self) -> dict[str, object]:
        log_level = self._mcp.settings.log_level
        return {
            "version": 1,
            "disable_existing_loggers": False,
            "formatters": {
                "default": {
                    "format": "%(name)s: %(message)s",
                }
            },
            "handlers": {
                "qgis": {
                    "class": "processing.mcp.log_handler.QgisLogHandler",
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
    @property
    def thread_name(self) -> str:
        return "ProcessingMCPStreamableHTTP"

    def describe(self) -> str:
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        path = self._mcp.settings.streamable_http_path
        return f"Streamable HTTP on http://{host}:{port}{path}"

    def _build_app(self):
        return self._apply_cors(self._mcp.streamable_http_app())


class SseTransport(UvicornTransport):
    @property
    def thread_name(self) -> str:
        return "ProcessingMCPSSE"

    def describe(self) -> str:
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        sse_path = self._mcp.settings.sse_path
        message_path = self._mcp.settings.message_path
        return f"SSE on http://{host}:{port}{sse_path} (messages {message_path})"

    def _build_app(self):
        return self._apply_cors(self._mcp.sse_app(self._config.mount_path))


class StdioTransport(BaseMcpTransport):
    @property
    def thread_name(self) -> str:
        return "ProcessingMCPStdio"

    def describe(self) -> str:
        return "STDIO (stdin/stdout)"

    def _run(self) -> None:
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
        if self.is_running():
            self._log(
                "STDIO transport does not support graceful shutdown; close QGIS or stdin.",
                Qgis.Warning,
            )
        super().stop()


def create_transport(mcp: FastMCP, config: McpServerConfig) -> BaseMcpTransport:
    transport = (config.transport or "streamable-http").lower()
    if transport == "stdio":
        return StdioTransport(mcp, config)
    if transport == "sse":
        return SseTransport(mcp, config)
    return StreamableHttpTransport(mcp, config)
