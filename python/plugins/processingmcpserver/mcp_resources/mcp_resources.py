from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any, Callable

from qgis.core import QgsProject

from . import (
    mcp_resources_qgis_workflow_shapefile_quality_profile_default as _MOD_qgis_workflow_shapefile_quality_profile_default,
)
from . import (
    mcp_resources_qgis_workflow_shapefile_run_summary as _MOD_qgis_workflow_shapefile_run_summary,
)
from . import (
    mcp_resources_qgis_workflow_shapefile_template as _MOD_qgis_workflow_shapefile_template,
)

_RESOURCE_MODULES = [
    _MOD_qgis_workflow_shapefile_template,
    _MOD_qgis_workflow_shapefile_quality_profile_default,
    _MOD_qgis_workflow_shapefile_run_summary,
]

REGISTERED_RESOURCE_URIS: tuple[str, ...] = tuple(
    module.RESOURCE_URI for module in _RESOURCE_MODULES
)
RESOURCE_SCHEMA_VERSION = "1.0.0"

_REGISTERED_RESOURCE_DOCSTRINGS: dict[str, str] = {
    module.RESOURCE_URI: module.RESOURCE_DOC for module in _RESOURCE_MODULES
}


def _validate_registered_resource_docstrings() -> None:
    """
    作用：封装内部辅助步骤 `_validate_registered_resource_docstrings`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_validate_registered_resource_docstrings`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP resource 注册与读取流程中被调用，用于组织资源响应内容。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    异常：可能显式抛出 `RuntimeError`。
    """
    missing: list[str] = []
    invalid: list[str] = []
    extra = sorted(set(_REGISTERED_RESOURCE_DOCSTRINGS) - set(REGISTERED_RESOURCE_URIS))
    for resource_uri in REGISTERED_RESOURCE_URIS:
        description = _REGISTERED_RESOURCE_DOCSTRINGS.get(resource_uri)
        if not isinstance(description, str):
            missing.append(resource_uri)
            continue
        if not description.strip():
            invalid.append(resource_uri)
    if missing or invalid or extra:
        raise RuntimeError(
            "Failed to initialize registered MCP resource docstrings: "
            f"missing={missing}, invalid={invalid}, extra={extra}"
        )


_validate_registered_resource_docstrings()


def _utc_now_iso() -> str:
    """
    作用：封装内部辅助步骤 `_utc_now_iso`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_utc_now_iso`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP resource 注册与读取流程中被调用，用于组织资源响应内容。
    参数与返回：
    - 参数：无。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def register_resources(mcp: Any, tools: Any) -> None:
    """
    作用：注册 `resources`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `resources`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP resource 注册与读取流程中被调用，用于组织资源响应内容。
    参数与返回：
    - 参数 `mcp`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `tools`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    resource_factory = getattr(mcp, "resource", None)
    if not callable(resource_factory):
        return

    def encode(payload: dict[str, Any]) -> str:
        """
        作用：处理 `encode` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `encode` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP resource 注册与读取流程中被调用，用于组织资源响应内容。
        参数与返回：
        - 参数 `payload`（`dict[str, Any]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        encoder = getattr(tools, "_resource_json", None)
        if callable(encoder):
            return encoder(payload)
        return json.dumps(payload, ensure_ascii=False, indent=2)

    def project_id() -> str:
        """
        作用：处理 `project_id` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `project_id` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP resource 注册与读取流程中被调用，用于组织资源响应内容。
        参数与返回：
        - 参数：无。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
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
        """
        作用：构建流程，完成当前函数负责的处理步骤并产出结果。
        用途：构建流程，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP resource 注册与读取流程中被调用，用于组织资源响应内容。
        参数与返回：
        - 参数 `uri`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 参数 `supplier`（`Callable[[], Any]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
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

    for module in _RESOURCE_MODULES:
        module.register_resource(resource_factory, tools, build)

