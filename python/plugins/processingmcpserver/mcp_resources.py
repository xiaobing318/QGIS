from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any, Callable

from qgis.core import QgsProject

REGISTERED_RESOURCE_URIS: tuple[str, ...] = (
    "qgis://project/info",
    "qgis://project/layers/summary",
)

RESOURCE_SCHEMA_VERSION = "1.0.0"


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def register_resources(mcp: Any, tools: Any) -> None:
    resource_factory = getattr(mcp, "resource", None)
    if not callable(resource_factory):
        return

    def encode(payload: dict[str, Any]) -> str:
        encoder = getattr(tools, "_resource_json", None)
        if callable(encoder):
            return encoder(payload)
        return json.dumps(payload, ensure_ascii=False, indent=2)

    def project_id() -> str:
        if hasattr(tools, "get_project_snapshot") and callable(tools.get_project_snapshot):
            try:
                snapshot = tools.get_project_snapshot()
                if isinstance(snapshot, dict):
                    file_name = str(snapshot.get("file_name") or "").strip()
                    if file_name:
                        return file_name
                    title = str(snapshot.get("title") or "").strip()
                    if title:
                        return title
            except Exception:
                pass
        project = QgsProject.instance()
        return project.fileName() or project.title() or "in-memory-project"

    def build(uri: str, supplier: Callable[[], Any]) -> str:
        payload: dict[str, Any] = {
            "ok": True,
            "uri": uri,
            "schema_version": RESOURCE_SCHEMA_VERSION,
            "generated_at": _utc_now_iso(),
            "project_id": project_id(),
        }
        try:
            payload["data"] = supplier()
        except Exception as exc:
            payload["ok"] = False
            payload["error"] = {"message": str(exc)}
        return encode(payload)

    @resource_factory("qgis://project/info")
    def qgis_project_info() -> str:
        if hasattr(tools, "get_project_snapshot") and callable(tools.get_project_snapshot):
            return build("qgis://project/info", tools.get_project_snapshot)
        return build("qgis://project/info", tools.common_get_qgis_info)

    @resource_factory("qgis://project/layers/summary")
    def qgis_project_layers_summary() -> str:
        if hasattr(tools, "get_layers_summary") and callable(tools.get_layers_summary):
            return build("qgis://project/layers/summary", tools.get_layers_summary)
        return build("qgis://project/layers/summary", lambda: {"layers": tools.layer_list()})
