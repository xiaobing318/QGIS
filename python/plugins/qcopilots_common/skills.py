"""Agent Skills discovery and MCP exposure for QCopilots.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

Portions of this module are derived from skillz
<https://github.com/intellectronica/skillz>, MIT License, Copyright (c) 2025
Eleanor Berger. The adapted code keeps QCopilots' local JSON-RPC HTTP
transport so the QGIS service manager can keep using its health endpoint.
"""

from __future__ import annotations

import base64
import hashlib
import html
import json
import math
import mimetypes
import os
import re
import stat
import threading
import time
import zipfile
from collections.abc import Callable, Iterable, Iterator, Mapping
from dataclasses import dataclass, field
from datetime import date, datetime
from pathlib import Path, PurePosixPath
from types import MappingProxyType
from typing import Any
from urllib.parse import quote

from qcopilots_common.constants import MAX_FILE_READ_BYTES
from qcopilots_common.mcp_http import McpPrompt, McpResource, McpTool, ToolError


FRONT_MATTER_PATTERN = re.compile(r"^---\s*\n(.*?)\n---\s*\n(.*)", re.DOTALL)
STRICT_FRONT_MATTER_PATTERN = re.compile(r"\A---\n(.*?)\n---(?:\n|\Z)(.*)\Z", re.DOTALL)
SKILL_MARKDOWN = "SKILL.md"
SKILL_MARKDOWN_LOWER = SKILL_MARKDOWN.lower()
SKILL_NAME_PATTERN = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
SKILL_VALIDATION_STRICT = "strict"
SKILL_VALIDATION_COMPATIBLE = "compatible"
SKILL_VALIDATION_MODES = frozenset(
    {SKILL_VALIDATION_STRICT, SKILL_VALIDATION_COMPATIBLE}
)
MAX_SKILL_SCAN_DEPTH = 16
MAX_SKILL_SCAN_DIRECTORIES = 4096
MAX_SKILL_SCAN_FILES = 20000
MAX_SKILL_ROOTS = 64
MAX_SKILL_ARCHIVE_MEMBERS = 2048
MAX_SKILL_ARCHIVE_CENTRAL_DIRECTORY_BYTES = 8 * 1024 * 1024
MAX_SKILL_ARCHIVE_UNCOMPRESSED_BYTES = 32 * 1024 * 1024
MAX_SKILL_ARCHIVE_COMPRESSION_RATIO = 200
MAX_SKILL_ARCHIVE_FINGERPRINT_CANDIDATES = 256
MAX_SKILL_ARCHIVE_FINGERPRINT_BYTES = 64 * 1024 * 1024
MAX_JSON_SAFE_RECURSION_DEPTH = 64
MAX_JSON_SAFE_VISITED_NODES = 32768
MAX_JSON_SAFE_GENERATED_NODES = 32768
MAX_JSON_SAFE_OUTPUT_BYTES = MAX_FILE_READ_BYTES * 4
MAX_SKILL_SNAPSHOT_ATTEMPTS = 3
MAX_SKILL_SELECTOR_CATALOG_ITEMS = 20
MAX_SKILL_SELECTOR_CATALOG_CHARS = 512
DEFAULT_SKILL_REFRESH_INTERVAL_SECONDS = 0.25
RESERVED_TOOL_NAMES = frozenset(
    {
        "list_skills",
        "read_skill",
        "list_skill_resources",
        "read_skill_resource",
    }
)


class SkillError(Exception):
    """Base exception for skill-related failures."""


class SkillValidationError(SkillError):
    """Raised when a skill package is structurally invalid."""


class _SkillFilesystemChanged(SkillError):
    """A recoverable filesystem race which must preserve the current snapshot."""

    diagnostic_code = "skills.scan.filesystem_changed"

    def __init__(self, source: str, message: str):
        super().__init__(message)
        self.source = source


class _SkillMetadataKeyCollision(SkillValidationError):
    """Raised when compatible metadata key conversion is ambiguous."""


@dataclass(frozen=True, slots=True)
class _PathIdentity:
    device: int
    inode: int
    file_type: int
    file_attributes: int
    size: int | None
    modified_ns: int | None
    changed_ns: int | None


@dataclass(slots=True)
class _JsonSafeBudget:
    source: str
    max_depth: int = MAX_JSON_SAFE_RECURSION_DEPTH
    max_visited_nodes: int = MAX_JSON_SAFE_VISITED_NODES
    max_generated_nodes: int = MAX_JSON_SAFE_GENERATED_NODES
    max_output_bytes: int = MAX_JSON_SAFE_OUTPUT_BYTES
    visited_nodes: int = 0
    generated_nodes: int = 0
    output_bytes: int = 0

    def visit(self, field_path: str, depth: int) -> None:
        if depth > self.max_depth:
            self.fail(field_path, "recursion depth")
        self.visited_nodes += 1
        if self.visited_nodes > self.max_visited_nodes:
            self.fail(field_path, "visited node")

    def generate(self, field_path: str, output_bytes: int = 0) -> None:
        self.generated_nodes += 1
        if self.generated_nodes > self.max_generated_nodes:
            self.fail(field_path, "generated node")
        if output_bytes > self.max_output_bytes - self.output_bytes:
            self.fail(field_path, "output byte")
        self.output_bytes += output_bytes

    def fail(self, field_path: str, budget_name: str) -> None:
        raise SkillValidationError(
            f"Field '{field_path}' in {self.source} exceeds the JSON-safe "
            f"{budget_name} budget."
        )


_SKILL_TOOL_FAILURES = (
    SkillError,
    OSError,
    UnicodeError,
    zipfile.BadZipFile,
)
_SKILL_UNAVAILABLE_MESSAGE = "The requested skill is unavailable."
_SKILL_RESOURCE_UNAVAILABLE_MESSAGE = "The requested skill resource is unavailable."
_DYNAMIC_SKILL_UNAVAILABLE_MESSAGE = (
    "The skill is no longer available; refresh the skill catalog."
)


@dataclass(frozen=True, slots=True)
class SkillDiagnostic:
    """Stable, serializable diagnostic produced while discovering skills."""

    code: str
    severity: str
    source: str
    message: str

    def to_dict(self) -> dict[str, str]:
        return {
            "code": self.code,
            "severity": self.severity,
            "source": self.source,
            "message": self.message,
        }

    def as_dict(self) -> dict[str, str]:
        return self.to_dict()


