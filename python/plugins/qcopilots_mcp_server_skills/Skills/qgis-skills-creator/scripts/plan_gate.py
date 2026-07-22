"""Read-only review, render, and staged-package gate for QGIS skill plans.

The gate uses only the Python standard library.  It never creates, updates,
moves, or deletes a skill.  User approval and every later tool invocation stay
subject to the current host's permission workflow.
"""

from __future__ import annotations

import argparse
import copy
import datetime as dt
import hashlib
import json
import os
import posixpath
import re
import stat
import sys
import tempfile
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import (
    parse_qsl,
    quote,
    unquote,
    urlencode,
    urlsplit,
    urlunsplit,
)

ZERO_HASH = "sha256:" + "0" * 64
SHA256_RE = re.compile(r"^sha256:[a-f0-9]{64}$")
SKILL_NAME_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
RISK_RANK = {"R0": 0, "R1": 1, "R2": 2, "R3": 3}
APPROVED_OR_LATER_STATUSES = frozenset(
    {"approved", "implementing", "validating", "completed"}
)
PLAN_HASH_EXCLUDED = {
    "$schema",
    "status",
    "created_at",
    "updated_at",
    "plan_hash",
    "approval",
}
CAPABILITY_HASH_EXCLUDED = {"discovered_at", "snapshot_hash", "notes"}
REVIEW_EXCLUDED = {"$schema", "status", "created_at", "updated_at", "approval"}
SUPPORTED_SCHEMA_KEYWORDS = {
    "$schema",
    "$id",
    "$defs",
    "$ref",
    "title",
    "description",
    "type",
    "additionalProperties",
    "required",
    "properties",
    "const",
    "enum",
    "minLength",
    "maxLength",
    "pattern",
    "format",
    "minimum",
    "maximum",
    "minItems",
    "maxItems",
    "uniqueItems",
    "items",
    "oneOf",
    "allOf",
    "if",
    "then",
    "else",
    "default",
}
WINDOWS_RESERVED = {
    "CON",
    "PRN",
    "AUX",
    "NUL",
    *{f"COM{number}" for number in range(1, 10)},
    *{f"LPT{number}" for number in range(1, 10)},
}
LOCAL_PATH_TARGET_KINDS = frozenset(
    {"input-path", "output-path", "file-read", "file-write", "cwd", "executable"}
)
INPUT_PATH_TARGET_KINDS = frozenset({"input-path", "file-read"})
OUTPUT_PATH_TARGET_KINDS = frozenset({"output-path", "file-write"})
NESTED_EFFECT_TARGET_KINDS = frozenset(
    {
        "input-path",
        "output-path",
        "file-read",
        "file-write",
        "qgis-state",
        "network-endpoint",
        "database",
        "credential",
        "device",
        "executable",
    }
)
NESTED_TARGET_KINDS = frozenset({"none", *NESTED_EFFECT_TARGET_KINDS})
ABSOLUTE_URI_SCHEME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:")
DATABASE_URI_SCHEMES = frozenset(
    {"mariadb", "mssql", "mysql", "oracle", "postgres", "postgresql", "sqlite"}
)
NETWORK_URI_SCHEMES = frozenset({"ftp", "ftps", "http", "https", "s3", "ws", "wss"})
LOCAL_FILE_URI_AUTHORITIES = frozenset({"", "localhost", "127.0.0.1", "[::1]", "::1"})
DOMAIN_POLICY_FIELDS = {
    "vector": "geometry_policy",
    "raster": "raster_policy",
    "point-cloud": "point_cloud_policy",
    "mesh": "mesh_policy",
    "three-dimensional": "three_d_policy",
    "geospatial-service": "service_policy",
    "spatiotemporal": "temporal_policy",
    "tabular": "tabular_policy",
}
NOT_APPLICABLE_RE = re.compile(
    r"(?i)^\s*(?:n\s*/?\s*a\b|not[\s-]*applicable\b|不适用)"
)
LIKELY_LOCAL_PATH_SUFFIXES = frozenset(
    {
        ".csv",
        ".db",
        ".fgb",
        ".geojson",
        ".gpkg",
        ".json",
        ".las",
        ".laz",
        ".qgz",
        ".shp",
        ".sqlite",
        ".tif",
        ".tiff",
        ".vrt",
    }
)
MARKDOWN_LINK_RE = re.compile(
    r"!?\[[^\]]*\]\(\s*(?:<([^>]+)>|([^\s)]+))(?:\s+[^)]*)?\)"
)
MARKDOWN_REFERENCE_RE = re.compile(
    r"(?m)^[ \t]{0,3}\[[^\]]+\]:\s*(?:<([^>]+)>|([^\s]+))"
)
TEMPLATE_TEXT_RE = re.compile(
    r"(?i)(?:\bTODO\b|\bTBD\b|\bFIXME\b|\bXXX\b|\{\{|\}\}|"
    r"replace with|pending-qgis-workflow|pending-runtime-probe|待确认|待定义|替换为)"
)
OBVIOUS_MACHINE_PATH_RE = re.compile(
    r"(?i)(?:(?<![a-z0-9])[a-z]:[\\/]|"
    r"(?<![a-z0-9:])/(?:opt|usr|var|srv|home|users|mnt|data|src|workspace)"
    r"(?:/|$))"
)


def _unc_server_component(value: str) -> bool:
    """Accept DNS/NetBIOS-like server names, including Unicode and WSL's `$`."""
    return bool(value) and all(
        character.isalnum() or character in {"-", ".", "_", "$"} for character in value
    )


def _unc_share_component(value: str) -> bool:
    """Require a concrete share component, not regex or string-escape syntax."""
    forbidden = frozenset('<>:"/\\|?*')
    return bool(value) and all(
        not character.isspace()
        and (ord(character) >= 32 and ord(character) != 127)
        and character not in forbidden
        for character in value
    )


def _text_component(text: str, start: int, separator: str) -> tuple[str, int]:
    terminators = frozenset({'"', "'", ",", ";", ")", "}"})
    end = start
    while end < len(text):
        character = text[end]
        if character == separator or character.isspace() or character in terminators:
            break
        end += 1
    return text[start:end], end


def _backslash_unc_at(text: str, index: int) -> bool:
    prefix_end = index
    while prefix_end < len(text) and text[prefix_end] == "\\":
        prefix_end += 1
    if prefix_end - index < 2:
        return False

    cursor = prefix_end
    if cursor < len(text) and text[cursor] == "?":
        cursor += 1
        separator_end = cursor
        while separator_end < len(text) and text[separator_end] == "\\":
            separator_end += 1
        if (
            separator_end == cursor
            or text[separator_end : separator_end + 3].casefold() != "unc"
        ):
            return False
        cursor = separator_end + 3
        separator_end = cursor
        while separator_end < len(text) and text[separator_end] == "\\":
            separator_end += 1
        if separator_end == cursor:
            return False
        cursor = separator_end

    server, server_end = _text_component(text, cursor, "\\")
    if not _unc_server_component(server):
        return False
    share_start = server_end
    while share_start < len(text) and text[share_start] == "\\":
        share_start += 1
    if share_start == server_end:
        return False
    share, _ = _text_component(text, share_start, "\\")
    return _unc_share_component(share)


def _uri_scheme_before_authority(text: str, authority_index: int) -> str | None:
    if authority_index == 0 or text[authority_index - 1] != ":":
        return None
    scheme_start = authority_index - 1
    while scheme_start and (
        text[scheme_start - 1].isalnum() or text[scheme_start - 1] in {"+", "-", "."}
    ):
        scheme_start -= 1
    scheme = text[scheme_start : authority_index - 1]
    if (
        not scheme
        or not scheme[0].isalpha()
        or not ABSOLUTE_URI_SCHEME_RE.match(scheme + ":")
    ):
        return None
    return scheme.casefold()


def _forward_unc_at(text: str, index: int) -> bool:
    scheme = _uri_scheme_before_authority(text, index)
    if scheme is not None and scheme != "file":
        return False
    if scheme is None and index and text[index - 1] == "/":
        return False

    server, server_end = _text_component(text, index + 2, "/")
    share_start = server_end
    while share_start < len(text) and text[share_start] == "/":
        share_start += 1
    if share_start == server_end:
        return False
    share, _ = _text_component(text, share_start, "/")
    if scheme == "file" and server.casefold() in LOCAL_FILE_URI_AUTHORITIES:
        return False
    if scheme == "file":
        return bool(server) and _unc_share_component(share)
    return _unc_server_component(server) and _unc_share_component(share)


def contains_unc_prefix(text: str) -> bool:
    """Recognize syntactically complete server/share UNC spellings in text."""
    index = 0
    while index < len(text):
        if text[index] == "\\":
            run_end = index
            while run_end < len(text) and text[run_end] == "\\":
                run_end += 1
            if run_end - index >= 2 and _backslash_unc_at(text, index):
                return True
            index = run_end
            continue
        if text[index] == "/":
            run_end = index
            while run_end < len(text) and text[run_end] == "/":
                run_end += 1
            if run_end - index >= 2 and _forward_unc_at(text, index):
                return True
            index = run_end
            continue
        index += 1
    return False


def contains_obvious_machine_path(text: str) -> bool:
    return OBVIOUS_MACHINE_PATH_RE.search(text) is not None or contains_unc_prefix(text)


class GateInputError(ValueError):
    """Raised when a gate input is malformed or ambiguous."""


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise GateInputError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_nonfinite(value: str) -> None:
    raise GateInputError(f"non-finite JSON number is forbidden: {value}")


def load_strict_json(path: Path) -> Any:
    raw = path.read_bytes()
    if raw.startswith(b"\xef\xbb\xbf"):
        raise GateInputError(f"{path}: UTF-8 BOM is forbidden")
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        raise GateInputError(f"{path}: not valid UTF-8: {error}") from error
    try:
        return json.loads(
            text,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_nonfinite,
        )
    except (json.JSONDecodeError, GateInputError) as error:
        raise GateInputError(f"{path}: invalid strict JSON: {error}") from error


def canonical_bytes(value: Any) -> bytes:
    try:
        text = json.dumps(
            value,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        )
    except (TypeError, ValueError) as error:
        raise GateInputError(f"value cannot be canonicalized: {error}") from error
    return text.encode("utf-8")


def sha256_value(value: Any) -> str:
    return "sha256:" + hashlib.sha256(canonical_bytes(value)).hexdigest()


def sha256_bytes(value: bytes) -> str:
    return "sha256:" + hashlib.sha256(value).hexdigest()


def sha256_text(value: str) -> str:
    return sha256_bytes(value.encode("utf-8"))


def _resolve_local_ref(root_schema: dict[str, Any], reference: str) -> Any:
    if not reference.startswith("#/"):
        raise GateInputError(
            f"only local JSON Schema references are supported: {reference}"
        )
    current: Any = root_schema
    for raw_part in reference[2:].split("/"):
        part = raw_part.replace("~1", "/").replace("~0", "~")
        if not isinstance(current, dict) or part not in current:
            raise GateInputError(f"unresolved JSON Schema reference: {reference}")
        current = current[part]
    return current


def _matches_type(value: Any, expected: str) -> bool:
    if expected == "null":
        return value is None
    if expected == "boolean":
        return isinstance(value, bool)
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if expected == "string":
        return isinstance(value, str)
    if expected == "array":
        return isinstance(value, list)
    if expected == "object":
        return isinstance(value, dict)
    raise GateInputError(f"unsupported JSON Schema type: {expected}")


def audit_schema_keywords(
    schema: Any, path: str = "$schema", errors: list[str] | None = None
) -> list[str]:
    if errors is None:
        errors = []
    if isinstance(schema, bool):
        return errors
    if not isinstance(schema, dict):
        errors.append(f"{path}: schema node must be an object or boolean")
        return errors
    unknown = sorted(set(schema) - SUPPORTED_SCHEMA_KEYWORDS)
    if unknown:
        errors.append(f"{path}: unsupported schema keywords {unknown}")
    for key in ("properties", "$defs"):
        children = schema.get(key)
        if isinstance(children, dict):
            for name, child in children.items():
                audit_schema_keywords(child, f"{path}/{key}/{name}", errors)
    for key in ("items", "additionalProperties", "if", "then", "else"):
        child = schema.get(key)
        if isinstance(child, (dict, bool)):
            audit_schema_keywords(child, f"{path}/{key}", errors)
    for key in ("oneOf", "allOf"):
        children = schema.get(key)
        if isinstance(children, list):
            for index, child in enumerate(children):
                audit_schema_keywords(child, f"{path}/{key}/{index}", errors)
    return errors


def validate_schema_subset(
    instance: Any,
    schema: Any,
    *,
    root_schema: dict[str, Any] | None = None,
    path: str = "$",
    errors: list[str] | None = None,
) -> list[str]:
    """Validate the Draft 2020-12 subset used by the bundled schemas."""
    if errors is None:
        errors = []
    if root_schema is None:
        if not isinstance(schema, dict):
            raise GateInputError("root JSON Schema must be an object")
        root_schema = schema
        errors.extend(audit_schema_keywords(schema))
    if isinstance(schema, bool):
        if not schema:
            errors.append(f"{path}: rejected by false schema")
        return errors
    if not isinstance(schema, dict):
        raise GateInputError(f"{path}: schema node must be an object or boolean")

    reference = schema.get("$ref")
    if reference is not None:
        validate_schema_subset(
            instance,
            _resolve_local_ref(root_schema, reference),
            root_schema=root_schema,
            path=path,
            errors=errors,
        )
    if "const" in schema and instance != schema["const"]:
        errors.append(f"{path}: value does not equal const")
    if "enum" in schema and instance not in schema["enum"]:
        errors.append(f"{path}: value is not in enum")

    expected_type = schema.get("type")
    if expected_type is not None:
        candidates = (
            expected_type if isinstance(expected_type, list) else [expected_type]
        )
        if not any(_matches_type(instance, candidate) for candidate in candidates):
            errors.append(f"{path}: type does not match {candidates}")
            return errors

    if isinstance(instance, str):
        if len(instance) < schema.get("minLength", 0):
            errors.append(f"{path}: shorter than minLength")
        if "maxLength" in schema and len(instance) > schema["maxLength"]:
            errors.append(f"{path}: longer than maxLength")
        if "pattern" in schema and re.search(schema["pattern"], instance) is None:
            errors.append(f"{path}: does not match pattern")
        if schema.get("format") == "date-time":
            try:
                parsed = dt.datetime.fromisoformat(instance.replace("Z", "+00:00"))
                if parsed.tzinfo is None:
                    raise ValueError("timezone is required")
            except ValueError:
                errors.append(f"{path}: invalid date-time")

    if isinstance(instance, (int, float)) and not isinstance(instance, bool):
        if "minimum" in schema and instance < schema["minimum"]:
            errors.append(f"{path}: below minimum")
        if "maximum" in schema and instance > schema["maximum"]:
            errors.append(f"{path}: above maximum")

    if isinstance(instance, list):
        if len(instance) < schema.get("minItems", 0):
            errors.append(f"{path}: fewer than minItems")
        if "maxItems" in schema and len(instance) > schema["maxItems"]:
            errors.append(f"{path}: more than maxItems")
        if schema.get("uniqueItems"):
            encoded = [canonical_bytes(item) for item in instance]
            if len(encoded) != len(set(encoded)):
                errors.append(f"{path}: array items are not unique")
        if "items" in schema:
            for index, item in enumerate(instance):
                validate_schema_subset(
                    item,
                    schema["items"],
                    root_schema=root_schema,
                    path=f"{path}/{index}",
                    errors=errors,
                )

    if isinstance(instance, dict):
        for key in schema.get("required", []):
            if key not in instance:
                errors.append(f"{path}: missing required property {key}")
        properties = schema.get("properties", {})
        for key, child in properties.items():
            if key in instance:
                validate_schema_subset(
                    instance[key],
                    child,
                    root_schema=root_schema,
                    path=f"{path}/{key}",
                    errors=errors,
                )
        extras = set(instance) - set(properties)
        additional = schema.get("additionalProperties", True)
        if additional is False:
            for key in sorted(extras):
                errors.append(f"{path}: additional property {key} is forbidden")
        elif isinstance(additional, dict):
            for key in extras:
                validate_schema_subset(
                    instance[key],
                    additional,
                    root_schema=root_schema,
                    path=f"{path}/{key}",
                    errors=errors,
                )

    for child in schema.get("allOf", []):
        validate_schema_subset(
            instance,
            child,
            root_schema=root_schema,
            path=path,
            errors=errors,
        )
    if "oneOf" in schema:
        matches = 0
        for child in schema["oneOf"]:
            branch_errors: list[str] = []
            validate_schema_subset(
                instance,
                child,
                root_schema=root_schema,
                path=path,
                errors=branch_errors,
            )
            if not branch_errors:
                matches += 1
        if matches != 1:
            errors.append(f"{path}: oneOf matched {matches} branches")
    if "if" in schema:
        probe_errors: list[str] = []
        validate_schema_subset(
            instance,
            schema["if"],
            root_schema=root_schema,
            path=path,
            errors=probe_errors,
        )
        branch = schema.get("then") if not probe_errors else schema.get("else")
        if branch is not None:
            validate_schema_subset(
                instance,
                branch,
                root_schema=root_schema,
                path=path,
                errors=errors,
            )
    return errors


def compute_catalog_hash(catalog: dict[str, Any]) -> str:
    projection = copy.deepcopy(catalog)
    projection.pop("$schema", None)
    return sha256_value(projection)


def compute_capability_hash(snapshot: dict[str, Any]) -> str:
    projection = {
        key: copy.deepcopy(value)
        for key, value in snapshot.items()
        if key not in CAPABILITY_HASH_EXCLUDED
    }
    return sha256_value(projection)


def compute_plan_hash(plan: dict[str, Any]) -> str:
    projection = {
        key: copy.deepcopy(value)
        for key, value in plan.items()
        if key not in PLAN_HASH_EXCLUDED
    }
    snapshot = plan.get("capability_snapshot")
    if isinstance(snapshot, dict):
        projection["capability_snapshot"] = {
            "snapshot_hash": snapshot.get("snapshot_hash")
        }
    return sha256_value(projection)


def review_projection(plan: dict[str, Any]) -> dict[str, Any]:
    return {
        key: copy.deepcopy(value)
        for key, value in plan.items()
        if key not in REVIEW_EXCLUDED
    }


def render_review_markdown(plan: dict[str, Any]) -> str:
    projection = review_projection(plan)
    identity = plan.get("skill_identity", {})
    package = plan.get("target_skill_package", {})
    snapshot = plan.get("capability_snapshot", {})
    plan_id = plan.get("plan_id")
    revision = plan.get("revision")
    lines = [
        "# QGIS 技能创建计划审查",
        "",
        "> 此文档由权威计划 JSON 确定性渲染。请只批准这份完整输出。",
        "",
        "## A. 计划标识",
        "",
        f"- Plan ID: {plan_id}",
        f"- Revision: {revision}",
        "- Review status: under-review",
        f"- Plan hash: {plan.get('plan_hash')}",
        f"- Capability hash: {snapshot.get('snapshot_hash')}",
        f"- Catalog hash: {snapshot.get('catalog_hash')}",
        f"- Target skill: {identity.get('name')}",
        f"- Target relative path: {package.get('target_relative_path')}",
        "",
    ]
    sections = [
        ("B. 需求理解", ["request_summary", "facts", "scope"]),
        ("C. 技能触发与边界", ["skill_identity", "usage_scenarios"]),
        (
            "D. 数据契约和地理语义",
            ["input_contracts", "output_contracts", "geospatial_semantics"],
        ),
        ("E. 环境与工具发现快照", ["environment_contract", "capability_snapshot"]),
        ("F. 选中工具", ["selected_tools", "known_issue_acceptances"]),
        ("G. 工作流编排", ["workflow_nodes"]),
        ("H-I. 目标技能包与文件说明", ["target_skill_package"]),
        ("J. 权限和副作用", ["permissions"]),
        ("K. 验证矩阵", ["validation_matrix"]),
        ("L. 风险、替代和未解决决定", ["risks", "unresolved_decisions"]),
        ("M. 修订日志", ["decision_log"]),
    ]
    for title, keys in sections:
        content = {key: projection.get(key) for key in keys}
        lines.extend(
            [
                f"## {title}",
                "",
                "~~~json",
                json.dumps(
                    content,
                    ensure_ascii=False,
                    sort_keys=True,
                    indent=2,
                    allow_nan=False,
                ),
                "~~~",
                "",
            ]
        )
    lines.extend(
        [
            "## N. 明确审批请求",
            "",
            f"当前为计划 {plan_id} revision {revision}。",
            "在您明确批准前，不会创建或修改目标技能。",
            f"若计划正确，请明确回复：批准计划 {plan_id} revision {revision}，按该计划创建。",
            "如需调整，请指出条目；Creator 必须生成新的 revision 并重新提交。",
            "",
            "## 权威审查投影",
            "",
            "以下 JSON 覆盖所有可批准字段。生命周期时间、状态和批准审计副本被排除。",
            "",
            "~~~json",
            json.dumps(
                projection,
                ensure_ascii=False,
                sort_keys=True,
                indent=2,
                allow_nan=False,
            ),
            "~~~",
            "",
        ]
    )
    return "\n".join(lines)


def is_relative_package_path(value: Any) -> bool:
    if not isinstance(value, str) or not value or "\x00" in value:
        return False
    if "\\" in value or value.startswith("/") or re.match(r"^[A-Za-z]:", value):
        return False
    if any(character in value for character in ':*?"<>|'):
        return False
    parts = value.split("/")
    if any(part in {"", ".", ".."} for part in parts):
        return False
    for part in parts:
        stem = part.rstrip(" .").split(".", 1)[0].upper()
        if part.endswith((" ", ".")) or stem in WINDOWS_RESERVED:
            return False
    return str(PurePosixPath(value)) == value


def is_reparse_point(path: Path) -> bool:
    try:
        if path.is_symlink():
            return True
        is_junction = getattr(os.path, "isjunction", None)
        if is_junction is not None and is_junction(path):
            return True
        attributes = getattr(path.lstat(), "st_file_attributes", 0)
        return bool(attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))
    except OSError:
        return False


def existing_ancestors(path: Path) -> list[Path]:
    result: list[Path] = []
    current = path
    while True:
        if os.path.lexists(current):
            result.append(current)
        if current.parent == current:
            return result
        current = current.parent


def is_within(child: Path, parent: Path) -> bool:
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def normalized_resolved_path(path: Path) -> str:
    return str(path.resolve(strict=False)).replace("\\", "/").casefold()


def compute_skills_root_identity(manifest_hash: str, skills_root: Path) -> str:
    return sha256_value(
        {
            "manifest_hash": manifest_hash,
            "skills_root": normalized_resolved_path(skills_root),
        }
    )


def _strip_endpoint_decorations(value: str) -> str:
    candidate = value.strip()
    if "|" in candidate:
        candidate = candidate.split("|", 1)[0].strip()
    upper = candidate.upper()
    for prefix in ("GPKG:", "OGR:"):
        if upper.startswith(prefix):
            return candidate[len(prefix) :].strip()
    if upper.startswith("SQLITE:"):
        provider_value = candidate[len("SQLITE:") :].strip()
        if re.match(r"^[A-Za-z]:[\\/]", provider_value) or provider_value.startswith(
            "\\\\"
        ):
            return provider_value
    return candidate


