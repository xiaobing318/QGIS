"""User-editable, atomic configuration for General service SSH transfers."""

from __future__ import annotations

import copy
import json
import os
import threading
import uuid
from collections.abc import Mapping
from pathlib import Path

from qcopilots_common.constants import qcopilots_home

from .file_transfer_errors import FileTransferError
from .file_transfer_connection import validate_connection_fields


CONFIG_VERSION = 1
CONFIG_FILENAME = "file_transfer_config.json"
DEFAULT_CONFIG = {
    "config_version": CONFIG_VERSION,
    "host": "",
    "port": 22,
    "username": "",
    "password": "",
    "upload_sources": [],
    "upload_destination": "",
    "download_sources": [],
    "download_destination": "",
    "connect_timeout": 15,
    "io_timeout": 30,
    "known_hosts_file": "~/.ssh/known_hosts",
    "max_bytes_per_second": 0,
}
CONFIG_FIELDS = frozenset(DEFAULT_CONFIG) - {"config_version"}
_METADATA_FIELDS = frozenset({"config_version", "$schema"})


class FileTransferConfigStore:
    """Read the latest on-disk settings and publish only password-free values."""

    def __init__(self, root=None):
        self.root = (
            Path(root).expanduser().resolve()
            if root is not None
            else qcopilots_home() / "processing_general"
        )
        self.path = self.root / CONFIG_FILENAME
        self._lock = threading.RLock()

    def load(self, require_connection=False):
        with self._lock:
            data = copy.deepcopy(DEFAULT_CONFIG)
            try:
                raw = self.path.read_text(encoding="utf-8-sig")
            except FileNotFoundError:
                raw = None
            except (OSError, UnicodeError):
                raise FileTransferError(
                    "config_read_failed", "The file transfer configuration file could not be read."
                ) from None
            if raw is not None:
                try:
                    parsed = json.loads(raw)
                except json.JSONDecodeError as error:
                    raise FileTransferError(
                        "invalid_config",
                        "The file transfer configuration file is not valid JSON.",
                        details={"line": error.lineno, "column": error.colno},
                    ) from None
                if not isinstance(parsed, dict):
                    raise FileTransferError("invalid_config", "The file transfer configuration must be a JSON object.")
                if set(parsed) - CONFIG_FIELDS - _METADATA_FIELDS:
                    raise FileTransferError("invalid_config", "The file transfer configuration contains unsupported fields.")
                data.update(parsed)
            return _validate_config(data, require_connection=require_connection)

    def public(self):
        return self._public(self.load())

    def update(self, changes):
        if not isinstance(changes, Mapping):
            raise FileTransferError("invalid_config", "Configuration changes must be an object.")
        if set(changes) - CONFIG_FIELDS:
            raise FileTransferError("invalid_config", "Configuration changes contain unsupported fields.")
        with self._lock:
            data = self.load()
            data.update(copy.deepcopy(dict(changes)))
            data = _validate_config(data)
            data["$schema"] = Path(__file__).with_name("file_transfer_config.schema.json").as_uri()
            try:
                _atomic_write_json(self.path, data)
            except (OSError, ValueError, TypeError):
                raise FileTransferError(
                    "config_write_failed",
                    "The file transfer configuration could not be saved. Previous settings were preserved.",
                ) from None
            return self._public(data)

    def _public(self, config):
        result = {
            field: copy.deepcopy(config[field])
            for field in CONFIG_FIELDS
            if field != "password"
        }
        result.update(
            {
                "config_version": CONFIG_VERSION,
                "config_path": str(self.path),
                "password_configured": bool(config["password"]),
            }
        )
        return result


def _validate_config(data, *, require_connection=False):
    version = data.get("config_version")
    if isinstance(version, bool) or not isinstance(version, int) or version != CONFIG_VERSION:
        raise FileTransferError("invalid_config", "Unsupported file transfer configuration version.")
    if "$schema" in data and not isinstance(data["$schema"], str):
        raise FileTransferError("invalid_config", "The configuration schema reference must be a string.")
    for field in ("host", "username", "password", "upload_destination", "download_destination", "known_hosts_file"):
        value = data[field]
        if not isinstance(value, str) or "\0" in value:
            raise FileTransferError("invalid_config", f"Configuration field {field} must be a string without NUL characters.")
        if field not in {"password", "username", "host"}:
            data[field] = value.strip()
    host, port, username = validate_connection_fields(
        data, required=require_connection, missing_code="connection_not_configured"
    )
    data.update(host=host, port=port, username=username)
    if not data["known_hosts_file"]:
        data["known_hosts_file"] = DEFAULT_CONFIG["known_hosts_file"]
    rate = data["max_bytes_per_second"]
    if isinstance(rate, bool) or not isinstance(rate, int) or not 0 <= rate <= 2**63 - 1:
        raise FileTransferError("invalid_config", "The bandwidth limit must be a non-negative integer.")
    for field in ("upload_sources", "download_sources"):
        values = data[field]
        if not isinstance(values, list) or any(
            not isinstance(value, str) or not value.strip() or "\0" in value
            for value in values
        ):
            raise FileTransferError("invalid_config", f"Configuration field {field} must be an array of non-empty path strings.")
        data[field] = [value.strip() for value in values]
    return data


def _atomic_write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.{uuid.uuid4().hex}.tmp")
    try:
        with temporary.open("x", encoding="utf-8", newline="\n") as handle:
            json.dump(data, handle, ensure_ascii=False, allow_nan=False, indent=2)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)