@dataclass(slots=True)
class SkillMetadata:
    name: str
    description: str
    license: str | None = None
    compatibility: str | None = None
    allowed_tools: tuple[str, ...] = ()
    allowed_tools_declaration: Any = None
    metadata: dict[str, str] = field(default_factory=dict)
    extra: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class Skill:
    slug: str
    root: Path
    directory: Path
    instructions_path: Path
    metadata: SkillMetadata
    resources: tuple[Path, ...]
    body_text: str | None = None
    zip_path: Path | None = None
    zip_root_prefix: str = ""
    instructions_member: str = SKILL_MARKDOWN
    conformance: str = SKILL_VALIDATION_STRICT
    _discovered_instructions_identity: _PathIdentity | None = field(
        default=None,
        repr=False,
        compare=False,
    )
    _discovered_zip_identity: _PathIdentity | None = field(
        default=None,
        repr=False,
        compare=False,
    )
    _zip_members: frozenset[str] | None = field(default=None, init=False, repr=False)
    _trusted_root: Path = field(init=False, repr=False)
    _trusted_directory: Path = field(init=False, repr=False)
    _trusted_instructions: Path = field(init=False, repr=False)
    _trusted_zip: Path | None = field(default=None, init=False, repr=False)
    _root_identity: _PathIdentity = field(init=False, repr=False)
    _directory_identity: _PathIdentity = field(init=False, repr=False)
    _instructions_identity: _PathIdentity | None = field(
        default=None,
        init=False,
        repr=False,
    )
    _zip_identity: _PathIdentity | None = field(default=None, init=False, repr=False)
    _resource_identities: Mapping[str, _PathIdentity] = field(
        default_factory=dict,
        init=False,
        repr=False,
    )

    def __post_init__(self) -> None:
        self._trusted_root = _absolute_lexical_path(self.root)
        self._trusted_directory = _absolute_lexical_path(self.directory)
        self._trusted_instructions = _absolute_lexical_path(self.instructions_path)
        _validate_no_reparse_path_chain(self._trusted_root, self._trusted_directory)
        self._root_identity = _capture_path_identity(
            self._trusted_root,
            require_directory=True,
        )
        self._directory_identity = _capture_path_identity(
            self._trusted_directory,
            require_directory=True,
        )

        if self.zip_path is not None:
            self._trusted_zip = _absolute_lexical_path(self.zip_path)
            _validate_no_reparse_path_chain(self._trusted_root, self._trusted_zip)
            self._zip_identity = _capture_discovered_identity(
                self._trusted_zip,
                self._discovered_zip_identity,
            )
            members = {
                info.filename
                for info in self._validated_archive_inventory()
                if not info.is_dir()
            }
            if self.zip_root_prefix:
                self._zip_members = frozenset(
                    name[len(self.zip_root_prefix):]
                    for name in members
                    if name.startswith(self.zip_root_prefix)
                    and _is_valid_relative_resource_path(
                        name[len(self.zip_root_prefix):]
                    )
                )
            else:
                self._zip_members = frozenset(
                    name
                    for name in members
                    if _is_valid_relative_resource_path(name)
                )
            return

        _validate_no_reparse_path_chain(
            self._trusted_root,
            self._trusted_instructions,
        )
        self._instructions_identity = _capture_discovered_identity(
            self._trusted_instructions,
            self._discovered_instructions_identity,
        )
        resource_identities: dict[str, _PathIdentity] = {}
        for resource in self.resources:
            trusted_resource = _absolute_lexical_path(resource)
            try:
                rel_path = trusted_resource.relative_to(
                    self._trusted_directory
                ).as_posix()
            except ValueError as exc:
                raise SkillError(
                    f"Resource '{resource}' escapes skill directory"
                ) from exc
            _validate_no_reparse_path_chain(self._trusted_root, trusted_resource)
            resource_identities[_resource_identity_key(rel_path)] = (
                _capture_path_identity(
                    trusted_resource,
                    require_file=True,
                    include_content=True,
                )
            )
        self._resource_identities = MappingProxyType(resource_identities)

    @property
    def name(self) -> str:
        return self.metadata.name

    @property
    def description(self) -> str:
        return self.metadata.description

    @property
    def root_path(self) -> Path:
        return self.directory

    @property
    def source_kind(self) -> str:
        return "archive" if self.is_zip else "directory"

    @property
    def skill_path(self) -> Path:
        return self.instructions_path

    @property
    def body(self) -> str:
        return self.read_body()

    @property
    def is_zip(self) -> bool:
        return self.zip_path is not None

    def open_bytes(self, rel_path: str) -> bytes:
        _validate_relative_resource_path(rel_path)
        if self.is_zip:
            return self._read_archive_member(rel_path)

        identity = self._resource_identities.get(_resource_identity_key(rel_path))
        if identity is None:
            raise SkillError(f"Resource '{rel_path}' was not found")
        candidate = self._trusted_directory.joinpath(*PurePosixPath(rel_path).parts)
        return self._read_bound_file(candidate, identity, rel_path)

    def exists(self, rel_path: str) -> bool:
        _validate_relative_resource_path(rel_path)
        if self.is_zip:
            self._validate_archive_source()
            return rel_path in (self._zip_members or frozenset())

        identity = self._resource_identities.get(_resource_identity_key(rel_path))
        if identity is None:
            return False
        candidate = self._trusted_directory.joinpath(*PurePosixPath(rel_path).parts)
        self._validate_bound_path(candidate, identity)
        return True

    def iter_resource_paths(self) -> Iterator[str]:
        if self.zip_path is not None:
            for name in sorted(self._zip_members or set()):
                if name.lower() == SKILL_MARKDOWN_LOWER:
                    continue
                if "__MACOSX/" in name or name.endswith(".DS_Store"):
                    continue
                if not _is_valid_relative_resource_path(name):
                    continue
                yield name
            return

        for file_path in self.resources:
            try:
                yield file_path.relative_to(self.directory).as_posix()
            except ValueError:
                continue

    def read_body(self, *, allow_cached_instructions: bool = False) -> str:
        if allow_cached_instructions and self.body_text is not None:
            return self.body_text
        self._validate_instructions_source()
        if self.body_text is not None:
            return self.body_text
        if self.is_zip:
            raw = self.open_bytes(self.instructions_member).decode("utf-8")
        else:
            if self._instructions_identity is None:
                raise SkillError("Skill instructions identity is unavailable")
            raw = self._read_bound_file(
                self._trusted_instructions,
                self._instructions_identity,
                self.instructions_member,
            ).decode("utf-8")
        match = FRONT_MATTER_PATTERN.match(raw)
        if not match:
            raise SkillValidationError(
                f"Skill {self.slug} is missing YAML front matter."
            )
        return match.group(2).lstrip()

    def _validate_base_paths(self) -> None:
        _validate_no_reparse_path_chain(self._trusted_root, self._trusted_directory)
        _verify_path_identity(
            self._trusted_root,
            self._root_identity,
            require_directory=True,
        )
        _verify_path_identity(
            self._trusted_directory,
            self._directory_identity,
            require_directory=True,
        )

    def _validate_bound_path(
        self,
        path: Path,
        identity: _PathIdentity,
    ) -> None:
        self._validate_base_paths()
        _validate_no_reparse_path_chain(self._trusted_root, path)
        _verify_path_identity(
            path,
            identity,
            require_file=True,
            include_content=True,
        )

    def _validate_instructions_source(self) -> None:
        if self.is_zip:
            self._validate_archive_source()
            return
        if self._instructions_identity is None:
            raise SkillError("Skill instructions identity is unavailable")
        self._validate_bound_path(
            self._trusted_instructions,
            self._instructions_identity,
        )

    def _validate_archive_source(self) -> None:
        if self._trusted_zip is None or self._zip_identity is None:
            raise SkillError("Skill archive identity is unavailable")
        self._validate_bound_path(self._trusted_zip, self._zip_identity)

    def _validate_discovered_sources(self) -> None:
        self._validate_instructions_source()
        if self.is_zip:
            return
        for resource in self.resources:
            trusted_resource = _absolute_lexical_path(resource)
            rel_path = trusted_resource.relative_to(
                self._trusted_directory
            ).as_posix()
            identity = self._resource_identities[_resource_identity_key(rel_path)]
            self._validate_bound_path(trusted_resource, identity)

    def _open_bound_file(
        self,
        path: Path,
        identity: _PathIdentity,
    ) -> Any:
        self._validate_bound_path(path, identity)
        flags = os.O_RDONLY | getattr(os, "O_BINARY", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        try:
            descriptor = os.open(path, flags)
        except OSError as exc:
            raise SkillError(
                f"Bound skill source '{path}' could not be opened"
            ) from exc
        try:
            handle = os.fdopen(descriptor, "rb", closefd=True)
        except Exception:
            os.close(descriptor)
            raise
        try:
            _verify_stat_identity(os.fstat(handle.fileno()), identity, path)
        except Exception:
            handle.close()
            raise
        return handle

    def _verify_open_file(
        self,
        handle: Any,
        path: Path,
        identity: _PathIdentity,
    ) -> None:
        _verify_stat_identity(os.fstat(handle.fileno()), identity, path)
        # Windows stdlib 没有句柄相对的 no-follow 打开；组件和句柄身份前后复核用于失败关闭。
        self._validate_bound_path(path, identity)

    def _read_bound_file(
        self,
        path: Path,
        identity: _PathIdentity,
        display_path: str,
    ) -> bytes:
        if identity.size is not None and identity.size > MAX_FILE_READ_BYTES:
            raise SkillError(
                f"Resource '{display_path}' is too large "
                f"({identity.size} bytes, max {MAX_FILE_READ_BYTES})"
            )
        try:
            with self._open_bound_file(path, identity) as handle:
                data = handle.read(MAX_FILE_READ_BYTES + 1)
                self._verify_open_file(handle, path, identity)
        except SkillError:
            raise
        except OSError as exc:
            raise SkillError(
                f"Resource '{display_path}' could not be read: {exc}"
            ) from exc
        if len(data) > MAX_FILE_READ_BYTES:
            raise SkillError(
                f"Resource '{display_path}' is too large "
                f"(max {MAX_FILE_READ_BYTES} bytes)"
            )
        return data

    def _validated_archive_inventory(self) -> tuple[zipfile.ZipInfo, ...]:
        if self._trusted_zip is None or self._zip_identity is None:
            raise SkillError("Skill archive identity is unavailable")
        try:
            with self._open_bound_file(
                self._trusted_zip,
                self._zip_identity,
            ) as handle:
                with _open_skill_archive(handle) as archive:
                    inventory = _validate_archive_inventory_entries(
                        tuple(archive.infolist())
                    )
                self._verify_open_file(handle, self._trusted_zip, self._zip_identity)
                return inventory
        except SkillError:
            raise
        except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
            raise SkillError("Skill archive could not be inspected") from exc

    def _read_archive_member(self, rel_path: str) -> bytes:
        if self._trusted_zip is None or self._zip_identity is None:
            raise SkillError("Skill archive identity is unavailable")
        member_name = f"{self.zip_root_prefix}{rel_path}"
        try:
            with self._open_bound_file(
                self._trusted_zip,
                self._zip_identity,
            ) as handle:
                with _open_skill_archive(handle) as archive:
                    inventory = _validate_archive_inventory_entries(
                        tuple(archive.infolist())
                    )
                    try:
                        info = next(
                            item
                            for item in inventory
                            if item.filename == member_name
                        )
                    except StopIteration as exc:
                        raise SkillError(
                            f"Resource '{rel_path}' was not found"
                        ) from exc
                    data = _read_archive_info(archive, info)
                self._verify_open_file(handle, self._trusted_zip, self._zip_identity)
        except SkillError:
            raise
        except (KeyError, OSError, RuntimeError, zipfile.BadZipFile) as exc:
            raise SkillError(
                f"Resource '{rel_path}' could not be read: {exc}"
            ) from exc
        if len(data) > MAX_FILE_READ_BYTES:
            raise SkillError(
                f"Resource '{rel_path}' is too large "
                f"(max {MAX_FILE_READ_BYTES} bytes)"
            )
        return data


@dataclass(slots=True)
class _ScanState:
    roots: int = 0
    directories: int = 0
    files: int = 0


class SkillRegistry:
    """Discover and manage skills from one or more root directories."""

    def __init__(
        self,
        roots: str | Path | Iterable[str | Path],
        validation_mode: str = SKILL_VALIDATION_STRICT,
    ):
        if isinstance(roots, (str, Path)):
            roots = [roots]
        self.validation_mode = _validate_validation_mode(validation_mode)
        self.roots = tuple(
            _dedupe(
                Path(root).expanduser().resolve(strict=False)
                for root in roots
            )
        )
        self._skills_by_slug: dict[str, Skill] = {}
        self._skills_by_name: dict[str, Skill] = {}
        self._diagnostics: tuple[SkillDiagnostic, ...] = ()
        self._lock = threading.RLock()

    @property
    def root(self) -> Path:
        return self.roots[0] if self.roots else Path.cwd()

    @property
    def skills(self) -> tuple[Skill, ...]:
        with self._lock:
            return tuple(self._skills_by_slug.values())

    @property
    def diagnostics(self) -> tuple[SkillDiagnostic, ...]:
        with self._lock:
            return self._diagnostics

    def discover_skills(self) -> list[Skill]:
        self.load()
        return list(self.skills)

    def load(self) -> None:
        skills_by_slug: dict[str, Skill] = {}
        skills_by_name: dict[str, Skill] = {}
        diagnostics: list[SkillDiagnostic] = []
        state = _ScanState()
        visited: set[str] = set()

        if len(self.roots) > MAX_SKILL_ROOTS:
            message = f"Skill scan exceeds {MAX_SKILL_ROOTS} configured roots."
            diagnostics.append(
                SkillDiagnostic(
                    "skills.scan.root_limit",
                    "error",
                    ", ".join(str(root) for root in self.roots[:MAX_SKILL_ROOTS]),
                    message,
                )
            )
            with self._lock:
                self._diagnostics = tuple(diagnostics)
            raise SkillError(message)

        for root in self.roots:
            state.roots += 1
            try:
                root_stat = root.stat()
            except OSError as exc:
                message = f"Skills root {root} could not be inspected: {exc}"
                diagnostic = SkillDiagnostic(
                    "skills.root.invalid",
                    "error",
                    str(root),
                    message,
                )
                diagnostics.append(diagnostic)
                with self._lock:
                    self._diagnostics = tuple(diagnostics)
                raise SkillError(message) from exc
            if not stat.S_ISDIR(root_stat.st_mode):
                diagnostic = SkillDiagnostic(
                    "skills.root.invalid",
                    "error",
                    str(root),
                    "Skills root does not exist or is not a directory.",
                )
                diagnostics.append(diagnostic)
                with self._lock:
                    self._diagnostics = tuple(diagnostics)
                raise SkillError(
                    f"Skills root {root} does not exist or is not a directory."
                )
            try:
                self._scan_directory(
                    root,
                    root,
                    0,
                    state,
                    visited,
                    skills_by_slug,
                    skills_by_name,
                    diagnostics,
                )
            except OSError as exc:
                error = SkillError(f"Skill scan cannot inspect {root}: {exc}")
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.scan.filesystem_changed",
                        "error",
                        str(root),
                        str(error),
                    )
                )
                with self._lock:
                    self._diagnostics = tuple(diagnostics)
                raise error from exc
            except SkillError as exc:
                if not diagnostics or diagnostics[-1].message != str(exc):
                    diagnostics.append(
                        SkillDiagnostic(
                            "skills.scan.failed",
                            "error",
                            str(root),
                            str(exc),
                        )
                    )
                with self._lock:
                    self._diagnostics = tuple(diagnostics)
                raise

        with self._lock:
            self._skills_by_slug = skills_by_slug
            self._skills_by_name = skills_by_name
            self._diagnostics = tuple(diagnostics)

    def get(self, slug: str) -> Skill:
        with self._lock:
            try:
                return self._skills_by_slug[slug]
            except KeyError as exc:
                raise SkillError(f"Unknown skill '{slug}'") from exc

    def get_skill(self, name: str) -> Skill:
        with self._lock:
            try:
                return self._skills_by_name[name]
            except KeyError as exc:
                raise KeyError(name) from exc

    def read_resource(self, name: str, resource: str) -> str:
        skill = self.get_skill(name)
        try:
            data = skill.open_bytes(resource)
        except SkillError as exc:
            raise PermissionError(str(exc)) from exc
        return _normalize_text(data.decode("utf-8", errors="replace"))

    def _scan_directory(
        self,
        directory: Path,
        root: Path,
        depth: int,
        state: _ScanState,
        visited: set[str],
        skills_by_slug: dict[str, Skill],
        skills_by_name: dict[str, Skill],
        diagnostics: list[SkillDiagnostic],
    ) -> None:
        if depth > MAX_SKILL_SCAN_DEPTH:
            self._raise_scan_limit(
                diagnostics,
                directory,
                "skills.scan.depth_limit",
                f"Skill scan depth exceeds {MAX_SKILL_SCAN_DEPTH}.",
            )
        resolved_directory = directory.resolve(strict=True)
        if not _path_is_relative_to(resolved_directory, root):
            diagnostics.append(
                SkillDiagnostic(
                    "skills.scan.root_escape",
                    "error",
                    str(directory),
                    "Directory resolves outside the configured skills root and was skipped.",
                )
            )
            return
        identity = _normalized_path_key(resolved_directory)
        if identity in visited:
            diagnostics.append(
                SkillDiagnostic(
                    "skills.scan.directory_loop",
                    "warning",
                    str(directory),
                    "Directory was already visited and was skipped to prevent a loop.",
                )
            )
            return
        visited.add(identity)
        state.directories += 1
        if state.directories > MAX_SKILL_SCAN_DIRECTORIES:
            self._raise_scan_limit(
                diagnostics,
                directory,
                "skills.scan.directory_limit",
                f"Skill scan exceeds {MAX_SKILL_SCAN_DIRECTORIES} directories.",
            )

        try:
            entries = sorted(directory.iterdir(), key=_path_sort_key)
        except (OSError, PermissionError) as exc:
            diagnostics.append(
                SkillDiagnostic(
                    "skills.scan.directory_unreadable",
                    "error",
                    str(directory),
                    f"Directory could not be read: {exc}",
                )
            )
            raise SkillError(f"Skill scan cannot read {directory}: {exc}") from exc

        files: list[Path] = []
        directories: list[Path] = []
        for entry in entries:
            if _is_link_or_junction(entry, diagnostics):
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.scan.link_skipped",
                        "warning",
                        str(entry),
                        "Symbolic link or junction was skipped.",
                    )
                )
                continue
            try:
                if entry.is_dir():
                    directories.append(entry)
                elif entry.is_file():
                    files.append(entry)
                    state.files += 1
                    if state.files > MAX_SKILL_SCAN_FILES:
                        self._raise_scan_limit(
                            diagnostics,
                            entry,
                            "skills.scan.file_limit",
                            f"Skill scan exceeds {MAX_SKILL_SCAN_FILES} files.",
                        )
            except OSError as exc:
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.scan.entry_unreadable",
                        "error",
                        str(entry),
                        f"Directory entry could not be inspected: {exc}",
                    )
                )
                raise SkillError(f"Skill scan cannot inspect {entry}: {exc}") from exc

        skill_md = self._select_skill_markdown(directory, files, diagnostics)
        if skill_md is not None:
            self._register_dir_skill(
                directory,
                skill_md,
                root,
                state,
                visited,
                len(files),
                depth,
                skills_by_slug,
                skills_by_name,
                diagnostics,
            )
            return

        for child in directories:
            self._scan_directory(
                child,
                root,
                depth + 1,
                state,
                visited,
                skills_by_slug,
                skills_by_name,
                diagnostics,
            )

        for entry in files:
            if entry.suffix.lower() not in (".zip", ".skill"):
                continue
            if self.validation_mode == SKILL_VALIDATION_STRICT:
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.archive.strict_unsupported",
                        "warning",
                        str(entry),
                        "Archive skills are a compatible-mode extension and were skipped.",
                    )
                )
                continue
            self._try_register_zip_skill(
                entry,
                root,
                skills_by_slug,
                skills_by_name,
                diagnostics,
            )

    def _select_skill_markdown(
        self,
        directory: Path,
        files: list[Path],
        diagnostics: list[SkillDiagnostic],
    ) -> Path | None:
        candidates = [
            path
            for path in files
            if path.name.lower() == SKILL_MARKDOWN_LOWER
        ]
        if not candidates:
            return None
        exact = [path for path in candidates if path.name == SKILL_MARKDOWN]
        if len(candidates) > 1:
            diagnostics.append(
                SkillDiagnostic(
                    "skills.instructions.ambiguous",
                    "error",
                    str(directory),
                    "Skill directory contains multiple case variants of SKILL.md.",
                )
            )
            return None
        if exact:
            return exact[0]
        if self.validation_mode == SKILL_VALIDATION_STRICT:
            diagnostics.append(
                SkillDiagnostic(
                    "skills.instructions.noncanonical_name",
                    "error",
                    str(candidates[0]),
                    "Strict mode requires the canonical file name SKILL.md.",
                )
            )
            return None
        diagnostics.append(
            SkillDiagnostic(
                "skills.compat.instructions_lowercase",
                "warning",
                str(candidates[0]),
                "Accepted a noncanonical case variant of SKILL.md in compatible mode.",
            )
        )
        return candidates[0]

    def _register_dir_skill(
        self,
        directory: Path,
        skill_md: Path,
        root: Path,
        state: _ScanState,
        visited: set[str],
        counted_files: int,
        depth: int,
        skills_by_slug: dict[str, Skill],
        skills_by_name: dict[str, Skill],
        diagnostics: list[SkillDiagnostic],
    ) -> None:
        source = str(skill_md)
        before_diagnostics = len(diagnostics)
        try:
            raw_bytes, instructions_identity = _read_discovered_file(
                skill_md,
                root,
            )
            if len(raw_bytes) > MAX_FILE_READ_BYTES:
                raise SkillValidationError(
                    f"Instructions exceed {MAX_FILE_READ_BYTES} bytes."
                )
            metadata, body_text, conformance = _parse_skill_document(
                raw_bytes.decode("utf-8"),
                source=source,
                container_name=directory.name,
                validation_mode=self.validation_mode,
                diagnostics=diagnostics,
            )
            if skill_md.name != SKILL_MARKDOWN:
                conformance = SKILL_VALIDATION_COMPATIBLE
            resources = self._collect_resources(
                directory,
                diagnostics,
                state=state,
                visited=visited,
                counted_files=counted_files,
                depth=depth,
            )
        except (OSError, UnicodeDecodeError, SkillValidationError) as exc:
            del diagnostics[before_diagnostics:]
            if isinstance(exc, _SkillMetadataKeyCollision):
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.metadata.key_collision",
                        "error",
                        source,
                        str(exc),
                    )
                )
            diagnostics.append(
                SkillDiagnostic(
                    "skills.skill.invalid",
                    "error",
                    source,
                    str(exc),
                )
            )
            return

        skill = Skill(
            slug=slugify(metadata.name),
            root=root,
            directory=directory.resolve(),
            instructions_path=skill_md.resolve(),
            metadata=metadata,
            resources=resources,
            body_text=body_text,
            conformance=conformance,
            _discovered_instructions_identity=instructions_identity,
        )
        self._register_skill(
            skill,
            skills_by_slug,
            skills_by_name,
            diagnostics,
        )

    def _try_register_zip_skill(
        self,
        zip_path: Path,
        root: Path,
        skills_by_slug: dict[str, Skill],
        skills_by_name: dict[str, Skill],
        diagnostics: list[SkillDiagnostic],
    ) -> None:
        source = str(zip_path)
        before_diagnostics = len(diagnostics)
        try:
            archive_handle, archive_identity = _open_discovered_file(zip_path, root)
            with archive_handle:
                with _open_skill_archive(archive_handle) as archive:
                    inventory = _validate_archive_inventory_entries(
                        tuple(archive.infolist())
                    )
                    discovery_members = [
                        info.filename
                        for info in inventory
                        if not info.is_dir()
                        and not _is_ignored_zip_member(info.filename)
                    ]
                    skill_md_path, root_prefix = _select_archive_instructions(
                        discovery_members,
                        source,
                        diagnostics,
                    )
                    info = next(
                        item
                        for item in inventory
                        if item.filename == skill_md_path
                    )
                    if info.file_size > MAX_FILE_READ_BYTES:
                        raise SkillValidationError(
                            f"Instructions exceed {MAX_FILE_READ_BYTES} bytes."
                        )
                    raw_bytes = _read_archive_info(archive, info)
                _verify_discovered_file(
                    archive_handle,
                    zip_path,
                    root,
                    archive_identity,
                )
            if len(raw_bytes) > MAX_FILE_READ_BYTES:
                raise SkillValidationError(
                    f"Instructions exceed {MAX_FILE_READ_BYTES} bytes."
                )
            raw = _normalize_text(raw_bytes.decode("utf-8"))
            container_name = root_prefix.rstrip("/") or zip_path.stem
            metadata, body_text, conformance = _parse_skill_document(
                raw,
                source=f"{zip_path}!/{skill_md_path}",
                container_name=container_name,
                validation_mode=SKILL_VALIDATION_COMPATIBLE,
                diagnostics=diagnostics,
                conformance_override=SKILL_VALIDATION_COMPATIBLE,
            )
        except (
            KeyError,
            OSError,
            UnicodeDecodeError,
            zipfile.BadZipFile,
            SkillValidationError,
        ) as exc:
            del diagnostics[before_diagnostics:]
            if isinstance(exc, _SkillMetadataKeyCollision):
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.metadata.key_collision",
                        "error",
                        source,
                        str(exc),
                    )
                )
            diagnostics.append(
                SkillDiagnostic(
                    "skills.archive.invalid",
                    "error",
                    source,
                    str(exc),
                )
            )
            return

        diagnostics.append(
            SkillDiagnostic(
                "skills.compat.archive",
                "warning",
                source,
                "Accepted an archive skill in compatible mode; resources are "
                "exposed only through MCP.",
            )
        )
        skill = Skill(
            slug=slugify(metadata.name),
            root=root,
            directory=zip_path.parent.resolve(),
            instructions_path=zip_path.resolve(),
            metadata=metadata,
            resources=(),
            body_text=body_text,
            zip_path=zip_path.resolve(),
            zip_root_prefix=root_prefix,
            instructions_member=skill_md_path[len(root_prefix):],
            conformance=conformance,
            _discovered_zip_identity=archive_identity,
        )
        self._register_skill(
            skill,
            skills_by_slug,
            skills_by_name,
            diagnostics,
        )

    def _register_skill(
        self,
        skill: Skill,
        skills_by_slug: dict[str, Skill],
        skills_by_name: dict[str, Skill],
        diagnostics: list[SkillDiagnostic],
    ) -> None:
        source = _skill_source(skill)
        if skill.slug in _reserved_skill_slugs():
            diagnostics.append(
                SkillDiagnostic(
                    "skills.skill.reserved_name",
                    "error",
                    source,
                    f"Skill slug '{skill.slug}' conflicts with a reserved MCP tool name.",
                )
            )
            return
        if skill.slug in skills_by_slug:
            diagnostics.append(
                SkillDiagnostic(
                    "skills.skill.slug_collision",
                    "warning",
                    source,
                    f"Skill slug '{skill.slug}' was already registered; the first "
                    "configured root wins.",
                )
            )
            return
        if skill.metadata.name in skills_by_name:
            diagnostics.append(
                SkillDiagnostic(
                    "skills.skill.name_collision",
                    "warning",
                    source,
                    f"Skill name '{skill.metadata.name}' was already registered; "
                    "the first configured root wins.",
                )
            )
            return
        skills_by_slug[skill.slug] = skill
        skills_by_name[skill.metadata.name] = skill

    def _collect_resources(
        self,
        directory: Path,
        diagnostics: list[SkillDiagnostic],
        *,
        state: _ScanState | None = None,
        visited: set[str] | None = None,
        counted_files: int = 0,
        depth: int = 0,
    ) -> tuple[Path, ...]:
        resource_state = None
        resource_visited: set[str] | None = None
        if state is not None:
            resource_state = _ScanState(
                roots=state.roots,
                directories=max(0, state.directories - 1),
                files=max(0, state.files - counted_files),
            )
            resource_visited = set()
        resources: list[Path] = []
        for path in _bounded_tree_files(
            directory,
            diagnostics,
            state=resource_state,
            visited=resource_visited,
            initial_depth=depth,
        ):
            if path.name.lower() == SKILL_MARKDOWN_LOWER:
                continue
            try:
                size = path.stat().st_size
            except OSError as exc:
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.resource.unreadable",
                        "error",
                        str(path),
                        f"Resource could not be inspected during the scan: {exc}",
                    )
                )
                raise SkillError(
                    f"Skill resource {path} changed while it was being collected: {exc}"
                ) from exc
            if size > MAX_FILE_READ_BYTES:
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.resource.too_large",
                        "warning",
                        str(path),
                        f"Resource exceeds {MAX_FILE_READ_BYTES} bytes and cannot be "
                        "read through MCP.",
                    )
                )
            resources.append(path)
        if state is not None and resource_state is not None:
            state.directories = resource_state.directories
            state.files = resource_state.files
            if visited is not None and resource_visited is not None:
                visited.update(resource_visited)
        return tuple(resources)

    @staticmethod
    def _raise_scan_limit(
        diagnostics: list[SkillDiagnostic],
        source: Path,
        code: str,
        message: str,
    ) -> None:
        diagnostics.append(SkillDiagnostic(code, "error", str(source), message))
        raise SkillError(message)