def lexical_local_path(value: Any) -> Path | None:
    """Return an absolute lexical local path without resolving reparse points."""
    if not isinstance(value, str) or not value.strip():
        raise ValueError("endpoint must be a non-empty string")
    candidate = _strip_endpoint_decorations(value)
    local_path: str | None = None
    if re.match(r"^[A-Za-z]:[\\/]", candidate) or candidate.startswith(
        ("/", "\\\\", "//")
    ):
        local_path = candidate
    else:
        try:
            parts = urlsplit(candidate)
        except ValueError as error:
            raise ValueError(f"invalid provider URI: {error}") from error
        if parts.scheme.casefold() != "file":
            return None
        local_path = unquote(parts.path)
        if re.match(r"^/[A-Za-z]:/", local_path):
            local_path = local_path[1:]
        if parts.netloc and parts.netloc.casefold() not in {
            "localhost",
            "127.0.0.1",
            "[::1]",
            "::1",
        }:
            local_path = f"//{parts.netloc}{local_path}"
    if not (
        re.match(r"^[A-Za-z]:[\\/]", local_path)
        or local_path.startswith(("/", "\\\\", "//"))
    ):
        raise ValueError("local data path must be absolute")
    native = local_path.replace("/", os.sep).replace("\\", os.sep)
    return Path(native).expanduser().absolute()


def _validate_lexical_path_no_reparse(
    value: Any,
    label: str,
    errors: list[str],
    *,
    reparse_checker: Any = None,
) -> Path | None:
    """Reject a reparse leaf or existing lexical ancestor before resolution."""
    try:
        lexical_path = lexical_local_path(value)
    except (OSError, ValueError) as error:
        errors.append(f"{label}: invalid lexical path: {error}")
        return None
    if lexical_path is None:
        return None
    checker = reparse_checker or is_reparse_point
    for ancestor in existing_ancestors(lexical_path):
        if checker(ancestor):
            errors.append(f"{label}: lexical path contains a reparse point: {ancestor}")
    return lexical_path


def canonical_local_endpoint(value: str) -> str:
    lexical_path = lexical_local_path(value)
    if lexical_path is None:
        raise ValueError("local data path must be absolute")
    resolved = lexical_path.resolve(strict=False)
    normalized = os.path.normcase(str(resolved)).replace("\\", "/")
    return "file:" + normalized


def canonical_data_endpoint(value: Any) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError("endpoint must be a non-empty string")
    candidate = _strip_endpoint_decorations(value)
    if lexical_local_path(candidate) is not None:
        return canonical_local_endpoint(candidate)
    try:
        parts = urlsplit(candidate)
    except ValueError as error:
        raise ValueError(f"invalid provider URI: {error}") from error
    scheme = parts.scheme.casefold()
    if not scheme:
        raise ValueError("endpoint must be an absolute local path or canonical URI")
    if any(character.isspace() for character in candidate):
        raise ValueError("ambiguous provider strings are forbidden")
    if scheme in {"memory", "temporary", "logical"}:
        return "uri:" + urlunsplit((scheme, parts.netloc, parts.path, parts.query, ""))
    try:
        port = parts.port
    except ValueError as error:
        raise ValueError(f"invalid provider URI port: {error}") from error
    userinfo = ""
    if parts.username is not None:
        userinfo = quote(unquote(parts.username), safe="")
        if parts.password is not None:
            userinfo += ":" + quote(unquote(parts.password), safe="")
        userinfo += "@"
    hostname = (parts.hostname or "").casefold()
    if not hostname and parts.netloc:
        raise ValueError("provider URI hostname is invalid")
    default_ports = {
        "http": 80,
        "https": 443,
        "mssql": 1433,
        "mysql": 3306,
        "oracle": 1521,
        "postgres": 5432,
        "postgresql": 5432,
        "ws": 80,
        "wss": 443,
    }
    port_suffix = (
        "" if port is None or default_ports.get(scheme) == port else f":{port}"
    )
    netloc = f"{userinfo}{hostname}{port_suffix}"
    decoded_path = unquote(parts.path)
    normalized_path = posixpath.normpath(decoded_path) if decoded_path else ""
    if (
        decoded_path.startswith("/")
        and normalized_path
        and not normalized_path.startswith("/")
    ):
        normalized_path = "/" + normalized_path
    encoded_path = quote(normalized_path, safe="/:@")
    normalized_query = urlencode(sorted(parse_qsl(parts.query, keep_blank_values=True)))
    return "uri:" + urlunsplit((scheme, netloc, encoded_path, normalized_query, ""))


def endpoint_is_within(endpoint: str, root: str) -> bool:
    if endpoint.startswith("file:") and root.startswith("file:"):
        endpoint_path = endpoint[5:].rstrip("/")
        root_path = root[5:].rstrip("/")
        try:
            common = os.path.commonpath([endpoint_path, root_path])
        except ValueError:
            return False
        normalized = os.path.normcase(common).replace("\\", "/").rstrip("/")
        return normalized == root_path
    if endpoint.startswith("uri:") and root.startswith("uri:"):
        endpoint_parts = urlsplit(endpoint[4:])
        root_parts = urlsplit(root[4:])
        if (
            endpoint_parts.scheme != root_parts.scheme
            or endpoint_parts.netloc != root_parts.netloc
        ):
            return False
        if root_parts.query:
            return endpoint == root
        endpoint_path = endpoint_parts.path.rstrip("/")
        root_path = root_parts.path.rstrip("/")
        return endpoint_path == root_path or endpoint_path.startswith(root_path + "/")
    return endpoint == root


def canonical_file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return "sha256:" + digest.hexdigest()


def _pointer_token(value: str) -> str:
    return value.replace("~", "~0").replace("/", "~1")


def _iter_json_leaves(value: Any, pointer: str) -> list[tuple[str, Any]]:
    if isinstance(value, dict):
        result: list[tuple[str, Any]] = []
        for key, child in value.items():
            result.extend(_iter_json_leaves(child, f"{pointer}/{_pointer_token(key)}"))
        return result or [(pointer, value)]
    if isinstance(value, list):
        result = []
        for index, child in enumerate(value):
            result.extend(_iter_json_leaves(child, f"{pointer}/{index}"))
        return result or [(pointer, value)]
    return [(pointer, value)]


def nested_leaf_values(arguments: dict[str, Any], stdin_payload: Any) -> dict[str, Any]:
    leaves: dict[str, Any] = {}
    for name, value in arguments.items():
        if isinstance(value, (dict, list)):
            pointer = f"/arguments/{_pointer_token(name)}"
            leaves.update(_iter_json_leaves(value, pointer))
    if isinstance(stdin_payload, (dict, list)):
        leaves.update(_iter_json_leaves(stdin_payload, "/stdin_payload"))
    return leaves


def _is_canonical_nested_pointer(pointer: Any) -> bool:
    if not isinstance(pointer, str) or not pointer.startswith("/"):
        return False
    tokens = pointer[1:].split("/")
    if not tokens or tokens[0] not in {"arguments", "stdin_payload"}:
        return False
    if tokens[0] == "arguments" and len(tokens) < 2:
        return False
    token_pattern = re.compile(r"(?:[^~]|~[01])*")
    return all(token_pattern.fullmatch(token) is not None for token in tokens[1:])


def validate_authoritative_nested_contract(
    node_id: str,
    leaf_values: dict[str, Any],
    nested_index: dict[str, dict[str, Any]],
    contract: Any,
    errors: list[str],
) -> None:
    if not isinstance(contract, dict):
        errors.append(f"{node_id}: authoritative nested target contract is required")
        return
    actual = set(leaf_values)
    reviewed = set(contract)
    missing = sorted(actual - reviewed)
    extra = sorted(reviewed - actual)
    if missing:
        errors.append(
            f"{node_id}: reviewed nested target contract misses leaves {missing}"
        )
    if extra:
        errors.append(
            f"{node_id}: reviewed nested target contract has extra leaves {extra}"
        )
    for pointer in sorted(actual & reviewed & set(nested_index)):
        if nested_index[pointer].get("target_kind") != contract.get(pointer):
            errors.append(
                f"{node_id}{pointer}: target_kind differs from authoritative "
                "reviewed contract"
            )


def validate_reviewed_capability_contracts(
    capability_id: str, capability: dict[str, Any], errors: list[str]
) -> None:
    nested_contract = capability.get("reviewed_nested_target_kinds")
    if not isinstance(nested_contract, dict):
        errors.append(f"{capability_id}: reviewed nested target contract is required")
    else:
        for pointer, kind in nested_contract.items():
            if not _is_canonical_nested_pointer(pointer):
                errors.append(
                    f"{capability_id}: invalid reviewed nested RFC 6901 pointer"
                )
            if kind not in NESTED_TARGET_KINDS:
                errors.append(f"{capability_id}: invalid reviewed nested target kind")
        if capability.get("reviewed_nested_target_kinds_hash") != sha256_value(
            nested_contract
        ):
            errors.append(
                f"{capability_id}: reviewed_nested_target_kinds_hash differs "
                "from reviewed contract"
            )
    evidence = capability.get("nested_target_review_evidence")
    if not isinstance(evidence, str) or not evidence.strip():
        errors.append(f"{capability_id}: nested target review evidence is required")

    parameter_contract = capability.get("reviewed_parameter_constraints")
    if not isinstance(parameter_contract, dict):
        errors.append(f"{capability_id}: reviewed parameter constraints are required")
    else:
        errors.extend(
            f"{capability_id} reviewed parameter constraints: {error}"
            for error in audit_schema_keywords(parameter_contract)
        )
        if capability.get("reviewed_parameter_constraints_hash") != sha256_value(
            parameter_contract
        ):
            errors.append(
                f"{capability_id}: reviewed_parameter_constraints_hash differs "
                "from reviewed contract"
            )
        algorithm_id = capability.get("algorithm_id")
        if capability.get("source_type") in {"cli", "qgis-processing"} and isinstance(
            algorithm_id, str
        ):
            properties = parameter_contract.get("properties")
            required = parameter_contract.get("required")
            algorithm_schema = (
                properties.get("algorithm_id") if isinstance(properties, dict) else None
            )
            if (
                not isinstance(required, list)
                or "algorithm_id" not in required
                or not isinstance(algorithm_schema, dict)
                or algorithm_schema.get("const") != algorithm_id
            ):
                errors.append(
                    f"{capability_id}: reviewed parameter constraints do not bind "
                    "exact algorithm_id"
                )
    evidence = capability.get("parameter_constraints_review_evidence")
    if not isinstance(evidence, str) or not evidence.strip():
        errors.append(
            f"{capability_id}: parameter constraints review evidence is required"
        )


def _nested_value_family(value: Any) -> str | None:
    if not isinstance(value, str) or not value.strip():
        return None
    candidate = _strip_endpoint_decorations(value)
    lowered = candidate.casefold()
    if (
        re.match(r"(?i)^[a-z]:[\\/]", candidate)
        or candidate.startswith(("/", "\\\\", "//"))
        or lowered.startswith("file:")
    ):
        return "local-path"
    scheme_match = ABSOLUTE_URI_SCHEME_RE.match(candidate)
    if scheme_match:
        scheme = scheme_match.group(0)[:-1].casefold()
        if scheme == "file":
            return "local-path"
        if scheme in DATABASE_URI_SCHEMES:
            return "database"
        if scheme in NETWORK_URI_SCHEMES:
            return "network"
        return "network"
    if (
        "\\" in candidate
        or "/../" in candidate
        or candidate.startswith(("../", "./"))
        or Path(candidate).suffix.casefold() in LIKELY_LOCAL_PATH_SUFFIXES
    ):
        return "local-path"
    return None


def _validate_nested_family_target_kind(
    family: str | None, target_kind: Any, label: str, errors: list[str]
) -> None:
    if family == "local-path" and target_kind not in LOCAL_PATH_TARGET_KINDS:
        errors.append(f"{label}: local path needs a file/path target kind")
    if family == "network" and target_kind != "network-endpoint":
        errors.append(f"{label}: network URI needs network-endpoint")
    if family == "database" and target_kind != "database":
        errors.append(f"{label}: database URI needs database")


def _canonical_allowed_roots(
    tool: dict[str, Any], field: str, label: str, errors: list[str]
) -> list[str]:
    values = tool.get(field)
    if not isinstance(values, list):
        errors.append(f"{label}: {field} must be an array")
        return []
    result: list[str] = []
    for value in values:
        lexical_path = _validate_lexical_path_no_reparse(
            value, f"{label}.{field}", errors
        )
        try:
            endpoint = canonical_data_endpoint(value)
        except (OSError, ValueError) as error:
            errors.append(f"{label}: invalid {field} entry: {error}")
            continue
        if field in {"allowed_executable_roots", "allowed_cwd_roots"} and (
            lexical_path is None
        ):
            errors.append(f"{label}: {field} entry must be a local directory")
        if lexical_path is not None:
            root_path = lexical_path.resolve(strict=False)
            if not root_path.is_dir() or is_reparse_point(root_path):
                errors.append(f"{label}: {field} entry is not a live directory")
        result.append(endpoint)
    return result


def derive_permission_target(
    kind: str,
    value: Any,
    sensitive: bool,
    tool: dict[str, Any],
    label: str,
    errors: list[str],
) -> tuple[str, str] | None:
    target_value: str
    if kind in LOCAL_PATH_TARGET_KINDS:
        if not isinstance(value, str):
            errors.append(f"{label}: {kind} value must be a string")
            return None
        lexical_path = _validate_lexical_path_no_reparse(value, label, errors)
        try:
            endpoint = canonical_data_endpoint(value)
        except (OSError, ValueError) as error:
            errors.append(f"{label}: invalid {kind} endpoint: {error}")
            return None
        if not endpoint.startswith("file:"):
            errors.append(f"{label}: {kind} must resolve to a local file endpoint")
            return None
        if lexical_path is None:
            errors.append(f"{label}: {kind} must have an absolute lexical path")
            return None
        local_path = lexical_path.resolve(strict=False)
        if kind == "executable" and (
            not local_path.is_file() or is_reparse_point(local_path)
        ):
            errors.append(f"{label}: executable is not a live regular file")
        if kind == "cwd" and (not local_path.is_dir() or is_reparse_point(local_path)):
            errors.append(f"{label}: cwd is not a live regular directory")
        if kind in INPUT_PATH_TARGET_KINDS:
            root_field = "allowed_input_roots"
        elif kind in OUTPUT_PATH_TARGET_KINDS:
            root_field = "allowed_output_roots"
        elif kind == "executable":
            root_field = "allowed_executable_roots"
        else:
            root_field = "allowed_cwd_roots"
        roots = _canonical_allowed_roots(tool, root_field, label, errors)
        if not roots:
            errors.append(f"{label}: {kind} has no canonical allowed root")
        elif not any(endpoint_is_within(endpoint, root) for root in roots):
            errors.append(f"{label}: {kind} is outside selected allowed roots")
        target_value = sha256_value(value) if sensitive else endpoint
    elif kind in {"network-endpoint", "database"}:
        if not isinstance(value, str):
            errors.append(f"{label}: {kind} value must be a string")
            return None
        try:
            endpoint = canonical_data_endpoint(value)
        except (OSError, ValueError) as error:
            errors.append(f"{label}: invalid {kind}: {error}")
            return None
        if not endpoint.startswith("uri:"):
            errors.append(f"{label}: {kind} must be an absolute URI")
            return None
        if kind == "network-endpoint" and tool.get("network_policy") == "no-network":
            errors.append(f"{label}: network target conflicts with no-network policy")
        target_value = sha256_value(value) if sensitive else endpoint
    elif kind in {"credential", "device"}:
        if not sensitive:
            errors.append(f"{label}: {kind} target must be sensitive")
        target_value = sha256_value(value)
    elif kind in {"arguments", "argv", "stdin"}:
        target_value = sha256_value(value)
    elif isinstance(value, str) and not sensitive:
        target_value = value
    else:
        target_value = sha256_value(value)
    return f"{kind}:{target_value}", target_value


def _expected_scope_effects(
    tool: dict[str, Any], label: str, errors: list[str]
) -> dict[str, tuple[str, str]]:
    expected: dict[str, tuple[str, str]] = {}
    for field, kind in (
        ("allowed_input_roots", "input-root"),
        ("allowed_output_roots", "output-root"),
        ("allowed_executable_roots", "executable-root"),
        ("allowed_cwd_roots", "cwd-root"),
    ):
        for root in _canonical_allowed_roots(tool, field, label, errors):
            target = f"{kind}:{root}"
            expected[target] = (kind, root)
    for value in tool.get("qgis_state_changes", []):
        if isinstance(value, str):
            target = f"qgis-state:{value}"
            expected[target] = ("qgis-state", value)
    return expected


def validate_skills_root_binding(
    plan: dict[str, Any],
    manifest_value: Path | str | None,
    skills_root_value: Path | str | None,
    errors: list[str],
    *,
    reparse_checker: Any = None,
) -> Path | None:
    if manifest_value is None:
        errors.append("verify-staged requires --skills-service-manifest")
        return None
    if skills_root_value is None:
        errors.append("verify-staged requires --skills-root")
        return None
    checker = reparse_checker or is_reparse_point
    lexical_manifest = Path(manifest_value).expanduser().absolute()
    for ancestor in existing_ancestors(lexical_manifest):
        if checker(ancestor):
            errors.append(
                f"Skills service manifest path contains a reparse point: {ancestor}"
            )
    manifest_path = lexical_manifest.resolve(strict=False)
    if not manifest_path.is_file():
        errors.append("Skills service manifest does not exist or is not a file")
        return None
    try:
        manifest = load_strict_json(manifest_path)
    except (OSError, GateInputError) as error:
        errors.append(f"Skills service manifest is invalid: {error}")
        return None
    if not isinstance(manifest, dict):
        errors.append("Skills service manifest must be a JSON object")
        return None
    if manifest.get("service_id") != "qcopilots.mcp_server_skills":
        errors.append("Skills service manifest has the wrong service_id")
    relative_root = manifest.get("skillsRoot")
    if not is_relative_package_path(relative_root):
        errors.append("Skills service manifest skillsRoot must be a safe relative path")
        return None
    manifest_root_lexical = (lexical_manifest.parent / relative_root).absolute()
    for ancestor in existing_ancestors(manifest_root_lexical):
        if checker(ancestor):
            errors.append(
                "manifest-derived Skills root path contains a reparse point: "
                f"{ancestor}"
            )
    manifest_root = manifest_root_lexical.resolve(strict=False)
    if not is_within(manifest_root, manifest_path.parent):
        errors.append("Skills service manifest skillsRoot escapes its directory")
    if not manifest_root.is_dir():
        errors.append("manifest-resolved Skills root is not a directory")
    supplied_root_lexical = Path(skills_root_value).expanduser().absolute()
    for ancestor in existing_ancestors(supplied_root_lexical):
        if checker(ancestor):
            errors.append(f"Skills root path contains a reparse point: {ancestor}")
    supplied_root = supplied_root_lexical.resolve(strict=False)
    if not supplied_root.is_dir():
        errors.append("supplied Skills root is not a directory")
    if normalized_resolved_path(supplied_root) != normalized_resolved_path(
        manifest_root
    ):
        errors.append("--skills-root does not match manifest-resolved skillsRoot")
    manifest_hash = sha256_value(manifest)
    identity_hash = compute_skills_root_identity(manifest_hash, manifest_root)
    environment = plan.get("environment_contract")
    contract = environment.get("skills_root") if isinstance(environment, dict) else None
    if not isinstance(contract, dict):
        errors.append("environment_contract.skills_root must be an object")
    else:
        expected = {
            "display_path": normalized_resolved_path(manifest_root),
            "manifest_path": normalized_resolved_path(manifest_path),
            "manifest_hash": manifest_hash,
            "identity_hash": identity_hash,
        }
        for field, expected_value in expected.items():
            actual = contract.get(field)
            if field in {"display_path", "manifest_path"} and isinstance(actual, str):
                actual = actual.replace("\\", "/").casefold()
            if actual != expected_value:
                errors.append(
                    "environment_contract.skills_root."
                    f"{field} differs from live manifest binding"
                )
    return manifest_root


def validate_staged_path_binding(
    plan: dict[str, Any],
    staged_root_value: Path | str | None,
    manifest_value: Path | str | None,
    skills_root_value: Path | str | None,
    errors: list[str],
) -> Path | None:
    skills_root = validate_skills_root_binding(
        plan, manifest_value, skills_root_value, errors
    )
    if staged_root_value is None:
        errors.append("verify-staged requires --staged-package-root")
        return None
    lexical_target = Path(staged_root_value).expanduser().absolute()
    for ancestor in existing_ancestors(lexical_target):
        if is_reparse_point(ancestor):
            errors.append(f"staged package path contains a reparse point: {ancestor}")
    target = lexical_target.resolve(strict=False)
    if not target.is_dir():
        errors.append("staged package root does not exist or is not a directory")
        return None
    identity = plan.get("skill_identity")
    package = plan.get("target_skill_package")
    skill_name = identity.get("name") if isinstance(identity, dict) else None
    target_name = (
        package.get("target_relative_path") if isinstance(package, dict) else None
    )
    if target.name != skill_name or target.name != target_name:
        errors.append("staged package name differs from approved skill identity")
    if skills_root is not None:
        if target.parent != skills_root or not is_within(target, skills_root):
            errors.append("staged package must be a direct child of the Skills root")
        creator = (skills_root / "qgis-skills-creator").resolve(strict=False)
        if not creator.is_dir():
            errors.append("qgis-skills-creator sibling is missing from Skills root")
        if is_reparse_point(creator):
            errors.append("qgis-skills-creator sibling is a reparse point")
        if target == creator or is_within(target, creator):
            errors.append("staged package cannot equal or be inside the Creator")
        if target.parent != creator.parent:
            errors.append("staged package and Creator must be siblings")
    return target


def find_placeholders(value: Any, location: str = "$") -> list[str]:
    found: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            found.extend(find_placeholders(child, f"{location}/{key}"))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            found.extend(find_placeholders(child, f"{location}/{index}"))
    elif isinstance(value, str):
        lowered = value.lower()
        if (
            value == ZERO_HASH
            or "{{" in value
            or lowered
            in {
                "pending-runtime-probe",
                "pending-resolved-skills-root",
                "pending-qcopilots-service-manifest",
                "pending-domain-selection",
            }
            or "pending-qgis-workflow" in lowered
            or "pending qgis workflow" in lowered
            or lowered.startswith("replace with")
            or lowered.startswith("qgis-skill-plan-replace-")
            or value == "2000-01-01T00:00:00Z"
            or value.startswith("替换为")
            or "待确认" in value
            or "待定义" in value
        ):
            found.append(location)
    return found


def _indexed(items: Any, prefix: str, errors: list[str]) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    if not isinstance(items, list):
        errors.append(f"{prefix}: must be an array")
        return result
    for index, item in enumerate(items):
        if not isinstance(item, dict) or not isinstance(item.get("id"), str):
            errors.append(f"{prefix}[{index}]: object with string id required")
            continue
        item_id = item["id"]
        if item_id in result:
            errors.append(f"{prefix}: duplicate id {item_id}")
        result[item_id] = item
    return result


def _catalog_indexes(
    catalog: dict[str, Any], errors: list[str]
) -> tuple[dict[tuple[str, str], dict[str, Any]], dict[str, dict[str, Any]]]:
    mcp: dict[tuple[str, str], dict[str, Any]] = {}
    cli: dict[str, dict[str, Any]] = {}
    for service in catalog.get("mcp_services", []):
        if not isinstance(service, dict):
            continue
        service_id = service.get("service_id")
        for tool in service.get("tools", []):
            if (
                isinstance(service_id, str)
                and isinstance(tool, dict)
                and isinstance(tool.get("name"), str)
            ):
                key = (service_id, tool["name"])
                if key in mcp:
                    errors.append(f"catalog: duplicate MCP entry {key}")
                mcp[key] = tool
    for tool in catalog.get("cli_tools", []):
        if isinstance(tool, dict) and isinstance(tool.get("id"), str):
            tool_id = tool["id"]
            if tool_id in cli:
                errors.append(f"catalog: duplicate CLI entry {tool_id}")
            cli[tool_id] = tool
    return mcp, cli


