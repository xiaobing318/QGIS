"""Catalog and package validation for QCopilots QGIS Binary.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-08-02"
__copyright__ = "Copyright 2026, The QGIS Project"

import argparse
import copy
import hashlib
import json
import math
import os
import re
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path, PurePosixPath
from types import SimpleNamespace


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
PLUGIN_ROOT = (
    REPOSITORY_ROOT
    / "python"
    / "plugins"
    / "qcopilots_mcp_server_qgis_binary"
)
CATALOG_PATH = PLUGIN_ROOT / "qgis_binaries.json"
SCHEMA_PATH = PLUGIN_ROOT / "qgis_binaries.schema.json"
DEFAULT_SNAPSHOT_ROOT = Path(
    r"C:\Data\QGISPackages\ff40200qcopilots\QGIS40200-RelWithDebInfo"
)
EXPECTED_GROUP_COUNTS = {
    "qgis_cli": 3,
    "gdal_native": 21,
    "ogr_gnm_avc": 7,
    "gdal_python": 16,
    "proj": 8,
    "geos": 1,
    "geotiff": 4,
    "gps": 1,
    "laszip": 6,
    "hdf5": 15,
    "xml_and_utilities": 6,
    "grass_disabled": 419,
    "qt_runtime_disabled": 22,
    "python_runtime_disabled": 29,
    "qgis_internal_disabled": 3,
    "osgeo4w_internal_disabled": 12,
    "risk3_blocked": 18,
}
EXPECTED_GROUP_ENABLED_COUNTS = {
    "qgis_cli": 3,
    "gdal_native": 21,
    "ogr_gnm_avc": 7,
    "gdal_python": 16,
    "proj": 8,
    "geos": 1,
    "geotiff": 4,
    "gps": 0,
    "laszip": 6,
    "hdf5": 15,
    "xml_and_utilities": 6,
    "grass_disabled": 0,
    "qt_runtime_disabled": 0,
    "python_runtime_disabled": 0,
    "qgis_internal_disabled": 0,
    "osgeo4w_internal_disabled": 0,
    "risk3_blocked": 0,
}
EXPECTED_RISK_COUNTS = {"R1": 51, "R2": 522, "R3": 18}
EXPECTED_ENABLED_RISK_COUNTS = {"R1": 29, "R2": 58}
EXPECTED_STATIC_ENABLED_PATHS = {
    "bin/avcexport.exe",
    "bin/avcimport.exe",
    "bin/applygeo.exe",
    "bin/geotifcp.exe",
    "bin/listgeo.exe",
    "bin/makegeo.exe",
    "bin/lasblock.exe",
}
VALID_PE_MACHINES = {0x014C, 0x8664, 0xAA64}


class CatalogValidationError(ValueError):
    pass


def load_catalog(path=CATALOG_PATH):
    with Path(path).open("r", encoding="utf-8") as stream:
        return json.load(stream)


SCHEMA_ANNOTATION_KEYWORDS = {
    "$comment",
    "$id",
    "$schema",
    "default",
    "deprecated",
    "description",
    "examples",
    "readOnly",
    "title",
    "writeOnly",
}
SCHEMA_VALIDATION_KEYWORDS = {
    "$defs",
    "$ref",
    "additionalProperties",
    "allOf",
    "const",
    "contains",
    "else",
    "enum",
    "exclusiveMaximum",
    "exclusiveMinimum",
    "if",
    "items",
    "maxContains",
    "maxItems",
    "maxLength",
    "maxProperties",
    "maximum",
    "minContains",
    "minItems",
    "minLength",
    "minProperties",
    "minimum",
    "not",
    "oneOf",
    "pattern",
    "properties",
    "propertyNames",
    "required",
    "then",
    "type",
    "uniqueItems",
}
SCHEMA_SUPPORTED_KEYWORDS = (
    SCHEMA_ANNOTATION_KEYWORDS | SCHEMA_VALIDATION_KEYWORDS
)


def _json_equal(left, right):
    if isinstance(left, bool) or isinstance(right, bool):
        return isinstance(left, bool) and isinstance(right, bool) and left == right
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return left == right
    if type(left) is not type(right):
        return False
    if isinstance(left, list):
        return len(left) == len(right) and all(
            _json_equal(left_item, right_item)
            for left_item, right_item in zip(left, right)
        )
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(
            _json_equal(left[key], right[key]) for key in left
        )
    return left == right


def _json_fingerprint(value):
    if value is None:
        return ("null",)
    if isinstance(value, bool):
        return ("boolean", value)
    if isinstance(value, (int, float)):
        return ("number", value)
    if isinstance(value, str):
        return ("string", value)
    if isinstance(value, list):
        return ("array", tuple(_json_fingerprint(item) for item in value))
    if isinstance(value, dict):
        return (
            "object",
            tuple(
                (key, _json_fingerprint(value[key]))
                for key in sorted(value)
            ),
        )
    raise TypeError(f"Unsupported JSON value: {type(value).__name__}")


def _matches_json_type(value, expected):
    if expected == "null":
        return value is None
    if expected == "boolean":
        return isinstance(value, bool)
    if expected == "integer":
        return (
            isinstance(value, int)
            and not isinstance(value, bool)
        ) or (
            isinstance(value, float)
            and math.isfinite(value)
            and value.is_integer()
        )
    if expected == "number":
        return (
            isinstance(value, (int, float))
            and not isinstance(value, bool)
            and math.isfinite(value)
        )
    if expected == "string":
        return isinstance(value, str)
    if expected == "array":
        return isinstance(value, list)
    if expected == "object":
        return isinstance(value, dict)
    raise AssertionError(f"Unsupported JSON Schema type: {expected}")


def _resolve_local_schema_ref(root_schema, reference):
    if not isinstance(reference, str) or not reference.startswith("#"):
        raise AssertionError(f"Only local JSON Schema refs are supported: {reference}")
    if reference == "#":
        return root_schema
    if not reference.startswith("#/"):
        raise AssertionError(f"Invalid local JSON Schema ref: {reference}")
    node = root_schema
    for raw_part in reference[2:].split("/"):
        part = raw_part.replace("~1", "/").replace("~0", "~")
        if not isinstance(node, dict) or part not in node:
            raise AssertionError(f"Unresolved local JSON Schema ref: {reference}")
        node = node[part]
    if not isinstance(node, (dict, bool)):
        raise AssertionError(f"JSON Schema ref does not identify a schema: {reference}")
    return node


def _assert_supported_schema(schema):
    root_schema = schema
    supported_types = {
        "array",
        "boolean",
        "integer",
        "null",
        "number",
        "object",
        "string",
    }
    schema_value_keywords = (
        "additionalProperties",
        "contains",
        "else",
        "if",
        "items",
        "not",
        "propertyNames",
        "then",
    )
    nonnegative_integer_keywords = (
        "maxContains",
        "maxItems",
        "maxLength",
        "maxProperties",
        "minContains",
        "minItems",
        "minLength",
        "minProperties",
    )
    number_keywords = (
        "exclusiveMaximum",
        "exclusiveMinimum",
        "maximum",
        "minimum",
    )

    def visit(node, path):
        if isinstance(node, bool):
            return
        if not isinstance(node, dict):
            raise AssertionError(f"{path} must be an object or boolean schema")
        unknown = set(node) - SCHEMA_SUPPORTED_KEYWORDS
        if unknown:
            raise AssertionError(
                f"{path} contains unsupported schema keywords: {sorted(unknown)}"
            )
        reference = node.get("$ref")
        if reference is not None:
            if not isinstance(reference, str):
                raise AssertionError(f"{path}.$ref must be a string")
            _resolve_local_schema_ref(root_schema, reference)
        expected_types = node.get("type")
        if expected_types is not None:
            if isinstance(expected_types, str):
                expected_types = [expected_types]
            if (
                not isinstance(expected_types, list)
                or not expected_types
                or any(
                    not isinstance(expected, str)
                    or expected not in supported_types
                    for expected in expected_types
                )
                or len(expected_types) != len(set(expected_types))
            ):
                raise AssertionError(f"{path}.type is not a supported type set")
        required = node.get("required")
        if required is not None and (
            not isinstance(required, list)
            or any(not isinstance(name, str) for name in required)
            or len(required) != len(set(required))
        ):
            raise AssertionError(
                f"{path}.required must be a unique string array"
            )
        enum = node.get("enum")
        if enum is not None and (
            not isinstance(enum, list)
            or not enum
            or len({_json_fingerprint(value) for value in enum}) != len(enum)
        ):
            raise AssertionError(f"{path}.enum must be a non-empty unique array")
        unique_items = node.get("uniqueItems")
        if unique_items is not None and not isinstance(unique_items, bool):
            raise AssertionError(f"{path}.uniqueItems must be boolean")
        for keyword in nonnegative_integer_keywords:
            value = node.get(keyword)
            if value is not None and (
                not isinstance(value, int)
                or isinstance(value, bool)
                or value < 0
            ):
                raise AssertionError(
                    f"{path}.{keyword} must be a non-negative integer"
                )
        for keyword in number_keywords:
            value = node.get(keyword)
            if value is not None and (
                not isinstance(value, (int, float))
                or isinstance(value, bool)
                or not math.isfinite(value)
            ):
                raise AssertionError(
                    f"{path}.{keyword} must be a finite number"
                )
        for minimum_name, maximum_name in (
            ("minContains", "maxContains"),
            ("minItems", "maxItems"),
            ("minLength", "maxLength"),
            ("minProperties", "maxProperties"),
            ("minimum", "maximum"),
        ):
            if (
                minimum_name in node
                and maximum_name in node
                and node[minimum_name] > node[maximum_name]
            ):
                raise AssertionError(
                    f"{path}.{minimum_name} exceeds {maximum_name}"
                )
        pattern = node.get("pattern")
        if pattern is not None:
            if not isinstance(pattern, str):
                raise AssertionError(f"{path}.pattern must be a string")
            try:
                re.compile(pattern)
            except re.error as error:
                raise AssertionError(
                    f"{path}.pattern is not supported by the test validator: {error}"
                ) from error
        for keyword in ("$defs", "properties"):
            children = node.get(keyword, {})
            if not isinstance(children, dict):
                raise AssertionError(f"{path}.{keyword} must be an object")
            for name, child in children.items():
                visit(child, f"{path}.{keyword}.{name}")
        for keyword in schema_value_keywords:
            if keyword not in node:
                continue
            child = node[keyword]
            if not isinstance(child, (dict, bool)):
                raise AssertionError(
                    f"{path}.{keyword} must be an object or boolean schema"
                )
            visit(child, f"{path}.{keyword}")
        for keyword in ("allOf", "oneOf"):
            children = node.get(keyword, [])
            if keyword in node and (
                not isinstance(children, list) or not children
            ):
                raise AssertionError(
                    f"{path}.{keyword} must be a non-empty array"
                )
            for index, child in enumerate(children):
                visit(child, f"{path}.{keyword}[{index}]")

    visit(schema, "#")

    def assert_acyclic_refs(node, path, references):
        if isinstance(node, bool):
            return
        reference = node.get("$ref")
        next_references = references
        if reference is not None:
            if reference in references:
                chain = " -> ".join((*references, reference))
                raise AssertionError(f"{path} contains a cyclic $ref: {chain}")
            next_references = (*references, reference)
            assert_acyclic_refs(
                _resolve_local_schema_ref(root_schema, reference),
                reference,
                next_references,
            )
        for keyword in ("$defs", "properties"):
            for name, child in node.get(keyword, {}).items():
                assert_acyclic_refs(
                    child,
                    f"{path}.{keyword}.{name}",
                    next_references,
                )
        for keyword in schema_value_keywords:
            child = node.get(keyword)
            if isinstance(child, (dict, bool)):
                assert_acyclic_refs(
                    child,
                    f"{path}.{keyword}",
                    next_references,
                )
        for keyword in ("allOf", "oneOf"):
            for index, child in enumerate(node.get(keyword, [])):
                assert_acyclic_refs(
                    child,
                    f"{path}.{keyword}[{index}]",
                    next_references,
                )

    assert_acyclic_refs(schema, "#", ())


def _schema_contract_errors(schema, instance, root_schema=None, path="$"):
    if root_schema is None:
        _assert_supported_schema(schema)
        root_schema = schema
    if schema is True:
        return []
    if schema is False:
        return [f"{path}: boolean false schema rejected the value"]
    if not isinstance(schema, dict):
        return [f"{path}: invalid non-object schema"]

    errors = []
    if "$ref" in schema:
        target = _resolve_local_schema_ref(root_schema, schema["$ref"])
        errors.extend(
            _schema_contract_errors(target, instance, root_schema, path)
        )

    for child in schema.get("allOf", []):
        errors.extend(
            _schema_contract_errors(child, instance, root_schema, path)
        )
    if "oneOf" in schema:
        matches = sum(
            not _schema_contract_errors(child, instance, root_schema, path)
            for child in schema["oneOf"]
        )
        if matches != 1:
            errors.append(
                f"{path}: oneOf matched {matches} branches instead of exactly one"
            )
    if "not" in schema and not _schema_contract_errors(
        schema["not"], instance, root_schema, path
    ):
        errors.append(f"{path}: not schema matched")
    if "if" in schema:
        condition_matches = not _schema_contract_errors(
            schema["if"], instance, root_schema, path
        )
        selected = "then" if condition_matches else "else"
        if selected in schema:
            errors.extend(
                _schema_contract_errors(
                    schema[selected], instance, root_schema, path
                )
            )

    expected_types = schema.get("type")
    if expected_types is not None:
        if isinstance(expected_types, str):
            expected_types = [expected_types]
        if not any(
            _matches_json_type(instance, expected)
            for expected in expected_types
        ):
            errors.append(
                f"{path}: expected JSON type {expected_types}, "
                f"got {type(instance).__name__}"
            )
            return errors

    if "const" in schema and not _json_equal(instance, schema["const"]):
        errors.append(f"{path}: value does not equal const {schema['const']!r}")
    if "enum" in schema and not any(
        _json_equal(instance, candidate) for candidate in schema["enum"]
    ):
        errors.append(f"{path}: value is not in enum {schema['enum']!r}")

    if isinstance(instance, dict):
        properties = schema.get("properties", {})
        for name in schema.get("required", []):
            if name not in instance:
                errors.append(f"{path}: required property {name!r} is missing")
        for name, value in instance.items():
            child_path = f"{path}.{name}"
            if name in properties:
                errors.extend(
                    _schema_contract_errors(
                        properties[name], value, root_schema, child_path
                    )
                )
            else:
                additional = schema.get("additionalProperties", True)
                if additional is False:
                    errors.append(f"{child_path}: additional property is forbidden")
                elif isinstance(additional, (dict, bool)):
                    errors.extend(
                        _schema_contract_errors(
                            additional, value, root_schema, child_path
                        )
                    )
                else:
                    raise AssertionError(
                        f"{path}.additionalProperties is not a schema"
                    )
            if "propertyNames" in schema:
                errors.extend(
                    _schema_contract_errors(
                        schema["propertyNames"], name, root_schema, child_path
                    )
                )
        if len(instance) < schema.get("minProperties", 0):
            errors.append(f"{path}: object has too few properties")
        if "maxProperties" in schema and len(instance) > schema["maxProperties"]:
            errors.append(f"{path}: object has too many properties")

    if isinstance(instance, list):
        if "items" in schema:
            for index, value in enumerate(instance):
                errors.extend(
                    _schema_contract_errors(
                        schema["items"],
                        value,
                        root_schema,
                        f"{path}[{index}]",
                    )
                )
        if len(instance) < schema.get("minItems", 0):
            errors.append(f"{path}: array has too few items")
        if "maxItems" in schema and len(instance) > schema["maxItems"]:
            errors.append(f"{path}: array has too many items")
        if schema.get("uniqueItems"):
            fingerprints = [_json_fingerprint(value) for value in instance]
            if len(fingerprints) != len(set(fingerprints)):
                errors.append(f"{path}: array items are not unique")
        if "contains" in schema:
            matches = sum(
                not _schema_contract_errors(
                    schema["contains"],
                    value,
                    root_schema,
                    f"{path}[{index}]",
                )
                for index, value in enumerate(instance)
            )
            minimum = schema.get("minContains", 1)
            maximum = schema.get("maxContains")
            if matches < minimum:
                errors.append(
                    f"{path}: contains matched {matches}, below {minimum}"
                )
            if maximum is not None and matches > maximum:
                errors.append(
                    f"{path}: contains matched {matches}, above {maximum}"
                )

    if isinstance(instance, str):
        if len(instance) < schema.get("minLength", 0):
            errors.append(f"{path}: string is shorter than minLength")
        if "maxLength" in schema and len(instance) > schema["maxLength"]:
            errors.append(f"{path}: string is longer than maxLength")
        if "pattern" in schema and re.search(schema["pattern"], instance) is None:
            errors.append(f"{path}: string does not match pattern")

    if (
        isinstance(instance, (int, float))
        and not isinstance(instance, bool)
        and math.isfinite(instance)
    ):
        if "minimum" in schema and instance < schema["minimum"]:
            errors.append(f"{path}: number is below minimum")
        if "maximum" in schema and instance > schema["maximum"]:
            errors.append(f"{path}: number is above maximum")
        if (
            "exclusiveMinimum" in schema
            and instance <= schema["exclusiveMinimum"]
        ):
            errors.append(f"{path}: number is not above exclusiveMinimum")
        if (
            "exclusiveMaximum" in schema
            and instance >= schema["exclusiveMaximum"]
        ):
            errors.append(f"{path}: number is not below exclusiveMaximum")
    return errors


def _prompting_schema_nodes(schema):
    yield "#", schema

    def walk(node, path):
        if not isinstance(node, dict):
            return
        for keyword in ("$defs", "properties"):
            for name, child in node.get(keyword, {}).items():
                child_path = f"{path}.{keyword}.{name}"
                yield child_path, child
                yield from walk(child, child_path)
        for keyword in ("additionalProperties", "items", "propertyNames"):
            child = node.get(keyword)
            if isinstance(child, dict):
                child_path = f"{path}.{keyword}"
                yield child_path, child
                yield from walk(child, child_path)
        for index, child in enumerate(node.get("oneOf", [])):
            child_path = f"{path}.oneOf[{index}]"
            yield child_path, child
            yield from walk(child, child_path)

    yield from walk(schema, "#")


def _described_schema_nodes(node, path="#"):
    if isinstance(node, dict):
        if isinstance(node.get("description"), str):
            yield path, node
        for name, value in node.items():
            yield from _described_schema_nodes(value, f"{path}.{name}")
    elif isinstance(node, list):
        for index, value in enumerate(node):
            yield from _described_schema_nodes(value, f"{path}[{index}]")


def _validated_relative_path(value):
    if not isinstance(value, str) or not value:
        raise CatalogValidationError("binary path must be a non-empty string")
    if "\\" in value or re.match(r"^[A-Za-z]:", value) or value.startswith("/"):
        raise CatalogValidationError(f"path_escape: {value}")
    relative = PurePosixPath(value)
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise CatalogValidationError(f"path_escape: {value}")
    return relative


def _verify_static_probe(root, item):
    relative = _validated_relative_path(item["path"])
    executable = root.joinpath(*relative.parts)
    try:
        with executable.open("rb") as stream:
            header = stream.read(4096)
    except OSError as error:
        raise CatalogValidationError(
            f"static_probe_unreadable: {item['path']}: {error}"
        ) from error
    if len(header) < 64 or header[:2] != b"MZ":
        raise CatalogValidationError(f"static_probe_not_pe: {item['path']}")
    pe_offset = struct.unpack_from("<I", header, 0x3C)[0]
    if pe_offset + 6 > len(header) or header[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise CatalogValidationError(f"static_probe_not_pe: {item['path']}")
    machine = struct.unpack_from("<H", header, pe_offset + 4)[0]
    if machine not in VALID_PE_MACHINES:
        raise CatalogValidationError(
            f"static_probe_unsupported_machine: {item['path']}: 0x{machine:04x}"
        )
    with executable.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != item["probe"]["sha256"]:
        raise CatalogValidationError(f"static_probe_sha256_mismatch: {item['path']}")


def validate_catalog_against_package(
    catalog_path=CATALOG_PATH,
    package_root=DEFAULT_SNAPSHOT_ROOT,
    verify_static_probes=True,
):
    catalog = load_catalog(catalog_path)
    root = Path(package_root).resolve(strict=True)
    binaries = catalog.get("binaries")
    if not isinstance(binaries, list):
        raise CatalogValidationError("binaries must be an array")

    counts = catalog.get("expected_counts") or {}
    configured = len(binaries)
    enabled = sum(item.get("enabled") is True for item in binaries)
    disabled = configured - enabled
    actual_counts = {
        "configured": configured,
        "enabled": enabled,
        "disabled": disabled,
    }
    if counts != actual_counts:
        raise CatalogValidationError(
            f"count_mismatch: expected {counts}, actual {actual_counts}"
        )

    ids = [item.get("id") for item in binaries]
    paths = [item.get("path") for item in binaries]
    duplicate_ids = sorted(
        value for value in set(ids) if ids.count(value) > 1
    )
    duplicate_paths = sorted(
        value for value in set(paths) if paths.count(value) > 1
    )
    if duplicate_ids:
        raise CatalogValidationError(f"duplicate_id: {duplicate_ids[0]}")
    if duplicate_paths:
        raise CatalogValidationError(f"duplicate_path: {duplicate_paths[0]}")

    known_groups = set(catalog.get("groups") or {})
    known_environments = set(catalog.get("environment_profiles") or {})
    for item in binaries:
        relative = _validated_relative_path(item.get("path"))
        resolved = root.joinpath(*relative.parts).resolve(strict=False)
        if not resolved.is_relative_to(root):
            raise CatalogValidationError(f"path_escape: {item['path']}")
        if item.get("group") not in known_groups:
            raise CatalogValidationError(f"unknown_group: {item.get('group')}")
        if item.get("environment") not in known_environments:
            raise CatalogValidationError(
                f"unknown_environment: {item.get('environment')}"
            )
        if item.get("enabled") and item.get("disabled_reason") is not None:
            raise CatalogValidationError(
                f"enabled_binary_has_disabled_reason: {item['id']}"
            )
        if not item.get("enabled") and not item.get("disabled_reason"):
            raise CatalogValidationError(
                f"disabled_binary_missing_reason: {item['id']}"
            )
        if item.get("risk") == "R3" and item.get("enabled"):
            raise CatalogValidationError(f"risk3_enabled: {item['id']}")

    configured_paths = set(paths)
    actual_paths = {
        executable.relative_to(root).as_posix()
        for executable in root.rglob("*.exe")
        if executable.is_file()
    }
    missing = sorted(configured_paths - actual_paths)
    extra = sorted(actual_paths - configured_paths)
    if missing:
        raise CatalogValidationError(f"missing_executable: {missing[0]}")
    if extra:
        raise CatalogValidationError(f"extra_executable: {extra[0]}")

    if verify_static_probes:
        for item in binaries:
            if item.get("probe", {}).get("mode") == "static":
                _verify_static_probe(root, item)
    return {
        **actual_counts,
        "missing": 0,
        "extra": 0,
        "duplicate_ids": 0,
        "duplicate_paths": 0,
    }


def _materialize_configured_paths(catalog, root, skip_path=None):
    for item in catalog["binaries"]:
        if item["path"] == skip_path:
            continue
        relative = _validated_relative_path(item["path"])
        executable = Path(root).joinpath(*relative.parts)
        executable.parent.mkdir(parents=True, exist_ok=True)
        executable.write_bytes(b"MZ")


def _write_catalog(catalog, path):
    Path(path).write_text(
        json.dumps(catalog, ensure_ascii=False),
        encoding="utf-8",
    )


def probe_enabled_binaries(
    package_root,
    catalog_path=CATALOG_PATH,
    run_factory=None,
):
    """Run configured read-only probes in an isolated temporary workspace."""

    from qcopilots_common.qgis_binary_jobs import QGISBinaryJobManager

    root = Path(package_root).resolve(strict=True)
    catalog = load_catalog(catalog_path)
    validation = validate_catalog_against_package(
        catalog_path,
        root,
        verify_static_probes=False,
    )
    enabled = [item for item in catalog["binaries"] if item["enabled"]]
    failures = []
    probed = 0
    runner = run_factory or subprocess.run
    manager = None
    with tempfile.TemporaryDirectory(prefix="qcopilots-binary-probes-") as directory:
        isolation_root = Path(directory)
        isolated_paths = {
            "TEMP": isolation_root / "temp",
            "TMP": isolation_root / "tmp",
            "APPDATA": isolation_root / "appdata",
            "LOCALAPPDATA": isolation_root / "localappdata",
            "QGIS_CUSTOM_CONFIG_PATH": isolation_root / "qgis-config",
            "XDG_CONFIG_HOME": isolation_root / "xdg-config",
            "GDAL_PAM_PROXY_DIR": isolation_root / "gdal-pam",
            "PROJ_USER_WRITABLE_DIRECTORY": isolation_root / "proj-user",
        }
        for path in isolated_paths.values():
            path.mkdir(parents=True, exist_ok=True)
        base_environment = dict(os.environ)
        base_environment.pop("HOME", None)
        base_environment.update(
            {name: str(path) for name, path in isolated_paths.items()}
        )
        manager = QGISBinaryJobManager(
            catalog_path=catalog_path,
            package_root=root,
            dependencies={
                "environment": base_environment,
                "run_factory": runner,
            },
        )
        try:
            for index, item in enumerate(enabled):
                probed += 1
                try:
                    if item["probe"]["mode"] == "static":
                        _verify_static_probe(root, item)
                        continue
                    relative = _validated_relative_path(item["path"])
                    executable = root.joinpath(*relative.parts).resolve(strict=True)
                    working_directory = isolation_root / "work" / f"probe-{index:03d}"
                    working_directory.mkdir(parents=True, exist_ok=True)
                    environment = manager._environment_for(item["environment"])
                    environment.pop("HOME", None)
                    environment.update(
                        {name: str(path) for name, path in isolated_paths.items()}
                    )
                    keywords = {
                        "cwd": working_directory,
                        "env": environment,
                        "shell": False,
                        "stdin": subprocess.DEVNULL,
                        "stdout": subprocess.PIPE,
                        "stderr": subprocess.PIPE,
                        "timeout": item["probe"]["timeout_seconds"],
                        "check": False,
                    }
                    if os.name == "nt":
                        keywords["creationflags"] = subprocess.CREATE_NO_WINDOW
                    completed = runner(
                        [str(executable), *item["probe"]["arguments"]],
                        **keywords,
                    )
                    if completed.returncode not in item["probe"][
                        "success_exit_codes"
                    ]:
                        failures.append(
                            {
                                "binary_id": item["id"],
                                "reason": f"exit_code={completed.returncode}",
                            }
                        )
                except Exception as error:
                    failures.append(
                        {"binary_id": item["id"], "reason": str(error)}
                    )
        finally:
            manager.shutdown(timeout_seconds=0)
    return {
        "configured": validation["configured"],
        "enabled": len(enabled),
        "probed": probed,
        "failed": len(failures),
        "failures": failures,
    }


class TestQCopilotsQGISBinaryCatalog(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = load_catalog()
        cls.schema = load_catalog(SCHEMA_PATH)

    def test_schema_is_strict_draft_2020_12(self):
        self.assertEqual(
            self.schema["$schema"],
            "https://json-schema.org/draft/2020-12/schema",
        )
        _assert_supported_schema(self.schema)
        self.assertIs(self.schema["additionalProperties"], False)
        self.assertEqual(
            set(self.schema["required"]),
            {
                "$schema",
                "version",
                "expected_counts",
                "package_root_resolution",
                "environment_profiles",
                "groups",
                "binaries",
            },
        )
        for definition in (
            "expectedCounts",
            "packageRootResolution",
            "environmentProfile",
            "groupDefaults",
            "argvProbe",
            "staticProbe",
            "binary",
        ):
            with self.subTest(definition=definition):
                self.assertIs(
                    self.schema["$defs"][definition]["additionalProperties"],
                    False,
                )

    def test_schema_descriptions_cover_prompting_contract(self):
        prompting_nodes = list(_prompting_schema_nodes(self.schema))
        self.assertGreater(len(prompting_nodes), 50)
        for path, node in prompting_nodes:
            with self.subTest(path=path):
                description = node.get("description")
                self.assertIsInstance(description, str)
                self.assertGreaterEqual(len(description), 16)

        described_nodes = list(_described_schema_nodes(self.schema))
        self.assertGreaterEqual(len(described_nodes), len(prompting_nodes))
        for path, node in described_nodes:
            with self.subTest(path=path):
                description = node["description"]
                self.assertIn("字段作用", description)
                self.assertIn("候选值", description)
                self.assertIn("影响", description)
                self.assertRegex(description, r"[一-鿿]")

    def test_standard_library_schema_validator_semantics(self):
        with self.assertRaisesRegex(AssertionError, "unsupported schema"):
            _schema_contract_errors({"unsupported_keyword": True}, None)
        with self.assertRaisesRegex(
            AssertionError, "additionalProperties must be"
        ):
            _schema_contract_errors(
                {"type": "object", "additionalProperties": 0},
                {"unexpected": True},
            )
        with self.assertRaisesRegex(AssertionError, "supported type"):
            _schema_contract_errors(
                {"$defs": {"unused": {"type": "date"}}},
                None,
            )
        with self.assertRaisesRegex(AssertionError, "Only local"):
            _schema_contract_errors(
                {"$ref": "https://example.com/schema.json"},
                None,
            )
        with self.assertRaisesRegex(AssertionError, "Unresolved local"):
            _schema_contract_errors({"$ref": "#/$defs/missing"}, None)
        with self.assertRaisesRegex(AssertionError, "cyclic"):
            _schema_contract_errors({"$ref": "#"}, None)

        ref_with_sibling = {
            "$defs": {
                "text": {
                    "type": "string",
                    "minLength": 1,
                }
            },
            "$ref": "#/$defs/text",
            "maxLength": 2,
        }
        self.assertEqual(
            _schema_contract_errors(ref_with_sibling, "ok"),
            [],
        )
        self.assertTrue(
            _schema_contract_errors(ref_with_sibling, "long")
        )

        overlapping_one_of = {
            "oneOf": [
                {"type": "number"},
                {"type": "integer"},
            ]
        }
        self.assertTrue(
            _schema_contract_errors(overlapping_one_of, 1)
        )
        self.assertTrue(
            _schema_contract_errors(overlapping_one_of, 1.0)
        )
        self.assertEqual(
            _schema_contract_errors({"type": "integer"}, 1.0),
            [],
        )

        conditional = {
            "if": {
                "type": "object",
                "required": ["enabled"],
                "properties": {"enabled": {"const": True}},
            },
            "then": {
                "type": "object",
                "required": ["reason"],
            },
            "else": {
                "type": "object",
                "properties": {"reason": {"type": "null"}},
            },
        }
        self.assertEqual(
            _schema_contract_errors(
                conditional, {"enabled": True, "reason": "ready"}
            ),
            [],
        )
        self.assertTrue(
            _schema_contract_errors(conditional, {"enabled": True})
        )

        contains = {
            "type": "array",
            "contains": {"const": True},
            "minContains": 1,
            "maxContains": 1,
        }
        self.assertEqual(
            _schema_contract_errors(contains, [True, 1]),
            [],
        )
        self.assertTrue(
            _schema_contract_errors(contains, [True, True])
        )
        self.assertTrue(
            _schema_contract_errors({"const": True}, 1)
        )
        self.assertEqual(
            _schema_contract_errors({"const": 1}, 1.0),
            [],
        )

    def test_schema_accepts_complete_current_catalog(self):
        self.assertEqual(
            self.catalog["$schema"],
            "./qgis_binaries.schema.json",
        )
        self.assertEqual(
            _schema_contract_errors(self.schema, self.catalog),
            [],
        )

    def test_schema_rejects_invalid_catalog_mutations(self):
        mutations = []

        def mutate(name, callback, expected_path):
            document = copy.deepcopy(self.catalog)
            callback(document)
            mutations.append((name, document, expected_path))

        mutate(
            "wrong_version",
            lambda document: document.__setitem__("version", 2),
            "$.version",
        )
        mutate(
            "extra_root_property",
            lambda document: document.__setitem__("unexpected", True),
            "$.unexpected",
        )
        mutate(
            "wrong_expected_count",
            lambda document: document["expected_counts"].__setitem__(
                "enabled", 88
            ),
            "$.expected_counts.enabled",
        )
        mutate(
            "too_many_binaries",
            lambda document: document["binaries"].append(
                copy.deepcopy(document["binaries"][0])
            ),
            "$.binaries",
        )
        mutate(
            "too_few_binaries",
            lambda document: document["binaries"].pop(),
            "$.binaries",
        )
        mutate(
            "path_escape",
            lambda document: document["binaries"][0].__setitem__(
                "path", "../escape.exe"
            ),
            "$.binaries[0].path",
        )
        mutate(
            "backslash_path",
            lambda document: document["binaries"][0].__setitem__(
                "path", r"bin\escape.exe"
            ),
            "$.binaries[0].path",
        )
        mutate(
            "drive_path",
            lambda document: document["binaries"][0].__setitem__(
                "path", "C:/bin/escape.exe"
            ),
            "$.binaries[0].path",
        )
        mutate(
            "absolute_path",
            lambda document: document["binaries"][0].__setitem__(
                "path", "/bin/escape.exe"
            ),
            "$.binaries[0].path",
        )
        mutate(
            "dot_segment_path",
            lambda document: document["binaries"][0].__setitem__(
                "path", "bin/./escape.exe"
            ),
            "$.binaries[0].path",
        )
        mutate(
            "repeated_separator_path",
            lambda document: document["binaries"][0].__setitem__(
                "path", "bin//escape.exe"
            ),
            "$.binaries[0].path",
        )
        mutate(
            "non_executable_path",
            lambda document: document["binaries"][0].__setitem__(
                "path", "bin/not-an-executable.txt"
            ),
            "$.binaries[0].path",
        )
        mutate(
            "extra_binary_property",
            lambda document: document["binaries"][0].__setitem__(
                "unexpected", True
            ),
            "$.binaries[0].unexpected",
        )
        mutate(
            "invalid_profile_name",
            lambda document: document["environment_profiles"].__setitem__(
                "Not Safe",
                copy.deepcopy(document["environment_profiles"]["direct"]),
            ),
            "$.environment_profiles.Not Safe",
        )
        mutate(
            "invalid_variable_name",
            lambda document: document["environment_profiles"]["direct"][
                "variables"
            ].__setitem__("=C:", "value"),
            "$.environment_profiles.direct.variables.=C:",
        )
        mutate(
            "untrusted_setup_script",
            lambda document: document["environment_profiles"]["direct"].__setitem__(
                "setup_scripts", ["bin/untrusted.bat"]
            ),
            "$.environment_profiles.direct.setup_scripts",
        )
        mutate(
            "unknown_progress_parser",
            lambda document: document["binaries"][0].__setitem__(
                "progress_parser", {"type": "unknown"}
            ),
            "$.binaries[0].progress_parser",
        )
        mutate(
            "invalid_static_checks",
            lambda document: document["binaries"][0]["probe"].__setitem__(
                "checks", ["exists", "readable", "pe_machine"]
            ),
            "$.binaries[0].probe",
        )
        mutate(
            "invalid_static_hash",
            lambda document: document["binaries"][0]["probe"].__setitem__(
                "sha256", "A" * 64
            ),
            "$.binaries[0].probe",
        )

        for name, document, expected_path in mutations:
            with self.subTest(name=name):
                errors = _schema_contract_errors(self.schema, document)
                self.assertTrue(errors)
                self.assertTrue(
                    any(expected_path in error for error in errors),
                    errors[:10],
                )

    def test_schema_rejects_closed_object_and_candidate_mutations(self):
        argv_index = next(
            index
            for index, item in enumerate(self.catalog["binaries"])
            if item["probe"]["mode"] == "argv"
        )
        static_index = next(
            index
            for index, item in enumerate(self.catalog["binaries"])
            if item["probe"]["mode"] == "static"
        )
        progress_index = next(
            index
            for index, item in enumerate(self.catalog["binaries"])
            if isinstance(item["progress_parser"], dict)
        )
        cases = []

        def add_case(name, callback, expected_path):
            document = copy.deepcopy(self.catalog)
            callback(document)
            cases.append((name, document, expected_path))

        add_case(
            "wrong_schema_pointer",
            lambda document: document.__setitem__(
                "$schema", "./wrong.schema.json"
            ),
            "$.$schema",
        )
        add_case(
            "wrong_root_strategy",
            lambda document: document["package_root_resolution"].__setitem__(
                "strategy", "path_search"
            ),
            "$.package_root_resolution.strategy",
        )
        add_case(
            "extra_expected_count_property",
            lambda document: document["expected_counts"].__setitem__(
                "unexpected", 0
            ),
            "$.expected_counts.unexpected",
        )
        add_case(
            "extra_root_resolution_property",
            lambda document: document["package_root_resolution"].__setitem__(
                "unexpected", True
            ),
            "$.package_root_resolution.unexpected",
        )
        add_case(
            "extra_environment_profile_property",
            lambda document: document["environment_profiles"]["direct"].__setitem__(
                "unexpected", True
            ),
            "$.environment_profiles.direct.unexpected",
        )
        add_case(
            "extra_group_default_property",
            lambda document: document["groups"]["qgis_cli"].__setitem__(
                "unexpected", True
            ),
            "$.groups.qgis_cli",
        )
        add_case(
            "extra_argv_probe_property",
            lambda document: document["binaries"][argv_index][
                "probe"
            ].__setitem__("unexpected", True),
            f"$.binaries[{argv_index}].probe",
        )
        add_case(
            "unsafe_argv_probe_argument",
            lambda document: document["binaries"][argv_index][
                "probe"
            ].__setitem__("arguments", ["--destructive-maintenance"]),
            f"$.binaries[{argv_index}].probe",
        )
        add_case(
            "extra_static_probe_property",
            lambda document: document["binaries"][static_index][
                "probe"
            ].__setitem__("unexpected", True),
            f"$.binaries[{static_index}].probe",
        )
        add_case(
            "extra_progress_parser_property",
            lambda document: document["binaries"][progress_index][
                "progress_parser"
            ].__setitem__("unexpected", True),
            f"$.binaries[{progress_index}].progress_parser",
        )
        add_case(
            "non_string_environment_value",
            lambda document: document["environment_profiles"]["direct"][
                "variables"
            ].__setitem__("CUSTOM_VALUE", 1),
            "$.environment_profiles.direct.variables.CUSTOM_VALUE",
        )
        add_case(
            "unknown_static_check",
            lambda document: document["binaries"][static_index][
                "probe"
            ].__setitem__(
                "checks",
                ["exists", "readable", "pe_machine", "signature"],
            ),
            f"$.binaries[{static_index}].probe",
        )
        add_case(
            "duplicate_static_check",
            lambda document: document["binaries"][static_index][
                "probe"
            ].__setitem__(
                "checks",
                ["exists", "readable", "pe_machine", "exists"],
            ),
            f"$.binaries[{static_index}].probe",
        )

        for name, document, expected_path in cases:
            with self.subTest(name=name):
                errors = _schema_contract_errors(self.schema, document)
                self.assertTrue(errors)
                self.assertTrue(
                    any(expected_path in error for error in errors),
                    errors[:10],
                )

        valid_custom_variable = copy.deepcopy(self.catalog)
        valid_custom_variable["environment_profiles"]["direct"]["variables"][
            "CUSTOM_VALUE"
        ] = "value"
        self.assertEqual(
            _schema_contract_errors(self.schema, valid_custom_variable),
            [],
        )

    def test_binary_schema_rejects_invalid_conditions_and_boundaries(self):
        binary_schema = self.schema["$defs"]["binary"]
        enabled_argv = next(
            item
            for item in self.catalog["binaries"]
            if item["enabled"] and item["probe"]["mode"] == "argv"
        )
        disabled_static = next(
            item
            for item in self.catalog["binaries"]
            if not item["enabled"] and item["risk"] != "R3"
        )
        risk3_static = next(
            item
            for item in self.catalog["binaries"]
            if item["risk"] == "R3"
        )

        cases = []

        def invalid(name, source, callback):
            item = copy.deepcopy(source)
            callback(item)
            cases.append((name, item))

        invalid(
            "enabled_with_reason",
            enabled_argv,
            lambda item: item.__setitem__("disabled_reason", "not null"),
        )
        invalid(
            "disabled_without_reason",
            disabled_static,
            lambda item: item.__setitem__("disabled_reason", None),
        )
        invalid(
            "disabled_with_empty_reason",
            disabled_static,
            lambda item: item.__setitem__("disabled_reason", ""),
        )
        invalid(
            "risk3_enabled",
            risk3_static,
            lambda item: (
                item.__setitem__("enabled", True),
                item.__setitem__("disabled_reason", None),
            ),
        )
        invalid(
            "disabled_argv_probe",
            enabled_argv,
            lambda item: (
                item.__setitem__("enabled", False),
                item.__setitem__("disabled_reason", "disabled for test"),
            ),
        )
        invalid(
            "unknown_risk",
            enabled_argv,
            lambda item: item.__setitem__("risk", "R4"),
        )
        invalid(
            "unknown_probe",
            enabled_argv,
            lambda item: item.__setitem__("probe", {"mode": "command"}),
        )
        invalid(
            "mixed_probe_fields",
            enabled_argv,
            lambda item: item["probe"].__setitem__(
                "checks", ["exists", "readable", "pe_machine", "sha256"]
            ),
        )
        invalid(
            "percent_regex_without_pattern",
            enabled_argv,
            lambda item: item.__setitem__(
                "progress_parser", {"type": "percent_regex"}
            ),
        )
        invalid(
            "zero_timeout",
            enabled_argv,
            lambda item: item.__setitem__("timeout_seconds", 0),
        )
        invalid(
            "oversized_output",
            enabled_argv,
            lambda item: item.__setitem__(
                "max_output_bytes", 16777217
            ),
        )
        invalid(
            "missing_enabled",
            enabled_argv,
            lambda item: item.pop("enabled"),
        )

        for name, item in cases:
            with self.subTest(name=name):
                errors = _schema_contract_errors(
                    binary_schema,
                    item,
                    self.schema,
                    "$.binary",
                )
                self.assertTrue(errors)

    def test_schema_count_constraints_reject_both_directions(self):
        enabled_static = next(
            item
            for item in self.catalog["binaries"]
            if item["enabled"] and item["probe"]["mode"] == "static"
        )
        disabled_r2 = next(
            item
            for item in self.catalog["binaries"]
            if not item["enabled"] and item["risk"] == "R2"
        )

        too_few_enabled = copy.deepcopy(self.catalog)
        item = next(
            candidate
            for candidate in too_few_enabled["binaries"]
            if candidate["id"] == enabled_static["id"]
        )
        item["enabled"] = False
        item["disabled_reason"] = "disabled for count test"

        too_many_enabled = copy.deepcopy(self.catalog)
        item = next(
            candidate
            for candidate in too_many_enabled["binaries"]
            if candidate["id"] == disabled_r2["id"]
        )
        item["enabled"] = True
        item["disabled_reason"] = None

        for name, document in (
            ("too_few_enabled", too_few_enabled),
            ("too_many_enabled", too_many_enabled),
        ):
            with self.subTest(name=name):
                errors = _schema_contract_errors(self.schema, document)
                self.assertTrue(
                    any("$.binaries: contains matched" in error for error in errors),
                    errors[:10],
                )

    def test_catalog_has_exact_matrix_and_explicit_fields(self):
        binaries = self.catalog["binaries"]
        self.assertEqual(
            self.catalog["expected_counts"],
            {"configured": 591, "enabled": 87, "disabled": 504},
        )
        self.assertEqual(len(binaries), 591)
        self.assertEqual(sum(item["enabled"] for item in binaries), 87)
        self.assertEqual(sum(not item["enabled"] for item in binaries), 504)
        group_counts = {
            group: sum(item["group"] == group for item in binaries)
            for group in EXPECTED_GROUP_COUNTS
        }
        self.assertEqual(group_counts, EXPECTED_GROUP_COUNTS)
        group_enabled_counts = {
            group: sum(
                item["group"] == group and item["enabled"]
                for item in binaries
            )
            for group in EXPECTED_GROUP_ENABLED_COUNTS
        }
        self.assertEqual(
            group_enabled_counts,
            EXPECTED_GROUP_ENABLED_COUNTS,
        )
        risk_counts = {
            risk: sum(item["risk"] == risk for item in binaries)
            for risk in EXPECTED_RISK_COUNTS
        }
        self.assertEqual(risk_counts, EXPECTED_RISK_COUNTS)
        enabled_risk_counts = {
            risk: sum(
                item["enabled"] and item["risk"] == risk
                for item in binaries
            )
            for risk in EXPECTED_ENABLED_RISK_COUNTS
        }
        self.assertEqual(
            enabled_risk_counts,
            EXPECTED_ENABLED_RISK_COUNTS,
        )
        self.assertEqual(len({item["id"] for item in binaries}), 591)
        self.assertEqual(len({item["path"] for item in binaries}), 591)

        gpsbabel = next(
            item for item in binaries if item["id"] == "bin.gpsbabel"
        )
        self.assertFalse(gpsbabel["enabled"])
        self.assertEqual(gpsbabel["risk"], "R2")
        self.assertIn("Qt5Core.dll", gpsbabel["disabled_reason"])
        self.assertEqual(gpsbabel["probe"]["mode"], "static")

        required_fields = set(
            self.schema["$defs"]["binary"]["required"]
        )
        for item in binaries:
            with self.subTest(binary_id=item["id"]):
                self.assertEqual(set(item), required_fields)
                self.assertIn(item["environment"], self.catalog["environment_profiles"])
                self.assertIn(item["group"], self.catalog["groups"])
                self.assertEqual(item["path"], PurePosixPath(item["path"]).as_posix())
                self.assertNotIn("..", PurePosixPath(item["path"]).parts)
                self.assertEqual(item["success_exit_codes"], [0])
                self.assertGreater(item["timeout_seconds"], 0)
                self.assertGreaterEqual(item["max_output_bytes"], 1024)
                self.assertGreaterEqual(item["max_stdin_bytes"], 0)

    def test_risk_and_probe_policy_is_explicit(self):
        binaries = self.catalog["binaries"]
        risk3 = [item for item in binaries if item["risk"] == "R3"]
        self.assertEqual(len(risk3), 18)
        self.assertTrue(all(not item["enabled"] for item in risk3))
        self.assertTrue(
            all(item["group"] == "risk3_blocked" for item in risk3)
        )
        static_enabled = {
            item["path"]
            for item in binaries
            if item["enabled"] and item["probe"]["mode"] == "static"
        }
        self.assertEqual(static_enabled, EXPECTED_STATIC_ENABLED_PATHS)
        for item in binaries:
            with self.subTest(binary_id=item["id"]):
                probe = item["probe"]
                if probe["mode"] == "static":
                    self.assertEqual(
                        set(probe["checks"]),
                        {"exists", "readable", "pe_machine", "sha256"},
                    )
                    self.assertRegex(probe["sha256"], r"^[0-9a-f]{64}$")
                else:
                    self.assertTrue(item["enabled"])
                    self.assertTrue(probe["arguments"])
                    self.assertEqual(probe["success_exit_codes"], [0, 1, 2])

    def test_environment_scripts_are_fixed_relative_catalog_paths(self):
        profiles = self.catalog["environment_profiles"]
        self.assertEqual(
            profiles["qgis"]["setup_scripts"],
            ["bin/qgis-qt6-env.bat"],
        )
        self.assertEqual(
            profiles["osgeo4w"]["setup_scripts"],
            ["bin/o4w_env.bat"],
        )
        self.assertEqual(profiles["direct"]["setup_scripts"], [])
        for profile in profiles.values():
            for setup_script in profile["setup_scripts"]:
                _validated_relative_path(setup_script)

    def test_validation_rejects_duplicate_id_path_count_and_escape(self):
        variants = []

        duplicate_id = copy.deepcopy(self.catalog)
        duplicate_id["binaries"][1]["id"] = duplicate_id["binaries"][0]["id"]
        variants.append(("duplicate_id", duplicate_id, "duplicate_id"))

        duplicate_path = copy.deepcopy(self.catalog)
        duplicate_path["binaries"][1]["path"] = duplicate_path["binaries"][0]["path"]
        variants.append(("duplicate_path", duplicate_path, "duplicate_path"))

        count_mismatch = copy.deepcopy(self.catalog)
        count_mismatch["expected_counts"]["enabled"] = 88
        variants.append(("count_mismatch", count_mismatch, "count_mismatch"))

        path_escape = copy.deepcopy(self.catalog)
        path_escape["binaries"][0]["path"] = "../escape.exe"
        variants.append(("path_escape", path_escape, "path_escape"))

        with tempfile.TemporaryDirectory(prefix="qcopilots-binary-catalog-") as root:
            for name, catalog, message in variants:
                with self.subTest(name=name):
                    catalog_path = Path(root) / f"{name}.json"
                    _write_catalog(catalog, catalog_path)
                    with self.assertRaisesRegex(CatalogValidationError, message):
                        validate_catalog_against_package(
                            catalog_path,
                            root,
                            verify_static_probes=False,
                        )

    def test_validation_reports_missing_and_extra_executables(self):
        with tempfile.TemporaryDirectory(prefix="qcopilots-binary-missing-") as root:
            missing_path = self.catalog["binaries"][0]["path"]
            _materialize_configured_paths(self.catalog, root, skip_path=missing_path)
            catalog_path = Path(root) / "catalog.json"
            _write_catalog(self.catalog, catalog_path)
            with self.assertRaisesRegex(
                CatalogValidationError,
                f"missing_executable: {re.escape(missing_path)}",
            ):
                validate_catalog_against_package(
                    catalog_path,
                    root,
                    verify_static_probes=False,
                )

        with tempfile.TemporaryDirectory(prefix="qcopilots-binary-extra-") as root:
            _materialize_configured_paths(self.catalog, root)
            extra = Path(root) / "bin" / "unexpected.exe"
            extra.parent.mkdir(parents=True, exist_ok=True)
            extra.write_bytes(b"MZ")
            catalog_path = Path(root) / "catalog.json"
            _write_catalog(self.catalog, catalog_path)
            with self.assertRaisesRegex(
                CatalogValidationError,
                "extra_executable: bin/unexpected.exe",
            ):
                validate_catalog_against_package(
                    catalog_path,
                    root,
                    verify_static_probes=False,
                )

    def test_static_probe_verifies_pe_machine_and_hash(self):
        with tempfile.TemporaryDirectory(prefix="qcopilots-binary-probe-") as root:
            executable = Path(root) / "bin" / "probe.exe"
            executable.parent.mkdir(parents=True)
            payload = bytearray(256)
            payload[:2] = b"MZ"
            struct.pack_into("<I", payload, 0x3C, 0x80)
            payload[0x80:0x84] = b"PE\0\0"
            struct.pack_into("<H", payload, 0x84, 0x8664)
            executable.write_bytes(payload)
            item = {
                "path": "bin/probe.exe",
                "probe": {
                    "mode": "static",
                    "sha256": hashlib.sha256(payload).hexdigest(),
                },
            }
            _verify_static_probe(Path(root), item)
            item["probe"]["sha256"] = "0" * 64
            with self.assertRaisesRegex(
                CatalogValidationError,
                "static_probe_sha256_mismatch",
            ):
                _verify_static_probe(Path(root), item)

    def test_current_published_snapshot_matches_and_static_probes_pass(self):
        package_root = Path(
            os.environ.get(
                "QCOPILOTS_QGIS_BINARY_PACKAGE_ROOT",
                DEFAULT_SNAPSHOT_ROOT,
            )
        )
        if not package_root.is_dir():
            self.skipTest(f"Published QGIS package is unavailable: {package_root}")
        self.assertEqual(
            validate_catalog_against_package(
                CATALOG_PATH,
                package_root,
                verify_static_probes=True,
            ),
            {
                "configured": 591,
                "enabled": 87,
                "disabled": 504,
                "missing": 0,
                "extra": 0,
                "duplicate_ids": 0,
                "duplicate_paths": 0,
            },
        )

    def test_enabled_probe_runner_isolates_environment_and_cleans_workspace(self):
        with tempfile.TemporaryDirectory(
            prefix="qcopilots-binary-probe-runner-"
        ) as root:
            package_root = Path(root)
            argv_executable = package_root / "bin" / "argv.exe"
            static_executable = package_root / "bin" / "static.exe"
            argv_executable.parent.mkdir(parents=True)
            argv_executable.write_bytes(b"MZ argv")
            payload = bytearray(256)
            payload[:2] = b"MZ"
            struct.pack_into("<I", payload, 0x3C, 0x80)
            payload[0x80:0x84] = b"PE\0\0"
            struct.pack_into("<H", payload, 0x84, 0x8664)
            static_executable.write_bytes(payload)
            defaults = {
                "environment": "direct",
                "enabled": True,
                "disabled_reason": None,
                "risk": "R1",
                "success_exit_codes": [0],
                "timeout_seconds": 10,
                "max_output_bytes": 4096,
                "max_stdin_bytes": 0,
                "progress_parser": None,
            }
            catalog = {
                "version": 1,
                "expected_counts": {
                    "configured": 2,
                    "enabled": 2,
                    "disabled": 0,
                },
                "package_root_resolution": {
                    "strategy": "qgis_prefix_ancestor",
                    "markers": ["bin/argv.exe"],
                    "max_parent_levels": 2,
                },
                "environment_profiles": {
                    "direct": {
                        "inherit_environment": True,
                        "setup_scripts": [],
                        "variables": {},
                    }
                },
                "groups": {"test": {"defaults": defaults}},
                "binaries": [
                    {
                        **defaults,
                        "id": "argv",
                        "name": "Argv",
                        "description": "argv probe",
                        "path": "bin/argv.exe",
                        "group": "test",
                        "probe": {
                            "mode": "argv",
                            "arguments": ["--version"],
                            "success_exit_codes": [0],
                            "timeout_seconds": 3,
                        },
                    },
                    {
                        **defaults,
                        "id": "static",
                        "name": "Static",
                        "description": "static probe",
                        "path": "bin/static.exe",
                        "group": "test",
                        "probe": {
                            "mode": "static",
                            "checks": [
                                "exists",
                                "readable",
                                "pe_machine",
                                "sha256",
                            ],
                            "sha256": hashlib.sha256(payload).hexdigest(),
                        },
                    },
                ],
            }
            catalog_path = package_root / "catalog.json"
            _write_catalog(catalog, catalog_path)
            calls = []
            parent_home = os.environ.get("HOME")

            def run_factory(command, **keywords):
                calls.append((command, keywords))
                return SimpleNamespace(returncode=0, stdout=b"", stderr=b"")

            summary = probe_enabled_binaries(
                package_root,
                catalog_path,
                run_factory=run_factory,
            )
            self.assertEqual(
                summary,
                {
                    "configured": 2,
                    "enabled": 2,
                    "probed": 2,
                    "failed": 0,
                    "failures": [],
                },
            )
            self.assertEqual(len(calls), 1)
            command, keywords = calls[0]
            self.assertTrue(os.path.samefile(command[0], argv_executable))
            self.assertEqual(command[1:], ["--version"])
            self.assertFalse(keywords["shell"])
            self.assertNotIn("HOME", keywords["env"])
            self.assertEqual(os.environ.get("HOME"), parent_home)
            temporary_paths = [
                Path(keywords["cwd"]),
                Path(keywords["env"]["TEMP"]),
                Path(keywords["env"]["APPDATA"]),
                Path(keywords["env"]["QGIS_CUSTOM_CONFIG_PATH"]),
            ]
            self.assertTrue(all(not path.exists() for path in temporary_paths))


def main():
    parser = argparse.ArgumentParser()
    operation = parser.add_mutually_exclusive_group()
    operation.add_argument(
        "--validate-package-root",
        type=Path,
        help="Validate the configured catalog against an unpacked QGIS package.",
    )
    operation.add_argument(
        "--probe-enabled-root",
        type=Path,
        help="Run every enabled catalog probe against an unpacked QGIS package.",
    )
    arguments, remaining = parser.parse_known_args()
    if arguments.validate_package_root:
        print(
            json.dumps(
                validate_catalog_against_package(
                    CATALOG_PATH,
                    arguments.validate_package_root,
                    verify_static_probes=True,
                ),
                sort_keys=True,
            )
        )
        return
    if arguments.probe_enabled_root:
        summary = probe_enabled_binaries(arguments.probe_enabled_root)
        print(json.dumps(summary, ensure_ascii=False, sort_keys=True))
        if summary["failed"]:
            raise SystemExit(1)
        return
    unittest.main(argv=[__file__, *remaining])


if __name__ == "__main__":
    main()
