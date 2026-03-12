from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any, Callable

from qgis.core import QgsProject

REGISTERED_RESOURCE_URIS: tuple[str, ...] = (
    "qgis://workflow/shapefile/template",
    "qgis://workflow/shapefile/quality-profile/default",
    "qgis://workflow/shapefile/run-summary",
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
    "qgis://workflow/shapefile/template": _resource_doc(
        "返回 shapefile 六阶段模板的机器可读流程定义，供模型在 llama-server WebUI 中按固定阶段调用 tools。",
        "无业务输入；由固定 URI qgis://workflow/shapefile/template 标识。",
        "processingmcpserver 已注册 resources 能力，且 tools 暴露了 shapefile workflow template helper。",
        "无写操作，只读取模板快照并封装为 resource envelope。",
        "无。",
        "返回包含 workflow_stages、required_inputs、defaults、stage_tool_map 与 phases 的 JSON 文本。",
    ),
    "qgis://workflow/shapefile/quality-profile/default": _resource_doc(
        "返回 shapefile 默认质量规则，供模型在预检、标准化和导出阶段复用。",
        "无业务输入；由固定 URI qgis://workflow/shapefile/quality-profile/default 标识。",
        "processingmcpserver 已注册 resources 能力，且 tools 暴露了 shapefile quality profile helper。",
        "无写操作，只读取默认质量规则并封装为 resource envelope。",
        "无。",
        "返回包含 quality_checks、blocking_rules、warning_rules、export_constraints 和 confirmation_policy 的 JSON 文本。",
    ),
    "qgis://workflow/shapefile/run-summary": _resource_doc(
        "返回最近一次 shapefile 工作流运行的摘要 manifest，便于模型续跑、审计和导出后清理。",
        "无业务输入；由固定 URI qgis://workflow/shapefile/run-summary 标识。",
        "processingmcpserver 已注册 resources 能力，且当前会话中已初始化过 shapefile run summary。",
        "无写操作，只读取最近一次运行摘要并封装为 resource envelope。",
        "无。",
        "返回包含 task_name、status、inputs、steps、warnings、outputs 和 generated_at 的 JSON 文本。",
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

    qgis_shapefile_template_description = _REGISTERED_RESOURCE_DOCSTRINGS[
        "qgis://workflow/shapefile/template"
    ]

    @resource_factory(
        "qgis://workflow/shapefile/template",
        description=qgis_shapefile_template_description,
    )
    def qgis_workflow_shapefile_template() -> str:
        """执行 QGIS workflow shapefile template 相关逻辑。"""
        return build(
            "qgis://workflow/shapefile/template",
            tools.get_shapefile_workflow_template,
        )

    qgis_workflow_shapefile_template.__doc__ = qgis_shapefile_template_description

    qgis_shapefile_quality_profile_description = _REGISTERED_RESOURCE_DOCSTRINGS[
        "qgis://workflow/shapefile/quality-profile/default"
    ]

    @resource_factory(
        "qgis://workflow/shapefile/quality-profile/default",
        description=qgis_shapefile_quality_profile_description,
    )
    def qgis_workflow_shapefile_quality_profile_default() -> str:
        """执行 QGIS workflow shapefile quality profile default 相关逻辑。"""
        return build(
            "qgis://workflow/shapefile/quality-profile/default",
            tools.get_shapefile_quality_profile,
        )

    qgis_workflow_shapefile_quality_profile_default.__doc__ = (
        qgis_shapefile_quality_profile_description
    )

    qgis_shapefile_run_summary_description = _REGISTERED_RESOURCE_DOCSTRINGS[
        "qgis://workflow/shapefile/run-summary"
    ]

    @resource_factory(
        "qgis://workflow/shapefile/run-summary",
        description=qgis_shapefile_run_summary_description,
    )
    def qgis_workflow_shapefile_run_summary() -> str:
        """执行 QGIS workflow shapefile run summary 相关逻辑。"""
        return build(
            "qgis://workflow/shapefile/run-summary",
            tools.get_shapefile_run_summary,
        )

    qgis_workflow_shapefile_run_summary.__doc__ = qgis_shapefile_run_summary_description
