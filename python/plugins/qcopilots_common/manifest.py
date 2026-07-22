"""QCopilots MCP service manifest model.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import base64
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from qcopilots_common.constants import (
    DEFAULT_CORS_ORIGINS,
    DEFAULT_HOST,
    DEFAULT_MCP_PATH,
    DEFAULT_SERVICE_PORTS,
)
from qcopilots_common.service_id import is_safe_service_id


class ManifestValidationError(ValueError):
    """Raised when a service manifest is not usable by the manager."""


@dataclass(frozen=True)
class ServiceTransport:
    type: str = "http"
    host: str = DEFAULT_HOST
    port: int = 0
    path: str = DEFAULT_MCP_PATH
    advertised_host: str = ""

    @property
    def endpoint_host(self) -> str:
        return self.advertised_host or self.host

    @property
    def endpoint_url(self) -> str:
        return f"http://{self.endpoint_host}:{self.port}{self.path}"


@dataclass(frozen=True)
class ServiceManifest:
    service_id: str
    display_name: str
    description: str
    plugin_name: str
    plugin_dir: Path
    manifest_path: Path
    transport: ServiceTransport
    entry_point: str = "server.py"
    category: str = "qcopilots"
    runtime: dict[str, Any] = field(default_factory=dict)
    capabilities: list[str] = field(default_factory=list)
    enabled: bool = True
    cors_origins: list[str] = field(default_factory=lambda: list(DEFAULT_CORS_ORIGINS))
    icon: str = ""

    @property
    def host(self) -> str:
        return self.transport.host

    @property
    def advertised_host(self) -> str:
        return self.transport.advertised_host

    @property
    def default_port(self) -> int:
        return self.transport.port

    @property
    def mcp_path(self) -> str:
        return self.transport.path

    @property
    def endpoint_url(self) -> str:
        return self.transport.endpoint_url

    @property
    def source_path(self) -> Path:
        return self.manifest_path

    @property
    def entry_path(self) -> Path:
        return self.plugin_dir / self.entry_point

    @property
    def requirements_path(self) -> Path:
        requirements = self.runtime.get("requirements", "requirements.lock")
        return self.plugin_dir / requirements

    @property
    def icon_path(self) -> Path | None:
        if not self.icon or self.icon.startswith((":/", "http://", "https://", "data:")):
            return None
        path = Path(self.icon)
        if path.is_absolute():
            return None
        try:
            plugin_dir = self.plugin_dir.resolve()
            resolved = (plugin_dir / path).resolve()
            resolved.relative_to(plugin_dir)
        except (OSError, ValueError):
            return None
        return resolved

    @property
    def icon_mime_type(self) -> str:
        if self.icon.startswith("data:") and ";" in self.icon:
            return self.icon[5 : self.icon.index(";")]
        path = self.icon_path
        suffix = path.suffix.lower() if path else Path(self.icon).suffix.lower()
        if suffix == ".svg":
            return "image/svg+xml"
        if suffix in (".jpg", ".jpeg"):
            return "image/jpeg"
        if suffix == ".webp":
            return "image/webp"
        return "image/png"

    def icon_descriptors(self) -> list[dict[str, Any]]:
        if not self.icon:
            return []
        if self.icon.startswith("data:"):
            return [
                {
                    "src": self.icon,
                    "mimeType": self.icon_mime_type,
                    "sizes": ["any"],
                }
            ]
        if self.icon.startswith(("http://", "https://")):
            return []

        path = self.icon_path
        if not path or not path.is_file():
            return []
        encoded = base64.b64encode(path.read_bytes()).decode("ascii")
        mime_type = self.icon_mime_type
        return [
            {
                "src": f"data:{mime_type};base64,{encoded}",
                "mimeType": mime_type,
                "sizes": ["any"],
            }
        ]

    def url(self, port: int | None = None) -> str:
        if port is None:
            return self.endpoint_url
        endpoint_host = self.advertised_host or self.host
        return f"http://{endpoint_host}:{port}{self.mcp_path}"


def load_service_manifest(path: str | Path) -> ServiceManifest:
    manifest_path = Path(path)
    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except Exception as err:
        raise ManifestValidationError(f"Could not read service manifest {manifest_path}: {err}") from err

    plugin_dir = manifest_path.parent
    service_id = _required_string(data, "service_id", manifest_path)
    if not is_safe_service_id(service_id):
        raise ManifestValidationError(f"Manifest {manifest_path} has unsafe service_id: {service_id}")
    display_name = _required_string(data, "display_name", manifest_path)
    description = _required_string(data, "description", manifest_path)
    transport = _transport_from_data(data, service_id, manifest_path)

    return ServiceManifest(
        service_id=service_id,
        display_name=display_name,
        description=description,
        plugin_name=data.get("plugin_name", plugin_dir.name),
        plugin_dir=plugin_dir,
        manifest_path=manifest_path,
        entry_point=data.get("entry_point", "server.py"),
        transport=transport,
        category=data.get("category", "qcopilots"),
        runtime=data.get("runtime", {}),
        capabilities=list(data.get("capabilities", [])),
        enabled=bool(data.get("enabled", True)),
        cors_origins=list(data.get("cors_origins", DEFAULT_CORS_ORIGINS)),
        icon=data.get("icon") or _metadata_value(plugin_dir, "icon") or "",
    )


def manifest_to_dict(manifest: ServiceManifest, port: int | None = None) -> dict[str, Any]:
    return {
        "service_id": manifest.service_id,
        "display_name": manifest.display_name,
        "description": manifest.description,
        "plugin_name": manifest.plugin_name,
        "plugin_dir": str(manifest.plugin_dir),
        "entry_point": str(manifest.entry_path),
        "host": manifest.host,
        "bind_host": manifest.host,
        "advertised_host": manifest.advertised_host,
        "port": port if port is not None else manifest.default_port,
        "mcp_path": manifest.mcp_path,
        "url": manifest.url(port),
        "category": manifest.category,
        "capabilities": manifest.capabilities,
        "enabled": manifest.enabled,
        "cors_origins": list(manifest.cors_origins),
        "icon": manifest.icon,
        "icon_path": str(manifest.icon_path or ""),
        "icons": manifest.icon_descriptors(),
    }


def _required_string(data: dict[str, Any], key: str, path: Path) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ManifestValidationError(f"Manifest {path} is missing required field: {key}")
    return value


def _metadata_value(plugin_dir: Path, key: str) -> str:
    metadata_path = plugin_dir / "metadata.txt"
    if not metadata_path.is_file():
        return ""
    try:
        lines = metadata_path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return ""
    prefix = f"{key}="
    for line in lines:
        if line.strip().startswith(prefix):
            return line.split("=", 1)[1].strip()
    return ""


def _transport_from_data(data: dict[str, Any], service_id: str, path: Path) -> ServiceTransport:
    transport_data = data.get("transport") or {}
    transport_type = transport_data.get("type", "http")
    if transport_type != "http":
        raise ManifestValidationError(f"Manifest {path} uses unsupported transport: {transport_type}")

    port_value = transport_data.get("port", data.get("default_port") or DEFAULT_SERVICE_PORTS.get(service_id))
    try:
        port = int(port_value)
    except (TypeError, ValueError) as err:
        raise ManifestValidationError(f"Manifest {path} does not define a valid MCP port") from err
    if port <= 0:
        raise ManifestValidationError(f"Manifest {path} does not define a valid MCP port")

    advertised_host = transport_data.get("advertised_host", data.get("advertised_host", ""))
    if advertised_host is None:
        advertised_host = ""
    if not isinstance(advertised_host, str):
        raise ManifestValidationError(f"Manifest {path} does not define a valid advertised host")

    return ServiceTransport(
        type=transport_type,
        host=transport_data.get("host", data.get("host", DEFAULT_HOST)),
        port=port,
        path=transport_data.get("path", data.get("mcp_path", DEFAULT_MCP_PATH)),
        advertised_host=advertised_host.strip(),
    )
