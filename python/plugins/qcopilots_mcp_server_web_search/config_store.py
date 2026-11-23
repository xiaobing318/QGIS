# SPDX-License-Identifier: GPL-2.0-or-later
"""Read transport configuration without loading business or admin credentials."""
from __future__ import annotations
import ipaddress
import json
import math
import os
import stat
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlsplit
from qcopilots_common.constants import qcopilots_home
from qcopilots_common.security_policy import filesystem_policy_from_environment

CONFIG_ENV = "QCOPILOTS_WEBSEARCH_CONFIG"
DEFAULT_BACKEND_URL = "https://web-search.vastarmorai.com/mcp"


class ConfigError(ValueError):
    pass


@dataclass(frozen=True)
class BackendConfig:
    backend_url: str
    timeout_seconds: float = 25.0
    max_response_bytes: int = 1024 * 1024
    max_concurrency: int = 5
    ca_file: str | None = None


def _text(path: Path) -> str:
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    with os.fdopen(os.open(path, flags), "rb") as stream:
        if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
            raise ConfigError("Configuration must be a regular file.")
        data = stream.read(64 * 1024 + 1)
    if len(data) > 64 * 1024:
        raise ConfigError("Configuration exceeds its size limit.")
    return data.decode("utf-8-sig")


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ConfigError("Duplicate configuration keys are not permitted.")
        result[key] = value
    return result


def _integer(value, minimum, maximum):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ConfigError("An integer configuration value is outside its range.")
    return value


def _endpoint(url):
    if not isinstance(url, str) or not url or url != url.strip():
        raise ConfigError("backend_url must be a non-empty MCP endpoint.")
    if "\\" in url or any(ord(ch) <= 32 or ord(ch) == 127 for ch in url):
        raise ConfigError("backend_url contains unsafe characters.")
    parsed = urlsplit(url)
    if (parsed.scheme not in {"http", "https"} or not parsed.hostname
            or parsed.username is not None or parsed.password is not None
            or parsed.query or parsed.fragment or "%" in parsed.netloc
            or parsed.path not in {"", "/", "/mcp"}):
        raise ConfigError("backend_url must identify /mcp without credentials, query or fragment.")
    if parsed.port is not None and not 1 <= parsed.port <= 65535:
        raise ConfigError("backend_url has an invalid port.")
    if parsed.scheme == "http":
        try:
            loopback = ipaddress.ip_address(parsed.hostname).is_loopback
        except ValueError:
            loopback = False
        if not loopback:
            raise ConfigError("Remote WebSearch connections require HTTPS.")
    return url.rstrip("/") + ("" if parsed.path == "/mcp" else "/mcp")


def load_config(path: str | Path | None = None) -> BackendConfig:
    try:
        policy = filesystem_policy_from_environment()
        configured = path or os.environ.get(CONFIG_ENV)
        if configured is None:
            user_path = qcopilots_home() / "web_search" / "config.json"
            configured = user_path if user_path.is_file() else Path(__file__).with_name("qcopilots_web_search_config.json")
        elif path is None and not Path(configured).is_absolute():
            raise ConfigError("QCOPILOTS_WEBSEARCH_CONFIG must be an absolute path.")
        config_path = policy.resolve_path(configured, access="read")
        value = json.loads(_text(config_path), object_pairs_hook=_unique_object)
        allowed = {"$schema", "backend_url", "timeout_seconds", "max_response_bytes", "max_concurrency",
                   "ca_file", "auth", "settings"}
        if not isinstance(value, dict) or set(value) - allowed:
            raise ConfigError("Unknown or invalid backend configuration fields.")
        # Legacy auth/settings are ignored without consulting env or files.
        url = _endpoint(value.get("backend_url", DEFAULT_BACKEND_URL))
        policy.validate_network_url(url)
        timeout = value.get("timeout_seconds", 25)
        if (isinstance(timeout, bool) or not isinstance(timeout, (int, float))
                or not math.isfinite(timeout) or not 1 <= timeout <= 120):
            raise ConfigError("timeout_seconds must be between 1 and 120.")
        ca = value.get("ca_file", "")
        if not isinstance(ca, str):
            raise ConfigError("ca_file must be a local path or an empty string.")
        ca_file = str(policy.resolve_path(ca, access="read", base=config_path.parent)) if ca else None
        return BackendConfig(url, float(timeout),
            _integer(value.get("max_response_bytes", 1024 * 1024), 1, 1024 * 1024),
            _integer(value.get("max_concurrency", 5), 1, 64), ca_file)
    except (ConfigError, PermissionError):
        raise
    except (OSError, ValueError, TypeError, UnicodeError):
        raise ConfigError("Cannot read the WebSearch MCP configuration.") from None
