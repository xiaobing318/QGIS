"""Builtin MCP tools compatible with llama-server tool names.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import fnmatch
import hashlib
import locale
import os
import re
import shutil
import signal
import stat
import subprocess
import tempfile
import threading
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
from qcopilots_common.mcp_http import McpTool, ToolError
from qcopilots_common.security_policy import (
    FilesystemPolicy,
    filesystem_policy_from_environment,
)
from qcopilots_common.subprocess_utils import hidden_subprocess_kwargs


MAX_GREP_SEARCH_SECONDS = 5.0
MAX_GREP_PATTERN_CHARS = 512
MAX_GREP_REGEX_LINE_CHARS = 64 * 1024
MAX_GREP_REGEX_AST_NODES = 256
MAX_SEARCH_SCANNED_ENTRIES = 20_000
PIPE_CAPTURE_GRACE_SECONDS = 0.25
FILE_SIGNATURE_BYTES = 16
BUILTIN_ALLOWED_ROOTS_ENV = "QCOPILOTS_BUILTIN_ALLOWED_ROOTS"
BUILTIN_ALLOW_FULL_ACCESS_ENV = "QCOPILOTS_BUILTIN_ALLOW_FULL_ACCESS"
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


class _FileSizeLimitError(ValueError):
    pass


class BuiltinTools:
    """Builtin tool implementation used by tests and service wrappers."""

    def __init__(
        self,
        root_path: str | Path | None = None,
        command_timeout_seconds: float = MAX_EXEC_TIMEOUT_SECONDS,
        allowed_roots: list[str | Path] | None = None,
        allow_full_access: bool = False,
        filesystem_policy: FilesystemPolicy | None = None,
    ):
        self.root_path = Path(root_path or Path.cwd()).expanduser().resolve()
        self.command_timeout_seconds = min(float(command_timeout_seconds), float(MAX_EXEC_TIMEOUT_SECONDS))
        self.allow_full_access = bool(allow_full_access)
        self.filesystem_policy = filesystem_policy or FilesystemPolicy()
        self.allowed_roots = tuple(
            dict.fromkeys(
                Path(root).expanduser().resolve()
                for root in (allowed_roots or [])
            )
        )
        self._write_lock = threading.RLock()

    def read_file(self, arguments: dict[str, Any]) -> dict[str, Any]:
        path = self._safe_path(arguments["path"], access="read")
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
            data, file_size = _read_verified_bytes(
                path,
                offset=0,
                read_limit=MAX_FILE_READ_BYTES + 1,
            )
            truncated = len(data) > MAX_FILE_READ_BYTES
            text = data[:MAX_FILE_READ_BYTES].decode(encoding, errors="replace")
            lines = text.splitlines()
            if not truncated and file_size > len(data):
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
        data, _file_size = _read_verified_bytes(
            path,
            offset=max(0, offset),
            read_limit=limit + 1,
        )
        truncated = len(data) > limit
        data = data[:limit]
        content = data.decode(encoding, errors="replace")
        return {
            "path": self._relative(path),
            "content": content,
            "plain_text_response": content,
            "truncated": truncated,
        }

    def get_file_metadata(self, arguments: dict[str, Any]) -> dict[str, Any]:
        path = self._safe_path(arguments["path"], access="read")
        file_size, signature, sha256 = _read_verified_file_metadata(path)
        return {
            "path": self._relative(path),
            "bytes": file_size,
            "extension": path.suffix.lower(),
            "signature_hex": signature.hex(),
            "sha256": sha256,
        }

    def copy_file(self, arguments: dict[str, Any]) -> dict[str, Any]:
        with self._write_lock:
            source = self._safe_path(arguments["source"], access="read")
            target = self._safe_path(arguments["target"], access="write")
            if source == target:
                raise ValueError("copy_file source and target must be different")
            expected_sha256 = _expected_sha256(
                arguments["expected_source_sha256"]
            )
            if expected_sha256 is None:
                raise ValueError("expected_source_sha256 is required")
            if target.exists():
                raise FileExistsError(
                    f"File already exists: {self._relative(target)}"
                )
            if not target.parent.exists():
                if not bool(arguments.get("create_dirs", True)):
                    raise FileNotFoundError(
                        "Parent directory does not exist: "
                        f"{self._relative(target.parent)}"
                    )
                target.parent.mkdir(parents=True, exist_ok=True)
            bytes_copied, signature, sha256 = _copy_file_no_clobber(
                source,
                target,
                expected_source_sha256=expected_sha256,
            )
            return {
                "source": self._relative(source),
                "target": self._relative(target),
                "bytes_copied": bytes_copied,
                "signature_hex": signature.hex(),
                "sha256": sha256,
                "cleanup": {
                    "complete": True,
                    "residual_paths": [],
                    "retry_recommended": False,
                },
            }

    def file_glob_search(self, arguments: dict[str, Any]) -> dict[str, Any]:
        root = self._safe_path(
            arguments.get("path", arguments.get("root", ".")),
            access="read",
        )
        if not root.is_dir():
            raise NotADirectoryError(f"Glob root must be a directory: {self._relative(root)}")
        include = arguments.get("include", arguments.get("pattern", "**"))
        exclude = arguments.get("exclude", [])
        max_results = min(int(arguments.get("max_results", MAX_GLOB_RESULTS)), MAX_GLOB_RESULTS)
        deadline = time.monotonic() + min(
            max(0.01, float(arguments.get("timeout_seconds", MAX_GREP_SEARCH_SECONDS))),
            MAX_GREP_SEARCH_SECONDS,
        )
        traversal_budget = _TraversalBudget(
            max_entries=min(
                max(
                    1,
                    int(
                        arguments.get(
                            "max_scanned_entries",
                            MAX_SEARCH_SCANNED_ENTRIES,
                        )
                    ),
                ),
                MAX_SEARCH_SCANNED_ENTRIES,
            ),
            deadline=deadline,
        )
        matches = []
        for path in _iter_files(root, include, traversal_budget):
            try:
                self._safe_path(path, access="read")
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
            "truncated": (
                len(matches) >= max_results
                or traversal_budget.exhausted
                or traversal_budget.timed_out
            ),
            "timed_out": traversal_budget.timed_out,
            "scanned_entries": traversal_budget.entries,
        }

    def grep_search(self, arguments: dict[str, Any]) -> dict[str, Any]:
        target = self._safe_path(
            arguments.get("path", arguments.get("root", ".")),
            access="read",
        )
        deadline = time.monotonic() + min(
            max(0.01, float(arguments.get("timeout_seconds", MAX_GREP_SEARCH_SECONDS))),
            MAX_GREP_SEARCH_SECONDS,
        )
        traversal_budget = _TraversalBudget(
            max_entries=min(
                max(
                    1,
                    int(
                        arguments.get(
                            "max_scanned_entries",
                            MAX_SEARCH_SCANNED_ENTRIES,
                        )
                    ),
                ),
                MAX_SEARCH_SCANNED_ENTRIES,
            ),
            deadline=deadline,
        )
        if target.is_file():
            traversal_budget.consume()
            files = [(target, self._relative(target))]
        elif target.is_dir():
            include = arguments.get("include", "**")
            files = (
                (path, self._relative(path, target))
                for path in _iter_files(target, include, traversal_budget)
            )
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
            pattern = _compile_safe_regex(
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
        skipped_long_lines = 0
        scan_truncated = False
        timed_out = False
        for path, relative in files:
            if time.monotonic() >= deadline:
                timed_out = True
                scan_truncated = True
                break
            try:
                path = self._safe_path(path, access="read")
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
                data, _file_size = _read_verified_bytes(
                    path,
                    offset=0,
                    read_limit=MAX_FILE_READ_BYTES + 1,
                    reject_oversize=True,
                )
                lines = data.decode(
                    arguments.get("encoding", "utf-8"),
                    errors="replace",
                ).splitlines()
            except _FileSizeLimitError:
                skipped_large_files += 1
                continue
            except (OSError, RuntimeError):
                skipped_files += 1
                continue
            for line_number, line in enumerate(lines, 1):
                if time.monotonic() >= deadline:
                    timed_out = True
                    scan_truncated = True
                    break
                haystack = line if case_sensitive else line.lower()
                needle = literal if case_sensitive else literal.lower()
                if pattern is not None and len(line) > MAX_GREP_REGEX_LINE_CHARS:
                    skipped_long_lines += 1
                    scan_truncated = True
                    continue
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
        if traversal_budget.exhausted:
            scan_truncated = True
        if traversal_budget.timed_out:
            timed_out = True
            scan_truncated = True
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
            "skipped_long_lines": skipped_long_lines,
            "scanned_entries": traversal_budget.entries,
        }

    def exec_shell_command(self, arguments: dict[str, Any]) -> dict[str, Any]:
        cwd = self._safe_path(arguments.get("cwd", "."), access="shell")
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
        command = _validate_shell_command(
            command,
            cwd,
            self.filesystem_policy,
        )
        process = None
        stdout_capture = None
        stderr_capture = None
        windows_job = None
        process_scope_active = False
        timed_out = False
        terminated = True
        termination_error = None
        scope_cleanup_error = None
        try:
            popen_kwargs: dict[str, Any] = hidden_subprocess_kwargs()
            windows_process_suspended = False
            if os.name == "nt":
                create_suspended = getattr(subprocess, "CREATE_SUSPENDED", 0x00000004)
                popen_kwargs["creationflags"] = (
                    int(popen_kwargs.get("creationflags", 0)) | create_suspended
                )
                windows_process_suspended = True
            else:
                popen_kwargs["start_new_session"] = True
            process = subprocess.Popen(
                command,
                shell=isinstance(command, str),
                cwd=str(cwd),
                env=_scrubbed_shell_environment(),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                **popen_kwargs,
            )
            windows_job, job_error = _create_windows_kill_job(process)
            if job_error:
                terminated, termination_error = _terminate_builtin_process_tree(process)
                raise RuntimeError(
                    f"Could not establish a bounded process tree: {job_error}"
                )
            process_scope_active = True
            if windows_process_suspended:
                resume_error = _resume_suspended_windows_process(process)
                if resume_error:
                    raise RuntimeError(
                        "Could not resume the bounded process tree: "
                        f"{resume_error}"
                    )
            stdout_capture = _start_bounded_pipe_capture(
                process.stdout, max_output_size, "stdout"
            )
            stderr_capture = _start_bounded_pipe_capture(
                process.stderr, max_output_size, "stderr"
            )
            try:
                process.wait(timeout=max(timeout, 0.01))
            except subprocess.TimeoutExpired:
                timed_out = True
                scope_cleanup_error = _close_builtin_process_scope(
                    process,
                    windows_job,
                )
                windows_job = None
                process_scope_active = False
                terminated, termination_error = _terminate_builtin_process_tree(process)
        finally:
            if process is not None and process_scope_active:
                scope_cleanup_error = _merge_error_text(
                    scope_cleanup_error,
                    _close_builtin_process_scope(process, windows_job),
                )
                windows_job = None
                process_scope_active = False
            capture_error = _finish_bounded_pipe_captures(
                [stdout_capture, stderr_capture]
            )
            capture_error = _merge_error_text(scope_cleanup_error, capture_error)
        stdout = _bounded_capture_text(stdout_capture)
        stderr = _bounded_capture_text(stderr_capture)
        result = {
            "exit_code": (
                -1
                if timed_out and process is not None and process.returncode == 0
                else process.returncode
                if process is not None and process.returncode is not None
                else -1
            ),
            "timed_out": timed_out,
            "stdout": stdout,
            "stderr": stderr,
            "stdout_truncated": bool(
                stdout_capture and stdout_capture["truncated"]
            ),
            "stderr_truncated": bool(
                stderr_capture and stderr_capture["truncated"]
            ),
            "capture_error": capture_error,
        }
        if timed_out:
            result.update(
                {
                    "process_tree_terminated": terminated,
                    "termination_error": termination_error,
                }
            )
        return result

    def write_file(self, arguments: dict[str, Any]) -> dict[str, Any]:
        with self._write_lock:
            return self._write_file_unlocked(arguments)

    def _write_file_unlocked(self, arguments: dict[str, Any]) -> dict[str, Any]:
        path = self._safe_path(arguments["path"], access="write")
        create_dirs = bool(arguments.get("create_dirs", True))
        overwrite = bool(arguments.get("overwrite", False))
        if not path.parent.exists():
            if not create_dirs:
                raise FileNotFoundError(f"Parent directory does not exist: {self._relative(path.parent)}")
            path.parent.mkdir(parents=True, exist_ok=True)
        encoding = arguments.get("encoding", "utf-8")
        content = arguments.get("content", "")
        data = content.encode(encoding)
        expected_sha256 = _expected_sha256(arguments.get("expected_sha256"))
        previous_sha256 = None
        existed = path.exists()
        if existed:
            if not path.is_file():
                raise IsADirectoryError(f"Write target is not a file: {self._relative(path)}")
            if not overwrite:
                raise FileExistsError(f"File already exists: {self._relative(path)}")
            if expected_sha256 is None:
                raise ValueError(
                    "expected_sha256 is required when overwrite is true for an "
                    "existing file"
                )
            previous_sha256 = _sha256_file(path)
            if expected_sha256 is not None and previous_sha256 != expected_sha256:
                raise ValueError(
                    "expected_sha256 does not match the current file: "
                    f"{self._relative(path)}"
                )
        elif expected_sha256 is not None:
            raise FileNotFoundError(
                "expected_sha256 requires an existing file: "
                f"{self._relative(path)}"
            )
        cleanup_residuals = _atomic_write_bytes(
            path,
            data,
            overwrite=overwrite,
            expected_current_sha256=previous_sha256,
        )
        return {
            "path": self._relative(path),
            "bytes_written": len(data),
            "sha256": _sha256_bytes(data),
            "previous_sha256": previous_sha256,
            "overwritten": existed,
            "cleanup": {
                "complete": not cleanup_residuals,
                "residual_paths": cleanup_residuals,
                "retry_recommended": bool(cleanup_residuals),
            },
        }

    def edit_file(self, arguments: dict[str, Any]) -> dict[str, Any]:
        with self._write_lock:
            return self._edit_file_unlocked(arguments)

    def _edit_file_unlocked(self, arguments: dict[str, Any]) -> dict[str, Any]:
        has_edits = "edits" in arguments
        single_selectors = [
            name for name in ("old_text", "search") if name in arguments
        ]
        if has_edits == bool(single_selectors) or len(single_selectors) > 1:
            raise ValueError(
                "edit_file requires exactly one edit mode: edits, old_text, or search"
            )
        if has_edits and (
            not isinstance(arguments["edits"], list) or not arguments["edits"]
        ):
            raise ValueError("edit_file edits must be a non-empty array")
        path = self._safe_path(arguments["path"], access="write")
        encoding = arguments.get("encoding", "utf-8")
        original, _file_size = _read_verified_bytes(
            path,
            offset=0,
            read_limit=MAX_FILE_READ_BYTES + 1,
            reject_oversize=True,
        )
        previous_sha256 = _sha256_bytes(original)
        expected_sha256 = _expected_sha256(arguments.get("expected_sha256"))
        if expected_sha256 is None:
            raise ValueError("expected_sha256 is required for edit_file")
        if expected_sha256 is not None and previous_sha256 != expected_sha256:
            raise ValueError(
                "expected_sha256 does not match the current file: "
                f"{self._relative(path)}"
            )
        text = original.decode(encoding)
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
        edited_bytes = edited.encode(encoding)
        cleanup_residuals = []
        if edited_bytes != original:
            cleanup_residuals = _atomic_write_bytes(
                path,
                edited_bytes,
                overwrite=True,
                expected_current_sha256=previous_sha256,
            )
        return {
            "path": self._relative(path),
            "replacements": len(replacements),
            "bytes_written": len(edited_bytes),
            "previous_sha256": previous_sha256,
            "sha256": _sha256_bytes(edited_bytes),
            "changed": edited_bytes != original,
            "cleanup": {
                "complete": not cleanup_residuals,
                "residual_paths": cleanup_residuals,
                "retry_recommended": bool(cleanup_residuals),
            },
        }

    def get_datetime(self, arguments: dict[str, Any]) -> dict[str, Any]:
        timezone_name = arguments.get("timezone")
        if timezone_name is None and arguments.get("utc"):
            timezone_name = "UTC"
        timezone_name = timezone_name or "local"
        if timezone_name not in {"local", "UTC"}:
            raise ValueError("timezone must be local or UTC")
        if timezone_name == "UTC":
            value = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
            return {"iso": value, "timezone": "UTC"}
        value = datetime.now().astimezone().replace(microsecond=0).isoformat()
        return {"iso": value, "timezone": "local"}

    def _safe_path(
        self,
        value: str | Path,
        *,
        access: str = "read",
    ) -> Path:
        candidate = self.filesystem_policy.resolve_path(
            value,
            access=access,
            base=self.root_path,
        )
        if (
            access == "shell"
            and self.allowed_roots
            and not self.allow_full_access
            and not any(_path_is_within(candidate, root) for root in self.allowed_roots)
        ):
            raise PermissionError(
                "Path is outside the configured allowed_roots: "
                f"{candidate.as_posix()}"
            )
        return candidate

    def _relative(self, path: Path, root: Path | None = None) -> str:
        base = (root or self.root_path).resolve()
        try:
            return path.resolve().relative_to(base).as_posix()
        except ValueError:
            return path.resolve().as_posix()


def build_builtin_tools(
    root_path: str | Path | None = None,
    *,
    allowed_roots: list[str | Path] | None = None,
    allow_full_access: bool | None = None,
    filesystem_policy: FilesystemPolicy | None = None,
) -> list[McpTool]:
    default_root = Path(root_path).expanduser().resolve() if root_path else Path.cwd().resolve()
    if allowed_roots is None:
        allowed_roots = _configured_allowed_roots(default_root)
    if allow_full_access is None:
        allow_full_access = _configured_full_access()
    if filesystem_policy is None:
        filesystem_policy = filesystem_policy_from_environment()
    tools = BuiltinTools(
        default_root,
        allowed_roots=allowed_roots,
        allow_full_access=allow_full_access,
        filesystem_policy=filesystem_policy,
    )
    shell_scope = (
        "any normal local directory because explicit shell full access is enabled"
        if allow_full_access
        else "the configured shell working-directory roots"
    )
    return [
        McpTool(
            "read_file",
            "Read a UTF-8 text file. Relative paths resolve from the current base directory; "
            "absolute paths may address any normal local file permitted by the OS.",
            _read_file_schema(),
            _builtin_mcp_handler(tools.read_file),
        ),
        McpTool(
            "get_file_metadata",
            "Return the byte size, extension, leading file-signature bytes and full "
            "SHA-256 of a regular file. Relative paths resolve from the current base "
            "directory; absolute paths may address any normal local file permitted by the OS.",
            _get_file_metadata_schema(),
            _builtin_mcp_handler(tools.get_file_metadata),
        ),
        McpTool(
            "copy_file",
            "Copy one regular file without overwriting an existing target. The "
            "source version must be bound by its full SHA-256.",
            _copy_file_schema(),
            _builtin_mcp_handler(tools.copy_file),
        ),
        McpTool(
            "file_glob_search",
            "Find files using a glob pattern. Relative roots resolve from the current base directory; "
            "absolute roots may address any normal local directory permitted by the OS.",
            _glob_schema(),
            _builtin_mcp_handler(tools.file_glob_search),
        ),
        McpTool(
            "grep_search",
            "Search text files. Relative roots resolve from the current base directory; "
            "absolute roots may address any normal local path permitted by the OS.",
            _grep_schema(),
            _builtin_mcp_handler(tools.grep_search),
        ),
        McpTool(
            "exec_shell_command",
            "Run a bounded command from a non-empty argv string array and terminate "
            "its process tree on timeout. Each array item is one argument and shell "
            "quoting is not required. "
            "Relative cwd resolves from the current base directory. "
            f"Absolute cwd values must remain within {shell_scope}. Formal restricted "
            "mode validates the executable allowlist and path arguments.",
            _exec_schema(),
            _builtin_mcp_handler(tools.exec_shell_command),
        ),
        McpTool(
            "write_file",
            "Write a UTF-8 text file. Relative paths resolve from the current base directory; "
            "absolute paths may address any normal local file permitted by the OS.",
            _write_schema(),
            _builtin_mcp_handler(tools.write_file),
        ),
        McpTool(
            "edit_file",
            "Replace text in a UTF-8 file. Relative paths resolve from the current base directory; "
            "absolute paths may address any normal local file permitted by the OS.",
            _edit_schema(),
            _builtin_mcp_handler(tools.edit_file),
        ),
        McpTool(
            "get_datetime",
            "Return the current date and time.",
            _datetime_schema(),
            _builtin_mcp_handler(tools.get_datetime),
        ),
    ]


def _builtin_mcp_handler(handler):
    """Translate expected tool and OS failures to MCP tool errors."""

    def invoke(arguments: dict[str, Any]) -> dict[str, Any]:
        try:
            return handler(arguments)
        except ToolError:
            raise
        except (LookupError, OSError, RuntimeError, UnicodeError, ValueError) as err:
            raise ToolError(str(err)) from err

    return invoke


def read_file(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().read_file(arguments)


def get_file_metadata(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().get_file_metadata(arguments)


def copy_file(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().copy_file(arguments)


def file_glob_search(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().file_glob_search(arguments)


def grep_search(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().grep_search(arguments)


def exec_shell_command(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().exec_shell_command(arguments)


def write_file(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().write_file(arguments)


def edit_file(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().edit_file(arguments)


def get_datetime(arguments: dict[str, Any]) -> dict[str, Any]:
    return _configured_builtin_tools().get_datetime(arguments)


def _default_root() -> Path:
    return Path.cwd().resolve()


def _configured_builtin_tools() -> BuiltinTools:
    root = _default_root()
    return BuiltinTools(
        root,
        allowed_roots=_configured_allowed_roots(root),
        allow_full_access=_configured_full_access(),
        filesystem_policy=filesystem_policy_from_environment(),
    )


def _configured_allowed_roots(default_root: Path) -> list[Path]:
    configured = os.environ.get(BUILTIN_ALLOWED_ROOTS_ENV, "")
    roots = [default_root]
    if configured:
        roots.extend(
            Path(item).expanduser().resolve()
            for item in configured.split(os.pathsep)
            if item.strip()
        )
    return list(dict.fromkeys(roots))


def _configured_full_access() -> bool:
    value = os.environ.get(BUILTIN_ALLOW_FULL_ACCESS_ENV, "").strip().casefold()
    if not value:
        return False
    if value in {"1", "true", "yes", "on"}:
        return True
    if value in {"0", "false", "no", "off"}:
        return False
    raise ValueError(
        f"{BUILTIN_ALLOW_FULL_ACCESS_ENV} must be a boolean value"
    )


def _validate_shell_command(
    command: Any,
    cwd: Path,
    policy: FilesystemPolicy,
) -> str | list[str]:
    if not policy.shell_enabled:
        raise PermissionError("Shell execution is disabled by the security policy")
    if not policy.restricted and not policy.shell_executables:
        return command
    if isinstance(command, str):
        if policy.restricted:
            raise PermissionError(
                "Formal restricted mode rejects string shell commands"
            )
        raise PermissionError(
            "A configured shell executable allowlist requires an array command"
        )
    if not isinstance(command, list) or not command or any(
        not isinstance(item, str) or not item for item in command
    ):
        raise ValueError("command must be a non-empty array of strings")
    if not policy.shell_executables:
        raise PermissionError(
            "Formal restricted mode has no allowed shell executables"
        )

    requested = command[0]
    if "\0" in requested:
        raise PermissionError("Shell executable contains a null byte")
    requested_path = Path(requested).expanduser()
    if requested_path.is_absolute():
        executable = _validated_shell_executable_path(requested_path, policy)
    else:
        located = shutil.which(
            requested,
            path=_scrubbed_shell_environment().get("PATH", ""),
        )
        if not located:
            raise FileNotFoundError(f"Allowed shell executable was not found: {requested}")
        executable = _validated_shell_executable_path(located, policy)

    allowed_names = {
        Path(item).name.casefold()
        for item in policy.shell_executables
        if not Path(item).is_absolute()
    }
    allowed_paths = {
        _validated_shell_executable_path(item, policy)
        for item in policy.shell_executables
        if Path(item).is_absolute()
    }
    if executable not in allowed_paths and executable.name.casefold() not in allowed_names:
        raise PermissionError(
            f"Shell executable is not allowed by the security policy: {requested}"
        )

    if policy.restricted:
        for argument in command[1:]:
            if "\0" in argument:
                raise PermissionError("Shell argument contains a null byte")
            candidate = argument.partition("=")[2] if argument.startswith("-") and "=" in argument else argument
            if candidate.startswith("-"):
                continue
            looks_like_path = (
                Path(candidate).is_absolute()
                or "/" in candidate
                or "\\" in candidate
                or candidate.startswith((".", "~"))
                or bool(Path(candidate).suffix)
                or (cwd / candidate).exists()
                or bool(re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", candidate))
            )
            if looks_like_path:
                policy.resolve_path(candidate, access="shell", base=cwd)
    return [str(executable), *command[1:]]


def _validated_shell_executable_path(
    value: str | Path,
    policy: FilesystemPolicy,
) -> Path:
    executable = policy.resolve_path(value, access="read")
    if not executable.is_file():
        raise FileNotFoundError(f"Allowed shell executable was not found: {value}")
    return executable


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


class _TraversalBudget:
    def __init__(self, *, max_entries: int, deadline: float):
        self.max_entries = max(1, int(max_entries))
        self.deadline = float(deadline)
        self.entries = 0
        self.exhausted = False
        self.timed_out = False

    def check(self) -> bool:
        if self.exhausted or self.timed_out:
            return False
        if time.monotonic() >= self.deadline:
            self.timed_out = True
            return False
        return True

    def consume(self) -> bool:
        if not self.check():
            return False
        if self.entries >= self.max_entries:
            self.exhausted = True
            return False
        self.entries += 1
        return True


def _iter_files(
    root: Path,
    include: str | list[str],
    budget: _TraversalBudget | None = None,
) -> Iterable[Path]:
    if budget is None:
        budget = _TraversalBudget(
            max_entries=MAX_SEARCH_SCANNED_ENTRIES,
            deadline=time.monotonic() + MAX_GREP_SEARCH_SECONDS,
        )
    root_resolved = root.resolve()
    visited_dirs = set()
    stack = [root]
    while stack:
        if not budget.check():
            return
        directory = stack.pop()
        try:
            directory_resolved = directory.resolve()
        except OSError:
            continue
        if directory_resolved in visited_dirs:
            continue
        visited_dirs.add(directory_resolved)
        children = []
        try:
            with os.scandir(directory) as iterator:
                for entry in iterator:
                    if not budget.consume():
                        break
                    children.append(Path(entry.path))
        except OSError:
            continue
        children.sort(key=lambda item: item.name.lower())
        for path in children:
            if time.monotonic() >= budget.deadline:
                budget.timed_out = True
                return
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
        selectors = [name for name in ("old_text", "search") if name in edit]
        if len(selectors) != 1:
            raise ValueError("Each edit requires exactly one of old_text or search")
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


def _compile_safe_regex(value: Any, flags: int) -> re.Pattern[str]:
    pattern_text = str(value)
    if len(pattern_text) > MAX_GREP_PATTERN_CHARS:
        raise ValueError(
            f"Regex pattern exceeds the {MAX_GREP_PATTERN_CHARS} character limit"
        )
    if _contains_unescaped_alternation(pattern_text):
        raise ValueError("Unsafe regex pattern: alternation is not supported")
    try:
        parsed = re._parser.parse(pattern_text, flags)
    except re.error as err:
        raise ValueError(f"Invalid regex pattern: {err}") from err
    _validate_safe_regex_nodes(parsed)
    return re.compile(pattern_text, flags)


def _contains_unescaped_alternation(pattern_text: str) -> bool:
    escaped = False
    inside_character_class = False
    for character in pattern_text:
        if escaped:
            escaped = False
            continue
        if character == "\\":
            escaped = True
            continue
        if character == "[":
            inside_character_class = True
            continue
        if character == "]" and inside_character_class:
            inside_character_class = False
            continue
        if character == "|" and not inside_character_class:
            return True
    return False


def _validate_safe_regex_nodes(
    nodes: Any,
    inside_repeat: bool = False,
    state: dict[str, int] | None = None,
) -> None:
    if state is None:
        state = {"nodes": 0, "repeats": 0}
    unsafe_operations = {
        "ASSERT",
        "ASSERT_NOT",
        "ATOMIC_GROUP",
        "BRANCH",
        "GROUPREF",
        "GROUPREF_EXISTS",
        "GROUPREF_IGNORE",
        "GROUPREF_LOC_IGNORE",
        "GROUPREF_UNI_IGNORE",
    }
    repeat_operations = {"MAX_REPEAT", "MIN_REPEAT", "POSSESSIVE_REPEAT"}
    for operation, argument in nodes:
        state["nodes"] += 1
        if state["nodes"] > MAX_GREP_REGEX_AST_NODES:
            raise ValueError(
                "Unsafe regex pattern: syntax tree exceeds the node budget"
            )
        operation_name = getattr(operation, "name", str(operation))
        if operation_name in unsafe_operations:
            raise ValueError(
                "Unsafe regex pattern: alternation, lookarounds, backreferences, "
                "conditional groups and atomic groups are not supported"
            )
        if operation_name in repeat_operations:
            if inside_repeat:
                raise ValueError(
                    "Unsafe regex pattern: nested repetitions are not supported"
                )
            state["repeats"] += 1
            if state["repeats"] > 1:
                raise ValueError(
                    "Unsafe regex pattern: at most one repetition is supported"
                )
            _minimum, _maximum, child = argument
            _validate_safe_regex_nodes(child, inside_repeat=True, state=state)
        elif operation_name == "SUBPATTERN":
            _validate_safe_regex_nodes(
                argument[-1],
                inside_repeat=inside_repeat,
                state=state,
            )


def _expected_sha256(value: Any) -> str | None:
    if value is None:
        return None
    normalized = str(value).strip().lower()
    if not re.fullmatch(r"[0-9a-f]{64}", normalized):
        raise ValueError("expected_sha256 must contain exactly 64 hexadecimal characters")
    return normalized


def _path_is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _read_verified_bytes(
    path: Path,
    *,
    offset: int,
    read_limit: int,
    reject_oversize: bool = False,
) -> tuple[bytes, int]:
    """Read a bounded slice while rejecting replacement and growth races."""

    before = path.stat()
    if not stat.S_ISREG(before.st_mode):
        raise IsADirectoryError(f"Read target is not a regular file: {path}")
    if reject_oversize and before.st_size > MAX_FILE_READ_BYTES:
        raise _FileSizeLimitError(
            f"File exceeds the {MAX_FILE_READ_BYTES} byte edit limit: {path}"
        )
    with path.open("rb") as handle:
        opened = os.fstat(handle.fileno())
        _require_same_open_file(path, before, opened)
        if reject_oversize and opened.st_size > MAX_FILE_READ_BYTES:
            raise _FileSizeLimitError(
                f"File exceeds the {MAX_FILE_READ_BYTES} byte edit limit: {path}"
            )
        handle.seek(max(0, int(offset)))
        data = handle.read(max(0, int(read_limit)))
        after_read = os.fstat(handle.fileno())
    after_path = path.stat()
    if _file_version(before) != _file_version(after_read):
        raise RuntimeError(f"File changed while it was being read: {path}")
    if _file_version(before) != _file_version(after_path):
        raise RuntimeError(f"File was replaced while it was being read: {path}")
    if reject_oversize and len(data) > MAX_FILE_READ_BYTES:
        raise _FileSizeLimitError(
            f"File exceeds the {MAX_FILE_READ_BYTES} byte edit limit: {path}"
        )
    return data, int(after_read.st_size)


def _read_verified_file_metadata(path: Path) -> tuple[int, bytes, str]:
    """Hash one regular file while rejecting replacement and mutation races."""

    before = path.stat()
    if not stat.S_ISREG(before.st_mode):
        raise IsADirectoryError(f"Metadata target is not a regular file: {path}")
    digest = hashlib.sha256()
    signature = bytearray()
    bytes_read = 0
    with path.open("rb") as handle:
        opened = os.fstat(handle.fileno())
        _require_same_open_file(path, before, opened)
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            if len(signature) < FILE_SIGNATURE_BYTES:
                signature.extend(chunk[: FILE_SIGNATURE_BYTES - len(signature)])
            digest.update(chunk)
            bytes_read += len(chunk)
        after_read = os.fstat(handle.fileno())
    after_path = path.stat()
    if _file_version(before) != _file_version(after_read):
        raise RuntimeError(f"File changed while it was being hashed: {path}")
    if _file_version(before) != _file_version(after_path):
        raise RuntimeError(f"File was replaced while it was being hashed: {path}")
    if bytes_read != int(after_read.st_size):
        raise RuntimeError(f"File size changed while it was being hashed: {path}")
    return bytes_read, bytes(signature), digest.hexdigest()


def _copy_file_no_clobber(
    source: Path,
    target: Path,
    *,
    expected_source_sha256: str,
) -> tuple[int, bytes, str]:
    """Copy a verified source snapshot and atomically create the target."""

    before = source.stat()
    if not stat.S_ISREG(before.st_mode):
        raise IsADirectoryError(f"Copy source is not a regular file: {source}")
    temporary_path = None
    primary_error = None
    try:
        digest = hashlib.sha256()
        signature = bytearray()
        bytes_copied = 0
        with source.open("rb") as source_handle:
            opened = os.fstat(source_handle.fileno())
            _require_same_open_file(source, before, opened)
            with tempfile.NamedTemporaryFile(
                mode="wb",
                prefix=f".{target.name}.qcopilots-copy-",
                suffix=".tmp",
                dir=str(target.parent),
                delete=False,
            ) as target_handle:
                temporary_path = Path(target_handle.name)
                for chunk in iter(lambda: source_handle.read(1024 * 1024), b""):
                    if len(signature) < FILE_SIGNATURE_BYTES:
                        signature.extend(
                            chunk[: FILE_SIGNATURE_BYTES - len(signature)]
                        )
                    digest.update(chunk)
                    target_handle.write(chunk)
                    bytes_copied += len(chunk)
                target_handle.flush()
                os.fsync(target_handle.fileno())
            after_read = os.fstat(source_handle.fileno())
        after_path = source.stat()
        if _file_version(before) != _file_version(after_read):
            raise RuntimeError(f"Copy source changed while being read: {source}")
        if _file_version(before) != _file_version(after_path):
            raise RuntimeError(f"Copy source was replaced while being read: {source}")
        if bytes_copied != int(after_read.st_size):
            raise RuntimeError(f"Copy source size changed while being read: {source}")
        source_sha256 = digest.hexdigest()
        if source_sha256 != expected_source_sha256:
            raise ValueError("expected_source_sha256 does not match the copy source")

        os.chmod(temporary_path, stat.S_IMODE(opened.st_mode))
        copied_bytes, copied_signature, copied_sha256 = _read_verified_file_metadata(
            temporary_path
        )
        if (
            copied_bytes != bytes_copied
            or copied_signature != bytes(signature)
            or copied_sha256 != source_sha256
        ):
            raise RuntimeError("Copied file verification failed before publication")
        _publish_new_file(temporary_path, target)
        temporary_path = None
        return bytes_copied, bytes(signature), source_sha256
    except Exception as err:
        primary_error = err
        raise
    finally:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except Exception as cleanup_error:
                if primary_error is not None:
                    raise RuntimeError(
                        f"{primary_error}. Copy cleanup was incomplete. Residual path: "
                        f"{temporary_path}"
                    ) from primary_error
                raise RuntimeError(
                    f"Copy cleanup was incomplete. Residual path: {temporary_path}"
                ) from cleanup_error


def _require_same_open_file(path: Path, before: os.stat_result, opened: os.stat_result) -> None:
    if _file_identity(before) != _file_identity(opened):
        raise RuntimeError(f"File was replaced before it could be opened: {path}")
    if _file_version(before) != _file_version(opened):
        raise RuntimeError(f"File changed before it could be opened: {path}")


def _file_identity(value: os.stat_result) -> tuple[int, int]:
    return int(value.st_dev), int(value.st_ino)


def _file_version(value: os.stat_result) -> tuple[int, int, int, int]:
    return (
        int(value.st_dev),
        int(value.st_ino),
        int(value.st_size),
        int(getattr(value, "st_mtime_ns", int(value.st_mtime * 1_000_000_000))),
    )


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _atomic_write_bytes(
    path: Path,
    data: bytes,
    *,
    overwrite: bool,
    expected_current_sha256: str | None,
) -> list[str]:
    mode = None
    if path.exists():
        mode = stat.S_IMODE(path.stat().st_mode)
    temporary_path = None
    backup_root = None
    backup_path = None
    cleanup_residuals = []
    primary_error = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=f".{path.name}.qcopilots-",
            suffix=".tmp",
            dir=str(path.parent),
            delete=False,
        ) as handle:
            temporary_path = Path(handle.name)
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        if mode is not None:
            os.chmod(temporary_path, mode)

        if expected_current_sha256 is not None:
            if not path.is_file() or _sha256_file(path) != expected_current_sha256:
                raise RuntimeError(f"File changed before atomic replacement: {path}")
        if overwrite:
            backup_root = Path(
                tempfile.mkdtemp(
                    prefix=f".{path.name}.qcopilots-backup-",
                    dir=str(path.parent),
                )
            )
            backup_path = backup_root / path.name
            _replace_existing_file_with_backup(
                path,
                temporary_path,
                backup_path,
                expected_sha256=expected_current_sha256,
            )
            temporary_path = None
            captured_sha256 = _sha256_file(backup_path)
            if captured_sha256 != expected_current_sha256:
                try:
                    cleanup_residuals.extend(
                        _restore_captured_builtin_version(
                            path,
                            backup_path,
                            backup_root,
                            requested_sha256=_sha256_bytes(data),
                            captured_sha256=captured_sha256,
                        )
                    )
                except Exception:
                    cleanup_residuals.extend(
                        str(candidate)
                        for candidate in (
                            *(
                                sorted(backup_root.iterdir())
                                if backup_root.exists()
                                else []
                            ),
                            backup_root,
                        )
                        if candidate.exists()
                    )
                    raise
                raise RuntimeError(
                    f"File changed during atomic replacement and was preserved: {path}"
                )
            cleanup_residuals.extend(
                _cleanup_builtin_backup(backup_path, backup_root)
            )
        else:
            _publish_new_file(temporary_path, path)
            temporary_path = None
        return sorted(set(cleanup_residuals))
    except Exception as err:
        primary_error = err
        raise
    finally:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except Exception:
                cleanup_residuals.append(str(temporary_path))
        if primary_error is not None and backup_path is not None and backup_path.exists():
            cleanup_residuals.append(str(backup_path))
        if backup_root is not None and not (
            backup_path is not None and backup_path.exists()
        ):
            try:
                backup_root.rmdir()
            except FileNotFoundError:
                pass
            except Exception:
                cleanup_residuals.append(str(backup_root))
        if primary_error is not None and cleanup_residuals:
            raise RuntimeError(
                f"{primary_error}. Output cleanup was incomplete. Residual paths: "
                + ", ".join(sorted(set(cleanup_residuals)))
            ) from primary_error

def _cleanup_builtin_backup(backup_path: Path, backup_root: Path) -> list[str]:
    residuals = []
    try:
        backup_path.unlink(missing_ok=True)
    except Exception:
        residuals.append(str(backup_path))
    try:
        backup_root.rmdir()
    except FileNotFoundError:
        pass
    except Exception:
        residuals.append(str(backup_root))
    return sorted(set(residuals))


def _publish_new_file(temporary: Path, target: Path) -> None:
    """Publish a new file without clobbering an existing path.

    Windows rename is an atomic no-clobber operation and works on ordinary
    removable filesystems which do not implement hard links. POSIX rename may
    overwrite an existing target, so those platforms retain link publication.
    """

    try:
        if os.name == "nt":
            os.rename(temporary, target)
        else:
            os.link(temporary, target)
            temporary.unlink()
    except FileExistsError as err:
        raise FileExistsError(f"File already exists: {target}") from err


def _replace_existing_file_with_backup(
    target: Path,
    replacement: Path,
    backup: Path,
    *,
    expected_sha256: str,
) -> None:
    """Atomically replace target after capturing and revalidating its version."""

    if os.name == "nt":
        _windows_replace_file_with_backup(target, replacement, backup)
        return

    os.link(target, backup)
    try:
        target_stat = target.stat()
        backup_stat = backup.stat()
        if _file_identity(target_stat) != _file_identity(backup_stat):
            raise RuntimeError(
                f"File was replaced before atomic publication: {target}"
            )
        if (
            _sha256_file(backup) != expected_sha256
            or _sha256_file(target) != expected_sha256
        ):
            raise RuntimeError(
                f"File changed before atomic publication: {target}"
            )
        os.replace(replacement, target)
    except Exception:
        backup.unlink(missing_ok=True)
        raise


def _windows_replace_file_with_backup(
    target: Path,
    replacement: Path,
    backup: Path,
) -> None:
    """Atomically replace *target* and capture the exact displaced version."""

    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    replace_file = kernel32.ReplaceFileW
    replace_file.argtypes = [
        wintypes.LPCWSTR,
        wintypes.LPCWSTR,
        wintypes.LPCWSTR,
        wintypes.DWORD,
        wintypes.LPVOID,
        wintypes.LPVOID,
    ]
    replace_file.restype = wintypes.BOOL
    if not replace_file(
        str(target),
        str(replacement),
        str(backup),
        0,
        None,
        None,
    ):
        raise ctypes.WinError(ctypes.get_last_error())


def _restore_captured_builtin_version(
    target: Path,
    captured: Path,
    backup_root: Path,
    *,
    requested_sha256: str,
    captured_sha256: str,
) -> list[str]:
    """Restore a concurrent version captured by the atomic publish operation."""

    candidate = captured
    candidate_sha256 = captured_sha256
    installed_sha256 = requested_sha256
    for attempt in range(16):
        if not target.is_file() or _sha256_file(target) != installed_sha256:
            return _cleanup_builtin_backup(candidate, backup_root)

        displaced = backup_root / f".{target.name}.displaced-{attempt}"
        _replace_existing_file_with_backup(
            target,
            candidate,
            displaced,
            expected_sha256=installed_sha256,
        )
        displaced_sha256 = _sha256_file(displaced)
        if displaced_sha256 == installed_sha256:
            return _cleanup_builtin_backup(displaced, backup_root)

        # Another writer published between the check and the atomic swap. The
        # swap captured that newer version, so make it the next restore candidate.
        installed_sha256 = candidate_sha256
        candidate = displaced
        candidate_sha256 = displaced_sha256

    raise RuntimeError(
        "File kept changing while its concurrent version was being restored"
    )


def _create_windows_kill_job(
    process: subprocess.Popen,
) -> tuple[dict[str, Any] | None, str | None]:
    if os.name != "nt" or not hasattr(process, "_handle"):
        return None, None
    try:
        import ctypes
        from ctypes import wintypes

        class _IoCounters(ctypes.Structure):
            _fields_ = [
                ("ReadOperationCount", ctypes.c_ulonglong),
                ("WriteOperationCount", ctypes.c_ulonglong),
                ("OtherOperationCount", ctypes.c_ulonglong),
                ("ReadTransferCount", ctypes.c_ulonglong),
                ("WriteTransferCount", ctypes.c_ulonglong),
                ("OtherTransferCount", ctypes.c_ulonglong),
            ]

        class _BasicLimitInformation(ctypes.Structure):
            _fields_ = [
                ("PerProcessUserTimeLimit", ctypes.c_longlong),
                ("PerJobUserTimeLimit", ctypes.c_longlong),
                ("LimitFlags", wintypes.DWORD),
                ("MinimumWorkingSetSize", ctypes.c_size_t),
                ("MaximumWorkingSetSize", ctypes.c_size_t),
                ("ActiveProcessLimit", wintypes.DWORD),
                ("Affinity", ctypes.c_size_t),
                ("PriorityClass", wintypes.DWORD),
                ("SchedulingClass", wintypes.DWORD),
            ]

        class _ExtendedLimitInformation(ctypes.Structure):
            _fields_ = [
                ("BasicLimitInformation", _BasicLimitInformation),
                ("IoInfo", _IoCounters),
                ("ProcessMemoryLimit", ctypes.c_size_t),
                ("JobMemoryLimit", ctypes.c_size_t),
                ("PeakProcessMemoryUsed", ctypes.c_size_t),
                ("PeakJobMemoryUsed", ctypes.c_size_t),
            ]

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
        kernel32.CreateJobObjectW.restype = wintypes.HANDLE
        kernel32.SetInformationJobObject.argtypes = [
            wintypes.HANDLE,
            ctypes.c_int,
            ctypes.c_void_p,
            wintypes.DWORD,
        ]
        kernel32.SetInformationJobObject.restype = wintypes.BOOL
        kernel32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
        kernel32.AssignProcessToJobObject.restype = wintypes.BOOL
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel32.CloseHandle.restype = wintypes.BOOL

        handle = kernel32.CreateJobObjectW(None, None)
        if not handle:
            return None, f"CreateJobObjectW failed with Windows error {ctypes.get_last_error()}"
        information = _ExtendedLimitInformation()
        information.BasicLimitInformation.LimitFlags = 0x00002000
        if not kernel32.SetInformationJobObject(
            handle,
            9,
            ctypes.byref(information),
            ctypes.sizeof(information),
        ):
            error = ctypes.get_last_error()
            kernel32.CloseHandle(handle)
            return None, f"SetInformationJobObject failed with Windows error {error}"
        if not kernel32.AssignProcessToJobObject(handle, wintypes.HANDLE(process._handle)):
            error = ctypes.get_last_error()
            kernel32.CloseHandle(handle)
            return None, f"AssignProcessToJobObject failed with Windows error {error}"
        return {"kernel32": kernel32, "handle": handle}, None
    except Exception as err:
        return None, str(err)


def _resume_suspended_windows_process(process: subprocess.Popen) -> str | None:
    """Resume a process only after its kill-on-close job has been assigned."""

    if os.name != "nt" or not hasattr(process, "_handle"):
        return None
    try:
        import ctypes
        from ctypes import wintypes

        ntdll = ctypes.WinDLL("ntdll")
        resume_process = ntdll.NtResumeProcess
        resume_process.argtypes = [wintypes.HANDLE]
        resume_process.restype = ctypes.c_long
        status = int(resume_process(wintypes.HANDLE(process._handle)))
        if status != 0:
            return f"NtResumeProcess failed with NTSTATUS 0x{status & 0xFFFFFFFF:08x}"
        return None
    except Exception as err:
        return str(err)


def _close_builtin_process_scope(
    process: subprocess.Popen,
    windows_job: dict[str, Any] | None,
) -> str | None:
    if os.name == "nt":
        if windows_job is None:
            return None
        try:
            if not windows_job["kernel32"].CloseHandle(windows_job["handle"]):
                import ctypes

                return f"CloseHandle failed with Windows error {ctypes.get_last_error()}"
        except Exception as err:
            return str(err)
        return None
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        return None
    except Exception as err:
        return str(err)
    return None


def _merge_error_text(*values: str | None) -> str | None:
    items = [value for value in values if value]
    return " | ".join(items) or None


def _start_bounded_pipe_capture(
    stream: Any,
    limit: int,
    label: str,
) -> dict[str, Any] | None:
    if stream is None:
        return None
    capture: dict[str, Any] = {
        "stream": stream,
        "data": bytearray(),
        "limit": max(0, int(limit)),
        "truncated": False,
        "error": None,
        "label": label,
    }

    def drain() -> None:
        try:
            while True:
                chunk = stream.read(64 * 1024)
                if not chunk:
                    break
                if isinstance(chunk, str):
                    chunk = chunk.encode(
                        locale.getpreferredencoding(False), errors="replace"
                    )
                remaining = capture["limit"] - len(capture["data"])
                if remaining > 0:
                    capture["data"].extend(chunk[:remaining])
                if len(chunk) > max(0, remaining):
                    capture["truncated"] = True
        except Exception as err:
            capture["error"] = str(err)

    thread = threading.Thread(
        target=drain,
        name=f"qcopilots-{label}-capture",
        daemon=True,
    )
    capture["thread"] = thread
    thread.start()
    return capture


def _finish_bounded_pipe_captures(
    captures: list[dict[str, Any] | None],
) -> str | None:
    errors = []
    for capture in captures:
        if capture is None:
            continue
        thread = capture["thread"]
        thread.join(timeout=PIPE_CAPTURE_GRACE_SECONDS)
        if thread.is_alive():
            try:
                capture["stream"].close()
            except Exception as err:
                errors.append(f"close {capture['label']} pipe: {err}")
            thread.join(timeout=PIPE_CAPTURE_GRACE_SECONDS)
        if thread.is_alive():
            errors.append(f"{capture['label']} pipe reader did not stop")
        if capture["error"]:
            errors.append(f"{capture['label']} pipe read failed: {capture['error']}")
        try:
            capture["stream"].close()
        except Exception as err:
            errors.append(f"close {capture['label']} pipe: {err}")
    return " | ".join(errors) or None


def _bounded_capture_text(capture: dict[str, Any] | None) -> str:
    if capture is None:
        return ""
    encoding = locale.getpreferredencoding(False) or "utf-8"
    return bytes(capture["data"]).decode(encoding, errors="replace")


def _truncate(text: str | bytes, limit: int = MAX_EXEC_OUTPUT_CHARS) -> str:
    if isinstance(text, bytes):
        text = text.decode(errors="replace")
    if len(text) <= limit:
        return text
    return text[:limit]


def _terminate_builtin_process_tree(
    process: subprocess.Popen | None,
) -> tuple[bool, str | None]:
    if process is None or process.poll() is not None:
        return True, None
    errors = []
    if os.name == "nt":
        try:
            from qcopilots_common.process_controller import terminate_process_tree

            if not terminate_process_tree(
                process.pid,
                force=True,
                timeout_seconds=5,
            ):
                errors.append("taskkill did not terminate the process tree")
        except Exception as err:
            errors.append(str(err))
    else:
        try:
            os.killpg(os.getpgid(process.pid), signal.SIGKILL)
        except Exception as err:
            errors.append(str(err))
    try:
        process.wait(timeout=5)
    except Exception as err:
        errors.append(str(err))
        try:
            process.kill()
            process.wait(timeout=1)
        except Exception as kill_error:
            errors.append(str(kill_error))
    alive = process.poll() is None
    if alive:
        errors.append("process tree remains alive after timeout termination")
    return not alive, " | ".join(error for error in errors if error) or None


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


def _get_file_metadata_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "path": {"type": "string"},
        },
        ["path"],
    )


def _copy_file_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "source": {"type": "string", "minLength": 1},
            "target": {"type": "string", "minLength": 1},
            "expected_source_sha256": {
                "type": "string",
                "pattern": "^[0-9A-Fa-f]{64}$",
            },
            "create_dirs": {"type": "boolean", "default": True},
        },
        ["source", "target", "expected_source_sha256"],
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
            "max_scanned_entries": {
                "type": "integer",
                "minimum": 1,
                "maximum": MAX_SEARCH_SCANNED_ENTRIES,
            },
            "timeout_seconds": {
                "type": "number",
                "minimum": 0.01,
                "maximum": MAX_GREP_SEARCH_SECONDS,
            },
        },
        [],
    )


def _grep_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "path": {"type": "string", "default": "."},
            "root": {"type": "string", "default": "."},
            "pattern": {"type": "string", "maxLength": MAX_GREP_PATTERN_CHARS},
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
            "max_scanned_entries": {
                "type": "integer",
                "minimum": 1,
                "maximum": MAX_SEARCH_SCANNED_ENTRIES,
            },
            "timeout_seconds": {"type": "number", "minimum": 0.01, "maximum": MAX_GREP_SEARCH_SECONDS},
            "encoding": {"type": "string", "default": "utf-8"},
        },
        ["pattern"],
    )


def _exec_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "command": {
                "type": "array",
                "minItems": 1,
                "items": {"type": "string", "minLength": 1},
                "description": (
                    "Non-empty argv array. The first item is the executable and every "
                    "remaining item is one argument."
                ),
            },
            "cwd": {"type": "string", "default": "."},
            "timeout_seconds": {"type": "integer", "minimum": 1, "maximum": MAX_EXEC_TIMEOUT_SECONDS},
            "timeout": {"type": "integer", "minimum": 1, "maximum": MAX_EXEC_TIMEOUT_SECONDS},
            "max_output_size": {"type": "integer", "minimum": 0, "maximum": MAX_EXEC_OUTPUT_CHARS},
        },
        ["command"],
    )


def _write_schema() -> dict[str, Any]:
    schema = _base_schema(
        {
            "path": {"type": "string"},
            "content": {"type": "string"},
            "encoding": {"type": "string", "default": "utf-8"},
            "create_dirs": {"type": "boolean", "default": True},
            "overwrite": {"type": "boolean", "default": False},
            "expected_sha256": {
                "type": "string",
                "pattern": "^[0-9a-fA-F]{64}$",
                "description": "Optional SHA-256 required to match an existing overwrite target.",
            },
        },
        ["path", "content"],
    )
    schema["allOf"] = [
        {
            "if": {
                "properties": {"overwrite": {"const": True}},
                "required": ["overwrite"],
            },
            "then": {"required": ["expected_sha256"]},
        }
    ]
    return schema


def _edit_schema() -> dict[str, Any]:
    schema = _base_schema(
        {
            "path": {"type": "string"},
            "search": {"type": "string", "minLength": 1},
            "replace": {"type": "string"},
            "old_text": {"type": "string", "minLength": 1},
            "new_text": {"type": "string"},
            "edits": {
                "type": "array",
                "minItems": 1,
                "items": {
                    "type": "object",
                    "properties": {
                        "old_text": {"type": "string", "minLength": 1},
                        "new_text": {"type": "string"},
                        "search": {"type": "string", "minLength": 1},
                        "replace": {"type": "string"},
                    },
                    "additionalProperties": False,
                    "oneOf": [
                        {
                            "required": ["old_text"],
                            "not": {"required": ["search"]},
                        },
                        {
                            "required": ["search"],
                            "not": {"required": ["old_text"]},
                        },
                    ],
                },
            },
            "encoding": {"type": "string", "default": "utf-8"},
            "expected_replacements": {"type": "integer", "minimum": 0},
            "expected_sha256": {
                "type": "string",
                "pattern": "^[0-9a-fA-F]{64}$",
                "description": "SHA-256 required to match the file before editing.",
            },
        },
        ["path", "expected_sha256"],
    )
    schema["oneOf"] = [
        {
            "required": ["edits"],
            "not": {
                "anyOf": [
                    {"required": ["old_text"]},
                    {"required": ["search"]},
                ]
            },
        },
        {
            "required": ["old_text"],
            "not": {
                "anyOf": [
                    {"required": ["edits"]},
                    {"required": ["search"]},
                ]
            },
        },
        {
            "required": ["search"],
            "not": {
                "anyOf": [
                    {"required": ["edits"]},
                    {"required": ["old_text"]},
                ]
            },
        },
    ]
    return schema


def _datetime_schema() -> dict[str, Any]:
    return _base_schema(
        {
            "timezone": {
                "type": "string",
                "enum": ["local", "UTC"],
                "default": "local",
            },
            "utc": {"type": "boolean", "default": False},
        },
        [],
    )
