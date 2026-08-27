"""QGIS unit tests for QCopilots builtin MCP tools.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import hashlib
import io
import json
import os
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from urllib.request import Request, urlopen


class TestQCopilotsMcpServerBuiltinTools(unittest.TestCase):
    def test_get_file_metadata_hashes_full_regular_file(self):
        from qcopilots_common.builtin_tools import (
            FILE_SIGNATURE_BYTES,
            BuiltinTools,
        )
        from qcopilots_common.constants import MAX_FILE_READ_BYTES

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "sample.BIN"
            content = bytes(range(256)) * (MAX_FILE_READ_BYTES // 256 + 2)
            path.write_bytes(content)

            result = BuiltinTools(root_path=root).get_file_metadata(
                {"path": path.name}
            )

            self.assertEqual(result["path"], path.name)
            self.assertEqual(result["bytes"], len(content))
            self.assertEqual(result["extension"], ".bin")
            self.assertEqual(
                result["signature_hex"],
                content[:FILE_SIGNATURE_BYTES].hex(),
            )
            self.assertEqual(
                result["sha256"], hashlib.sha256(content).hexdigest()
            )

            with self.assertRaisesRegex(IsADirectoryError, "regular file"):
                BuiltinTools(root_path=root).get_file_metadata({"path": "."})

    def test_get_file_metadata_detects_change_while_hashing(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "changing.bin"
            path.write_bytes(b"stable content")
            real_fstat = builtin_tools.os.fstat
            calls = 0

            def changed_after_read(file_descriptor):
                nonlocal calls
                current = real_fstat(file_descriptor)
                calls += 1
                if calls != 2:
                    return current
                return SimpleNamespace(
                    st_dev=current.st_dev,
                    st_ino=current.st_ino,
                    st_size=current.st_size,
                    st_mtime=current.st_mtime + 1,
                    st_mtime_ns=current.st_mtime_ns + 1_000_000_000,
                )

            with patch.object(
                builtin_tools.os,
                "fstat",
                side_effect=changed_after_read,
            ), self.assertRaisesRegex(
                RuntimeError,
                "changed while it was being hashed",
            ):
                BuiltinTools(root_path=root).get_file_metadata({"path": path.name})

    def test_copy_file_succeeds_with_verified_source_snapshot(self):
        from qcopilots_common.builtin_tools import (
            FILE_SIGNATURE_BYTES,
            BuiltinTools,
        )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.bin"
            target = root / "nested" / "copied.bin"
            content = bytes(range(64)) + b"qcopilots-copy"
            source.write_bytes(content)
            expected_sha256 = hashlib.sha256(content).hexdigest()

            result = BuiltinTools(root_path=root).copy_file(
                {
                    "source": source.name,
                    "target": "nested/copied.bin",
                    "expected_source_sha256": expected_sha256,
                }
            )

            self.assertEqual(result["source"], source.name)
            self.assertEqual(result["target"], "nested/copied.bin")
            self.assertEqual(result["bytes_copied"], len(content))
            self.assertEqual(
                result["signature_hex"],
                content[:FILE_SIGNATURE_BYTES].hex(),
            )
            self.assertEqual(result["sha256"], expected_sha256)
            self.assertEqual(
                result["cleanup"],
                {
                    "complete": True,
                    "residual_paths": [],
                    "retry_recommended": False,
                },
            )
            self.assertEqual(target.read_bytes(), content)
            self.assertEqual(source.read_bytes(), content)

    def test_copy_file_rejects_sha_mismatch_and_existing_target(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.txt"
            source.write_bytes(b"verified source")
            tools = BuiltinTools(root_path=root)

            mismatched_target = root / "mismatched.txt"
            with self.assertRaisesRegex(ValueError, "expected_source_sha256"):
                tools.copy_file(
                    {
                        "source": source.name,
                        "target": mismatched_target.name,
                        "expected_source_sha256": "0" * 64,
                    }
                )
            self.assertFalse(mismatched_target.exists())
            self.assertEqual(
                list(root.glob(".mismatched.txt.qcopilots-copy-*.tmp")),
                [],
            )

            existing_target = root / "existing.txt"
            existing_target.write_bytes(b"preserve me")
            with self.assertRaisesRegex(FileExistsError, "already exists"):
                tools.copy_file(
                    {
                        "source": source.name,
                        "target": existing_target.name,
                        "expected_source_sha256": hashlib.sha256(
                            source.read_bytes()
                        ).hexdigest(),
                    }
                )
            self.assertEqual(existing_target.read_bytes(), b"preserve me")
            self.assertEqual(
                list(root.glob(".existing.txt.qcopilots-copy-*.tmp")),
                [],
            )

    def test_copy_file_formal_policy_allows_paths_outside_shell_roots(self):
        from qcopilots_common.builtin_tools import BuiltinTools
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp)
            source = outside / "source.txt"
            source.write_bytes(b"outside source")
            expected_sha256 = hashlib.sha256(source.read_bytes()).hexdigest()
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )
            tools = BuiltinTools(
                root_path=root,
                allowed_roots=[root],
                filesystem_policy=policy,
            )

            target = outside / "copied.txt"
            result = tools.copy_file(
                {
                    "source": str(source),
                    "target": str(target),
                    "expected_source_sha256": expected_sha256,
                }
            )
            self.assertEqual(result["sha256"], expected_sha256)
            self.assertEqual(target.read_bytes(), source.read_bytes())

    def test_copy_file_publication_failure_removes_temporary_output(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.txt"
            target = root / "target.txt"
            source.write_bytes(b"copy publication must be atomic")
            expected_sha256 = hashlib.sha256(source.read_bytes()).hexdigest()

            with patch.object(
                builtin_tools,
                "_publish_new_file",
                side_effect=OSError("publication failed"),
            ), self.assertRaisesRegex(OSError, "publication failed"):
                BuiltinTools(root_path=root).copy_file(
                    {
                        "source": source.name,
                        "target": target.name,
                        "expected_source_sha256": expected_sha256,
                    }
                )

            self.assertFalse(target.exists())
            self.assertEqual(
                list(root.glob(".target.txt.qcopilots-copy-*.tmp")),
                [],
            )
            self.assertEqual(source.read_bytes(), b"copy publication must be atomic")

    @unittest.skipUnless(os.name == "nt", "Windows no-clobber publication")
    def test_new_file_publication_does_not_require_hard_links(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.txt"
            source.write_bytes(b"ordinary removable volume semantics")
            expected_sha256 = hashlib.sha256(source.read_bytes()).hexdigest()
            tools = BuiltinTools(root_path=root)

            with patch.object(
                builtin_tools.os,
                "link",
                side_effect=OSError("hard links are unsupported"),
            ):
                written = tools.write_file(
                    {"path": "written.txt", "content": "written"}
                )
                copied = tools.copy_file(
                    {
                        "source": source.name,
                        "target": "copied.txt",
                        "expected_source_sha256": expected_sha256,
                    }
                )

            self.assertEqual(written["sha256"], hashlib.sha256(b"written").hexdigest())
            self.assertEqual(copied["sha256"], expected_sha256)
            self.assertEqual((root / "copied.txt").read_bytes(), source.read_bytes())

    def test_file_writes_are_atomic_and_sha_guarded(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "guarded.txt"
            tools = BuiltinTools(root_path=root)

            created = tools.write_file({"path": "guarded.txt", "content": "alpha\n"})
            alpha_sha = hashlib.sha256(b"alpha\n").hexdigest()
            self.assertEqual(created["sha256"], alpha_sha)
            self.assertFalse(created["overwritten"])
            with self.assertRaises(FileExistsError):
                tools.write_file({"path": "guarded.txt", "content": "unsafe\n"})
            self.assertEqual(path.read_bytes(), b"alpha\n")

            with self.assertRaisesRegex(ValueError, "expected_sha256"):
                tools.write_file(
                    {
                        "path": "guarded.txt",
                        "content": "unsafe\n",
                        "overwrite": True,
                    }
                )

            with self.assertRaisesRegex(ValueError, "expected_sha256"):
                tools.write_file(
                    {
                        "path": "guarded.txt",
                        "content": "unsafe\n",
                        "overwrite": True,
                        "expected_sha256": "0" * 64,
                    }
                )
            self.assertEqual(path.read_bytes(), b"alpha\n")

            overwritten = tools.write_file(
                {
                    "path": "guarded.txt",
                    "content": "beta\n",
                    "overwrite": True,
                    "expected_sha256": alpha_sha,
                }
            )
            beta_sha = hashlib.sha256(b"beta\n").hexdigest()
            self.assertTrue(overwritten["overwritten"])
            self.assertEqual(overwritten["previous_sha256"], alpha_sha)
            self.assertEqual(overwritten["sha256"], beta_sha)
            self.assertTrue(overwritten["cleanup"]["complete"])

            with self.assertRaisesRegex(ValueError, "expected_sha256"):
                tools.edit_file(
                    {
                        "path": "guarded.txt",
                        "search": "beta",
                        "replace": "gamma",
                        "expected_sha256": alpha_sha,
                    }
                )
            self.assertEqual(path.read_bytes(), b"beta\n")

            with patch(
                "qcopilots_common.builtin_tools._replace_existing_file_with_backup",
                side_effect=OSError("replace failed"),
            ):
                with self.assertRaisesRegex(OSError, "replace failed"):
                    tools.edit_file(
                        {
                            "path": "guarded.txt",
                            "search": "beta",
                            "replace": "gamma",
                            "expected_sha256": beta_sha,
                        }
                    )
            self.assertEqual(path.read_bytes(), b"beta\n")
            self.assertEqual(list(root.glob(".guarded.txt.qcopilots-*.tmp")), [])

    def test_edit_file_requires_exactly_one_non_empty_edit_mode(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "guarded.txt"
            path.write_text("alpha", encoding="utf-8")
            expected_sha256 = hashlib.sha256(path.read_bytes()).hexdigest()
            tools = BuiltinTools(root_path=root)
            invalid_modes = (
                {},
                {"search": "alpha", "old_text": "alpha"},
                {"edits": []},
                {"edits": [{"search": "alpha", "old_text": "alpha"}]},
            )

            for mode in invalid_modes:
                with self.subTest(mode=mode), self.assertRaisesRegex(
                    ValueError,
                    "edit mode|non-empty|exactly one",
                ):
                    tools.edit_file(
                        {
                            "path": str(path),
                            "expected_sha256": expected_sha256,
                            **mode,
                        }
                    )
            self.assertEqual(path.read_text(encoding="utf-8"), "alpha")

    def test_file_overwrite_detects_race_and_restores_concurrent_content(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "guarded.txt"
            path.write_bytes(b"original")
            expected = hashlib.sha256(b"original").hexdigest()
            real_replace = builtin_tools._replace_existing_file_with_backup
            injected = False

            def race_at_publication(
                target, replacement, backup, *, expected_sha256
            ):
                nonlocal injected
                if not injected:
                    injected = True
                    path.write_bytes(b"intruder")
                return real_replace(
                    target,
                    replacement,
                    backup,
                    expected_sha256=expected_sha256,
                )

            with patch.object(
                builtin_tools,
                "_replace_existing_file_with_backup",
                side_effect=race_at_publication,
            ), self.assertRaisesRegex(RuntimeError, "changed (before|during)"):
                BuiltinTools(root_path=root).write_file(
                    {
                        "path": path.name,
                        "content": "replacement",
                        "overwrite": True,
                        "expected_sha256": expected,
                    }
                )
            self.assertEqual(path.read_bytes(), b"intruder")
            self.assertEqual(list(root.glob(".guarded.txt.qcopilots-*")), [])

    def test_file_overwrite_preserves_concurrent_atomic_replacement(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "guarded.txt"
            path.write_bytes(b"original")
            expected = hashlib.sha256(b"original").hexdigest()
            real_replace = builtin_tools._replace_existing_file_with_backup
            injected = False

            def race_at_publication(
                target, replacement, backup, *, expected_sha256
            ):
                nonlocal injected
                if not injected:
                    injected = True
                    intruder = root / "intruder.tmp"
                    intruder.write_bytes(b"concurrent-replacement")
                    os.replace(intruder, path)
                return real_replace(
                    target,
                    replacement,
                    backup,
                    expected_sha256=expected_sha256,
                )

            with patch.object(
                builtin_tools,
                "_replace_existing_file_with_backup",
                side_effect=race_at_publication,
            ), self.assertRaisesRegex(RuntimeError, "changed (before|during)"):
                BuiltinTools(root_path=root).write_file(
                    {
                        "path": path.name,
                        "content": "replacement",
                        "overwrite": True,
                        "expected_sha256": expected,
                    }
                )
            self.assertEqual(path.read_bytes(), b"concurrent-replacement")
            self.assertEqual(list(root.glob(".guarded.txt.qcopilots-*")), [])

    def test_file_overwrite_atomic_replace_cleans_displaced_version(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "guarded.txt"
            path.write_bytes(b"original")
            expected = hashlib.sha256(b"original").hexdigest()
            real_replace = builtin_tools._replace_existing_file_with_backup
            def slow_atomic_replace(
                target, replacement, backup, *, expected_sha256
            ):
                self.assertEqual(Path(target), path)
                self.assertTrue(path.exists())
                time.sleep(0.02)
                result = real_replace(
                    target,
                    replacement,
                    backup,
                    expected_sha256=expected_sha256,
                )
                self.assertTrue(path.exists())
                time.sleep(0.02)
                return result

            with patch.object(
                builtin_tools,
                "_replace_existing_file_with_backup",
                side_effect=slow_atomic_replace,
            ):
                result = BuiltinTools(root_path=root).write_file(
                    {
                        "path": path.name,
                        "content": "replacement",
                        "overwrite": True,
                        "expected_sha256": expected,
                    }
                )

            self.assertEqual(path.read_bytes(), b"replacement")
            self.assertTrue(result["cleanup"]["complete"])
            self.assertEqual(list(root.glob(".guarded.txt.qcopilots-*")), [])

    def test_concurrent_writes_on_one_instance_serialize_sha_validation(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "guarded.txt"
            path.write_bytes(b"original")
            expected = hashlib.sha256(b"original").hexdigest()
            tools = BuiltinTools(root_path=root)
            barrier = threading.Barrier(3)
            results = []
            errors = []

            def write(content):
                barrier.wait()
                try:
                    results.append(
                        tools.write_file(
                            {
                                "path": path.name,
                                "content": content,
                                "overwrite": True,
                                "expected_sha256": expected,
                            }
                        )
                    )
                except Exception as err:
                    errors.append(err)

            threads = [
                threading.Thread(target=write, args=("first",)),
                threading.Thread(target=write, args=("second",)),
            ]
            for thread in threads:
                thread.start()
            barrier.wait()
            for thread in threads:
                thread.join(2)

            self.assertTrue(all(not thread.is_alive() for thread in threads))
            self.assertEqual(len(results), 1)
            self.assertEqual(len(errors), 1)
            self.assertIsInstance(errors[0], ValueError)
            self.assertIn("expected_sha256", str(errors[0]))
            self.assertIn(path.read_bytes(), {b"first", b"second"})
            self.assertEqual(list(root.glob(".guarded.txt.qcopilots-*")), [])

    def test_file_tools_keep_relative_paths_under_configured_root(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tools = BuiltinTools(root_path=root, command_timeout_seconds=2)

            write_result = tools.write_file(
                {"path": "notes/data.txt", "content": "alpha\nneedle\nomega\n"}
            )
            data_file = root / "notes" / "data.txt"
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
                    "expected_sha256": hashlib.sha256(data_file.read_bytes()).hexdigest(),
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
                        "expected_sha256": edit_result["sha256"],
                    }
                )
            with self.assertRaises(ValueError):
                tools.edit_file(
                    {
                        "path": "notes/data.txt",
                        "edits": [{"old_text": "omega", "new_text": "omega-edited"}],
                        "expected_replacements": 2,
                        "expected_sha256": edit_result["sha256"],
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
                        "expected_sha256": edit_result["sha256"],
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
                    "expected_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
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
                        "expected_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
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

    def test_file_tools_constructor_applies_allowed_roots_only_to_shell(self):
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

            self.assertEqual(
                tools.read_file({"path": str(source)})["content"],
                "alpha\nneedle\n",
            )
            written = outside / "written.txt"
            tools.write_file({"path": str(written), "content": "new\n"})
            self.assertEqual(written.read_text(encoding="utf-8"), "new\n")
            with self.assertRaisesRegex(PermissionError, "allowed_roots"):
                tools.exec_shell_command(
                    {
                        "command": [sys.executable, "-c", "print('blocked')"],
                        "cwd": str(outside),
                    }
                )

            full_access_tools = BuiltinTools(
                root_path=root,
                command_timeout_seconds=2,
                allowed_roots=[root],
                allow_full_access=True,
            )
            self.assertEqual(
                full_access_tools.exec_shell_command(
                    {
                        "command": [sys.executable, "-c", "print('allowed')"],
                        "cwd": str(outside),
                    }
                )["stdout"].strip(),
                "allowed",
            )

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
            self.assertEqual(tools.get_datetime({"utc": True})["timezone"], "UTC")
            self.assertEqual(
                tools.get_datetime({"timezone": "local", "utc": True})["timezone"],
                "local",
            )
            with self.assertRaisesRegex(ValueError, "local or UTC"):
                tools.get_datetime({"timezone": "Europe/London"})

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

    def test_shell_timeout_terminates_child_and_grandchild_process_tree(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            marker = root / "grandchild-survived.txt"
            grandchild_code = (
                "import time; from pathlib import Path; time.sleep(0.8); "
                f"Path({str(marker)!r}).write_text('survived', encoding='utf-8')"
            )
            child_code = (
                "import subprocess, sys, time; "
                f"subprocess.Popen([sys.executable, '-c', {grandchild_code!r}]); "
                "time.sleep(2)"
            )
            root_code = (
                "import subprocess, sys, time; "
                f"subprocess.Popen([sys.executable, '-c', {child_code!r}]); "
                "time.sleep(2)"
            )
            tools = BuiltinTools(root_path=root, command_timeout_seconds=1)
            result = tools.exec_shell_command(
                {
                    "command": [sys.executable, "-c", root_code],
                    "cwd": ".",
                    "timeout_seconds": 0.2,
                }
            )
            self.assertTrue(result["timed_out"])
            self.assertTrue(result["process_tree_terminated"], result)
            self.assertIsNone(result["termination_error"], result)
            time.sleep(1)
            self.assertFalse(marker.exists())

    @unittest.skipUnless(os.name == "nt", "Windows Job Object regression")
    def test_shell_normal_parent_exit_terminates_pipe_holding_descendant(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            marker = root / "descendant-survived.txt"
            child_code = (
                "import time; from pathlib import Path; time.sleep(0.8); "
                f"Path({str(marker)!r}).write_text('survived', encoding='utf-8')"
            )
            parent_code = (
                "import subprocess, sys; "
                f"subprocess.Popen([sys.executable, '-c', {child_code!r}]); "
                "print('parent-exited')"
            )
            started = time.monotonic()
            result = BuiltinTools(
                root_path=root,
                command_timeout_seconds=3,
            ).exec_shell_command(
                {
                    "command": [sys.executable, "-c", parent_code],
                    "cwd": ".",
                }
            )
            elapsed = time.monotonic() - started

            self.assertEqual(result["exit_code"], 0, result)
            self.assertLess(elapsed, 0.75, result)
            self.assertIsNone(result["capture_error"], result)
            time.sleep(1)
            self.assertFalse(marker.exists(), result)

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

            safe_result = tools.grep_search(
                {"path": str(first), "pattern": r"n[e]{2}dle"}
            )
            self.assertEqual(len(safe_result["matches"]), 1)
            for unsafe_pattern in (
                r"(a+)+$",
                r"a|aa",
                r"(a)\1",
                r"(?=a)",
                r"(?>a)",
                r"a{0,100}b{0,100}",
                r"a?b?",
                "a" * 300,
            ):
                with self.subTest(unsafe_pattern=unsafe_pattern), self.assertRaisesRegex(
                    ValueError, "Unsafe regex"
                ):
                    tools.grep_search(
                        {"path": str(first), "pattern": unsafe_pattern}
                    )

    def test_file_reads_and_edits_enforce_cap_and_opened_identity(self):
        import qcopilots_common.builtin_tools as builtin_tools
        from qcopilots_common.builtin_tools import BuiltinTools
        from qcopilots_common.constants import MAX_FILE_READ_BYTES

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "large.txt"
            path.write_bytes(b"x" * (MAX_FILE_READ_BYTES + 1))
            tools = BuiltinTools(root_path=root)

            read_result = tools.read_file({"path": path.name})
            self.assertEqual(len(read_result["content"]), MAX_FILE_READ_BYTES)
            self.assertTrue(read_result["truncated"])
            with self.assertRaisesRegex(ValueError, "byte edit limit"):
                tools.edit_file(
                    {
                        "path": path.name,
                        "search": "x",
                        "replace": "y",
                        "expected_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    }
                )

            small = root / "small.txt"
            small.write_text("needle\n", encoding="utf-8")
            original_fstat = builtin_tools.os.fstat

            def replaced_identity(fd):
                values = list(original_fstat(fd))
                values[1] += 1
                return os.stat_result(values)

            with patch.object(builtin_tools.os, "fstat", side_effect=replaced_identity):
                with self.assertRaisesRegex(RuntimeError, "replaced"):
                    tools.read_file({"path": small.name})
                with self.assertRaisesRegex(RuntimeError, "replaced"):
                    tools.edit_file(
                        {
                            "path": small.name,
                            "search": "needle",
                            "replace": "safe",
                            "expected_sha256": hashlib.sha256(
                                small.read_bytes()
                            ).hexdigest(),
                        }
                    )

    def test_search_entry_budget_counts_nonmatching_and_junk_entries(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for index in range(8):
                (root / f"ignored-{index}.bin").write_bytes(b"data")
            junk = root / ".git"
            junk.mkdir()
            (junk / "hidden.txt").write_text("needle\n", encoding="utf-8")
            tools = BuiltinTools(root_path=root)

            glob_result = tools.file_glob_search(
                {
                    "path": ".",
                    "include": "*.txt",
                    "max_scanned_entries": 3,
                }
            )
            self.assertEqual(glob_result["scanned_entries"], 3)
            self.assertTrue(glob_result["truncated"])

            grep_result = tools.grep_search(
                {
                    "path": ".",
                    "pattern": "needle",
                    "include": "*.txt",
                    "max_scanned_entries": 4,
                }
            )
            self.assertEqual(grep_result["scanned_entries"], 4)
            self.assertEqual(grep_result["scanned_files"], 0)
            self.assertTrue(grep_result["truncated"])

    def test_shell_capture_drains_both_pipes_with_bounded_memory(self):
        from qcopilots_common.builtin_tools import BuiltinTools

        with tempfile.TemporaryDirectory() as tmp:
            code = (
                "import os, sys\n"
                "chunk = b'x' * 65536\n"
                "for _ in range(32):\n"
                " os.write(sys.stdout.fileno(), chunk)\n"
                " os.write(sys.stderr.fileno(), chunk)\n"
            )
            result = BuiltinTools(
                root_path=tmp, command_timeout_seconds=10
            ).exec_shell_command(
                {
                    "command": [sys.executable, "-c", code],
                    "cwd": ".",
                    "max_output_size": 4096,
                }
            )
            self.assertEqual(result["exit_code"], 0, result)
            self.assertFalse(result["timed_out"])
            self.assertEqual(len(result["stdout"]), 4096)
            self.assertEqual(len(result["stderr"]), 4096)
            self.assertTrue(result["stdout_truncated"])
            self.assertTrue(result["stderr_truncated"])
            self.assertIsNone(result["capture_error"])

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

                def fake_iter_files(_root, _include, _budget=None):
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

        original_popen = builtin_tools.subprocess.Popen
        calls = []

        class FakeProcess:
            returncode = 0
            stdout = io.BytesIO(b"ready\n")
            stderr = io.BytesIO(b"")

            @staticmethod
            def wait(timeout):
                del timeout
                return 0

        def fake_popen(command, *args, **kwargs):
            del args
            calls.append((command, kwargs))
            return FakeProcess()

        try:
            builtin_tools.subprocess.Popen = fake_popen
            with tempfile.TemporaryDirectory() as tmp:
                result = BuiltinTools(root_path=tmp).exec_shell_command(
                    {"command": [sys.executable, "-c", "print('ready')"], "cwd": "."}
                )

            self.assertEqual(result["exit_code"], 0)
            self.assertEqual(len(calls), 1)
            self.assertTrue(calls[0][1]["creationflags"] & subprocess.CREATE_NO_WINDOW)
            self.assertTrue(calls[0][1]["creationflags"] & 0x00000004)
        finally:
            builtin_tools.subprocess.Popen = original_popen

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
            expected_tool_names = {
                "read_file",
                "get_file_metadata",
                "copy_file",
                "file_glob_search",
                "grep_search",
                "exec_shell_command",
                "write_file",
                "edit_file",
                "get_datetime",
            }
            self.assertEqual(len(tool_names), 9)
            self.assertEqual(len(tool_names), len(set(tool_names)))
            self.assertSetEqual(set(tool_names), expected_tool_names)
            self.assertIn("copy_file", tool_names)
            self.assertNotIn("probe_local_http_service", tool_names)
            descriptors = {
                tool["name"]: tool
                for tool in server.handle_json_rpc(
                    {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}}
                )["result"]["tools"]
            }
            self.assertIn("start_line", descriptors["read_file"]["inputSchema"]["properties"])
            self.assertIn("end_line", descriptors["read_file"]["inputSchema"]["properties"])
            self.assertIn("append_loc", descriptors["read_file"]["inputSchema"]["properties"])
            self.assertEqual(
                descriptors["get_file_metadata"]["inputSchema"]["required"],
                ["path"],
            )
            self.assertEqual(
                descriptors["copy_file"]["inputSchema"]["required"],
                ["source", "target", "expected_source_sha256"],
            )
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
            self.assertEqual(
                descriptors["grep_search"]["inputSchema"]["properties"]["encoding"]["default"],
                "utf-8",
            )
            self.assertIn("edits", descriptors["edit_file"]["inputSchema"]["properties"])
            self.assertEqual(
                len(descriptors["edit_file"]["inputSchema"]["oneOf"]),
                3,
            )
            self.assertFalse(
                descriptors["write_file"]["inputSchema"]["properties"]["overwrite"]["default"]
            )
            self.assertIn(
                "expected_sha256",
                descriptors["write_file"]["inputSchema"]["properties"],
            )
            self.assertIn(
                "expected_sha256",
                descriptors["edit_file"]["inputSchema"]["properties"],
            )
            self.assertIn(
                "expected_sha256",
                descriptors["edit_file"]["inputSchema"]["required"],
            )
            self.assertEqual(
                descriptors["write_file"]["inputSchema"]["allOf"][0]["then"]["required"],
                ["expected_sha256"],
            )
            self.assertIn("timezone", descriptors["get_datetime"]["inputSchema"]["properties"])
            self.assertEqual(
                descriptors["get_datetime"]["inputSchema"]["properties"]["timezone"]["enum"],
                ["local", "UTC"],
            )

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
                    "get_file_metadata",
                    {"path": "notes.txt"},
                )["sha256"],
                hashlib.sha256(b"alpha\nneedle\n").hexdigest(),
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
                    {
                        "path": "notes.txt",
                        "search": "needle",
                        "replace": "pin",
                        "expected_sha256": hashlib.sha256(b"alpha\nneedle\n").hexdigest(),
                    },
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

    def test_builtin_policy_rejection_is_an_mcp_tool_error(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpJsonRpcServer, ToolError
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as tmp:
            policy = filesystem_policy_from_config(
                {
                    "mode": "compatible",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": True, "allowed_origins": []},
                }
            )
            server = McpJsonRpcServer(
                server_name="qcopilots-builtin-tool-error-test",
                server_version="1.0.0",
                tools=build_builtin_tools(
                    root_path=tmp,
                    filesystem_policy=policy,
                ),
            )

            response = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "tools/call",
                    "params": {
                        "name": "exec_shell_command",
                        "arguments": {
                            "command": [sys.executable, "-c", "print('blocked')"],
                        },
                    },
                }
            )

            self.assertNotIn("error", response)
            self.assertTrue(response["result"]["isError"])
            self.assertIn(
                "disabled",
                response["result"]["content"][0]["text"],
            )

            source = Path(tmp) / "source.txt"
            source.write_text("text", encoding="utf-8")
            invalid_encoding = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "method": "tools/call",
                    "params": {
                        "name": "read_file",
                        "arguments": {
                            "path": str(source),
                            "encoding": "definitely-not-a-codec",
                        },
                    },
                }
            )

            self.assertNotIn("error", invalid_encoding)
            self.assertTrue(invalid_encoding["result"]["isError"])
            self.assertIn(
                "unknown encoding",
                invalid_encoding["result"]["content"][0]["text"],
            )

    def test_build_builtin_tools_defaults_to_unrestricted_files_and_contained_shell(self):
        from qcopilots_common.builtin_tools import (
            BUILTIN_ALLOWED_ROOTS_ENV,
            BUILTIN_ALLOW_FULL_ACCESS_ENV,
            build_builtin_tools,
        )
        from qcopilots_common.mcp_http import McpJsonRpcServer, ToolError

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp).resolve()
            source = outside / "source.txt"
            source.write_bytes(b"alpha\nneedle\n")

            with patch.dict(
                os.environ,
                {
                    BUILTIN_ALLOWED_ROOTS_ENV: "",
                    BUILTIN_ALLOW_FULL_ACCESS_ENV: "",
                },
            ):
                built_tools = build_builtin_tools(root_path=root)
                server = McpJsonRpcServer(
                    server_name="qcopilots-builtin-contained-test",
                    server_version="1.0.0",
                    tools=built_tools,
                )
                handlers = {tool.name: tool.handler for tool in built_tools}

                self.assertEqual(
                    handlers["read_file"]({"path": str(source)})["content"],
                    "alpha\nneedle\n",
                )
                written = outside / "written.txt"
                handlers["write_file"](
                    {"path": str(written), "content": "new\n"},
                )
                self.assertEqual(written.read_text(encoding="utf-8"), "new\n")
                with self.assertRaisesRegex(ToolError, "allowed_roots"):
                    handlers["exec_shell_command"](
                        {
                            "command": [sys.executable, "-c", "print('outside')"],
                            "cwd": str(outside),
                        },
                    )

    def test_build_builtin_tools_profile_roots_only_scope_shell(self):
        from qcopilots_common.builtin_tools import (
            BUILTIN_ALLOWED_ROOTS_ENV,
            BUILTIN_ALLOW_FULL_ACCESS_ENV,
            build_builtin_tools,
        )

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as data_tmp, tempfile.TemporaryDirectory() as second_tmp:
            root = Path(root_tmp)
            data = Path(data_tmp).resolve()
            source = data / "source.txt"
            source.write_text("profile data", encoding="utf-8")
            second_source = Path(second_tmp).resolve() / "outside.txt"
            second_source.write_text("outside profile data", encoding="utf-8")

            with patch.dict(
                os.environ,
                {
                    BUILTIN_ALLOWED_ROOTS_ENV: str(data),
                    BUILTIN_ALLOW_FULL_ACCESS_ENV: "false",
                },
            ):
                handlers = {tool.name: tool.handler for tool in build_builtin_tools(root)}
                self.assertEqual(
                    handlers["read_file"]({"path": str(source)})["content"],
                    "profile data",
                )
                self.assertEqual(
                    handlers["read_file"]({"path": str(second_source)})["content"],
                    "outside profile data",
                )

            handlers = {
                tool.name: tool.handler
                for tool in build_builtin_tools(root, allow_full_access=True)
            }
            self.assertEqual(
                handlers["read_file"]({"path": str(source)})["content"],
                "profile data",
            )

    def test_formal_policy_allows_all_normal_local_files_and_disables_shell(self):
        from qcopilots_common.builtin_tools import BuiltinTools
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            outside = root / "outside"
            outside.mkdir()
            source = outside / "source.txt"
            source.write_text("approved", encoding="utf-8")
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )
            tools = BuiltinTools(
                root_path=root,
                allowed_roots=[root],
                filesystem_policy=policy,
            )

            self.assertEqual(
                tools.read_file({"path": str(source)})["content"],
                "approved",
            )
            self.assertEqual(
                tools.get_file_metadata({"path": str(source)})["sha256"],
                hashlib.sha256(b"approved").hexdigest(),
            )
            tools.write_file(
                {"path": str(outside / "result.txt"), "content": "result"}
            )
            self.assertEqual(
                (outside / "result.txt").read_text(encoding="utf-8"),
                "result",
            )
            with self.assertRaisesRegex(PermissionError, "URI"):
                tools.read_file({"path": source.as_uri()})
            with self.assertRaisesRegex(PermissionError, "disabled"):
                tools.exec_shell_command(
                    {"command": [sys.executable, "-c", "print('blocked')"]}
                )

    def test_formal_policy_lexically_rejects_paths_before_metadata_probes(self):
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            read_root = root / "read"
            outside_root = root / "outside"
            for directory in (read_root, outside_root):
                directory.mkdir()
            inside = read_root / "inside.txt"
            inside.write_text("inside", encoding="utf-8")
            outside = outside_root / "outside.txt"
            outside.write_text("outside", encoding="utf-8")
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )
            blocked = (
                (r"\\server.invalid\share\outside.txt", None),
                (r"\\?\C:\outside.txt", None),
                (r"\\.\C:\outside.txt", None),
                (r"\??\C:\outside.txt", None),
                (r"C:outside.txt", None),
                (str(read_root / "NUL:stream"), None),
                (str(read_root / "COM\u00b9.txt"), None),
                (str(read_root / "CONIN$"), None),
                (str(read_root) + "\0outside.txt", None),
                (inside.as_uri(), None),
            )

            with patch.object(
                Path,
                "resolve",
                side_effect=AssertionError("Path.resolve must not be called"),
            ) as resolve_probe:
                with patch.object(
                    Path,
                    "is_symlink",
                    side_effect=AssertionError("path metadata must not be probed"),
                ) as symlink_probe, patch.object(
                    Path,
                    "exists",
                    side_effect=AssertionError("path metadata must not be probed"),
                ) as exists_probe, patch.object(
                    Path,
                    "is_junction",
                    create=True,
                    side_effect=AssertionError("path metadata must not be probed"),
                ) as junction_probe:
                    for value, base in blocked:
                        with self.subTest(value=value, base=base):
                            with self.assertRaises(PermissionError):
                                policy.resolve_path(value, access="read", base=base)
            resolve_probe.assert_not_called()
            symlink_probe.assert_not_called()
            exists_probe.assert_not_called()
            junction_probe.assert_not_called()

            self.assertEqual(
                policy.resolve_path(inside, access="read"),
                inside.resolve(),
            )
            self.assertEqual(
                policy.resolve_path("inside.txt", access="read", base=read_root),
                inside.resolve(),
            )
            self.assertEqual(
                policy.resolve_path(outside, access="read"),
                outside.resolve(),
            )
            self.assertEqual(
                policy.resolve_path("outside.txt", access="read", base=outside_root),
                outside.resolve(),
            )

    def test_formal_policy_reparse_probe_covers_full_local_path(self):
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            read_root = root / "read"
            read_root.mkdir()
            inside = read_root / "nested" / "inside.txt"
            inside.parent.mkdir()
            inside.write_text("inside", encoding="utf-8")
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )
            probed = []
            original_is_symlink = Path.is_symlink

            def record_symlink_probe(candidate):
                probed.append(candidate)
                return original_is_symlink(candidate)

            with patch.object(Path, "is_symlink", record_symlink_probe):
                self.assertEqual(
                    policy.resolve_path(inside, access="read"),
                    inside.resolve(),
                )
            self.assertIn(read_root.resolve(), probed)
            self.assertIn(read_root.parent.resolve(), probed)

    @unittest.skipUnless(os.name == "nt", "Windows mapped-drive policy")
    def test_filesystem_policy_rejects_mapped_network_drives_in_both_modes(self):
        import qcopilots_common.security_policy as security_policy

        with tempfile.TemporaryDirectory() as tmp:
            local_path = Path(tmp) / "ordinary.txt"
            local_path.write_text("ordinary", encoding="utf-8")

            for mode in ("compatible", "formal_restricted"):
                with self.subTest(mode=mode):
                    policy = security_policy.filesystem_policy_from_config(
                        {
                            "mode": mode,
                            "shell": {"enabled": False, "executables": []},
                            "network": {"enabled": False, "allowed_origins": []},
                        }
                    )
                    with patch.object(
                        security_policy,
                        "_windows_drive_type",
                        return_value=4,
                    ), self.assertRaisesRegex(PermissionError, "mapped network"):
                        policy.resolve_path(local_path, access="read")

                    with patch.object(
                        security_policy,
                        "_windows_drive_type",
                        return_value=2,
                    ):
                        self.assertEqual(
                            policy.resolve_path(local_path, access="read"),
                            local_path.resolve(),
                        )

    def test_formal_write_policy_rejects_hardlinks_and_ads(self):
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            read_root = root / "read"
            write_root = root / "write"
            outside_root = root / "outside"
            for directory in (read_root, write_root, outside_root):
                directory.mkdir()
            read_source = read_root / "read-source.bin"
            outside_source = outside_root / "outside-source.bin"
            read_source.write_bytes(b"read source")
            outside_source.write_bytes(b"outside source")
            read_alias = write_root / "read-alias.bin"
            outside_alias = write_root / "outside-alias.bin"
            os.link(read_source, read_alias)
            os.link(outside_source, outside_alias)
            ordinary = write_root / "ordinary.bin"
            ordinary.write_bytes(b"ordinary")
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )

            for alias in (read_alias, outside_alias):
                with self.subTest(alias=alias), self.assertRaisesRegex(
                    PermissionError, "multiple hard links"
                ):
                    policy.resolve_path(alias, access="write")
                with self.subTest(shell_alias=alias), self.assertRaisesRegex(
                    PermissionError, "multiple hard links"
                ):
                    policy.resolve_path(alias, access="shell")

            self.assertEqual(
                policy.resolve_path(read_source, access="read"),
                read_source.resolve(),
            )
            self.assertEqual(
                policy.resolve_path(ordinary, access="write"),
                ordinary.resolve(),
            )
            if os.name == "nt":
                with self.assertRaisesRegex(PermissionError, "alternate data streams"):
                    policy.resolve_path(f"{read_alias}:stream", access="write")

            self.assertEqual(read_source.read_bytes(), b"read source")
            self.assertEqual(outside_source.read_bytes(), b"outside source")

    def test_formal_shell_requires_array_allowlist_and_bounded_path_arguments(self):
        from qcopilots_common.builtin_tools import BuiltinTools
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(tmp)
            outside = Path(outside_tmp)
            outside_script = outside / "outside.py"
            outside_script.write_text("print('outside')", encoding="utf-8")
            policy = filesystem_policy_from_config(
                {
                    "mode": "formal_restricted",
                    "shell": {
                        "enabled": True,
                        "executables": [sys.executable],
                    },
                    "network": {"enabled": False, "allowed_origins": []},
                }
            )
            tools = BuiltinTools(
                root_path=root,
                allowed_roots=[root],
                filesystem_policy=policy,
            )

            with self.assertRaisesRegex(PermissionError, "string shell"):
                tools.exec_shell_command(
                    {"command": f'"{sys.executable}" -c "print(1)"'}
                )
            outside_result = tools.exec_shell_command(
                {"command": [sys.executable, str(outside_script)]}
            )
            self.assertEqual(outside_result["exit_code"], 0)
            self.assertEqual(outside_result["stdout"].strip(), "outside")
            result = tools.exec_shell_command(
                {"command": [sys.executable, "-c", "print('approved')"]}
            )
            self.assertEqual(result["exit_code"], 0)
            self.assertEqual(result["stdout"].strip(), "approved")

    def test_compatible_shell_honors_enabled_state_and_optional_allowlist(self):
        from qcopilots_common.builtin_tools import BuiltinTools
        from qcopilots_common.security_policy import filesystem_policy_from_config

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            disabled = filesystem_policy_from_config(
                {
                    "mode": "compatible",
                    "shell": {"enabled": False, "executables": []},
                    "network": {"enabled": True, "allowed_origins": []},
                }
            )
            with self.assertRaisesRegex(PermissionError, "disabled"):
                BuiltinTools(root_path=root, filesystem_policy=disabled).exec_shell_command(
                    {"command": [sys.executable, "-c", "print('blocked')"]}
                )

            allowed = filesystem_policy_from_config(
                {
                    "mode": "compatible",
                    "shell": {"enabled": True, "executables": [sys.executable]},
                    "network": {"enabled": True, "allowed_origins": []},
                }
            )
            tools = BuiltinTools(root_path=root, filesystem_policy=allowed)
            with self.assertRaisesRegex(PermissionError, "array command"):
                tools.exec_shell_command(
                    {"command": f'"{sys.executable}" -c "print(1)"'}
                )
            result = tools.exec_shell_command(
                {"command": [sys.executable, "-c", "print('allowed')"]}
            )
            self.assertEqual(result["exit_code"], 0)
            self.assertEqual(result["stdout"].strip(), "allowed")

    def test_builtin_service_main_allows_files_but_contains_shell_cwd(self):
        from qcopilots_common.builtin_tools import (
            BUILTIN_ALLOWED_ROOTS_ENV,
            BUILTIN_ALLOW_FULL_ACCESS_ENV,
        )
        from qcopilots_common.mcp_http import ToolError
        from qcopilots_mcp_server_builtin_tools import server as server_module

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp).resolve()
            outside = Path(outside_tmp).resolve()
            source = outside / "source.txt"
            source.write_text("outside", encoding="utf-8")
            previous_cwd = Path.cwd()
            try:
                os.chdir(root)
                with patch.dict(
                    os.environ,
                    {
                        BUILTIN_ALLOWED_ROOTS_ENV: "",
                        BUILTIN_ALLOW_FULL_ACCESS_ENV: "false",
                    },
                ), patch.object(server_module, "run_mcp_server") as runner:
                    server_module.main()
            finally:
                os.chdir(previous_cwd)

            live_tools = {
                tool.name: tool.handler for tool in runner.call_args.kwargs["tools"]
            }
            self.assertEqual(
                live_tools["read_file"]({"path": str(source)})["content"],
                "outside",
            )
            with self.assertRaisesRegex(ToolError, "allowed_roots"):
                live_tools["exec_shell_command"](
                    {
                        "command": [sys.executable, "-c", "print('outside')"],
                        "cwd": str(outside),
                    }
                )

    def test_build_builtin_tools_requires_explicit_full_access(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpJsonRpcServer

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp).resolve()
            source = outside / "source.txt"
            source.write_bytes(b"alpha\nneedle\n")

            tools = build_builtin_tools(root_path=root, allow_full_access=True)
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

    def test_builtin_http_tools_allow_local_client_with_bearer_auth(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpHttpServer

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            auth_token = "builtin-http-test-token"
            authorization_headers = {"Authorization": f"Bearer {auth_token}"}
            server = McpHttpServer(
                name="qcopilots-builtin-http-test",
                version="1.0.0",
                tools=build_builtin_tools(root_path=root),
                host="127.0.0.1",
                port=0,
                auth_token=auth_token,
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
                        headers=authorization_headers,
                    )["bytes_written"],
                    13,
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "read_file",
                        {"path": "notes.txt"},
                        headers=authorization_headers,
                    )["content"],
                    "alpha\nneedle\n",
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "file_glob_search",
                        {"path": ".", "include": "*.txt"},
                        headers=authorization_headers,
                    )["matches"],
                    ["notes.txt"],
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "grep_search",
                        {"path": ".", "pattern": "needle"},
                        headers=authorization_headers,
                    )["matches"][0]["line_number"],
                    2,
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "edit_file",
                        {
                            "path": "notes.txt",
                            "search": "needle",
                            "replace": "pin",
                            "expected_sha256": hashlib.sha256(b"alpha\nneedle\n").hexdigest(),
                        },
                        headers=authorization_headers,
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
                        headers=authorization_headers,
                    )["stdout"].strip(),
                    "qcopilots",
                )
                self.assertIn(
                    "iso",
                    self._post_tool(
                        port,
                        "get_datetime",
                        {"timezone": "UTC"},
                        headers=authorization_headers,
                    ),
                )
            finally:
                httpd.shutdown()
                httpd.server_close()
                thread.join(timeout=5)

    def test_builtin_http_full_access_tools_allow_local_client_with_bearer_auth(self):
        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.mcp_http import McpHttpServer

        with tempfile.TemporaryDirectory() as root_tmp, tempfile.TemporaryDirectory() as outside_tmp:
            root = Path(root_tmp)
            outside = Path(outside_tmp).resolve()
            source = outside / "source.txt"
            source.write_bytes(b"alpha\nneedle\n")
            auth_token = "builtin-http-full-access-test-token"
            authorization_headers = {"Authorization": f"Bearer {auth_token}"}
            server = McpHttpServer(
                name="qcopilots-builtin-http-full-access-test",
                version="1.0.0",
                tools=build_builtin_tools(root_path=root, allow_full_access=True),
                host="127.0.0.1",
                port=0,
                auth_token=auth_token,
            )
            httpd = server.create_http_server()
            port = httpd.server_address[1]
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()
            try:
                self.assertEqual(
                    self._post_tool(
                        port,
                        "read_file",
                        {"path": str(source)},
                        headers=authorization_headers,
                    )["content"],
                    "alpha\nneedle\n",
                )
                self.assertEqual(
                    self._post_tool(
                        port,
                        "write_file",
                        {"path": str(outside / "written.txt"), "content": "new\n"},
                        headers=authorization_headers,
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
                            "expected_sha256": hashlib.sha256(b"alpha\nneedle\n").hexdigest(),
                        },
                        headers=authorization_headers,
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
        request_headers = {
            "Accept": "application/json, text/event-stream",
            "Content-Type": "application/json",
        }
        if headers:
            request_headers.update(headers)
        sessions = getattr(self, "_http_sessions", None)
        if sessions is None:
            sessions = {}
            self._http_sessions = sessions
        session_id = sessions.get(port)
        if session_id is None:
            initialize_payload = json.dumps(
                {
                    "jsonrpc": "2.0",
                    "id": "initialize",
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-06-18",
                        "capabilities": {},
                        "clientInfo": {
                            "name": "builtin-tools-test",
                            "version": "1.0",
                        },
                    },
                }
            ).encode("utf-8")
            initialize_request = Request(
                f"http://127.0.0.1:{port}/mcp",
                data=initialize_payload,
                headers=request_headers,
                method="POST",
            )
            with urlopen(initialize_request, timeout=5) as response:
                response.read()
                session_id = response.headers["mcp-session-id"]
            sessions[port] = session_id
            initialized_headers = dict(request_headers)
            initialized_headers.update(
                {
                    "MCP-Protocol-Version": "2025-06-18",
                    "MCP-Session-Id": session_id,
                }
            )
            initialized_request = Request(
                f"http://127.0.0.1:{port}/mcp",
                data=json.dumps(
                    {
                        "jsonrpc": "2.0",
                        "method": "notifications/initialized",
                    }
                ).encode("utf-8"),
                headers=initialized_headers,
                method="POST",
            )
            with urlopen(initialized_request, timeout=5) as response:
                self.assertEqual(response.status, 202)
                response.read()

        payload = json.dumps(
            {
                "jsonrpc": "2.0",
                "id": 101,
                "method": "tools/call",
                "params": {"name": name, "arguments": arguments},
            }
        ).encode("utf-8")
        request_headers.update(
            {
                "MCP-Protocol-Version": "2025-06-18",
                "MCP-Session-Id": session_id,
            }
        )
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