class SkillRepository(SkillRegistry):
    """Compatibility wrapper kept for existing QCopilots tests."""


@dataclass(frozen=True, slots=True)
class _SkillSnapshot:
    roots: tuple[Path, ...]
    registry: SkillRegistry
    fingerprint: tuple[tuple[Any, ...], ...]
    tools: tuple[McpTool, ...]
    resources: tuple[McpResource, ...]
    prompts: tuple[McpPrompt, ...]


class SkillService:
    """Hot-reloading Skills runtime used by QCopilots MCP services."""

    def __init__(
        self,
        roots: Iterable[str | Path] | Callable[[], Iterable[str | Path]],
        validation_mode: str = SKILL_VALIDATION_STRICT,
        refresh_interval_seconds: float = DEFAULT_SKILL_REFRESH_INTERVAL_SECONDS,
        clock: Callable[[], float] = time.monotonic,
    ):
        self._lock = threading.RLock()
        self._roots_source = roots
        self.validation_mode = _validate_validation_mode(validation_mode)
        self.refresh_interval_seconds = _validate_nonnegative_finite_number(
            refresh_interval_seconds,
            "refresh_interval_seconds",
        )
        if not callable(clock):
            raise ValueError("clock must be callable")
        self._clock = clock
        initial_time = _skill_clock_value(self._clock)
        snapshot = _build_stable_skill_snapshot(
            self._resolve_roots(),
            self.validation_mode,
        )
        self.roots = snapshot.roots
        self._registry = snapshot.registry
        self._tools = list(snapshot.tools)
        self._resources = list(snapshot.resources)
        self._prompts = list(snapshot.prompts)
        self._fingerprint = snapshot.fingerprint
        self._failed_fingerprint: tuple[tuple[Any, ...], ...] | None = None
        self._reload_diagnostics: tuple[SkillDiagnostic, ...] = ()
        self._refresh_in_progress = False
        self._generation = 0
        self._next_refresh_check = initial_time

    @property
    def registry(self) -> SkillRegistry:
        self.ensure_current()
        with self._lock:
            return self._registry

    @property
    def diagnostics(self) -> tuple[SkillDiagnostic, ...]:
        self.ensure_current()
        with self._lock:
            return self._registry.diagnostics + self._reload_diagnostics

    def tools(self) -> list[McpTool]:
        self.ensure_current()
        with self._lock:
            return list(self._tools)

    def resources(self) -> list[McpResource]:
        self.ensure_current()
        with self._lock:
            return list(self._resources)

    def prompts(self) -> list[McpPrompt]:
        self.ensure_current()
        with self._lock:
            return list(self._prompts)

    def ensure_current(self) -> None:
        now = _skill_clock_value(self._clock)
        with self._lock:
            if self._refresh_in_progress or now < self._next_refresh_check:
                return
            self._refresh_in_progress = True
            self._next_refresh_check = now + self.refresh_interval_seconds
            generation = self._generation
            current_roots = self.roots
            current_registry = self._registry
            current_fingerprint = self._fingerprint
            failed_fingerprint = self._failed_fingerprint

        try:
            try:
                requested_roots = self._resolve_roots()
                fingerprint = _build_roots_fingerprint(
                    requested_roots,
                    validation_mode=self.validation_mode,
                )
            except (OSError, SkillError) as exc:
                failed = (("__reload_error__", str(exc)),)
                self._record_reload_failure(
                    failed,
                    exc,
                    generation=generation,
                    attempted_roots=current_roots,
                )
                return

            if (
                requested_roots == current_roots
                and fingerprint == current_fingerprint
                and failed_fingerprint is None
            ):
                return
            if fingerprint == failed_fingerprint:
                return

            candidate_diagnostics: list[SkillDiagnostic] = []
            try:
                snapshot = _build_stable_skill_snapshot(
                    requested_roots,
                    self.validation_mode,
                    current_registry=(
                        current_registry
                        if requested_roots == current_roots
                        else None
                    ),
                    first_fingerprint=fingerprint,
                    failure_diagnostics=candidate_diagnostics,
                )
            except SkillError as exc:
                self._record_reload_failure(
                    fingerprint,
                    exc,
                    tuple(candidate_diagnostics),
                    generation=generation,
                    attempted_roots=requested_roots,
                )
                return
            self._commit_snapshot(snapshot, generation)
        finally:
            with self._lock:
                self._refresh_in_progress = False
                try:
                    finished_at = _skill_clock_value(self._clock)
                except Exception:
                    # 完成时钟失败时保留入口 deadline；后续请求可以重试，且不会
                    # 覆盖原刷新异常或让 single-flight 闸门保持锁定。
                    pass
                else:
                    self._next_refresh_check = max(
                        self._next_refresh_check,
                        finished_at + self.refresh_interval_seconds,
                    )

    def _commit_snapshot(
        self,
        snapshot: _SkillSnapshot,
        generation: int,
    ) -> None:
        with self._lock:
            if generation != self._generation:
                return
            self.roots = snapshot.roots
            self._registry = snapshot.registry
            self._tools = list(snapshot.tools)
            self._resources = list(snapshot.resources)
            self._prompts = list(snapshot.prompts)
            self._fingerprint = snapshot.fingerprint
            self._failed_fingerprint = None
            self._reload_diagnostics = ()
            self._generation += 1

    def _record_reload_failure(
        self,
        fingerprint: tuple[tuple[Any, ...], ...],
        error: Exception,
        candidate_diagnostics: tuple[SkillDiagnostic, ...] = (),
        *,
        generation: int,
        attempted_roots: tuple[Path, ...],
    ) -> None:
        with self._lock:
            if generation != self._generation:
                return
            registry = self._registry
        filesystem_diagnostics: tuple[SkillDiagnostic, ...] = ()
        if (
            isinstance(error, _SkillFilesystemChanged)
            and not any(
                diagnostic.code == error.diagnostic_code
                for diagnostic in candidate_diagnostics
            )
        ):
            filesystem_diagnostics = (
                SkillDiagnostic(
                    error.diagnostic_code,
                    "error",
                    error.source,
                    str(error),
                ),
            )
        reload_diagnostics = candidate_diagnostics + filesystem_diagnostics + (
            SkillDiagnostic(
                "skills.reload.failed",
                "error",
                ", ".join(str(root) for root in attempted_roots),
                "Skill reload failed; the last-known-good snapshot remains "
                f"active: {error}",
            ),
        )
        tools, resources, prompts = _build_runtime_components(
            registry,
            reload_diagnostics,
            use_cached_instructions=True,
            last_known_good=True,
        )
        with self._lock:
            if generation != self._generation:
                return
            self._failed_fingerprint = fingerprint
            self._reload_diagnostics = reload_diagnostics
            self._tools = list(tools)
            self._resources = list(resources)
            self._prompts = list(prompts)

    def _resolve_roots(self) -> tuple[Path, ...]:
        roots = (
            self._roots_source()
            if callable(self._roots_source)
            else self._roots_source
        )
        if isinstance(roots, (str, Path)):
            roots = (roots,)
        return tuple(
            _dedupe(
                Path(root).expanduser().resolve(strict=False)
                for root in roots
            )
        )


def slugify(value: str) -> str:
    cleaned = re.sub(r"[^a-zA-Z0-9]+", "-", value.strip().lower()).strip("-")
    return cleaned or "skill"


def configured_skill_roots(plugin_dir: str | Path | None = None) -> list[Path]:
    """Resolve the single configured skill root for this service."""

    roots: list[Path] = []
    plugin_path = Path(plugin_dir).resolve() if plugin_dir else None
    manifest_data = _read_service_manifest(plugin_path) if plugin_path else {}
    default_root = plugin_path / "Skills" if plugin_path else None

    configured = manifest_data.get("skillsRoot")
    if configured is None:
        if default_root is not None:
            roots.append(default_root.resolve(strict=False))
    else:
        roots.append(_resolve_config_path(configured, plugin_path))

    if not roots and default_root is not None:
        roots.append(default_root.resolve(strict=False))

    return _dedupe(roots)


def configured_skill_validation_mode(
    plugin_dir: str | Path | None = None,
) -> str:
    """Return the configured Agent Skills validation policy."""

    plugin_path = Path(plugin_dir).resolve() if plugin_dir else None
    manifest_data = _read_service_manifest(plugin_path) if plugin_path else {}
    skills_config = manifest_data.get("skills") or {}
    configured = (
        skills_config.get("validation_mode")
        if isinstance(skills_config, Mapping)
        else None
    )
    if configured is None:
        configured = manifest_data.get("skills_validation_mode")
    return _validate_validation_mode(
        configured if configured is not None else SKILL_VALIDATION_STRICT
    )


def build_skill_service(plugin_dir: str | Path | None = None) -> SkillService:
    return SkillService(
        lambda: configured_skill_roots(plugin_dir),
        validation_mode=configured_skill_validation_mode(plugin_dir),
    )


def build_skill_tools(plugin_dir: str | Path | None = None) -> list[McpTool]:
    return build_skill_service(plugin_dir).tools()


def build_skill_resources(plugin_dir: str | Path | None = None) -> list[McpResource]:
    return build_skill_service(plugin_dir).resources()


def discover_skills(plugin_dir: str | Path | None = None) -> list[dict[str, Any]]:
    service = build_skill_service(plugin_dir)
    return [_skill_summary_with_paths(skill) for skill in service.registry.skills]


def list_skills(arguments: dict[str, Any]) -> dict[str, Any]:
    del arguments
    service = build_skill_service()
    skills = [_skill_summary(skill) for skill in service.registry.skills]
    return {
        "count": len(skills),
        "skills": skills,
        "validation_mode": service.validation_mode,
        "diagnostics": _diagnostics_dicts(service.diagnostics),
    }


