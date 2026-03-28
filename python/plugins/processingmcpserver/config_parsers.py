from __future__ import annotations

from typing import Callable, Optional, TypeVar

from qgis.core import Qgis, QgsMessageLog

from processingmcpserver.config_types import (
    MCP_LOG_CATEGORY,
    SOURCE_DEFAULT,
    SOURCE_JSON,
    SOURCE_SETTINGS,
)

T = TypeVar("T")


def _log_warning(message: str) -> None:
    """
    作用：封装内部辅助步骤 `_log_warning`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_log_warning`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
    参数与返回：
    - 参数 `message`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：无返回值。
    返回结果：无返回值。
    """
    QgsMessageLog.logMessage(message, MCP_LOG_CATEGORY, Qgis.Warning)


def _parse_bool(value: object) -> tuple[bool, bool]:
    """
    作用：封装内部辅助步骤 `_parse_bool`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_parse_bool`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
    参数与返回：
    - 参数 `value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `tuple[bool, bool]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[bool, bool]` 类型结果，返回值语义遵循该函数实现约定。
    """
    if isinstance(value, bool):
        return True, value
    if isinstance(value, str):
        text = value.strip().lower()
        if text in ("1", "true", "yes", "on"):
            return True, True
        if text in ("0", "false", "no", "off"):
            return True, False
    if isinstance(value, (int, float)) and value in (0, 1):
        return True, bool(value)
    return False, False


def _parse_transport(value: object) -> tuple[bool, str]:
    """
    作用：封装内部辅助步骤 `_parse_transport`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_parse_transport`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
    参数与返回：
    - 参数 `value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    """
    if not isinstance(value, str):
        return False, ""
    text = value.strip().lower()
    if text in ("stdio", "sse", "streamable-http"):
        return True, text
    if text in ("streamable_http", "streamablehttp"):
        return True, "streamable-http"
    return False, ""


def _parse_string(value: object) -> tuple[bool, str]:
    """
    作用：封装内部辅助步骤 `_parse_string`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_parse_string`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
    参数与返回：
    - 参数 `value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    """
    if not isinstance(value, str):
        return False, ""
    text = value.strip()
    if not text:
        return False, ""
    return True, text


def _parse_log_level(value: object) -> tuple[bool, str]:
    """
    作用：封装内部辅助步骤 `_parse_log_level`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_parse_log_level`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
    参数与返回：
    - 参数 `value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[bool, str]` 类型结果，返回值语义遵循该函数实现约定。
    """
    ok, text = _parse_string(value)
    if not ok:
        return False, ""
    return True, text.upper()


def _parse_string_list_or_none(value: object) -> tuple[bool, Optional[list[str]]]:
    """
    作用：封装内部辅助步骤 `_parse_string_list_or_none`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_parse_string_list_or_none`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
    参数与返回：
    - 参数 `value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `tuple[bool, Optional[list[str]]]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `tuple[bool, Optional[list[str]]]` 类型结果，返回值语义遵循该函数实现约定。
    """
    if value is None:
        return True, None
    if isinstance(value, str):
        text = value.strip()
        if not text:
            return True, None
        items = [item.strip() for item in text.split(",") if item.strip()]
        return True, items or None
    if isinstance(value, (list, tuple)):
        items = [str(item).strip() for item in value if str(item).strip()]
        return True, items or None
    return False, None


def _int_parser(min_value: int, max_value: int) -> Callable[[object], tuple[bool, int]]:
    """
    作用：封装内部辅助步骤 `_int_parser`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_int_parser`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
    参数与返回：
    - 参数 `min_value`（`int`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `max_value`（`int`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `Callable[[object], tuple[bool, int]]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Callable[[object], tuple[bool, int]]` 类型结果，返回值语义遵循该函数实现约定。
    """

    def _parse(value: object) -> tuple[bool, int]:
        """
        作用：封装内部辅助步骤 `_parse`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_parse`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
        参数与返回：
        - 参数 `value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回 `tuple[bool, int]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `tuple[bool, int]` 类型结果，返回值语义遵循该函数实现约定。
        """
        if isinstance(value, bool):
            return False, 0
        try:
            parsed = int(value)
        except (TypeError, ValueError):
            return False, 0
        if parsed < min_value or parsed > max_value:
            return False, 0
        return True, parsed

    return _parse


def _resolve_value(
    key: str,
    json_value: object,
    settings_value: object,
    default_value: object,
    parser: Callable[[object], tuple[bool, T]],
    sources: dict[str, str],
) -> T:
    """
    作用：封装内部辅助步骤 `_resolve_value`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_resolve_value`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在配置解析流程中被调用，用于把原始输入转换为可用配置值。
    参数与返回：
    - 参数 `key`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `json_value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `settings_value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `default_value`（`object`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `parser`（`Callable[[object], tuple[bool, T]]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `sources`（`dict[str, str]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `T` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `T` 类型结果，返回值语义遵循该函数实现约定。
    异常：可能显式抛出 `RuntimeError`。
    """
    candidates: list[tuple[str, object]] = [
        (SOURCE_JSON, json_value),
        (SOURCE_SETTINGS, settings_value),
        (SOURCE_DEFAULT, default_value),
    ]
    for source, candidate in candidates:
        ok, parsed = parser(candidate)
        if ok:
            sources[key] = source
            return parsed
        if source != SOURCE_DEFAULT and candidate is not None:
            _log_warning(
                f"Invalid value for '{key}' from {source}: {candidate!r}. "
                "Trying lower-priority source."
            )

    ok, parsed = parser(default_value)
    if ok:
        sources[key] = SOURCE_DEFAULT
        return parsed
    raise RuntimeError(f"Default value for '{key}' is invalid: {default_value!r}")
