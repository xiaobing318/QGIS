"""Startup cleanup of precisely owned file transfer summary staging files.

All files and lock handles belong to automatically cleaned temporary directories.
The subprocess cases exercise OS lease release without opening any connection.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
import uuid
from pathlib import Path


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))

from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general.file_transfer_config import FileTransferConfigStore
from qcopilots_mcp_server_processing_general.file_transfer_jobs import FileTransferJobManager, _TransferLease
from qcopilots_mcp_server_processing_general.file_transfer_storage import DiskTransferStore


class TestFileTransferOrphanSummaryCleanup(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-orphans-")
        self.addCleanup(temporary.cleanup)
        self.config_root = Path(temporary.name) / "config"
        self.root = self.config_root / "file_transfers"
        self.disk = DiskTransferStore(self.root)
        self.transfer_id = str(uuid.uuid4())

    def start_manager(self):
        manager = FileTransferJobManager(
            config_store=FileTransferConfigStore(self.config_root),
            backend=object(),
            policy=FilesystemPolicy(),
        )
        self.addCleanup(manager.shutdown)
        return manager

    def temporary_path(self, *, directory=None, transfer_id=None):
        owner = transfer_id or self.transfer_id
        return (directory or self.root) / (owner + ".json.tmp-" + owner + "-" + uuid.uuid4().hex)

    def run_child(self, code, *arguments):
        result = subprocess.run(
            [sys.executable, "-B", "-c", code, str(PLUGINS_ROOT), *map(str, arguments)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=20,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1", "PYTHONIOENCODING": "utf-8"},
        )
        return result

    def test_hard_exit_before_first_summary_replace_is_cleaned(self):
        code = """
import os, sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from qcopilots_mcp_server_processing_general.file_transfer_jobs import _TransferLease
from qcopilots_mcp_server_processing_general.file_transfer_storage import DiskTransferStore
root, owner, archived = Path(sys.argv[2]), sys.argv[3], sys.argv[4] == '1'
store = DiskTransferStore(root)
lease = _TransferLease(root / (owner + '.lock'))
assert lease.acquire()
os.replace = lambda *args, **kwargs: os._exit(74)
store.write_summary({'transfer_id': owner, 'schema_version': 2,
                     'archived': archived, 'attempts': []})
"""
        for archived in (False, True):
            with self.subTest(archived=archived):
                owner = str(uuid.uuid4())
                result = self.run_child(code, self.root, owner, int(archived))
                self.assertEqual(result.returncode, 74, result.stderr)
                directory = self.disk.history_root if archived else self.root
                temporary = list(directory.glob(owner + ".json.tmp-*"))
                self.assertEqual(len(temporary), 1)
                self.assertFalse((directory / (owner + ".json")).exists())
                self.assertEqual(self.start_manager()._records, {})
                self.assertFalse(temporary[0].exists())

    def test_active_process_lease_preserves_new_and_legacy_staging_files(self):
        current = self.temporary_path()
        legacy = self.root / (self.transfer_id + ".tmp-" + uuid.uuid4().hex)
        current.write_bytes(b'{"partial":')
        legacy.write_bytes(b"old partial summary")
        lease = _TransferLease(self.root / (self.transfer_id + ".lock"))
        self.assertTrue(lease.acquire())
        code = """
import sys
sys.path.insert(0, sys.argv[1])
from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general.file_transfer_config import FileTransferConfigStore
from qcopilots_mcp_server_processing_general.file_transfer_jobs import FileTransferJobManager
manager = FileTransferJobManager(config_store=FileTransferConfigStore(sys.argv[2]),
                        backend=object(), policy=FilesystemPolicy())
manager.shutdown()
"""
        try:
            result = self.run_child(code, self.config_root)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(current.read_bytes(), b'{"partial":')
            self.assertEqual(legacy.read_bytes(), b"old partial summary")
        finally:
            lease.close()
        self.start_manager()
        self.assertFalse(current.exists())
        self.assertFalse(legacy.exists())

    def test_invalid_names_and_unrelated_files_are_preserved(self):
        other = str(uuid.uuid4())
        owner = self.transfer_id
        token = "0123456789abcdef" * 2
        names = [
            owner + ".json.tmp-" + other + "-" + token,
            owner + ".json.tmp-" + token,
            owner + ".json.tmp-" + owner + "-" + token[:-1],
            owner + ".json.tmp-" + owner + "-" + token + "0",
            owner + ".json.tmp-" + owner + "-" + token.upper(),
            owner + ".json.tmp-" + owner + "-" + token + ".bak",
            owner + ".json.bak.tmp-" + owner + "-" + token,
            owner + ".tmp-" + other + "-" + token,
            owner + ".tmp-" + token.upper(),
            "notes.tmp-" + token,
            "notes.json.tmp-" + owner + "-" + token,
            str(uuid.uuid1()) + ".tmp-" + token,
        ]
        paths = []
        for directory in (self.root, self.disk.history_root):
            for name in names:
                path = directory / name
                path.write_bytes(b"unrelated user content")
                paths.append(path)
        self.start_manager()
        for path in paths:
            with self.subTest(name=path.name, directory=path.parent):
                self.assertEqual(path.read_bytes(), b"unrelated user content")
        self.assertEqual(list(self.root.glob("*.lock")), [])

    def test_valid_temporary_name_does_not_delete_directory(self):
        directory = self.temporary_path()
        directory.mkdir()
        payload = directory / "keep.txt"
        payload.write_text("keep", encoding="utf-8")
        self.start_manager()
        self.assertEqual(payload.read_text(encoding="utf-8"), "keep")

    def test_legacy_orphans_are_cleaned_in_both_summary_directories(self):
        paths = []
        for directory in (self.root, self.disk.history_root):
            path = directory / (self.transfer_id + ".tmp-" + uuid.uuid4().hex)
            path.write_bytes(b"unfinished legacy summary")
            paths.append(path)
        self.start_manager()
        self.assertTrue(all(not path.exists() for path in paths))


if __name__ == "__main__":
    unittest.main()
