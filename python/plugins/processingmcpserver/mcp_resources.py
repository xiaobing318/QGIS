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


def _resource_doc(
    purpose: str,
    inputs: str,
    preconditions: str,
    effects: str,
    safety: str,
    returns: str,
) -> str:
    """执行 resource doc 相关逻辑。"""
    return (
        f"用途：{purpose} 输入语义：{inputs} 前置条件：{preconditions} "
        f"主要副作用：{effects} 安全开关：{safety} 返回结果：{returns}"
    )


_REGISTERED_RESOURCE_DOCSTRINGS: dict[str, str] = {
    "qgis://project/info": _resource_doc(
        "返回当前 QGIS 工程的项目级摘要资源，适合作为会话初始化上下文。",
        "无业务输入；由固定 URI qgis://project/info 标识。",
        "processingmcpserver 已注册 resources 能力，且当前 QGIS 工程可访问。",
        "无写操作，只读取工程快照并封装为 resource envelope。",
        "无。",
        "返回带 generated_at、project_id、schema_version、uri、ok 和 data/error 的 JSON 文本。",
    ),
    "qgis://project/layers/summary": _resource_doc(
        "返回当前 QGIS 工程的图层摘要资源，适合在模型调用 tools 前先建立图层上下文。",
        "无业务输入；由固定 URI qgis://project/layers/summary 标识。",
        "processingmcpserver 已注册 resources 能力，且当前工程图层注册表可访问。",
        "无写操作，只读取图层摘要并封装为 resource envelope。",
        "无。",
        "返回带 generated_at、project_id、schema_version、uri、ok 和 data/error 的 JSON 文本，data 中包含图层摘要。",
    ),
}


def _validate_registered_resource_docstrings() -> None:
    """校验 registered resource docstrings。"""
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
    """执行 utc now iso 相关逻辑。"""
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def register_resources(mcp: Any, tools: Any) -> None:
    """注册 resources 能力。"""
    resource_factory = getattr(mcp, "resource", None)
    if not callable(resource_factory):
        return

    def encode(payload: dict[str, Any]) -> str:
        """编码当前负载并返回文本结果。"""
        encoder = getattr(tools, "_resource_json", None)
        if callable(encoder):
            return encoder(payload)
        return json.dumps(payload, ensure_ascii=False, indent=2)

    def project_id() -> str:
        """返回当前工程标识。"""
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
        """构建当前负载并返回结果。"""
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

    qgis_project_info_description = _REGISTERED_RESOURCE_DOCSTRINGS["qgis://project/info"]

    @resource_factory("qgis://project/info", description=qgis_project_info_description)
    def qgis_project_info() -> str:
        """执行 QGIS project info 相关逻辑。"""
        if hasattr(tools, "get_project_snapshot") and callable(tools.get_project_snapshot):
            return build("qgis://project/info", tools.get_project_snapshot)
        return build("qgis://project/info", tools.common_get_qgis_info)

    qgis_project_info.__doc__ = qgis_project_info_description

    qgis_project_layers_summary_description = _REGISTERED_RESOURCE_DOCSTRINGS[
        "qgis://project/layers/summary"
    ]

    @resource_factory(
        "qgis://project/layers/summary",
        description=qgis_project_layers_summary_description,
    )
    def qgis_project_layers_summary() -> str:
        """执行 QGIS project layers summary 相关逻辑。"""
        if hasattr(tools, "get_layers_summary") and callable(tools.get_layers_summary):
            return build("qgis://project/layers/summary", tools.get_layers_summary)
        return build("qgis://project/layers/summary", lambda: {"layers": tools.layer_list()})

    qgis_project_layers_summary.__doc__ = qgis_project_layers_summary_description
