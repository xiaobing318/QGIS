from __future__ import annotations

import asyncio
import sys
import threading
import traceback
from typing import Optional

from mcp.server.fastmcp import FastMCP
from qgis.core import Qgis, QgsMessageLog

from processing.processingmcpserver.config import ProcessingMCPServerConfig


# 在后台线程中运行 MCP 服务逻辑的基础传输类。
class BaseMcpTransport:
    def __init__(self, mcp: FastMCP, config: ProcessingMCPServerConfig) -> None:
        # 保存服务器实例与配置。
        self._mcp = mcp
        self._config = config
        self._thread: Optional[threading.Thread] = None

    def start(self) -> bool:
        # 未运行时启动传输线程。
        if self.is_running():
            return True
        self._thread = threading.Thread(
            target=self._run_with_logging, name=self.thread_name, daemon=True
        )
        self._thread.start()
        return True

    def stop(self) -> None:
        # 请求停止并等待线程退出。
        self._request_stop()
        if not self._thread:
            return
        self._thread.join(timeout=2)
        if not self._thread.is_alive():
            self._thread = None

    def is_running(self) -> bool:
        # 判断传输线程是否存活。
        return self._thread is not None and self._thread.is_alive()

    @property
    def thread_name(self) -> str:
        # 子类可覆写以提供更明确的线程名。
        return "ProcessingMCPTransport"

    def describe(self) -> str:
        # 面向日志的传输描述信息。
        return "MCP transport"

    def _run(self) -> None:
        # 子类在此实现运行循环。
        raise NotImplementedError

    def _run_with_logging(self) -> None:
        # 包装 _run，修复流并记录异常。
        try:
            self._ensure_stdio_streams()
            self._run()
        except Exception:
            self._log_exception("Processing MCP transport crashed")

    def _ensure_stdio_streams(self) -> None:
        # 若 stdout/stderr 被清空，则恢复。
        if sys.stdout is None and sys.__stdout__ is not None:
            sys.stdout = sys.__stdout__
        if sys.stderr is None and sys.__stderr__ is not None:
            sys.stderr = sys.__stderr__

    def _request_stop(self) -> None:
        # 子类可覆写实现优雅停止。
        return

    def _log(self, message: str, level=Qgis.Info) -> None:
        # 将消息写入 QGIS 日志。
        QgsMessageLog.logMessage(message, "Processing MCP", level)

    def _log_exception(self, message: str, level=Qgis.Critical) -> None:
        # 追加回溯信息便于诊断。
        details = traceback.format_exc()
        self._log(f"{message}\n{details}", level)

    def _apply_cors(self, app):
        # 根据配置为 ASGI 应用添加 CORS。
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


# 基于 uvicorn 的 HTTP 传输基类。
class UvicornTransport(BaseMcpTransport):
    def __init__(self, mcp: FastMCP, config: ProcessingMCPServerConfig) -> None:
        super().__init__(mcp, config)
        # 保存正在运行的 uvicorn 服务引用。
        self._uvicorn_server = None

    def _run(self) -> None:
        # 导入 uvicorn 并启动 ASGI 服务。
        try:
            import uvicorn
        except Exception as exc:
            self._log(f"Failed to import uvicorn: {exc}", Qgis.Critical)
            return

        try:
            # 构建当前传输所需的 ASGI 应用。
            app = self._build_app()
        except Exception:
            self._log_exception("Failed to build MCP ASGI app")
            return

        config = self._create_uvicorn_config(uvicorn, app)
        if config is None:
            return
        self._uvicorn_server = uvicorn.Server(config)

        try:
            # 在当前线程运行服务循环。
            asyncio.run(self._uvicorn_server.serve())
        except Exception:
            self._log_exception("Processing MCP server stopped with error")
        finally:
            self._uvicorn_server = None

    def _request_stop(self) -> None:
        # 请求 uvicorn 进行优雅退出。
        if self._uvicorn_server:
            self._uvicorn_server.should_exit = True

    def _build_app(self):
        # 子类提供具体的 ASGI 应用。
        raise NotImplementedError

    def _build_log_config(self) -> dict[str, object]:
        # 构建日志配置，将 uvicorn 日志转发到 QGIS。
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
        # 创建 uvicorn 配置，并可选使用自定义日志。
        log_config = None
        try:
            log_config = self._build_log_config()
        except Exception:
            self._log_exception("Failed to build uvicorn logging configuration")

        try:
            # 优先尝试使用自定义日志配置。
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
            # 自定义配置失败时，改用默认日志配置重试。
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


# MCP 的 Streamable HTTP 传输。
class StreamableHttpTransport(UvicornTransport):
    @property
    def thread_name(self) -> str:
        # 供日志识别的线程名。
        return "ProcessingMCPStreamableHTTP"

    def describe(self) -> str:
        # 返回包含端点信息的描述。
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        path = self._mcp.settings.streamable_http_path
        return f"Streamable HTTP on http://{host}:{port}{path}"

    def _build_app(self):
        # 构建带 CORS 的 Streamable HTTP ASGI 应用。
        return self._apply_cors(self._mcp.streamable_http_app())


# MCP 的 SSE（服务端事件）传输。
class SseTransport(UvicornTransport):
    @property
    def thread_name(self) -> str:
        # 供日志识别的线程名。
        return "ProcessingMCPSSE"

    def describe(self) -> str:
        # 返回包含端点信息的描述。
        host = self._mcp.settings.host
        port = self._mcp.settings.port
        sse_path = self._mcp.settings.sse_path
        message_path = self._mcp.settings.message_path
        return f"SSE on http://{host}:{port}{sse_path} (messages {message_path})"

    def _build_app(self):
        # 构建带 CORS 的 SSE ASGI 应用。
        return self._apply_cors(self._mcp.sse_app(self._config.mount_path))


# 供 QGIS 以命令行方式运行时使用的 STDIO 传输。
class StdioTransport(BaseMcpTransport):
    @property
    def thread_name(self) -> str:
        # 供日志识别的线程名。
        return "ProcessingMCPStdio"

    def describe(self) -> str:
        # 描述 stdin/stdout 传输方式。
        return "STDIO (stdin/stdout)"

    def _run(self) -> None:
        # 通过 stdin/stdout 运行 MCP 服务。
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
        # STDIO 无法优雅停止，记录告警。
        if self.is_running():
            self._log(
                "STDIO transport does not support graceful shutdown; close QGIS or stdin.",
                Qgis.Warning,
            )
        super().stop()


# 根据配置创建对应的传输实例。
def create_transport(mcp: FastMCP, config: ProcessingMCPServerConfig) -> BaseMcpTransport:
    # 规范化传输名称便于比较。
    transport = (config.transport or "streamable-http").lower()
    if transport == "stdio":
        return StdioTransport(mcp, config)
    if transport == "sse":
        return SseTransport(mcp, config)
    return StreamableHttpTransport(mcp, config)