def read_skill(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        service = build_skill_service()
        skill = _resolve_skill(service.registry, arguments)
        resources = tuple(_build_skill_resources(skill))
        return {
            "name": skill.metadata.name,
            "slug": skill.slug,
            "conformance": skill.conformance,
            "source_kind": skill.source_kind,
            "skill_root": str(skill.directory) if not skill.is_zip else None,
            "instructions_path": _instructions_source_path(skill),
            "path": str(skill.instructions_path),
            "content": skill.read_body(),
            "skill_content": _format_skill_content(
                skill,
                task="",
                resources=resources,
            ),
            "metadata": _metadata_dict(skill),
            "resource_metadata": [
                _resource_metadata(resource) for resource in resources
            ],
        }
    except _SKILL_TOOL_FAILURES as exc:
        raise ToolError(_SKILL_UNAVAILABLE_MESSAGE) from exc


def list_skill_resources(arguments: dict[str, Any]) -> dict[str, Any]:
    try:
        service = build_skill_service()
        skill = _resolve_skill(service.registry, arguments)
        resources = [
            _resource_metadata(resource) for resource in _build_skill_resources(skill)
        ]
        return {
            "name": skill.metadata.name,
            "slug": skill.slug,
            "resources": [entry["name"].split("/", 1)[1] for entry in resources],
            "resource_metadata": resources,
        }
    except _SKILL_TOOL_FAILURES as exc:
        raise ToolError(_SKILL_UNAVAILABLE_MESSAGE) from exc


def read_skill_resource(arguments: dict[str, Any]) -> dict[str, Any]:
    resource = str(arguments.get("resource") or "")
    if not resource:
        raise ToolError("resource is required")
    try:
        service = build_skill_service()
        skill = _resolve_skill(service.registry, arguments)
        data = skill.open_bytes(resource)
    except _SKILL_TOOL_FAILURES as exc:
        raise ToolError(_SKILL_RESOURCE_UNAVAILABLE_MESSAGE) from exc
    try:
        content: str | bytes = data.decode("utf-8")
    except UnicodeDecodeError:
        return {
            "name": skill.metadata.name,
            "slug": skill.slug,
            "resource": resource,
            "blob": base64.b64encode(data).decode("ascii"),
            "encoding": "base64",
        }
    return {
        "name": skill.metadata.name,
        "slug": skill.slug,
        "resource": resource,
        "content": content,
        "encoding": "utf-8",
    }


def _validate_runtime_components(registry: SkillRegistry) -> None:
    for skill in registry.skills:
        if skill.slug in _reserved_skill_slugs():
            raise SkillError(
                f"Skill slug '{skill.slug}' conflicts with a management tool name."
            )
        tuple(_build_skill_resources(skill))


def _build_runtime_components(
    registry: SkillRegistry,
    reload_diagnostics: tuple[SkillDiagnostic, ...] = (),
    *,
    use_cached_instructions: bool = False,
    last_known_good: bool = False,
) -> tuple[tuple[McpTool, ...], tuple[McpResource, ...], tuple[McpPrompt, ...]]:
    resources_by_slug: dict[str, tuple[McpResource, ...]] = {}
    resources: list[McpResource] = []
    for skill in registry.skills:
        skill_resources = tuple(_build_skill_resources(skill))
        resources_by_slug[skill.slug] = skill_resources
        resources.extend(skill_resources)

    tools = [
        _list_skills_tool(
            registry,
            registry.diagnostics + reload_diagnostics,
        )
    ]
    tools.extend(
        _compatibility_tools(
            registry,
            use_cached_instructions=use_cached_instructions,
            last_known_good=last_known_good,
        )
    )
    prompts: list[McpPrompt] = []
    for skill in registry.skills:
        skill_resources = resources_by_slug[skill.slug]
        tools.append(
            _skill_tool(
                skill,
                skill_resources,
                use_cached_instructions=use_cached_instructions,
                last_known_good=last_known_good,
            )
        )
        prompts.append(
            _skill_prompt(
                skill,
                skill_resources,
                use_cached_instructions=use_cached_instructions,
                last_known_good=last_known_good,
            )
        )
    return tuple(tools), tuple(resources), tuple(prompts)


def _build_stable_skill_snapshot(
    roots: tuple[Path, ...],
    validation_mode: str,
    *,
    current_registry: SkillRegistry | None = None,
    first_fingerprint: tuple[tuple[Any, ...], ...] | None = None,
    failure_diagnostics: list[SkillDiagnostic] | None = None,
) -> _SkillSnapshot:
    source = ", ".join(str(root) for root in roots)
    for attempt in range(MAX_SKILL_SNAPSHOT_ATTEMPTS):
        before = (
            first_fingerprint
            if attempt == 0 and first_fingerprint is not None
            else _build_roots_fingerprint(
                roots,
                validation_mode=validation_mode,
            )
        )
        candidate = SkillRegistry(roots, validation_mode=validation_mode)
        try:
            candidate.load()
            if current_registry is not None:
                _validate_existing_skill_snapshot(current_registry, candidate)
            tools, resources, prompts = _build_runtime_components(
                candidate,
                use_cached_instructions=True,
            )
            after = _build_roots_fingerprint(
                roots,
                validation_mode=validation_mode,
            )
            _validate_candidate_snapshot_sources(candidate)
        except _SkillFilesystemChanged:
            if failure_diagnostics is not None:
                failure_diagnostics[:] = candidate.diagnostics
            continue
        except SkillError:
            if failure_diagnostics is not None:
                failure_diagnostics[:] = candidate.diagnostics
            raise
        if before == after:
            if failure_diagnostics is not None:
                failure_diagnostics.clear()
            return _SkillSnapshot(
                roots=roots,
                registry=candidate,
                fingerprint=after,
                tools=tools,
                resources=resources,
                prompts=prompts,
            )
        if failure_diagnostics is not None:
            failure_diagnostics[:] = candidate.diagnostics
    raise _SkillFilesystemChanged(
        source,
        "Skills changed while a stable snapshot was being built.",
    )


def _validate_candidate_snapshot_sources(registry: SkillRegistry) -> None:
    for skill in registry.skills:
        try:
            skill._validate_discovered_sources()
        except (OSError, SkillError) as exc:
            raise _SkillFilesystemChanged(
                _skill_source(skill),
                "Skill source changed while a stable snapshot was being built.",
            ) from exc


def _validate_existing_skill_snapshot(
    current: SkillRegistry,
    candidate: SkillRegistry,
) -> None:
    candidates_by_slug: dict[str, Skill] = {}
    candidates_by_source: dict[str, Skill] = {}
    for candidate_skill in candidate.skills:
        if candidate_skill.slug in candidates_by_slug:
            raise SkillError(
                f"Candidate skill slug '{candidate_skill.slug}' is not unique; "
                "keeping previous snapshot."
            )
        candidates_by_slug[candidate_skill.slug] = candidate_skill

        source_key = _normalized_path_key(candidate_skill.instructions_path)
        previous = candidates_by_source.get(source_key)
        if previous is not None:
            raise SkillError(
                f"Candidate skills '{previous.slug}' and '{candidate_skill.slug}' "
                f"share instructions source '{_skill_source(candidate_skill)}'; "
                "keeping previous snapshot."
            )
        candidates_by_source[source_key] = candidate_skill

    for skill in current.skills:
        source_key = _normalized_path_key(skill.instructions_path)
        slug_replacement = candidates_by_slug.get(skill.slug)
        if (
            slug_replacement is not None
            and _normalized_path_key(slug_replacement.instructions_path) == source_key
        ):
            continue
        source_replacement = candidates_by_source.get(source_key)
        if source_replacement is not None:
            if slug_replacement is not None:
                raise SkillError(
                    f"Existing skill '{skill.slug}' was renamed to "
                    f"'{source_replacement.slug}' at the same source while its old "
                    "slug was claimed by a different source; keeping previous snapshot."
                )
            continue
        try:
            skill.instructions_path.stat()
        except FileNotFoundError:
            continue
        except OSError as exc:
            raise _SkillFilesystemChanged(
                str(skill.instructions_path),
                "Existing skill source could not be inspected; keeping previous snapshot.",
            ) from exc
        raise SkillError(
            f"Existing skill '{skill.slug}' became invalid; keeping previous snapshot."
        )


def _list_skills_tool(
    registry: SkillRegistry,
    diagnostics: tuple[SkillDiagnostic, ...] | None = None,
) -> McpTool:
    def _handler(arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        return {
            "count": len(registry.skills),
            "skills": [_skill_summary(skill) for skill in registry.skills],
            "validation_mode": registry.validation_mode,
            "diagnostics": _diagnostics_dicts(
                diagnostics if diagnostics is not None else registry.diagnostics
            ),
        }

    return McpTool(
        "list_skills",
        "List skills currently available using catalog metadata only: slug, name, "
        "description, and conformance. Use this stable management tool after skill "
        "files or configuration change to refresh the in-session catalog, then use "
        "read_skill with a current slug.",
        {"type": "object", "properties": {}, "additionalProperties": False},
        _handler,
    )


def _compatibility_tools(
    registry: SkillRegistry,
    *,
    use_cached_instructions: bool = False,
    last_known_good: bool = False,
) -> list[McpTool]:
    catalog_note = _format_selector_catalog_note(registry)
    return [
        McpTool(
            "read_skill",
            "Read a skill's instructions and metadata. After refreshing with "
            "list_skills, use a current or newly discovered slug. Scripts remain "
            "resources: run them from skill_root only when a user or host explicitly "
            "approves the execution tool. allowed-tools does not automatically grant "
            f"permission.{catalog_note}",
            _read_skill_schema(),
            lambda arguments: _read_skill_from_registry(
                registry,
                arguments,
                use_cached_instructions=use_cached_instructions,
                last_known_good=last_known_good,
            ),
        ),
        McpTool(
            "list_skill_resources",
            f"List resources exposed by a skill.{catalog_note}",
            _read_skill_schema(),
            lambda arguments: _list_resources_from_registry(registry, arguments),
        ),
        McpTool(
            "read_skill_resource",
            f"Read a resource from a skill package.{catalog_note}",
            _resource_read_schema(),
            lambda arguments: _read_resource_from_registry(registry, arguments),
        ),
    ]


def _skill_tool(
    skill: Skill,
    resources: tuple[McpResource, ...],
    *,
    use_cached_instructions: bool = False,
    last_known_good: bool = False,
) -> McpTool:
    if skill.slug in _reserved_skill_slugs():
        raise SkillError(
            f"Skill slug '{skill.slug}' conflicts with a management tool name."
        )

    def _handler(arguments: dict[str, Any]) -> dict[str, Any]:
        task = str(arguments.get("task") or "")
        if not task.strip():
            raise ToolError("The 'task' parameter must be a non-empty string.")
        try:
            return _skill_response(
                skill,
                task=task,
                resources=resources,
                use_cached_instructions=use_cached_instructions,
                last_known_good=last_known_good,
            )
        except _SKILL_TOOL_FAILURES as exc:
            raise ToolError(_DYNAMIC_SKILL_UNAVAILABLE_MESSAGE) from exc

    return McpTool(
        skill.slug,
        _format_tool_description(skill),
        {
            "type": "object",
            "properties": {
                "task": {
                    "type": "string",
                    "description": "Specific task the client wants to complete with this skill.",
                }
            },
            "required": ["task"],
            "additionalProperties": False,
        },
        _handler,
    )


def _skill_prompt(
    skill: Skill,
    resources: tuple[McpResource, ...],
    *,
    use_cached_instructions: bool = False,
    last_known_good: bool = False,
) -> McpPrompt:
    if skill.slug in _reserved_skill_slugs():
        raise SkillError(
            f"Skill slug '{skill.slug}' conflicts with a management prompt name."
        )

    def _handler(arguments: dict[str, Any]) -> dict[str, Any]:
        task = str(arguments.get("task") or "")
        if not task.strip():
            raise ToolError("The 'task' parameter must be a non-empty string.")
        try:
            return _skill_prompt_response(
                skill,
                task=task,
                resources=resources,
                use_cached_instructions=use_cached_instructions,
                last_known_good=last_known_good,
            )
        except _SKILL_TOOL_FAILURES as exc:
            raise ToolError(_DYNAMIC_SKILL_UNAVAILABLE_MESSAGE) from exc

    return McpPrompt(
        skill.slug,
        _format_prompt_description(skill),
        [
            {
                "name": "task",
                "description": "Specific task the client wants to complete with this skill.",
                "required": True,
            }
        ],
        _handler,
        title=f"Use {skill.metadata.name}",
    )


def _read_skill_from_registry(
    registry: SkillRegistry,
    arguments: dict[str, Any],
    *,
    use_cached_instructions: bool = False,
    last_known_good: bool = False,
) -> dict[str, Any]:
    try:
        skill = _resolve_skill(registry, arguments)
        resources = tuple(_build_skill_resources(skill))
        return {
            "name": skill.metadata.name,
            "slug": skill.slug,
            "conformance": skill.conformance,
            "source_kind": skill.source_kind,
            "skill_root": str(skill.directory) if not skill.is_zip else None,
            "instructions_path": _instructions_source_path(skill),
            "instructions_file": skill.instructions_member,
            "content": skill.read_body(
                allow_cached_instructions=use_cached_instructions
            ),
            "skill_content": _format_skill_content(
                skill,
                task="",
                resources=resources,
                use_cached_instructions=use_cached_instructions,
                last_known_good=last_known_good,
            ),
            "metadata": _metadata_dict(skill),
            "resource_metadata": [
                _resource_metadata(resource) for resource in resources
            ],
        }
    except _SKILL_TOOL_FAILURES as exc:
        raise ToolError(_SKILL_UNAVAILABLE_MESSAGE) from exc


def _list_resources_from_registry(
    registry: SkillRegistry,
    arguments: dict[str, Any],
) -> dict[str, Any]:
    try:
        skill = _resolve_skill(registry, arguments)
        resources = [
            _resource_metadata(resource) for resource in _build_skill_resources(skill)
        ]
        return {
            "name": skill.metadata.name,
            "slug": skill.slug,
            "resources": [entry["name"].split("/", 1)[1] for entry in resources],
            "resource_metadata": resources,
        }
    except _SKILL_TOOL_FAILURES as exc:
        raise ToolError(_SKILL_UNAVAILABLE_MESSAGE) from exc


def _read_resource_from_registry(
    registry: SkillRegistry,
    arguments: dict[str, Any],
) -> dict[str, Any]:
    resource = str(arguments.get("resource") or "")
    if not resource:
        raise ToolError("resource is required")
    try:
        _validate_relative_resource_path(resource)
    except SkillError as exc:
        raise ToolError("Invalid resource path.") from exc
    try:
        skill = _resolve_skill(registry, arguments)
        data = skill.open_bytes(resource)
    except _SKILL_TOOL_FAILURES as exc:
        raise ToolError(_SKILL_RESOURCE_UNAVAILABLE_MESSAGE) from exc
    try:
        content = _normalize_text(data.decode("utf-8"))
    except UnicodeDecodeError:
        return {
            "name": skill.metadata.name,
            "slug": skill.slug,
            "resource": resource,
            "blob": base64.b64encode(data).decode("ascii"),
            "encoding": "base64",
        }
    return {
        "name": skill.metadata.name,
        "slug": skill.slug,
        "resource": resource,
        "content": content,
        "encoding": "utf-8",
    }


def _resolve_skill(
    registry: SkillRegistry,
    arguments: dict[str, Any],
) -> Skill:
    slug = str(arguments.get("slug") or "").strip()
    if slug:
        try:
            return registry.get(slug)
        except (KeyError, SkillError) as exc:
            raise SkillError("The requested skill was not found.") from exc
    name = str(arguments.get("name") or "").strip()
    if name:
        try:
            return registry.get_skill(name)
        except (KeyError, SkillError) as exc:
            raise SkillError("The requested skill was not found.") from exc
    path = str(arguments.get("path") or "").strip()
    if path:
        candidate = Path(path).expanduser().resolve(strict=False)
        for skill in registry.skills:
            if skill.is_zip and candidate == skill.zip_path:
                return skill
            if not skill.is_zip and candidate == skill.directory:
                return skill
        raise SkillError("The requested skill was not found.")
    raise ToolError("name, slug or path is required")


def _skill_response(
    skill: Skill,
    *,
    task: str,
    resources: tuple[McpResource, ...],
    use_cached_instructions: bool = False,
    last_known_good: bool = False,
) -> dict[str, Any]:
    usage = (
        "HOW TO USE THIS SKILL:\n\n"
        "1. Read the instructions carefully.\n"
        "2. Use the task field as the immediate request context.\n"
        "3. Retrieve listed MCP resources when the instructions reference additional files.\n"
        "4. Treat metadata.allowed_tools as the skill author's declaration, not permission.\n"
        "5. Run scripts only from a directory skill root when the host exposes an appropriate "
        "execution tool, such as exec_shell_command, and the user or host permission policy "
        "approves it. Use skill_root as the working directory. Archive resources are read-only "
        "MCP resources and are not executable directories.\n"
        "6. Apply the skill guidance with your own judgment."
    )
    if last_known_good:
        usage += (
            "\n7. These are cached last-known-good instructions. Listed resources remain "
            "bound to their discovered identity and may be unavailable until reload succeeds."
        )
    return {
        "skill": skill.slug,
        "task": task,
        "conformance": skill.conformance,
        "source_kind": skill.source_kind,
        "skill_root": str(skill.directory) if not skill.is_zip else None,
        "instructions_path": _instructions_source_path(skill),
        "metadata": _metadata_dict(skill),
        "resources": [_resource_metadata(resource) for resource in resources],
        "instructions": skill.read_body(
            allow_cached_instructions=use_cached_instructions
        ),
        "skill_content": _format_skill_content(
            skill,
            task=task,
            resources=resources,
            use_cached_instructions=use_cached_instructions,
            last_known_good=last_known_good,
        ),
        "usage": usage,
    }


def _skill_prompt_response(
    skill: Skill,
    *,
    task: str,
    resources: tuple[McpResource, ...],
    use_cached_instructions: bool = False,
    last_known_good: bool = False,
) -> dict[str, Any]:
    return {
        "description": f"Use the {skill.metadata.name} Agent Skill.",
        "messages": [
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": _format_skill_content(
                        skill,
                        task=task,
                        resources=resources,
                        use_cached_instructions=use_cached_instructions,
                        last_known_good=last_known_good,
                    ),
                },
            }
        ],
    }


def _bridge_metadata_payload(
    metadata: SkillMetadata,
    conformance: str,
) -> dict[str, Any]:
    return {
        "name": metadata.name,
        "description": metadata.description,
        "license": metadata.license,
        "compatibility": metadata.compatibility,
        "allowed_tools": list(metadata.allowed_tools),
        "allowed_tools_declaration": metadata.allowed_tools_declaration,
        "metadata": metadata.metadata,
        "extra": metadata.extra,
        "conformance": conformance,
    }


def _validate_bridge_metadata_budget(
    metadata: SkillMetadata,
    conformance: str,
    source: str,
) -> None:
    # 转换阶段已限制原始标量字节和生成节点；这里的 bridge payload 有界，
    # 只做一次包含 JSON 键名、标点和重复投影的精确 UTF-8 字节核验。
    try:
        serialized = json.dumps(
            _bridge_metadata_payload(metadata, conformance),
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        ).encode("utf-8")
    except (TypeError, ValueError, OverflowError, UnicodeError, RecursionError) as exc:
        raise SkillValidationError(
            f"Front matter in {source} could not be serialized as a JSON-safe "
            "metadata document."
        ) from exc
    if len(serialized) > MAX_JSON_SAFE_OUTPUT_BYTES:
        raise SkillValidationError(
            f"Front matter in {source} serialized metadata document exceeds the "
            "JSON-safe output byte budget."
        )


def _metadata_dict(skill: Skill) -> dict[str, Any]:
    return _bridge_metadata_payload(skill.metadata, skill.conformance)


def _skill_summary(skill: Skill) -> dict[str, str]:
    return {
        "slug": skill.slug,
        "name": skill.metadata.name,
        "description": skill.metadata.description,
        "conformance": skill.conformance,
    }


def _skill_summary_with_paths(skill: Skill) -> dict[str, str]:
    return {
        **_skill_summary(skill),
        "path": str(skill.directory),
        "root": str(skill.root),
    }


def _format_tool_description(skill: Skill) -> str:
    return (
        f"[SKILL] {skill.metadata.description} - Invoke this to receive "
        "specialized instructions and resources for this task."
    )


def _format_prompt_description(skill: Skill) -> str:
    return (
        f"Activate the {skill.metadata.name} Agent Skill for a task. "
        f"{skill.metadata.description}"
    )


def _format_selector_catalog_note(registry: SkillRegistry) -> str:
    skills = registry.skills
    if not skills:
        return ""
    slugs = [skill.slug for skill in skills]
    shown_slugs: list[str] = []
    for slug in slugs[:MAX_SKILL_SELECTOR_CATALOG_ITEMS]:
        candidate = shown_slugs + [slug]
        note = _format_selector_catalog_note_text(candidate, len(slugs))
        if len(note) > MAX_SKILL_SELECTOR_CATALOG_CHARS:
            break
        shown_slugs = candidate
    if not shown_slugs:
        first_slug = slugs[0]
        shown_slugs = [_truncate_selector_catalog_slug(first_slug, len(slugs))]
    return _format_selector_catalog_note_text(shown_slugs, len(slugs))


def _truncate_selector_catalog_slug(slug: str, total: int) -> str:
    available = max(
        0,
        MAX_SKILL_SELECTOR_CATALOG_CHARS
        - len(_format_selector_catalog_note_text([""], total)),
    )
    if len(slug) <= available:
        return slug
    if available <= 3:
        return slug[:available]
    return slug[: available - 3].rstrip("-") + "..."


def _format_selector_catalog_note_text(slugs: list[str], total: int) -> str:
    visible = len(slugs)
    note = (
        f" Current skill slugs, first {visible} of {total}: "
        f"{', '.join(slugs)}."
    )
    if visible < total:
        note += " Call list_skills for the full catalog."
    return note


def _format_xml_cdata(value: str) -> str:
    return f"<![CDATA[{value.replace(']]>', ']]]]><![CDATA[>')}]]>"


def _format_skill_content(
    skill: Skill,
    *,
    task: str,
    resources: tuple[McpResource, ...],
    use_cached_instructions: bool = False,
    last_known_good: bool = False,
) -> str:
    resource_lines = [
        _format_resource_reference(resource)
        for resource in resources
    ]
    if not resource_lines:
        resource_lines = ["  <none />"]
    task_block = (
        html.escape(task.strip()) or "No task was supplied with this activation."
    )
    allowed_tools = html.escape(
        ", ".join(skill.metadata.allowed_tools) or "not specified"
    )
    compatibility = html.escape(skill.metadata.compatibility or "not specified")
    license_name = html.escape(skill.metadata.license or "not specified")
    skill_name = html.escape(skill.metadata.name, quote=True)
    skill_slug = html.escape(skill.slug, quote=True)
    skill_description = html.escape(skill.metadata.description)
    conformance = html.escape(skill.conformance, quote=True)
    instructions = skill.read_body(
        allow_cached_instructions=use_cached_instructions
    )
    lkg_guidance = ""
    if last_known_good:
        lkg_guidance = (
            " These are cached last-known-good instructions. Listed resources "
            "remain identity-bound and may be unavailable until reload succeeds."
        )
    if skill.is_zip:
        root_guidance = (
            "This skill comes from an archive and has no local executable skill root. "
            "Retrieve its files on demand through the listed MCP resource URIs."
        )
        skill_root = ""
    else:
        skill_root = html.escape(str(skill.directory), quote=True)
        root_guidance = (
            f"The skill root is {skill_root}. Relative paths in the instructions are based "
            "on this directory. Scripts may run with this root as their working directory "
            "only when the host exposes an appropriate execution tool, such as "
            "exec_shell_command, and its permission policy approves the action."
        )
    return "\n".join(
        [
            (
                f'<skill_content name="{skill_name}" slug="{skill_slug}" '
                f'conformance="{conformance}" source_kind="{skill.source_kind}">'
            ),
            "<activation_task>",
            task_block,
            "</activation_task>",
            "<skill_metadata>",
            f"name: {html.escape(skill.metadata.name)}",
            f"description: {skill_description}",
            f"license: {license_name}",
            f"compatibility: {compatibility}",
            f"allowed_tools: {allowed_tools}",
            f"conformance: {conformance}",
            f"skill_root: {skill_root or 'not available for archive skills'}",
            "</skill_metadata>",
            (
                "<skill_instructions>"
                f"{_format_xml_cdata(instructions)}"
                "</skill_instructions>"
            ),
            "<skill_resources>",
            *resource_lines,
            "</skill_resources>",
            (
                f"{root_guidance}{lkg_guidance} Read only the listed resources that "
                "the instructions require, using MCP resources/read or the "
                "read_skill_resource tool. "
                "The allowed-tools declaration never bypasses host permissions."
            ),
            "</skill_content>",
        ]
    )


def _format_resource_reference(resource: McpResource) -> str:
    metadata = _resource_metadata(resource)
    mime_type = metadata.get("mimeType") or "application/octet-stream"
    return (
        f'  <file path="{html.escape(metadata["path"], quote=True)}" '
        f'uri="{html.escape(metadata["uri"], quote=True)}" '
        f'mimeType="{html.escape(mime_type, quote=True)}" '
        f'kind="{html.escape(metadata["kind"], quote=True)}" />'
    )


