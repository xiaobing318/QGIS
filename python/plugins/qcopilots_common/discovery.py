"""Discovery for plugin-provided QCopilots MCP services.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

from pathlib import Path

from qcopilots_common.manifest import ServiceManifest, load_service_manifest


def plugins_root_from_common() -> Path:
    return Path(__file__).resolve().parents[1]


def discover_service_manifests(
    plugins_root: str | Path | None = None,
    include_disabled: bool = False,
) -> list[ServiceManifest]:
    root = Path(plugins_root).resolve() if plugins_root else plugins_root_from_common()
    manifests: list[ServiceManifest] = []

    for path in sorted(root.glob("*/qcopilots_service.json")):
        try:
            manifest = load_service_manifest(path)
        except Exception:
            continue
        if manifest.enabled or include_disabled:
            manifests.append(manifest)

    return sorted(manifests, key=lambda manifest: manifest.service_id)
