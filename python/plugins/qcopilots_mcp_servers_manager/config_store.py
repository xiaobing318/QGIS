"""Persistent user configuration for QCopilots MCP Servers Manager.

The JSON file installed beside the plugin is an immutable template. Runtime
changes are stored below the per-user QCopilots state directory so package
updates and read-only installation layouts do not discard user choices.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import copy
import json
import os
import re
import secrets
import shutil
import threading
import uuid
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from qcopilots_common.constants import DEFAULT_HOST, qcopilots_home
from qcopilots_common.security_policy import filesystem_policy_from_config
from qcopilots_common.service_id import is_safe_service_id


MANAGER_CONFIG_FILENAME = "qcopilots_manager_config.json"
MANAGER_CONFIG_VERSION = 1
_INCOMPATIBLE_CONFIG_REASONS = frozenset(
    {
        "missing-version",
        "older-version",
        "future-version",
        "invalid-version",
        "invalid-json",
        "non-object",
        "invalid-structure",
    }
)
DEFAULT_STARTUP_SERVICE_IDS = (
    "qcopilots.mcp_server_builtin_tools",
    "qcopilots.mcp_server_interactive_tools",
    "qcopilots.mcp_server_processing_vector",
    "qcopilots.mcp_server_processing_raster",
    "qcopilots.mcp_server_processing_general",
    "qcopilots.mcp_server_skills",
    "qcopilots.mcp_server_qgis_binary",
)

BROWSER_AUTH_TOKEN_PATTERN = re.compile(r"^[A-Za-z0-9_-]{32,256}$")
DEFAULT_BROWSER_ACCESS = {
    "enabled": True,
    "origin_source": "configured_qcopilots_url",
    "auth_token": "",
    "port_conflict_policy": "fail",
}
DEFAULT_SECURITY_POLICY = {
    "mode": "compatible",
    "shell": {
        "enabled": True,
        "executables": [],
    },
    "network": {
        "enabled": True,
        "allowed_origins": [],
    },
}
@dataclass(frozen=True)
class ConfigSaveResult:
    """Result of attempting to persist the current in-memory configuration."""

    changed: bool
    saved: bool
    dirty: bool
    error: str = ""


class ManagerConfigStore:
    """Load a package template and persist user startup choices atomically."""

    def __init__(
        self,
        template_path: str | Path,
        logger=None,
        user_path: str | Path | None = None,
    ):
        self.template_path = Path(template_path)
        self.user_path = (
            Path(user_path)
            if user_path is not None
            else qcopilots_home() / MANAGER_CONFIG_FILENAME
        )
        self.logger = logger
        self._lock = threading.RLock()
        self._dirty = False
        self._last_error = ""
        self._blocked_error = ""
        self._user_config_usable = False
        self._config = self._load()

    @property
    def dirty(self) -> bool:
        with self._lock:
            return self._dirty

    @property
    def last_error(self) -> str:
        with self._lock:
            return self._last_error

    @property
    def blocked(self) -> bool:
        with self._lock:
            return bool(self._blocked_error)

    @property
    def blocked_error(self) -> str:
        with self._lock:
            return self._blocked_error

    def snapshot(self) -> dict[str, Any]:
        """Return a defensive copy of the effective configuration."""

        with self._lock:
            return copy.deepcopy(self._config)

    def reload(self) -> dict[str, Any]:
        """Reload the template and user override, discarding unsaved changes."""

        with self._lock:
            self._blocked_error = ""
            self._config = self._load()
            self._dirty = False
            self._last_error = self._blocked_error
            return copy.deepcopy(self._config)

    def add_startup_service(self, service_id: str) -> ConfigSaveResult:
        """Remember that an explicitly started service should start next time."""

        return self.set_startup_service_enabled(service_id, True)

    def remove_startup_service(self, service_id: str) -> ConfigSaveResult:
        """Remember that an explicitly stopped service should stay stopped."""

        return self.set_startup_service_enabled(service_id, False)

    def set_startup_service_enabled(
        self,
        service_id: str,
        enabled: bool,
    ) -> ConfigSaveResult:
        """Update one service preference and persist the complete snapshot.

        The in-memory preference is retained if persistence fails. A later call
        retries the full dirty snapshot, including all intervening changes.
        """

        normalized_id = _validated_service_id(service_id)
        if not isinstance(enabled, bool):
            raise TypeError("enabled must be a boolean")

        with self._lock:
            if self._blocked_error:
                return self._blocked_result(changed=False)
            startup = self._config["default_startup"]
            service_ids = list(startup["service_ids"])
            changed = False
            if enabled and normalized_id not in service_ids:
                service_ids.append(normalized_id)
                changed = True
            elif not enabled and normalized_id in service_ids:
                service_ids.remove(normalized_id)
                changed = True

            if changed:
                startup["service_ids"] = service_ids
                self._dirty = True

            if not self._dirty and self._user_config_usable:
                return ConfigSaveResult(
                    changed=changed,
                    saved=True,
                    dirty=False,
                )
            return self._save_locked(changed=changed)

    def save(self) -> ConfigSaveResult:
        """Persist the current effective configuration even if it is unchanged."""

        with self._lock:
            return self._save_locked(changed=False)

    def ensure_browser_auth_token(self) -> ConfigSaveResult:
        """Create and persist the shared browser token when it is missing.

        A valid token already loaded from the user configuration is left
        untouched. The generated value remains in the dirty in-memory snapshot
        after a failed write so a later explicit save can retry atomically.
        """

        with self._lock:
            if self._blocked_error:
                return self._blocked_result(changed=False)
            token = self._config["browser_access"]["auth_token"]
            if is_valid_browser_auth_token(token):
                if self._user_config_usable and not self._dirty:
                    return ConfigSaveResult(
                        changed=False,
                        saved=True,
                        dirty=False,
                    )
                return self._save_locked(changed=False)

            self._config["browser_access"]["auth_token"] = secrets.token_urlsafe(32)
            self._dirty = True
            return self._save_locked(changed=True)

    def set_browser_auth_token(self, auth_token: str) -> ConfigSaveResult:
        """Persist a caller-supplied shared browser token."""

        if not is_valid_browser_auth_token(auth_token):
            raise ValueError(
                "Browser auth token must contain 32 to 256 URL-safe characters"
            )
        with self._lock:
            if self._blocked_error:
                return self._blocked_result(changed=False)
            changed = self._config["browser_access"]["auth_token"] != auth_token
            if changed:
                self._config["browser_access"]["auth_token"] = auth_token
                self._dirty = True
            if not self._dirty and self._user_config_usable:
                return ConfigSaveResult(changed=False, saved=True, dirty=False)
            return self._save_locked(changed=changed)

    def regenerate_browser_auth_token(self) -> ConfigSaveResult:
        """Replace the shared browser token with a fresh high-entropy value."""

        return self.set_browser_auth_token(secrets.token_urlsafe(32))

    def _load(self) -> dict[str, Any]:
        config = _default_config()
        try:
            template = _read_json_object(
                self.template_path,
                self.logger,
                label="packaged manager config template",
                missing_is_error=True,
                strict_existing=True,
            )
        except (OSError, TypeError, ValueError) as err:
            return self._block_configuration(
                f"Packaged manager configuration is invalid: {err}"
            )
        if template is None:
            return self._block_configuration(
                f"Packaged manager configuration is missing: {self.template_path}"
            )
        try:
            config = validate_manager_config(template, "packaged template")
        except (TypeError, ValueError) as err:
            return self._block_configuration(
                f"Packaged manager configuration is invalid: {err}"
            )

        try:
            user_config, incompatibility_reason = _read_user_config(
                self.user_path
            )
        except OSError as err:
            _warning(
                self.logger,
                "Could not read QCopilots manager user config %s (%s)",
                self.user_path,
                type(err).__name__,
            )
            return self._block_configuration(
                "Could not read user manager configuration at "
                f"{self.user_path}"
            )

        if user_config is None and incompatibility_reason is None:
            return config

        if incompatibility_reason is None:
            try:
                config = validate_manager_config(user_config, "user config")
            except (TypeError, ValueError):
                incompatibility_reason = "invalid-structure"
            else:
                self._user_config_usable = True
                return config

        return self._rebuild_incompatible_user_config(
            config,
            incompatibility_reason,
        )

    def _rebuild_incompatible_user_config(
        self,
        template: dict[str, Any],
        reason: str,
    ) -> dict[str, Any]:
        """Archive an incompatible user file and rebuild it from *template*."""

        backup_path = None
        try:
            backup_path = _archive_incompatible_user_config(
                self.user_path,
                reason,
            )
        except Exception as err:
            _warning(
                self.logger,
                "Could not archive incompatible QCopilots manager user config "
                "%s for reason %s (%s)",
                self.user_path,
                reason,
                type(err).__name__,
            )
            return self._block_configuration(
                "Could not archive incompatible user manager configuration at "
                f"{self.user_path}"
            )

        try:
            replacement = copy.deepcopy(template)
            replacement["browser_access"]["auth_token"] = secrets.token_urlsafe(32)
            _atomic_write_json(self.user_path, replacement)
        except Exception as err:
            _warning(
                self.logger,
                "Could not rebuild QCopilots manager user config %s from the "
                "packaged template (%s)",
                self.user_path,
                type(err).__name__,
            )
            try:
                _restore_archived_user_config(backup_path, self.user_path)
            except Exception as restore_err:
                _warning(
                    self.logger,
                    "Could not restore archived QCopilots manager user config "
                    "%s to %s (%s)",
                    backup_path,
                    self.user_path,
                    type(restore_err).__name__,
                )
                return self._block_configuration(
                    "Could not rebuild incompatible user manager configuration "
                    f"at {self.user_path}, and the original file could not be "
                    "restored"
                )
            return self._block_configuration(
                "Could not rebuild incompatible user manager configuration at "
                f"{self.user_path}. The original file was restored"
            )

        self._user_config_usable = True
        _warning(
            self.logger,
            "Archived incompatible QCopilots manager user config %s for reason "
            "%s to %s and generated version %s from the packaged template",
            self.user_path,
            reason,
            backup_path,
            MANAGER_CONFIG_VERSION,
        )
        return replacement

    def _save_locked(self, *, changed: bool) -> ConfigSaveResult:
        if self._blocked_error:
            return self._blocked_result(changed=changed)
        try:
            _atomic_write_json(self.user_path, self._config)
        except Exception as err:
            self._dirty = True
            self._last_error = str(err)
            _warning(
                self.logger,
                "Could not save QCopilots manager user config %s: %s",
                self.user_path,
                err,
            )
            return ConfigSaveResult(
                changed=changed,
                saved=False,
                dirty=True,
                error=self._last_error,
            )

        self._dirty = False
        self._last_error = ""
        self._user_config_usable = True
        return ConfigSaveResult(
            changed=changed,
            saved=True,
            dirty=False,
        )

    def _block_configuration(self, error: str) -> dict[str, Any]:
        self._blocked_error = error
        self._last_error = error
        self._dirty = False
        self._user_config_usable = False
        _warning(self.logger, "QCopilots manager configuration is blocked: %s", error)
        return _blocked_config()

    def _blocked_result(self, *, changed: bool) -> ConfigSaveResult:
        self._dirty = False
        self._last_error = self._blocked_error
        return ConfigSaveResult(
            changed=changed,
            saved=False,
            dirty=False,
            error=self._blocked_error,
        )


def _default_config() -> dict[str, Any]:
    return {
        "config_version": MANAGER_CONFIG_VERSION,
        "default_startup": {
            "enabled": True,
            "service_ids": list(DEFAULT_STARTUP_SERVICE_IDS),
        },
        "service_network": {
            "enabled": False,
            "host": DEFAULT_HOST,
            "advertised_host": DEFAULT_HOST,
            "cors_origins": [],
        },
        "browser_access": copy.deepcopy(DEFAULT_BROWSER_ACCESS),
        "security_policy": copy.deepcopy(DEFAULT_SECURITY_POLICY),
    }


def _blocked_config() -> dict[str, Any]:
    config = _default_config()
    config["default_startup"] = {"enabled": False, "service_ids": []}
    config["service_network"]["enabled"] = False
    config["browser_access"]["enabled"] = False
    config["browser_access"]["auth_token"] = ""
    return config


def _read_json_object(
    path: Path,
    logger,
    *,
    label: str,
    missing_is_error: bool,
    strict_existing: bool = False,
) -> dict[str, Any] | None:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        if missing_is_error:
            _warning(logger, "QCopilots %s is missing: %s", label, path)
        return None
    except Exception as err:
        _warning(logger, "Could not read QCopilots %s %s: %s", label, path, err)
        if strict_existing:
            raise ValueError(f"Could not read {label}: {err}") from err
        return None
    if not isinstance(data, dict):
        _warning(logger, "Ignoring QCopilots %s because it is not a JSON object: %s", label, path)
        if strict_existing:
            raise TypeError(f"{label} must be a JSON object")
        return None
    return data


def validate_manager_config(value: Any, label: str = "manager config") -> dict[str, Any]:
    """Return a normalized copy of a complete latest-format configuration."""
    if not isinstance(value, dict):
        raise TypeError(f"{label} must be a JSON object")
    required = {
        "config_version",
        "default_startup",
        "service_network",
        "browser_access",
        "security_policy",
    }
    obsolete = {"read_roots", "write_roots", "workspace_roots"}.intersection(value)
    if obsolete:
        raise ValueError(
            f"{label} contains obsolete fields: " + ", ".join(sorted(obsolete))
        )
    missing = required - set(value)
    unexpected = set(value) - required - {"$schema"}
    if missing:
        raise ValueError(
            f"{label} is missing required fields: " + ", ".join(sorted(missing))
        )
    if unexpected:
        raise ValueError(
            f"{label} contains unsupported fields: "
            + ", ".join(sorted(str(item) for item in unexpected))
        )
    if "$schema" in value and (
        not isinstance(value["$schema"], str) or not value["$schema"].strip()
    ):
        raise TypeError(f"{label}.$schema must be a non-empty string")
    config_version = value["config_version"]
    if type(config_version) is not int:
        raise TypeError(f"{label}.config_version must be an integer")
    if config_version != MANAGER_CONFIG_VERSION:
        raise ValueError(
            f"{label}.config_version {config_version} is unsupported. "
            f"expected {MANAGER_CONFIG_VERSION}"
        )

    startup = value["default_startup"]
    _require_object(startup, f"{label}.default_startup")
    _require_exact_fields(
        startup,
        {"enabled", "service_ids"},
        f"{label}.default_startup",
    )
    if not isinstance(startup["enabled"], bool):
        raise TypeError(f"{label}.default_startup.enabled must be a boolean")
    service_ids = startup["service_ids"]
    if not isinstance(service_ids, list):
        raise TypeError(f"{label}.default_startup.service_ids must be a list")
    normalized_ids = []
    for item in service_ids:
        service_id = _validated_service_id(item)
        if service_id != item:
            raise ValueError(
                f"{label}.default_startup.service_ids entries must not contain surrounding whitespace"
            )
        if service_id in normalized_ids:
            raise ValueError(
                f"{label}.default_startup.service_ids contains a duplicate: {service_id}"
            )
        normalized_ids.append(service_id)

    service_network = value["service_network"]
    _require_object(service_network, f"{label}.service_network")
    _require_exact_fields(
        service_network,
        {"enabled", "host", "advertised_host", "cors_origins"},
        f"{label}.service_network",
    )
    if not isinstance(service_network["enabled"], bool):
        raise TypeError(f"{label}.service_network.enabled must be a boolean")
    for field in ("host", "advertised_host"):
        if service_network[field] != DEFAULT_HOST:
            raise ValueError(f"{label}.service_network.{field} must be {DEFAULT_HOST}")
    if service_network["cors_origins"] != []:
        raise ValueError(f"{label}.service_network.cors_origins must be an empty list")

    browser_access = value["browser_access"]
    _require_object(browser_access, f"{label}.browser_access")
    _require_exact_fields(
        browser_access,
        {"enabled", "origin_source", "auth_token", "port_conflict_policy"},
        f"{label}.browser_access",
    )
    if not isinstance(browser_access["enabled"], bool):
        raise TypeError(f"{label}.browser_access.enabled must be a boolean")
    if browser_access["origin_source"] != "configured_qcopilots_url":
        raise ValueError(
            f"{label}.browser_access.origin_source must be configured_qcopilots_url"
        )
    token = browser_access["auth_token"]
    if token != "" and not is_valid_browser_auth_token(token):
        raise ValueError(f"{label}.browser_access.auth_token is invalid")
    if browser_access["port_conflict_policy"] != "fail":
        raise ValueError(f"{label}.browser_access.port_conflict_policy must be fail")

    security_policy = filesystem_policy_from_config(value["security_policy"]).to_config()
    return {
        "config_version": MANAGER_CONFIG_VERSION,
        "default_startup": {
            "enabled": startup["enabled"],
            "service_ids": normalized_ids,
        },
        "service_network": {
            "enabled": service_network["enabled"],
            "host": DEFAULT_HOST,
            "advertised_host": DEFAULT_HOST,
            "cors_origins": [],
        },
        "browser_access": {
            "enabled": browser_access["enabled"],
            "origin_source": "configured_qcopilots_url",
            "auth_token": token,
            "port_conflict_policy": "fail",
        },
        "security_policy": security_policy,
    }


def _read_user_config(path: Path) -> tuple[dict[str, Any] | None, str | None]:
    """Read an optional user file and classify incompatible data safely."""

    try:
        payload = path.read_bytes()
    except FileNotFoundError:
        return None, None

    try:
        value = json.loads(payload.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError):
        return None, "invalid-json"
    if not isinstance(value, dict):
        return None, "non-object"
    if "config_version" not in value:
        return value, "missing-version"

    version = value["config_version"]
    if type(version) is not int or version < 0:
        return value, "invalid-version"
    if version < MANAGER_CONFIG_VERSION:
        return value, "older-version"
    if version > MANAGER_CONFIG_VERSION:
        return value, "future-version"
    return value, None


def _archive_incompatible_user_config(path: Path, reason: str) -> Path:
    """Move *path* to a uniquely named, byte-preserving backup."""

    if reason not in _INCOMPATIBLE_CONFIG_REASONS:
        raise ValueError("Unsupported incompatible configuration reason")
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup_path = path.with_name(
        f"{path.name}.incompatible-{reason}-{timestamp}-{uuid.uuid4().hex}.bak"
    )
    os.replace(path, backup_path)
    return backup_path


def _restore_archived_user_config(backup_path: Path, user_path: Path) -> None:
    """Atomically restore *user_path* while retaining the permanent backup."""

    temp_path = user_path.with_name(
        f".{user_path.name}.{os.getpid()}.{threading.get_ident()}."
        f"{uuid.uuid4().hex}.restore.tmp"
    )
    try:
        with backup_path.open("rb") as source, temp_path.open("xb") as target:
            shutil.copyfileobj(source, target)
            target.flush()
            os.fsync(target.fileno())
        os.replace(temp_path, user_path)
    finally:
        temp_path.unlink(missing_ok=True)


def _require_object(value: Any, label: str) -> None:
    if not isinstance(value, dict):
        raise TypeError(f"{label} must be an object")


def _require_exact_fields(value: dict[str, Any], expected: set[str], label: str) -> None:
    missing = expected - set(value)
    unexpected = set(value) - expected
    if missing:
        raise ValueError(
            f"{label} is missing required fields: " + ", ".join(sorted(missing))
        )
    if unexpected:
        raise ValueError(
            f"{label} contains unsupported fields: "
            + ", ".join(sorted(str(item) for item in unexpected))
        )


def _validated_service_id(service_id: str) -> str:
    if not isinstance(service_id, str):
        raise TypeError("service_id must be a string")
    normalized = service_id.strip()
    if not is_safe_service_id(normalized):
        raise ValueError(f"Invalid QCopilots service id: {service_id}")
    return normalized


def is_valid_browser_auth_token(value: Any) -> bool:
    """Return whether *value* is safe to copy into a Bearer header."""

    return isinstance(value, str) and BROWSER_AUTH_TOKEN_PATTERN.fullmatch(value) is not None


def _atomic_write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    temp_path = path.with_name(
        f".{path.name}.{os.getpid()}.{threading.get_ident()}.{uuid.uuid4().hex}.tmp"
    )
    try:
        with temp_path.open("x", encoding="utf-8", newline="\n") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_path, path)
    finally:
        temp_path.unlink(missing_ok=True)


def _warning(logger, message: str, *args) -> None:
    if logger is not None:
        logger.warning(message, *args)