def _build_skill_resources(skill: Skill) -> Iterator[McpResource]:
    for rel_path in skill.iter_resource_paths():
        mime_type, _ = mimetypes.guess_type(rel_path)
        uri = _build_resource_uri(skill, rel_path)
        name = f"{skill.slug}/{rel_path}"

        def _make_reader(
            bound_skill: Skill, bound_path: str
        ) -> Callable[[], str | bytes]:
            def _reader() -> str | bytes:
                try:
                    data = bound_skill.open_bytes(bound_path)
                except _SKILL_TOOL_FAILURES as exc:
                    raise ToolError(_SKILL_RESOURCE_UNAVAILABLE_MESSAGE) from exc
                try:
                    return _normalize_text(data.decode("utf-8"))
                except UnicodeDecodeError:
                    return data

            return _reader

        yield McpResource(
            uri=uri,
            name=name,
            mime_type=mime_type,
            handler=_make_reader(skill, rel_path),
        )


def _resource_metadata(resource: McpResource) -> dict[str, Any]:
    rel_path = resource.name.split("/", 1)[1] if "/" in resource.name else resource.name
    metadata = {
        "uri": resource.uri,
        "name": resource.name,
        "path": rel_path,
        "mime_type": resource.mime_type,
        "kind": _resource_kind(rel_path),
    }
    if resource.mime_type:
        metadata["mimeType"] = resource.mime_type
    return metadata


def _build_resource_uri(skill: Skill, rel_path: str) -> str:
    encoded_slug = quote(skill.slug, safe="")
    encoded_path = quote(rel_path, safe="/")
    return f"resource://skillz/{encoded_slug}/{encoded_path}"


def _find_skill_markdown_file(directory: Path) -> Path | None:
    try:
        matches = [
            entry
            for entry in directory.iterdir()
            if entry.is_file() and entry.name.lower() == SKILL_MARKDOWN_LOWER
        ]
    except (OSError, PermissionError):
        return None
    if len(matches) > 1:
        raise SkillValidationError(
            f"Skill directory {directory} must contain exactly one SKILL.md file."
        )
    return matches[0] if matches else None


def _is_ignored_zip_member(name: str) -> bool:
    return (
        name.startswith("__MACOSX/")
        or "/__MACOSX/" in name
        or name.endswith(".DS_Store")
        or "/.DS_Store" in name
    )


def _validate_relative_resource_path(rel_path: str) -> None:
    if not _is_valid_relative_resource_path(rel_path):
        raise SkillError(f"Invalid resource path: {rel_path}")


def _is_valid_relative_resource_path(rel_path: str) -> bool:
    parts = PurePosixPath(rel_path).parts
    raw_parts = rel_path.split("/")
    return not (
        rel_path.startswith("/")
        or "\\" in rel_path
        or not rel_path
        or (parts and parts[0].endswith(":"))
        or any(part in {"", ".", ".."} for part in raw_parts)
    )


def _parse_required_skill_metadata_fields(
    data: Mapping[str, Any],
    *,
    source: str,
    container_name: str | None,
    validation_mode: str,
    json_budget: _JsonSafeBudget,
    compatibility_warning: Callable[[str, str], None],
) -> tuple[str, str]:
    raw_name = data.get("name")
    raw_description = data.get("description")
    safe_raw_name, _ = _json_safe_value(
        raw_name,
        source=source,
        field_path="name",
        budget=json_budget,
    )
    safe_raw_description, _ = _json_safe_value(
        raw_description,
        source=source,
        field_path="description",
        budget=json_budget,
    )
    if validation_mode == SKILL_VALIDATION_STRICT:
        if not isinstance(raw_name, str) or not raw_name:
            raise SkillValidationError(
                f"Front matter in {source} must contain a non-empty string 'name'."
            )
        if (
            not isinstance(raw_description, str)
            or not raw_description
            or raw_description.isspace()
        ):
            raise SkillValidationError(
                f"Front matter in {source} must contain a non-empty string 'description'."
            )
        name = raw_name
        description = raw_description
    else:
        description_text = _json_safe_text(
            raw_description,
            safe_raw_description,
        )
        if raw_description is None or not description_text.strip():
            raise SkillValidationError(
                f"Front matter in {source} is missing 'description'."
            )
        if not isinstance(raw_description, str):
            compatibility_warning(
                "skills.compat.description_type",
                "Converted a non-string description to text in compatible mode.",
            )
        description = description_text.strip()
        if description != description_text:
            compatibility_warning(
                "skills.compat.description_whitespace",
                "Trimmed description padding in compatible mode.",
            )
        if raw_name is None or not _json_safe_text(raw_name, safe_raw_name).strip():
            if not container_name:
                raise SkillValidationError(
                    f"Front matter in {source} is missing 'name'."
                )
            name = slugify(container_name)
            compatibility_warning(
                "skills.compat.name_missing",
                f"Derived missing skill name '{name}' from the container.",
            )
        else:
            if not isinstance(raw_name, str):
                compatibility_warning(
                    "skills.compat.name_type",
                    "Converted a non-string skill name to text in compatible mode.",
                )
            name_text = _json_safe_text(raw_name, safe_raw_name)
            name = name_text.strip()
            if name != name_text:
                compatibility_warning(
                    "skills.compat.name_whitespace",
                    "Trimmed skill name padding in compatible mode.",
                )

    if not 1 <= len(name) <= 64 or not SKILL_NAME_PATTERN.fullmatch(name):
        if validation_mode == SKILL_VALIDATION_STRICT:
            raise SkillValidationError(
                f"Skill name '{name}' in {source} must be 1-64 lowercase ASCII "
                "letters, digits, or single hyphen-separated segments."
            )
        compatibility_warning(
            "skills.compat.name_format",
            f"Accepted nonportable skill name '{name}' in compatible mode.",
        )
    if container_name is not None and name != container_name:
        if validation_mode == SKILL_VALIDATION_STRICT:
            raise SkillValidationError(
                f"Skill name '{name}' in {source} must match container '{container_name}'."
            )
        compatibility_warning(
            "skills.compat.container_mismatch",
            f"Accepted skill name '{name}' that does not match container '{container_name}'.",
        )
    if not 1 <= len(description) <= 1024:
        raise SkillValidationError(
            f"Description in {source} must be 1-1024 characters."
        )
    return name, description


def _parse_skill_metadata_mapping(
    data: Mapping[str, Any],
    *,
    source: str,
    validation_mode: str,
    json_budget: _JsonSafeBudget,
    compatibility_warning: Callable[[str, str], None],
) -> dict[str, str]:
    raw_metadata = data["metadata"] if "metadata" in data else {}
    if not isinstance(raw_metadata, Mapping):
        _json_safe_value(
            raw_metadata,
            source=source,
            field_path="metadata",
            budget=json_budget,
        )
        if validation_mode == SKILL_VALIDATION_STRICT:
            raise SkillValidationError(f"Metadata in {source} must be a mapping.")
        compatibility_warning(
            "skills.compat.metadata_type",
            "Ignored non-mapping metadata in compatible mode.",
        )
        raw_metadata = {}
    metadata: dict[str, str] = {}
    for key, value in raw_metadata.items():
        safe_key, _ = _json_safe_mapping_key(
            key,
            source=source,
            field_path="metadata",
            budget=json_budget,
        )
        if not isinstance(key, str):
            if validation_mode == SKILL_VALIDATION_STRICT:
                raise SkillValidationError(
                    f"Metadata keys and values in {source} must be strings."
                )
            compatibility_warning(
                "skills.compat.metadata_key",
                f"Converted non-string metadata key to '{safe_key}' in compatible mode.",
            )
        if safe_key in metadata:
            raise _SkillMetadataKeyCollision(
                f"Metadata keys in {source} collide after compatible conversion: "
                f"'{safe_key}'."
            )
        safe_metadata_value, _ = _json_safe_value(
            value,
            source=source,
            field_path=f"metadata.{safe_key}",
            budget=json_budget,
        )
        if not isinstance(value, str):
            if validation_mode == SKILL_VALIDATION_STRICT:
                raise SkillValidationError(
                    f"Metadata keys and values in {source} must be strings."
                )
            compatibility_warning(
                "skills.compat.metadata_value",
                "Converted a non-string metadata value to text in compatible mode.",
            )
            safe_value = _json_safe_text(value, safe_metadata_value)
        else:
            safe_value = value
        metadata[safe_key] = safe_value
    return metadata


def _parse_allowed_tools_declaration(
    data: Mapping[str, Any],
    *,
    source: str,
    validation_mode: str,
    json_budget: _JsonSafeBudget,
    compatibility_warning: Callable[[str, str], None],
    diagnostics: list[SkillDiagnostic],
) -> tuple[tuple[str, ...], Any]:
    raw_allowed_tools: Any = None
    if "allowed-tools" in data:
        raw_allowed_tools = data["allowed-tools"]
        allowed_tools_declaration, declaration_converted = _json_safe_value(
            raw_allowed_tools,
            source=source,
            field_path="allowed-tools",
            budget=json_budget,
        )
        allowed_tools = _parse_allowed_tools(
            raw_allowed_tools,
            source=source,
            validation_mode=validation_mode,
            compatibility_warning=compatibility_warning,
        )
    elif "allowed_tools" in data and validation_mode == SKILL_VALIDATION_COMPATIBLE:
        raw_allowed_tools = data["allowed_tools"]
        allowed_tools_declaration, declaration_converted = _json_safe_value(
            raw_allowed_tools,
            source=source,
            field_path="allowed-tools",
            budget=json_budget,
        )
        compatibility_warning(
            "skills.compat.allowed_tools_alias",
            "Accepted the noncanonical allowed_tools alias in compatible mode.",
        )
        allowed_tools = _parse_allowed_tools(
            raw_allowed_tools,
            source=source,
            validation_mode=validation_mode,
            compatibility_warning=compatibility_warning,
        )
    else:
        allowed_tools = ()
        allowed_tools_declaration, declaration_converted = _json_safe_value(
            raw_allowed_tools,
            source=source,
            field_path="allowed-tools",
            budget=json_budget,
        )
    if declaration_converted:
        diagnostics.append(
            SkillDiagnostic(
                "skills.metadata.allowed_tools_declaration_converted",
                "warning",
                source,
                "Converted the allowed-tools declaration to a stable JSON representation.",
            )
        )
    return allowed_tools, allowed_tools_declaration


def _parse_unknown_skill_metadata_fields(
    data: Mapping[str, Any],
    *,
    source: str,
    validation_mode: str,
    json_budget: _JsonSafeBudget,
    diagnostics: list[SkillDiagnostic],
) -> dict[str, Any]:
    known_keys = {
        "name",
        "description",
        "license",
        "compatibility",
        "metadata",
        "allowed-tools",
    }
    if validation_mode == SKILL_VALIDATION_COMPATIBLE:
        known_keys.add("allowed_tools")
    extra: dict[str, Any] = {}
    for key, value in data.items():
        if key in known_keys:
            continue
        safe_key, key_converted = _json_safe_mapping_key(
            key,
            source=source,
            field_path="front matter",
            budget=json_budget,
        )
        safe_value, value_converted = _json_safe_value(
            value,
            source=source,
            field_path=safe_key,
            budget=json_budget,
        )
        if safe_key in extra:
            raise SkillValidationError(
                f"Unknown front matter fields in {source} collide after JSON key conversion: "
                f"'{safe_key}'."
            )
        extra[safe_key] = safe_value
        diagnostics.append(
            SkillDiagnostic(
                "skills.metadata.unknown_field",
                "warning",
                source,
                f"Unknown top-level front matter field '{safe_key}' was preserved.",
            )
        )
        if key_converted or value_converted:
            diagnostics.append(
                SkillDiagnostic(
                    "skills.metadata.extra_converted",
                    "warning",
                    source,
                    f"Unknown field '{safe_key}' was converted to a stable JSON representation.",
                )
            )
    try:
        json.dumps(extra, ensure_ascii=False, sort_keys=True, allow_nan=False)
    except (TypeError, ValueError) as exc:
        raise SkillValidationError(
            f"Unknown front matter fields in {source} are not JSON-safe: {exc}"
        ) from exc
    return extra


def _parse_skill_metadata(
    front_matter: str,
    *,
    source: str,
    container_name: str | None,
    validation_mode: str,
    diagnostics: list[SkillDiagnostic],
) -> tuple[SkillMetadata, str]:
    data = _load_front_matter_mapping(front_matter, source)
    json_budget = _new_json_safe_budget(source)
    compatible = False

    def compatibility_warning(code: str, message: str) -> None:
        nonlocal compatible
        compatible = True
        diagnostics.append(SkillDiagnostic(code, "warning", source, message))

    name, description = _parse_required_skill_metadata_fields(
        data,
        source=source,
        container_name=container_name,
        validation_mode=validation_mode,
        json_budget=json_budget,
        compatibility_warning=compatibility_warning,
    )
    metadata = _parse_skill_metadata_mapping(
        data,
        source=source,
        validation_mode=validation_mode,
        json_budget=json_budget,
        compatibility_warning=compatibility_warning,
    )

    license_name = _optional_string_field(
        data,
        "license",
        source,
        validation_mode,
        compatibility_warning,
        json_budget,
    )
    compatibility = _optional_string_field(
        data,
        "compatibility",
        source,
        validation_mode,
        compatibility_warning,
        json_budget,
    )
    if compatibility is not None and not 1 <= len(compatibility) <= 500:
        if validation_mode == SKILL_VALIDATION_COMPATIBLE and not compatibility:
            compatibility_warning(
                "skills.compat.compatibility_empty",
                "Treated an empty compatibility field as absent in compatible mode.",
            )
            compatibility = None
        else:
            raise SkillValidationError(
                f"Compatibility in {source} must be 1-500 characters when present."
            )

    allowed_tools, allowed_tools_declaration = _parse_allowed_tools_declaration(
        data,
        source=source,
        validation_mode=validation_mode,
        json_budget=json_budget,
        compatibility_warning=compatibility_warning,
        diagnostics=diagnostics,
    )
    extra = _parse_unknown_skill_metadata_fields(
        data,
        source=source,
        validation_mode=validation_mode,
        json_budget=json_budget,
        diagnostics=diagnostics,
    )
    return SkillMetadata(
        name=name,
        description=description,
        license=license_name,
        compatibility=compatibility,
        allowed_tools=allowed_tools,
        allowed_tools_declaration=allowed_tools_declaration,
        metadata=metadata,
        extra=extra,
    ), SKILL_VALIDATION_COMPATIBLE if compatible else SKILL_VALIDATION_STRICT


def _parse_skill_document(
    raw: str,
    *,
    source: str,
    container_name: str | None,
    validation_mode: str,
    diagnostics: list[SkillDiagnostic],
    conformance_override: str | None = None,
) -> tuple[SkillMetadata, str, str]:
    validation_mode = _validate_validation_mode(validation_mode)
    normalized = _normalize_text(raw)
    if validation_mode == SKILL_VALIDATION_STRICT:
        match = STRICT_FRONT_MATTER_PATTERN.match(normalized)
    else:
        had_bom = normalized.startswith("\ufeff")
        normalized = normalized.removeprefix("\ufeff")
        if had_bom:
            diagnostics.append(
                SkillDiagnostic(
                    "skills.compat.frontmatter_format",
                    "warning",
                    source,
                    "Accepted a UTF-8 BOM before YAML front matter in compatible mode.",
                )
            )
        match = STRICT_FRONT_MATTER_PATTERN.match(normalized)
        if match is None:
            match = FRONT_MATTER_PATTERN.match(normalized)
            if match is not None:
                diagnostics.append(
                    SkillDiagnostic(
                        "skills.compat.frontmatter_format",
                        "warning",
                        source,
                        "Accepted noncanonical YAML front matter delimiters in compatible mode.",
                    )
                )
    if not match:
        raise SkillValidationError(
            f"Skill {source} is missing YAML front matter."
        )
    metadata, conformance = _parse_skill_metadata(
        match.group(1),
        source=source,
        container_name=container_name,
        validation_mode=validation_mode,
        diagnostics=diagnostics,
    )
    if any(
        diagnostic.source == source and diagnostic.code.startswith("skills.compat.")
        for diagnostic in diagnostics
    ):
        conformance = SKILL_VALIDATION_COMPATIBLE
    if conformance_override is not None:
        conformance = _validate_validation_mode(conformance_override)
    _validate_bridge_metadata_budget(metadata, conformance, source)
    return metadata, match.group(2), conformance


def _validate_pyyaml_duplicate_mapping_keys(
    yaml_module: Any,
    front_matter: str,
    source: str,
) -> None:
    safe_loader = getattr(yaml_module, "SafeLoader", None)
    mapping_node_type = getattr(yaml_module, "MappingNode", None)
    sequence_node_type = getattr(yaml_module, "SequenceNode", None)
    if not (
        isinstance(safe_loader, type)
        and isinstance(mapping_node_type, type)
        and isinstance(sequence_node_type, type)
    ):
        return

    loader = safe_loader(front_matter)
    try:
        root_node = loader.get_single_node()
        if root_node is None:
            return
        stack = [root_node]
        visited: set[int] = set()
        while stack:
            node = stack.pop()
            identity = id(node)
            if identity in visited:
                continue
            visited.add(identity)

            if isinstance(node, mapping_node_type):
                seen_keys: set[Any] = set()
                for key_node, value_node in node.value:
                    try:
                        key = loader.construct_object(key_node, deep=True)
                        hash(key)
                    except RecursionError as exc:
                        raise SkillValidationError(
                            f"YAML front matter in {source} exceeds the "
                            "duplicate-key recursion depth budget."
                        ) from exc
                    except Exception:
                        # 无法构造或散列的键仍交由后续 safe_load 生成原有诊断。
                        pass
                    else:
                        if key in seen_keys:
                            raise SkillValidationError(
                                f"Duplicate YAML mapping key in {source}."
                            )
                        seen_keys.add(key)
                    stack.append(value_node)
                    stack.append(key_node)
            elif isinstance(node, sequence_node_type):
                stack.extend(node.value)
    finally:
        loader.dispose()