def validate_capability_identity(
    item: dict[str, Any], label: str, errors: list[str]
) -> None:
    source_type = item.get("source_type")
    service_id = item.get("service_id")
    tool_name = item.get("tool_name")
    cli_id = item.get("cli_id")
    algorithm_id = item.get("algorithm_id")
    provider = item.get("provider")
    if source_type == "cli":
        if not isinstance(cli_id, str) or not cli_id:
            errors.append(f"{label}: CLI identity requires cli_id")
        if service_id is not None or tool_name is not None:
            errors.append(f"{label}: CLI identity forbids MCP fields")
    elif source_type in {"mcp", "qgis-processing"}:
        if not isinstance(service_id, str) or not service_id:
            errors.append(f"{label}: {source_type} identity requires service_id")
        if not isinstance(tool_name, str) or not tool_name:
            errors.append(f"{label}: {source_type} identity requires tool_name")
        if cli_id is not None:
            errors.append(f"{label}: {source_type} identity forbids cli_id")
        if source_type == "mcp" and algorithm_id is not None:
            errors.append(f"{label}: plain MCP identity forbids algorithm_id")
        if source_type == "qgis-processing" and (
            not isinstance(algorithm_id, str) or not algorithm_id
        ):
            errors.append(f"{label}: Processing identity requires algorithm_id")
    else:
        errors.append(f"{label}: unsupported source_type {source_type!r}")
    if isinstance(algorithm_id, str):
        if not isinstance(provider, str) or not provider:
            errors.append(f"{label}: algorithm identity requires provider")
        elif not algorithm_id.startswith(f"{provider}:"):
            errors.append(f"{label}: algorithm provider prefix differs")


def validate_capability_state(
    capability: dict[str, Any], label: str, errors: list[str]
) -> None:
    if capability.get("discovered") is not True:
        errors.append(f"{label}: capability must be discovered")
    evidence = capability.get("eligibility_evidence")
    if not isinstance(evidence, str) or not evidence.strip():
        errors.append(f"{label}: eligibility_evidence must be non-empty")
    eligible = capability.get("eligible")
    reason = capability.get("ineligibility_reason")
    if eligible is True and reason is not None:
        errors.append(f"{label}: eligible capability requires null reason")
    elif eligible is False and (not isinstance(reason, str) or not reason.strip()):
        errors.append(f"{label}: ineligible capability requires a reason")
    elif not isinstance(eligible, bool):
        errors.append(f"{label}: eligible must be boolean")


def _policy_is_not_weaker(reviewed: dict[str, Any], baseline: dict[str, Any]) -> bool:
    if baseline.get(
        "require_all_schema_properties_explicit"
    ) is True and not reviewed.get("require_all_schema_properties_explicit"):
        return False
    for field in ("required_arguments", "required_target_arguments"):
        if not set(baseline.get(field, [])).issubset(reviewed.get(field, [])):
            return False
    for field in (
        "required_any_argument_groups",
        "required_any_target_argument_groups",
    ):
        reviewed_groups = {tuple(sorted(group)) for group in reviewed.get(field, [])}
        for group in baseline.get(field, []):
            if tuple(sorted(group)) not in reviewed_groups:
                return False
    return reviewed.get("command_shape") == baseline.get("command_shape")


def validate_live_capability_contract(
    capability_id: str,
    capability: dict[str, Any],
    baseline: dict[str, Any] | None,
    errors: list[str],
) -> None:
    risk = capability.get("reviewed_risk_level")
    side_effects = capability.get("reviewed_side_effects")
    invocation = capability.get("reviewed_invocation_policy")
    status = capability.get("risk_review_status")
    if status == "unknown":
        if (
            risk != "R3"
            or not isinstance(side_effects, dict)
            or not all(side_effects.values())
        ):
            errors.append(
                f"{capability_id}: unknown risk must fail closed as R3 with "
                "all side effects"
            )
    elif status != "reviewed":
        errors.append(f"{capability_id}: invalid risk_review_status")
    evidence = capability.get("risk_review_evidence")
    if not isinstance(evidence, str) or not evidence.strip():
        errors.append(f"{capability_id}: risk review evidence is required")
    if not isinstance(invocation, dict):
        errors.append(f"{capability_id}: reviewed invocation policy is required")
        invocation = {}
    if not isinstance(side_effects, dict):
        errors.append(f"{capability_id}: reviewed side effects are required")
        side_effects = {}

    if baseline is None and risk in RISK_RANK:
        required_floor = "R0"
        if side_effects.get("modifies_qgis_state") is True:
            required_floor = "R1"
        if side_effects.get("writes_files") is True:
            required_floor = "R2"
        if any(
            side_effects.get(field) is True
            for field in ("uses_network", "runs_shell", "destructive_possible")
        ):
            required_floor = "R3"
        if capability.get("source_type") == "cli":
            required_floor = "R3"
            if side_effects.get("runs_shell") is not True:
                errors.append(
                    f"{capability_id}: uncatalogued CLI must disclose Shell effect"
                )
        if RISK_RANK[risk] < RISK_RANK[required_floor]:
            errors.append(
                f"{capability_id}: uncatalogued risk understates reviewed effects"
            )
    elif isinstance(baseline, dict):
        baseline_risk = baseline.get("risk_level")
        if risk in RISK_RANK and baseline_risk in RISK_RANK:
            if RISK_RANK[risk] < RISK_RANK[baseline_risk]:
                errors.append(
                    f"{capability_id}: live risk understates catalog baseline"
                )
        baseline_effects = baseline.get("side_effects")
        if isinstance(baseline_effects, dict):
            for field, value in baseline_effects.items():
                if value is True and side_effects.get(field) is not True:
                    errors.append(
                        f"{capability_id}: live side effects understate {field}"
                    )
        baseline_invocation = baseline.get("invocation_policy")
        if isinstance(baseline_invocation, dict) and not _policy_is_not_weaker(
            invocation, baseline_invocation
        ):
            errors.append(
                f"{capability_id}: live invocation policy is weaker than catalog"
            )

    source_type = capability.get("source_type")
    if source_type == "cli":
        executable = capability.get("resolved_executable")
        expected_hash = capability.get("resolved_executable_sha256")
        if not isinstance(executable, str):
            errors.append(f"{capability_id}: CLI resolved executable is required")
        else:
            lexical_executable = _validate_lexical_path_no_reparse(
                executable, f"{capability_id}.resolved_executable", errors
            )
            try:
                endpoint = canonical_data_endpoint(executable)
            except (OSError, ValueError) as error:
                errors.append(f"{capability_id}: invalid resolved executable: {error}")
            else:
                if not endpoint.startswith("file:"):
                    errors.append(f"{capability_id}: resolved executable must be local")
            executable_path = (
                lexical_executable.resolve(strict=False)
                if lexical_executable is not None
                else None
            )
            if (
                executable_path is None
                or not executable_path.is_file()
                or is_reparse_point(executable_path)
            ):
                errors.append(
                    f"{capability_id}: resolved executable is not a regular file"
                )
            else:
                try:
                    actual_hash = canonical_file_sha256(executable_path)
                except OSError as error:
                    errors.append(f"{capability_id}: executable hash failed: {error}")
                else:
                    if expected_hash != actual_hash:
                        errors.append(
                            f"{capability_id}: resolved executable hash drifted"
                        )
        executable_evidence = capability.get("executable_evidence")
        if not isinstance(executable_evidence, str) or not executable_evidence.strip():
            errors.append(f"{capability_id}: executable evidence is required")
        argv = capability.get("reviewed_argv_template")
        if not isinstance(argv, list) or not argv:
            errors.append(f"{capability_id}: reviewed CLI argv is required")
        else:
            for token in argv:
                if isinstance(token, str) and ("<" in token or ">" in token):
                    if re.fullmatch(r"<[a-z][a-z0-9_]*>", token) is None:
                        errors.append(
                            f"{capability_id}: invalid whole-token CLI placeholder"
                        )
        if invocation.get("command_shape") != "argv-only":
            errors.append(f"{capability_id}: CLI command shape must be argv-only")
    else:
        for field in (
            "resolved_executable",
            "resolved_executable_sha256",
            "executable_evidence",
            "reviewed_argv_template",
            "reviewed_stdin_contract",
        ):
            if capability.get(field) is not None:
                errors.append(f"{capability_id}: non-CLI {field} must be null")
        if capability.get("reviewed_argument_target_kinds") != {}:
            errors.append(
                f"{capability_id}: non-CLI argument target kinds must be empty"
            )
        if capability.get("reviewed_operation_id") != capability.get("tool_name"):
            errors.append(
                f"{capability_id}: MCP reviewed operation must equal tool name"
            )


def validate_selected_capability(
    tool_id: str, capability: dict[str, Any], errors: list[str]
) -> None:
    if capability.get("discovered") is not True:
        errors.append(f"{tool_id}: selected capability is not discovered")
    if capability.get("eligible") is not True:
        errors.append(f"{tool_id}: selected capability is not eligible")
    if capability.get("available") is not True:
        errors.append(f"{tool_id}: selected capability is not actually available")
    if capability.get("deprecated") is True:
        errors.append(f"{tool_id}: selected capability is deprecated")
    if capability.get("risk_review_status") != "reviewed":
        errors.append(f"{tool_id}: selected capability risk is not fully reviewed")


def validate_algorithm_binding(
    tool_id: str, tool: dict[str, Any], errors: list[str]
) -> None:
    algorithm_id = tool.get("algorithm_id")
    reviewed = tool.get("allowed_algorithm_ids")
    if not isinstance(reviewed, list):
        errors.append(f"{tool_id}: allowed_algorithm_ids must be an array")
        reviewed = []
    if any("*" in str(item) for item in reviewed):
        errors.append(f"{tool_id}: algorithm wildcard is forbidden")
    if algorithm_id is None:
        if reviewed:
            errors.append(f"{tool_id}: non-algorithm tool must not bind algorithms")
        return
    if not isinstance(algorithm_id, str) or not algorithm_id:
        errors.append(f"{tool_id}: algorithm_id must be exact and non-empty")
        return
    if reviewed != [algorithm_id]:
        errors.append(
            f"{tool_id}: allowed_algorithm_ids must contain only exact algorithm_id"
        )
    provider = tool.get("provider")
    if not isinstance(provider, str) or not algorithm_id.startswith(f"{provider}:"):
        errors.append(f"{tool_id}: provider and algorithm_id prefix differ")


def validate_creation_policy(package: dict[str, Any], errors: list[str]) -> None:
    if package.get("collision_policy") != "fail-no-overwrite":
        errors.append("new skill creation must use fail-no-overwrite")
    if package.get("staging_policy") != "resources-first-skill-md-last":
        errors.append("new skill creation must use resources-first-skill-md-last")


def validate_catalog_policy_binding(
    tool_id: str,
    tool: dict[str, Any],
    policy: dict[str, Any] | None,
    errors: list[str],
) -> dict[str, Any] | None:
    if policy is None:
        return None
    source_type = tool.get("source_type")
    if source_type == "cli":
        expected_ref = f"cli:{tool.get('cli_id')}"
    else:
        expected_ref = f"mcp:{tool.get('service_id')}#{tool.get('tool_name')}"
    if tool.get("catalog_ref") != expected_ref:
        errors.append(f"{tool_id}: catalog_ref must equal {expected_ref}")
    status = policy.get("status")
    if status not in {"allowed", "restricted"}:
        errors.append(f"{tool_id}: catalog status {status!r} is not selectable")
    operation: dict[str, Any] | None = None
    policy_risk = policy.get("risk_level")
    operation_risk = policy_risk
    if source_type == "cli":
        contracts = policy.get("operation_contracts")
        matches = [
            contract
            for contract in contracts or []
            if isinstance(contract, dict)
            and contract.get("id") == tool.get("operation_id")
        ]
        if len(matches) != 1:
            errors.append(f"{tool_id}: no unique catalog CLI operation contract")
        else:
            operation = matches[0]
            operation_risk = operation.get("risk_level")
            if tool.get("argv_template") != operation.get("argv_template"):
                errors.append(f"{tool_id}: argv_template differs from catalog")
            if tool.get("stdin_contract") != operation.get("stdin_schema"):
                errors.append(f"{tool_id}: stdin_contract differs from catalog")
            if tool.get("argument_target_kinds") != operation.get(
                "argument_target_kinds", {}
            ):
                errors.append(f"{tool_id}: argument target kinds differ from catalog")
            for token in operation.get("argv_template", []):
                if isinstance(token, str) and ("<" in token or ">" in token):
                    if re.fullmatch(r"<[a-z][a-z0-9_]*>", token) is None:
                        errors.append(
                            f"{tool_id}: CLI placeholder must occupy one token"
                        )
    else:
        if tool.get("operation_id") != tool.get("tool_name"):
            errors.append(f"{tool_id}: MCP operation_id must equal tool_name")
        if tool.get("argv_template") is not None:
            errors.append(f"{tool_id}: MCP tool must not declare argv_template")
        if tool.get("stdin_contract") is not None:
            errors.append(f"{tool_id}: MCP tool must not declare stdin_contract")
        if tool.get("risk_level") in {"R2", "R3"} and not isinstance(
            policy.get("invocation_policy"), dict
        ):
            errors.append(f"{tool_id}: high-risk MCP policy lacks invocation rules")
    effective_risks = [
        risk for risk in (policy_risk, operation_risk) if risk in RISK_RANK
    ]
    selected_risk = tool.get("risk_level")
    if effective_risks and selected_risk in RISK_RANK:
        baseline = max(effective_risks, key=RISK_RANK.__getitem__)
        if RISK_RANK[selected_risk] < RISK_RANK[baseline]:
            errors.append(f"{tool_id}: risk_level understates catalog policy")
    selected_effects = tool.get("catalog_side_effects")
    baseline_effects = policy.get("side_effects")
    if isinstance(selected_effects, dict) and isinstance(baseline_effects, dict):
        for field, value in baseline_effects.items():
            if value is True and selected_effects.get(field) is not True:
                errors.append(f"{tool_id}: effective side effects understate {field}")
    baseline_invocation = policy.get("invocation_policy")
    selected_invocation = tool.get("invocation_policy")
    if isinstance(baseline_invocation, dict) and (
        not isinstance(selected_invocation, dict)
        or not _policy_is_not_weaker(selected_invocation, baseline_invocation)
    ):
        errors.append(f"{tool_id}: invocation policy is weaker than catalog")
    if status == "restricted" or policy.get("requires_explicit_confirmation"):
        if tool.get("approval_required") is not True:
            errors.append(f"{tool_id}: catalog policy requires approval")
    side_effects = policy.get("side_effects")
    if isinstance(side_effects, dict):
        if (
            side_effects.get("destructive_possible") is True
            and tool.get("approval_policy") != "per-invocation-approval"
        ):
            errors.append(
                f"{tool_id}: destructive policy requires per-invocation approval"
            )
        if side_effects.get("modifies_qgis_state") is True and not tool.get(
            "qgis_state_changes"
        ):
            errors.append(f"{tool_id}: QGIS state changes must be recorded")
    return operation


def validate_invocation_policy(
    node_id: str,
    arguments: dict[str, Any],
    omitted: set[str],
    bindings: dict[str, dict[str, Any]],
    schema_properties: set[str],
    policy: dict[str, Any],
    errors: list[str],
) -> None:
    argument_names = set(arguments)
    required = set(policy.get("required_arguments", []))
    missing = required - argument_names
    if missing:
        errors.append(
            f"{node_id}: invocation policy arguments missing {sorted(missing)}"
        )
    for group in policy.get("required_any_argument_groups", []):
        if isinstance(group, list) and not argument_names.intersection(group):
            errors.append(f"{node_id}: invocation policy requires one of {group}")
    for group in policy.get("required_any_target_argument_groups", []):
        supplied = argument_names.intersection(group)
        if not supplied or not all(
            bindings.get(name, {}).get("target_kind") != "none" for name in supplied
        ):
            errors.append(
                f"{node_id}: invocation policy requires target bindings for {group}"
            )
    for name in policy.get("required_target_arguments", []):
        binding = bindings.get(name)
        if binding is None or binding.get("target_kind") == "none":
            errors.append(f"{node_id}: argument {name!r} needs an exact target")
    if (
        policy.get("require_all_schema_properties_explicit") is True
        and argument_names | omitted != schema_properties
    ):
        errors.append(f"{node_id}: every schema property must be bound or omitted")


