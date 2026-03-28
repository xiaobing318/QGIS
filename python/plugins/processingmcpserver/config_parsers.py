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
    """Log a configuration warning to the QGIS message log."""
    QgsMessageLog.logMessage(message, MCP_LOG_CATEGORY, Qgis.Warning)


def _parse_bool(value: object) -> tuple[bool, bool]:
    """Parse booleans from bools, numbers, and common string forms."""
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
    """Parse the transport field and normalize it to a supported transport."""
    if not isinstance(value, str):
        return False, ""
    text = value.strip().lower()
    if text in ("stdio", "sse", "streamable-http"):
        return True, text
    if text in ("streamable_http", "streamablehttp"):
        return True, "streamable-http"
    return False, ""


def _parse_string(value: object) -> tuple[bool, str]:
    """Parse a non-empty string."""
    if not isinstance(value, str):
        return False, ""
    text = value.strip()
    if not text:
        return False, ""
    return True, text


def _parse_log_level(value: object) -> tuple[bool, str]:
    """Parse a log level and normalize it to uppercase."""
    ok, text = _parse_string(value)
    if not ok:
        return False, ""
    return True, text.upper()


def _parse_string_list_or_none(value: object) -> tuple[bool, Optional[list[str]]]:
    """Parse a string list; empty values return `None` to disable the setting."""
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
    """Build an integer parser constrained to the given range."""

    def _parse(value: object) -> tuple[bool, int]:
        """Parse one value as an integer and validate its range."""
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
    """Resolve a configuration value with JSON > Settings > Default precedence and record the source."""
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
