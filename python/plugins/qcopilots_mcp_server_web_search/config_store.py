# SPDX-License-Identifier: GPL-2.0-or-later
"""Read-only configuration for the independently managed WebSearch backend."""
from __future__ import annotations
import ipaddress
import json
import math
import os
import re
import stat
from dataclasses import dataclass, field
from pathlib import Path
from urllib.parse import urlsplit
from qcopilots_common.constants import qcopilots_home
from qcopilots_common.security_policy import filesystem_policy_from_environment

CONFIG_ENV = "QCOPILOTS_WEBSEARCH_CONFIG"
LOCAL_AUTH_ENV = "QCOPILOTS_MCP_AUTH_TOKEN"


class ConfigError(ValueError):
    pass


@dataclass(frozen=True)
class BackendConfig:
    backend_url: str
    token: str = field(repr=False)
    timeout_seconds: float = 25.0
    max_response_bytes: int = 1024 * 1024
    max_concurrency: int = 5
    ca_file: str | None = None


def _text(path: Path, limit: int, *, secret: bool = False) -> str:
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    with os.fdopen(os.open(path, flags), "rb") as stream:
        metadata = os.fstat(stream.fileno())
        if not stat.S_ISREG(metadata.st_mode):
            raise ConfigError("Configuration inputs must be regular files.")
        if secret and os.name != "nt" and metadata.st_mode & 0o077:
            raise ConfigError("Token files must be readable only by their owner.")
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ConfigError("A configuration input exceeds its size limit.")
    return data.decode("utf-8-sig")


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ConfigError("Duplicate configuration keys are not permitted.")
        result[key] = value
    return result


def _integer(value, minimum, maximum):
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise ConfigError("An integer configuration value is outside its range.")
    return value


def load_config(path: str | Path | None = None) -> BackendConfig:
    try:
        return _load_config(path)
    except (ConfigError, PermissionError):
        raise
    except (OSError, ValueError, TypeError, UnicodeError):
        raise ConfigError("Cannot read the WebSearch backend configuration.") from None


def _load_config(path) -> BackendConfig:
    policy = filesystem_policy_from_environment()
    configured = path or os.environ.get(CONFIG_ENV)
    if configured is None:
        user_path = qcopilots_home() / "web_search" / "config.json"
        configured = user_path if user_path.is_file() else Path(__file__).with_name("qcopilots_web_search_config.json")
    elif path is None and not Path(configured).is_absolute():
        raise ConfigError("QCOPILOTS_WEBSEARCH_CONFIG must be an absolute path.")
    config_path = policy.resolve_path(configured, access="read")
    value = json.loads(_text(config_path, 64 * 1024), object_pairs_hook=_unique_object)
    allowed = {"$schema", "backend_url", "timeout_seconds", "max_response_bytes", "max_concurrency", "auth", "ca_file"}
    if not isinstance(value, dict) or set(value) - allowed:
        raise ConfigError("Unknown or invalid backend configuration fields.")
    url = value.get("backend_url")
    if not isinstance(url, str) or not url or url != url.strip():
        raise ConfigError("backend_url must be a non-empty URL.")
    if "\\" in url or any(ord(character) <= 32 or ord(character) == 127 for character in url):
        raise ConfigError("backend_url contains unsafe characters.")
    parsed = urlsplit(url)
    if (parsed.scheme not in {"http", "https"} or not parsed.hostname or parsed.username is not None
            or parsed.password is not None or parsed.query or parsed.fragment or parsed.path not in {"", "/"}
            or "%" in parsed.netloc):
        raise ConfigError("backend_url must be an HTTP(S) origin without credentials.")
    port = parsed.port
    if port is not None and not 1 <= port <= 65535:
        raise ConfigError("backend_url has an invalid port.")
    if parsed.scheme == "http":
        try:
            loopback = ipaddress.ip_address(parsed.hostname).is_loopback
        except ValueError:
            loopback = False
        if not loopback:
            raise ConfigError("Remote WebSearch backends require HTTPS.")
    policy.validate_network_url(url)
    auth = value.get("auth")
    if not isinstance(auth, dict) or len(auth) != 1 or not set(auth) <= {"token_env", "token_file"}:
        raise ConfigError("Select exactly one token_env or token_file.")
    if "token_env" in auth:
        name = auth["token_env"]
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) or name == LOCAL_AUTH_ENV:
            raise ConfigError("The backend token must use its own environment variable.")
        token = os.environ.get(name, "")
    else:
        name = auth["token_file"]
        if not isinstance(name, str) or not name.strip():
            raise ConfigError("token_file must be a non-empty local path.")
        token_path = policy.resolve_path(name, access="read", base=config_path.parent)
        token = _text(token_path, 8192, secret=True).removesuffix("\n").removesuffix("\r")
    if (not token or len(token) > 8192 or any(not 33 <= ord(character) <= 126 for character in token)
            or token == os.environ.get(LOCAL_AUTH_ENV)):
        raise ConfigError("A separate printable backend bearer token is required.")
    timeout = value.get("timeout_seconds", 25)
    if isinstance(timeout, bool) or not isinstance(timeout, (int, float)) or not math.isfinite(timeout) or not 1 <= timeout <= 120:
        raise ConfigError("timeout_seconds must be between 1 and 120.")
    ca = value.get("ca_file", "")
    if not isinstance(ca, str):
        raise ConfigError("ca_file must be a local path or an empty string.")
    ca_file = str(policy.resolve_path(ca, access="read", base=config_path.parent)) if ca else None
    return BackendConfig(url.rstrip("/"), token, float(timeout),
        _integer(value.get("max_response_bytes", 1024 * 1024), 1, 1024 * 1024),
        _integer(value.get("max_concurrency", 5), 1, 64), ca_file)