def validate_node_invocation(
    node_id: str,
    node: dict[str, Any],
    tool: dict[str, Any],
    capability: dict[str, Any],
    _catalog_policy: dict[str, Any] | None,
    errors: list[str],
) -> None:
    arguments = node.get("arguments")
    bindings = node.get("argument_bindings")
    nested_bindings = node.get("nested_target_bindings")
    omitted_value = node.get("omitted_optional_arguments")
    effects = node.get("effect_targets")
    if not isinstance(arguments, dict):
        errors.append(f"{node_id}: arguments must be an object")
        return
    if not isinstance(bindings, list):
        errors.append(f"{node_id}: argument_bindings must be an array")
        bindings = []
    if not isinstance(nested_bindings, list):
        errors.append(f"{node_id}: nested_target_bindings must be an array")
        nested_bindings = []
    if not isinstance(omitted_value, list):
        errors.append(f"{node_id}: omitted_optional_arguments must be an array")
        omitted_value = []
    omitted = {item for item in omitted_value if isinstance(item, str)}
    if len(omitted) != len(omitted_value):
        errors.append(f"{node_id}: omitted arguments must be unique strings")
    if not isinstance(effects, list):
        errors.append(f"{node_id}: effect_targets must be an array")
        effects = []

    effect_index: dict[str, dict[str, Any]] = {}
    for index, effect in enumerate(effects):
        if not isinstance(effect, dict):
            errors.append(f"{node_id}.effect_targets[{index}]: object required")
            continue
        kind = effect.get("kind")
        value = effect.get("value")
        permission_target = effect.get("permission_target")
        if not isinstance(kind, str) or not isinstance(value, str):
            errors.append(f"{node_id}.effect_targets[{index}]: invalid kind/value")
            continue
        expected = f"{kind}:{value}"
        if permission_target != expected:
            errors.append(
                f"{node_id}.effect_targets[{index}]: permission_target mismatch"
            )
        if isinstance(permission_target, str):
            if permission_target in effect_index:
                errors.append(f"{node_id}: duplicate effect target")
            effect_index[permission_target] = effect

    expected_effects = _expected_scope_effects(tool, node_id, errors)
    arguments_value = sha256_value(arguments)
    arguments_target = f"arguments:{arguments_value}"
    expected_effects[arguments_target] = ("arguments", arguments_value)

    binding_index: dict[str, dict[str, Any]] = {}
    for index, binding in enumerate(bindings):
        if not isinstance(binding, dict):
            errors.append(f"{node_id}.argument_bindings[{index}]: object required")
            continue
        name = binding.get("name")
        if not isinstance(name, str) or not name:
            errors.append(f"{node_id}.argument_bindings[{index}].name: invalid")
            continue
        if name in binding_index:
            errors.append(f"{node_id}: duplicate argument binding {name!r}")
        binding_index[name] = binding
        if name not in arguments:
            errors.append(f"{node_id}: binding {name!r} has no argument")
            continue
        source_kind = binding.get("source_kind")
        source_ref = binding.get("source_ref")
        if source_kind == "literal":
            if source_ref is not None:
                errors.append(f"{node_id}.{name}: literal source_ref must be null")
        elif not isinstance(source_ref, str) or not source_ref.strip():
            errors.append(f"{node_id}.{name}: non-literal needs source_ref")
        if source_kind == "input-contract" and source_ref not in node.get("inputs", []):
            errors.append(f"{node_id}.{name}: source_ref is not a node input")
        constraint = binding.get("constraint_schema")
        if not isinstance(constraint, dict):
            errors.append(f"{node_id}.{name}: constraint_schema must be object")
        else:
            constraint_errors = validate_schema_subset(arguments[name], constraint)
            errors.extend(
                f"{node_id}.{name} constraint: {error}" for error in constraint_errors
            )
            if (
                tool.get("risk_level") in {"R2", "R3"}
                and source_kind != "literal"
                and not constraint
            ):
                errors.append(
                    f"{node_id}.{name}: high-risk runtime value needs constraints"
                )
        target_kind = binding.get("target_kind")
        permission_target = binding.get("permission_target")
        if target_kind == "none":
            if permission_target is not None:
                errors.append(f"{node_id}.{name}: non-target must use null target")
            continue
        derived = derive_permission_target(
            target_kind,
            arguments[name],
            binding.get("sensitive") is True,
            tool,
            f"{node_id}.{name}",
            errors,
        )
        if derived is None:
            continue
        expected_target, target_value = derived
        if permission_target != expected_target:
            errors.append(f"{node_id}.{name}: permission_target mismatch")
        expected_effects[expected_target] = (target_kind, target_value)

    argument_names = set(arguments)
    if set(binding_index) != argument_names:
        errors.append(f"{node_id}: bindings must exactly cover arguments")
    if argument_names & omitted:
        errors.append(f"{node_id}: argument cannot also be omitted")
    parameter_constraints = tool.get("parameter_constraints")
    if not isinstance(parameter_constraints, dict):
        errors.append(f"{node_id}: parameter_constraints must be an object")
    else:
        parameter_errors = validate_schema_subset(arguments, parameter_constraints)
        errors.extend(
            f"{node_id} parameter_constraints: {error}" for error in parameter_errors
        )
    input_schema = capability.get("input_schema")
    if not isinstance(input_schema, dict):
        errors.append(f"{node_id}: live capability input_schema must be object")
        input_schema = {}
    else:
        input_errors = validate_schema_subset(arguments, input_schema)
        errors.extend(f"{node_id} live input_schema: {error}" for error in input_errors)
    properties_value = input_schema.get("properties", {})
    properties = set(properties_value) if isinstance(properties_value, dict) else set()
    required_value = input_schema.get("required", [])
    required = set(required_value) if isinstance(required_value, list) else set()
    if not required.issubset(argument_names):
        errors.append(
            f"{node_id}: required arguments missing {sorted(required - argument_names)}"
        )
    if not omitted.issubset(properties - required):
        errors.append(f"{node_id}: invalid omitted optional arguments")

    leaf_values = nested_leaf_values(arguments, node.get("stdin_payload"))
    nested_index: dict[str, dict[str, Any]] = {}
    for index, binding in enumerate(nested_bindings):
        if not isinstance(binding, dict):
            errors.append(f"{node_id}.nested_target_bindings[{index}]: object required")
            continue
        pointer = binding.get("json_pointer")
        if not isinstance(pointer, str) or pointer not in leaf_values:
            errors.append(
                f"{node_id}.nested_target_bindings[{index}]: pointer is not an "
                "actual nested leaf"
            )
            continue
        if pointer in nested_index:
            errors.append(f"{node_id}: duplicate nested pointer {pointer!r}")
            continue
        nested_index[pointer] = binding
        target_kind = binding.get("target_kind")
        permission_target = binding.get("permission_target")
        value = leaf_values[pointer]
        family = _nested_value_family(value)
        if target_kind == "none":
            if permission_target is not None:
                errors.append(f"{node_id}{pointer}: non-target must use null target")
            if family is not None:
                errors.append(
                    f"{node_id}{pointer}: target-like nested value cannot be none"
                )
            continue
        if target_kind not in NESTED_EFFECT_TARGET_KINDS:
            errors.append(f"{node_id}{pointer}: invalid nested target kind")
            continue
        _validate_nested_family_target_kind(
            family, target_kind, f"{node_id}{pointer}", errors
        )
        if target_kind == "executable" and not (
            tool.get("source_type") in {"mcp", "qgis-processing"}
            and tool.get("invocation_policy", {}).get("command_shape") == "argv-only"
            and pointer == "/arguments/command/0"
        ):
            errors.append(f"{node_id}{pointer}: executable target is not argv[0]")
        rationale = binding.get("rationale")
        if not isinstance(rationale, str) or not rationale.strip():
            errors.append(f"{node_id}{pointer}: nested rationale is required")
        derived = derive_permission_target(
            target_kind,
            value,
            binding.get("sensitive") is True,
            tool,
            f"{node_id}{pointer}",
            errors,
        )
        if derived is None:
            continue
        expected_target, target_value = derived
        if permission_target != expected_target:
            errors.append(f"{node_id}{pointer}: permission_target mismatch")
        expected_effects[expected_target] = (target_kind, target_value)
    missing_nested = sorted(set(leaf_values) - set(nested_index))
    extra_nested = sorted(set(nested_index) - set(leaf_values))
    if missing_nested:
        errors.append(
            f"{node_id}: nested target contract misses leaves {missing_nested}"
        )
    if extra_nested:
        errors.append(
            f"{node_id}: nested target contract has extra leaves {extra_nested}"
        )
    validate_authoritative_nested_contract(
        node_id,
        leaf_values,
        nested_index,
        tool.get("nested_target_kinds"),
        errors,
    )

    source_type = tool.get("source_type")
    invocation_policy = tool.get("invocation_policy")
    if source_type in {"cli", "qgis-processing"} and arguments.get(
        "algorithm_id"
    ) != tool.get("algorithm_id"):
        errors.append(f"{node_id}: algorithm_id differs from selected tool")
    if source_type in {"mcp", "qgis-processing"}:
        if node.get("stdin_payload") is not None:
            errors.append(f"{node_id}: MCP invocation forbids stdin_payload")
        if isinstance(invocation_policy, dict):
            validate_invocation_policy(
                node_id,
                arguments,
                omitted,
                binding_index,
                properties,
                invocation_policy,
                errors,
            )
            if invocation_policy.get("command_shape") == "argv-only":
                command = arguments.get("command")
                cwd = arguments.get("cwd")
                if (
                    not isinstance(command, list)
                    or not command
                    or not all(isinstance(item, str) and item for item in command)
                ):
                    errors.append(f"{node_id}: shell command must be argv array")
                else:
                    derived = derive_permission_target(
                        "executable",
                        command[0],
                        False,
                        tool,
                        f"{node_id}.command[0]",
                        errors,
                    )
                    if derived is not None:
                        executable_target, executable_value = derived
                        expected_effects[executable_target] = (
                            "executable",
                            executable_value,
                        )
                        full_argv = [executable_value, *command[1:]]
                        argv_value = sha256_value(full_argv)
                        expected_effects[f"argv:{argv_value}"] = (
                            "argv",
                            argv_value,
                        )
                if not isinstance(cwd, str) or not cwd:
                    errors.append(f"{node_id}: shell invocation requires cwd")
                else:
                    derived = derive_permission_target(
                        "cwd",
                        cwd,
                        False,
                        tool,
                        f"{node_id}.cwd",
                        errors,
                    )
                    if derived is not None:
                        cwd_target, cwd_value = derived
                        expected_effects[cwd_target] = ("cwd", cwd_value)
                if any(
                    node.get(field) is not None
                    for field in ("executable", "materialized_argv", "cwd")
                ):
                    errors.append(
                        f"{node_id}: MCP shell derives process fields from arguments"
                    )
            elif any(
                node.get(field) is not None
                for field in ("executable", "materialized_argv", "cwd")
            ):
                errors.append(f"{node_id}: non-CLI process fields must be null")
        elif tool.get("risk_level") in {"R2", "R3"}:
            errors.append(f"{node_id}: reviewed invocation policy is missing")
    elif source_type == "cli":
        if isinstance(invocation_policy, dict):
            validate_invocation_policy(
                node_id,
                arguments,
                omitted,
                binding_index,
                properties,
                invocation_policy,
                errors,
            )
        else:
            errors.append(f"{node_id}: reviewed CLI invocation policy is missing")
        argv_template = tool.get("argv_template")
        placeholders = {
            match.group(1)
            for token in argv_template or []
            if isinstance(token, str)
            for match in [re.fullmatch(r"<([a-z][a-z0-9_]*)>", token)]
            if match is not None
        }
        if argument_names != placeholders:
            errors.append(
                f"{node_id}: CLI arguments differ from placeholders "
                f"{sorted(placeholders)}"
            )
        target_policy = tool.get("argument_target_kinds")
        if not isinstance(target_policy, dict):
            errors.append(f"{node_id}: CLI target policy must be an object")
            target_policy = {}
        for name, expected_kind in target_policy.items():
            if binding_index.get(name, {}).get("target_kind") != expected_kind:
                errors.append(
                    f"{node_id}: CLI argument {name!r} needs {expected_kind!r}"
                )
        for name in placeholders:
            if name.endswith("_path") and name not in target_policy:
                actual_kind = binding_index.get(name, {}).get("target_kind")
                if actual_kind not in LOCAL_PATH_TARGET_KINDS:
                    errors.append(
                        f"{node_id}: CLI path placeholder lacks target binding"
                    )
        executable = node.get("executable")
        selected_executable = tool.get("resolved_executable")
        if not isinstance(executable, str):
            errors.append(f"{node_id}: CLI executable is required")
            executable_value = None
        else:
            derived = derive_permission_target(
                "executable",
                executable,
                False,
                tool,
                f"{node_id}.executable",
                errors,
            )
            executable_value = derived[1] if derived is not None else None
            if executable != selected_executable:
                errors.append(
                    f"{node_id}: executable text differs from selected contract"
                )
            try:
                selected_endpoint = canonical_data_endpoint(selected_executable)
            except (OSError, ValueError, TypeError) as error:
                errors.append(
                    f"{node_id}: invalid selected executable contract: {error}"
                )
            else:
                if executable_value != selected_endpoint:
                    errors.append(
                        f"{node_id}: executable differs from selected live contract"
                    )
            if derived is not None:
                expected_effects[derived[0]] = ("executable", derived[1])
        expected_argv: list[str] = []
        for token in argv_template or []:
            match = re.fullmatch(r"<([a-z][a-z0-9_]*)>", token)
            if match is None:
                expected_argv.append(token)
                continue
            value = arguments.get(match.group(1))
            if not isinstance(value, str) or not value:
                errors.append(
                    f"{node_id}: CLI placeholder {match.group(1)!r} must be a "
                    "non-empty string"
                )
                continue
            expected_argv.append(value)
        if node.get("materialized_argv") != expected_argv:
            errors.append(f"{node_id}: materialized argv differs from template")
        if executable_value is not None:
            argv_value = sha256_value([executable_value, *expected_argv])
            expected_effects[f"argv:{argv_value}"] = ("argv", argv_value)
        cwd = node.get("cwd")
        if not isinstance(cwd, str) or not cwd:
            errors.append(f"{node_id}: CLI cwd is required")
        else:
            derived = derive_permission_target(
                "cwd", cwd, False, tool, f"{node_id}.cwd", errors
            )
            if derived is not None:
                expected_effects[derived[0]] = ("cwd", derived[1])
        stdin_contract = tool.get("stdin_contract")
        stdin_payload = node.get("stdin_payload")
        if stdin_contract is None:
            if stdin_payload is not None:
                errors.append(f"{node_id}: CLI operation forbids stdin")
        elif not isinstance(stdin_payload, dict):
            errors.append(f"{node_id}: CLI operation requires object stdin")
        else:
            stdin_errors = validate_schema_subset(stdin_payload, stdin_contract)
            errors.extend(f"{node_id} stdin_payload: {error}" for error in stdin_errors)
            stdin_value = sha256_value(stdin_payload)
            expected_effects[f"stdin:{stdin_value}"] = ("stdin", stdin_value)

    side_effects = tool.get("catalog_side_effects")
    if isinstance(side_effects, dict):
        if tool.get("network_policy") == "no-network":
            expected_effects["network-endpoint:deny-all"] = (
                "network-endpoint",
                "deny-all",
            )
        if side_effects.get("uses_network") is True:
            if tool.get("network_policy") != "no-network" and not any(
                kind == "network-endpoint" and value != "deny-all"
                for kind, value in expected_effects.values()
            ):
                errors.append(f"{node_id}: network endpoint target is missing")
        if (
            side_effects.get("writes_files") is True
            and tool.get("overwrite_policy") != "read-only"
        ):
            write_kinds = {
                "output-root",
                "output-path",
                "file-write",
                "database",
                "qgis-state",
            }
            if not any(
                kind in write_kinds for kind, _value in expected_effects.values()
            ):
                errors.append(f"{node_id}: exact write target is missing")

    missing_effects = sorted(set(expected_effects) - set(effect_index))
    extra_effects = sorted(set(effect_index) - set(expected_effects))
    if missing_effects:
        errors.append(f"{node_id}: effect targets miss {missing_effects}")
    if extra_effects:
        errors.append(
            f"{node_id}: effect targets contain un-derived extras {extra_effects}"
        )


def validate_permission_audit(plan: dict[str, Any], errors: list[str]) -> None:
    tools = _indexed(plan.get("selected_tools"), "selected_tools", errors)
    nodes = _indexed(plan.get("workflow_nodes"), "workflow_nodes", errors)
    permissions = plan.get("permissions")
    if not isinstance(permissions, list):
        errors.append("permissions: must be an array")
        return
    tool_coverage: dict[str, list[dict[str, Any]]] = {key: [] for key in tools}
    node_coverage: dict[str, list[dict[str, Any]]] = {key: [] for key in nodes}
    permission_ids: set[str] = set()
    for index, permission in enumerate(permissions):
        if not isinstance(permission, dict):
            errors.append(f"permissions[{index}]: object required")
            continue
        permission_id = permission.get("id")
        if (
            not isinstance(permission_id, str)
            or re.fullmatch(r"PERM-[0-9]{3}", permission_id) is None
        ):
            errors.append(f"permissions[{index}].id: invalid")
        elif permission_id in permission_ids:
            errors.append(f"permissions: duplicate id {permission_id}")
        else:
            permission_ids.add(permission_id)
        tool_ids = permission.get("tool_ids", [])
        node_ids = permission.get("workflow_node_ids", [])
        if not isinstance(tool_ids, list) or not isinstance(node_ids, list):
            errors.append(f"{permission_id}: tool/node references must be arrays")
            continue
        for tool_id in tool_ids:
            if tool_id not in tools:
                errors.append(f"{permission_id}: unknown tool {tool_id!r}")
            else:
                tool_coverage[tool_id].append(permission)
        for node_id in node_ids:
            if node_id not in nodes:
                errors.append(f"{permission_id}: unknown node {node_id!r}")
            else:
                node_coverage[node_id].append(permission)
                node_tool = nodes[node_id].get("tool_id")
                if node_tool not in tool_ids:
                    errors.append(
                        f"{permission_id}: node permission must reference its tool"
                    )
        expected_targets: set[str] = set()
        for node_id in node_ids:
            expected_targets.update(
                target.get("permission_target")
                for target in nodes.get(node_id, {}).get("effect_targets", [])
                if isinstance(target, dict)
                and isinstance(target.get("permission_target"), str)
            )
        actual_targets = {
            target
            for target in permission.get("targets", [])
            if isinstance(target, str)
        }
        unexpected_targets = actual_targets - expected_targets
        if unexpected_targets:
            errors.append(
                f"{permission_id}: permission targets are not derived from effects"
            )

    def compatible(
        subject: dict[str, Any], coverage: list[dict[str, Any]]
    ) -> list[dict[str, Any]]:
        risk = subject.get("risk_level")
        policy = subject.get("approval_policy")
        result: list[dict[str, Any]] = []
        for permission in coverage:
            permission_risk = permission.get("risk_level")
            permission_policy = permission.get("approval_policy")
            if risk in RISK_RANK and permission_risk in RISK_RANK:
                if RISK_RANK[permission_risk] < RISK_RANK[risk]:
                    continue
            if risk == "R3" or policy == "per-invocation-approval":
                if permission_policy != "per-invocation-approval":
                    continue
            elif risk == "R2" and permission_policy not in {
                "plan-approval",
                "per-invocation-approval",
            }:
                continue
            result.append(permission)
        return result

    for tool_id, tool in tools.items():
        if tool.get("risk_level") not in {"R2", "R3"}:
            continue
        coverage = compatible(tool, tool_coverage[tool_id])
        if not coverage:
            errors.append(f"{tool_id}: compatible permission coverage is missing")
            continue
        covered = {
            target
            for permission in coverage
            for target in permission.get("targets", [])
            if isinstance(target, str)
        }
        expected = {
            target.get("permission_target")
            for node in nodes.values()
            if node.get("tool_id") == tool_id
            for target in node.get("effect_targets", [])
            if isinstance(target, dict)
            and isinstance(target.get("permission_target"), str)
        }
        missing = expected - covered
        if missing:
            errors.append(f"{tool_id}: permission targets miss {sorted(missing)}")
    for node_id, node in nodes.items():
        if node.get("risk_level") not in {"R2", "R3"}:
            continue
        subject = dict(node)
        subject["approval_policy"] = tools.get(node.get("tool_id"), {}).get(
            "approval_policy"
        )
        coverage = compatible(subject, node_coverage[node_id])
        if not coverage:
            errors.append(f"{node_id}: compatible permission coverage is missing")
            continue
        covered = {
            target
            for permission in coverage
            for target in permission.get("targets", [])
            if isinstance(target, str)
        }
        expected = {
            target.get("permission_target")
            for target in node.get("effect_targets", [])
            if isinstance(target, dict)
            and isinstance(target.get("permission_target"), str)
        }
        missing = expected - covered
        if missing:
            errors.append(f"{node_id}: permission targets miss {sorted(missing)}")


def validate_known_issue_acceptances(
    plan: dict[str, Any],
    capabilities: dict[str, dict[str, Any]],
    tools: dict[str, dict[str, Any]],
    errors: list[str],
) -> None:
    selected_capabilities = {
        tool.get("capability_id")
        for tool in tools.values()
        if isinstance(tool.get("capability_id"), str)
    }
    required = {
        (capability_id, issue)
        for capability_id in selected_capabilities
        for issue in capabilities.get(capability_id, {}).get("known_issues", [])
        if isinstance(issue, str)
    }
    records = plan.get("known_issue_acceptances")
    if not isinstance(records, list):
        errors.append("known_issue_acceptances: must be an array")
        return
    accepted: set[tuple[str, str]] = set()
    record_ids: set[str] = set()
    for index, record in enumerate(records):
        if not isinstance(record, dict):
            errors.append(f"known_issue_acceptances[{index}]: object required")
            continue
        record_id = record.get("id")
        if not isinstance(record_id, str) or record_id in record_ids:
            errors.append(f"known_issue_acceptances[{index}]: invalid or duplicate id")
        else:
            record_ids.add(record_id)
        capability_id = record.get("capability_id")
        issue = record.get("issue")
        key = (capability_id, issue)
        capability = capabilities.get(capability_id)
        if capability_id not in selected_capabilities or capability is None:
            errors.append(f"{record_id}: acceptance capability is not selected")
            continue
        if issue not in capability.get("known_issues", []):
            errors.append(f"{record_id}: acceptance issue is not exact")
        if key in accepted:
            errors.append(f"{record_id}: duplicate known-issue acceptance")
        accepted.add(key)
        expected = {
            "plan_id": plan.get("plan_id"),
            "revision": plan.get("revision"),
            "capability_description_hash": capability.get("description_hash"),
        }
        for field, value in expected.items():
            if record.get(field) != value:
                errors.append(
                    f"{record_id}: {field} differs from current revision binding"
                )
        for field in ("rationale", "acceptance_basis"):
            value = record.get(field)
            if not isinstance(value, str) or not value.strip():
                errors.append(f"{record_id}: {field} must be explicit")
    missing = sorted(required - accepted, key=repr)
    extra = sorted(accepted - required, key=repr)
    if missing:
        errors.append(f"known_issue_acceptances miss selected issues {missing}")
    if extra:
        errors.append(f"known_issue_acceptances contain unrelated issues {extra}")


def validate_geospatial_semantics(plan: dict[str, Any], errors: list[str]) -> None:
    semantics = plan.get("geospatial_semantics")
    if not isinstance(semantics, dict):
        return
    domains = semantics.get("domains")
    if not isinstance(domains, list) or not domains:
        errors.append("geospatial_semantics.domains: select at least one data domain")
        return
    for domain in domains:
        policy_field = DOMAIN_POLICY_FIELDS.get(domain)
        if policy_field is None:
            continue
        policy = semantics.get(policy_field)
        if not isinstance(policy, str) or not policy.strip():
            errors.append(
                f"geospatial_semantics.{policy_field}: selected domain {domain!r} "
                "requires an explicit policy"
            )
        elif NOT_APPLICABLE_RE.match(policy):
            errors.append(
                f"geospatial_semantics.{policy_field}: selected domain {domain!r} "
                "cannot be marked not-applicable"
            )


def validate_plan_semantics(
    plan: dict[str, Any], catalog: dict[str, Any], errors: list[str]
) -> None:
    validate_geospatial_semantics(plan, errors)
    snapshot = plan.get("capability_snapshot", {})
    capabilities = _indexed(snapshot.get("capabilities"), "capabilities", errors)
    tools = _indexed(plan.get("selected_tools"), "selected_tools", errors)
    nodes = _indexed(plan.get("workflow_nodes"), "workflow_nodes", errors)
    if not capabilities:
        errors.append(
            "capability_snapshot.capabilities: at least one live capability is required"
        )
    if not tools:
        errors.append(
            "selected_tools: at least one task-specific selection is required"
        )
    if not nodes:
        errors.append("workflow_nodes: at least one node is required")

    available_services = set(snapshot.get("available_services", []))
    available_providers = set(snapshot.get("available_providers", []))
    required_services = set(
        plan.get("environment_contract", {}).get("required_services", [])
    )
    required_providers = set(
        plan.get("environment_contract", {}).get("required_providers", [])
    )
    if not required_services.issubset(available_services):
        errors.append(
            "environment_contract.required_services contains unavailable services"
        )
    if not required_providers.issubset(available_providers):
        errors.append(
            "environment_contract.required_providers contains unavailable providers"
        )

    mcp_catalog, cli_catalog = _catalog_indexes(catalog, errors)
    for capability_id, capability in capabilities.items():
        validate_capability_identity(capability, capability_id, errors)
        validate_capability_state(capability, capability_id, errors)
        validate_reviewed_capability_contracts(capability_id, capability, errors)
        for schema_name in ("input_schema", "output_schema"):
            schema = capability.get(schema_name)
            if not isinstance(schema, dict):
                errors.append(f"{capability_id}: {schema_name} must be an object")
                continue
            if capability.get(f"{schema_name}_hash") != sha256_value(schema):
                errors.append(
                    f"{capability_id}: {schema_name}_hash differs from live schema"
                )
        description = capability.get("description")
        if not isinstance(description, str) or capability.get(
            "description_hash"
        ) != sha256_text(description or ""):
            errors.append(
                f"{capability_id}: description_hash differs from live description"
            )
        if capability.get("source_type") == "cli":
            baseline = cli_catalog.get(capability.get("cli_id"))
        else:
            baseline = mcp_catalog.get(
                (capability.get("service_id"), capability.get("tool_name"))
            )
        validate_live_capability_contract(capability_id, capability, baseline, errors)

    for tool_id, tool in tools.items():
        validate_capability_identity(tool, tool_id, errors)
        capability = capabilities.get(tool.get("capability_id"))
        if capability is None:
            errors.append(f"{tool_id}: selected capability does not exist")
            continue
        validate_selected_capability(tool_id, capability, errors)
        for field in (
            "catalog_ref",
            "source_type",
            "service_id",
            "tool_name",
            "algorithm_id",
            "cli_id",
            "provider",
            "description_hash",
            "input_schema_hash",
            "output_schema_hash",
        ):
            if tool.get(field) != capability.get(field):
                errors.append(f"{tool_id}: {field} differs from live capability")
        selected_bindings = {
            "risk_level": "reviewed_risk_level",
            "catalog_side_effects": "reviewed_side_effects",
            "invocation_policy": "reviewed_invocation_policy",
            "operation_id": "reviewed_operation_id",
            "argv_template": "reviewed_argv_template",
            "stdin_contract": "reviewed_stdin_contract",
            "argument_target_kinds": "reviewed_argument_target_kinds",
            "nested_target_kinds": "reviewed_nested_target_kinds",
            "nested_target_kinds_hash": "reviewed_nested_target_kinds_hash",
            "parameter_constraints": "reviewed_parameter_constraints",
            "parameter_constraints_hash": "reviewed_parameter_constraints_hash",
            "resolved_executable": "resolved_executable",
            "resolved_executable_sha256": "resolved_executable_sha256",
        }
        for selected_field, capability_field in selected_bindings.items():
            if tool.get(selected_field) != capability.get(capability_field):
                errors.append(
                    f"{tool_id}: {selected_field} differs from live capability"
                )
        nested_contract = tool.get("nested_target_kinds")
        if isinstance(nested_contract, dict) and tool.get(
            "nested_target_kinds_hash"
        ) != sha256_value(nested_contract):
            errors.append(
                f"{tool_id}: nested_target_kinds_hash differs from selected contract"
            )
        parameter_contract = tool.get("parameter_constraints")
        if isinstance(parameter_contract, dict) and tool.get(
            "parameter_constraints_hash"
        ) != sha256_value(parameter_contract):
            errors.append(
                f"{tool_id}: parameter_constraints_hash differs from selected contract"
            )

        validate_algorithm_binding(tool_id, tool, errors)

        source_type = tool.get("source_type")
        if source_type == "cli":
            policy = cli_catalog.get(tool.get("cli_id"))
        else:
            policy = mcp_catalog.get((tool.get("service_id"), tool.get("tool_name")))
        # The bundled catalog is a portable baseline, not a discovery allowlist.
        # A live capability absent from it remains reviewable through its complete
        # descriptor and schemas.  When a baseline entry exists, do not understate it.
        validate_catalog_policy_binding(tool_id, tool, policy, errors)
        if source_type == "cli":
            derive_permission_target(
                "executable",
                tool.get("resolved_executable"),
                False,
                tool,
                f"{tool_id}.resolved_executable",
                errors,
            )
            if not tool.get("allowed_cwd_roots"):
                errors.append(f"{tool_id}: CLI allowed_cwd_roots must not be empty")
        elif tool.get("invocation_policy", {}).get("command_shape") == "argv-only":
            if not tool.get("allowed_executable_roots"):
                errors.append(f"{tool_id}: argv-only MCP needs executable roots")
            if not tool.get("allowed_cwd_roots"):
                errors.append(f"{tool_id}: argv-only MCP needs cwd roots")
        if (
            tool.get("risk_level") in {"R2", "R3"}
            and tool.get("approval_required") is not True
        ):
            errors.append(
                f"{tool_id}: persistent or sensitive operation requires approval"
            )
        if (
            tool.get("risk_level") == "R3"
            and tool.get("approval_policy") != "per-invocation-approval"
        ):
            errors.append(f"{tool_id}: R3 requires per-invocation-approval")
        if tool.get("risk_level") == "R2" and tool.get("approval_policy") not in {
            "plan-approval",
            "per-invocation-approval",
        }:
            errors.append(f"{tool_id}: R2 requires a usable approval policy")
        if tool.get("approval_policy") == "default-deny":
            errors.append(f"{tool_id}: default-deny tool cannot be selected")

    validate_known_issue_acceptances(plan, capabilities, tools, errors)

    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(node_id: str) -> None:
        if node_id in visiting:
            errors.append(f"workflow_nodes: dependency cycle at {node_id}")
            return
        if node_id in visited:
            return
        visiting.add(node_id)
        for dependency in nodes[node_id].get("depends_on", []):
            if dependency not in nodes:
                errors.append(f"{node_id}: unknown dependency {dependency}")
            else:
                visit(dependency)
        visiting.remove(node_id)
        visited.add(node_id)

    for node_id, node in nodes.items():
        tool = tools.get(node.get("tool_id"))
        if tool is None:
            errors.append(f"{node_id}: unknown tool_id")
        else:
            if node.get("operation_id") != tool.get("operation_id"):
                errors.append(f"{node_id}: operation_id differs from selected tool")
            if set(node.get("arguments", {})) != {
                binding.get("name")
                for binding in node.get("argument_bindings", [])
                if isinstance(binding, dict)
            }:
                errors.append(
                    f"{node_id}: argument bindings must exactly cover arguments"
                )
            if tool.get("source_type") == "cli":
                policy = cli_catalog.get(tool.get("cli_id"))
            else:
                policy = mcp_catalog.get(
                    (tool.get("service_id"), tool.get("tool_name"))
                )
            validate_node_invocation(
                node_id,
                node,
                tool,
                capabilities.get(tool.get("capability_id"), {}),
                policy,
                errors,
            )
            node_risk = node.get("risk_level")
            tool_risk = tool.get("risk_level")
            if node_risk in RISK_RANK and tool_risk in RISK_RANK:
                if RISK_RANK[node_risk] < RISK_RANK[tool_risk]:
                    errors.append(f"{node_id}: risk_level understates selected tool")
        visit(node_id)

    validate_permission_audit(plan, errors)
    identity = plan.get("skill_identity", {})
    package = plan.get("target_skill_package", {})
    skill_name = identity.get("name")
    if not isinstance(skill_name, str) or not SKILL_NAME_RE.fullmatch(skill_name):
        errors.append("skill_identity.name: invalid strict skill name")
    if package.get("target_relative_path") != skill_name:
        errors.append("target_relative_path must equal skill_identity.name")
    validate_creation_policy(package, errors)
    files = package.get("files")
    if not isinstance(files, list) or not files:
        errors.append("target_skill_package.files: at least SKILL.md is required")
        return
    seen: set[str] = set()
    has_skill = False
    for index, entry in enumerate(files):
        if not isinstance(entry, dict):
            errors.append(f"target_skill_package.files[{index}]: object required")
            continue
        relative = entry.get("relative_path")
        if not is_relative_package_path(relative):
            errors.append(f"target_skill_package.files[{index}]: unsafe relative path")
            continue
        folded = relative.casefold()
        if folded in seen:
            errors.append(f"target_skill_package.files: duplicate path {relative}")
        seen.add(folded)
        expected_hash = entry.get("expected_sha256")
        if not isinstance(expected_hash, str) or not SHA256_RE.fullmatch(expected_hash):
            errors.append(f"target_skill_package.files[{index}]: invalid SHA-256")
        size = entry.get("expected_size_bytes")
        if (
            not isinstance(size, int)
            or isinstance(size, bool)
            or not 0 <= size <= 262144
        ):
            errors.append(f"target_skill_package.files[{index}]: invalid byte size")
        if relative == "SKILL.md" and entry.get("required") is True:
            has_skill = True
            if entry.get("content_kind") != "text-utf8":
                errors.append("target_skill_package.files: SKILL.md must be text-utf8")
    if not has_skill:
        errors.append("target_skill_package.files: required SKILL.md is missing")


