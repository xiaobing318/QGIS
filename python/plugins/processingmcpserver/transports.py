from __future__ import annotations

"""Transport adapters for exposing FastMCP over HTTP, SSE, or stdio."""

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
        """Store the MCP instance and config, and initialize the worker thread handle."""
        self._mcp = mcp
        self._config = config
        self._thread: Optional[threading.Thread] = None

    def start(self) -> bool:
        """Start the transport thread and launch the background service thread."""
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
        """Request shutdown and wait for the worker thread to exit."""
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
        """Return whether the transport thread is still running."""
        return self._thread is not None and self._thread.is_alive()

    @property
    def thread_name(self) -> str:
        """Return the default thread name for subclasses to override."""
        return "ProcessingMCPTransport"

    def describe(self) -> str:
        """Return a transport description for logs and UI display."""
        return "MCP transport"

    def _run(self) -> None:
        """Run the transport implementation; subclasses must provide it."""
        raise NotImplementedError

    def _run_with_logging(self) -> None:
        """Wrap `_run` and centralize exception logging."""
        try:
            self._ensure_stdio_streams()
            self._run()
        except Exception:
            self._log_exception(
                f"Processing MCP transport crashed ({self._context_details()})"
            )

    def _ensure_stdio_streams(self) -> None:
        """Fill in `stdout/stderr` when needed to avoid empty-stream failures."""
        if sys.stdout is None and sys.__stdout__ is not None:
            sys.stdout = sys.__stdout__
        if sys.stderr is None and sys.__stderr__ is not None:
            sys.stderr = sys.__stderr__

    def _request_stop(self) -> None:
        """Request shutdown; subclasses implement protocol-specific behavior."""
        return

    def _context_details(self) -> str:
        """Summarize host, port, and path context for lifecycle logging."""
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
        """Write a message to the QGIS message log."""
        QgsMessageLog.logMessage(message, MCP_LOG_CATEGORY, level)

    def _log_exception(self, message: str, level=Qgis.Critical) -> None:
        """Log an exception message together with its traceback."""
        details = traceback.format_exc()
        self._log(f"{message}\n{details}", level)

    def _apply_cors(self, app):
        """Inject CORS middleware into an ASGI app when configured."""
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
        """Initialize the uvicorn server handle."""
        super().__init__(mcp, config)
        self._uvicorn_server = None

    def _run(self) -> None:
        """Build the ASGI app and start the uvicorn server loop."""
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
        """Tell uvicorn to exit its main loop."""
        if self._uvicorn_server:
            self._uvicorn_server.should_exit = True

    def _build_app(self):
        """Return the ASGI application object; subclasses must implement this."""
        raise NotImplementedError

    def _build_log_config(self) -> dict[str, object]:
        """Build the log bridge configuration from uvicorn to QGIS."""
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
        """Create the uvicorn config and fall back to default logging on failure."""
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
        """Return the Streamable HTTP thread name."""
        return "ProcessingMCPStreamableHTTP"

    def describe(self) -> str:
        """Return the Streamable HTTP access description."""
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        path = self._mcp.settings.streamable_http_path
        return f"Streamable HTTP on http://{host}:{port}{path}"

    def _build_app(self):
        """Build the Streamable HTTP ASGI app and apply CORS."""
        return self._apply_cors(self._mcp.streamable_http_app())


class SseTransport(UvicornTransport):
    """MCP transport implementation based on SSE."""

    @property
    def thread_name(self) -> str:
        """Return the SSE thread name."""
        return "ProcessingMCPSSE"

    def describe(self) -> str:
        """Return the SSE endpoint and message-path description."""
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        sse_path = self._mcp.settings.sse_path
        message_path = self._mcp.settings.message_path
        return f"SSE on http://{host}:{port}{sse_path} (messages {message_path})"

    def _build_app(self):
        """Build the SSE ASGI app and apply CORS."""
        return self._apply_cors(self._mcp.sse_app(self._config.mount_path))


class StdioTransport(BaseMcpTransport):
    """MCP transport implementation based on stdin/stdout."""

    @property
    def thread_name(self) -> str:
        """Return the STDIO thread name."""
        return "ProcessingMCPStdio"

    def describe(self) -> str:
        """Return the STDIO transport description."""
        return "STDIO (stdin/stdout)"

    def _run(self) -> None:
        """Run the MCP stdio mode when stdin/stdout are available."""
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
        """Log STDIO shutdown limitations and then call the base stop flow."""
        if self.is_running():
            self._log(
                "STDIO transport does not support graceful shutdown; close QGIS or stdin.",
                Qgis.Warning,
            )
        super().stop()


def create_transport(
    mcp: "FastMCP", config: ProcessingMCPServerConfig
) -> BaseMcpTransport:
    """Create the configured transport implementation, defaulting to Streamable HTTP."""
    transport = (config.transport or "streamable-http").lower()
    if transport == "stdio":
        return StdioTransport(mcp, config)
    if transport == "sse":
        return SseTransport(mcp, config)
    return StreamableHttpTransport(mcp, config)