def _load_front_matter_mapping(front_matter: str, source: str) -> Mapping[str, Any]:
    try:
        import yaml  # type: ignore[import-not-found]
    except ImportError:
        data = _parse_simple_yaml(front_matter, source)
    else:
        try:
            _validate_pyyaml_duplicate_mapping_keys(yaml, front_matter, source)
            data = yaml.safe_load(front_matter) or {}
        except SkillValidationError:
            raise
        except Exception as exc:
            raise SkillValidationError(
                f"YAML front matter in {source} is invalid: {exc}"
            ) from exc
    if not isinstance(data, Mapping):
        raise SkillValidationError(f"Front matter in {source} must be a mapping.")
    return data


def _parse_simple_yaml(front_matter: str, source: str) -> dict[str, Any]:
    result: dict[str, Any] = {}
    in_metadata = False
    metadata_indent: int | None = None
    for line_number, raw_line in enumerate(front_matter.splitlines(), start=1):
        if not raw_line.strip() or raw_line.lstrip().startswith("#"):
            continue
        if "\t" in raw_line:
            raise SkillValidationError(
                f"Unsupported YAML indentation in {source}:{line_number}."
            )
        content = raw_line.lstrip(" ")
        indent = len(raw_line) - len(content)
        if indent:
            if not in_metadata:
                raise SkillValidationError(
                    f"Unsupported YAML indentation in {source}:{line_number}."
                )
            if metadata_indent is None:
                metadata_indent = indent
            elif indent != metadata_indent:
                raise SkillValidationError(
                    f"Inconsistent metadata indentation in {source}:{line_number}."
                )
            if content.startswith("-"):
                raise SkillValidationError(
                    f"YAML sequences in {source}:{line_number} require PyYAML."
                )
            key, value = _parse_simple_yaml_entry(content, source, line_number)
            if not value:
                raise SkillValidationError(
                    f"Nested YAML mappings in {source}:{line_number} require PyYAML."
                )
            metadata = result["metadata"]
            if not isinstance(metadata, dict):
                raise SkillValidationError(
                    f"Metadata in {source}:{line_number} must be a one-level mapping."
                )
            if key in metadata:
                raise SkillValidationError(
                    f"Duplicate metadata key '{key}' in {source}:{line_number}."
                )
            metadata[key] = _parse_yaml_scalar(value, source, line_number)
            continue

        in_metadata = False
        metadata_indent = None
        if content.startswith("-"):
            raise SkillValidationError(
                f"YAML sequences in {source}:{line_number} require PyYAML."
            )
        key, value = _parse_simple_yaml_entry(content, source, line_number)
        if key in result:
            raise SkillValidationError(
                f"Duplicate YAML key '{key}' in {source}:{line_number}."
            )
        if key == "metadata":
            if value:
                raise SkillValidationError(
                    f"Metadata in {source}:{line_number} must be a one-level mapping."
                )
            result[key] = {}
            in_metadata = True
        else:
            result[key] = (
                None
                if not value
                else _parse_yaml_scalar(value, source, line_number)
            )
    return result


def _parse_simple_yaml_entry(
    content: str,
    source: str,
    line_number: int,
) -> tuple[str, str]:
    if ":" not in content:
        raise SkillValidationError(
            f"Unsupported YAML syntax in {source}:{line_number}."
        )
    raw_key, raw_value = content.split(":", 1)
    key = raw_key.strip()
    if (
        not key
        or raw_key != key
        or re.fullmatch(r"[A-Za-z0-9_.-]+", key) is None
    ):
        raise SkillValidationError(
            f"Unsupported YAML key syntax in {source}:{line_number}."
        )
    return key, raw_value.strip()


def _parse_yaml_scalar(value: str, source: str, line_number: int) -> Any:
    if value.startswith(("|", ">")):
        raise SkillValidationError(
            f"Block YAML scalars in {source}:{line_number} require PyYAML."
        )
    if value.startswith(("- ", "[")) or value.endswith("]"):
        raise SkillValidationError(
            f"YAML sequences in {source}:{line_number} require PyYAML."
        )
    if value.startswith("{") or value.endswith("}"):
        raise SkillValidationError(
            f"Inline YAML mappings in {source}:{line_number} require PyYAML."
        )
    return _parse_plain_yaml_scalar(value, source, line_number)


def _parse_plain_yaml_scalar(value: str, source: str, line_number: int) -> Any:
    if value.startswith("'") or value.endswith("'"):
        if len(value) < 2 or not (value.startswith("'") and value.endswith("'")):
            raise SkillValidationError(
                f"Invalid single-quoted YAML scalar in {source}:{line_number}."
            )
        inner = value[1:-1]
        parsed: list[str] = []
        index = 0
        while index < len(inner):
            if inner[index] != "'":
                parsed.append(inner[index])
                index += 1
                continue
            if index + 1 >= len(inner) or inner[index + 1] != "'":
                raise SkillValidationError(
                    f"Invalid single-quoted YAML scalar in {source}:{line_number}."
                )
            parsed.append("'")
            index += 2
        return "".join(parsed)
    if value.startswith('"') or value.endswith('"'):
        if len(value) < 2 or not (value.startswith('"') and value.endswith('"')):
            raise SkillValidationError(
                f"Invalid double-quoted YAML scalar in {source}:{line_number}."
            )
        try:
            parsed_value = json.loads(value)
        except json.JSONDecodeError as exc:
            raise SkillValidationError(
                f"Invalid double-quoted YAML scalar in {source}:{line_number}."
            ) from exc
        if not isinstance(parsed_value, str):
            raise SkillValidationError(
                f"Invalid double-quoted YAML scalar in {source}:{line_number}."
            )
        return parsed_value
    if value.startswith(("? ", ": ")):
        raise SkillValidationError(
            f"Nested YAML mappings in {source}:{line_number} require PyYAML."
        )
    if re.search(r"(?:^|\s)#", value):
        raise SkillValidationError(
            f"Inline YAML comments in {source}:{line_number} require PyYAML."
        )
    if value.startswith(("&", "*", "!")):
        raise SkillValidationError(
            f"YAML anchors, aliases, and tags in {source}:{line_number} require PyYAML."
        )
    if re.search(r":(?:\s|$)", value):
        raise SkillValidationError(
            f"Nested YAML mappings in {source}:{line_number} require PyYAML."
        )
    lowered = value.casefold()
    if lowered in {"null", "~"}:
        return None
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    if re.fullmatch(r"[+-]?(?:0|[1-9][0-9]*)", value):
        try:
            return int(value, 10)
        except ValueError as exc:
            raise SkillValidationError(
                f"YAML integer in {source}:{line_number} is too large."
            ) from exc
    if re.fullmatch(
        r"[+-]?(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)",
        value,
    ):
        parsed_number = float(value)
        if not math.isfinite(parsed_number):
            raise SkillValidationError(
                f"Non-finite YAML number in {source}:{line_number} is unsupported."
            )
        return parsed_number
    if lowered in {".nan", ".inf", "+.inf", "-.inf"}:
        raise SkillValidationError(
            f"Non-finite YAML number in {source}:{line_number} is unsupported."
        )
    return value


def _json_safe_value(
    value: Any,
    *,
    source: str,
    field_path: str,
    active_containers: set[int] | None = None,
    budget: _JsonSafeBudget | None = None,
    depth: int = 0,
) -> tuple[Any, bool]:
    """Convert YAML values to deterministic JSON values without dependencies."""

    json_budget = budget if budget is not None else _new_json_safe_budget(source)
    try:
        return _json_safe_value_impl(
            value,
            source=source,
            field_path=field_path,
            active_containers=active_containers,
            budget=json_budget,
            depth=depth,
        )
    except RecursionError as exc:
        raise SkillValidationError(
            f"Field '{field_path}' in {source} exceeds the JSON-safe recursion "
            "depth budget."
        ) from exc


def _json_safe_value_impl(
    value: Any,
    *,
    source: str,
    field_path: str,
    active_containers: set[int] | None,
    budget: _JsonSafeBudget,
    depth: int,
) -> tuple[Any, bool]:
    budget.visit(field_path, depth)
    if value is None or isinstance(value, (str, bool, int)):
        budget.generate(field_path, _json_scalar_output_size(value))
        return value, False
    if isinstance(value, float):
        if not math.isfinite(value):
            raise SkillValidationError(
                f"Field '{field_path}' in {source} contains a non-finite number."
            )
        budget.generate(field_path, _json_scalar_output_size(value))
        return value, False
    if isinstance(value, (datetime, date)):
        converted_date = value.isoformat()
        budget.generate(field_path, _json_scalar_output_size(converted_date))
        return converted_date, True

    active = active_containers if active_containers is not None else set()
    identity = id(value)
    if identity in active:
        raise SkillValidationError(
            f"Field '{field_path}' in {source} contains a cyclic YAML value."
        )

    if isinstance(value, Mapping):
        active.add(identity)
        try:
            budget.generate(field_path)
            converted: dict[str, Any] = {}
            changed = False
            for key, item in value.items():
                safe_key, key_changed = _json_safe_mapping_key(
                    key,
                    source=source,
                    field_path=field_path,
                    active_containers=active,
                    budget=budget,
                    depth=depth + 1,
                )
                if safe_key in converted:
                    raise SkillValidationError(
                        f"Field '{field_path}' in {source} has keys that collide after "
                        f"JSON conversion: '{safe_key}'."
                    )
                safe_item, item_changed = _json_safe_value(
                    item,
                    source=source,
                    field_path=f"{field_path}.{safe_key}",
                    active_containers=active,
                    budget=budget,
                    depth=depth + 1,
                )
                converted[safe_key] = safe_item
                changed = changed or key_changed or item_changed
            return converted, changed
        finally:
            active.remove(identity)

    if isinstance(value, (list, tuple, set, frozenset)):
        active.add(identity)
        try:
            budget.generate(field_path)
            converted_items: list[Any] = []
            changed = not isinstance(value, list)
            for index, item in enumerate(value):
                safe_item, item_changed = _json_safe_value(
                    item,
                    source=source,
                    field_path=f"{field_path}[{index}]",
                    active_containers=active,
                    budget=budget,
                    depth=depth + 1,
                )
                converted_items.append(safe_item)
                changed = changed or item_changed
            if isinstance(value, (set, frozenset)):
                converted_items.sort(key=_stable_json_sort_key)
            return converted_items, changed
        finally:
            active.remove(identity)

    raise SkillValidationError(
        f"Field '{field_path}' in {source} contains unsupported YAML type "
        f"'{type(value).__name__}'."
    )


def _json_safe_mapping_key(
    value: Any,
    *,
    source: str,
    field_path: str,
    active_containers: set[int] | None = None,
    budget: _JsonSafeBudget | None = None,
    depth: int = 0,
) -> tuple[str, bool]:
    json_budget = budget if budget is not None else _new_json_safe_budget(source)
    safe_value, _ = _json_safe_value(
        value,
        source=source,
        field_path=f"{field_path} key",
        active_containers=active_containers,
        budget=json_budget,
        depth=depth,
    )
    if isinstance(value, str):
        return value, False
    if isinstance(safe_value, (dict, list)):
        safe_key = _stable_json_sort_key(safe_value)
    elif isinstance(safe_value, str):
        safe_key = safe_value
    elif safe_value is None:
        safe_key = "null"
    elif isinstance(safe_value, bool):
        safe_key = "true" if safe_value else "false"
    else:
        safe_key = json.dumps(safe_value, ensure_ascii=False, allow_nan=False)
    json_budget.generate(f"{field_path} key", _json_scalar_output_size(safe_key))
    return safe_key, True


def _new_json_safe_budget(source: str) -> _JsonSafeBudget:
    return _JsonSafeBudget(
        source=source,
        max_depth=MAX_JSON_SAFE_RECURSION_DEPTH,
        max_visited_nodes=MAX_JSON_SAFE_VISITED_NODES,
        max_generated_nodes=MAX_JSON_SAFE_GENERATED_NODES,
        max_output_bytes=MAX_JSON_SAFE_OUTPUT_BYTES,
    )


def _json_scalar_output_size(value: Any) -> int:
    if value is None:
        return 4
    if value is True:
        return 4
    if value is False:
        return 5
    if isinstance(value, str):
        return len(value.encode("utf-8"))
    return len(str(value).encode("utf-8"))


def _json_safe_text(original: Any, safe_value: Any) -> str:
    if isinstance(safe_value, str):
        return safe_value
    if isinstance(safe_value, (dict, list)):
        return _stable_json_sort_key(safe_value)
    return str(original)


def _stable_json_sort_key(value: Any) -> str:
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    )


def _normalize_text(value: str) -> str:
    return value.replace("\r\n", "\n").replace("\r", "\n")


def _optional_string_field(
    data: Mapping[str, Any],
    key: str,
    source: str,
    validation_mode: str,
    compatibility_warning: Callable[[str, str], None],
    budget: _JsonSafeBudget,
) -> str | None:
    if key not in data:
        return None
    value = data[key]
    safe_value, _ = _json_safe_value(
        value,
        source=source,
        field_path=key,
        budget=budget,
    )
    if not isinstance(value, str):
        if validation_mode == SKILL_VALIDATION_STRICT:
            raise SkillValidationError(
                f"Front matter field '{key}' in {source} must be a string."
            )
        compatibility_warning(
            f"skills.compat.{key}_type",
            f"Converted non-string field '{key}' to text in compatible mode.",
        )
    if validation_mode == SKILL_VALIDATION_STRICT:
        return value
    text = _json_safe_text(value, safe_value)
    stripped = text.strip()
    if stripped != text:
        compatibility_warning(
            f"skills.compat.{key}_whitespace",
            f"Trimmed field '{key}' padding in compatible mode.",
        )
    return stripped


def _parse_allowed_tools(
    value: Any,
    *,
    source: str,
    validation_mode: str,
    compatibility_warning: Callable[[str, str], None],
) -> tuple[str, ...]:
    if value is None:
        if validation_mode == SKILL_VALIDATION_STRICT:
            raise SkillValidationError(
                f"Front matter field 'allowed-tools' in {source} must be a space-delimited string."
            )
        compatibility_warning(
            "skills.compat.allowed_tools_type",
            "Treated a null allowed-tools value as empty in compatible mode.",
        )
        return ()
    if isinstance(value, str):
        if validation_mode == SKILL_VALIDATION_STRICT:
            if "," in value:
                raise SkillValidationError(
                    f"Front matter field 'allowed-tools' in {source} must use spaces, "
                    "not commas, as separators."
                )
            if value and re.fullmatch(r"[^,\s]+(?: [^,\s]+)*", value) is None:
                raise SkillValidationError(
                    f"Front matter field 'allowed-tools' in {source} must use "
                    "single ASCII spaces without padding."
                )
            return tuple(value.split(" ")) if value else ()
        if "," in value:
            compatibility_warning(
                "skills.compat.allowed_tools_commas",
                "Accepted comma-delimited allowed-tools in compatible mode.",
            )
        if value != value.strip() or re.search(r"\s{2,}|[^ ]\s", value):
            compatibility_warning(
                "skills.compat.allowed_tools_whitespace",
                "Normalized noncanonical allowed-tools whitespace in compatible mode.",
            )
        return tuple(part for part in re.split(r"[\s,]+", value.strip()) if part)
    if validation_mode == SKILL_VALIDATION_STRICT:
        raise SkillValidationError(
            f"Front matter field 'allowed-tools' in {source} must be a space-delimited string."
        )
    if isinstance(value, Iterable) and not isinstance(value, Mapping):
        compatibility_warning(
            "skills.compat.allowed_tools_list",
            "Accepted a YAML list for allowed-tools in compatible mode.",
        )
        items = value
        if isinstance(value, (set, frozenset)):
            items = sorted(value, key=lambda item: str(item))
        return tuple(str(item).strip() for item in items if str(item).strip())
    compatibility_warning(
        "skills.compat.allowed_tools_type",
        "Ignored an unsupported allowed-tools value in compatible mode.",
    )
    return ()


def _validate_validation_mode(value: Any) -> str:
    if not isinstance(value, str) or value not in SKILL_VALIDATION_MODES:
        expected = ", ".join(sorted(SKILL_VALIDATION_MODES))
        raise ValueError(f"validation_mode must be one of: {expected}")
    return value


def _validate_nonnegative_finite_number(value: Any, name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{name} must be a non-negative finite number")
    result = float(value)
    if result < 0 or not math.isfinite(result):
        raise ValueError(f"{name} must be a non-negative finite number")
    return result


def _skill_clock_value(clock: Callable[[], float]) -> float:
    value = clock()
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("clock must return a finite number")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError("clock must return a finite number")
    return result


def _diagnostics_dicts(
    diagnostics: Iterable[SkillDiagnostic],
) -> list[dict[str, str]]:
    return [diagnostic.as_dict() for diagnostic in diagnostics]


def _instructions_source_path(skill: Skill) -> str:
    if skill.zip_path is not None:
        return f"{skill.zip_path}!/{skill.zip_root_prefix}{skill.instructions_member}"
    return str(skill.instructions_path)


def _skill_source(skill: Skill) -> str:
    return _instructions_source_path(skill)


def _resource_kind(rel_path: str) -> str:
    first = PurePosixPath(rel_path).parts[0].lower()
    if first in {"scripts", "script"}:
        return "script"
    if first in {"references", "reference", "docs", "doc"}:
        return "reference"
    if first in {"assets", "asset"}:
        return "asset"
    return "file"


def _select_archive_instructions(
    members: Iterable[str],
    source: str,
    diagnostics: list[SkillDiagnostic],
) -> tuple[str, str]:
    candidates = [
        name
        for name in members
        if PurePosixPath(name).name.lower() == SKILL_MARKDOWN_LOWER
    ]
    if len(candidates) != 1:
        if not candidates:
            raise SkillValidationError(
                "Archive does not contain a canonical skill root."
            )
        raise SkillValidationError("Archive contains multiple canonical skill roots.")
    candidate = candidates[0]
    parts = PurePosixPath(candidate).parts
    if len(parts) not in (1, 2):
        raise SkillValidationError(
            "Archive SKILL.md must be at archive root or inside one top-level directory."
        )
    if parts[-1] != SKILL_MARKDOWN:
        diagnostics.append(
            SkillDiagnostic(
                "skills.compat.instructions_lowercase",
                "warning",
                f"{source}!/{candidate}",
                "Accepted a noncanonical case variant of SKILL.md in compatible mode.",
            )
        )
    return candidate, f"{parts[0]}/" if len(parts) == 2 else ""


_ZIP_END_RECORD_SIGNATURE = b"PK\x05\x06"
_ZIP_END_RECORD_SIZE = 22
_ZIP_MAX_COMMENT_BYTES = 65535
_ZIP64_END_LOCATOR_SIGNATURE = b"PK\x06\x07"
_ZIP64_END_LOCATOR_SIZE = 20
_ZIP_CENTRAL_DIRECTORY_SIGNATURE = b"PK\x01\x02"
_ZIP_CENTRAL_DIRECTORY_ENTRY_SIZE = 46


def _archive_uint16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "little")


