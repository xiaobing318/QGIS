"""Builtin MCP tools compatible with llama-server tool names.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import fnmatch
import os
import re
import subprocess
import time
from collections.abc import Iterable
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from qcopilots_common.constants import (
    BRIDGE_URL_ENV,
    MAX_EXEC_OUTPUT_CHARS,
    MAX_EXEC_TIMEOUT_SECONDS,
    MAX_FILE_READ_BYTES,
    MAX_GLOB_RESULTS,
    MAX_GREP_RESULTS,
    MAX_GREP_SCANNED_FILES,
)
from qcopilots_common.mcp_http import McpTool
from qcopilots_common.subprocess_utils import hidden_subprocess_kwargs


MAX_GREP_SEARCH_SECONDS = 5.0
JUNK_DIRECTORY_NAMES = {
    ".git",
    ".svn",
    ".hg",
    "node_modules",
    "__pycache__",
    ".venv",
    "venv",
    "dist",
    "build",
    "target",
    ".cache",
    ".idea",
    ".vscode",
}


class BuiltinTools:
    """Builtin tool implementation used by tests and service wrappers."""

    def __init__(
        self,
        root_path: str | Path | None = None,
        command_timeout_seconds: float = MAX_EXEC_TIMEOUT_SECONDS,
        allowed_roots: list[str | Path] | None = None,
        allow_full_access: bool = False,
    ):
        self.root_path = Path(root_path or Path.cwd()).expanduser().resolve()
        self.command_timeout_seconds = min(float(command_timeout_seconds), float(MAX_EXEC_TIMEOUT_SECONDS))

    def read_file(self, arguments: dict[str, Any]) -> dict[str, Any]:
        path = self._safe_path(arguments["path"])
        encoding = arguments.get("encoding", "utf-8")
        if (
            "start_line" in arguments
            or "end_line" in arguments
            or arguments.get("append_loc")
        ):
            start_line = max(1, int(arguments.get("start_line", 1)))
            end_line = int(arguments.get("end_line", start_line + MAX_GREP_RESULTS - 1))
            if end_line < start_line:
                raise ValueError("end_line must be greater than or equal to start_line")
            with path.open("rb") as handle:
                data = handle.read(MAX_FILE_READ_BYTES + 1)
            truncated = len(data) > MAX_FILE_READ_BYTES
            text = data[:MAX_FILE_READ_BYTES].decode(encoding, errors="replace")
            lines = text.splitlines()
            if not truncated and path.stat().st_size > len(data):
                truncated = True
            selected = lines[start_line - 1:end_line]
            if end_line < len(lines):
                truncated = True
            total_lines = len(lines)
            if arguments.get("append_loc"):
                selected = [
                    f"{line_number}\u2192 {line}"
                    for line_number, line in enumerate(selected, start_line)
                ]
            content = "\n".join(selected)
            return {
                "path": self._relative(path),
                "content": content,
                "plain_text_response": content,
                "start_line": start_line,
                "end_line": min(end_line, total_lines),
                "total_lines": total_lines,
                "total_lines_known": not truncated,
                "truncated": truncated,
            }

        offset = int(arguments.get("offset", 0))
        limit = max(1, min(int(arguments.get("limit", MAX_FILE_READ_BYTES)), MAX_FILE_READ_BYTES))
        with path.open("rb") as handle:
            handle.seek(max(0, offset))
            data = handle.read(limit + 1)
        truncated = len(data) > limit
        data = data[:limit]
        content = data.decode(encoding, errors="replace")
        return {
            "path": self._relative(path),
            "content": content,
            "plain_text_response": content,
            "truncated": truncated,
        }

    def file_glob_search(self, arguments: dict[str, Any]) -> dict[str, Any]:
        root = self._safe_path(arguments.get("path", arguments.get("root", ".")))
        if not root.is_dir():
            raise NotADirectoryError(f"Glob root must be a directory: {self._relative(root)}")
        include = arguments.get("include", arguments.get("pattern", "**"))
        exclude = arguments.get("exclude", [])
        max_results = min(int(arguments.get("max_results", MAX_GLOB_RESULTS)), MAX_GLOB_RESULTS)
        matches = []
        for path in _iter_files(root, include):
            try:
                self._safe_path(path)
            except PermissionError:
                continue
            if len(matches) >= max_results:
                break
            relative = self._relative(path, root)
            if not _matches_patterns(
                path.name,
                relative,
                exclude,
            ):
                matches.append(relative)
        return {
            "matches": matches,
            "plain_text_response": "\n".join(matches),
            "truncated": len(matches) >= max_results,
        }

    def grep_search(self, arguments: dict[str, Any]) -> dict[str, Any]:
        target = self._safe_path(arguments.get("path", arguments.get("root", ".")))
        if target.is_file():
            files = [(target, self._relative(target))]
        elif target.is_dir():
            include = arguments.get("include", "**")
            files = ((path, self._relative(path, target)) for path in _iter_files(target, include))
        else:
            raise FileNotFoundError(f"Search path does not exist: {self._relative(target)}")
        if "case_sensitive" in arguments:
            case_sensitive = bool(arguments["case_sensitive"])
        else:
            case_sensitive = not bool(arguments.get("ignore_case", False))
        if arguments.get("literal"):
            pattern = None
            literal = arguments["pattern"]
        else:
            pattern = re.compile(
                arguments["pattern"],
                0 if case_sensitive else re.IGNORECASE,
            )
            literal = ""
        exclude = arguments.get("exclude", [])
        context_lines = max(0, int(arguments.get("context_lines", 0)))
        return_line_numbers = bool(arguments.get("return_line_numbers", False))
        max_results = min(int(arguments.get("max_results", MAX_GREP_RESULTS)), MAX_GREP_RESULTS)
        max_scanned_files = min(
            max(1, int(arguments.get("max_scanned_files", MAX_GREP_SCANNED_FILES))),
            MAX_GREP_SCANNED_FILES,
        )
        matches = []
        scanned_files = 0
        skipped_files = 0
        skipped_large_files = 0
        scan_truncated = False
        timed_out = False
        deadline = time.monotonic() + min(
            max(0.01, float(arguments.get("timeout_seconds", MAX_GREP_SEARCH_SECONDS))),
            MAX_GREP_SEARCH_SECONDS,
        )
        for path, relative in files:
            if time.monotonic() >= deadline:
                timed_out = True
                scan_truncated = True
                break
            try:
                path = self._safe_path(path)
            except PermissionError:
                skipped_files += 1
                continue
            if _matches_patterns(path.name, relative, exclude):
                continue
            if len(matches) >= max_results:
                break
            if scanned_files >= max_scanned_files:
                scan_truncated = True
                break
            scanned_files += 1
            try:
                if path.stat().st_size > MAX_FILE_READ_BYTES:
                    skipped_large_files += 1
                    continue
                lines = path.read_text(encoding=arguments.get("encoding", "utf-8"), errors="replace").splitlines()
            except OSError:
                skipped_files += 1
                continue
            for line_number, line in enumerate(lines, 1):
                if time.monotonic() >= deadline:
                    timed_out = True
                    scan_truncated = True
                    break
                haystack = line if case_sensitive else line.lower()
                needle = literal if case_sensitive else literal.lower()
                matched = needle in haystack if pattern is None else bool(pattern.search(line))
                if matched:
                    entry = {"path": relative, "line_number": line_number, "line": line}
                    if context_lines:
                        start = max(1, line_number - context_lines)
                        end = min(len(lines), line_number + context_lines)
                        entry["context"] = [
                            {
                                "line_number": context_number,
                                "line": lines[context_number - 1],
                            }
                            for context_number in range(start, end + 1)
                        ]
                    matches.append(entry)
                    if len(matches) >= max_results:
                        break
            if timed_out:
                break
        if return_line_numbers or context_lines:
            plain_text = "\n".join(
                f"{match['path']}:{match['line_number']}:{match['line']}" for match in matches
            )
        else:
            plain_text = "\n".join(f"{match['path']}:{match['line']}" for match in matches)
        return {
            "matches": matches,
            "plain_text_response": plain_text,
            "truncated": len(matches) >= max_results or scan_truncated or bool(skipped_files or skipped_large_files),
            "timed_out": timed_out,
            "scanned_files": scanned_files,
            "skipped_files": skipped_files,
            "skipped_large_files": skipped_large_files,
        }

    def exec_shell_command(self, arguments: dict[str, Any]) -> dict[str, Any]:
        cwd = self._safe_path(arguments.get("cwd", "."))
        if not cwd.is_dir():
            raise NotADirectoryError(f"cwd must be a directory: {self._relative(cwd)}")
        timeout = min(
            float(arguments.get("timeout_seconds", arguments.get("timeout", self.command_timeout_seconds))),
            self.command_timeout_seconds,
        )
        max_output_size = min(
            max(0, int(arguments.get("max_output_size", MAX_EXEC_OUTPUT_CHARS))),
            MAX_EXEC_OUTPUT_CHARS,
        )
        command = arguments["command"]
        try:
            completed = subprocess.run(
                command,
                shell=isinstance(command, str),
                cwd=str(cwd),
                env=_scrubbed_shell_environment(),
                text=True,
                capture_output=True,
                timeout=max(timeout, 0.01),
                check=False,
                **hidden_subprocess_kwargs(),
            )
            return {
                "exit_code": completed.returncode,
                "timed_out": False,
                "stdout": _truncate(completed.stdout, max_output_size),
                "stderr": _truncate(completed.stderr, max_output_size),
            }
        except subprocess.TimeoutExpired as err:
            return {
                "exit_code": -1,
                "timed_out": True,
                "stdout": _truncate(err.stdout or "", max_output_size),
                "stderr": _truncate(err.stderr or "", max_output_size),
            }

    def write_file(self, arguments: dict[str, Any]) -> dict[str, Any]:
        path = self._safe_path(arguments["path"])
        create_dirs = bool(arguments.get("create_dirs", True))
        overwrite = bool(arguments.get("overwrite", True))
        if not path.parent.exists():
            if not create_dirs:
                raise FileNotFoundError(f"Parent directory does not exist: {self._relative(path.parent)}")
            path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists() and not overwrite:
            raise FileExistsError(f"File already exists: {self._relative(path)}")
        encoding = arguments.get("encoding", "utf-8")
        content = arguments.get("content", "")
        with path.open("w", encoding=encoding, newline="\n") as handle:
            handle.write(content)
        return {"path": self._relative(path), "bytes_written": len(content.encode(encoding))}

    def edit_file(self, arguments: dict[str, Any]) -> dict[str, Any]:
        path = self._safe_path(arguments["path"])
        text = path.read_text(encoding=arguments.get("encoding", "utf-8"))
        edits = arguments.get("edits")
        if edits is None:
            edits = [
                {
                    "old_text": arguments.get("old_text", arguments.get("search")),
                    "new_text": arguments.get("new_text", arguments.get("replace", "")),
                }
            ]
        replacements = _resolve_edits(text, edits)
        expected_replacements = arguments.get("expected_replacements")
        if expected_replacements is not None and len(replacements) != int(expected_replacements):
            raise ValueError(
                f"Expected {expected_replacements} replacement(s), found {len(replacements)}"
            )
        edited = text
        for start, end, new_text in reversed(replacements):
            edited = edited[:start] + new_text + edited[end:]
        with path.open("w", encoding=arguments.get("encoding", "utf-8"), newline="\n") as handle:
            handle.write(edited)
        return {"path": self._relative(path), "replacements": len(replacements)}

    def get_datetime(self, arguments: dict[str, Any]) -> dict[str, Any]:
        timezone_name = arguments.get("timezone")
        if timezone_name is None and arguments.get("utc"):
            timezone_name = "UTC"
        timezone_name = timezone_name or "local"
        if timezone_name.upper() == "UTC":
            value = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
            return {"iso": value, "timezone": "UTC"}
        value = datetime.now().astimezone().replace(microsecond=0).isoformat()
        return {"iso": value, "timezone": timezone_name}

    def _safe_path(self, value: str | Path) -> Path:
        candidate = Path(value).expanduser()
        if not candidate.is_absolute():
            candidate = self.root_path / candidate
        candidate = candidate.resolve()
        return candidate

    def _relative(self, path: Path, root: Path | None = None) -> str:
        base = (root or self.root_path).resolve()
        try:
            return path.resolve().relative_to(base).as_posix()
        except ValueError:
            return path.resolve().as_posix()


def build_builtin_tools(root_path: str | Path | None = None) -> list[McpTool]:
    default_root = Path(root_path).expanduser().resolve() if root_path else Path.cwd().resolve()
    tools = BuiltinTools(default_root)
    return [
        McpTool(
            "read_file",
            "Read a UTF-8 text file. Relative paths resolve from the current base directory; "
            "absolute paths are allowed.",
            _read_file_schema(),
            tools.read_file,
        ),
        McpTool(
            "file_glob_search",
            "Find files using a glob pattern. Relative roots resolve from the current base directory; "
            "absolute roots are allowed.",
            _glob_schema(),
            tools.file_glob_search,
        ),
        McpTool(
            "grep_search",
            "Search text files. Relative roots resolve from the current base directory; "
            "absolute roots are allowed.",
            _grep_schema(),
            tools.grep_search,
        ),
        McpTool(
            "exec_shell_command",
            "Run a bounded shell command. Relative cwd resolves from the current base directory; "
            "absolute cwd values are allowed.",
            _exec_schema(),
            tools.exec_shell_command,
        ),
        McpTool(
            "write_file",
            "Write a UTF-8 text file. Relative paths resolve from the current base directory; "
            "absolute paths are allowed.",
            _write_schema(),
            tools.write_file,
        ),
        McpTool(
            "edit_file",
            "Replace text in a UTF-8 file. Relative paths resolve from the current base directory; "
            "absolute paths are allowed.",
            _edit_schema(),
            tools.edit_file,
        ),
        McpTool("get_datetime", "Return the current date and time.", _datetime_schema(), tools.get_datetime),
    ]


def read_file(arguments: dict[str, Any]) -> dict[str, Any]:
    return BuiltinTools(_default_root()).read_file(arguments)


def file_glob_search(arguments: dict[str, Any]) -> dict[str, Any]:
    return BuiltinTools(_default_root()).file_glob_search(arguments)


def grep_search(arguments: dict[str, Any]) -> dict[str, Any]:
    return BuiltinTools(_default_root()).grep_search(arguments)


def exec_shell_command(arguments: dict[str, Any]) -> dict[str, Any]:
    return BuiltinTools(_default_root()).exec_shell_command(arguments)


def write_file(arguments: dict[str, Any]) -> dict[str, Any]:
    return BuiltinTools(_default_root()).write_file(arguments)


def edit_file(arguments: dict[str, Any]) -> dict[str, Any]:
    return BuiltinTools(_default_root()).edit_file(arguments)


def get_datetime(arguments: dict[str, Any]) -> dict[str, Any]:
    return BuiltinTools(_default_root()).get_datetime(arguments)


def _default_root() -> Path:
    return Path.cwd().resolve()


def _coerce_patterns(patterns: str | list[str] | None) -> list[str]:
    if patterns is None:
        return []
    if isinstance(patterns, str):
        return [patterns] if patterns else []
    return [pattern for pattern in patterns if pattern]


def _matches_patterns(name: str, relative_path: str, patterns: str | list[str] | None) -> bool:
    items = _coerce_patterns(patterns)
    if not items:
        return False
    for pattern in items:
        if "/" in pattern or "\\" in pattern:
            normalized = pattern.replace("\\", "/")
            if fnmatch.fnmatch(relative_path, normalized):
                return True
            if not normalized.startswith(("**/", "/")) and fnmatch.fnmatch(relative_path, f"**/{normalized}"):
                return True
        elif fnmatch.fnmatch(name, pattern) or fnmatch.fnmatch(relative_path, pattern):
            return True
    return False


def _iter_files(root: Path, include: str | list[str]) -> Iterable[Path]:
    root_resolved = root.resolve()
    visited_dirs = set()
    stack = [root]
    while stack:
        directory = stack.pop()
        try:
            directory_resolved = directory.resolve()
        except OSError:
            continue
        if directory_resolved in visited_dirs:
            continue
        visited_dirs.add(directory_resolved)
        try:
            children = sorted(directory.iterdir(), key=lambda item: item.name.lower())
        except OSError:
            continue
        for path in children:
            if path.is_dir() and not _is_reparse_directory(path):
                if path.name in JUNK_DIRECTORY_NAMES:
                    continue
                try:
                    path_resolved = path.resolve()
                    path_resolved.relative_to(root_resolved)
                except (OSError, ValueError):
                    continue
                if path_resolved in visited_dirs:
                    continue
                stack.append(path)
                continue
            relative = path.relative_to(root).as_posix()
            if path.is_file() and _matches_patterns(path.name, relative, include):
                yield path


def _is_reparse_directory(path: Path) -> bool:
    is_junction = getattr(path, "is_junction", None)
    return path.is_symlink() or bool(is_junction and is_junction())


def _resolve_edits(text: str, edits: list[dict[str, Any]]) -> list[tuple[int, int, str]]:
    replacements: list[tuple[int, int, str]] = []
    for edit in edits:
        old_text = edit.get("old_text", edit.get("search"))
        new_text = edit.get("new_text", edit.get("replace", ""))
        if not old_text:
            raise ValueError("Each edit requires old_text or search")
        start = text.find(old_text)
        if start < 0:
            raise ValueError("Search text was not found")
        if text.find(old_text, start + len(old_text)) >= 0:
            raise ValueError("Search text must match exactly one location")
        replacements.append((start, start + len(old_text), new_text))

    replacements.sort(key=lambda replacement: replacement[0])
    previous_end = -1
    for start, end, _ in replacements:
        if start < previous_end:
            raise ValueError("Edits must not overlap")
        previous_end = end
    return replacements


def _truncate(text: str | bytes, limit: int = MAX_EXEC_OUTPUT_CHARS) -> str:
    if isinstance(text, bytes):
        text = text.decode(errors="replace")
    if len(text) <= limit:
        return text
    return text[:limit]


def _scrubbed_shell_environment() -> dict[str, str]:
    blocked_names = {
        BRIDGE_URL_ENV,
    }
    environment = {}
    for key, value in os.environ.items():
        upper_key = key.upper()
        if key in blocked_names:
            continue
        if any(token in upper_key for token in ("TOKEN", "PASSWORD", "SECRET", "API_KEY", "PRIVATE_KEY")):
            continue
        environment[key] = value
    return environment


def _base_schema(properties: dict[str, Any], required: list[str]) -> dict[str, Any]:
    return {"type": "object", "properties": properties, "required": required, "additionalProperties": False}


def _read_file_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "path": {"type": "string"},
            "offset": {"type": "integer", "minimum": 0},
            "limit": {"type": "integer", "minimum": 1, "maximum": MAX_FILE_READ_BYTES},
            "start_line": {"type": "integer", "minimum": 1},
            "end_line": {"type": "integer", "minimum": 1},
            "append_loc": {"type": "boolean", "default": False},
            "encoding": {"type": "string", "default": "utf-8"},
        },
        ["path"],
    )


def _glob_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "path": {"type": "string", "default": "."},
            "root": {"type": "string", "default": "."},
            "pattern": {"type": "string", "default": "**"},
            "include": {
                "oneOf": [
                    {"type": "string"},
                    {"type": "array", "items": {"type": "string"}},
                ],
                "default": "**",
            },
            "exclude": {
                "oneOf": [
                    {"type": "string"},
                    {"type": "array", "items": {"type": "string"}},
                ],
                "default": [],
            },
            "max_results": {"type": "integer", "minimum": 1, "maximum": MAX_GLOB_RESULTS},
        },
        [],
    )


def _grep_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "path": {"type": "string", "default": "."},
            "root": {"type": "string", "default": "."},
            "pattern": {"type": "string"},
            "include": {"type": "string", "default": "**"},
            "exclude": {
                "oneOf": [
                    {"type": "string"},
                    {"type": "array", "items": {"type": "string"}},
                ],
                "default": [],
            },
            "ignore_case": {"type": "boolean", "default": False},
            "case_sensitive": {"type": "boolean", "default": True},
            "literal": {"type": "boolean", "default": False},
            "context_lines": {"type": "integer", "minimum": 0, "default": 0},
            "return_line_numbers": {"type": "boolean", "default": False},
            "max_results": {"type": "integer", "minimum": 1, "maximum": MAX_GREP_RESULTS},
            "max_scanned_files": {"type": "integer", "minimum": 1, "maximum": MAX_GREP_SCANNED_FILES},
            "timeout_seconds": {"type": "number", "minimum": 0.01, "maximum": MAX_GREP_SEARCH_SECONDS},
        },
        ["pattern"],
    )


def _exec_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "command": {"oneOf": [{"type": "string"}, {"type": "array", "items": {"type": "string"}}]},
            "cwd": {"type": "string", "default": "."},
            "timeout_seconds": {"type": "integer", "minimum": 1, "maximum": MAX_EXEC_TIMEOUT_SECONDS},
            "timeout": {"type": "integer", "minimum": 1, "maximum": MAX_EXEC_TIMEOUT_SECONDS},
            "max_output_size": {"type": "integer", "minimum": 0, "maximum": MAX_EXEC_OUTPUT_CHARS},
        },
        ["command"],
    )


def _write_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "path": {"type": "string"},
            "content": {"type": "string"},
            "encoding": {"type": "string", "default": "utf-8"},
            "create_dirs": {"type": "boolean", "default": True},
            "overwrite": {"type": "boolean", "default": True},
        },
        ["path", "content"],
    )


def _edit_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "path": {"type": "string"},
            "search": {"type": "string"},
            "replace": {"type": "string"},
            "old_text": {"type": "string"},
            "new_text": {"type": "string"},
            "edits": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "old_text": {"type": "string"},
                        "new_text": {"type": "string"},
                        "search": {"type": "string"},
                        "replace": {"type": "string"},
                    },
                    "additionalProperties": False,
                },
            },
            "encoding": {"type": "string", "default": "utf-8"},
            "expected_replacements": {"type": "integer", "minimum": 0},
        },
        ["path"],
    )


def _datetime_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "timezone": {"type": "string", "default": "local"},
            "utc": {"type": "boolean", "default": False},
        },
        [],
    )