def canonical_approval_statement(plan: dict[str, Any]) -> str:
    return (
        f"批准计划 {plan.get('plan_id')} revision {plan.get('revision')}，"
        "按该计划创建。"
    )


def parse_aware_datetime(
    value: Any, label: str, errors: list[str]
) -> dt.datetime | None:
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{label} must be a non-empty aware ISO-8601 timestamp")
        return None
    try:
        parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        errors.append(f"{label} must be a valid ISO-8601 timestamp")
        return None
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        errors.append(f"{label} must include an explicit timezone")
        return None
    return parsed


def validate_approval_bindings(
    plan: dict[str, Any], computed: dict[str, str], errors: list[str]
) -> None:
    """Bind approved-or-later plans to the exact reviewed conversation state."""
    if plan.get("status") not in APPROVED_OR_LATER_STATUSES:
        return
    approval = plan.get("approval")
    if not isinstance(approval, dict):
        errors.append("approval: approved or later plans require an approval object")
        return
    target = plan.get("target_skill_package")
    target_relative_path = (
        target.get("target_relative_path") if isinstance(target, dict) else None
    )
    expected = {
        "state": "approved",
        "approved_revision": plan.get("revision"),
        "approved_plan_hash": computed.get("plan_hash"),
        "approved_capability_hash": computed.get("capability_hash"),
        "approved_review_markdown_hash": computed.get("review_markdown_hash"),
        "approved_target_relative_path": target_relative_path,
        "intent": "create-current-plan",
        "user_statement": canonical_approval_statement(plan),
    }
    for field, expected_value in expected.items():
        if approval.get(field) != expected_value:
            errors.append(f"approval.{field} differs from the current approved plan")
    created_at = parse_aware_datetime(plan.get("created_at"), "created_at", errors)
    updated_at = parse_aware_datetime(plan.get("updated_at"), "updated_at", errors)
    approved_at = parse_aware_datetime(
        approval.get("approved_at"), "approval.approved_at", errors
    )
    if created_at is not None and updated_at is not None:
        if created_at > updated_at:
            errors.append("created_at must be less than or equal to updated_at")
    if updated_at is not None and approved_at is not None:
        if updated_at > approved_at:
            errors.append("updated_at must be less than or equal to approved_at")


def validate_common(
    plan: dict[str, Any],
    catalog: dict[str, Any],
    plan_schema: dict[str, Any],
    catalog_schema: dict[str, Any],
) -> tuple[list[str], dict[str, str]]:
    errors = [
        *(
            f"plan schema: {error}"
            for error in validate_schema_subset(plan, plan_schema)
        ),
        *(
            f"catalog schema: {error}"
            for error in validate_schema_subset(catalog, catalog_schema)
        ),
    ]
    catalog_hash = compute_catalog_hash(catalog)
    snapshot = plan.get("capability_snapshot", {})
    capability_hash = (
        compute_capability_hash(snapshot) if isinstance(snapshot, dict) else ""
    )
    plan_hash = compute_plan_hash(plan)
    computed = {
        "catalog_hash": catalog_hash,
        "capability_hash": capability_hash,
        "plan_hash": plan_hash,
        "review_markdown_hash": sha256_text(render_review_markdown(plan)),
    }
    validate_approval_bindings(plan, computed, errors)
    if isinstance(snapshot, dict):
        if snapshot.get("catalog_version") != catalog.get("catalog_version"):
            errors.append("capability_snapshot.catalog_version differs from catalog")
        if snapshot.get("catalog_hash") != catalog_hash:
            errors.append("capability_snapshot.catalog_hash differs from catalog")
        if snapshot.get("snapshot_hash") != capability_hash:
            errors.append("capability_snapshot.snapshot_hash differs from content")
        for key in ("available_services", "available_providers"):
            values = snapshot.get(key)
            if isinstance(values, list) and (
                values != sorted(values) or len(values) != len(set(values))
            ):
                errors.append(f"capability_snapshot.{key} must be sorted and unique")
    if plan.get("plan_hash") != plan_hash:
        errors.append("plan_hash differs from current plan revision")
    placeholders = find_placeholders(plan)
    if placeholders:
        errors.append(f"unreplaced placeholders at {placeholders[:20]}")
    validate_plan_semantics(plan, catalog, errors)
    if not plan.get("validation_matrix"):
        errors.append("validation_matrix: at least one case is required")
    if plan.get("unresolved_decisions") and plan.get("status") in {
        "approved",
        "implementing",
        "validating",
        "completed",
    }:
        errors.append("approved or later plans cannot contain unresolved decisions")
    return errors, computed


def parse_skill_frontmatter(text: str, errors: list[str]) -> dict[str, str]:
    if not text.startswith("---\n"):
        errors.append("SKILL.md must begin with an exact frontmatter delimiter")
        return {}
    closing = text.find("\n---\n", 4)
    if closing < 0:
        errors.append("SKILL.md has no exact closing frontmatter delimiter")
        return {}
    if not text[closing + 5 :].strip():
        errors.append("SKILL.md body must not be empty")
    parsed: dict[str, str] = {}
    for line_number, line in enumerate(text[4:closing].split("\n"), start=2):
        if not line or line.startswith((" ", "\t", "#")) or ":" not in line:
            errors.append(f"SKILL.md frontmatter line {line_number} is not canonical")
            continue
        key, raw = line.split(":", 1)
        if key not in {"name", "description"}:
            errors.append(f"SKILL.md frontmatter field {key!r} is forbidden")
            continue
        if key in parsed:
            errors.append(f"SKILL.md frontmatter field {key!r} is duplicated")
            continue
        raw = raw.lstrip(" ")
        if key == "name" and SKILL_NAME_RE.fullmatch(raw):
            parsed[key] = raw
            continue
        try:
            value = json.loads(raw)
        except json.JSONDecodeError:
            errors.append(f"SKILL.md frontmatter field {key!r} must be a quoted scalar")
            continue
        if not isinstance(value, str):
            errors.append(f"SKILL.md frontmatter field {key!r} must be a string")
            continue
        parsed[key] = value
    if set(parsed) != {"name", "description"}:
        errors.append("SKILL.md frontmatter must contain only name and description")
    return parsed


def _parse_quoted_yaml_scalar(raw: str) -> str | None:
    raw = raw.strip()
    if raw.startswith('"'):
        try:
            value = json.loads(raw)
        except json.JSONDecodeError:
            return None
        return value if isinstance(value, str) else None
    if len(raw) >= 2 and raw.startswith("'") and raw.endswith("'"):
        body = raw[1:-1]
        if "'" in body.replace("''", ""):
            return None
        return body.replace("''", "'")
    return None


def validate_openai_yaml(text: str, skill_name: str | None, errors: list[str]) -> None:
    """Validate the dependency-free core interface subset of agents/openai.yaml."""

    if "\t" in text:
        errors.append("agents/openai.yaml must use spaces, not tabs")
    lines = text.splitlines()
    interface_lines = [index for index, line in enumerate(lines) if line == "interface:"]
    if len(interface_lines) != 1:
        errors.append("agents/openai.yaml must contain one top-level interface mapping")
        return
    values: dict[str, str] = {}
    start = interface_lines[0] + 1
    for line_number in range(start, len(lines)):
        line = lines[line_number]
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if not line.startswith((" ", "\t")):
            break
        match = re.fullmatch(r"  ([a-z][a-z0-9_]*):\s*(.+)", line)
        if match is None:
            if not line.startswith("    "):
                errors.append(
                    f"agents/openai.yaml line {line_number + 1} is not a canonical interface field"
                )
            continue
        key, raw = match.groups()
        if key in values:
            errors.append(f"agents/openai.yaml interface field {key!r} is duplicated")
            continue
        value = _parse_quoted_yaml_scalar(raw)
        if value is None:
            errors.append(
                f"agents/openai.yaml interface field {key!r} must be a quoted string"
            )
            continue
        values[key] = value
    required = {"display_name", "short_description", "default_prompt"}
    missing = sorted(required - set(values))
    if missing:
        errors.append(f"agents/openai.yaml interface misses required fields: {missing}")
    display_name = values.get("display_name", "")
    if not display_name.strip():
        errors.append("agents/openai.yaml display_name must not be empty")
    short_description = values.get("short_description", "")
    if "short_description" in values and not 25 <= len(short_description) <= 64:
        errors.append(
            "agents/openai.yaml short_description must contain 25 to 64 characters"
        )
    default_prompt = values.get("default_prompt", "")
    expected_reference = f"${skill_name}" if isinstance(skill_name, str) else None
    if "default_prompt" in values and (
        expected_reference is None or expected_reference not in default_prompt
    ):
        errors.append(
            "agents/openai.yaml default_prompt must explicitly mention the exact $skill-name"
        )


def validate_verify_status(plan: dict[str, Any], errors: list[str]) -> None:
    if plan.get("status") not in APPROVED_OR_LATER_STATUSES:
        errors.append("verify-staged requires plan status approved or later")


def validate_markdown_links(
    text: str, source_path: Path, staged_root: Path, errors: list[str]
) -> None:
    matches = [
        *MARKDOWN_LINK_RE.finditer(text),
        *MARKDOWN_REFERENCE_RE.finditer(text),
    ]
    for match in matches:
        raw_target = match.group(1) or match.group(2) or ""
        if raw_target.startswith("#"):
            continue
        try:
            parsed = urlsplit(raw_target)
        except ValueError:
            errors.append(f"{source_path.name}: malformed Markdown link")
            continue
        if parsed.scheme or parsed.netloc:
            if parsed.scheme.casefold() in {"http", "https", "mailto"}:
                continue
            errors.append(f"{source_path.name}: local Markdown link is absolute")
            continue
        relative_target = unquote(parsed.path)
        if not relative_target:
            continue
        if (
            relative_target.startswith(("/", "\\"))
            or "\\" in relative_target
            or re.match(r"^[A-Za-z]:", relative_target)
        ):
            errors.append(f"{source_path.name}: unsafe Markdown relative link")
            continue
        resolved = (source_path.parent / relative_target).resolve(strict=False)
        if not is_within(resolved, staged_root):
            errors.append(f"{source_path.name}: Markdown link escapes staged tree")
        elif not resolved.exists():
            errors.append(f"{source_path.name}: Markdown link target is missing")


def validate_staged_text(
    relative: str,
    path: Path,
    text: str,
    staged_root: Path,
    plan: dict[str, Any],
    errors: list[str],
) -> None:
    if TEMPLATE_TEXT_RE.search(text):
        errors.append(f"staged text contains TODO or template placeholder: {relative}")
    if contains_obvious_machine_path(text):
        errors.append(f"staged text contains an obvious machine path: {relative}")
    environment = plan.get("environment_contract")
    skills_contract = (
        environment.get("skills_root") if isinstance(environment, dict) else None
    )
    folded_text = text.replace("\\", "/").casefold()
    if isinstance(skills_contract, dict):
        for field in ("display_path", "manifest_path"):
            value = skills_contract.get(field)
            if isinstance(value, str) and value.strip():
                blocked = value.replace("\\", "/").casefold()
                if blocked in folded_text:
                    errors.append(
                        f"staged text embeds plan {field} absolute path: {relative}"
                    )
    validate_markdown_links(text, path, staged_root, errors)
    if relative == "SKILL.md":
        if len(text.splitlines()) >= 500:
            errors.append("SKILL.md must contain fewer than 500 lines")
        identity = plan.get("skill_identity", {})
        frontmatter = parse_skill_frontmatter(text, errors)
        if frontmatter.get("name") != identity.get("name"):
            errors.append("SKILL.md name differs from skill_identity.name")
        if frontmatter.get("description") != identity.get("description"):
            errors.append("SKILL.md description differs from skill identity")
    elif relative.casefold() == "agents/openai.yaml":
        identity = plan.get("skill_identity", {})
        skill_name = identity.get("name") if isinstance(identity, dict) else None
        validate_openai_yaml(text, skill_name, errors)


def sniff_utf8_text(raw: bytes) -> str | None:
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        return None
    if "\x00" in text:
        return None
    if any(ord(character) < 32 and character not in "\t\n\r" for character in text):
        return None
    return text


def validate_staged_package(
    plan: dict[str, Any],
    staged_root_value: Path | str | None,
    manifest_value: Path | str | None,
    skills_root_value: Path | str | None,
    errors: list[str],
) -> None:
    validate_verify_status(plan, errors)
    staged_root = validate_staged_path_binding(
        plan,
        staged_root_value,
        manifest_value,
        skills_root_value,
        errors,
    )
    if staged_root is None:
        return
    package = plan.get("target_skill_package", {})
    entries = package.get("files", [])
    expected: dict[str, dict[str, Any]] = {}
    expected_directories = {"."}
    for entry in entries if isinstance(entries, list) else []:
        if not isinstance(entry, dict) or not is_relative_package_path(
            entry.get("relative_path")
        ):
            continue
        relative = entry["relative_path"]
        expected[relative.casefold()] = entry
        parent = PurePosixPath(relative).parent
        while str(parent) not in {"", "."}:
            expected_directories.add(str(parent).casefold())
            parent = parent.parent

    actual: dict[str, Path] = {}
    actual_directories = {"."}
    for current, directory_names, file_names in os.walk(
        staged_root, topdown=True, followlinks=False
    ):
        current_path = Path(current)
        safe_directories: list[str] = []
        for name in directory_names:
            path = current_path / name
            if is_reparse_point(path):
                errors.append(f"staged package contains reparse directory: {path}")
                continue
            actual_directories.add(path.relative_to(staged_root).as_posix().casefold())
            safe_directories.append(name)
        directory_names[:] = safe_directories
        for name in file_names:
            path = current_path / name
            if is_reparse_point(path):
                errors.append(f"staged package contains reparse file: {path}")
                continue
            relative = path.relative_to(staged_root).as_posix()
            if not is_relative_package_path(relative):
                errors.append(f"staged package contains unsafe path: {relative}")
                continue
            folded = relative.casefold()
            if folded in actual:
                errors.append(
                    f"staged package has case-insensitive duplicate: {relative}"
                )
            actual[folded] = path
    missing = sorted(set(expected) - set(actual))
    extra = sorted(set(actual) - set(expected))
    extra_directories = sorted(actual_directories - expected_directories)
    if missing:
        errors.append(f"staged package is missing planned files: {missing}")
    if extra:
        errors.append(f"staged package contains unplanned files: {extra}")
    if extra_directories:
        errors.append(
            f"staged package contains unplanned directories: {extra_directories}"
        )

    for folded, entry in expected.items():
        path = actual.get(folded)
        if path is None:
            continue
        raw = path.read_bytes()
        relative = entry["relative_path"]
        if len(raw) > 262144:
            errors.append(f"staged file exceeds 256 KiB: {relative}")
        if sha256_bytes(raw) != entry.get("expected_sha256"):
            errors.append(f"staged file hash differs: {relative}")
        if len(raw) != entry.get("expected_size_bytes"):
            errors.append(f"staged file size differs: {relative}")
        content_kind = entry.get("content_kind")
        text = sniff_utf8_text(raw)
        if content_kind == "text-utf8" and text is None:
            errors.append(f"declared staged text file is not UTF-8: {relative}")
        if text is not None:
            if content_kind != "text-utf8":
                errors.append(
                    f"actual UTF-8 text must declare content_kind text-utf8: {relative}"
                )
            if raw.startswith(b"\xef\xbb\xbf"):
                errors.append(f"staged text file contains a UTF-8 BOM: {relative}")
            if "\r" in text:
                errors.append(f"staged text file must use LF: {relative}")
            if not raw.endswith(b"\n"):
                errors.append(f"staged text file needs a final newline: {relative}")
            validate_staged_text(relative, path, text, staged_root, plan, errors)


def _finalize_fixture_plan(
    plan: dict[str, Any], *, approve: bool = False
) -> dict[str, Any]:
    """Recompute mutable hashes and optional approval for test-only fixtures."""
    result = copy.deepcopy(plan)
    snapshot = result["capability_snapshot"]
    snapshot["snapshot_hash"] = compute_capability_hash(snapshot)
    result["status"] = "draft"
    result["updated_at"] = "2030-01-01T00:01:00Z"
    result["approval"] = {
        "required": True,
        "state": "pending",
        "approved_revision": None,
        "approved_plan_hash": None,
        "approved_capability_hash": None,
        "approved_review_markdown_hash": None,
        "approved_target_relative_path": None,
        "intent": None,
        "user_statement": None,
        "approved_at": None,
    }
    result["plan_hash"] = compute_plan_hash(result)
    if approve:
        result["status"] = "approved"
        result["approval"] = {
            "required": True,
            "state": "approved",
            "approved_revision": result["revision"],
            "approved_plan_hash": result["plan_hash"],
            "approved_capability_hash": snapshot["snapshot_hash"],
            "approved_review_markdown_hash": sha256_text(
                render_review_markdown(result)
            ),
            "approved_target_relative_path": result["target_skill_package"][
                "target_relative_path"
            ],
            "intent": "create-current-plan",
            "user_statement": canonical_approval_statement(result),
            "approved_at": "2030-01-01T00:02:00Z",
        }
    return result


def _effect_fixture(kind: str, value: str) -> dict[str, str]:
    return {
        "kind": kind,
        "value": value,
        "permission_target": f"{kind}:{value}",
        "rationale": "Exact deterministic invocation effect.",
    }


