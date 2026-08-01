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
import threading
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from qcopilots_common.constants import DEFAULT_HOST, qcopilots_home
from qcopilots_common.service_id import is_safe_service_id


MANAGER_CONFIG_FILENAME = "qcopilots_manager_config.json"
DEFAULT_STARTUP_SERVICE_IDS = (
    "qcopilots.mcp_server_builtin_tools",
    "qcopilots.mcp_server_interactive_tools",
    "qcopilots.mcp_server_processing_vector",
    "qcopilots.mcp_server_processing_raster",
    "qcopilots.mcp_server_skills",
)


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

    def snapshot(self) -> dict[str, Any]:
        """Return a defensive copy of the effective configuration."""

        with self._lock:
            return copy.deepcopy(self._config)

    def reload(self) -> dict[str, Any]:
        """Reload the template and user override, discarding unsaved changes."""

        with self._lock:
            self._config = self._load()
            self._dirty = False
            self._last_error = ""
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

    def _load(self) -> dict[str, Any]:
        config = _default_config()
        template = _read_json_object(
            self.template_path,
            self.logger,
            label="packaged manager config template",
            missing_is_error=True,
        )
        if template is not None:
            config = _apply_config_layer(config, template, self.logger, "packaged template")

        user_config = _read_json_object(
            self.user_path,
            self.logger,
            label="user manager config",
            missing_is_error=False,
        )
        self._user_config_usable = user_config is not None
        if user_config is not None:
            config = _apply_config_layer(config, user_config, self.logger, "user config")
        return config

    def _save_locked(self, *, changed: bool) -> ConfigSaveResult:
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


def _default_config() -> dict[str, Any]:
    return {
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
    }


def _read_json_object(
    path: Path,
    logger,
    *,
    label: str,
    missing_is_error: bool,
) -> dict[str, Any] | None:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        if missing_is_error:
            _warning(logger, "QCopilots %s is missing: %s", label, path)
        return None
    except Exception as err:
        _warning(logger, "Could not read QCopilots %s %s: %s", label, path, err)
        return None
    if not isinstance(data, dict):
        _warning(logger, "Ignoring QCopilots %s because it is not a JSON object: %s", label, path)
        return None
    return data


def _apply_config_layer(
    base: dict[str, Any],
    layer: dict[str, Any],
    logger,
    label: str,
) -> dict[str, Any]:
    result = copy.deepcopy(base)
    startup = layer.get("default_startup")
    if startup is not None:
        if not isinstance(startup, dict):
            _warning(logger, "Ignoring QCopilots %s default_startup because it is not an object", label)
        else:
            if "enabled" in startup:
                if isinstance(startup["enabled"], bool):
                    result["default_startup"]["enabled"] = startup["enabled"]
                else:
                    _warning(logger, "Ignoring QCopilots %s default_startup.enabled because it is not a boolean", label)
            if "service_ids" in startup:
                service_ids = _normalized_service_ids(startup["service_ids"], logger, label)
                if service_ids is not None:
                    result["default_startup"]["service_ids"] = service_ids

    network = layer.get("service_network")
    if network is not None:
        if not isinstance(network, dict):
            _warning(logger, "Ignoring QCopilots %s service_network because it is not an object", label)
        else:
            _apply_network_fields(result["service_network"], network, logger, label)
    return result


def _normalized_service_ids(value: Any, logger, label: str) -> list[str] | None:
    if not isinstance(value, list):
        _warning(logger, "Ignoring QCopilots %s default_startup.service_ids because it is not a list", label)
        return None

    result = []
    for item in value:
        if not isinstance(item, str):
            _warning(logger, "Ignoring non-string QCopilots %s startup service id", label)
            continue
        service_id = item.strip()
        if not is_safe_service_id(service_id):
            _warning(logger, "Ignoring unsafe QCopilots %s startup service id: %s", label, service_id)
            continue
        if service_id in result:
            _warning(logger, "Ignoring duplicate QCopilots %s startup service id: %s", label, service_id)
            continue
        result.append(service_id)
    return result


def _apply_network_fields(
    destination: dict[str, Any],
    source: dict[str, Any],
    logger,
    label: str,
) -> None:
    if "enabled" in source:
        if isinstance(source["enabled"], bool):
            destination["enabled"] = source["enabled"]
        else:
            _warning(logger, "Ignoring QCopilots %s service_network.enabled because it is not a boolean", label)

    for field in ("host", "advertised_host"):
        if field not in source:
            continue
        if source[field] == DEFAULT_HOST:
            destination[field] = DEFAULT_HOST
        else:
            _warning(
                logger,
                "Ignoring QCopilots %s service_network.%s because it is not %s",
                label,
                field,
                DEFAULT_HOST,
            )

    if "cors_origins" in source:
        if isinstance(source["cors_origins"], list) and not source["cors_origins"]:
            destination["cors_origins"] = []
        else:
            _warning(
                logger,
                "Ignoring QCopilots %s service_network.cors_origins because it is not an empty list",
                label,
            )


def _validated_service_id(service_id: str) -> str:
    if not isinstance(service_id, str):
        raise TypeError("service_id must be a string")
    normalized = service_id.strip()
    if not is_safe_service_id(normalized):
        raise ValueError(f"Invalid QCopilots service id: {service_id}")
    return normalized


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
