"""Processing MCP resource module."""

from __future__ import annotations

from typing import Any, Callable

RESOURCE_URI = "qgis://workflow/shapefile/quality-profile/default"
RESOURCE_DOC = (
    "用途：返回 shapefile 默认质量规则，供模型在预检、标准化和导出阶段复用。 "
    "输入：无业务输入；由固定 URI qgis://workflow/shapefile/quality-profile/default 标识。 "
    "前置条件：processingmcpserver 已注册 resources 能力，且 tools 暴露了 shapefile quality profile helper。 "
    "副作用：无写操作，只读取默认质量规则并封装为 resource envelope。 "
    "安全控制：无。 "
    "返回结果：返回包含 quality_checks、blocking_rules、warning_rules、export_constraints 和 confirmation_policy 的 JSON 文本。"
)


def register_resource(
    resource_factory: Callable[..., Any],
    tools: Any,
    build: Callable[[str, Callable[[], Any]], str],
) -> None:
    """
    作用：注册 `qgis://workflow/shapefile/quality-profile/default`，完成当前函数负责的处理步骤并产出结果。
    用途：注册 `qgis://workflow/shapefile/quality-profile/default`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP resource 注册流程中由聚合模块调用，用于挂载单个 resource 实现。
    参数与返回：
    - 参数 `resource_factory`（`Callable[..., Any]`）：注册器参数。
    - 参数 `tools`（`Any`）：工具对象，提供资源数据读取函数。
    - 参数 `build`（`Callable[[str, Callable[[], Any]], str]`）：envelope 构建函数。
    - 返回：无返回值。
    返回结果：无返回值。
    """

    @resource_factory(RESOURCE_URI, description=RESOURCE_DOC)
    def qgis_workflow_shapefile_quality_profile_default() -> str:
        """
        作用：处理 `qgis_workflow_shapefile_quality_profile_default` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        用途：处理 `qgis_workflow_shapefile_quality_profile_default` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
        使用场景：在 MCP resource 注册与读取流程中被调用，用于组织资源响应内容。
        参数与返回：
        - 参数：无。
        - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
        """
        return build(
            RESOURCE_URI,
            tools.get_shapefile_quality_profile,
        )

    qgis_workflow_shapefile_quality_profile_default.__doc__ = RESOURCE_DOC

