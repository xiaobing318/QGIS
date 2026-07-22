"""Small JSON-RPC over HTTP MCP server used by QCopilots services.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import base64
import hashlib
import hmac
import inspect
import ipaddress
import json
import logging
import math
import os
import re
import secrets
import threading
import time
import uuid
from argparse import ArgumentParser, Namespace
from collections import OrderedDict
from collections.abc import Callable, Mapping
from dataclasses import dataclass
from decimal import Decimal
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any

from qcopilots_common.constants import (
    CORS_ORIGINS_ENV,
    DEFAULT_CORS_ORIGINS,
    DEFAULT_HOST,
    DEFAULT_MCP_PATH,
    HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS,
    MAX_HTTP_ERROR_BODY_DRAIN_BYTES,
    MAX_HTTP_REQUEST_BODY_BYTES,
    MCP_PROTOCOL_VERSION,
    SERVICE_DESCRIPTION_ENV,
    SERVICE_ICON_ENV,
    SERVICE_TITLE_ENV,
)
from qcopilots_common.logging import configure_logger


class ToolError(Exception):
    """Raised when a tool call should return a structured MCP tool error."""


@dataclass(frozen=True)
class McpRequestContext:
    client_name: str = ""
    client_version: str = ""


@dataclass(frozen=True)
class _SessionContextEntry:
    context: McpRequestContext
    last_used: float


@dataclass(frozen=True)
class McpTool:
    name: str
    description: str
    input_schema: Mapping[str, Any]
    handler: Callable[..., Any]

    def descriptor(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "description": self.description,
            "inputSchema": dict(self.input_schema),
        }


@dataclass(frozen=True)
class McpResource:
    uri: str
    name: str
    mime_type: str | None
    handler: Callable[[], str | bytes]

    def descriptor(self) -> dict[str, Any]:
        descriptor = {
            "uri": self.uri,
            "name": self.name,
        }
        if self.mime_type:
            descriptor["mimeType"] = self.mime_type
        return descriptor


@dataclass(frozen=True)
class McpPrompt:
    name: str
    description: str
    arguments: list[Mapping[str, Any]]
    handler: Callable[..., Any]
    title: str = ""

    def descriptor(self) -> dict[str, Any]:
        descriptor = {
            "name": self.name,
            "description": self.description,
            "arguments": [dict(argument) for argument in self.arguments],
        }
        if self.title:
            descriptor["title"] = self.title
        return descriptor


McpToolSource = list[McpTool] | Callable[[], list[McpTool]]
McpResourceSource = list[McpResource] | Callable[[], list[McpResource]]
McpPromptSource = list[McpPrompt] | Callable[[], list[McpPrompt]]
MAX_JSON_RPC_BATCH_ITEMS = 64
MAX_MCP_CLIENT_INFO_FIELD_LENGTH = 256
# Disabled by default because llama-ui currently discovers tools and prompts
# with one list call and does not follow nextCursor for those methods.
DEFAULT_MCP_LIST_PAGE_SIZE = 0
DEFAULT_MAX_SESSION_CONTEXTS = 256
DEFAULT_SESSION_TTL_SECONDS = 3600.0


class McpJsonRpcServer:
    """In-memory JSON-RPC MCP dispatcher shared by tests and HTTP transport."""

    def __init__(
        self,
        server_name: str,
        server_version: str,
        tools: McpToolSource,
        resources: McpResourceSource | None = None,
        prompts: McpPromptSource | None = None,
        logger: logging.Logger | None = None,
        server_title: str = "",
        server_description: str = "",
        server_icons: list[dict[str, Any]] | None = None,
        max_session_contexts: int = DEFAULT_MAX_SESSION_CONTEXTS,
        session_ttl_seconds: float = DEFAULT_SESSION_TTL_SECONDS,
        clock: Callable[[], float] = time.monotonic,
        list_page_size: int = DEFAULT_MCP_LIST_PAGE_SIZE,
    ):
        self.server_name = server_name
        self.server_version = server_version
        self._tools_source = tools
        self._resources_source = resources
        self._prompts_source = prompts
        self.logger = logger or logging.getLogger(server_name)
        self._context_lock = threading.RLock()
        self._max_session_contexts = _validate_nonnegative_integer(
            max_session_contexts,
            "max_session_contexts",
        )
        self._session_ttl_seconds = _validate_nonnegative_finite_number(
            session_ttl_seconds,
            "session_ttl_seconds",
        )
        self._list_page_size = _validate_nonnegative_integer(
            list_page_size,
            "list_page_size",
        )
        self._clock = _validate_clock(clock)
        _clock_value(self._clock)
        self._contexts: OrderedDict[str, _SessionContextEntry] = OrderedDict()
        self._pagination_secret = secrets.token_bytes(32)
        self._tool_names_cache = tuple(sorted(self._tools_by_name()))
        self.server_title = server_title
        self.server_description = server_description
        self.server_icons = server_icons or []

    @property
    def tools(self) -> dict[str, McpTool]:
        return self._tools_by_name()

    @property
    def resources(self) -> dict[str, McpResource]:
        return self._resources_by_uri()

    @property
    def prompts(self) -> dict[str, McpPrompt]:
        return self._prompts_by_name()

    def tool_names_snapshot(self) -> list[str]:
        return list(self._tool_names_cache)

    def _tools_by_name(self) -> dict[str, McpTool]:
        tools = {tool.name: tool for tool in _resolve_tools(self._tools_source)}
        self._tool_names_cache = tuple(sorted(tools))
        return tools

    def _resources_by_uri(self) -> dict[str, McpResource]:
        if self._resources_source is None:
            return {}
        return {
            resource.uri: resource
            for resource in _resolve_resources(self._resources_source)
        }

    def _prompts_by_name(self) -> dict[str, McpPrompt]:
        if self._prompts_source is None:
            return {}
        return {
            prompt.name: prompt
            for prompt in _resolve_prompts(self._prompts_source)
        }

    def handle_json_rpc(
        self,
        request: Any,
        session_id: str | None = None,
    ) -> dict[str, Any] | None:
        if not isinstance(request, dict) or request.get("jsonrpc") != "2.0":
            return _jsonrpc_error(
                _request_id(request),
                -32600,
                "Invalid JSON-RPC request",
            )

        request_id = request.get("id")
        method = request.get("method")
        is_notification = request_id is None

        try:
            raw_params = request.get("params", {})
            if raw_params is None:
                raw_params = {}
            if not isinstance(raw_params, Mapping):
                raise ToolError("JSON-RPC params must be an object")
            result = self._dispatch(method, raw_params, session_id)
        except ToolError as err:
            if is_notification:
                return None
            return _jsonrpc_error(request_id, -32602, str(err))
        except NotImplementedError as err:
            if is_notification:
                return None
            return _jsonrpc_error(request_id, -32601, str(err))
        except Exception as err:
            self.logger.exception("MCP request failed: %s", method)
            if is_notification:
                return None
            return _jsonrpc_error(request_id, -32000, str(err))

        if is_notification:
            return None
        return {"jsonrpc": "2.0", "id": request_id, "result": result}

    def _dispatch(
        self,
        method: str,
        params: Mapping[str, Any],
        session_id: str | None,
    ) -> Any:
        if method == "initialize":
            client_info = _validated_client_info(params)
            self._set_context(
                session_id,
                McpRequestContext(
                    client_name=_client_info_field(client_info, "name"),
                    client_version=_client_info_field(client_info, "version"),
                ),
            )
            server_info = {"name": self.server_name, "version": self.server_version}
            if self.server_title:
                server_info["title"] = self.server_title
            if self.server_description:
                server_info["description"] = self.server_description
            if self.server_icons:
                server_info["icons"] = self.server_icons

            return {
                "protocolVersion": MCP_PROTOCOL_VERSION,
                "capabilities": self._capabilities(),
                "serverInfo": server_info,
            }

        if method == "notifications/initialized":
            return None

        if method == "tools/list":
            tools = self._tools_by_name()
            return _paginated_list_result(
                [tool.descriptor() for tool in tools.values()],
                params,
                "tools",
                self._list_page_size,
                self._pagination_secret,
            )

        if method == "tools/call":
            tool_name = params.get("name")
            tools = self._tools_by_name()
            tool = tools.get(tool_name)
            if tool is None:
                raise ToolError(f"Unknown tool: {tool_name}")
            arguments = params.get("arguments", {})
            _validate_tool_arguments(arguments, tool.input_schema)
            try:
                return _tool_result(
                    _call_tool_handler(
                        tool,
                        arguments,
                        self._context_for_session(session_id),
                    )
                )
            except ToolError as err:
                return {
                    "content": [{"type": "text", "text": str(err)}],
                    "isError": True,
                }

        if method == "resources/list":
            resources = self._resources_by_uri()
            return _paginated_list_result(
                [
                    resource.descriptor()
                    for resource in resources.values()
                ],
                params,
                "resources",
                self._list_page_size,
                self._pagination_secret,
            )

        if method == "resources/read":
            uri = params.get("uri")
            resources = self._resources_by_uri()
            if not isinstance(uri, str) or uri not in resources:
                raise ToolError(f"Unknown resource: {uri}")
            return _resource_result(resources[uri])

        if method == "resources/templates/list":
            return _paginated_list_result(
                [],
                params,
                "resourceTemplates",
                self._list_page_size,
                self._pagination_secret,
            )

        if method == "prompts/list":
            prompts = self._prompts_by_name()
            return _paginated_list_result(
                [prompt.descriptor() for prompt in prompts.values()],
                params,
                "prompts",
                self._list_page_size,
                self._pagination_secret,
            )

        if method == "prompts/get":
            prompt_name = params.get("name")
            arguments = params.get("arguments", {})
            if not isinstance(arguments, Mapping):
                raise ToolError("Prompt arguments must be an object")
            prompts = self._prompts_by_name()
            prompt = prompts.get(prompt_name)
            if prompt is None:
                raise ToolError(f"Unknown prompt: {prompt_name}")
            return _prompt_result(
                _call_prompt_handler(
                    prompt,
                    arguments,
                    self._context_for_session(session_id),
                )
            )

        raise NotImplementedError(f"Unsupported method: {method}")

    def _set_context(
        self,
        session_id: str | None,
        context: McpRequestContext,
    ) -> None:
        now = _clock_value(self._clock)
        with self._context_lock:
            self._purge_expired_contexts(now)
            if not session_id or self._max_session_contexts == 0:
                return
            self._contexts.pop(session_id, None)
            while len(self._contexts) >= self._max_session_contexts:
                self._contexts.popitem(last=False)
            self._contexts[session_id] = _SessionContextEntry(context, now)

    def _context_for_session(self, session_id: str | None) -> McpRequestContext:
        if not session_id:
            return McpRequestContext()
        now = _clock_value(self._clock)
        with self._context_lock:
            self._purge_expired_contexts(now)
            entry = self._contexts.pop(session_id, None)
            if entry is None:
                return McpRequestContext()
            self._contexts[session_id] = _SessionContextEntry(entry.context, now)
            return entry.context

    def _purge_expired_contexts(self, now: float) -> None:
        expired = [
            session_id
            for session_id, entry in self._contexts.items()
            if now - entry.last_used >= self._session_ttl_seconds
        ]
        for session_id in expired:
            self._contexts.pop(session_id, None)

    def clear_session(self, session_id: str | None) -> None:
        if not session_id:
            return
        with self._context_lock:
            self._contexts.pop(session_id, None)

    def clear_all_sessions(self) -> None:
        with self._context_lock:
            self._contexts.clear()

    def _capabilities(self) -> dict[str, Any]:
        capabilities = {"tools": {}}
        if self._resources_source is not None:
            capabilities["resources"] = {}
        if self._prompts_source is not None:
            capabilities["prompts"] = {}
        return capabilities


class McpHttpServer:
    def __init__(
        self,
        name: str,
        version: str,
        tools: McpToolSource,
        resources: McpResourceSource | None = None,
        prompts: McpPromptSource | None = None,
        host: str = DEFAULT_HOST,
        port: int = 0,
        path: str = DEFAULT_MCP_PATH,
        cors_origins: list[str] | None = None,
        logger: logging.Logger | None = None,
        server_title: str = "",
        server_description: str = "",
        server_icons: list[dict[str, Any]] | None = None,
        max_session_contexts: int = DEFAULT_MAX_SESSION_CONTEXTS,
        session_ttl_seconds: float = DEFAULT_SESSION_TTL_SECONDS,
        clock: Callable[[], float] = time.monotonic,
        list_page_size: int = DEFAULT_MCP_LIST_PAGE_SIZE,
    ):
        self.name = name
        self.version = version
        self._tools_source = tools
        self._resources_source = resources
        self._prompts_source = prompts
        self.host = _service_bind_host(host)
        self.port = port
        self.path = path or DEFAULT_MCP_PATH
        self.cors_origins = cors_origins or _cors_origins_from_env()
        self.logger = logger or logging.getLogger(name)
        self.rpc_server = McpJsonRpcServer(
            name,
            version,
            tools,
            resources=resources,
            prompts=prompts,
            logger=self.logger,
            server_title=server_title,
            server_description=server_description,
            server_icons=server_icons,
            max_session_contexts=max_session_contexts,
            session_ttl_seconds=session_ttl_seconds,
            list_page_size=list_page_size,
            clock=clock,
        )

    @property
    def tools(self) -> dict[str, McpTool]:
        return {tool.name: tool for tool in _resolve_tools(self._tools_source)}

    def create_http_server(self) -> ThreadingHTTPServer:
        controller = self

        class Handler(BaseHTTPRequestHandler):
            server_version = "QCopilotsMCP/0.1"

            def log_message(self, fmt: str, *args: Any) -> None:
                controller.logger.info(fmt, *args)

            def do_OPTIONS(self) -> None:
                self._send_empty(HTTPStatus.NO_CONTENT)

            def do_DELETE(self) -> None:
                if self.path != controller.path:
                    self._send_json(HTTPStatus.NOT_FOUND, {"error": "not_found"})
                    return
                controller.rpc_server.clear_session(self.headers.get("mcp-session-id"))
                self._send_empty(HTTPStatus.NO_CONTENT)

            def do_GET(self) -> None:
                if self.path.rstrip("/") == "/health":
                    self._send_json(
                        HTTPStatus.OK,
                        {
                            "status": "ok",
                            "server": controller.name,
                            "tools": controller.rpc_server.tool_names_snapshot(),
                        },
                    )
                    return
                self._send_json(HTTPStatus.NOT_FOUND, {"error": "not_found"})

            def do_POST(self) -> None:
                if self.path != controller.path:
                    self._discard_request_body()
                    self._send_json(HTTPStatus.NOT_FOUND, {"error": "not_found"})
                    return
                if self.headers.get_content_type().lower() != "application/json":
                    self._discard_request_body()
                    self._send_json(
                        HTTPStatus.UNSUPPORTED_MEDIA_TYPE,
                        {"error": "unsupported_media_type"},
                    )
                    return
                try:
                    length = self._request_body_length()
                    if length < 0:
                        self._send_json(
                            HTTPStatus.BAD_REQUEST,
                            _jsonrpc_error(None, -32700, "bad_content_length"),
                        )
                        return
                    if length > MAX_HTTP_REQUEST_BODY_BYTES:
                        self.close_connection = True
                        self._send_json(
                            HTTPStatus.REQUEST_ENTITY_TOO_LARGE,
                            {"error": "request_too_large"},
                        )
                        return
                    payload = json.loads(self.rfile.read(length).decode("utf-8"))
                except Exception as err:
                    self._send_json(
                        HTTPStatus.BAD_REQUEST,
                        _jsonrpc_error(None, -32700, str(err)),
                    )
                    return

                if isinstance(payload, list):
                    if not 1 <= len(payload) <= MAX_JSON_RPC_BATCH_ITEMS:
                        self._send_json(
                            HTTPStatus.OK,
                            _jsonrpc_error(
                                None,
                                -32600,
                                "Invalid JSON-RPC batch: expected "
                                f"1-{MAX_JSON_RPC_BATCH_ITEMS} requests",
                            ),
                        )
                        return
                    session_id = self._request_session_id(payload)
                    responses = [
                        controller.rpc_server.handle_json_rpc(
                            item,
                            session_id=session_id,
                        )
                        for item in payload
                    ]
                    responses = [item for item in responses if item is not None]
                    if not responses:
                        self._send_empty(
                            HTTPStatus.ACCEPTED,
                            self._session_headers(session_id),
                        )
                    else:
                        self._send_json(
                            HTTPStatus.OK,
                            responses,
                            self._session_headers(session_id),
                        )
                    return

                session_id = self._request_session_id(payload)
                response = controller.rpc_server.handle_json_rpc(
                    payload,
                    session_id=session_id,
                )
                if response is None:
                    self._send_empty(
                        HTTPStatus.ACCEPTED,
                        self._session_headers(session_id),
                    )
                else:
                    self._send_json(
                        HTTPStatus.OK,
                        response,
                        self._session_headers(session_id),
                    )

            def _send_empty(
                self,
                status: HTTPStatus,
                extra_headers: Mapping[str, str] | None = None,
            ) -> None:
                self.send_response(status)
                self._send_cors_headers()
                for name, value in (extra_headers or {}).items():
                    self.send_header(name, value)
                if not (
                    100 <= int(status) < 200
                    or status in (HTTPStatus.NO_CONTENT, HTTPStatus.NOT_MODIFIED)
                ):
                    self.send_header("Content-Length", "0")
                if self.close_connection:
                    self.send_header("Connection", "close")
                self.end_headers()

            def _send_json(
                self,
                status: HTTPStatus,
                body: Any,
                extra_headers: Mapping[str, str] | None = None,
            ) -> None:
                data = json.dumps(
                    body,
                    ensure_ascii=False,
                    allow_nan=False,
                ).encode("utf-8")
                self.send_response(status)
                self._send_cors_headers()
                for name, value in (extra_headers or {}).items():
                    self.send_header(name, value)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                if self.close_connection:
                    self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(data)

            def _request_session_id(self, payload: Any) -> str | None:
                session_id = self.headers.get("mcp-session-id")
                if session_id:
                    return session_id
                if self._payload_contains_initialize(payload):
                    return uuid.uuid4().hex
                return None

            def _session_headers(self, session_id: str | None) -> dict[str, str]:
                if not session_id:
                    return {}
                return {"mcp-session-id": session_id}

            def _payload_contains_initialize(self, payload: Any) -> bool:
                if isinstance(payload, list):
                    return any(
                        self._payload_contains_initialize(item) for item in payload
                    )
                return (
                    isinstance(payload, dict)
                    and payload.get("method") == "initialize"
                )

            def _discard_request_body(self) -> None:
                length = self._request_body_length()
                if length <= 0:
                    return
                if length > MAX_HTTP_ERROR_BODY_DRAIN_BYTES:
                    self.close_connection = True
                    return
                old_timeout = self.connection.gettimeout()
                try:
                    self.connection.settimeout(HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS)
                    remaining = length
                    while remaining > 0:
                        chunk = self.rfile.read(min(remaining, 8192))
                        if not chunk:
                            self.close_connection = True
                            break
                        remaining -= len(chunk)
                except OSError:
                    self.close_connection = True
                finally:
                    try:
                        self.connection.settimeout(old_timeout)
                    except OSError:
                        self.close_connection = True

            def _request_body_length(self) -> int:
                try:
                    return max(0, int(self.headers.get("Content-Length", "0")))
                except ValueError:
                    self.close_connection = True
                    return -1

            def _send_cors_headers(self) -> None:
                origin = self.headers.get("Origin")
                if "*" in controller.cors_origins:
                    allowed_origin = "*"
                elif origin in controller.cors_origins:
                    allowed_origin = origin
                else:
                    allowed_origin = (
                        controller.cors_origins[0]
                        if controller.cors_origins
                        else DEFAULT_CORS_ORIGINS[0]
                    )
                self.send_header("Access-Control-Allow-Origin", allowed_origin)
                self.send_header(
                    "Access-Control-Allow-Headers",
                    "content-type, mcp-protocol-version, mcp-session-id",
                )
                self.send_header(
                    "Access-Control-Allow-Methods",
                    "GET, POST, DELETE, OPTIONS",
                )
                self.send_header("Access-Control-Expose-Headers", "mcp-session-id")
                self.send_header("Access-Control-Allow-Private-Network", "true")

        return ThreadingHTTPServer((self.host, self.port), Handler)

    def serve_forever(self) -> None:
        httpd = self.create_http_server()
        self.port = int(httpd.server_address[1])
        self.logger.info(
            "Serving %s on http://%s:%s%s",
            self.name,
            self.host,
            self.port,
            self.path,
        )
        try:
            httpd.serve_forever()
        finally:
            try:
                httpd.server_close()
            finally:
                self.rpc_server.clear_all_sessions()


def parse_server_args(default_port: int, description: str) -> Namespace:
    parser = ArgumentParser(description=description)
    parser.add_argument(
        "--host",
        default=os.environ.get("QCOPILOTS_SERVICE_HOST", DEFAULT_HOST),
        type=_service_bind_host,
    )
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.environ.get("QCOPILOTS_SERVICE_PORT", default_port)),
    )
    parser.add_argument(
        "--path",
        default=os.environ.get("QCOPILOTS_MCP_PATH", DEFAULT_MCP_PATH),
    )
    parser.add_argument("--log-file", default=os.environ.get("QCOPILOTS_LOG_FILE"))
    return parser.parse_args()


def run_mcp_server(
    name: str,
    version: str,
    tools: McpToolSource,
    default_port: int,
    description: str,
    logger: logging.Logger,
    resources: McpResourceSource | None = None,
    prompts: McpPromptSource | None = None,
) -> None:
    args = parse_server_args(default_port, description)
    if args.log_file:
        logger = configure_logger(logger.name, args.log_file)
    server = McpHttpServer(
        name=name,
        version=version,
        tools=tools,
        resources=resources,
        prompts=prompts,
        host=args.host,
        port=args.port,
        path=args.path,
        cors_origins=_cors_origins_from_env(),
        logger=logger,
        server_title=os.environ.get(SERVICE_TITLE_ENV, ""),
        server_description=os.environ.get(SERVICE_DESCRIPTION_ENV, description),
        server_icons=_server_icons_from_env(),
    )
    server.serve_forever()


def _jsonrpc_error(request_id: Any, code: int, message: str) -> dict[str, Any]:
    return {
        "jsonrpc": "2.0",
        "id": request_id,
        "error": {"code": code, "message": message},
    }


def _validated_client_info(params: Mapping[str, Any]) -> Mapping[str, Any]:
    if "clientInfo" not in params:
        return {}
    client_info = params["clientInfo"]
    if not isinstance(client_info, Mapping):
        raise ToolError("clientInfo must be an object")
    return client_info


def _client_info_field(client_info: Mapping[str, Any], field_name: str) -> str:
    if field_name not in client_info:
        return ""
    value = client_info[field_name]
    if not isinstance(value, str):
        raise ToolError(f"clientInfo.{field_name} must be a string")
    if len(value) > MAX_MCP_CLIENT_INFO_FIELD_LENGTH:
        raise ToolError(
            f"clientInfo.{field_name} must not exceed "
            f"{MAX_MCP_CLIENT_INFO_FIELD_LENGTH} characters"
        )
    return value


def _validate_nonnegative_integer(value: Any, name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ValueError(f"{name} must be a non-negative integer")
    return value


def _validate_positive_integer(value: Any, name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ValueError(f"{name} must be a positive integer")
    return value


def _validate_nonnegative_finite_number(value: Any, name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{name} must be a non-negative finite number")
    result = float(value)
    if result < 0 or not math.isfinite(result):
        raise ValueError(f"{name} must be a non-negative finite number")
    return result


def _validate_clock(clock: Any) -> Callable[[], float]:
    if not callable(clock):
        raise ValueError("clock must be callable")
    return clock


def _clock_value(clock: Callable[[], float]) -> float:
    value = clock()
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("clock must return a finite number")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError("clock must return a finite number")
    return result


def _request_id(request: Any) -> Any:
    return request.get("id") if isinstance(request, dict) else None


def _paginated_list_result(
    items: list[dict[str, Any]],
    params: Mapping[str, Any],
    result_key: str,
    page_size: int,
    secret: bytes,
) -> dict[str, Any]:
    if page_size == 0:
        if "cursor" in params:
            raise ToolError("Invalid pagination cursor")
        return {result_key: items}
    start = _pagination_start_index(items, params, result_key, secret)
    end = min(start + page_size, len(items))
    result: dict[str, Any] = {result_key: items[start:end]}
    if end < len(items):
        result["nextCursor"] = _encode_pagination_cursor(
            items,
            end,
            result_key,
            secret,
        )
    return result


def _pagination_start_index(
    items: list[dict[str, Any]],
    params: Mapping[str, Any],
    result_key: str,
    secret: bytes,
) -> int:
    if "cursor" not in params:
        return 0
    cursor = params["cursor"]
    if not isinstance(cursor, str) or not cursor:
        raise ToolError("Invalid pagination cursor")
    try:
        decoded = base64.urlsafe_b64decode(
            cursor.encode("ascii") + b"=" * (-len(cursor) % 4)
        ).decode("utf-8")
        payload = json.loads(decoded)
    except Exception as exc:
        raise ToolError("Invalid pagination cursor") from exc
    if not isinstance(payload, Mapping) or payload.get("v") != 1:
        raise ToolError("Invalid pagination cursor")
    signature = payload.get("mac")
    unsigned_payload = {
        key: value
        for key, value in payload.items()
        if key != "mac"
    }
    if (
        not isinstance(signature, str)
        or not hmac.compare_digest(
            signature,
            _pagination_mac(unsigned_payload, secret),
        )
    ):
        raise ToolError("Invalid pagination cursor")
    if payload.get("kind") != result_key:
        raise ToolError("Invalid pagination cursor")
    start = payload.get("start")
    digest = payload.get("digest")
    if (
        isinstance(start, bool)
        or not isinstance(start, int)
        or start < 0
        or start > len(items)
        or digest != _pagination_digest(items, result_key)
    ):
        raise ToolError("Invalid pagination cursor")
    return start


def _encode_pagination_cursor(
    items: list[dict[str, Any]],
    start: int,
    result_key: str,
    secret: bytes,
) -> str:
    payload = {
        "v": 1,
        "kind": result_key,
        "start": start,
        "digest": _pagination_digest(items, result_key),
    }
    payload["mac"] = _pagination_mac(payload, secret)
    encoded = base64.urlsafe_b64encode(
        json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    ).decode("ascii")
    return encoded.rstrip("=")


def _pagination_digest(items: list[dict[str, Any]], result_key: str) -> str:
    identities = [_list_item_identity(item) for item in items]
    payload = json.dumps(
        [result_key, identities],
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def _pagination_mac(payload: Mapping[str, Any], secret: bytes) -> str:
    message = json.dumps(
        dict(payload),
        ensure_ascii=False,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")
    return hmac.new(secret, message, hashlib.sha256).hexdigest()


def _list_item_identity(item: Mapping[str, Any]) -> str:
    for key in ("uri", "uriTemplate", "name"):
        value = item.get(key)
        if isinstance(value, str):
            return f"{key}:{value}"
    return json.dumps(item, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


class _SchemaValidationError(ValueError):
    pass


def _validate_tool_arguments(
    arguments: Any,
    schema: Mapping[str, Any],
) -> None:
    if not isinstance(arguments, Mapping):
        raise ToolError("Tool arguments must be an object")
    try:
        _validate_schema_value(arguments, schema, "$")
    except _SchemaValidationError as exc:
        raise ToolError(f"Invalid tool arguments: {exc}") from exc


def _validate_schema_value(value: Any, schema: Any, path: str) -> None:
    if schema is True:
        return
    if schema is False:
        raise _SchemaValidationError(f"{path} is not allowed")
    if not isinstance(schema, Mapping):
        raise _SchemaValidationError(f"{path} uses an invalid schema")

    if "oneOf" in schema:
        alternatives = schema["oneOf"]
        if not isinstance(alternatives, list):
            raise _SchemaValidationError(f"{path} uses an invalid oneOf schema")
        matches = sum(
            _schema_matches(value, alternative, path)
            for alternative in alternatives
        )
        if matches != 1:
            raise _SchemaValidationError(
                f"{path} must match exactly one oneOf alternative"
            )
    if "anyOf" in schema:
        alternatives = schema["anyOf"]
        if not isinstance(alternatives, list):
            raise _SchemaValidationError(f"{path} uses an invalid anyOf schema")
        if not any(
            _schema_matches(value, alternative, path)
            for alternative in alternatives
        ):
            raise _SchemaValidationError(
                f"{path} must match at least one anyOf alternative"
            )

    if "const" in schema and not _json_values_equal(value, schema["const"]):
        raise _SchemaValidationError(f"{path} must equal the declared const value")
    if "enum" in schema:
        choices = schema["enum"]
        if not isinstance(choices, list) or not any(
            _json_values_equal(value, choice) for choice in choices
        ):
            raise _SchemaValidationError(f"{path} is not one of the allowed values")

    expected_type = schema.get("type")
    if expected_type is not None:
        expected_types = (
            expected_type if isinstance(expected_type, list) else [expected_type]
        )
        if not expected_types or not all(
            isinstance(item, str) for item in expected_types
        ):
            raise _SchemaValidationError(f"{path} uses an invalid type schema")
        if not any(_matches_json_type(value, item) for item in expected_types):
            expected = " or ".join(expected_types)
            raise _SchemaValidationError(f"{path} must be {expected}")

    if isinstance(value, Mapping):
        _validate_object_schema(value, schema, path)
    elif isinstance(value, list):
        _validate_array_schema(value, schema, path)
    elif isinstance(value, str):
        _validate_string_schema(value, schema, path)
    elif _is_json_number(value):
        _validate_number_schema(value, schema, path)


def _validate_object_schema(
    value: Mapping[str, Any],
    schema: Mapping[str, Any],
    path: str,
) -> None:
    required = schema.get("required", [])
    if not isinstance(required, list):
        raise _SchemaValidationError(f"{path} uses an invalid required schema")
    for name in required:
        if not isinstance(name, str):
            raise _SchemaValidationError(f"{path} uses an invalid required schema")
        if name not in value:
            raise _SchemaValidationError(f"{path}.{name} is required")

    properties = schema.get("properties", {})
    if not isinstance(properties, Mapping):
        raise _SchemaValidationError(f"{path} uses an invalid properties schema")
    additional = schema.get("additionalProperties", True)
    for name, item in value.items():
        if not isinstance(name, str):
            raise _SchemaValidationError(f"{path} object keys must be strings")
        item_path = f"{path}.{name}"
        if name in properties:
            _validate_schema_value(item, properties[name], item_path)
        elif additional is False:
            raise _SchemaValidationError(f"{item_path} is not allowed")
        elif isinstance(additional, Mapping) or isinstance(additional, bool):
            _validate_schema_value(item, additional, item_path)
        else:
            raise _SchemaValidationError(
                f"{path} uses an invalid additionalProperties schema"
            )
    _validate_count(len(value), schema, path, "Properties")


def _validate_array_schema(
    value: list[Any],
    schema: Mapping[str, Any],
    path: str,
) -> None:
    _validate_count(len(value), schema, path, "Items")
    item_schema = schema.get("items")
    if item_schema is not None:
        for index, item in enumerate(value):
            _validate_schema_value(item, item_schema, f"{path}[{index}]")
    if schema.get("uniqueItems") is True:
        seen: set[str] = set()
        for item in value:
            key = _json_value_key(item)
            if key in seen:
                raise _SchemaValidationError(f"{path} items must be unique")
            seen.add(key)


def _validate_string_schema(
    value: str,
    schema: Mapping[str, Any],
    path: str,
) -> None:
    minimum = schema.get("minLength")
    maximum = schema.get("maxLength")
    if minimum is not None and len(value) < minimum:
        raise _SchemaValidationError(f"{path} is shorter than minLength {minimum}")
    if maximum is not None and len(value) > maximum:
        raise _SchemaValidationError(f"{path} is longer than maxLength {maximum}")
    pattern = schema.get("pattern")
    if pattern is not None:
        if not isinstance(pattern, str):
            raise _SchemaValidationError(f"{path} uses an invalid pattern schema")
        try:
            matches = re.search(pattern, value)
        except re.error as exc:
            raise _SchemaValidationError(
                f"{path} uses an invalid pattern schema: {exc}"
            ) from exc
        if matches is None:
            raise _SchemaValidationError(f"{path} does not match pattern {pattern}")


def _validate_number_schema(
    value: int | float,
    schema: Mapping[str, Any],
    path: str,
) -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise _SchemaValidationError(f"{path} must be a finite number")
    comparisons = (
        ("minimum", lambda current, limit: current >= limit, ">="),
        ("maximum", lambda current, limit: current <= limit, "<="),
        ("exclusiveMinimum", lambda current, limit: current > limit, ">"),
        ("exclusiveMaximum", lambda current, limit: current < limit, "<"),
    )
    for keyword, comparison, operator in comparisons:
        if keyword in schema and not comparison(value, schema[keyword]):
            raise _SchemaValidationError(
                f"{path} must be {operator} {schema[keyword]}"
            )


def _validate_count(
    count: int,
    schema: Mapping[str, Any],
    path: str,
    suffix: str,
) -> None:
    minimum = schema.get(f"min{suffix}")
    maximum = schema.get(f"max{suffix}")
    if minimum is not None and count < minimum:
        raise _SchemaValidationError(
            f"{path} contains fewer than min{suffix} {minimum}"
        )
    if maximum is not None and count > maximum:
        raise _SchemaValidationError(
            f"{path} contains more than max{suffix} {maximum}"
        )


def _schema_matches(value: Any, schema: Any, path: str) -> bool:
    try:
        _validate_schema_value(value, schema, path)
    except _SchemaValidationError:
        return False
    return True


def _matches_json_type(value: Any, expected: str) -> bool:
    if expected == "object":
        return isinstance(value, Mapping)
    if expected == "array":
        return isinstance(value, list)
    if expected == "string":
        return isinstance(value, str)
    if expected == "number":
        return _is_json_number(value)
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "boolean":
        return isinstance(value, bool)
    if expected == "null":
        return value is None
    raise _SchemaValidationError(f"unsupported JSON Schema type '{expected}'")


def _is_json_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _json_values_equal(left: Any, right: Any) -> bool:
    return _json_value_key(left) == _json_value_key(right)


def _json_value_key(value: Any) -> str:
    if isinstance(value, bool):
        return f"boolean:{str(value).lower()}"
    if _is_json_number(value):
        return f"number:{Decimal(str(value)).normalize()}"
    if value is None:
        return "null"
    if isinstance(value, str):
        return f"string:{json.dumps(value, ensure_ascii=False)}"
    if isinstance(value, list):
        return "array:[" + ",".join(_json_value_key(item) for item in value) + "]"
    if isinstance(value, Mapping):
        return "object:{" + ",".join(
            f"{json.dumps(str(key), ensure_ascii=False)}:{_json_value_key(item)}"
            for key, item in sorted(value.items(), key=lambda pair: str(pair[0]))
        ) + "}"
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _call_tool_handler(
    tool: McpTool,
    arguments: Mapping[str, Any],
    context: McpRequestContext,
) -> Any:
    parameters = inspect.signature(tool.handler).parameters
    if "context" in parameters:
        return tool.handler(dict(arguments), context=context)
    return tool.handler(dict(arguments))


def _call_prompt_handler(
    prompt: McpPrompt,
    arguments: Mapping[str, Any],
    context: McpRequestContext,
) -> Any:
    parameters = inspect.signature(prompt.handler).parameters
    if "context" in parameters:
        return prompt.handler(dict(arguments), context=context)
    return prompt.handler(dict(arguments))


def _tool_result(result: Any) -> dict[str, Any]:
    if isinstance(result, dict) and (
        "isError" in result
        or "structuredContent" in result
        or ("content" in result and isinstance(result.get("content"), list))
    ):
        return result
    if isinstance(result, dict):
        return {
            "content": [
                {
                    "type": "text",
                    "text": json.dumps(
                        result,
                        ensure_ascii=False,
                        indent=2,
                        allow_nan=False,
                    ),
                }
            ],
            "structuredContent": result,
        }
    if isinstance(result, str):
        text = result
    else:
        text = json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False)
    return {"content": [{"type": "text", "text": text}]}


def _prompt_result(result: Any) -> dict[str, Any]:
    if isinstance(result, dict) and "messages" in result:
        return result
    return {
        "messages": [
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": str(result),
                },
            }
        ]
    }


def _resource_result(resource: McpResource) -> dict[str, Any]:
    content = resource.handler()
    entry: dict[str, Any] = {
        "uri": resource.uri,
    }
    if resource.mime_type:
        entry["mimeType"] = resource.mime_type
    if isinstance(content, bytes):
        entry["blob"] = base64.b64encode(content).decode("ascii")
    else:
        entry["text"] = str(content)
    return {"contents": [entry]}


def _resolve_tools(source: McpToolSource) -> list[McpTool]:
    return source() if callable(source) else source


def _resolve_resources(source: McpResourceSource) -> list[McpResource]:
    return source() if callable(source) else source


def _resolve_prompts(source: McpPromptSource) -> list[McpPrompt]:
    return source() if callable(source) else source


def _server_icons_from_env() -> list[dict[str, Any]]:
    raw = os.environ.get(SERVICE_ICON_ENV, "")
    if not raw:
        return []
    try:
        icons = json.loads(raw)
    except json.JSONDecodeError:
        return []
    if not isinstance(icons, list):
        return []
    clean_icons = []
    for icon in icons:
        if not isinstance(icon, dict) or not isinstance(icon.get("src"), str):
            continue
        clean_icons.append(dict(icon))
    return clean_icons


SERVICE_BIND_HOST_ERROR = (
    "QCopilots MCP services require localhost, an IPv4 LAN/private address, or 0.0.0.0"
)


def _service_bind_host(value: str) -> str:
    host = value.strip()
    if not host:
        raise ValueError("QCopilots MCP services require an explicit IPv4 bind host.")
    if host.lower() == "localhost":
        return host
    try:
        address = ipaddress.ip_address(host)
        if address.version == 4 and (
            address.is_loopback
            or address.is_private
            or address.is_link_local
            or host == "0.0.0.0"
        ):
            return host
    except ValueError as err:
        raise ValueError(f"{SERVICE_BIND_HOST_ERROR}: {host}") from err
    raise ValueError(f"{SERVICE_BIND_HOST_ERROR}: {host}")


def _cors_origins_from_env() -> list[str]:
    configured = os.environ.get(CORS_ORIGINS_ENV)
    if configured is None:
        return list(DEFAULT_CORS_ORIGINS)
    origins = [item.strip() for item in configured.split(os.pathsep) if item.strip()]
    return origins or list(DEFAULT_CORS_ORIGINS)
