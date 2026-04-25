from __future__ import annotations

from typing import Any

from . import (
    mcp_prompts_qgis_shapefile_buffer_join_workflow as _MOD_qgis_shapefile_buffer_join_workflow,
)
from . import (
    mcp_prompts_qgis_shapefile_overlay_clip_stats_workflow as _MOD_qgis_shapefile_overlay_clip_stats_workflow,
)
from . import (
    mcp_prompts_qgis_shapefile_quality_repair_export_workflow as _MOD_qgis_shapefile_quality_repair_export_workflow,
)

_PROMPT_MODULES = [
    _MOD_qgis_shapefile_quality_repair_export_workflow,
    _MOD_qgis_shapefile_overlay_clip_stats_workflow,
    _MOD_qgis_shapefile_buffer_join_workflow,
]

REGISTERED_PROMPT_NAMES: tuple[str, ...] = tuple(
    module.PROMPT_NAME for module in _PROMPT_MODULES
)

_REGISTERED_PROMPT_DOCSTRINGS: dict[str, str] = {
    module.PROMPT_NAME: module.PROMPT_DOC for module in _PROMPT_MODULES
}


def _validate_registered_prompt_docstrings() -> None:
    """
    作用：封装内部辅助步骤 `_validate_registered_prompt_docstrings`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_validate_registered_prompt_docstrings`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 MCP prompt 注册与调用流程中被调用，用于返回提示词模板内容。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    异常：可能显式抛出 `RuntimeError`。
    """
    missing: list[str] = []
    invalid: list[str] = []
    extra = sorted(set(_REGISTERED_PROMPT_DOCSTRINGS) - set(REGISTERED_PROMPT_NAMES))
    for prompt_name in REGISTERED_PROMPT_NAMES:
        description = _REGISTERED_PROMPT_DOCSTRINGS.get(prompt_name)
        if not isinstance(description, str):
            missing.append(prompt_name)
            continue
        if not description.strip():
            invalid.append(prompt_name)
    if missing or invalid or extra:
        raise RuntimeError(
            "Failed to initialize registered MCP prompt docstrings: "
            f"missing={missing}, invalid={invalid}, extra={extra}"
        )


_validate_registered_prompt_docstrings()


def register_prompts(mcp: Any, tools: Any) -> None:
    """
    作用：注册 `prompts`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `prompts`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP prompt 注册与调用流程中被调用，用于返回提示词模板内容。
    参数与返回：
    - 参数 `mcp`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `tools`（`Any`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    prompt_factory = getattr(mcp, "prompt", None)
    if not callable(prompt_factory):
        return

    workflow_template: dict[str, Any] = {}
    quality_profile: dict[str, Any] = {}
    if hasattr(tools, "get_shapefile_workflow_template") and callable(
        tools.get_shapefile_workflow_template
    ):
        try:
            workflow_template = tools.get_shapefile_workflow_template()
        except Exception:
            workflow_template = {}
    if hasattr(tools, "get_shapefile_quality_profile") and callable(
        tools.get_shapefile_quality_profile
    ):
        try:
            quality_profile = tools.get_shapefile_quality_profile()
        except Exception:
            quality_profile = {}

    for module in _PROMPT_MODULES:
        module.register_prompt(prompt_factory, workflow_template, quality_profile)