def _archive_uint32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 4], "little")


def _read_exact_archive_bytes(handle: Any, size: int) -> bytes:
    data = handle.read(size)
    if len(data) != size:
        raise SkillValidationError("Archive central directory is truncated.")
    return data


def _preflight_skill_archive(handle: Any) -> None:
    try:
        handle.seek(0, os.SEEK_END)
        archive_size = handle.tell()
        tail_size = min(
            archive_size,
            _ZIP_END_RECORD_SIZE + _ZIP_MAX_COMMENT_BYTES,
        )
        handle.seek(archive_size - tail_size)
        tail = handle.read(tail_size)
    except (OSError, ValueError) as exc:
        raise SkillValidationError(
            "Archive cannot be read: end record could not be read."
        ) from exc
    if len(tail) != tail_size:
        raise SkillValidationError("Archive cannot be read: end record is truncated.")

    record_index = len(tail)
    while True:
        record_index = tail.rfind(
            _ZIP_END_RECORD_SIGNATURE,
            0,
            record_index,
        )
        if record_index < 0:
            raise SkillValidationError(
                "Archive cannot be read: end record is missing or invalid."
            )
        if record_index + _ZIP_END_RECORD_SIZE <= len(tail):
            comment_size = _archive_uint16(tail, record_index + 20)
            if record_index + _ZIP_END_RECORD_SIZE + comment_size == len(tail):
                break

    disk_number = _archive_uint16(tail, record_index + 4)
    central_directory_disk = _archive_uint16(tail, record_index + 6)
    entries_on_disk = _archive_uint16(tail, record_index + 8)
    entry_count = _archive_uint16(tail, record_index + 10)
    central_directory_size = _archive_uint32(tail, record_index + 12)
    central_directory_offset = _archive_uint32(tail, record_index + 16)
    record_offset = archive_size - tail_size + record_index

    if record_offset >= _ZIP64_END_LOCATOR_SIZE:
        try:
            handle.seek(record_offset - _ZIP64_END_LOCATOR_SIZE)
            locator = handle.read(_ZIP64_END_LOCATOR_SIZE)
        except (OSError, ValueError) as exc:
            raise SkillValidationError(
                "Archive cannot be read: end record could not be read."
            ) from exc
        if len(locator) != _ZIP64_END_LOCATOR_SIZE:
            raise SkillValidationError(
                "Archive cannot be read: end record is truncated."
            )
        if locator.startswith(_ZIP64_END_LOCATOR_SIGNATURE):
            raise SkillValidationError("ZIP64 archives are unsupported.")

    if central_directory_size > MAX_SKILL_ARCHIVE_CENTRAL_DIRECTORY_BYTES:
        raise SkillValidationError(
            "Archive central directory exceeds "
            f"{MAX_SKILL_ARCHIVE_CENTRAL_DIRECTORY_BYTES} bytes."
        )
    if (
        disk_number == 0xFFFF
        or central_directory_disk == 0xFFFF
        or entries_on_disk == 0xFFFF
        or entry_count == 0xFFFF
        or central_directory_size == 0xFFFFFFFF
        or central_directory_offset == 0xFFFFFFFF
    ):
        raise SkillValidationError("ZIP64 archives are unsupported.")
    if (
        disk_number != 0
        or central_directory_disk != 0
        or entries_on_disk != entry_count
    ):
        raise SkillValidationError("Multi-disk archives are unsupported.")
    if entry_count > MAX_SKILL_ARCHIVE_MEMBERS:
        raise SkillValidationError(
            f"Archive contains more than {MAX_SKILL_ARCHIVE_MEMBERS} members."
        )
    if central_directory_size > record_offset:
        raise SkillValidationError("Archive central directory has an invalid size.")

    central_directory_start = record_offset - central_directory_size
    try:
        handle.seek(central_directory_start)
        consumed = 0
        parsed_entries = 0
        while consumed < central_directory_size:
            if parsed_entries >= MAX_SKILL_ARCHIVE_MEMBERS:
                raise SkillValidationError(
                    f"Archive contains more than {MAX_SKILL_ARCHIVE_MEMBERS} members."
                )
            header = _read_exact_archive_bytes(
                handle,
                _ZIP_CENTRAL_DIRECTORY_ENTRY_SIZE,
            )
            if not header.startswith(_ZIP_CENTRAL_DIRECTORY_SIGNATURE):
                raise SkillValidationError("Archive central directory is invalid.")
            variable_size = (
                _archive_uint16(header, 28)
                + _archive_uint16(header, 30)
                + _archive_uint16(header, 32)
            )
            entry_size = _ZIP_CENTRAL_DIRECTORY_ENTRY_SIZE + variable_size
            if entry_size > central_directory_size - consumed:
                raise SkillValidationError("Archive central directory is truncated.")
            if _archive_uint16(header, 34) != 0:
                raise SkillValidationError("Multi-disk archives are unsupported.")
            handle.seek(variable_size, os.SEEK_CUR)
            consumed += entry_size
            parsed_entries += 1
    except SkillValidationError:
        raise
    except (OSError, ValueError) as exc:
        raise SkillValidationError("Archive central directory could not be read.") from exc
    if parsed_entries != entry_count:
        raise SkillValidationError("Archive central directory entry count is invalid.")


def _open_skill_archive(handle: Any) -> zipfile.ZipFile:
    _preflight_skill_archive(handle)
    try:
        return zipfile.ZipFile(handle)
    except NotImplementedError as exc:
        raise SkillValidationError(
            "Archive uses an unsupported compression method or extraction version."
        ) from exc


def _read_archive_info(
    archive: zipfile.ZipFile,
    info: zipfile.ZipInfo,
) -> bytes:
    try:
        with archive.open(info) as member:
            return member.read(MAX_FILE_READ_BYTES + 1)
    except NotImplementedError as exc:
        raise SkillValidationError(
            "Archive uses an unsupported compression method or extraction version."
        ) from exc


def _validated_archive_inventory(zip_path: Path) -> tuple[zipfile.ZipInfo, ...]:
    try:
        with zip_path.open("rb") as handle:
            with _open_skill_archive(handle) as archive:
                inventory = tuple(archive.infolist())
    except SkillValidationError:
        raise
    except NotImplementedError as exc:
        raise SkillValidationError(
            "Archive uses an unsupported compression method or extraction version."
        ) from exc
    except (OSError, zipfile.BadZipFile) as exc:
        raise SkillValidationError(f"Archive cannot be read: {exc}") from exc
    return _validate_archive_inventory_entries(inventory)


def _validate_archive_member_compression(info: zipfile.ZipInfo) -> None:
    compression_check = getattr(zipfile, "_check_compression", None)
    if not callable(compression_check):
        error = NotImplementedError(
            "The runtime does not expose archive compression capability checks."
        )
        raise SkillValidationError(
            f"Archive member '{info.filename}' uses an unsupported compression method."
        ) from error
    try:
        compression_check(info.compress_type)
    except (NotImplementedError, RuntimeError) as exc:
        raise SkillValidationError(
            f"Archive member '{info.filename}' uses an unsupported compression method."
        ) from exc


def _validate_archive_inventory_entries(
    inventory: tuple[zipfile.ZipInfo, ...],
) -> tuple[zipfile.ZipInfo, ...]:
    if len(inventory) > MAX_SKILL_ARCHIVE_MEMBERS:
        raise SkillValidationError(
            f"Archive contains more than {MAX_SKILL_ARCHIVE_MEMBERS} members."
        )
    names: set[str] = set()
    total_size = 0
    for info in inventory:
        name = info.filename
        normalized_name = name.rstrip("/")
        if not normalized_name or not _is_valid_relative_resource_path(normalized_name):
            raise SkillValidationError(f"Archive member has an unsafe path: {name}")
        if normalized_name in names:
            raise SkillValidationError(f"Archive contains duplicate member: {name}")
        names.add(normalized_name)
        if info.flag_bits & 0x1:
            raise SkillValidationError(
                f"Encrypted archive member is unsupported: {name}"
            )
        unix_mode = info.external_attr >> 16
        if unix_mode and stat.S_ISLNK(unix_mode):
            raise SkillValidationError(f"Archive symlink member is unsupported: {name}")
        if info.is_dir():
            continue
        _validate_archive_member_compression(info)
        if info.file_size > MAX_FILE_READ_BYTES:
            raise SkillValidationError(
                f"Archive member '{name}' exceeds {MAX_FILE_READ_BYTES} bytes."
            )
        total_size += info.file_size
        if total_size > MAX_SKILL_ARCHIVE_UNCOMPRESSED_BYTES:
            raise SkillValidationError(
                "Archive uncompressed content exceeds "
                f"{MAX_SKILL_ARCHIVE_UNCOMPRESSED_BYTES} bytes."
            )
        if info.file_size:
            if info.compress_size <= 0:
                raise SkillValidationError(
                    f"Archive member '{name}' has an invalid compression ratio."
                )
            ratio = info.file_size / info.compress_size
            if ratio > MAX_SKILL_ARCHIVE_COMPRESSION_RATIO:
                raise SkillValidationError(
                    f"Archive member '{name}' exceeds compression ratio "
                    f"{MAX_SKILL_ARCHIVE_COMPRESSION_RATIO}."
                )
    return inventory


def _reserved_skill_slugs() -> frozenset[str]:
    return frozenset(slugify(name) for name in RESERVED_TOOL_NAMES)


def _update_fingerprint_digest(digest: Any, *values: Any) -> None:
    for value in values:
        encoded = str(value).encode("utf-8", errors="surrogatepass")
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)


def _bounded_archive_fallback_digest(handle: Any, size: int) -> str:
    digest = hashlib.sha256()
    _update_fingerprint_digest(digest, "archive-fallback-v1", size)
    offsets = [0]
    if size > MAX_FILE_READ_BYTES:
        offsets.append(size - MAX_FILE_READ_BYTES)
    for offset in offsets:
        handle.seek(offset)
        data = handle.read(MAX_FILE_READ_BYTES)
        _update_fingerprint_digest(digest, offset, len(data))
        digest.update(data)
    return digest.hexdigest()


def _archive_snapshot_source_digest(
    path: Path,
    root: Path,
    expected_identity: _PathIdentity,
) -> str:
    handle, actual_identity = _open_discovered_file(path, root)
    if actual_identity != expected_identity:
        handle.close()
        raise _SkillFilesystemChanged(
            str(path),
            "Skill archive changed while its fingerprint was being built.",
        )
    try:
        with handle:
            try:
                with _open_skill_archive(handle) as archive:
                    inventory = archive.infolist()
                    digest = hashlib.sha256()
                    _update_fingerprint_digest(
                        digest,
                        "archive-inventory-v1",
                        len(inventory),
                    )
                    if len(inventory) > MAX_SKILL_ARCHIVE_MEMBERS:
                        _update_fingerprint_digest(digest, "member-limit-exceeded")
                    else:
                        instructions: list[zipfile.ZipInfo] = []
                        for info in inventory:
                            _update_fingerprint_digest(
                                digest,
                                info.filename,
                                info.is_dir(),
                                info.CRC,
                                info.file_size,
                                info.compress_size,
                                info.compress_type,
                                info.flag_bits,
                                info.external_attr,
                            )
                            if (
                                not info.is_dir()
                                and PurePosixPath(info.filename).name.lower()
                                == SKILL_MARKDOWN_LOWER
                            ):
                                instructions.append(info)
                        if len(instructions) == 1:
                            info = instructions[0]
                            if info.file_size <= MAX_FILE_READ_BYTES:
                                data = _read_archive_info(archive, info)
                                _update_fingerprint_digest(
                                    digest,
                                    "instructions-sha256",
                                    hashlib.sha256(data).hexdigest(),
                                )
                            else:
                                _update_fingerprint_digest(
                                    digest,
                                    "instructions-size-limit-exceeded",
                                )
                    content_digest = digest.hexdigest()
            except (
                RuntimeError,
                SkillValidationError,
                NotImplementedError,
                zipfile.BadZipFile,
                zipfile.LargeZipFile,
            ):
                content_digest = _bounded_archive_fallback_digest(
                    handle,
                    expected_identity.size or 0,
                )
            _verify_discovered_file(
                handle,
                path,
                root,
                expected_identity,
            )
    except _SkillFilesystemChanged:
        raise
    except OSError as exc:
        raise _SkillFilesystemChanged(
            str(path),
            "Skill archive changed while its fingerprint was being built.",
        ) from exc
    return content_digest


def _snapshot_source_digest(
    path: Path,
    root: Path,
    path_stat: os.stat_result,
) -> tuple[str, str] | None:
    if not stat.S_ISREG(path_stat.st_mode):
        return None
    expected_identity = _path_identity_from_stat(
        path_stat,
        include_content=True,
    )
    if path.name.lower() == SKILL_MARKDOWN_LOWER:
        if path_stat.st_size > MAX_FILE_READ_BYTES:
            return "instructions", "size-limit-exceeded"
        data, actual_identity = _read_discovered_file(path, root)
        if actual_identity != expected_identity:
            raise _SkillFilesystemChanged(
                str(path),
                "Skill instructions changed while their fingerprint was being built.",
            )
        return "instructions-sha256", hashlib.sha256(data).hexdigest()
    if path.suffix.lower() in {".zip", ".skill"}:
        return (
            "archive-snapshot-sha256",
            _archive_snapshot_source_digest(path, root, expected_identity),
        )
    return None


def _is_archive_fingerprint_candidate(
    path: Path,
    root: Path,
    skill_directories: set[str],
) -> bool:
    if path.suffix.lower() not in {".zip", ".skill"}:
        return False
    root_key = _normalized_path_key(root)
    directory = path.parent
    while True:
        directory_key = _normalized_path_key(directory)
        if directory_key in skill_directories:
            return False
        if directory_key == root_key:
            return True
        parent = directory.parent
        if parent == directory:
            return False
        directory = parent


def _build_roots_fingerprint(
    roots: Iterable[Path],
    *,
    validation_mode: str = SKILL_VALIDATION_STRICT,
) -> tuple[tuple[Any, ...], ...]:
    validation_mode = _validate_validation_mode(validation_mode)
    roots = tuple(roots)
    if len(roots) > MAX_SKILL_ROOTS:
        raise SkillError(f"Skill scan exceeds {MAX_SKILL_ROOTS} configured roots.")

    entries: list[tuple[Any, ...]] = []
    state = _ScanState()
    visited: set[str] = set()
    fingerprint_roots: list[
        tuple[Path, list[tuple[Path, os.stat_result]], set[str]]
    ] = []
    for root in roots:
        state.roots += 1
        try:
            root_stat = root.stat()
            if not stat.S_ISDIR(root_stat.st_mode):
                raise SkillError(
                    f"Skills root {root} does not exist or is not a directory."
                )
            root = root.resolve(strict=True)
        except OSError as exc:
            raise SkillError(
                f"Skills root {root} changed while its fingerprint was being built: {exc}"
            ) from exc
        fingerprint_paths: list[tuple[Path, os.stat_result]] = []
        for path in _bounded_tree_entries(root, state=state, visited=visited):
            try:
                path_stat = path.stat()
            except OSError as exc:
                raise SkillError(
                    f"Skill scan path {path} changed while its fingerprint was being built: {exc}"
                ) from exc
            fingerprint_paths.append((path, path_stat))

        skill_directories = {
            _normalized_path_key(path.parent)
            for path, path_stat in fingerprint_paths
            if stat.S_ISREG(path_stat.st_mode)
            and path.name.lower() == SKILL_MARKDOWN_LOWER
        }
        fingerprint_roots.append((root, fingerprint_paths, skill_directories))

    archive_candidate_paths: set[str] = set()
    archive_bytes = 0
    if validation_mode == SKILL_VALIDATION_COMPATIBLE:
        for root, fingerprint_paths, skill_directories in fingerprint_roots:
            for path, path_stat in fingerprint_paths:
                if not stat.S_ISREG(path_stat.st_mode):
                    continue
                if not _is_archive_fingerprint_candidate(
                    path,
                    root,
                    skill_directories,
                ):
                    continue
                archive_candidate_paths.add(_normalized_path_key(path))
                if (
                    len(archive_candidate_paths)
                    > MAX_SKILL_ARCHIVE_FINGERPRINT_CANDIDATES
                ):
                    raise SkillError(
                        "Skill archive fingerprint exceeds "
                        f"{MAX_SKILL_ARCHIVE_FINGERPRINT_CANDIDATES} candidates."
                    )
                archive_bytes += path_stat.st_size
                if archive_bytes > MAX_SKILL_ARCHIVE_FINGERPRINT_BYTES:
                    raise SkillError(
                        "Skill archive fingerprint exceeds "
                        f"{MAX_SKILL_ARCHIVE_FINGERPRINT_BYTES} cumulative archive bytes."
                    )

    for root, fingerprint_paths, _skill_directories in fingerprint_roots:
        for path, path_stat in fingerprint_paths:
            try:
                rel_path = path.relative_to(root).as_posix()
            except ValueError as exc:
                raise SkillError(
                    f"Skill scan path escapes configured root: {path}"
                ) from exc
            if not rel_path:
                rel_path = "."
            if stat.S_ISDIR(path_stat.st_mode):
                kind = "dir"
            elif stat.S_ISREG(path_stat.st_mode):
                kind = "file"
            else:
                kind = "other"
            archive_candidate = _normalized_path_key(path) in archive_candidate_paths
            if archive_candidate:
                source_digest = _snapshot_source_digest(path, root, path_stat)
            elif path.suffix.lower() in {".zip", ".skill"}:
                source_digest = None
            else:
                source_digest = _snapshot_source_digest(path, root, path_stat)
            entries.append(
                (
                    str(root),
                    rel_path,
                    kind,
                    path_stat.st_mtime_ns,
                    path_stat.st_size,
                    path_stat.st_dev,
                    path_stat.st_ino,
                    path_stat.st_mode,
                    path_stat.st_ctime_ns,
                    getattr(path_stat, "st_file_attributes", 0),
                    source_digest,
                )
            )
    return tuple(entries)


