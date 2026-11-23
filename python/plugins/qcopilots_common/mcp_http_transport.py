# SPDX-License-Identifier: GPL-2.0-or-later
"""Shared HTTP boundary checks for local MCP services and relays.

This module does not authenticate requests or interpret MCP messages.
"""

from __future__ import annotations

import hmac
import re
import time

from qcopilots_common.constants import (
    HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS,
    MAX_HTTP_ERROR_BODY_DRAIN_BYTES,
)


CORS_ALLOWED_METHODS = ("GET", "POST", "DELETE")
CORS_ALLOWED_HEADERS = (
    "Authorization",
    "Content-Type",
    "Accept",
    "Mcp-Session-Id",
    "MCP-Protocol-Version",
    "Last-Event-ID",
)
CORS_EXPOSED_HEADERS = ("Mcp-Session-Id", "WWW-Authenticate", "Allow", "Retry-After")
CORS_PREFLIGHT_MAX_AGE_SECONDS = 600
_CORS_ALLOWED_HEADER_NAMES = frozenset(name.casefold() for name in CORS_ALLOWED_HEADERS)
_HTTP_FIELD_NAME_PATTERN = re.compile(r"^[!#$%&'*+\-.^_`|~0-9A-Za-z]+$")


class HttpTransportError(ValueError):
    """A bounded public HTTP error with no request data in its message."""

    def __init__(self, status: int, code: str):
        self.status = int(status)
        self.code = code
        super().__init__(code)


def validate_request_host(headers, host: str, port: int) -> None:
    values = headers.get_all("Host") or []
    expected = f"{host}:{int(port)}"
    if (
        len(values) != 1
        or not values[0].isascii()
        or not hmac.compare_digest(values[0], expected)
    ):
        raise HttpTransportError(403, "host_not_allowed")


def validate_request_origin(headers, cors_origins, require_origin: bool = False) -> str | None:
    values = headers.get_all("Origin") or []
    if not values:
        if require_origin:
            raise HttpTransportError(403, "origin_required")
        return None
    origin = values[0] if len(values) == 1 else ""
    if origin != "null" and origin and any(origin == allowed for allowed in cors_origins):
        return origin
    raise HttpTransportError(403, "origin_not_allowed")


def request_body_length(headers) -> int:
    transfer_encodings = headers.get_all("Transfer-Encoding") or []
    lengths = headers.get_all("Content-Length") or []
    if transfer_encodings or len(lengths) > 1:
        raise HttpTransportError(400, "bad_content_length")
    if not lengths:
        return 0
    value = lengths[0]
    if not re.fullmatch(r"0|[1-9][0-9]*", value):
        raise HttpTransportError(400, "bad_content_length")
    try:
        return int(value)
    except ValueError:
        raise HttpTransportError(400, "bad_content_length") from None


def validate_preflight(headers) -> None:
    methods = headers.get_all("Access-Control-Request-Method") or []
    if len(methods) != 1:
        raise HttpTransportError(400, "cors_method_required")
    if methods[0].strip() not in CORS_ALLOWED_METHODS:
        raise HttpTransportError(405, "cors_method_not_allowed")
    for line in headers.get_all("Access-Control-Request-Headers") or []:
        for value in line.split(","):
            name = value.strip()
            if (
                not name
                or not _HTTP_FIELD_NAME_PATTERN.fullmatch(name)
                or name.casefold() not in _CORS_ALLOWED_HEADER_NAMES
            ):
                raise HttpTransportError(403, "cors_headers_not_allowed")


def cors_response_headers(origin: str | None, preflight: bool = False) -> dict[str, str]:
    if origin is None:
        return {}
    headers = {"Access-Control-Allow-Origin": origin, "Vary": "Origin"}
    if preflight:
        headers.update({
            "Access-Control-Allow-Methods": ", ".join(CORS_ALLOWED_METHODS),
            "Access-Control-Allow-Headers": ", ".join(CORS_ALLOWED_HEADERS),
            "Access-Control-Max-Age": str(CORS_PREFLIGHT_MAX_AGE_SECONDS),
        })
    else:
        headers["Access-Control-Expose-Headers"] = ", ".join(CORS_EXPOSED_HEADERS)
    return headers


def discard_request_body(handler) -> None:
    try:
        length = request_body_length(handler.headers)
    except HttpTransportError:
        handler.close_connection = True
        return
    if length == 0:
        return
    if length > MAX_HTTP_ERROR_BODY_DRAIN_BYTES:
        handler.close_connection = True
        return
    old_timeout = handler.connection.gettimeout()
    deadline = time.monotonic() + HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS
    try:
        remaining = length
        while remaining > 0:
            timeout = deadline - time.monotonic()
            if timeout <= 0:
                handler.close_connection = True
                break
            handler.connection.settimeout(timeout)
            chunk = handler.rfile.read(min(remaining, 8192))
            if not chunk:
                handler.close_connection = True
                break
            remaining -= len(chunk)
    except OSError:
        handler.close_connection = True
    finally:
        try:
            handler.connection.settimeout(old_timeout)
        except OSError:
            handler.close_connection = True
