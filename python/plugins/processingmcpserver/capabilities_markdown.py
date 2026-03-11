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
    """执行 utc now iso 相关逻辑。"""
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _module_dir() -> Path:
    """执行 module dir 相关逻辑。"""
    return Path(__file__).resolve().parent


def _resolve_output_path(output_path: str | Path = "") -> Path:
    """解析 output path。"""
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
    """校验 registry。"""
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
    """执行 build section 相关逻辑。"""
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
    """执行 render markdown 相关逻辑。"""
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
    """Write MCP tools/prompts/resources markdown to the target path."""
    destination = _resolve_output_path(output_path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(_render_markdown(), encoding="utf-8")
    return destination