def _bounded_tree_entries(
    root: Path,
    diagnostics: list[SkillDiagnostic] | None = None,
    *,
    state: _ScanState | None = None,
    visited: set[str] | None = None,
    initial_depth: int = 0,
) -> tuple[Path, ...]:
    def raise_limit(source: Path, code: str, message: str) -> None:
        if diagnostics is not None:
            diagnostics.append(SkillDiagnostic(code, "error", str(source), message))
        raise SkillError(message)

    try:
        resolved_root = root.resolve(strict=True)
    except OSError as exc:
        raise SkillError(f"Skill scan cannot resolve {root}: {exc}") from exc
    result: list[Path] = []
    stack: list[tuple[Path, int]] = [(resolved_root, initial_depth)]
    scan_state = state if state is not None else _ScanState()
    visited_directories = visited if visited is not None else set()
    while stack:
        directory, depth = stack.pop()
        if depth > MAX_SKILL_SCAN_DEPTH:
            raise_limit(
                directory,
                "skills.scan.depth_limit",
                f"Skill scan depth exceeds {MAX_SKILL_SCAN_DEPTH}.",
            )
        try:
            resolved_directory = directory.resolve(strict=True)
        except OSError as exc:
            raise SkillError(f"Skill scan cannot resolve {directory}: {exc}") from exc
        if not _path_is_relative_to(resolved_directory, resolved_root):
            raise SkillError(f"Skill scan path escapes configured root: {directory}")
        identity = _normalized_path_key(resolved_directory)
        if identity in visited_directories:
            continue
        visited_directories.add(identity)
        scan_state.directories += 1
        if scan_state.directories > MAX_SKILL_SCAN_DIRECTORIES:
            raise_limit(
                resolved_directory,
                "skills.scan.directory_limit",
                f"Skill scan exceeds {MAX_SKILL_SCAN_DIRECTORIES} directories.",
            )
        result.append(resolved_directory)
        try:
            children = sorted(resolved_directory.iterdir(), key=_path_sort_key)
        except (OSError, PermissionError) as exc:
            raise SkillError(
                f"Skill scan cannot read {resolved_directory}: {exc}"
            ) from exc
        child_directories: list[Path] = []
        for child in children:
            if _is_link_or_junction(child, diagnostics):
                if diagnostics is not None:
                    diagnostics.append(
                        SkillDiagnostic(
                            "skills.scan.link_skipped",
                            "warning",
                            str(child),
                            "Symbolic link or junction was skipped.",
                        )
                    )
                continue
            try:
                resolved_child = child.resolve(strict=True)
                if not _path_is_relative_to(resolved_child, resolved_root):
                    raise SkillError(
                        f"Skill scan path escapes configured root: {child}"
                    )
                child_stat = resolved_child.stat()
                if stat.S_ISDIR(child_stat.st_mode):
                    child_directories.append(resolved_child)
                elif stat.S_ISREG(child_stat.st_mode):
                    scan_state.files += 1
                    if scan_state.files > MAX_SKILL_SCAN_FILES:
                        raise_limit(
                            resolved_child,
                            "skills.scan.file_limit",
                            f"Skill scan exceeds {MAX_SKILL_SCAN_FILES} files.",
                        )
                    result.append(resolved_child)
            except OSError as exc:
                raise SkillError(f"Skill scan cannot inspect {child}: {exc}") from exc
        for child_directory in reversed(child_directories):
            stack.append((child_directory, depth + 1))
    return tuple(result)


def _bounded_tree_files(
    root: Path,
    diagnostics: list[SkillDiagnostic] | None = None,
    *,
    state: _ScanState | None = None,
    visited: set[str] | None = None,
    initial_depth: int = 0,
) -> tuple[Path, ...]:
    files: list[Path] = []
    for path in _bounded_tree_entries(
        root,
        diagnostics,
        state=state,
        visited=visited,
        initial_depth=initial_depth,
    ):
        try:
            path_stat = path.stat()
        except OSError as exc:
            raise SkillError(
                f"Skill scan path {path} changed while files were being collected: {exc}"
            ) from exc
        if stat.S_ISREG(path_stat.st_mode):
            files.append(path)
    return tuple(files)


def _path_sort_key(path: Path) -> tuple[str, str]:
    return path.name.casefold(), path.name


def _absolute_lexical_path(path: Path) -> Path:
    return Path(os.path.abspath(os.fspath(path)))


def _resource_identity_key(rel_path: str) -> str:
    return os.path.normcase(rel_path.replace("/", os.sep))


def _path_identity_from_stat(
    path_stat: os.stat_result,
    *,
    include_content: bool,
) -> _PathIdentity:
    reparse_attribute = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    file_attributes = getattr(path_stat, "st_file_attributes", 0)
    return _PathIdentity(
        device=path_stat.st_dev,
        inode=path_stat.st_ino,
        file_type=stat.S_IFMT(path_stat.st_mode),
        file_attributes=file_attributes & reparse_attribute,
        size=path_stat.st_size if include_content else None,
        modified_ns=path_stat.st_mtime_ns if include_content else None,
        changed_ns=path_stat.st_ctime_ns if include_content else None,
    )


def _open_discovered_file(
    path: Path,
    root: Path,
) -> tuple[Any, _PathIdentity]:
    trusted_path = _absolute_lexical_path(path)
    trusted_root = _absolute_lexical_path(root)
    try:
        _validate_no_reparse_path_chain(trusted_root, trusted_path)
        identity = _capture_path_identity(
            trusted_path,
            require_file=True,
            include_content=True,
        )
    except SkillError as exc:
        raise _SkillFilesystemChanged(
            str(path),
            "Skill source changed while it was being discovered.",
        ) from exc

    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(trusted_path, flags)
    except OSError as exc:
        raise _SkillFilesystemChanged(
            str(path),
            "Skill source changed while it was being opened for discovery.",
        ) from exc
    try:
        handle = os.fdopen(descriptor, "rb", closefd=True)
    except Exception:
        os.close(descriptor)
        raise
    try:
        _verify_stat_identity(os.fstat(handle.fileno()), identity, trusted_path)
    except (OSError, SkillError) as exc:
        handle.close()
        raise _SkillFilesystemChanged(
            str(path),
            "Skill source changed while it was being opened for discovery.",
        ) from exc
    return handle, identity


def _verify_discovered_file(
    handle: Any,
    path: Path,
    root: Path,
    identity: _PathIdentity,
) -> None:
    trusted_path = _absolute_lexical_path(path)
    trusted_root = _absolute_lexical_path(root)
    try:
        _verify_stat_identity(os.fstat(handle.fileno()), identity, trusted_path)
        _validate_no_reparse_path_chain(trusted_root, trusted_path)
        _verify_path_identity(
            trusted_path,
            identity,
            require_file=True,
            include_content=True,
        )
    except (OSError, SkillError) as exc:
        raise _SkillFilesystemChanged(
            str(path),
            "Skill source changed while it was being discovered.",
        ) from exc


def _read_discovered_file(
    path: Path,
    root: Path,
) -> tuple[bytes, _PathIdentity]:
    handle, identity = _open_discovered_file(path, root)
    try:
        with handle:
            data = handle.read(MAX_FILE_READ_BYTES + 1)
            _verify_discovered_file(handle, path, root, identity)
    except _SkillFilesystemChanged:
        raise
    except OSError as exc:
        raise _SkillFilesystemChanged(
            str(path),
            "Skill source changed while it was being read for discovery.",
        ) from exc
    return data, identity


def _capture_discovered_identity(
    path: Path,
    expected: _PathIdentity | None,
) -> _PathIdentity:
    try:
        actual = _capture_path_identity(
            path,
            require_file=True,
            include_content=True,
        )
    except SkillError as exc:
        if expected is None:
            raise
        raise _SkillFilesystemChanged(
            str(path),
            "Skill source changed after it was discovered.",
        ) from exc
    if expected is not None and actual != expected:
        raise _SkillFilesystemChanged(
            str(path),
            "Skill source changed after it was discovered.",
        )
    return actual


def _capture_path_identity(
    path: Path,
    *,
    require_directory: bool = False,
    require_file: bool = False,
    include_content: bool = False,
) -> _PathIdentity:
    try:
        path_stat = path.stat(follow_symlinks=False)
    except OSError as exc:
        raise SkillError(f"Skill source '{path}' could not be inspected") from exc
    if require_directory and not stat.S_ISDIR(path_stat.st_mode):
        raise SkillError(f"Skill source '{path}' is not a directory")
    if require_file and not stat.S_ISREG(path_stat.st_mode):
        raise SkillError(f"Skill source '{path}' is not a regular file")
    return _path_identity_from_stat(
        path_stat,
        include_content=include_content,
    )


def _path_identities_match(
    actual: _PathIdentity,
    expected: _PathIdentity,
    *,
    compare_changed_ns: bool,
) -> bool:
    return (
        actual.device == expected.device
        and actual.inode == expected.inode
        and actual.file_type == expected.file_type
        and actual.file_attributes == expected.file_attributes
        and actual.size == expected.size
        and actual.modified_ns == expected.modified_ns
        and (not compare_changed_ns or actual.changed_ns == expected.changed_ns)
    )


def _verify_stat_identity(
    path_stat: os.stat_result,
    expected: _PathIdentity,
    path: Path,
) -> None:
    actual = _path_identity_from_stat(
        path_stat,
        include_content=expected.size is not None,
    )
    if not _path_identities_match(
        actual,
        expected,
        compare_changed_ns=os.name != "nt",
    ):
        raise SkillError(f"Bound skill source '{path}' changed since discovery")


def _verify_path_identity(
    path: Path,
    expected: _PathIdentity,
    *,
    require_directory: bool = False,
    require_file: bool = False,
    include_content: bool = False,
) -> None:
    actual = _capture_path_identity(
        path,
        require_directory=require_directory,
        require_file=require_file,
        include_content=include_content,
    )
    if actual != expected:
        raise SkillError(f"Bound skill source '{path}' changed since discovery")


def _validate_no_reparse_path_chain(root: Path, target: Path) -> None:
    root = _absolute_lexical_path(root)
    target = _absolute_lexical_path(target)
    try:
        relative = target.relative_to(root)
    except ValueError as exc:
        raise SkillError(
            f"Bound skill source '{target}' escapes configured root"
        ) from exc
    if any(part in {"", ".", ".."} for part in relative.parts):
        raise SkillError(f"Bound skill source '{target}' escapes configured root")

    components = [root]
    current = root
    for part in relative.parts:
        current /= part
        components.append(current)
    reparse_attribute = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    for component in components:
        try:
            path_stat = component.stat(follow_symlinks=False)
            is_junction = getattr(component, "is_junction", None)
            has_link = component.is_symlink() or bool(
                is_junction and is_junction()
            )
        except OSError as exc:
            raise SkillError(
                f"Bound skill source '{component}' could not be inspected"
            ) from exc
        file_attributes = getattr(path_stat, "st_file_attributes", 0)
        if (
            has_link
            or stat.S_ISLNK(path_stat.st_mode)
            or bool(file_attributes & reparse_attribute)
        ):
            raise SkillError(
                f"Bound skill source '{component}' contains a symbolic link or junction"
            )


def _normalized_path_key(path: Path) -> str:
    return os.path.normcase(str(path.resolve(strict=False)))


def _is_link_or_junction(
    path: Path,
    diagnostics: list[SkillDiagnostic] | None = None,
) -> bool:
    try:
        if path.is_symlink():
            return True
        is_junction = getattr(path, "is_junction", None)
        return bool(is_junction and is_junction())
    except OSError as exc:
        error = _SkillFilesystemChanged(
            str(path),
            f"Skill scan cannot inspect {path}: {exc}. "
            "Link or junction metadata could not be inspected during the skill scan.",
        )
        if diagnostics is not None:
            diagnostics.append(
                SkillDiagnostic(
                    error.diagnostic_code,
                    "error",
                    error.source,
                    str(error),
                )
            )
        raise error from exc


def _validate_manifest_validation_mode(
    value: Any,
    manifest_path: Path,
    field_name: str,
) -> None:
    if not isinstance(value, str) or value not in SKILL_VALIDATION_MODES:
        raise SkillError(
            f"Service manifest '{manifest_path}' field '{field_name}' must be "
            "'strict' or 'compatible'."
        )


def _validate_service_manifest_skills(
    data: dict[str, Any],
    manifest_path: Path,
) -> None:
    if "skills_roots" in data:
        raise SkillError(
            f"Service manifest '{manifest_path}' field 'skills_roots' is not "
            "supported; use the top-level 'skillsRoot' field."
        )
    if "skillsRoot" in data and (
        not isinstance(data["skillsRoot"], str) or not data["skillsRoot"].strip()
    ):
        raise SkillError(
            f"Service manifest '{manifest_path}' field 'skillsRoot' must be a "
            "non-empty string."
        )

    if "skills" in data:
        skills_config = data["skills"]
        if not isinstance(skills_config, dict):
            raise SkillError(
                f"Service manifest '{manifest_path}' field 'skills' must be an object."
            )
        if "roots" in skills_config:
            raise SkillError(
                f"Service manifest '{manifest_path}' field 'skills.roots' is not "
                "supported; use the top-level 'skillsRoot' field."
            )
        if set(skills_config) - {"validation_mode"}:
            raise SkillError(
                f"Service manifest '{manifest_path}' field 'skills' contains unknown "
                "properties; only 'validation_mode' is allowed."
            )
        if "validation_mode" in skills_config:
            _validate_manifest_validation_mode(
                skills_config["validation_mode"],
                manifest_path,
                "skills.validation_mode",
            )

    if "skills_validation_mode" in data:
        _validate_manifest_validation_mode(
            data["skills_validation_mode"],
            manifest_path,
            "skills_validation_mode",
        )


def _validate_service_manifest_json_budget(
    data: Any,
    manifest_path: Path,
) -> Any:
    try:
        safe_data, _ = _json_safe_value(
            data,
            source=str(manifest_path),
            field_path="service manifest",
        )
    except SkillValidationError as exc:
        error_message = str(exc)
        budget_category = next(
            (
                category
                for category in (
                    "recursion depth",
                    "visited node",
                    "generated node",
                    "output byte",
                )
                if category in error_message
            ),
            "structural",
        )
        raise SkillError(
            f"Service manifest '{manifest_path}' field 'document' exceeds the "
            f"JSON-safe {budget_category} budget."
        ) from None
    return safe_data


def _read_service_manifest(plugin_dir: Path | None) -> dict[str, Any]:
    if plugin_dir is None:
        return {}
    manifest_path = plugin_dir / "qcopilots_service.json"
    try:
        with manifest_path.open("rb") as handle:
            manifest_bytes = handle.read(MAX_FILE_READ_BYTES + 1)
    except FileNotFoundError:
        return {}
    except OSError as exc:
        raise SkillError(
            f"Service manifest '{manifest_path}' could not be read."
        ) from exc
    if len(manifest_bytes) > MAX_FILE_READ_BYTES:
        raise SkillError(
            f"Service manifest '{manifest_path}' exceeds {MAX_FILE_READ_BYTES} bytes."
        )
    try:
        manifest_text = manifest_bytes.decode("utf-8")
    except UnicodeError as exc:
        raise SkillError(
            f"Service manifest '{manifest_path}' is not valid UTF-8."
        ) from exc

    try:
        data = json.loads(manifest_text)
    except (RecursionError, ValueError) as exc:
        raise SkillError(
            f"Service manifest '{manifest_path}' contains invalid JSON."
        ) from exc
    data = _validate_service_manifest_json_budget(data, manifest_path)
    if not isinstance(data, dict):
        raise SkillError(
            f"Service manifest '{manifest_path}' must contain a top-level JSON object."
        )
    _validate_service_manifest_skills(data, manifest_path)
    return data


def _resolve_config_path(raw: str, plugin_dir: Path | None) -> Path:
    path = Path(raw).expanduser()
    if not path.is_absolute() and plugin_dir is not None:
        path = plugin_dir / path
    return path.resolve(strict=False)


def _path_is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.resolve(strict=True).relative_to(root.resolve(strict=True))
        return True
    except (OSError, ValueError):
        return False


def _dedupe(paths: Iterable[Path]) -> list[Path]:
    result: list[Path] = []
    seen: set[str] = set()
    for path in paths:
        path = path.resolve(strict=False)
        key = _normalized_path_key(path)
        if key not in seen:
            seen.add(key)
            result.append(path)
    return result


def _read_skill_schema() -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "slug": {"type": "string"},
            "path": {"type": "string"},
        },
        "additionalProperties": False,
    }


def _resource_read_schema() -> dict[str, Any]:
    schema = _read_skill_schema()
    schema["properties"]["resource"] = {"type": "string"}
    schema["required"] = ["resource"]
    return schema
