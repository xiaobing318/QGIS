"""Processing MCP resource module."""

from __future__ import annotations

from typing import Any, Callable

RESOURCE_URI = "qgis://workflow/shapefile/template"
RESOURCE_DOC = (
    "用途：返回 shapefile 六阶段模板的机器可读流程定义，供模型在 llama-server WebUI 中按固定阶段调用 tools。 "
    "输入：无业务输入；由固定 URI qgis://workflow/shapefile/template 标识。 "
    "前置条件：processingmcpserver 已注册 resources 能力，且 tools 暴露了 shapefile workflow template helper。 "
    "副作用：无写操作，只读取模板快照并封装为 resource envelope。 "
    "安全控制：无。 "
    "返回结果：返回包含 workflow_stages、required_inputs、defaults、stage_tool_map 与 phases 的 JSON 文本。"
)


def register_resource(
    resource_factory: Callable[..., Any],
    tools: Any,
    build: Callable[[str, Callable[[], Any]], str],
) -> None:
    """
    作用：注册 `qgis://workflow/shapefile/template`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `qgis://workflow/shapefile/template`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP resource 注册流程中由聚合模块调用，用于挂载单个 resource 实现。
    参数与返回：
    - 参数 `resource_factory`（`Callable[..., Any]`）：注册器参数。
    - 参数 `tools`（`Any`）：工具对象，提供资源数据读取函数。
    - 参数 `build`（`Callable[[str, Callable[[], Any]], str]`）：envelope 构建函数。
    - 返回：无返回值。
    返回结果：无返回值。
    """

    @resource_factory(RESOURCE_URI, description=RESOURCE_DOC)
    def qgis_workflow_shapefile_template() -> str:
        """
        作用：处理 `qgis_workflow_shapefile_template` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `qgis_workflow_shapefile_template` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP resource 注册与读取流程中被调用，用于组织资源响应内容。
        参数与返回：
        - 参数：无。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return build(
            RESOURCE_URI,
            tools.get_shapefile_workflow_template,
        )

    qgis_workflow_shapefile_template.__doc__ = RESOURCE_DOC

