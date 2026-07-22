"""QGIS unit tests for QCopilots builtin MCP tools.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import json
import os
import sys
import tempfile
import threading
import types
import unittest
from pathlib import Path
from urllib.request import Request, urlopen


class TestQCopilotsMcpServerBuiltinTools(unittest.TestCase):
    def test_file_tools_keep_relative_paths_under_configured_root(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tools = BuiltinTools(root_path=root, command_timeout_seconds=2)

            write_result = tools.write_file(
                {"path": "notes/data.txt", "content": "alpha\nneedle\nomega\n"}
            )
            self.assertEqual(write_result["path"], "notes/data.txt")
            self.assertEqual(write_result["bytes_written"], 19)

            read_result = tools.read_file({"path": "notes/data.txt"})
            self.assertEqual(read_result["content"], "alpha\nneedle\nomega\n")
            self.assertEqual(read_result["plain_text_response"], "alpha\nneedle\nomega\n")
            self.assertEqual(read_result["path"], "notes/data.txt")
            limited_result = tools.read_file(
                {
                    "path": "notes/data.txt",
                    "offset": 0,
                    "limit": 10**9,
                }
            )
            self.assertLessEqual(len(limited_result["content"].encode("utf-8")), 256 * 1024)
            with self.assertRaises(FileExistsError):
                tools.write_file(
                    {
                        "path": "notes/data.txt",
                        "content": "replace",
                        "overwrite": False,
                    }
                )
            with self.assertRaises(FileNotFoundError):
                tools.write_file(
                    {
                        "path": "missing/data.txt",
                        "content": "replace",
                        "create_dirs": False,
                    }
                )

            line_result = tools.read_file(
                {
                    "path": "notes/data.txt",
                    "start_line": 2,
                    "end_line": 3,
                    "append_loc": True,
                }
            )
            self.assertEqual(line_result["content"], "2\u2192 needle\n3\u2192 omega")
            self.assertEqual(line_result["start_line"], 2)
            self.assertEqual(line_result["end_line"], 3)

            glob_result = tools.file_glob_search({"include": "*.txt", "exclude": ["skip*"]})
            self.assertEqual(glob_result["matches"], ["notes/data.txt"])
            self.assertEqual(glob_result["plain_text_response"], "notes/data.txt")

            grep_result = tools.grep_search({"pattern": "needle", "path": "."})
            self.assertEqual(
                grep_result["matches"],
                [{"path": "notes/data.txt", "line_number": 2, "line": "needle"}],
            )
            self.assertEqual(
                tools.grep_search({"pattern": "NEEDLE", "path": "."})["matches"],
                [],
            )
            self.assertEqual(
                tools.grep_search(
                    {"pattern": "NEEDLE", "path": ".", "ignore_case": True}
                )["matches"],
                [{"path": "notes/data.txt", "line_number": 2, "line": "needle"}],
            )

            edit_result = tools.edit_file(
                {
                    "path": "notes/data.txt",
                    "edits": [
                        {"old_text": "alpha", "new_text": "alpha-edited"},
                        {"old_text": "needle", "new_text": "needle-edited"},
                    ],
                    "expected_replacements": 2,
                }
            )
            self.assertEqual(edit_result["replacements"], 2)
            self.assertEqual(
                tools.read_file({"path": "notes/data.txt"})["content"],
                "alpha-edited\nneedle-edited\nomega\n",
            )

            with self.assertRaises(ValueError):
                tools.edit_file(
                    {
                        "path": "notes/data.txt",
                        "edits": [{"old_text": "edited", "new_text": "twice"}],
                    }
                )
            with self.assertRaises(ValueError):
                tools.edit_file(
                    {
                        "path": "notes/data.txt",
                        "edits": [{"old_text": "omega", "new_text": "omega-edited"}],
                        "expected_replacements": 2,
                    }
                )
            with self.assertRaises(ValueError):
                tools.edit_file(
                    {
                        "path": "notes/data.txt",
                        "edits": [
                            {"old_text": "alpha-edited\nneedle-edited", "new_text": "merged"},
                            {"old_text": "needle-edited", "new_text": "needle"},
                        ],
                    }
                )

            (root / "notes" / "large.txt").write_text(
                "\n".join(f"line-{index}" for index in range(400)),
                encoding="utf-8",
            )
            large_line_result = tools.read_file(
                {
                    "path": "notes/large.txt",
                    "start_line": 1,
                    "append_loc": True,
                }
            )
            self.assertTrue(large_line_result["truncated"])
            self.assertLessEqual(large_line_result["end_line"], 200)

            (root / "notes" / "single-line.txt").write_text(
                "x" * (256 * 1024 + 4096),
                encoding="utf-8",
            )
            single_line_result = tools.read_file(
                {
                    "path": "notes/single-line.txt",
                    "start_line": 1,
                    "append_loc": True,
                }
            )
            self.assertTrue(single_line_result["truncated"])
            self.assertLessEqual(
                len(single_line_result["content"].encode("utf-8")),
                256 * 1024 + 16,
            )

    def test_file_tools_accept_absolute_paths_inside_configured_roots(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as release_tmp, tempfile.TemporaryDirectory() as workspace_tmp:
            release_bin = Path(release_tmp) / "bin"
            release_bin.mkdir()
            workspace = Path(workspace_tmp).resolve()
            data_dir = workspace / "data"
            data_dir.mkdir()
            source = data_dir / "source.txt"
            with source.open("w", encoding="utf-8", newline="\n") as handle:
                handle.write("alpha\nneedle\nomega\n")

            tools = BuiltinTools(
                root_path=release_bin,
                command_timeout_seconds=2,
                allowed_roots=[release_bin, workspace],
            )

            read_result = tools.read_file({"path": str(source)})
            self.assertEqual(read_result["content"], "alpha\nneedle\nomega\n")
            self.assertEqual(Path(read_result["path"]).resolve(), source)

            written = data_dir / "written.txt"
            write_result = tools.write_file({"path": str(written), "content": "new\nneedle\n"})
            self.assertEqual(Path(write_result["path"]).resolve(), written)
            self.assertEqual(written.read_text(encoding="utf-8"), "new\nneedle\n")

            edit_result = tools.edit_file(
                {
                    "path": str(source),
                    "edits": [{"old_text": "needle", "new_text": "pin"}],
                    "expected_replacements": 1,
                }
            )
            self.assertEqual(edit_result["replacements"], 1)
            self.assertEqual(source.read_text(encoding="utf-8"), "alpha\npin\nomega\n")

            glob_result = tools.file_glob_search({"path": str(workspace), "include": "*.txt"})
            self.assertEqual(
                sorted(Path(match).as_posix().split("/")[-2:] for match in glob_result["matches"]),
                [["data", "source.txt"], ["data", "written.txt"]],
            )

            grep_result = tools.grep_search({"path": str(workspace), "pattern": "pin"})
            self.assertEqual(len(grep_result["matches"]), 1)
            self.assertEqual(
                Path(grep_result["matches"][0]["path"]).as_posix().split("/")[-2:],
                ["data", "source.txt"],
            )
            self.assertEqual(grep_result["matches"][0]["line_number"], 2)
            self.assertEqual(grep_result["matches"][0]["line"], "pin")

            file_grep_result = tools.grep_search({"path": str(source), "pattern": "pin"})
            self.assertEqual(len(file_grep_result["matches"]), 1)
            self.assertEqual(Path(file_grep_result["matches"][0]["path"]).resolve(), source)
            self.assertEqual(file_grep_result["matches"][0]["line_number"], 2)
            self.assertEqual(file_grep_result["matches"][0]["line"], "pin")

            command_result = tools.exec_shell_command(
                {
                    "command": [
                        sys.executable,
                        "-c",
                        "from pathlib import Path; print(Path.cwd().resolve().name)",
                    ],
                    "cwd": str(workspace),
                }
            )
            self.assertEqual(command_result["exit_code"], 0)
            self.assertFalse(command_result["timed_out"])
            self.assertEqual(command_result["stdout"].strip(), workspace.name)

    def test_file_tools_allow_absolute_paths_outside_default_root(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp)
            source = outside / "source.txt"
            source.write_bytes(b"alpha\nneedle\n")
            tools = BuiltinTools(root_path=root, command_timeout_seconds=2)

            self.assertEqual(tools.read_file({"path": str(source)})["content"], "alpha\nneedle\n")
            self.assertEqual(
                tools.write_file({"path": str(outside / "written.txt"), "content": "new\n"})["path"],
                (outside / "written.txt").resolve().as_posix(),
            )
            self.assertEqual(
                tools.edit_file(
                    {
                        "path": str(source),
                        "edits": [{"old_text": "needle", "new_text": "pin"}],
                    }
                )["replacements"],
                1,
            )
            self.assertEqual(
                tools.file_glob_search({"path": str(outside), "include": "*.txt"})["matches"],
                ["source.txt", "written.txt"],
            )
            self.assertEqual(
                tools.grep_search({"path": str(outside), "pattern": "pin"})["matches"][0]["line"],
                "pin",
            )
            self.assertEqual(
                tools.exec_shell_command(
                    {
                        "command": [sys.executable, "-c", "print('allowed')"],
                        "cwd": str(outside),
                    }
                )["stdout"].strip(),
                "allowed",
            )

    def test_file_tools_constructor_allowed_roots_remains_non_restrictive(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp)
            source = outside / "source.txt"
            source.write_bytes(b"alpha\nneedle\n")
            tools = BuiltinTools(
                root_path=root,
                command_timeout_seconds=2,
                allowed_roots=[root],
            )

            self.assertEqual(tools.read_file({"path": str(source)})["content"], "alpha\nneedle\n")
            write_result = tools.write_file(
                {"path": str(outside / "written.txt"), "content": "new\n"}
            )
            self.assertEqual(write_result["path"], (outside / "written.txt").resolve().as_posix())
            command_result = tools.exec_shell_command(
                {
                    "command": [sys.executable, "-c", "print('allowed')"],
                    "cwd": str(outside),
                }
            )
            self.assertEqual(command_result["exit_code"], 0)
            self.assertEqual(command_result["stdout"].strip(), "allowed")

    def test_datetime_and_exec_shell_command_are_bounded(self):
        import json

        from qcopilots_common.builtin_tools import BuiltinTools
        from qcopilots_common.constants import BRIDGE_URL_ENV

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tools = BuiltinTools(root_path=root, command_timeout_seconds=1)

            datetime_result = tools.get_datetime({"timezone": "UTC"})
            self.assertEqual(datetime_result["timezone"], "UTC")
            self.assertRegex(
                datetime_result["iso"],
                r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?Z$",
            )

            command_result = tools.exec_shell_command(
                {
                    "command": [sys.executable, "-c", "print('qcopilots')"],
                    "cwd": ".",
                }
            )
            self.assertEqual(command_result["exit_code"], 0)
            self.assertFalse(command_result["timed_out"])
            self.assertEqual(command_result["stdout"].strip(), "qcopilots")
            self.assertEqual(command_result["stderr"], "")

            old_values = {
                BRIDGE_URL_ENV: os.environ.get(BRIDGE_URL_ENV),
                "EXAMPLE_SECRET": os.environ.get("EXAMPLE_SECRET"),
            }
            try:
                os.environ[BRIDGE_URL_ENV] = "http://127.0.0.1:48200"
                os.environ["EXAMPLE_SECRET"] = "secret-value"
                scrub_result = tools.exec_shell_command(
                    {
                        "command": [
                            sys.executable,
                            "-c",
                            (
                                "import json, os; "
                                "print(json.dumps({"
                                f"'{BRIDGE_URL_ENV}': os.environ.get('{BRIDGE_URL_ENV}'), "
                                "'EXAMPLE_SECRET': os.environ.get('EXAMPLE_SECRET')"
                                "}))"
                            ),
                        ],
                        "cwd": ".",
                    }
                )
                self.assertEqual(scrub_result["exit_code"], 0)
                self.assertEqual(
                    json.loads(scrub_result["stdout"]),
                    {
                        BRIDGE_URL_ENV: None,
                        "EXAMPLE_SECRET": None,
                    },
                )
            finally:
                for key, value in old_values.items():
                    if value is None:
                        os.environ.pop(key, None)
                    else:
                        os.environ[key] = value

            timeout_result = tools.exec_shell_command(
                {
                    "command": [
                        sys.executable,
                        "-c",
                        "import time; time.sleep(2)",
                    ],
                    "cwd": ".",
                    "timeout_seconds": 0.2,
                }
            )
            self.assertTrue(timeout_result["timed_out"])
            self.assertNotEqual(timeout_result["exit_code"], 0)

            timeout_alias_result = tools.exec_shell_command(
                {
                    "command": [
                        sys.executable,
                        "-c",
                        "import time; time.sleep(2)",
                    ],
                    "cwd": ".",
                    "timeout": 0.2,
                }
            )
            self.assertTrue(timeout_alias_result["timed_out"])
            self.assertNotEqual(timeout_alias_result["exit_code"], 0)

    def test_grep_search_reports_scan_limits_and_large_files(self):
        from qcopilots_common.builtin_tools import BuiltinTools
        from qcopilots_common.constants import MAX_FILE_READ_BYTES

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            first = root / "first.txt"
            second = root / "second.txt"
            large = root / "large.txt"
            first.write_text("alpha\nneedle\n", encoding="utf-8")
            second.write_text("omega\nneedle\n", encoding="utf-8")
            large.write_text("x" * (MAX_FILE_READ_BYTES + 1), encoding="utf-8")

            tools = BuiltinTools(root_path=root)
            limited_result = tools.grep_search(
                {"path": ".", "pattern": "needle", "max_scanned_files": 1}
            )
            self.assertEqual(limited_result["scanned_files"], 1)
            self.assertTrue(limited_result["truncated"])

            large_result = tools.grep_search({"path": str(large), "pattern": "x"})
            self.assertEqual(large_result["matches"], [])
            self.assertEqual(large_result["scanned_files"], 1)
            self.assertEqual(large_result["skipped_large_files"], 1)
            self.assertTrue(large_result["truncated"])

    def test_search_tools_allow_iterated_files_outside_default_root(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        original_iter_files = builtin_tools._iter_files
        try:
            with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
                root = Path(root_tmp)
                outside_file = Path(outside_tmp) / "outside.txt"
                outside_file.write_text("needle\n", encoding="utf-8")
                tools = BuiltinTools(root_path=root)

                def fake_iter_files(_root, _include):
                    yield outside_file

                builtin_tools._iter_files = fake_iter_files

                self.assertEqual(
                    tools.file_glob_search({"path": ".", "include": "*.txt"})["matches"],
                    [outside_file.resolve().as_posix()],
                )
                grep_result = tools.grep_search({"path": ".", "pattern": "needle"})
                self.assertEqual(len(grep_result["matches"]), 1)
                self.assertEqual(grep_result["matches"][0]["path"], outside_file.resolve().as_posix())
                self.assertEqual(grep_result["skipped_files"], 0)
        finally:
            builtin_tools._iter_files = original_iter_files

    def test_search_tools_skip_reparse_directories_before_traversal(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        original_is_reparse_directory = builtin_tools._is_reparse_directory
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                reparse_dir = root / "linked"
                reparse_dir.mkdir()
                (reparse_dir / "outside.txt").write_text("needle\n", encoding="utf-8")
                tools = BuiltinTools(root_path=root)

                def fake_is_reparse_directory(path):
                    return path.name == reparse_dir.name

                builtin_tools._is_reparse_directory = fake_is_reparse_directory

                self.assertEqual(
                    tools.file_glob_search({"path": ".", "include": "*.txt"})["matches"],
                    [],
                )
                grep_result = tools.grep_search({"path": ".", "pattern": "needle"})
                self.assertEqual(grep_result["matches"], [])
                self.assertEqual(grep_result["scanned_files"], 0)
        finally:
            builtin_tools._is_reparse_directory = original_is_reparse_directory

    def test_search_tools_do_not_revisit_resolved_directories(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        original_is_reparse_directory = builtin_tools._is_reparse_directory
        original_resolve = Path.resolve
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                linked = root / "linked"
                linked.mkdir()
                (linked / "loop.txt").write_text("needle\n", encoding="utf-8")
                root_resolved = original_resolve(root)
                tools = BuiltinTools(root_path=root)

                def fake_is_reparse_directory(_path):
                    return False

                def fake_resolve(path, *args, **kwargs):
                    if path.name == linked.name:
                        return root_resolved
                    return original_resolve(path, *args, **kwargs)

                builtin_tools._is_reparse_directory = fake_is_reparse_directory
                Path.resolve = fake_resolve

                self.assertEqual(
                    tools.file_glob_search({"path": ".", "include": "*.txt"})["matches"],
                    [],
                )
                grep_result = tools.grep_search({"path": ".", "pattern": "needle"})
                self.assertEqual(grep_result["matches"], [])
                self.assertEqual(grep_result["scanned_files"], 0)
        finally:
            builtin_tools._is_reparse_directory = original_is_reparse_directory
            Path.resolve = original_resolve

    def test_search_tools_skip_llama_cpp_junk_directories(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        junk_names = (
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
        )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "src").mkdir()
            (root / "src" / "visible.txt").write_text("needle\n", encoding="utf-8")
            (root / "app" / "src").mkdir(parents=True)
            (root / "app" / "src" / "nested.txt").write_text("needle\n", encoding="utf-8")
            for name in junk_names:
                directory = root / name
                directory.mkdir()
                (directory / "ignored.txt").write_text("needle\n", encoding="utf-8")

            tools = BuiltinTools(root_path=root)

            default_glob = tools.file_glob_search({"path": "."})
            self.assertEqual(
                sorted(default_glob["matches"]),
                ["app/src/nested.txt", "src/visible.txt"],
            )

            anchored_glob = tools.file_glob_search({"path": ".", "include": "src/*.txt"})
            self.assertEqual(
                sorted(anchored_glob["matches"]),
                ["app/src/nested.txt", "src/visible.txt"],
            )

            grep_result = tools.grep_search({"path": ".", "pattern": "needle"})
            self.assertEqual(
                sorted(match["path"] for match in grep_result["matches"]),
                ["app/src/nested.txt", "src/visible.txt"],
            )
            self.assertFalse(grep_result["timed_out"])
            for match in default_glob["matches"]:
                self.assertFalse(any(part in junk_names for part in Path(match).parts), match)

    @unittest.skipUnless(os.name == "nt", "Windows helper subprocess flags are platform-specific")
    def test_exec_shell_command_hides_windows_console(self):
        import subprocess

        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        original_run = builtin_tools.subprocess.run
        calls = []

        def fake_run(command, *args, **kwargs):
            del args
            calls.append((command, kwargs))
            return types.SimpleNamespace(returncode=0, stdout="ready\n", stderr="")

        try:
            builtin_tools.subprocess.run = fake_run
            with tempfile.TemporaryDirectory() as tmp:
                result = BuiltinTools(root_path=tmp).exec_shell_command(
                    {"command": [sys.executable, "-c", "print('ready')"], "cwd": "."}
                )

            self.assertEqual(result["exit_code"], 0)
            self.assertEqual(len(calls), 1)
            self.assertTrue(calls[0][1]["creationflags"] & subprocess.CREATE_NO_WINDOW)
        finally:
            builtin_tools.subprocess.run = original_run

    def test_build_builtin_tools_exposes_real_mcp_handlers(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpJsonRpcServer

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tools = build_builtin_tools(root_path=root)
            self.assertTrue(all("requires_auth" not in tool.descriptor() for tool in tools))

            server = McpJsonRpcServer(
                server_name="qcopilots-builtin-test",
                server_version="1.0.0",
                tools=tools,
            )

            tool_names = [
                tool["name"]
                for tool in server.handle_json_rpc(
                    {"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}}
                )["result"]["tools"]
            ]
            self.assertEqual(
                tool_names,
                [
                    "read_file",
                    "file_glob_search",
                    "grep_search",
                    "exec_shell_command",
                    "write_file",
                    "edit_file",
                    "get_datetime",
                ],
            )
            descriptors = {
                tool["name"]: tool
                for tool in server.handle_json_rpc(
                    {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}}
                )["result"]["tools"]
            }
            self.assertIn("start_line", descriptors["read_file"]["inputSchema"]["properties"])
            self.assertIn("end_line", descriptors["read_file"]["inputSchema"]["properties"])
            self.assertIn("append_loc", descriptors["read_file"]["inputSchema"]["properties"])
            self.assertIn("include", descriptors["file_glob_search"]["inputSchema"]["properties"])
            self.assertIn("exclude", descriptors["file_glob_search"]["inputSchema"]["properties"])
            self.assertEqual(
                descriptors["file_glob_search"]["inputSchema"]["properties"]["include"]["default"],
                "**",
            )
            self.assertIn("max_scanned_files", descriptors["grep_search"]["inputSchema"]["properties"])
            self.assertEqual(
                descriptors["grep_search"]["inputSchema"]["properties"]["include"]["default"],
                "**",
            )
            self.assertFalse(
                descriptors["grep_search"]["inputSchema"]["properties"]["ignore_case"]["default"]
            )
            self.assertTrue(
                descriptors["grep_search"]["inputSchema"]["properties"]["case_sensitive"]["default"]
            )
            self.assertFalse(
                descriptors["grep_search"]["inputSchema"]["properties"]["return_line_numbers"]["default"]
            )
            self.assertIn("timeout_seconds", descriptors["grep_search"]["inputSchema"]["properties"])
            self.assertIn("edits", descriptors["edit_file"]["inputSchema"]["properties"])
            self.assertIn("timezone", descriptors["get_datetime"]["inputSchema"]["properties"])

            self.assertEqual(
                self._call_tool(
                    server,
                    "write_file",
                    {"path": "notes.txt", "content": "alpha\nneedle\n"},
                )["bytes_written"],
                13,
            )
            self.assertEqual(
                self._call_tool(server, "read_file", {"path": "notes.txt"})["content"],
                "alpha\nneedle\n",
            )
            self.assertEqual(
                self._call_tool(
                    server,
                    "read_file",
                    {"path": "notes.txt", "start_line": 2, "append_loc": True},
                )["plain_text_response"],
                "2\u2192 needle",
            )
            self.assertEqual(
                self._call_tool(
                    server,
                    "file_glob_search",
                    {"path": ".", "include": "*.txt"},
                )["matches"],
                ["notes.txt"],
            )
            self.assertEqual(
                self._call_tool(
                    server,
                    "grep_search",
                    {"path": ".", "pattern": "needle"},
                )["matches"][0]["line_number"],
                2,
            )
            self.assertEqual(
                self._call_tool(
                    server,
                    "edit_file",
                    {"path": "notes.txt", "search": "needle", "replace": "pin"},
                )["replacements"],
                1,
            )
            self.assertEqual(
                self._call_tool(
                    server,
                    "exec_shell_command",
                    {"command": [sys.executable, "-c", "print('qcopilots')"], "cwd": "."},
                )["stdout"].strip(),
                "qcopilots",
            )
            self.assertIn("iso", self._call_tool(server, "get_datetime", {"timezone": "UTC"}))

    def test_build_builtin_tools_default_allows_absolute_paths(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpJsonRpcServer

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp).resolve()
            source = outside / "source.txt"
            source.write_bytes(b"alpha\nneedle\n")

            server = McpJsonRpcServer(
                server_name="qcopilots-builtin-default-full-access-test",
                server_version="1.0.0",
                tools=build_builtin_tools(root_path=root),
            )

            self.assertEqual(
                self._call_tool(server, "read_file", {"path": str(source)})["content"],
                "alpha\nneedle\n",
            )
            self.assertEqual(
                self._call_tool(
                    server,
                    "write_file",
                    {"path": str(outside / "written.txt"), "content": "new\n"},
                )["path"],
                (outside / "written.txt").resolve().as_posix(),
            )
            self.assertEqual(
                self._call_tool(
                    server,
                    "exec_shell_command",
                    {"command": [sys.executable, "-c", "print('outside')"], "cwd": str(outside)},
                )["stdout"].strip(),
                "outside",
            )

    def test_build_builtin_tools_constructor_compatibility_keeps_full_access(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpJsonRpcServer

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp).resolve()
            source = outside / "source.txt"
            source.write_bytes(b"alpha\nneedle\n")

            tools = build_builtin_tools(root_path=root)
            self.assertTrue(all("requires_auth" not in tool.descriptor() for tool in tools))
            server = McpJsonRpcServer(
                server_name="qcopilots-builtin-full-access-test",
                server_version="1.0.0",
                tools=tools,
            )

            self.assertEqual(
                self._call_tool(server, "read_file", {"path": str(source)})["content"],
                "alpha\nneedle\n",
            )

    def test_builtin_http_tools_allow_local_llama_ui_without_auth_header(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpHttpServer

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            server = McpHttpServer(
                name="qcopilots-builtin-http-test",
                version="1.0.0",
                tools=build_builtin_tools(root_path=root),
                host="127.0.0.1",
                port=0,
            )
            httpd = server.create_http_server()
            port = httpd.server_address[1]
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()
            try:
                self.assertEqual(
                    self._post_tool(
                        port,
                        "write_file",
                        {"path": "notes.txt", "content": "alpha\nneedle\n"},
                    )["bytes_written"],
                    13,
                )
                self.assertEqual(
                    self._post_tool(port, "read_file", {"path": "notes.txt"})["content"],
                    "alpha\nneedle\n",
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "file_glob_search",
                        {"path": ".", "include": "*.txt"},
                    )["matches"],
                    ["notes.txt"],
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "grep_search",
                        {"path": ".", "pattern": "needle"},
                    )["matches"][0]["line_number"],
                    2,
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "edit_file",
                        {"path": "notes.txt", "search": "needle", "replace": "pin"},
                    )["replacements"],
                    1,
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "exec_shell_command",
                        {
                            "command": [sys.executable, "-c", "print('qcopilots')"],
                            "cwd": ".",
                        },
                    )["stdout"].strip(),
                    "qcopilots",
                )
                self.assertIn(
                    "iso",
                    self._post_tool(port, "get_datetime", {"timezone": "UTC"}),
                )
            finally:
                httpd.shutdown()
                httpd.server_close()
                thread.join(timeout=5)

    def test_builtin_http_full_access_write_tools_allow_local_llama_ui_without_auth_header(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpHttpServer

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp).resolve()
            source = outside / "source.txt"
            source.write_bytes(b"alpha\nneedle\n")
            server = McpHttpServer(
                name="qcopilots-builtin-http-full-access-test",
                version="1.0.0",
                tools=build_builtin_tools(root_path=root),
                host="127.0.0.1",
                port=0,
            )
            httpd = server.create_http_server()
            port = httpd.server_address[1]
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()
            try:
                self.assertEqual(
                    self._post_tool(port, "read_file", {"path": str(source)})["content"],
                    "alpha\nneedle\n",
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "write_file",
                        {"path": str(outside / "written.txt"), "content": "new\n"},
                    )["path"],
                    (outside / "written.txt").resolve().as_posix(),
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "edit_file",
                        {
                            "path": str(source),
                            "search": "needle",
                            "replace": "pin",
                        },
                    )["replacements"],
                    1,
                )
            finally:
                httpd.shutdown()
                httpd.server_close()
                thread.join(timeout=5)

    def _call_tool(self, server, name, arguments):
        response = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 100,
                "method": "tools/call",
                "params": {"name": name, "arguments": arguments},
            }
        )
        self.assertNotIn("error", response)
        return json.loads(response["result"]["content"][0]["text"])

    def _post_tool(self, port, name, arguments, headers=None):
        payload = json.dumps(
            {
                "jsonrpc": "2.0",
                "id": 101,
                "method": "tools/call",
                "params": {"name": name, "arguments": arguments},
            }
        ).encode("utf-8")
        request_headers = {
            "Content-Type": "application/json",
            "Origin": "http://127.0.0.1:8282",
        }
        if headers:
            request_headers.update(headers)
        request = Request(
            f"http://127.0.0.1:{port}/mcp",
            data=payload,
            headers=request_headers,
            method="POST",
        )
        with urlopen(request, timeout=5) as response:
            body = json.loads(response.read().decode("utf-8"))
        self.assertNotIn("error", body)
        return json.loads(body["result"]["content"][0]["text"])


if __name__ == "__main__":
    unittest.main()
