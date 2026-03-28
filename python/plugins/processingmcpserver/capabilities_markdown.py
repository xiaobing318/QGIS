from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path

from processingmcpserver.mcp_prompts import (
    REGISTERED_PROMPT_NAMES,
    _REGISTERED_PROMPT_DOCSTRINGS,
)
from processingmcpserver.mcp_resources import (
    REGISTERED_RESOURCE_URIS,
    _REGISTERED_RESOURCE_DOCSTRINGS,
)
from processingmcpserver.mcp_tools import (
    REGISTERED_TOOL_NAMES,
    _REGISTERED_TOOL_DOCSTRINGS,
)

DEFAULT_CAPABILITIES_MARKDOWN_FILENAME = "MCP_CAPABILITIES.md"


def _utc_now_iso() -> str:
    """
    作用：封装内部辅助步骤 `_utc_now_iso`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_utc_now_iso`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在能力文档生成流程中被调用，用于导出 tools/prompts/resources 描述。
    参数与返回：
    - 参数：无。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _module_dir() -> Path:
    """
    作用：封装内部辅助步骤 `_module_dir`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_module_dir`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在能力文档生成流程中被调用，用于导出 tools/prompts/resources 描述。
    参数与返回：
    - 参数：无。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
    return Path(__file__).resolve().parent


def _resolve_output_path(output_path: str | Path = "") -> Path:
    """
    作用：封装内部辅助步骤 `_resolve_output_path`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_resolve_output_path`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在能力文档生成流程中被调用，用于导出 tools/prompts/resources 描述。
    参数与返回：
    - 参数 `output_path`（`str | Path`）：路径类参数，用于定位输入或输出文件系统位置。 默认值为 `""`。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
    if isinstance(output_path, Path):
        output_text = str(output_path)
    else:
        output_text = str(output_path or "")

    if not output_text.strip():
        return (_module_dir() / DEFAULT_CAPABILITIES_MARKDOWN_FILENAME).resolve(strict=False)

    return Path(output_text).expanduser().resolve(strict=False)


def _validate_registry(
    section_name: str,
    registered_names: tuple[str, ...],
    docstrings: dict[str, str],
) -> None:
    """
    作用：封装内部辅助步骤 `_validate_registry`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_validate_registry`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在能力文档生成流程中被调用，用于导出 tools/prompts/resources 描述。
    参数与返回：
    - 参数 `section_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `registered_names`（`tuple[str, ...]`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `docstrings`（`dict[str, str]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：无返回值。
    返回结果：无返回值。
    异常：可能显式抛出 `RuntimeError`。
    """
    missing: list[str] = []
    invalid: list[str] = []
    extra = sorted(set(docstrings) - set(registered_names))

    for registered_name in registered_names:
        description = docstrings.get(registered_name)
        if not isinstance(description, str):
            missing.append(registered_name)
            continue
        if not description.strip():
            invalid.append(registered_name)

    if missing or invalid or extra:
        raise RuntimeError(
            f"Capability registry is out of sync for {section_name}: "
            f"missing={missing}, invalid={invalid}, extra={extra}"
        )


def _build_section(
    heading: str,
    registered_names: tuple[str, ...],
    docstrings: dict[str, str],
) -> list[str]:
    """
    作用：构建 `_build_section` 相关对象或配置数据，供后续流程直接复用。
    用途：构建 `_build_section` 相关对象或配置数据，供后续流程直接复用。
    使用场景：在能力文档生成流程中被调用，用于导出 tools/prompts/resources 描述。
    参数与返回：
    - 参数 `heading`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `registered_names`（`tuple[str, ...]`）：标识或模式参数，用于指定目标对象或流程分支。
    - 参数 `docstrings`（`dict[str, str]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
    """
    lines = [f"## {heading}", ""]
    for registered_name in registered_names:
        lines.extend(
            [
                f"### `{registered_name}`",
                "",
                docstrings[registered_name].strip(),
                "",
            ]
        )
    return lines


def _render_markdown() -> str:
    """
    作用：封装内部辅助步骤 `_render_markdown`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_render_markdown`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在能力文档生成流程中被调用，用于导出 tools/prompts/resources 描述。
    参数与返回：
    - 参数：无。
    - 返回：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `str` 类型结果，返回值语义遵循该函数实现约定。
    """
    _validate_registry("tools", REGISTERED_TOOL_NAMES, _REGISTERED_TOOL_DOCSTRINGS)
    _validate_registry("prompts", REGISTERED_PROMPT_NAMES, _REGISTERED_PROMPT_DOCSTRINGS)
    _validate_registry(
        "resources",
        REGISTERED_RESOURCE_URIS,
        _REGISTERED_RESOURCE_DOCSTRINGS,
    )

    lines = [
        "# Processing MCP Capabilities",
        "",
        f"- Generated at (UTC): `{_utc_now_iso()}`",
        f"- Exporter module: `{Path(__file__).resolve()}`",
        f"- Package directory: `{_module_dir()}`",
        f"- Tool count: `{len(REGISTERED_TOOL_NAMES)}`",
        f"- Prompt count: `{len(REGISTERED_PROMPT_NAMES)}`",
        f"- Resource count: `{len(REGISTERED_RESOURCE_URIS)}`",
        "",
    ]
    lines.extend(_build_section("Tools", REGISTERED_TOOL_NAMES, _REGISTERED_TOOL_DOCSTRINGS))
    lines.extend(
        _build_section("Prompts", REGISTERED_PROMPT_NAMES, _REGISTERED_PROMPT_DOCSTRINGS)
    )
    lines.extend(
        _build_section(
            "Resources",
            REGISTERED_RESOURCE_URIS,
            _REGISTERED_RESOURCE_DOCSTRINGS,
        )
    )
    return "\n".join(lines).rstrip() + "\n"


def write_mcp_capabilities_markdown(output_path: str | Path = "") -> Path:
    """
    作用：写入 `mcp_capabilities_markdown`，完成当前函数负责的处理步骤并产出结果。
    用途：写入 `mcp_capabilities_markdown`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在能力文档生成流程中被调用，用于导出 tools/prompts/resources 描述。
    参数与返回：
    - 参数 `output_path`（`str | Path`）：路径类参数，用于定位输入或输出文件系统位置。 默认值为 `""`。
    - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
    """
    destination = _resolve_output_path(output_path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(_render_markdown(), encoding="utf-8")
    return destination
