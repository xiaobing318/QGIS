"""Strict recognition of derived GDAL PAM statistics for Processing inputs.

The caller must still protect the raster itself and all other file-family
members.  These helpers do not authorize overwrites or assert that cached
statistics are numerically correct for a raster.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import re
import stat
import xml.etree.ElementTree as ET
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Any


MAX_PAM_BYTES = 1024 * 1024
MAX_PAM_NODES = 10000
MAX_PAM_DEPTH = 32
PAM_STATISTICS_KEYS = frozenset(
    {
        "STATISTICS_MINIMUM",
        "STATISTICS_MAXIMUM",
        "STATISTICS_MEAN",
        "STATISTICS_STDDEV",
        "STATISTICS_VALID_PERCENT",
    }
)
_FORMAT = "gdal_pam_statistics_v1"
_BAND_NUMBER = re.compile(r"[1-9][0-9]*\Z")
_STATISTIC_NUMBER = re.compile(
    r"[-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?\Z"
)
_XML_DECLARATION = re.compile(r"^\s*<\?xml\s+([^?]*)\?>")
_ENCODING = re.compile(r"\bencoding\s*=\s*(['\"])(.*?)\1", re.IGNORECASE)


def _stat_signature(metadata: os.stat_result) -> dict[str, int]:
    return {
        "size": int(metadata.st_size),
        "modified_ns": int(metadata.st_mtime_ns),
        "changed_ns": int(metadata.st_ctime_ns),
        "device": int(metadata.st_dev),
        "inode": int(metadata.st_ino),
        "mode": int(metadata.st_mode),
    }


def _file_identity(metadata: os.stat_result) -> dict[str, int]:
    signature = _stat_signature(metadata)
    # On Windows path.stat() and fstat() can expose different meanings for
    # st_ctime (creation time versus change time). Compare each clock only with
    # a later observation through the same API, never with the other API.
    signature.pop("changed_ns")
    return signature


def _empty_semantics() -> list[Any]:
    return ["PAMDataset", [], "", [], ""]


def _semantic_digest(value: list[Any]) -> str:
    return hashlib.sha256(
        json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def _structural_text(value: str | None) -> str:
    return value if value and value.strip() else ""


def _valid_statistics(values: dict[str, str]) -> bool:
    if any(
        len(value) > 256 or not _STATISTIC_NUMBER.fullmatch(value)
        for value in values.values()
    ):
        return False
    try:
        numeric = {key: Decimal(value) for key, value in values.items()}
        finite = all(
            value.is_finite() and math.isfinite(float(value))
            for value in numeric.values()
        )
    except (TypeError, ValueError, OverflowError, InvalidOperation):
        return False
    if not finite:
        return False
    minimum = numeric.get("STATISTICS_MINIMUM")
    maximum = numeric.get("STATISTICS_MAXIMUM")
    mean = numeric.get("STATISTICS_MEAN")
    if minimum is not None and maximum is not None and minimum > maximum:
        return False
    if mean is not None and (
        (minimum is not None and mean < minimum)
        or (maximum is not None and mean > maximum)
    ):
        return False
    if numeric.get("STATISTICS_STDDEV", 0.0) < 0.0:
        return False
    valid_percent = numeric.get("STATISTICS_VALID_PERCENT", 0.0)
    return 0.0 <= valid_percent <= 100.0


def _pam_semantics(
    root: ET.Element, band_count: int | None
) -> tuple[list[Any], dict[str, dict[str, str]]] | None:
    if root.tag != "PAMDataset":
        return None
    stack = [(root, 0)]
    nodes = 0
    while stack:
        element, depth = stack.pop()
        nodes += 1
        if (
            nodes > MAX_PAM_NODES
            or depth > MAX_PAM_DEPTH
            or not isinstance(element.tag, str)
            or element.tag.startswith("{")
            or any(key.startswith("{") for key in element.attrib)
        ):
            return None
        stack.extend((child, depth + 1) for child in element)

    statistics: dict[str, dict[str, str]] = {}
    removed: set[int] = set()
    empty_wrappers: set[int] = set()
    seen_bands: set[str] = set()
    for band in root:
        if band.tag != "PAMRasterBand":
            continue
        number = band.get("band", "")
        if not _BAND_NUMBER.fullmatch(number) or len(number) > 9:
            return None
        if number in seen_bands or (
            band_count is not None and int(number) > band_count
        ):
            return None
        seen_bands.add(number)
        defaults = [
            item
            for item in band
            if item.tag == "Metadata" and item.get("domain", "") == ""
        ]
        if len(defaults) > 1:
            return None
        if not defaults:
            if (
                set(band.attrib) == {"band"}
                and not _structural_text(band.text)
                and not _structural_text(band.tail)
                and not len(band)
            ):
                empty_wrappers.add(id(band))
            continue
        metadata = defaults[0]
        values: dict[str, str] = {}
        seen_keys: set[str] = set()
        for item in metadata:
            if item.tag != "MDI":
                continue
            key = item.get("key", "")
            if key in seen_keys:
                return None
            seen_keys.add(key)
            if key not in PAM_STATISTICS_KEYS:
                continue
            if (
                set(item.attrib) != {"key"}
                or len(item)
                or _structural_text(item.tail)
                or not item.text
                or not item.text.strip()
            ):
                return None
            values[key] = item.text.strip()
            removed.add(id(item))
        if not _valid_statistics(values):
            return None
        if values:
            statistics[number] = values
        if (
            not _structural_text(metadata.text)
            and not _structural_text(metadata.tail)
            and set(metadata.attrib) <= {"domain"}
            and all(id(item) in removed for item in metadata)
        ):
            empty_wrappers.add(id(metadata))
        if (
            set(band.attrib) == {"band"}
            and not _structural_text(band.text)
            and not _structural_text(band.tail)
            and all(id(item) in empty_wrappers for item in band)
        ):
            empty_wrappers.add(id(band))

    def canonical(
        element: ET.Element, *, container: bool = False, parent_container: bool = False
    ) -> list[Any]:
        return [
            element.tag,
            sorted(element.attrib.items()),
            _structural_text(element.text) if container else (element.text or ""),
            [
                canonical(
                    child,
                    container=container and (
                        (element.tag == "PAMDataset" and child.tag == "PAMRasterBand")
                        or (element.tag == "PAMRasterBand" and child.tag == "Metadata")
                    ),
                    parent_container=container,
                )
                for child in element
                if id(child) not in removed and id(child) not in empty_wrappers
            ],
            (
                _structural_text(element.tail)
                if parent_container
                else (element.tail or "")
            ),
        ]

    return canonical(root, container=True, parent_container=True), statistics


def capture_input_cache(
    path: str | Path, band_count: int | None = None
) -> dict[str, Any] | None:
    """Capture a bounded PAM snapshot, or return None for exact-only comparison.

    ``band_count`` must come from the protected raster, not from the sidecar.
    Unknown band counts may be captured but cannot authorize cache changes.
    Before associating this snapshot with a separately computed strong file
    version, the caller must match ``exists``, ``file_sha256`` and the size and
    modification time in ``file_signature`` to that version.  Otherwise a
    concurrent edit between the two reads could replace the authorized baseline.
    """

    path = Path(path)
    if not path.name.lower().endswith(".aux.xml"):
        return None
    if band_count is not None and (
        isinstance(band_count, bool)
        or not isinstance(band_count, int)
        or band_count < 1
    ):
        return None
    snapshot = {
        "format": _FORMAT,
        "path": str(path),
        "exists": False,
        "band_count": band_count,
        "file_sha256": None,
        "file_signature": None,
        "semantic_sha256": _semantic_digest(_empty_semantics()),
        "statistics": {},
    }
    try:
        initial = path.stat(follow_symlinks=False)
    except FileNotFoundError:
        return snapshot
    except OSError:
        return None
    if not stat.S_ISREG(initial.st_mode) or initial.st_size > MAX_PAM_BYTES:
        return None
    try:
        with path.open("rb") as handle:
            opened = os.fstat(handle.fileno())
            if _file_identity(initial) != _file_identity(opened):
                return None
            contents = handle.read(MAX_PAM_BYTES + 1)
            finished = os.fstat(handle.fileno())
        final = path.stat(follow_symlinks=False)
        signature = _stat_signature(initial)
        if (
            _stat_signature(opened) != _stat_signature(finished)
            or signature != _stat_signature(final)
            or len(contents) != initial.st_size
            or len(contents) > MAX_PAM_BYTES
        ):
            return None
        text = contents.decode("utf-8-sig")
        if "\x00" in text or re.search(r"<!\s*(DOCTYPE|ENTITY)\b", text, re.I):
            return None
        declaration = _XML_DECLARATION.match(text)
        if declaration:
            encoding = _ENCODING.search(declaration.group(1))
            if encoding and encoding.group(2).lower() not in {"utf-8", "utf8"}:
                return None
        body = text[declaration.end() :] if declaration else text
        if "<!--" in body or "<?" in body:
            return None
        parser = ET.XMLParser(
            target=ET.TreeBuilder(insert_comments=True, insert_pis=True)
        )
        root = ET.fromstring(text, parser=parser)
        parsed = _pam_semantics(root, band_count)
        if parsed is None:
            return None
        semantics, statistics = parsed
    except (OSError, UnicodeError, ET.ParseError, ValueError, OverflowError):
        return None
    snapshot.update(
        exists=True,
        file_sha256=hashlib.sha256(contents).hexdigest(),
        file_signature=signature,
        semantic_sha256=_semantic_digest(semantics),
        statistics=statistics,
    )
    return snapshot


def compare_input_cache(
    expected: dict[str, Any] | None, current: dict[str, Any] | None
) -> list[dict[str, Any]] | None:
    """Return accepted statistics-change diagnostics, or None to reject.

    This is exclusively a post-run exception to exact input-version matching.
    The caller must first prove the parent raster and other family members are
    unchanged.  A missing baseline is distinct from an existing empty XML file.
    ``[]`` indicates identical captured snapshots, not a general XML exception.
    """

    if not isinstance(expected, dict) or not isinstance(current, dict):
        return None
    if expected == current:
        return [] if expected.get("format") == _FORMAT else None
    if (
        expected.get("format") != _FORMAT
        or current.get("format") != _FORMAT
        or expected.get("path") != current.get("path")
        or not current.get("exists")
        or not isinstance(expected.get("band_count"), int)
        or expected.get("band_count", 0) < 1
        or expected.get("band_count") != current.get("band_count")
        or expected.get("semantic_sha256") != current.get("semantic_sha256")
    ):
        return None
    before = expected.get("statistics", {})
    after = current.get("statistics", {})
    for band, values in before.items():
        if band not in after or not set(values) <= set(after[band]):
            return None
    diagnostics = []
    for band, values in sorted(after.items(), key=lambda item: int(item[0])):
        changed = sorted(
            key
            for key, value in values.items()
            if before.get(band, {}).get(key) != value
        )
        if changed:
            diagnostics.append(
                {
                    "path": current["path"],
                    "kind": "gdal_pam_statistics",
                    "band": int(band),
                    "fields": changed,
                }
            )
    return diagnostics or None
