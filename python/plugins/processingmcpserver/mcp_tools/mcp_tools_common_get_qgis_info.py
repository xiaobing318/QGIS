"""Processing MCP tool module."""
from __future__ import annotations

import inspect
import json
import platform
import re
import shutil
import statistics
import sys
from datetime import date, datetime, time as time_value, timezone
from fnmatch import fnmatch
from pathlib import Path
from typing import Any

import processing
from processing.core.Processing import Processing
from qgis.PyQt.QtCore import (
    QDate,
    QDateTime,
    QTime,
    Qt,
    QVariant)
from qgis.core import (
    Qgis,
    QgsApplication,
    QgsExpression,
    QgsExpressionContext,
    QgsExpressionContextUtils,
    QgsFeature,
    QgsFeatureRequest,
    QgsField,
    QgsGeometry,
    QgsLayerTreeGroup,
    QgsLayerTreeLayer,
    QgsMapLayer,
    QgsCoordinateReferenceSystem,
    QgsCoordinateTransform,
    QgsProcessingParameterDefinition,
    QgsProcessingParameterEnum,
    QgsProject,
    QgsRasterLayer,
    QgsVectorLayer,
    QgsVectorFileWriter,
    QgsWkbTypes,
)
from qgis.utils import active_plugins

DEFAULT_FEATURE_LIMIT = 10
MAX_FEATURE_LIMIT = 100
DEFAULT_DATASET_LIMIT = 50
MAX_DATASET_LIMIT = 100
DEFAULT_FILESYSTEM_LIST_LIMIT = 100
MAX_FILESYSTEM_LIST_LIMIT = 200
DEFAULT_ALGORITHM_LIST_LIMIT = 30
MAX_ALGORITHM_LIST_LIMIT = 60

_PROCESSING_INITIALIZED = False

def _ensure_processing_initialized() -> None:
    """
    作用：确保 `_ensure_processing_initialized` 负责的前置状态可用，必要时执行初始化或修复动作。
    用途：确保 `_ensure_processing_initialized` 负责的前置状态可用，必要时执行初始化或修复动作。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    global _PROCESSING_INITIALIZED
    if _PROCESSING_INITIALIZED:
        return
    Processing.initialize()
    _PROCESSING_INITIALIZED = True

TOOL_NAME = 'common_get_qgis_info'
TOOL_DOC = '返回当前 QGIS Desktop 会话、平台、插件与 Processing MCP 运行状态，适合作为所有自动化流程的环境探测入口。 无业务输入。 QGIS Desktop 已启动且 processingmcpserver 插件已加载。 无写操作，只读取当前应用、项目与插件状态。 无。 返回 qgis、platform、python、active_project、active_plugins，以及 processing_mcp.filesystem.write_policy 安全摘要等环境信息。'

def common_get_qgis_info(self) -> dict[str, Any]:
    """
    作用：处理 `common_get_qgis_info` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    用途：处理 `common_get_qgis_info` 相关逻辑，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 MCP 客户端调用对应 tool 时触发，作为工具公开入口处理请求与响应。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    return self._run(self._common_get_qgis_info_impl)

def _common_get_qgis_info_impl(self) -> dict[str, Any]:
    """
    作用：实现 `_common_get_qgis_info_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    用途：实现 `_common_get_qgis_info_impl` 对应的核心处理逻辑，承担实际数据处理与结果组织。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 返回：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `dict[str, Any]` 类型结果，返回值语义遵循该函数实现约定。
    """
    project = QgsProject.instance()
    available = False
    try:
        self._ensure_processing_runtime()
        available = True
    except Exception:
        available = False
    active_project = {
        "file_name": project.fileName(),
        "title": project.title(),
        "crs": project.crs().authid() if project.crs().isValid() else "",
        "layer_count": len(project.mapLayers()),
    }
    plugin_names = sorted(active_plugins)
    return {
        "qgis": {
            "version": Qgis.version(),
            "prefix_path": QgsApplication.prefixPath(),
            "settings_dir": QgsApplication.qgisSettingsDirPath(),
        },
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
        },
        "python": {
            "version": platform.python_version(),
            "executable": sys.executable,
        },
        "project": active_project,
        "active_project": active_project,
        "active_plugins": plugin_names,
        "processing_mcp": {
            "available": available,
            "registered_tools_count": len(self.REGISTERED_TOOL_NAMES),
            "active_plugins_count": len(plugin_names),
            "filesystem": {
                "write_policy": {
                    "require_confirm_write": True,
                    "require_confirm_destructive_for_overwrite": True,
                    "require_confirm_destructive_for_delete": True,
                },
            },
        },
    }

def _ensure_processing_runtime(self) -> None:
    """
    作用：确保 `_ensure_processing_runtime` 负责的前置状态可用，必要时执行初始化或修复动作。
    用途：确保 `_ensure_processing_runtime` 负责的前置状态可用，必要时执行初始化或修复动作。
    使用场景：在 MCP 工具内部处理链路中被同模块函数串联调用，用于完成分步业务处理。
    参数与返回：
    - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    _ensure_processing_initialized()

TOOL_METHODS: dict[str, object] = {
    'common_get_qgis_info': common_get_qgis_info,
    '_common_get_qgis_info_impl': _common_get_qgis_info_impl,
    '_ensure_processing_runtime': _ensure_processing_runtime,
}