def _build_cli_fixture(temp_root_value: Path | str) -> dict[str, Any]:
    """Build a complete CLI plan and staged package under a caller temp root."""
    temp_root = Path(temp_root_value).resolve(strict=False)
    system_temp = Path(tempfile.gettempdir()).resolve(strict=False)
    if (
        temp_root == system_temp
        or not is_within(temp_root, system_temp)
        or not temp_root.is_dir()
        or any(temp_root.iterdir())
    ):
        raise GateInputError(
            "fixture root must be an existing empty child of the system temp root"
        )
    service_root = temp_root / "service"
    skills_root = service_root / "Skills"
    creator_root = skills_root / "qgis-skills-creator"
    staged_root = skills_root / "sample-skill"
    references_root = staged_root / "references"
    runtime_root = temp_root / "runtime"
    executable_root = runtime_root / "bin"
    work_root = temp_root / "work"
    input_root = work_root / "input"
    output_root = work_root / "output"
    cwd_root = work_root / "cwd"
    for directory in (
        creator_root,
        references_root,
        executable_root,
        input_root,
        output_root,
        cwd_root,
    ):
        directory.mkdir(parents=True, exist_ok=True)

    executable_path = executable_root / "qgis-process-fixture.bat"
    executable_path.write_bytes(b"@echo off\r\n")
    input_path = input_root / "source.gpkg"
    output_path = output_root / "result.gpkg"
    input_path.write_bytes(b"fixture")
    manifest = {
        "service_id": "qcopilots.mcp_server_skills",
        "skillsRoot": "Skills",
    }
    manifest_path = service_root / "qcopilots_service.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    skill_description = "Run one reviewed fixture Processing workflow."
    skill_text = (
        "---\n"
        "name: sample-skill\n"
        f"description: {json.dumps(skill_description)}\n"
        "---\n\n"
        "# Sample skill\n\n"
        "Read [the contract](references/contract) before execution.\n"
    )
    contract_text = "# Contract\n\nValidate every reviewed target.\n"
    skill_path = staged_root / "SKILL.md"
    contract_path = references_root / "contract"
    skill_path.write_text(skill_text, encoding="utf-8", newline="\n")
    contract_path.write_text(contract_text, encoding="utf-8", newline="\n")

    skill_root = Path(__file__).resolve().parent.parent
    plan_schema_path = skill_root / "references" / "qgis-skill-plan.schema.json"
    catalog_path = skill_root / "references" / "qgis-tools.catalog.json"
    catalog_schema_path = skill_root / "references" / "qgis-tools.catalog.schema.json"
    plan_schema = load_strict_json(plan_schema_path)
    catalog = load_strict_json(catalog_path)
    catalog_schema = load_strict_json(catalog_schema_path)
    if not all(
        isinstance(value, dict) for value in (plan_schema, catalog, catalog_schema)
    ):
        raise GateInputError("fixture schemas and catalog must be objects")

    input_schema = {
        "type": "object",
        "additionalProperties": False,
        "required": ["algorithm_id"],
        "properties": {"algorithm_id": {"type": "string", "const": "native:buffer"}},
    }
    output_schema = {
        "type": "object",
        "additionalProperties": True,
    }
    stdin_contract = {
        "type": "object",
        "additionalProperties": False,
        "required": ["inputs"],
        "properties": {
            "inputs": {
                "type": "object",
                "additionalProperties": False,
                "required": ["INPUT", "OUTPUT"],
                "properties": {
                    "INPUT": {"type": "string", "minLength": 1},
                    "OUTPUT": {"type": "string", "minLength": 1},
                },
            }
        },
    }
    nested_target_kinds = {
        "/stdin_payload/inputs/INPUT": "input-path",
        "/stdin_payload/inputs/OUTPUT": "output-path",
    }
    invocation_policy = {
        "require_all_schema_properties_explicit": True,
        "required_arguments": ["algorithm_id"],
        "required_any_argument_groups": [],
        "required_any_target_argument_groups": [],
        "required_target_arguments": [],
        "command_shape": "argv-only",
    }
    side_effects = {
        "reads_files": True,
        "writes_files": True,
        "modifies_qgis_state": False,
        "uses_network": False,
        "runs_shell": True,
        "destructive_possible": True,
    }
    description = "Live reviewed CLI fixture for one exact Processing algorithm."
    executable_hash = canonical_file_sha256(executable_path)
    argv_template = ["--json", "run", "<algorithm_id>", "-"]
    capability = {
        "id": "CAP-001",
        "catalog_ref": "live:cli#qgis-process-fixture",
        "source_type": "cli",
        "service_id": None,
        "tool_name": None,
        "algorithm_id": "native:buffer",
        "cli_id": "qgis-process-fixture",
        "provider": "native",
        "description": description,
        "description_hash": sha256_text(description),
        "observed_version": "fixture-1",
        "discovered": True,
        "eligible": True,
        "eligibility_evidence": "Observed in the isolated fixture runtime.",
        "ineligibility_reason": None,
        "available": True,
        "deprecated": False,
        "known_issues": [],
        "input_schema": input_schema,
        "output_schema": output_schema,
        "input_schema_hash": sha256_value(input_schema),
        "output_schema_hash": sha256_value(output_schema),
        "discovery_evidence": "Exact executable and schemas were inspected.",
        "risk_review_status": "reviewed",
        "reviewed_risk_level": "R3",
        "reviewed_side_effects": copy.deepcopy(side_effects),
        "reviewed_invocation_policy": copy.deepcopy(invocation_policy),
        "reviewed_operation_id": "run-json-stdin",
        "reviewed_argv_template": copy.deepcopy(argv_template),
        "reviewed_stdin_contract": copy.deepcopy(stdin_contract),
        "reviewed_argument_target_kinds": {},
        "reviewed_nested_target_kinds": copy.deepcopy(nested_target_kinds),
        "reviewed_nested_target_kinds_hash": sha256_value(nested_target_kinds),
        "nested_target_review_evidence": (
            "Each structured stdin leaf was manually classified."
        ),
        "reviewed_parameter_constraints": copy.deepcopy(input_schema),
        "reviewed_parameter_constraints_hash": sha256_value(input_schema),
        "parameter_constraints_review_evidence": (
            "The exact algorithm argument schema was manually reviewed."
        ),
        "risk_review_evidence": "Manual fail-closed review of process effects.",
        "resolved_executable": str(executable_path),
        "resolved_executable_sha256": executable_hash,
        "executable_evidence": "The exact local wrapper was hashed.",
    }
    selected_tool = {
        "id": "TOOL-001",
        "capability_id": "CAP-001",
        "source_type": "cli",
        "catalog_ref": capability["catalog_ref"],
        "service_id": None,
        "tool_name": None,
        "algorithm_id": "native:buffer",
        "cli_id": "qgis-process-fixture",
        "provider": "native",
        "operation_id": "run-json-stdin",
        "argv_template": copy.deepcopy(argv_template),
        "argument_target_kinds": {},
        "nested_target_kinds": copy.deepcopy(nested_target_kinds),
        "nested_target_kinds_hash": sha256_value(nested_target_kinds),
        "stdin_contract": copy.deepcopy(stdin_contract),
        "description_hash": capability["description_hash"],
        "exact_operation": "Run native:buffer through structured stdin.",
        "purpose": "Exercise the complete reviewed CLI contract.",
        "availability_evidence": "Fixture executable exists and is hashed.",
        "risk_level": "R3",
        "catalog_side_effects": copy.deepcopy(side_effects),
        "invocation_policy": copy.deepcopy(invocation_policy),
        "input_schema_hash": capability["input_schema_hash"],
        "output_schema_hash": capability["output_schema_hash"],
        "parameter_constraints": copy.deepcopy(input_schema),
        "parameter_constraints_hash": sha256_value(input_schema),
        "allowed_algorithm_ids": ["native:buffer"],
        "allowed_input_roots": [str(input_root)],
        "allowed_output_roots": [str(output_root)],
        "allowed_executable_roots": [str(executable_root)],
        "allowed_cwd_roots": [str(cwd_root)],
        "resolved_executable": str(executable_path),
        "resolved_executable_sha256": executable_hash,
        "overwrite_policy": "fail-if-exists",
        "network_policy": "no-network",
        "qgis_state_changes": [],
        "approval_policy": "per-invocation-approval",
        "timeout_and_cancel": "Use a bounded timeout and propagate cancellation.",
        "idempotency": "Fail if the exact output already exists.",
        "rollback": "Remove only an approved incomplete fixture output.",
        "fallback": None,
        "approval_required": True,
    }
    arguments = {"algorithm_id": "native:buffer"}
    stdin_payload = {"inputs": {"INPUT": str(input_path), "OUTPUT": str(output_path)}}
    canonical_input = canonical_data_endpoint(str(input_path))
    canonical_output = canonical_data_endpoint(str(output_path))
    canonical_executable = canonical_data_endpoint(str(executable_path))
    canonical_cwd = canonical_data_endpoint(str(cwd_root))
    materialized_argv = ["--json", "run", "native:buffer", "-"]
    expected_effects = [
        _effect_fixture("arguments", sha256_value(arguments)),
        _effect_fixture("input-root", canonical_data_endpoint(str(input_root))),
        _effect_fixture("output-root", canonical_data_endpoint(str(output_root))),
        _effect_fixture(
            "executable-root", canonical_data_endpoint(str(executable_root))
        ),
        _effect_fixture("cwd-root", canonical_data_endpoint(str(cwd_root))),
        _effect_fixture("input-path", canonical_input),
        _effect_fixture("output-path", canonical_output),
        _effect_fixture("executable", canonical_executable),
        _effect_fixture(
            "argv",
            sha256_value([canonical_executable, *materialized_argv]),
        ),
        _effect_fixture("cwd", canonical_cwd),
        _effect_fixture("stdin", sha256_value(stdin_payload)),
        _effect_fixture("network-endpoint", "deny-all"),
    ]
    node = {
        "id": "STEP-001",
        "title": "Run the exact fixture algorithm",
        "depends_on": [],
        "tool_id": "TOOL-001",
        "operation_id": "run-json-stdin",
        "arguments": arguments,
        "argument_bindings": [
            {
                "name": "algorithm_id",
                "source_kind": "literal",
                "source_ref": None,
                "constraint_schema": {
                    "type": "string",
                    "const": "native:buffer",
                },
                "target_kind": "none",
                "permission_target": None,
                "sensitive": False,
            }
        ],
        "nested_target_bindings": [
            {
                "json_pointer": "/stdin_payload/inputs/INPUT",
                "target_kind": "input-path",
                "permission_target": f"input-path:{canonical_input}",
                "sensitive": False,
                "rationale": "Contained Processing input dataset.",
            },
            {
                "json_pointer": "/stdin_payload/inputs/OUTPUT",
                "target_kind": "output-path",
                "permission_target": f"output-path:{canonical_output}",
                "sensitive": False,
                "rationale": "Contained Processing output dataset.",
            },
        ],
        "omitted_optional_arguments": [],
        "stdin_payload": stdin_payload,
        "executable": str(executable_path),
        "materialized_argv": materialized_argv,
        "cwd": str(cwd_root),
        "effect_targets": expected_effects,
        "exact_operation": "Run exact native:buffer with structured stdin.",
        "inputs": ["IN-001"],
        "parameter_sources": {"algorithm_id": "approved literal"},
        "validated_parameters": arguments,
        "outputs": ["OUT-001"],
        "intermediates": [],
        "preconditions": ["The input is inside the reviewed input root."],
        "postconditions": ["The output passes content validation."],
        "side_effects": ["Read one input and write one new output."],
        "risk_level": "R3",
        "timeout_and_cancel": "Bound timeout and cancellation.",
        "retry_and_idempotency": "No retry after a partial write.",
        "fallback_condition": None,
        "acceptance_evidence": ["Validated output metadata."],
        "cleanup": "Remove only the isolated incomplete output when approved.",
    }
    manifest_hash = sha256_value(manifest)
    files = []
    for relative, text in (
        ("SKILL.md", skill_text),
        ("references/contract", contract_text),
    ):
        raw = text.encode("utf-8")
        files.append(
            {
                "relative_path": relative,
                "purpose": "Required reviewed fixture text.",
                "required": True,
                "source": "generated",
                "content_kind": "text-utf8",
                "expected_sha256": sha256_bytes(raw),
                "expected_size_bytes": len(raw),
            }
        )
    plan = {
        "$schema": str(plan_schema_path),
        "plan_schema_version": "3.1.0",
        "plan_id": "qgis-skill-plan-cli-fixture",
        "revision": 1,
        "status": "draft",
        "created_at": "2030-01-01T00:00:00Z",
        "updated_at": "2030-01-01T00:01:00Z",
        "plan_hash": ZERO_HASH,
        "request_summary": {
            "verbatim_request": "Create the isolated CLI fixture skill.",
            "interpreted_goal": "Review one deterministic Processing workflow.",
            "user_language": "en",
        },
        "scope": {
            "in_scope": ["One exact Processing invocation."],
            "out_of_scope": ["Live QGIS project edits."],
            "non_goals": ["General shell execution."],
        },
        "facts": [
            {
                "id": "REQ-001",
                "status": "CONFIRMED",
                "statement": "The fixture uses isolated local roots.",
                "evidence": "The builder created exact temporary roots.",
                "impact_if_wrong": "Containment validation would fail.",
            }
        ],
        "skill_identity": {
            "name": "sample-skill",
            "display_name": "Sample Skill",
            "description": skill_description,
            "compatibility": "Requires the reviewed isolated fixture runtime.",
        },
        "usage_scenarios": {
            "triggers": ["Run the isolated fixture workflow."],
            "positive_examples": ["Process the reviewed fixture input."],
            "negative_examples": ["Run an arbitrary shell command."],
            "prerequisites": ["Exact fixture roots are available."],
        },
        "input_contracts": [
            {
                "id": "IN-001",
                "role": "input",
                "format": "GeoPackage",
                "source_or_destination": "Reviewed isolated input.",
                "crs": "EPSG:4326",
                "data_type": "vector",
                "schema_or_bands": "One geometry layer.",
                "quality_constraints": ["Input opens successfully."],
                "overwrite_policy": "read-only",
            }
        ],
        "output_contracts": [
            {
                "id": "OUT-001",
                "role": "output",
                "format": "GeoPackage",
                "source_or_destination": "Reviewed isolated output.",
                "crs": "EPSG:4326",
                "data_type": "vector",
                "schema_or_bands": "One buffered geometry layer.",
                "quality_constraints": ["Output opens successfully."],
                "overwrite_policy": "fail-if-exists",
            }
        ],
        "geospatial_semantics": {
            "domains": ["vector"],
            "crs_policy": "Preserve the confirmed CRS.",
            "vertical_crs_policy": "Not applicable because the fixture is two-dimensional.",
            "units_policy": "Use CRS units.",
            "temporal_policy": "Not applicable because the fixture is not temporal.",
            "geometry_policy": "Reject invalid input geometry.",
            "raster_policy": "Not applicable because raster is not selected.",
            "point_cloud_policy": "Not applicable because point-cloud is not selected.",
            "mesh_policy": "Not applicable because mesh is not selected.",
            "three_d_policy": "Not applicable because three-dimensional is not selected.",
            "service_policy": "Not applicable because geospatial-service is not selected.",
            "tabular_policy": "Not applicable because tabular is not selected.",
            "quality_policy": "Validate output content and metadata.",
        },
        "environment_contract": {
            "execution_mode": "qgis-process-headless",
            "qgis_version_observed": "fixture-1",
            "qt_major": 6,
            "required_services": [],
            "required_providers": ["native"],
            "workspace_roots": [str(work_root)],
            "skills_root": {
                "display_path": normalized_resolved_path(skills_root),
                "manifest_path": normalized_resolved_path(manifest_path),
                "manifest_hash": manifest_hash,
                "identity_hash": compute_skills_root_identity(
                    manifest_hash, skills_root
                ),
            },
            "temporary_root_policy": "Use only the isolated fixture work root.",
            "project_path": None,
        },
        "capability_snapshot": {
            "catalog_version": catalog["catalog_version"],
            "discovered_at": "2030-01-01T00:00:00Z",
            "mcp_protocol_version": "fixture-1",
            "qgis_version": "fixture-1",
            "available_services": [],
            "available_providers": ["native"],
            "catalog_hash": compute_catalog_hash(catalog),
            "capabilities": [capability],
            "snapshot_hash": ZERO_HASH,
            "notes": "Isolated fixture capability snapshot.",
        },
        "selected_tools": [selected_tool],
        "known_issue_acceptances": [],
        "workflow_nodes": [node],
        "target_skill_package": {
            "skills_root_role": "qcopilots-skills-root",
            "target_relative_path": "sample-skill",
            "collision_policy": "fail-no-overwrite",
            "staging_policy": "resources-first-skill-md-last",
            "files": files,
        },
        "permissions": [
            {
                "id": "PERM-001",
                "action": "Invoke one exact isolated CLI process.",
                "scope": "Only the reviewed node and deterministic effects.",
                "tool_ids": ["TOOL-001"],
                "workflow_node_ids": ["STEP-001"],
                "targets": sorted(
                    effect["permission_target"] for effect in expected_effects
                ),
                "risk_level": "R3",
                "approval_policy": "per-invocation-approval",
                "rollback": "Remove only an approved incomplete output.",
            }
        ],
        "validation_matrix": [
            {
                "id": "TEST-001",
                "layer": "plan-gate",
                "case_type": "normal",
                "procedure": "Run review, render, and verify-staged.",
                "expected": "All deterministic gates pass.",
                "evidence": "Structured gate output.",
                "cleanup": "The caller deletes its temporary directory.",
            }
        ],
        "risks": [
            {
                "id": "RISK-001",
                "description": "The CLI process can write a file.",
                "likelihood": "low",
                "impact": "high",
                "mitigation": "Exact targets and fail-if-exists.",
                "residual_risk": "A process failure can leave a partial output.",
            }
        ],
        "unresolved_decisions": [],
        "decision_log": [
            {
                "revision": 1,
                "timestamp": "2030-01-01T00:00:00Z",
                "source": "fixture-builder",
                "decision": "Use one exact isolated CLI invocation.",
                "effect": "Bind every executable, argv, cwd and data effect.",
            }
        ],
        "approval": {},
    }
    approved_plan = _finalize_fixture_plan(plan, approve=True)
    plan_path = temp_root / "plan.json"
    plan_path.write_text(
        json.dumps(approved_plan, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return {
        "plan": approved_plan,
        "plan_path": plan_path,
        "plan_schema": plan_schema,
        "plan_schema_path": plan_schema_path,
        "catalog": catalog,
        "catalog_path": catalog_path,
        "catalog_schema": catalog_schema,
        "catalog_schema_path": catalog_schema_path,
        "staged_root": staged_root,
        "manifest_path": manifest_path,
        "skills_root": skills_root,
        "creator_root": creator_root,
        "runtime_root": runtime_root,
        "executable_path": executable_path,
        "allowed_input_root": input_root,
        "allowed_output_root": output_root,
        "allowed_executable_root": executable_root,
        "allowed_cwd_root": cwd_root,
        "input_path": input_path,
        "output_path": output_path,
    }


def _build_processing_invocation_fixture(
    temp_root_value: Path | str,
) -> dict[str, Any]:
    """Build one safe direct Processing invocation fixture for gate tests."""
    temp_root = Path(temp_root_value).resolve(strict=False)
    system_temp = Path(tempfile.gettempdir()).resolve(strict=False)
    if (
        temp_root == system_temp
        or not is_within(temp_root, system_temp)
        or not temp_root.is_dir()
        or any(temp_root.iterdir())
    ):
        raise GateInputError(
            "fixture root must be an existing empty child of the system temp root"
        )
    input_root = temp_root / "input"
    output_root = temp_root / "output"
    input_root.mkdir()
    output_root.mkdir()
    input_path = input_root / "input.gpkg"
    output_path = output_root / "output.gpkg"
    input_path.write_bytes(b"fixture")

    arguments = {
        "algorithm_id": "native:buffer",
        "parameters": {
            "INPUT": str(input_path),
            "OUTPUT": str(output_path),
        },
    }
    parameter_constraints = {
        "type": "object",
        "additionalProperties": False,
        "required": ["algorithm_id", "parameters"],
        "properties": {
            "algorithm_id": {"type": "string", "const": "native:buffer"},
            "parameters": {
                "type": "object",
                "additionalProperties": False,
                "required": ["INPUT", "OUTPUT"],
                "properties": {
                    "INPUT": {"type": "string"},
                    "OUTPUT": {"type": "string"},
                },
            },
        },
    }
    nested_target_kinds = {
        "/arguments/parameters/INPUT": "input-path",
        "/arguments/parameters/OUTPUT": "output-path",
    }
    invocation_policy = {
        "required_arguments": ["algorithm_id", "parameters"],
        "required_any_argument_groups": [],
        "required_any_target_argument_groups": [],
        "required_target_arguments": ["parameters"],
        "require_all_schema_properties_explicit": True,
        "command_shape": "schema-defined",
    }
    side_effects = {
        "reads_files": True,
        "writes_files": True,
        "modifies_qgis_state": False,
        "uses_network": False,
        "runs_shell": False,
        "destructive_possible": False,
    }
    tool = {
        "id": "TOOL-001",
        "source_type": "qgis-processing",
        "algorithm_id": "native:buffer",
        "risk_level": "R2",
        "approval_policy": "plan-approval",
        "allowed_input_roots": [str(input_root)],
        "allowed_output_roots": [str(output_root)],
        "allowed_executable_roots": [],
        "allowed_cwd_roots": [],
        "qgis_state_changes": [],
        "nested_target_kinds": copy.deepcopy(nested_target_kinds),
        "nested_target_kinds_hash": sha256_value(nested_target_kinds),
        "parameter_constraints": copy.deepcopy(parameter_constraints),
        "parameter_constraints_hash": sha256_value(parameter_constraints),
        "catalog_side_effects": side_effects,
        "network_policy": "no-network",
        "overwrite_policy": "fail-if-exists",
        "invocation_policy": invocation_policy,
    }
    canonical_input = canonical_data_endpoint(str(input_path))
    canonical_output = canonical_data_endpoint(str(output_path))
    parameters_hash = sha256_value(arguments["parameters"])
    effects = [
        _effect_fixture("arguments", sha256_value(arguments)),
        _effect_fixture("arguments", parameters_hash),
        _effect_fixture("input-root", canonical_data_endpoint(str(input_root))),
        _effect_fixture("output-root", canonical_data_endpoint(str(output_root))),
        _effect_fixture("input-path", canonical_input),
        _effect_fixture("output-path", canonical_output),
        _effect_fixture("network-endpoint", "deny-all"),
    ]
    node = {
        "id": "STEP-001",
        "arguments": arguments,
        "argument_bindings": [
            {
                "name": "algorithm_id",
                "source_kind": "literal",
                "source_ref": None,
                "constraint_schema": {
                    "type": "string",
                    "const": "native:buffer",
                },
                "target_kind": "none",
                "permission_target": None,
                "sensitive": False,
            },
            {
                "name": "parameters",
                "source_kind": "literal",
                "source_ref": None,
                "constraint_schema": copy.deepcopy(
                    parameter_constraints["properties"]["parameters"]
                ),
                "target_kind": "arguments",
                "permission_target": f"arguments:{parameters_hash}",
                "sensitive": False,
            },
        ],
        "nested_target_bindings": [
            {
                "json_pointer": "/arguments/parameters/INPUT",
                "target_kind": "input-path",
                "permission_target": f"input-path:{canonical_input}",
                "sensitive": False,
                "rationale": "Exact Processing input path.",
            },
            {
                "json_pointer": "/arguments/parameters/OUTPUT",
                "target_kind": "output-path",
                "permission_target": f"output-path:{canonical_output}",
                "sensitive": False,
                "rationale": "Exact Processing output path.",
            },
        ],
        "omitted_optional_arguments": [],
        "stdin_payload": None,
        "executable": None,
        "materialized_argv": None,
        "cwd": None,
        "effect_targets": effects,
        "inputs": [],
        "risk_level": "R2",
    }
    capability = {
        "algorithm_id": "native:buffer",
        "input_schema": parameter_constraints,
        "reviewed_nested_target_kinds": copy.deepcopy(nested_target_kinds),
        "reviewed_nested_target_kinds_hash": sha256_value(nested_target_kinds),
        "reviewed_parameter_constraints": copy.deepcopy(parameter_constraints),
        "reviewed_parameter_constraints_hash": sha256_value(parameter_constraints),
    }
    return {
        "tool": tool,
        "capability": capability,
        "node": node,
        "input_root": input_root,
        "output_root": output_root,
        "input_path": input_path,
        "output_path": output_path,
    }


def run_self_test() -> list[str]:
    errors: list[str] = []
    if canonical_bytes({"b": 2, "a": "é"}) != canonical_bytes({"a": "é", "b": 2}):
        errors.append("canonical JSON ordering is unstable")
    unsafe = [
        "../x",
        "C:" + "/x",
        "/x",
        "a//b",
        "a/./b",
        "a\\b",
        "NUL.txt",
    ]
    safe = ["SKILL.md", "references/tool-contract.json", "scripts/check.py"]
    if any(is_relative_package_path(value) for value in unsafe):
        errors.append("unsafe package path was accepted")
    if not all(is_relative_package_path(value) for value in safe):
        errors.append("safe package path was rejected")
    fixture_schema = {
        "type": "object",
        "additionalProperties": False,
        "required": ["value"],
        "properties": {"value": {"type": "integer", "minimum": 1}},
    }
    if validate_schema_subset({"value": 1}, fixture_schema):
        errors.append("schema subset rejected a valid fixture")
    if not validate_schema_subset({"value": 0, "extra": True}, fixture_schema):
        errors.append("schema subset accepted an invalid fixture")

    capability_fixture = {
        "discovered": True,
        "eligible": True,
        "eligibility_evidence": "Observed in the current session.",
        "ineligibility_reason": None,
        "available": True,
        "deprecated": False,
        "risk_review_status": "reviewed",
    }
    capability_errors: list[str] = []
    validate_capability_state(capability_fixture, "CAP-001", capability_errors)
    validate_selected_capability("TOOL-001", capability_fixture, capability_errors)
    if capability_errors:
        errors.append(f"valid capability-state fixture failed: {capability_errors}")
    for field, value in (("eligible", False), ("available", False)):
        unavailable = dict(capability_fixture)
        unavailable[field] = value
        if field == "eligible":
            unavailable["ineligibility_reason"] = "Not compatible with the task."
        selection_errors: list[str] = []
        validate_selected_capability("TOOL-001", unavailable, selection_errors)
        if not selection_errors:
            errors.append(f"selected capability accepted {field}=false")
    invalid_eligibility = dict(capability_fixture)
    invalid_eligibility["eligible"] = False
    eligibility_errors: list[str] = []
    validate_capability_state(invalid_eligibility, "CAP-001", eligibility_errors)
    if not eligibility_errors:
        errors.append("ineligible capability without reason was accepted")

    algorithm_fixture = {
        "algorithm_id": "native:buffer",
        "allowed_algorithm_ids": ["native:buffer"],
        "provider": "native",
    }
    algorithm_errors: list[str] = []
    validate_algorithm_binding("TOOL-001", algorithm_fixture, algorithm_errors)
    if algorithm_errors:
        errors.append(f"valid algorithm fixture failed: {algorithm_errors}")
    for reviewed in (["native:*"], ["native:centroids"]):
        invalid_algorithm = dict(algorithm_fixture)
        invalid_algorithm["allowed_algorithm_ids"] = reviewed
        invalid_algorithm_errors: list[str] = []
        validate_algorithm_binding(
            "TOOL-001", invalid_algorithm, invalid_algorithm_errors
        )
        if not invalid_algorithm_errors:
            errors.append(f"invalid algorithm binding was accepted: {reviewed}")

    old_staging_errors: list[str] = []
    validate_creation_policy(
        {
            "collision_policy": "fail-no-overwrite",
            "staging_policy": "outside-skills-root-then-atomic-move",
        },
        old_staging_errors,
    )
    if not old_staging_errors:
        errors.append("legacy atomic-move staging policy was accepted")

    computed_fixture = {
        "plan_hash": "sha256:" + "1" * 64,
        "capability_hash": "sha256:" + "2" * 64,
        "review_markdown_hash": "sha256:" + "3" * 64,
    }
    approval_fixture = {
        "required": True,
        "state": "approved",
        "approved_revision": 2,
        "approved_plan_hash": computed_fixture["plan_hash"],
        "approved_capability_hash": computed_fixture["capability_hash"],
        "approved_review_markdown_hash": computed_fixture["review_markdown_hash"],
        "approved_target_relative_path": "sample-skill",
        "intent": "create-current-plan",
        "user_statement": (
            "批准计划 qgis-skill-plan-sample revision 2，按该计划创建。"
        ),
        "approved_at": "2000-01-01T00:02:00Z",
    }
    approved_plan_fixture = {
        "plan_id": "qgis-skill-plan-sample",
        "status": "approved",
        "revision": 2,
        "created_at": "2000-01-01T00:00:00Z",
        "updated_at": "2000-01-01T00:01:00Z",
        "target_skill_package": {"target_relative_path": "sample-skill"},
        "approval": approval_fixture,
    }
    binding_errors: list[str] = []
    validate_approval_bindings(approved_plan_fixture, computed_fixture, binding_errors)
    if binding_errors:
        errors.append(f"valid approved binding fixture failed: {binding_errors}")
    mismatch_values: dict[str, Any] = {
        "state": "pending",
        "approved_revision": 1,
        "approved_plan_hash": ZERO_HASH,
        "approved_capability_hash": ZERO_HASH,
        "approved_review_markdown_hash": ZERO_HASH,
        "approved_target_relative_path": "other-skill",
        "intent": "other-intent",
    }
    for field, mismatch in mismatch_values.items():
        mismatched_plan = copy.deepcopy(approved_plan_fixture)
        mismatched_plan["approval"][field] = mismatch
        mismatch_errors: list[str] = []
        validate_approval_bindings(mismatched_plan, computed_fixture, mismatch_errors)
        if not any(f"approval.{field}" in error for error in mismatch_errors):
            errors.append(f"approved binding mismatch was accepted: {field}")
    for field in ("user_statement", "approved_at"):
        empty_plan = copy.deepcopy(approved_plan_fixture)
        empty_plan["approval"][field] = " "
        empty_errors: list[str] = []
        validate_approval_bindings(empty_plan, computed_fixture, empty_errors)
        if not any(f"approval.{field}" in error for error in empty_errors):
            errors.append(f"empty approved audit field was accepted: {field}")
    invalid_statements = [
        "继续",
        "不错",
        "可以试试",
        "批准计划 qgis-skill-plan-sample revision 1，按该计划创建。",
        ("批准计划 qgis-skill-plan-sample revision 2，按该计划创建。另外修改输出。"),
    ]
    for statement in invalid_statements:
        statement_plan = copy.deepcopy(approved_plan_fixture)
        statement_plan["approval"]["user_statement"] = statement
        statement_errors: list[str] = []
        validate_approval_bindings(statement_plan, computed_fixture, statement_errors)
        if not any("approval.user_statement" in item for item in statement_errors):
            errors.append(f"ambiguous approval was accepted: {statement!r}")
    reversed_time_plan = copy.deepcopy(approved_plan_fixture)
    reversed_time_plan["updated_at"] = "2000-01-01T00:03:00Z"
    time_errors: list[str] = []
    validate_approval_bindings(reversed_time_plan, computed_fixture, time_errors)
    if not any("updated_at" in item for item in time_errors):
        errors.append("approval timestamp reversal was accepted")

    draft_verify_errors: list[str] = []
    validate_verify_status({"status": "draft"}, draft_verify_errors)
    if not draft_verify_errors:
        errors.append("verify-staged status gate accepted a draft plan")
    approved_verify_errors: list[str] = []
    validate_verify_status({"status": "approved"}, approved_verify_errors)
    if approved_verify_errors:
        errors.append("verify-staged status gate rejected an approved plan")

    plan_schema_path = (
        Path(__file__).resolve().parent.parent
        / "references"
        / "qgis-skill-plan.schema.json"
    )
    try:
        plan_schema = load_strict_json(plan_schema_path)
        if not isinstance(plan_schema, dict):
            raise GateInputError("bundled plan schema must be an object")
        approval_schema = plan_schema["properties"]["approval"]
        approved_constraint = plan_schema["allOf"][0]["then"]["properties"]["approval"]
        schema_errors = validate_schema_subset(
            approval_fixture,
            approval_schema,
            root_schema=plan_schema,
            path="$.approval",
        )
        schema_errors.extend(
            validate_schema_subset(
                approval_fixture,
                approved_constraint,
                root_schema=plan_schema,
                path="$.approval",
            )
        )
        if schema_errors:
            errors.append(f"approved plan schema fixture failed: {schema_errors}")
    except (KeyError, IndexError, TypeError, OSError, GateInputError) as error:
        errors.append(f"approved plan schema fixture could not run: {error}")

    with tempfile.TemporaryDirectory(prefix="qgis-skill-gate-") as temp_value:
        temp_root = Path(temp_value)
        service_root = temp_root / "service"
        skills_root = service_root / "Skills"
        creator_root = skills_root / "qgis-skills-creator"
        staged_root = skills_root / "sample-skill"
        references_root = staged_root / "references"
        creator_root.mkdir(parents=True)
        references_root.mkdir(parents=True)
        manifest = {
            "service_id": "qcopilots.mcp_server_skills",
            "skillsRoot": "Skills",
        }
        manifest_path = service_root / "qcopilots_service.json"
        manifest_path.write_text(
            json.dumps(manifest, ensure_ascii=False) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        manifest_hash = sha256_value(manifest)
        path_plan = {
            "environment_contract": {
                "skills_root": {
                    "display_path": normalized_resolved_path(skills_root),
                    "manifest_path": normalized_resolved_path(manifest_path),
                    "manifest_hash": manifest_hash,
                    "identity_hash": compute_skills_root_identity(
                        manifest_hash, skills_root
                    ),
                }
            },
            "skill_identity": {
                "name": "sample-skill",
                "description": "Sample",
            },
            "target_skill_package": {"target_relative_path": "sample-skill"},
        }
        root_errors: list[str] = []
        resolved_target = validate_staged_path_binding(
            path_plan,
            staged_root,
            manifest_path,
            skills_root,
            root_errors,
        )
        if root_errors or resolved_target != staged_root.resolve():
            errors.append(f"valid Skills root binding failed: {root_errors}")
        for label, blocked_path in (
            ("leaf", skills_root),
            ("ancestor", service_root),
        ):
            manifest_root_reparse_errors: list[str] = []
            validate_skills_root_binding(
                path_plan,
                manifest_path,
                skills_root,
                manifest_root_reparse_errors,
                reparse_checker=lambda path, blocked=blocked_path: path == blocked,
            )
            if not any(
                "manifest-derived Skills root path contains a reparse point" in item
                and str(blocked_path) in item
                for item in manifest_root_reparse_errors
            ):
                errors.append(
                    f"manifest-derived Skills root lexical reparse {label} was accepted"
                )
        other_root = temp_root / "other-skills"
        other_root.mkdir()
        wrong_root_errors: list[str] = []
        validate_staged_path_binding(
            path_plan,
            staged_root,
            manifest_path,
            other_root,
            wrong_root_errors,
        )
        if not wrong_root_errors:
            errors.append("manifest/root mismatch was accepted")
        nested_target = skills_root / "container" / "nested-skill"
        nested_target.mkdir(parents=True)
        nested_plan = copy.deepcopy(path_plan)
        nested_plan["skill_identity"]["name"] = "nested-skill"
        nested_plan["target_skill_package"]["target_relative_path"] = "nested-skill"
        nested_errors: list[str] = []
        validate_staged_path_binding(
            nested_plan,
            nested_target,
            manifest_path,
            skills_root,
            nested_errors,
        )
        if not any("direct child" in item for item in nested_errors):
            errors.append("nested staged target was accepted")

        workflow_text = "# Workflow\n"
        skill_text = (
            '---\nname: sample-skill\ndescription: "Sample"\n---\n\n'
            "# Sample\n\n[Workflow](references/workflow.md)\n"
        )
        workflow_path = references_root / "workflow.md"
        skill_path = staged_root / "SKILL.md"
        workflow_path.write_text(workflow_text, encoding="utf-8", newline="\n")
        skill_path.write_text(skill_text, encoding="utf-8", newline="\n")
        package_plan = copy.deepcopy(path_plan)
        package_plan["status"] = "approved"
        package_plan["target_skill_package"].update(
            {
                "files": [
                    {
                        "relative_path": "SKILL.md",
                        "content_kind": "text-utf8",
                        "expected_sha256": sha256_bytes(skill_text.encode("utf-8")),
                        "expected_size_bytes": len(skill_text.encode("utf-8")),
                    },
                    {
                        "relative_path": "references/workflow.md",
                        "content_kind": "text-utf8",
                        "expected_sha256": sha256_bytes(workflow_text.encode("utf-8")),
                        "expected_size_bytes": len(workflow_text.encode("utf-8")),
                    },
                ]
            }
        )
        staged_errors: list[str] = []
        validate_staged_package(
            package_plan,
            staged_root,
            manifest_path,
            skills_root,
            staged_errors,
        )
        if staged_errors:
            errors.append(f"valid staged package fixture failed: {staged_errors}")
        portable_texts = {
            "json-regex-escape": '{"regex":"' + chr(92) * 4 + 'd+"}\n',
            "https-authority": "Use https://server.example/share/resource\n",
            "unknown-uri-authority": (
                "Use custom+scheme://server.example/share/resource\n"
            ),
            "localhost-file-uri": "Use file://localhost/share/resource\n",
        }
        for label, portable_text in portable_texts.items():
            portable_errors: list[str] = []
            validate_staged_text(
                "references/portable.md",
                references_root / "portable.md",
                portable_text,
                staged_root,
                path_plan,
                portable_errors,
            )
            if any("obvious machine path" in item for item in portable_errors):
                errors.append(f"portable staged text was rejected as UNC: {label}")
        text_failures = {
            "TODO": "TODO: replace this",
            "escape": "[outside](../../outside.md)",
            "machine-program-files": (
                "Use "
                + "C:"
                + chr(92)
                + "Program Files"
                + chr(92)
                + "QGIS"
                + chr(92)
                + "tool.json"
            ),
            "machine-other-drive": (
                "Use " + "D:" + chr(92) + "custom" + chr(92) + "tool.json"
            ),
            "machine-unix-opt": "Use /" + "opt/qgis/tool.json",
            "machine-unc-unicode": (
                "Use "
                + chr(92) * 2
                + "服务器"
                + chr(92)
                + "共享"
                + chr(92)
                + "tool.json"
            ),
            "machine-unc-escaped": (
                "Use "
                + chr(92) * 4
                + "server"
                + chr(92) * 2
                + "share"
                + chr(92) * 2
                + "tool.json"
            ),
            "machine-unc-extended": (
                "Use "
                + chr(92) * 2
                + "?"
                + chr(92)
                + "UNC"
                + chr(92)
                + "server"
                + chr(92)
                + "share"
            ),
            "machine-unc-wsl": (
                "Use "
                + chr(92) * 2
                + "wsl$"
                + chr(92)
                + "Ubuntu"
                + chr(92)
                + "tool.json"
            ),
            "machine-unc-forward": (
                "Use " + "/" * 2 + "服务器" + "/" + "共享" + "/tool.json"
            ),
            "machine-unc-file-uri": "Use file://server.example/share/tool.json",
            "skills-root": normalized_resolved_path(skills_root),
        }
        for label, bad_text in text_failures.items():
            text_errors: list[str] = []
            validate_staged_text(
                "references/bad.md",
                references_root / "bad.md",
                bad_text,
                staged_root,
                path_plan,
                text_errors,
            )
            if not text_errors:
                errors.append(f"staged text negative fixture was accepted: {label}")
        long_skill = (
            '---\nname: sample-skill\ndescription: "Sample"\n---\n' + "line\n" * 500
        )
        long_errors: list[str] = []
        validate_staged_text(
            "SKILL.md",
            skill_path,
            long_skill,
            staged_root,
            path_plan,
            long_errors,
        )
        if not any("fewer than 500" in item for item in long_errors):
            errors.append("500-line SKILL.md fixture was accepted")

        lexical_probe = temp_root / "lexical-probe"
        lexical_ancestor = lexical_probe / "ancestor"
        lexical_ancestor.mkdir(parents=True)
        lexical_leaf = lexical_ancestor / "leaf.exe"
        lexical_leaf.write_bytes(b"fixture")
        for label, blocked_path in (
            ("leaf", lexical_leaf),
            ("ancestor", lexical_ancestor),
        ):
            reparse_errors: list[str] = []
            _validate_lexical_path_no_reparse(
                str(lexical_leaf),
                f"lexical-{label}",
                reparse_errors,
                reparse_checker=lambda path, blocked=blocked_path: path == blocked,
            )
            if not any(
                "lexical path contains a reparse point" in item
                for item in reparse_errors
            ):
                errors.append(f"lexical reparse {label} fixture was accepted")

        allowed_root = temp_root / "allowed-input"
        allowed_root.mkdir()
        input_path = allowed_root / "input.gpkg"
        canonical_input = canonical_data_endpoint(str(input_path))
        canonical_input_root = canonical_data_endpoint(str(allowed_root))
        tool_fixture = {
            "id": "TOOL-001",
            "source_type": "mcp",
            "service_id": "service",
            "tool_name": "read_data",
            "risk_level": "R2",
            "approval_policy": "plan-approval",
            "allowed_input_roots": [str(allowed_root)],
            "allowed_output_roots": [],
            "allowed_executable_roots": [],
            "allowed_cwd_roots": [],
            "qgis_state_changes": [],
            "nested_target_kinds": {},
            "parameter_constraints": {
                "type": "object",
                "additionalProperties": False,
                "required": ["path"],
                "properties": {"path": {"type": "string"}},
            },
            "catalog_side_effects": {
                "reads_files": True,
                "writes_files": False,
                "modifies_qgis_state": False,
                "uses_network": False,
                "runs_shell": False,
                "destructive_possible": False,
            },
            "network_policy": "no-network",
            "overwrite_policy": "read-only",
            "invocation_policy": {
                "required_arguments": ["path"],
                "required_any_argument_groups": [],
                "required_any_target_argument_groups": [],
                "required_target_arguments": ["path"],
                "require_all_schema_properties_explicit": True,
                "command_shape": "schema-defined",
            },
        }
        capability_contract = {
            "input_schema": copy.deepcopy(tool_fixture["parameter_constraints"])
        }
        policy_fixture = {
            "invocation_policy": {
                "required_arguments": ["path"],
                "required_any_argument_groups": [],
                "required_any_target_argument_groups": [],
                "required_target_arguments": ["path"],
                "require_all_schema_properties_explicit": True,
                "command_shape": "schema-defined",
            }
        }
        node_fixture = {
            "id": "STEP-001",
            "tool_id": "TOOL-001",
            "risk_level": "R2",
            "arguments": {"path": str(input_path)},
            "argument_bindings": [
                {
                    "name": "path",
                    "source_kind": "literal",
                    "source_ref": None,
                    "constraint_schema": {"type": "string"},
                    "target_kind": "input-path",
                    "permission_target": f"input-path:{canonical_input}",
                    "sensitive": False,
                }
            ],
            "nested_target_bindings": [],
            "omitted_optional_arguments": [],
            "stdin_payload": None,
            "executable": None,
            "materialized_argv": None,
            "cwd": None,
            "inputs": [],
            "effect_targets": [
                {
                    "kind": "arguments",
                    "value": sha256_value({"path": str(input_path)}),
                    "permission_target": (
                        "arguments:" + sha256_value({"path": str(input_path)})
                    ),
                },
                {
                    "kind": "input-path",
                    "value": canonical_input,
                    "permission_target": f"input-path:{canonical_input}",
                },
                {
                    "kind": "input-root",
                    "value": canonical_input_root,
                    "permission_target": f"input-root:{canonical_input_root}",
                },
                {
                    "kind": "network-endpoint",
                    "value": "deny-all",
                    "permission_target": "network-endpoint:deny-all",
                },
            ],
        }
        invocation_errors: list[str] = []
        validate_node_invocation(
            "STEP-001",
            node_fixture,
            tool_fixture,
            capability_contract,
            policy_fixture,
            invocation_errors,
        )
        if invocation_errors:
            errors.append(f"valid invocation fixture failed: {invocation_errors}")
        constraint_node = copy.deepcopy(node_fixture)
        constraint_node["argument_bindings"][0]["constraint_schema"] = {
            "type": "integer"
        }
        constraint_errors: list[str] = []
        validate_node_invocation(
            "STEP-001",
            constraint_node,
            tool_fixture,
            capability_contract,
            policy_fixture,
            constraint_errors,
        )
        if not any("constraint" in item for item in constraint_errors):
            errors.append("binding constraint mismatch was accepted")
        outside_path = temp_root / "outside.gpkg"
        outside_endpoint = canonical_data_endpoint(str(outside_path))
        outside_node = copy.deepcopy(node_fixture)
        outside_node["arguments"]["path"] = str(outside_path)
        outside_node["argument_bindings"][0]["permission_target"] = (
            f"input-path:{outside_endpoint}"
        )
        outside_node["effect_targets"][0].update(
            {
                "value": outside_endpoint,
                "permission_target": f"input-path:{outside_endpoint}",
            }
        )
        outside_errors: list[str] = []
        validate_node_invocation(
            "STEP-001",
            outside_node,
            tool_fixture,
            capability_contract,
            policy_fixture,
            outside_errors,
        )
        if not any("outside selected" in item for item in outside_errors):
            errors.append("path outside allowed roots was accepted")
        effect_node = copy.deepcopy(node_fixture)
        effect_node["effect_targets"][0]["permission_target"] = "input-path:wrong"
        effect_errors: list[str] = []
        validate_node_invocation(
            "STEP-001",
            effect_node,
            tool_fixture,
            capability_contract,
            policy_fixture,
            effect_errors,
        )
        if not any("permission_target mismatch" in item for item in effect_errors):
            errors.append("effect target mismatch was accepted")

        output_root = temp_root / "allowed-output"
        output_root.mkdir()
        output_path = output_root / "output.gpkg"
        canonical_output = canonical_data_endpoint(str(output_path))
        canonical_output_root = canonical_data_endpoint(str(output_root))
        processing_arguments = {
            "algorithm_id": "native:buffer",
            "parameters": {
                "INPUT": str(input_path),
                "OUTPUT": str(output_path),
            },
        }
        processing_schema = {
            "type": "object",
            "additionalProperties": False,
            "required": ["algorithm_id", "parameters"],
            "properties": {
                "algorithm_id": {
                    "type": "string",
                    "const": "native:buffer",
                },
                "parameters": {
                    "type": "object",
                    "additionalProperties": False,
                    "required": ["INPUT", "OUTPUT"],
                    "properties": {
                        "INPUT": {"type": "string"},
                        "OUTPUT": {"type": "string"},
                    },
                },
            },
        }
        processing_tool = {
            "source_type": "qgis-processing",
            "algorithm_id": "native:buffer",
            "risk_level": "R2",
            "approval_policy": "plan-approval",
            "allowed_input_roots": [str(allowed_root)],
            "allowed_output_roots": [str(output_root)],
            "allowed_executable_roots": [],
            "allowed_cwd_roots": [],
            "qgis_state_changes": [],
            "nested_target_kinds": {
                "/arguments/parameters/INPUT": "input-path",
                "/arguments/parameters/OUTPUT": "output-path",
            },
            "parameter_constraints": processing_schema,
            "catalog_side_effects": {
                "reads_files": True,
                "writes_files": True,
                "modifies_qgis_state": False,
                "uses_network": False,
                "runs_shell": False,
                "destructive_possible": False,
            },
            "network_policy": "no-network",
            "overwrite_policy": "fail-if-exists",
            "invocation_policy": {
                "required_arguments": ["algorithm_id", "parameters"],
                "required_any_argument_groups": [],
                "required_any_target_argument_groups": [],
                "required_target_arguments": ["parameters"],
                "require_all_schema_properties_explicit": True,
                "command_shape": "schema-defined",
            },
        }
        parameters_hash = sha256_value(processing_arguments["parameters"])
        processing_effects = [
            _effect_fixture("arguments", sha256_value(processing_arguments)),
            _effect_fixture("arguments", parameters_hash),
            _effect_fixture("input-root", canonical_input_root),
            _effect_fixture("output-root", canonical_output_root),
            _effect_fixture("input-path", canonical_input),
            _effect_fixture("output-path", canonical_output),
            _effect_fixture("network-endpoint", "deny-all"),
        ]
        processing_node = {
            "arguments": processing_arguments,
            "argument_bindings": [
                {
                    "name": "algorithm_id",
                    "source_kind": "literal",
                    "source_ref": None,
                    "constraint_schema": {
                        "type": "string",
                        "const": "native:buffer",
                    },
                    "target_kind": "none",
                    "permission_target": None,
                    "sensitive": False,
                },
                {
                    "name": "parameters",
                    "source_kind": "literal",
                    "source_ref": None,
                    "constraint_schema": processing_schema["properties"]["parameters"],
                    "target_kind": "arguments",
                    "permission_target": f"arguments:{parameters_hash}",
                    "sensitive": False,
                },
            ],
            "nested_target_bindings": [
                {
                    "json_pointer": "/arguments/parameters/INPUT",
                    "target_kind": "input-path",
                    "permission_target": f"input-path:{canonical_input}",
                    "sensitive": False,
                    "rationale": "Exact Processing input path.",
                },
                {
                    "json_pointer": "/arguments/parameters/OUTPUT",
                    "target_kind": "output-path",
                    "permission_target": f"output-path:{canonical_output}",
                    "sensitive": False,
                    "rationale": "Exact Processing output path.",
                },
            ],
            "omitted_optional_arguments": [],
            "stdin_payload": None,
            "executable": None,
            "materialized_argv": None,
            "cwd": None,
            "effect_targets": processing_effects,
            "inputs": [],
            "risk_level": "R2",
        }
        processing_errors: list[str] = []
        validate_node_invocation(
            "STEP-002",
            processing_node,
            processing_tool,
            {"input_schema": processing_schema},
            None,
            processing_errors,
        )
        if processing_errors:
            errors.append(
                f"valid nested Processing fixture failed: {processing_errors}"
            )
        algorithm_mismatch = copy.deepcopy(processing_node)
        algorithm_mismatch["arguments"]["algorithm_id"] = "native:centroids"
        algorithm_errors: list[str] = []
        validate_node_invocation(
            "STEP-002",
            algorithm_mismatch,
            processing_tool,
            {"input_schema": processing_schema},
            None,
            algorithm_errors,
        )
        if not any(
            "algorithm_id differs from selected tool" in item
            for item in algorithm_errors
        ):
            errors.append("Processing algorithm mismatch was accepted")
        escaped_processing = copy.deepcopy(processing_node)
        escaped_processing["arguments"]["parameters"]["OUTPUT"] = str(outside_path)
        escaped_errors: list[str] = []
        validate_node_invocation(
            "STEP-002",
            escaped_processing,
            processing_tool,
            {"input_schema": processing_schema},
            None,
            escaped_errors,
        )
        if not any("outside selected" in item for item in escaped_errors):
            errors.append("nested Processing path escape was accepted")
        hidden_processing = copy.deepcopy(processing_node)
        hidden_binding = hidden_processing["nested_target_bindings"][1]
        hidden_binding["target_kind"] = "none"
        hidden_binding["permission_target"] = None
        hidden_processing["effect_targets"] = [
            effect
            for effect in hidden_processing["effect_targets"]
            if effect["kind"] != "output-path"
        ]
        hidden_errors: list[str] = []
        validate_node_invocation(
            "STEP-002",
            hidden_processing,
            processing_tool,
            {"input_schema": processing_schema},
            None,
            hidden_errors,
        )
        if not any("cannot be none" in item for item in hidden_errors):
            errors.append("nested Processing path was hidden as none")
        wrong_output_kind = copy.deepcopy(processing_node)
        output_binding = wrong_output_kind["nested_target_bindings"][1]
        output_binding["target_kind"] = "input-path"
        output_binding["permission_target"] = f"input-path:{canonical_output}"
        wrong_output_errors: list[str] = []
        validate_node_invocation(
            "STEP-002",
            wrong_output_kind,
            processing_tool,
            {"input_schema": processing_schema},
            None,
            wrong_output_errors,
        )
        if not any(
            "target_kind differs from authoritative reviewed contract" in item
            for item in wrong_output_errors
        ):
            errors.append("Processing OUTPUT was misreported as input-path")
        for uri in (
            "wss://example.invalid/socket",
            "mssql://example.invalid/database",
            "custom+scheme://example.invalid/resource",
        ):
            uri_none = copy.deepcopy(processing_node)
            uri_none["arguments"]["parameters"]["INPUT"] = uri
            input_binding = uri_none["nested_target_bindings"][0]
            input_binding["target_kind"] = "none"
            input_binding["permission_target"] = None
            uri_errors: list[str] = []
            validate_node_invocation(
                "STEP-002",
                uri_none,
                processing_tool,
                {"input_schema": processing_schema},
                None,
                uri_errors,
            )
            if not any(
                "target-like nested value cannot be none" in item for item in uri_errors
            ):
                errors.append(f"absolute URI was hidden as none: {uri}")

        uri_family_cases = (
            ("file://localhost/tmp/input.gpkg", "local-path"),
            ("wss://example.invalid/socket", "network"),
            ("mssql://example.invalid/database", "database"),
            ("sqlite:///tmp/catalog.sqlite", "database"),
            ("custom+scheme://example.invalid/resource", "network"),
        )
        for uri, expected_family in uri_family_cases:
            actual_family = _nested_value_family(uri)
            if actual_family != expected_family:
                errors.append(
                    "RFC 3986 nested URI family mismatch: "
                    f"{uri} -> {actual_family!r}, expected {expected_family!r}"
                )

        def coherent_processing_uri_errors(uri: str, target_kind: str) -> list[str]:
            coherent_tool = copy.deepcopy(processing_tool)
            coherent_tool["nested_target_kinds"]["/arguments/parameters/INPUT"] = (
                target_kind
            )
            coherent_node = copy.deepcopy(processing_node)
            coherent_node["arguments"]["parameters"]["INPUT"] = uri
            coherent_parameters = coherent_node["arguments"]["parameters"]
            coherent_parameters_hash = sha256_value(coherent_parameters)
            coherent_node["argument_bindings"][1]["permission_target"] = (
                f"arguments:{coherent_parameters_hash}"
            )
            coherent_endpoint = canonical_data_endpoint(uri)
            coherent_input = coherent_node["nested_target_bindings"][0]
            coherent_input["target_kind"] = target_kind
            coherent_input["permission_target"] = f"{target_kind}:{coherent_endpoint}"
            coherent_node["effect_targets"] = [
                _effect_fixture("arguments", sha256_value(coherent_node["arguments"])),
                _effect_fixture("arguments", coherent_parameters_hash),
                _effect_fixture("input-root", canonical_input_root),
                _effect_fixture("output-root", canonical_output_root),
                _effect_fixture(target_kind, coherent_endpoint),
                _effect_fixture("output-path", canonical_output),
                _effect_fixture("network-endpoint", "deny-all"),
            ]
            coherent_errors: list[str] = []
            validate_node_invocation(
                "STEP-002",
                coherent_node,
                coherent_tool,
                {"input_schema": processing_schema},
                None,
                coherent_errors,
            )
            return coherent_errors

        for uri, wrong_kind, expected_error in (
            (
                "wss://example.invalid/socket",
                "database",
                "network URI needs network-endpoint",
            ),
            (
                "mssql://example.invalid/database",
                "network-endpoint",
                "database URI needs database",
            ),
            (
                "custom+scheme://example.invalid/resource",
                "database",
                "network URI needs network-endpoint",
            ),
        ):
            wrong_kind_errors = coherent_processing_uri_errors(uri, wrong_kind)
            if not any(expected_error in item for item in wrong_kind_errors):
                errors.append(
                    f"coherent wrong nested URI kind was accepted: {uri} -> {wrong_kind}"
                )
        no_network_errors = coherent_processing_uri_errors(
            "wss://example.invalid/socket", "network-endpoint"
        )
        if not any(
            "network target conflicts with no-network policy" in item
            for item in no_network_errors
        ):
            errors.append("nested network URI bypassed no-network policy")
        extra_effect_processing = copy.deepcopy(processing_node)
        extra_effect_processing["effect_targets"].append(
            _effect_fixture("qgis-state", "forged-extra-state")
        )
        extra_effect_errors: list[str] = []
        validate_node_invocation(
            "STEP-002",
            extra_effect_processing,
            processing_tool,
            {"input_schema": processing_schema},
            None,
            extra_effect_errors,
        )
        if not any("un-derived extras" in item for item in extra_effect_errors):
            errors.append("forged extra effect target was accepted")
        permission_plan = {
            "selected_tools": [tool_fixture],
            "workflow_nodes": [node_fixture],
            "permissions": [
                {
                    "id": "PERM-001",
                    "tool_ids": ["TOOL-001"],
                    "workflow_node_ids": ["STEP-001"],
                    "targets": [f"input-root:{allowed_root}"],
                    "risk_level": "R2",
                    "approval_policy": "plan-approval",
                }
            ],
        }
        permission_errors: list[str] = []
        validate_permission_audit(permission_plan, permission_errors)
        if not any("permission targets miss" in item for item in permission_errors):
            errors.append("insufficient permission coverage was accepted")

    with tempfile.TemporaryDirectory(prefix="qgis-processing-gate-") as temp_value:
        processing_bundle = _build_processing_invocation_fixture(Path(temp_value))
        processing_builder_errors: list[str] = []
        validate_node_invocation(
            "STEP-001",
            processing_bundle["node"],
            processing_bundle["tool"],
            processing_bundle["capability"],
            None,
            processing_builder_errors,
        )
        if processing_builder_errors:
            errors.append(
                "private Processing fixture builder failed: "
                f"{processing_builder_errors}"
            )

    with tempfile.TemporaryDirectory(prefix="qgis-cli-gate-") as temp_value:
        bundle = _build_cli_fixture(Path(temp_value))

        def fixture_errors(candidate: dict[str, Any]) -> list[str]:
            result, _computed = validate_common(
                candidate,
                bundle["catalog"],
                bundle["plan_schema"],
                bundle["catalog_schema"],
            )
            return result

        valid_errors = fixture_errors(bundle["plan"])
        validate_staged_package(
            bundle["plan"],
            bundle["staged_root"],
            bundle["manifest_path"],
            bundle["skills_root"],
            valid_errors,
        )
        if valid_errors:
            errors.append(f"complete CLI fixture failed: {valid_errors}")
        if not render_review_markdown(bundle["plan"]).startswith(
            "# QGIS 技能创建计划审查"
        ):
            errors.append("complete CLI fixture render failed")

        nested_escape = copy.deepcopy(bundle["plan"])
        nested_escape["workflow_nodes"][0]["stdin_payload"]["inputs"]["OUTPUT"] = str(
            Path(temp_value) / "outside.gpkg"
        )
        nested_escape = _finalize_fixture_plan(nested_escape, approve=True)
        if not any(
            "outside selected" in item for item in fixture_errors(nested_escape)
        ):
            errors.append("nested stdin path escape was accepted")

        extra_effect = copy.deepcopy(bundle["plan"])
        extra_effect["workflow_nodes"][0]["effect_targets"].append(
            _effect_fixture("qgis-state", "forged-extra-state")
        )
        extra_effect = _finalize_fixture_plan(extra_effect, approve=True)
        if not any(
            "un-derived extras" in item for item in fixture_errors(extra_effect)
        ):
            errors.append("complete plan accepted a forged extra effect")

        argv_drift = copy.deepcopy(bundle["plan"])
        argv_drift["workflow_nodes"][0]["materialized_argv"].append("--drift")
        argv_drift = _finalize_fixture_plan(argv_drift, approve=True)
        if not any(
            "materialized argv differs" in item for item in fixture_errors(argv_drift)
        ):
            errors.append("CLI materialized argv drift was accepted")

        executable_escape = copy.deepcopy(bundle["plan"])
        outside_executable = Path(temp_value) / "outside-cli.bat"
        outside_executable.write_bytes(b"@echo off\r\n")
        executable_escape["workflow_nodes"][0]["executable"] = str(outside_executable)
        executable_escape = _finalize_fixture_plan(executable_escape, approve=True)
        executable_errors = fixture_errors(executable_escape)
        if not any("outside selected" in item for item in executable_errors):
            errors.append("CLI executable outside allowed roots was accepted")

        cwd_escape = copy.deepcopy(bundle["plan"])
        cwd_escape["workflow_nodes"][0]["cwd"] = str(Path(temp_value))
        cwd_escape = _finalize_fixture_plan(cwd_escape, approve=True)
        if not any("outside selected" in item for item in fixture_errors(cwd_escape)):
            errors.append("CLI cwd outside allowed roots was accepted")

        executable_path = bundle["executable_path"]
        executable_bytes = executable_path.read_bytes()
        executable_path.write_bytes(executable_bytes + b"rem drift\r\n")
        live_drift_errors = fixture_errors(bundle["plan"])
        executable_path.write_bytes(executable_bytes)
        if not any("executable hash drifted" in item for item in live_drift_errors):
            errors.append("live executable content drift was accepted")

        low_risk = copy.deepcopy(bundle["plan"])
        low_risk["capability_snapshot"]["capabilities"][0]["reviewed_risk_level"] = "R0"
        low_risk["selected_tools"][0]["risk_level"] = "R0"
        low_risk["workflow_nodes"][0]["risk_level"] = "R0"
        low_risk = _finalize_fixture_plan(low_risk, approve=True)
        if not any(
            "uncatalogued risk understates" in item for item in fixture_errors(low_risk)
        ):
            errors.append("uncatalogued Shell/write capability was reported as R0")

        nested_hash_drift = copy.deepcopy(bundle["plan"])
        nested_hash_drift["capability_snapshot"]["capabilities"][0][
            "reviewed_nested_target_kinds_hash"
        ] = ZERO_HASH
        nested_hash_drift = _finalize_fixture_plan(nested_hash_drift, approve=True)
        if not any(
            "reviewed_nested_target_kinds_hash differs" in item
            for item in fixture_errors(nested_hash_drift)
        ):
            errors.append("reviewed nested target contract hash drift was accepted")

        selected_nested_drift = copy.deepcopy(bundle["plan"])
        selected_contract = selected_nested_drift["selected_tools"][0][
            "nested_target_kinds"
        ]
        selected_contract["/stdin_payload/inputs/OUTPUT"] = "input-path"
        selected_nested_drift["selected_tools"][0]["nested_target_kinds_hash"] = (
            sha256_value(selected_contract)
        )
        selected_nested_drift = _finalize_fixture_plan(
            selected_nested_drift, approve=True
        )
        if not any(
            "nested_target_kinds differs from live capability" in item
            for item in fixture_errors(selected_nested_drift)
        ):
            errors.append("selected nested target contract drift was accepted")

        parameter_hash_drift = copy.deepcopy(bundle["plan"])
        parameter_hash_drift["capability_snapshot"]["capabilities"][0][
            "reviewed_parameter_constraints_hash"
        ] = ZERO_HASH
        parameter_hash_drift = _finalize_fixture_plan(
            parameter_hash_drift, approve=True
        )
        if not any(
            "reviewed_parameter_constraints_hash differs" in item
            for item in fixture_errors(parameter_hash_drift)
        ):
            errors.append("reviewed parameter contract hash drift was accepted")

        selected_parameter_drift = copy.deepcopy(bundle["plan"])
        selected_parameter = selected_parameter_drift["selected_tools"][0][
            "parameter_constraints"
        ]
        selected_parameter["properties"]["algorithm_id"]["const"] = "native:centroids"
        selected_parameter_drift["selected_tools"][0]["parameter_constraints_hash"] = (
            sha256_value(selected_parameter)
        )
        selected_parameter_drift = _finalize_fixture_plan(
            selected_parameter_drift, approve=True
        )
        if not any(
            "parameter_constraints differs from live capability" in item
            for item in fixture_errors(selected_parameter_drift)
        ):
            errors.append("selected parameter contract drift was accepted")

        issue_missing = copy.deepcopy(bundle["plan"])
        capability = issue_missing["capability_snapshot"]["capabilities"][0]
        capability["known_issues"] = ["Fixture output can be incomplete."]
        issue_missing = _finalize_fixture_plan(issue_missing, approve=True)
        if not any(
            "known_issue_acceptances miss" in item
            for item in fixture_errors(issue_missing)
        ):
            errors.append("selected known issue without acceptance was allowed")
        issue_accepted = copy.deepcopy(issue_missing)
        capability = issue_accepted["capability_snapshot"]["capabilities"][0]
        issue_accepted["known_issue_acceptances"] = [
            {
                "id": "ISSUE-ACCEPT-001",
                "plan_id": issue_accepted["plan_id"],
                "revision": issue_accepted["revision"],
                "capability_id": capability["id"],
                "capability_description_hash": capability["description_hash"],
                "issue": capability["known_issues"][0],
                "rationale": "The isolated fixture detects incomplete output.",
                "acceptance_basis": "Explicitly reviewed in this revision.",
            }
        ]
        issue_accepted = _finalize_fixture_plan(issue_accepted, approve=True)
        accepted_errors = fixture_errors(issue_accepted)
        if accepted_errors:
            errors.append(f"exact known-issue acceptance failed: {accepted_errors}")
        stale_acceptance = copy.deepcopy(issue_accepted)
        stale_acceptance["known_issue_acceptances"][0]["revision"] = 2
        stale_acceptance = _finalize_fixture_plan(stale_acceptance, approve=True)
        if not any(
            "revision differs from current revision binding" in item
            for item in fixture_errors(stale_acceptance)
        ):
            errors.append("stale known-issue acceptance was allowed")

        contract_path = bundle["staged_root"] / "references" / "contract"
        original_contract = contract_path.read_bytes()
        staged_machine_values = {
            "program-files": (
                "Use "
                + "C:"
                + chr(92)
                + "Program Files"
                + chr(92)
                + "QGIS"
                + chr(92)
                + "x\n"
            ),
            "other-drive": ("Use " + "D:" + chr(92) + "custom" + chr(92) + "x\n"),
            "unix-opt": "Use /" + "opt/qgis/x\n",
            "unc-unicode": (
                "Use " + chr(92) * 2 + "服务器" + chr(92) + "共享" + chr(92) + "x\n"
            ),
            "unc-escaped": (
                "Use "
                + chr(92) * 4
                + "server"
                + chr(92) * 2
                + "share"
                + chr(92) * 2
                + "x\n"
            ),
            "unc-extended": (
                "Use "
                + chr(92) * 2
                + "?"
                + chr(92)
                + "UNC"
                + chr(92)
                + "server"
                + chr(92)
                + "share\n"
            ),
            "unc-wsl": (
                "Use " + chr(92) * 2 + "wsl$" + chr(92) + "Ubuntu" + chr(92) + "x\n"
            ),
            "unc-forward": "Use " + "/" * 2 + "服务器/共享/x\n",
            "unc-file-uri": "Use file://server.example/share/x\n",
        }
        for label, value in staged_machine_values.items():
            raw = value.encode("utf-8")
            contract_path.write_bytes(raw)
            machine_plan = copy.deepcopy(bundle["plan"])
            entry = next(
                item
                for item in machine_plan["target_skill_package"]["files"]
                if item["relative_path"] == "references/contract"
            )
            entry["expected_sha256"] = sha256_bytes(raw)
            entry["expected_size_bytes"] = len(raw)
            machine_plan = _finalize_fixture_plan(machine_plan, approve=True)
            machine_errors = fixture_errors(machine_plan)
            validate_staged_package(
                machine_plan,
                bundle["staged_root"],
                bundle["manifest_path"],
                bundle["skills_root"],
                machine_errors,
            )
            if not any("obvious machine path" in item for item in machine_errors):
                errors.append(
                    f"extensionless staged machine path was accepted: {label}"
                )
        contract_path.write_bytes(original_contract)

    frontmatter_errors: list[str] = []
    parsed = parse_skill_frontmatter(
        '---\nname: sample-skill\ndescription: "Sample"\n---\n\nBody\n',
        frontmatter_errors,
    )
    if frontmatter_errors or parsed.get("name") != "sample-skill":
        errors.append("strict frontmatter fixture failed")
    forbidden_errors: list[str] = []
    parse_skill_frontmatter(
        '---\nname: sample-skill\ndescription: "Sample"\n'
        'allowed-tools: "x"\n---\nBody\n',
        forbidden_errors,
    )
    if not forbidden_errors:
        errors.append("forbidden frontmatter field was accepted")

    valid_openai_errors: list[str] = []
    validate_openai_yaml(
        'interface:\n'
        '  display_name: "Sample Skill"\n'
        '  short_description: "Create reviewed geospatial workflows"\n'
        '  default_prompt: "Use $sample-skill to process geospatial data."\n',
        "sample-skill",
        valid_openai_errors,
    )
    if valid_openai_errors:
        errors.append(f"valid agents/openai.yaml fixture failed: {valid_openai_errors}")
    invalid_openai_errors: list[str] = []
    validate_openai_yaml(
        'interface:\n'
        '  display_name: "Sample Skill"\n'
        '  short_description: "Too short"\n'
        '  default_prompt: "Process geospatial data."\n',
        "sample-skill",
        invalid_openai_errors,
    )
    if not any("25 to 64" in item for item in invalid_openai_errors):
        errors.append("short agents/openai.yaml description was accepted")
    if not any("exact $skill-name" in item for item in invalid_openai_errors):
        errors.append("agents/openai.yaml prompt without the skill name was accepted")

    domain_errors: list[str] = []
    validate_geospatial_semantics(
        {
            "geospatial_semantics": {
                "domains": ["point-cloud"],
                "point_cloud_policy": "Not applicable because no policy was written.",
            }
        },
        domain_errors,
    )
    if not any("cannot be marked not-applicable" in item for item in domain_errors):
        errors.append("selected point-cloud domain accepted a not-applicable policy")
    return errors


def build_parser() -> argparse.ArgumentParser:
    skill_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--mode",
        choices=("review", "render", "verify-staged"),
        default="review",
    )
    parser.add_argument("--plan", type=Path)
    parser.add_argument(
        "--plan-schema",
        type=Path,
        default=skill_root / "references" / "qgis-skill-plan.schema.json",
    )
    parser.add_argument(
        "--catalog",
        type=Path,
        default=skill_root / "references" / "qgis-tools.catalog.json",
    )
    parser.add_argument(
        "--catalog-schema",
        type=Path,
        default=skill_root / "references" / "qgis-tools.catalog.schema.json",
    )
    parser.add_argument("--staged-package-root", type=Path)
    parser.add_argument("--skills-service-manifest", type=Path)
    parser.add_argument("--skills-root", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser


def main() -> int:
    reconfigure_stdout = getattr(sys.stdout, "reconfigure", None)
    if callable(reconfigure_stdout):
        reconfigure_stdout(encoding="utf-8")
    arguments = build_parser().parse_args()
    if arguments.self_test:
        errors = run_self_test()
        print(
            json.dumps(
                {"ok": not errors, "mode": "self-test", "errors": errors},
                ensure_ascii=False,
                indent=2,
            )
        )
        return 0 if not errors else 2
    if arguments.plan is None:
        print(
            json.dumps(
                {"ok": False, "mode": arguments.mode, "errors": ["--plan is required"]},
                ensure_ascii=False,
                indent=2,
            )
        )
        return 2
    try:
        plan = load_strict_json(arguments.plan)
        plan_schema = load_strict_json(arguments.plan_schema)
        catalog = load_strict_json(arguments.catalog)
        catalog_schema = load_strict_json(arguments.catalog_schema)
        if not all(
            isinstance(value, dict)
            for value in (plan, plan_schema, catalog, catalog_schema)
        ):
            raise GateInputError("plan, catalog, and both schemas must be objects")
        errors, computed = validate_common(plan, catalog, plan_schema, catalog_schema)
        if arguments.mode == "verify-staged":
            validate_staged_package(
                plan,
                arguments.staged_package_root,
                arguments.skills_service_manifest,
                arguments.skills_root,
                errors,
            )
        if arguments.mode == "render" and not errors:
            sys.stdout.write(render_review_markdown(plan))
            return 0
    except (OSError, GateInputError) as error:
        errors = [str(error)]
        computed = {}
    print(
        json.dumps(
            {
                "ok": not errors,
                "mode": arguments.mode,
                "computed": computed,
                "errors": errors,
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0 if not errors else 2


if __name__ == "__main__":
    sys.exit(main())
