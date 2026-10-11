"""Deterministic SSH/SFTP transfer and recovery tests.

The fake server owns a TemporaryDirectory and never opens a network connection.
Faults occur at acknowledged I/O boundaries rather than depending on timing.
"""

from __future__ import annotations

import copy
import errno
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import uuid
from pathlib import Path, PurePosixPath
from types import SimpleNamespace
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
sys.path.insert(0, str(PLUGINS_ROOT))

from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError
from qcopilots_mcp_server_processing_general.file_transfer_jobs import FileTransferJobManager


class FakeSftpFile:
    """A real isolated file behind the minimal Paramiko file contract."""

    def __init__(self, sftp, path, mode):
        self.sftp = sftp
        self.path = path
        mode = mode.replace("wx", "x")
        if "b" not in mode:
            mode += "b"
        self.stream = sftp.local(path).open(mode, buffering=0)

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()

    def read(self, size=-1):
        self.sftp.check_connection()
        data = self.stream.read(size)
        self.sftp.backend.bytes_read += len(data)
        guard = self.sftp.backend.read_disconnect_guard
        if guard is not None and guard():
            self.sftp.backend.read_disconnect_guard = None
            self.sftp.closed = True
            raise EOFError("Injected disconnect after a local partial was written")
        return data

    def write(self, data):
        self.sftp.check_connection()
        count = self.stream.write(data)
        self.sftp.backend.bytes_written += count
        self.sftp.backend.write_observed.set()
        if self.sftp.backend.write_marker is not None:
            self.sftp.backend.write_marker.write_text("written", encoding="utf-8")
        gate = self.sftp.backend.write_gate
        if gate is not None:
            if not gate.wait(5):
                raise TimeoutError("Fake SFTP write gate was not released")
            self.sftp.check_connection()
        threshold = self.sftp.backend.disconnect_after_bytes
        if threshold is not None and self.sftp.backend.bytes_written >= threshold:
            self.sftp.backend.disconnect_after_bytes = None
            self.sftp.closed = True
            raise EOFError("Injected disconnect after server accepted the write")
        return count

    def seek(self, offset, whence=0):
        return self.stream.seek(offset, whence)

    def tell(self):
        return self.stream.tell()

    def flush(self):
        return self.stream.flush()

    def stat(self):
        return self.sftp.attrs(self.sftp.local(self.path))

    def set_pipelined(self, _enabled=True):
        pass

    def settimeout(self, _timeout):
        pass

    def close(self):
        self.stream.close()


class FakeSftp:
    def __init__(self, backend):
        self.backend = backend
        self.closed = False

    def check_connection(self):
        if self.closed or not self.backend.online:
            raise EOFError("Fake SFTP server disconnected")

    def local(self, path):
        value = str(path).replace("\\", "/")
        parts = PurePosixPath(value).parts
        if ".." in parts:
            raise AssertionError(f"Path escaped fake server: {value}")
        # Drive letters are directory names in this isolated fake namespace.
        parts = [item.replace(":", "_") for item in parts if item != "/"]
        candidate = self.backend.root.joinpath(*parts)
        candidate.relative_to(self.backend.root)
        return candidate

    @staticmethod
    def attrs(path):
        value = path.lstat()
        return SimpleNamespace(
            filename=path.name,
            st_mode=value.st_mode,
            st_size=value.st_size,
            st_mtime=int(value.st_mtime),
            st_atime=int(value.st_atime),
        )

    def stat(self, path):
        self.check_connection()
        return self.attrs(self.local(path))

    lstat = stat

    def listdir_attr(self, path):
        self.check_connection()
        return [self.attrs(item) for item in sorted(self.local(path).iterdir())]

    def listdir(self, path):
        return [item.filename for item in self.listdir_attr(path)]

    def normalize(self, path):
        self.check_connection()
        return str(PurePosixPath(str(path).replace("\\", "/")))

    def mkdir(self, path, mode=0o777):
        self.check_connection()
        self.local(path).mkdir(mode=mode)

    def open(self, path, mode="r", bufsize=-1):
        del bufsize
        self.check_connection()
        self.backend.operations.append(("open", str(path), mode))
        return FakeSftpFile(self, path, mode)

    file = open

    def remove(self, path):
        self.check_connection()
        if self.backend.fail_remove:
            raise PermissionError("Injected cleanup refusal")
        self.backend.operations.append(("remove", str(path)))
        self.local(path).unlink()

    unlink = remove

    def rmdir(self, path):
        self.check_connection()
        self.local(path).rmdir()

    def rename(self, source, target):
        self.check_connection()
        if self.backend.before_rename is not None:
            callback = self.backend.before_rename
            self.backend.before_rename = None
            callback(self.local(target))
        if self.local(target).exists():
            raise FileExistsError(errno.EEXIST, "Target already exists", target)
        self.backend.operations.append(("rename", str(source), str(target)))
        self.local(source).rename(self.local(target))
        self.after_commit()

    def posix_rename(self, source, target):
        self.check_connection()
        if not self.backend.atomic_overwrite:
            raise OSError(errno.EOPNOTSUPP, "Operation unsupported")
        self.backend.operations.append(("posix_rename", str(source), str(target)))
        self.local(source).replace(self.local(target))
        self.after_commit()

    def after_commit(self):
        if self.backend.disconnect_after_commit:
            self.backend.disconnect_after_commit = False
            self.closed = True
            raise EOFError("Injected disconnect after atomic publication")

    def utime(self, path, times):
        self.check_connection()
        os.utime(self.local(path), times)

    def get_channel(self):
        return self

    def settimeout(self, _timeout):
        pass

    def close(self):
        self.closed = True
        if self.backend.write_gate is not None:
            self.backend.write_gate.set()


class FakeBackend:
    fingerprint = "SHA256:" + "A" * 43

    def __init__(self, root):
        self.root = Path(root)
        self.root.mkdir(exist_ok=True)
        self.online = True
        self.atomic_overwrite = True
        self.fail_remove = False
        self.disconnect_after_bytes = None
        self.bytes_written = 0
        self.bytes_read = 0
        self.operations = []
        self.connections = []
        self.write_observed = threading.Event()
        self.write_gate = None
        self.write_marker = None
        self.before_rename = None
        self.disconnect_after_commit = False
        self.read_disconnect_guard = None

    def connect(self, config, expected_fingerprint=None, cancel_control=None):
        if cancel_control is not None:
            cancel_control.check()
        if not self.online:
            raise OSError("Fake SFTP server unavailable")
        if expected_fingerprint not in (None, "", self.fingerprint):
            raise ValueError("Host key fingerprint changed")
        sftp = FakeSftp(self)
        session = SimpleNamespace(
            sftp=sftp,
            fingerprint=self.fingerprint,
            close=sftp.close,
        )
        self.connections.append((copy.deepcopy(config), session))
        if cancel_control is not None:
            cancel_control.register(session)
        return session

    def local(self, remote):
        return FakeSftp(self).local(remote)

    def partials(self):
        return list(self.root.rglob(".qcopilots-file-transfer-*.part"))


class MemoryConfigStore:
    """Isolate engine behavior from configuration UI and platform defaults."""

    def __init__(self, root):
        self.root = Path(root)
        self.root.mkdir(exist_ok=True)
        self.path = self.root / "file_transfer_config.json"
        self.config = {
            "config_version": 1,
            "host": "test.invalid",
            "port": 22,
            "username": "test-user",
            "password": "test-password-do-not-log",
            "upload_sources": [],
            "upload_destination": "/uploads",
            "download_sources": [],
            "download_destination": "",
            "connect_timeout": 1,
            "io_timeout": 1,
            "known_hosts_file": str(self.root / "known_hosts"),
            "max_bytes_per_second": 0,
        }

    def load(self, require_connection=False):
        del require_connection
        return copy.deepcopy(self.config)


class TestQCopilotsMcpServerFileTransferJobs(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-test-")
        self.addCleanup(self.temporary.cleanup)
        self.thread_errors = []
        exception_hook = mock.patch(
            "threading.excepthook",
            side_effect=lambda args: self.thread_errors.append(
                f"{args.thread.name}: {args.exc_type.__name__}: {args.exc_value}"
            ),
        )
        exception_hook.start()
        self.addCleanup(exception_hook.stop)
        self.addCleanup(self.assert_no_worker_exceptions)
        self.root = Path(self.temporary.name)
        self.local = self.root / "local"
        self.local.mkdir()
        self.store = MemoryConfigStore(self.root / "config")
        self.backend = FakeBackend(self.root / "remote")
        self.backend.local("/uploads").mkdir()
        self.managers = []
        self.addCleanup(self.close_managers)
        self.manager = self.new_manager()

    def new_manager(self, **kwargs):
        kwargs.setdefault("policy", FilesystemPolicy())
        manager = FileTransferJobManager(
            config_store=self.store, backend=self.backend, **kwargs
        )
        self.managers.append(manager)
        return manager

    def close_managers(self):
        if self.backend.write_gate is not None:
            self.backend.write_gate.set()
        self.backend.online = True
        self.backend.fail_remove = False
        for manager in reversed(self.managers):
            manager.shutdown(timeout=2)

    def assert_no_worker_exceptions(self):
        self.assertEqual(self.thread_errors, [], "Uncaught transfer worker exception")

    def source(self, name="data.bin", data=None):
        path = self.local / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data if data is not None else bytes(range(256)) * 4096)
        return path

    def wait(self, value, states=None, timeout=8):
        job_id = value["job_id"] if isinstance(value, dict) else value
        states = states or {"succeeded", "failed", "cancelled"}
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            snapshot = self.manager.get_job(job_id)
            if snapshot["state"] in states:
                return snapshot
            time.sleep(0.01)
        self.fail(f"Job did not reach {states}: {snapshot}")

    def upload(self, sources, **kwargs):
        return self.manager.start(
            "upload", [str(path) for path in sources], "/uploads", **kwargs
        )

    def assert_success(self, value):
        snapshot = self.wait(value)
        self.assertEqual(snapshot["state"], "succeeded", snapshot)
        self.assertFalse(snapshot["resumable"], snapshot)
        self.assertTrue(snapshot["cleanup"]["complete"], snapshot)
        return snapshot

    @staticmethod
    def tree(root):
        return {
            item.relative_to(root).as_posix(): (
                "directory"
                if item.is_dir()
                else hashlib.sha256(item.read_bytes()).hexdigest()
            )
            for item in root.rglob("*")
        }

    def interrupted_upload(self):
        source = self.source()
        self.backend.disconnect_after_bytes = 65536
        snapshot = self.wait(self.upload([source]))
        self.assertEqual(snapshot["state"], "failed", snapshot)
        self.assertTrue(snapshot["resumable"], snapshot)
        self.assertEqual(len(self.backend.partials()), 1)
        return source, snapshot

    def test_upload_and_download_mixed_files_directories_and_unicode(self):
        loose = self.source("独立 文件.txt", "地图数据\n".encode())
        tree = self.local / "目录 with spaces"
        (tree / "empty-directory").mkdir(parents=True)
        (tree / "nested").mkdir()
        (tree / "nested" / "zero.bin").write_bytes(b"")
        (tree / "nested" / "二进制.bin").write_bytes(bytes(range(256)) * 257)
        self.assert_success(self.upload([loose, tree]))
        self.assertEqual(self.tree(self.local), self.tree(self.backend.local("/uploads")))
        destination = self.root / "download"
        destination.mkdir()
        self.assert_success(
            self.manager.start(
                "download",
                ["/uploads/独立 文件.txt", "/uploads/目录 with spaces"],
                str(destination),
            )
        )
        self.assertEqual(self.tree(self.local), self.tree(destination))
        self.assertEqual(self.backend.partials(), [])

    def test_default_conflict_preserves_existing_target(self):
        source = self.source(data=b"new data")
        target = self.backend.local("/uploads/data.bin")
        target.write_bytes(b"original data")
        snapshot = self.wait(self.upload([source]))
        self.assertEqual(snapshot["state"], "failed", snapshot)
        self.assertEqual(target.read_bytes(), b"original data")
        self.assertEqual(self.backend.bytes_written, 0)

    def test_atomic_overwrite_replaces_complete_file(self):
        source = self.source(data=b"replacement")
        target = self.backend.local("/uploads/data.bin")
        target.write_bytes(b"original")
        self.assert_success(self.upload([source], overwrite=True))
        self.assertEqual(target.read_bytes(), source.read_bytes())
        self.assertTrue(any(op[0] == "posix_rename" for op in self.backend.operations))

    def test_target_created_during_transfer_is_not_overwritten_by_default(self):
        source = self.source(data=b"new content")
        self.backend.before_rename = lambda path: path.write_bytes(b"concurrent writer")
        snapshot = self.wait(self.upload([source]))
        self.assertEqual(snapshot["state"], "failed", snapshot)
        self.assertEqual(
            self.backend.local("/uploads/data.bin").read_bytes(), b"concurrent writer"
        )

    def test_mixed_resume_keeps_completed_files_and_empty_directories(self):
        small = self.source("a-small.txt", b"completed first")
        directory = self.local / "z-tree"
        (directory / "empty").mkdir(parents=True)
        self.source("z-tree/payload.bin")
        self.backend.disconnect_after_bytes = 65536
        first = self.wait(self.upload([small, directory]))
        self.assertEqual(first["state"], "failed", first)
        self.assertTrue(first["resumable"], first)
        self.assertEqual(
            self.backend.local("/uploads/a-small.txt").read_bytes(), small.read_bytes()
        )
        self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(self.tree(self.local), self.tree(self.backend.local("/uploads")))

    def test_published_file_corruption_is_detected_during_directory_resume(self):
        small = self.source("a-small.txt", b"completed first")
        large = self.source("z-large.bin")
        self.backend.disconnect_after_bytes = 65536
        first = self.wait(self.upload([small, large]))
        self.assertTrue(first["resumable"], first)
        self.backend.local("/uploads/a-small.txt").write_bytes(b"corrupted first")
        resumed = self.wait(self.manager.resume_job(first["job_id"]))
        self.assertEqual(resumed["state"], "failed", resumed)
        self.assertFalse(self.backend.local("/uploads/z-large.bin").exists())

    def test_lost_rename_response_recovers_without_overwriting_published_file(self):
        source = self.source(data=b"published exactly once")
        self.backend.disconnect_after_commit = True
        first = self.wait(self.upload([source]))
        target = self.backend.local("/uploads/data.bin")
        self.assertEqual(target.read_bytes(), source.read_bytes())
        if first["state"] != "succeeded":
            self.assertEqual(first["state"], "failed", first)
            self.assertTrue(first["resumable"], first)
            self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(target.read_bytes(), source.read_bytes())
        self.assertEqual(sum(op[0] == "rename" for op in self.backend.operations), 1)

    def test_unsupported_atomic_overwrite_preserves_original(self):
        self.backend.atomic_overwrite = False
        source = self.source(data=b"replacement")
        target = self.backend.local("/uploads/data.bin")
        target.write_bytes(b"original")
        snapshot = self.wait(self.upload([source], overwrite=True))
        self.assertEqual(snapshot["state"], "failed", snapshot)
        self.assertEqual(target.read_bytes(), b"original")
        self.assertFalse(
            any(
                op[0] == "remove" and op[1] == "/uploads/data.bin"
                for op in self.backend.operations
            )
        )

    def test_disconnect_after_accepted_write_resumes_without_duplicating_bytes(self):
        source, first = self.interrupted_upload()
        recovered = self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertNotEqual(recovered["job_id"], first["job_id"])
        self.assertEqual(recovered["resumes_job_id"], first["job_id"])
        self.assertEqual(recovered["transfer_id"], first["transfer_id"])
        self.assertGreater(recovered["progress"]["bytes_reused"], 0)
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_download_disconnect_resumes_from_verified_local_partial(self):
        remote_source = self.backend.local("/uploads/remote.bin")
        remote_source.write_bytes(bytes(range(256)) * 4096)
        destination = self.root / "download"
        destination.mkdir()
        self.backend.read_disconnect_guard = lambda: any(
            path.stat().st_size > 0
            for path in destination.glob(".qcopilots-file-transfer-*.part")
        )
        first = self.wait(
            self.manager.start("download", ["/uploads/remote.bin"], str(destination))
        )
        self.assertEqual(first["state"], "failed", first)
        self.assertTrue(first["resumable"], first)
        resumed = self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertGreater(resumed["progress"]["bytes_reused"], 0)
        self.assertEqual((destination / "remote.bin").read_bytes(), remote_source.read_bytes())
        self.assertEqual(list(destination.glob(".qcopilots-file-transfer-*.part")), [])

    def test_download_cancel_interrupts_rate_limit_wait_and_retains_partial(self):
        self.backend.local("/uploads/remote.bin").write_bytes(bytes(range(256)) * 4096)
        destination = self.root / "download"
        destination.mkdir()
        self.store.config["max_bytes_per_second"] = 65536
        started = self.manager.start(
            "download", ["/uploads/remote.bin"], str(destination)
        )
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            snapshot = self.manager.get_job(started["job_id"])
            if snapshot["progress"]["bytes_transferred"] > 0:
                break
            time.sleep(0.01)
        self.assertGreater(snapshot["progress"]["bytes_transferred"], 0, snapshot)
        self.manager.cancel_job(started["job_id"])
        cancelled = self.wait(started, timeout=2)
        self.assertEqual(cancelled["state"], "cancelled", cancelled)
        self.assertTrue(cancelled["cleanup"]["complete"], cancelled)
        self.assertTrue(cancelled["resumable"])
        self.assertFalse(cancelled["cleanup_pending"])
        self.assertEqual(len(list(destination.glob(".qcopilots-file-transfer-*.part"))), 1)
        self.store.config["max_bytes_per_second"] = 0
        self.assert_success(self.manager.resume_job(started["job_id"]))
        self.assertEqual((destination / "remote.bin").read_bytes(), self.backend.local("/uploads/remote.bin").read_bytes())

    def test_resume_rejects_same_size_source_mutation_with_original_timestamp(self):
        source, first = self.interrupted_upload()
        original_stat = source.stat()
        changed = bytearray(source.read_bytes())
        changed[-1] ^= 0xFF
        source.write_bytes(changed)
        os.utime(source, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        recovered = self.wait(self.manager.resume_job(first["job_id"]))
        self.assertEqual(recovered["state"], "failed", recovered)
        self.assertFalse(self.backend.local("/uploads/data.bin").exists())

    def test_resume_rejects_same_size_partial_corruption(self):
        _source, first = self.interrupted_upload()
        partial = self.backend.partials()[0]
        changed = bytearray(partial.read_bytes())
        changed[0] ^= 0xFF
        partial.write_bytes(changed)
        recovered = self.wait(self.manager.resume_job(first["job_id"]))
        self.assertEqual(recovered["state"], "failed", recovered)
        self.assertFalse(self.backend.local("/uploads/data.bin").exists())

    def test_cancel_retains_partial_and_can_resume(self):
        self.backend.write_gate = threading.Event()
        started = self.upload([self.source()])
        self.assertTrue(self.backend.write_observed.wait(5))
        self.manager.cancel_job(started["job_id"])
        self.backend.write_gate.set()
        snapshot = self.wait(started)
        self.assertEqual(snapshot["state"], "cancelled", snapshot)
        self.assertTrue(snapshot["resumable"], snapshot)
        self.assertTrue(snapshot["cleanup"]["complete"], snapshot)
        self.assertFalse(snapshot["cleanup_pending"])
        self.assertEqual(len(self.backend.partials()), 1)
        resumed = self.assert_success(self.manager.resume_job(started["job_id"]))
        self.assertEqual(resumed["transfer_id"], snapshot["transfer_id"])
        self.assertNotEqual(resumed["job_id"], snapshot["job_id"])

    def test_persisted_interruption_survives_new_manager(self):
        source, first = self.interrupted_upload()
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        restored = self.manager.get_job(first["job_id"])
        self.assertTrue(restored["resumable"], restored)
        self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_transfer_namespace_and_stage_ownership_reject_legacy_prefix(self):
        _source, first = self.interrupted_upload()
        self.assertEqual(first["category"], "file_transfer")
        self.assertEqual(self.manager.root, self.store.root / "file_transfers")
        record = copy.deepcopy(self.manager._fresh_record(
            self.manager._records[first["transfer_id"]], details=True))
        self.manager._validate_entries(record)
        entry = next(item for item in record["files"] if item["kind"] == "file")
        self.assertIn(".qcopilots-file-transfer-", entry["stage"])
        current_partial = self.backend.local(entry["stage"])
        original = current_partial.read_bytes()
        entry["stage"] = entry["stage"].replace(".qcopilots-file-transfer-", ".qcopilots-scp-", 1)
        legacy_partial = self.backend.local(entry["stage"])
        legacy_partial.write_bytes(b"unrelated legacy partial")

        with self.assertRaises(FileTransferError) as failure:
            self.manager._validate_entries(record)

        self.assertEqual(failure.exception.code, "invalid_checkpoint")
        self.assertEqual(current_partial.read_bytes(), original)
        self.assertEqual(legacy_partial.read_bytes(), b"unrelated legacy partial")

    def test_stale_manager_cannot_resume_transfer_completed_by_another_manager(self):
        source, first = self.interrupted_upload()
        stale_manager = self.new_manager()
        finished = self.assert_success(self.manager.resume_job(first["job_id"]))
        bytes_written = self.backend.bytes_written
        with self.assertRaises(FileTransferError) as failure:
            stale_manager.resume_job(first["job_id"])
        self.assertIn(failure.exception.code, {"transfer_complete", "stale_checkpoint"})
        self.assertEqual(self.backend.bytes_written, bytes_written)
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())
        reloaded_manager = self.new_manager()
        self.assertEqual(reloaded_manager.get_job(finished["job_id"])["state"], "succeeded")
        checkpoint = self.store.root / "file_transfers" / (first["transfer_id"] + ".json")
        self.assertFalse(checkpoint.exists())

    def crash_worker(self, cancel=False):
        marker = self.root / "write-started"
        process = subprocess.Popen(
            [
                sys.executable,
                str(Path(__file__).resolve()),
                "--cancel-crash-worker" if cancel else "--crash-worker",
                str(self.root),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        def stop_worker():
            if process.poll() is None:
                process.kill()
            process.communicate(timeout=5)

        self.addCleanup(stop_worker)
        deadline = time.monotonic() + 5
        while (
            not marker.exists()
            and process.poll() is None
            and time.monotonic() < deadline
        ):
            time.sleep(0.01)
        if not marker.exists():
            stop_worker()
            self.fail("Isolated file transfer worker failed before its first write")
        if cancel:
            (self.root / "cancel-requested").write_text("cancel", encoding="utf-8")
            cancel_marker = self.root / "cancel-persisted"
            deadline = time.monotonic() + 5
            while not cancel_marker.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(cancel_marker.exists(), "Cancellation was not persisted")
        process.kill()
        process.communicate(timeout=5)

    def test_forced_process_exit_preserves_recoverable_checkpoint(self):
        source = self.source()
        self.crash_worker()
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        jobs = self.manager.list_jobs()["jobs"]
        self.assertEqual(len(jobs), 1, jobs)
        self.assertTrue(jobs[0]["resumable"], jobs)
        self.assert_success(self.manager.resume_job(jobs[0]["job_id"]))
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_crash_after_cancel_intent_preserves_resumability(self):
        self.source()
        self.crash_worker(cancel=True)
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        jobs = self.manager.list_jobs()["jobs"]
        self.assertEqual(len(jobs), 1, jobs)
        interrupted = jobs[0]
        self.assertEqual(interrupted["state"], "cancelled", interrupted)
        self.assertTrue(interrupted["resumable"], interrupted)
        self.assertTrue(interrupted["cleanup"]["complete"], interrupted)
        self.assertEqual(len(self.backend.partials()), 1)
        self.assert_success(self.manager.resume_job(interrupted["job_id"]))

    def test_resume_idempotency_has_only_one_writer(self):
        _source, first = self.interrupted_upload()
        self.backend.write_observed.clear()
        self.backend.write_gate = threading.Event()
        resumed = self.manager.resume_job(first["job_id"], client_request_id="resume-one")
        self.assertTrue(self.backend.write_observed.wait(5))
        duplicate = self.manager.resume_job(first["job_id"], client_request_id="resume-one")
        self.assertEqual(resumed["job_id"], duplicate["job_id"])
        self.backend.write_gate.set()
        self.assert_success(resumed)

    def test_concurrent_jobs_for_same_destination_are_rejected(self):
        self.backend.write_gate = threading.Event()
        started = self.upload([self.source("one.bin")])
        self.assertTrue(self.backend.write_observed.wait(5))
        second_source = self.source("two.bin")
        with self.assertRaises(FileTransferError):
            self.upload([second_source])
        other_manager = self.new_manager()
        with self.assertRaises(FileTransferError):
            other_manager.start("upload", [str(second_source)], "/uploads")
        self.assertEqual(len(self.backend.connections), 1)
        self.backend.write_gate.set()
        self.assert_success(started)

    def test_overlapping_destination_jobs_are_rejected(self):
        self.backend.write_gate = threading.Event()
        started = self.upload([self.source("one.bin")])
        self.assertTrue(self.backend.write_observed.wait(5))
        with self.assertRaises(FileTransferError):
            self.manager.start(
                "upload", [str(self.source("two.bin"))], "/uploads/nested"
            )
        self.backend.write_gate.set()
        self.assert_success(started)

    def test_two_nonoverlapping_destinations_can_run_together(self):
        self.backend.write_gate = threading.Event()
        first = self.upload([self.source("one.bin")])
        self.assertTrue(self.backend.write_observed.wait(5))
        self.backend.local("/other-uploads").mkdir()
        second = self.manager.start(
            "upload", [str(self.source("two.bin"))], "/other-uploads"
        )
        active = self.manager.list_jobs(states=["queued", "running"])["jobs"]
        self.assertEqual(len(active), 2, active)
        self.backend.write_gate.set()
        self.assert_success(first)
        self.assert_success(second)

    def test_malformed_checkpoint_roots_do_not_prevent_manager_start(self):
        self.manager.shutdown(timeout=2)
        checkpoint_directory = self.store.root / "file_transfers"
        for value in ([], None, "text", {"schema_version": 1}):
            (checkpoint_directory / f"{uuid.uuid4()}.json").write_text(
                json.dumps(value), encoding="utf-8"
            )
        self.manager = self.new_manager()
        self.assertEqual(self.manager.list_jobs()["jobs"], [])
        self.assert_success(self.upload([self.source(data=b"valid after corrupt record")]))

    def test_success_removes_checkpoint_and_retains_queryable_history(self):
        finished = self.assert_success(self.upload([self.source(data=b"history")]))
        self.manager.shutdown(timeout=2)
        checkpoint = self.store.root / "file_transfers" / (finished["transfer_id"] + ".json")
        self.assertFalse(checkpoint.exists())
        self.manager = self.new_manager()
        self.assertEqual(self.manager.get_job(finished["job_id"])["state"], "succeeded")

    def test_cancel_retains_checkpoint_and_remains_resumable_after_restart(self):
        self.backend.write_gate = threading.Event()
        started = self.upload([self.source()])
        self.assertTrue(self.backend.write_observed.wait(5))
        self.manager.cancel_job(started["job_id"])
        self.backend.write_gate.set()
        finished = self.wait(started)
        self.assertEqual(finished["state"], "cancelled", finished)
        self.manager.shutdown(timeout=2)
        checkpoint = self.store.root / "file_transfers" / (finished["transfer_id"] + ".json")
        self.assertTrue(checkpoint.exists())
        self.manager = self.new_manager()
        restored = self.manager.get_job(finished["job_id"])
        self.assertEqual(restored["state"], "cancelled", restored)
        self.assertTrue(restored["resumable"], restored)
        self.assert_success(self.manager.resume_job(finished["job_id"]))

    def test_schedule_checkpoint_failure_does_not_leave_a_phantom_active_job(self):
        source = self.source(data=b"can retry")
        with mock.patch.object(
            self.manager, "_persist", side_effect=OSError("Injected full checkpoint disk")
        ):
            with self.assertRaises((OSError, FileTransferError)):
                self.upload([source], client_request_id="storage-retry")
        self.assertEqual(
            self.manager.list_jobs(states=["queued", "running", "cancelling"])["jobs"],
            [],
        )
        self.assertEqual(self.backend.connections, [])
        self.assert_success(self.upload([source], client_request_id="storage-retry"))

    def test_runtime_checkpoint_failure_stops_writer_and_can_recover_after_restart(self):
        source = self.source()
        self.backend.write_gate = threading.Event()
        started = self.upload([source])
        self.assertTrue(self.backend.write_observed.wait(5))
        with mock.patch.object(
            self.manager, "_persist", side_effect=OSError("Injected full checkpoint disk")
        ):
            self.backend.write_gate.set()
            failed = self.wait(started)
            self.assertEqual(failed["state"], "failed", failed)
            self.manager.shutdown(timeout=2)
        self.assertEqual(self.thread_errors, [])
        self.manager = self.new_manager()
        restored = self.manager.get_job(started["job_id"])
        self.assertTrue(restored["resumable"], restored)
        self.assert_success(self.manager.resume_job(started["job_id"]))
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_password_is_absent_from_jobs_and_recovery_records(self):
        _source, first = self.interrupted_upload()
        password = self.store.config["password"]
        self.assertNotIn(password, json.dumps(first))
        self.assertNotIn(password, json.dumps(self.manager.list_jobs()))
        records = [path for path in (self.store.root / "file_transfers").rglob("*")
                   if path.is_file() and path.suffix in {".json", ".jsonl"}]
        self.assertTrue(records)
        for record in records:
            self.assertNotIn(password, record.read_text(encoding="utf-8"))

    def test_zero_byte_file_succeeds_with_complete_checksum(self):
        source = self.source(data=b"")
        self.assert_success(self.upload([source]))
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), b"")

    def test_start_idempotency_returns_same_job_and_conflict_is_rejected(self):
        source = self.source(data=b"one")
        first = self.upload([source], client_request_id="request-one")
        duplicate = self.upload([source], client_request_id="request-one")
        self.assertEqual(first["job_id"], duplicate["job_id"])
        self.assert_success(first)
        with self.assertRaises(FileTransferError):
            self.upload([self.source("different.bin")], client_request_id="request-one")

    def test_formal_restricted_policy_rejects_before_connect(self):
        self.manager = self.new_manager(policy=FilesystemPolicy(mode="formal_restricted"))
        with self.assertRaises(FileTransferError):
            self.upload([self.source()])
        self.assertEqual(self.backend.connections, [])

    def test_restricted_policy_also_rejects_recovery_before_connect(self):
        _source, first = self.interrupted_upload()
        self.manager.shutdown(timeout=2)
        connections = len(self.backend.connections)
        self.manager = self.new_manager(policy=FilesystemPolicy(mode="formal_restricted"))
        with self.assertRaises(FileTransferError):
            self.manager.resume_job(first["job_id"])
        self.assertEqual(len(self.backend.connections), connections)

    def test_parent_traversal_in_remote_destination_is_rejected(self):
        with self.assertRaises(FileTransferError):
            self.manager.start("upload", [str(self.source())], "/uploads/../outside")
        self.assertEqual(self.backend.connections, [])

    def test_discard_offline_remains_nonresumable_until_cleanup_retry(self):
        _source, first = self.interrupted_upload()
        self.backend.online = False
        discarded = self.manager.discard_transfer(first["transfer_id"])
        self.assertFalse(self.manager.get_job(first["job_id"])["resumable"])
        self.assertFalse(discarded["cleanup"]["complete"], discarded)
        with self.assertRaises(FileTransferError):
            self.manager.resume_job(first["job_id"])
        self.backend.online = True
        discarded = self.manager.discard_transfer(first["transfer_id"])
        self.assertTrue(discarded["cleanup"]["complete"], discarded)
        self.assertEqual(self.backend.partials(), [])

    def test_restart_finishes_discard_intent_persisted_before_cleanup(self):
        _source, first = self.interrupted_upload()
        # SystemExit bypasses normal exception recovery immediately after the
        # durable discard intent, preserving the pre-cleanup crash boundary.
        with mock.patch.object(
            self.manager,
            "_cleanup",
            side_effect=SystemExit("Injected crash after persisted discard intent"),
        ):
            with self.assertRaises(SystemExit):
                self.manager.discard_transfer(first["transfer_id"])
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        restored = self.manager.get_job(first["job_id"])
        self.assertFalse(restored["resumable"], restored)
        self.assertTrue(restored["cleanup"]["complete"], restored)
        self.assertEqual(self.backend.partials(), [])
        with self.assertRaises(FileTransferError):
            self.manager.resume_job(first["job_id"])
        checkpoint = self.store.root / "file_transfers" / (first["transfer_id"] + ".json")
        self.assertFalse(checkpoint.exists())

    def test_recursive_destination_parents_are_created_in_both_directions(self):
        source = self.source(data=b"nested destination")
        uploaded = self.assert_success(self.manager.start(
            "upload", [str(source)], "/missing/parent/deep/output"))
        self.assertEqual(self.backend.local("/missing/parent/deep/output/data.bin").read_bytes(), source.read_bytes())
        destination = self.root / "new-download" / "parents" / "output"
        downloaded = self.assert_success(self.manager.start(
            "download", ["/missing/parent/deep/output/data.bin"], str(destination)))
        self.assertEqual((destination / "data.bin").read_bytes(), source.read_bytes())
        self.assertTrue(uploaded["files"][0]["verified"])
        self.assertTrue(downloaded["files"][0]["verified"])

    def test_discard_removes_owned_recursive_parents_but_preserves_existing_parent(self):
        source = self.source()
        self.backend.write_gate = threading.Event()
        started = self.manager.start("upload", [str(source)], "/uploads/new/deep/output")
        self.assertTrue(self.backend.write_observed.wait(5))
        self.manager.cancel_job(started["job_id"])
        self.backend.write_gate.set()
        cancelled = self.wait(started)
        self.assertTrue(cancelled["resumable"])
        self.assertTrue(self.backend.local("/uploads/new/deep/output").exists())
        discarded = self.manager.discard_transfer(started["transfer_id"])
        self.assertTrue(discarded["cleanup"]["complete"], discarded)
        self.assertFalse(discarded["cleanup_pending"])
        self.assertFalse(self.backend.local("/uploads/new").exists())
        self.assertTrue(self.backend.local("/uploads").is_dir())
        with self.assertRaises(FileTransferError):
            self.manager.resume_job(started["job_id"])

    def test_discard_reports_all_recursive_directories_when_remote_offline(self):
        self.backend.disconnect_after_bytes = 65536
        first = self.wait(self.manager.start("upload", [str(self.source())], "/missing/deep/output"))
        self.backend.online = False
        discarded = self.manager.discard_transfer(first["transfer_id"])
        self.assertTrue(discarded["cleanup_pending"])
        self.assertIn("/missing", discarded["cleanup"]["residual_paths"])
        self.assertTrue(self.manager.get_job(first["job_id"])["cleanup_pending"])
        self.backend.online = True
        self.assertTrue(self.manager.discard_transfer(first["transfer_id"])["cleanup"]["complete"])

    def test_list_refreshes_other_manager_attempts_and_new_transfers(self):
        _source, first = self.interrupted_upload()
        observer = self.new_manager()
        finished = self.assert_success(self.manager.resume_job(first["job_id"]))
        seen = observer.list_jobs(states=["succeeded"])["jobs"]
        self.assertIn(finished["job_id"], [item["job_id"] for item in seen])
        new = self.assert_success(self.upload([self.source("other.bin", b"next")]))
        seen = observer.list_jobs()["jobs"]
        self.assertIn(new["job_id"], [item["job_id"] for item in seen])
        self.assertTrue(all("files" not in item for item in seen))

    def test_get_pages_and_old_attempt_receipt_remain_stable(self):
        directory = self.local / "many"
        directory.mkdir()
        for index in range(213):
            (directory / f"file-{index:03}.bin").write_bytes(str(index).encode())
        finished = self.wait(self.upload([directory]), timeout=60)
        self.assertEqual(finished["state"], "succeeded", finished)
        self.assertEqual(len(finished["files"]), 100)
        self.assertEqual(finished["files_total"], 214)
        self.assertEqual(finished["next_files_offset"], 100)
        middle = self.manager.get_job(finished["job_id"], files_offset=100, files_limit=100)
        tail = self.manager.get_job(finished["job_id"], files_offset=200, files_limit=100)
        self.assertEqual(len(tail["files"]), 14)
        self.assertIsNone(tail["next_files_offset"])
        targets = [item["target"] for page in (finished, middle, tail) for item in page["files"]]
        self.assertEqual(len(set(targets)), 214)
        self.assertEqual(self.manager.get_job(finished["job_id"], files_offset=500)["files"], [])
        reloaded = self.new_manager()
        self.assertEqual(reloaded.get_job(finished["job_id"], files_offset=200)["files"], tail["files"])
        self.assertFalse((self.manager.root / finished["transfer_id"]).exists())

        _source, failed = self.interrupted_upload()
        old_files = failed["files"]
        self.assert_success(self.manager.resume_job(failed["job_id"]))
        self.assertEqual(self.manager.get_job(failed["job_id"])["files"], old_files)

    def test_get_rejects_invalid_pagination(self):
        finished = self.assert_success(self.upload([self.source(data=b"page")]))
        for changes in ({"files_offset": -1}, {"files_offset": True}, {"files_offset": 1.5},
                        {"files_limit": 0}, {"files_limit": 201}, {"files_limit": True}):
            with self.subTest(changes=changes), self.assertRaises(FileTransferError):
                self.manager.get_job(finished["job_id"], **changes)

    def make_legacy_record(self, first, *, cancelled=False, cleanup_pending=False):
        record = copy.deepcopy(self.manager._fresh_record(
            self.manager._records[first["transfer_id"]], details=True))
        record = {key: value for key, value in record.items() if not key.startswith("_")}
        for key in ("revision", "manifest_count", "creation_root"):
            record.pop(key, None)
        record["schema_version"] = 1
        for item in record["files"]:
            for key in list(item):
                if key.startswith("_"):
                    del item[key]
        for attempt in record["attempts"]:
            page = self.manager.get_job(attempt["job_id"], files_limit=200)
            attempt["files"] = page["files"]
            attempt.pop("files_receipt", None)
        if cancelled:
            record["stop_reason"] = "user_cancel"
            record["cleanup_pending"] = cleanup_pending
            record["attempts"][-1].update(state="cancelled", resumable=False)
        self.manager._disk.remove_active_data(record["transfer_id"])
        path = self.manager.root / (record["transfer_id"] + ".json")
        path.write_text(json.dumps(record), encoding="utf-8")
        return path

    def test_legacy_failure_is_read_only_until_resume_migrates_under_lease(self):
        source, first = self.interrupted_upload()
        path = self.make_legacy_record(first)
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        self.assertEqual(json.loads(path.read_text())["schema_version"], 1)
        self.assertTrue(self.manager.get_job(first["job_id"])["resumable"])
        self.manager.list_jobs()
        self.assertEqual(json.loads(path.read_text())["schema_version"], 1)
        self.assert_success(self.manager.resume_job(first["job_id"]))
        history = self.manager.history_root / (first["transfer_id"] + ".json")
        self.assertEqual(json.loads(history.read_text())["schema_version"], 2)
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_legacy_cancel_never_revives_and_discard_migrates_it(self):
        _source, first = self.interrupted_upload()
        path = self.make_legacy_record(first, cancelled=True)
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        with self.assertRaises(FileTransferError):
            self.manager.resume_job(first["job_id"])
        self.assertEqual(json.loads(path.read_text())["schema_version"], 1)
        discarded = self.manager.discard_transfer(first["transfer_id"])
        self.assertTrue(discarded["cleanup"]["complete"], discarded)
        self.assertFalse(self.manager.get_job(first["job_id"])["resumable"])
        self.assertEqual(self.backend.partials(), [])

    def test_legacy_cancel_pending_cleanup_is_not_restored_as_pause(self):
        _source, first = self.interrupted_upload()
        self.make_legacy_record(first, cancelled=True, cleanup_pending=True)
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        restored = self.manager.get_job(first["job_id"])
        self.assertFalse(restored["resumable"])
        self.assertFalse(restored["cleanup_pending"])
        self.assertEqual(self.backend.partials(), [])

    def test_partial_initial_manifest_commit_replans_without_remote_writes(self):
        source = self.source(data=b"initial inventory transaction")
        original = self.manager._disk._write_json
        failed_once = False

        def fail_index(path, *args, **kwargs):
            nonlocal failed_once
            if Path(path).name == "offsets.json" and not failed_once:
                failed_once = True
                raise OSError("Injected manifest index failure")
            return original(path, *args, **kwargs)

        with mock.patch.object(self.manager._disk, "_write_json", side_effect=fail_index):
            first = self.wait(self.upload([source]))
        self.assertEqual(first["state"], "failed", first)
        self.assertEqual(self.backend.bytes_written, 0)
        summary = json.loads((self.manager.root / (first["transfer_id"] + ".json")).read_text())
        self.assertFalse(summary["manifest_ready"])
        self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_entry_commit_failures_preserve_recoverable_publication_boundaries(self):
        for phase in ("transferring", "ready", "publish_intent", "published"):
            with self.subTest(phase=phase):
                source = self.source(f"{phase}.bin", b"state boundary" * 100)
                original = self.manager._disk.write_entry
                failed_once = False

                def fail_state(record, index, entry):
                    nonlocal failed_once
                    if entry["phase"] == phase and not failed_once:
                        failed_once = True
                        raise OSError("Injected entry commit failure")
                    return original(record, index, entry)

                with mock.patch.object(self.manager._disk, "write_entry", side_effect=fail_state):
                    first = self.wait(self.upload([source]))
                self.assertEqual(first["state"], "failed", first)
                self.assertTrue(first["resumable"])
                self.assert_success(self.manager.resume_job(first["job_id"]))
                self.assertEqual(self.backend.local("/uploads/" + source.name).read_bytes(), source.read_bytes())

    def test_mkdir_ownership_commit_failure_does_not_authorize_deletion(self):
        original = self.manager._disk.write_directory
        failed_once = False

        def fail_ownership(record, path, owned):
            nonlocal failed_once
            if owned is True and not failed_once:
                failed_once = True
                raise OSError("Injected directory ownership commit failure")
            return original(record, path, owned)

        with mock.patch.object(self.manager._disk, "write_directory", side_effect=fail_ownership):
            first = self.wait(self.manager.start("upload", [str(self.source())], "/uploads/ownership-unknown"))
        self.assertEqual(first["state"], "failed", first)
        discarded = self.manager.discard_transfer(first["transfer_id"])
        self.assertTrue(discarded["cleanup_pending"])
        self.assertTrue(self.backend.local("/uploads/ownership-unknown").is_dir())
        self.assertIn("directory_ownership_unknown", [item["code"] for item in discarded["cleanup"]["errors"]])

    def test_legacy_migration_summary_failure_preserves_v1_and_retries(self):
        source, first = self.interrupted_upload()
        path = self.make_legacy_record(first)
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()
        original = self.manager._disk.write_summary
        failed_once = False

        def fail_commit(record):
            nonlocal failed_once
            if record["schema_version"] == 2 and not failed_once:
                failed_once = True
                raise OSError("Injected migration summary commit failure")
            return original(record)

        with mock.patch.object(self.manager._disk, "write_summary", side_effect=fail_commit):
            with self.assertRaises(OSError):
                self.manager.resume_job(first["job_id"])
        # A best-effort rollback may already commit a valid v2 retry. Both
        # versions must retain exactly the original resumable attempt.
        durable = json.loads(path.read_text())
        self.assertEqual(durable["latest_job_id"], first["job_id"])
        self.assertEqual(len(durable["attempts"]), 1)
        self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_schedule_thread_failure_rolls_back_job_and_request_indexes(self):
        source = self.source(data=b"thread can retry")
        with mock.patch.object(threading.Thread, "start", side_effect=RuntimeError("Injected thread start failure")):
            with self.assertRaises(RuntimeError):
                self.upload([source], client_request_id="thread-retry")
        self.assertEqual(self.manager.list_jobs()["jobs"], [])
        self.assert_success(self.upload([source], client_request_id="thread-retry"))

    def test_observer_can_discard_after_cached_active_attempt_stops(self):
        self.backend.write_gate = threading.Event()
        started = self.upload([self.source()])
        self.assertTrue(self.backend.write_observed.wait(5))
        observer = self.new_manager()
        self.manager.cancel_job(started["job_id"])
        self.backend.write_gate.set()
        self.wait(started)
        discarded = observer.discard_transfer(started["transfer_id"])
        self.assertTrue(discarded["cleanup"]["complete"], discarded)

    def test_unsaved_final_can_refresh_another_managers_recovery(self):
        source = self.source()
        self.backend.write_gate = threading.Event()
        started = self.upload([source])
        self.assertTrue(self.backend.write_observed.wait(5))
        with mock.patch.object(self.manager, "_persist", side_effect=OSError("Injected full disk")):
            self.backend.write_gate.set()
            first = self.wait(started)
        self.assertEqual(first["state"], "failed")
        observer = self.manager
        self.manager = self.new_manager()
        finished = self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertIn(finished["job_id"], [item["job_id"] for item in observer.list_jobs()["jobs"]])
        self.assertEqual(observer.get_job(finished["job_id"])["state"], "succeeded")

    def test_lost_mkdir_response_keeps_unknown_ownership_for_discard(self):
        original = FakeSftp.mkdir

        def accepted_without_response(sftp, path, mode=0o777):
            original(sftp, path, mode)
            raise OSError(errno.EIO, "Injected lost mkdir response")

        with mock.patch.object(FakeSftp, "mkdir", accepted_without_response):
            first = self.wait(self.manager.start("upload", [str(self.source())], "/uploads/lost-mkdir"))
        self.assertEqual(first["state"], "failed")
        discarded = self.manager.discard_transfer(first["transfer_id"])
        self.assertTrue(discarded["cleanup_pending"])
        self.assertIn("/uploads/lost-mkdir", discarded["cleanup"]["residual_paths"])
        self.assertTrue(self.backend.local("/uploads/lost-mkdir").is_dir())

    def test_receipt_index_commit_failure_recovers_original_attempt_details(self):
        original = self.manager._disk._write_json
        failed_once = False

        def fail_receipt_index(path, *args, **kwargs):
            nonlocal failed_once
            if Path(path).name.endswith(".files.offsets.json") and not failed_once:
                failed_once = True
                raise OSError("Injected receipt index failure")
            return original(path, *args, **kwargs)

        with mock.patch.object(self.manager._disk, "_write_json", side_effect=fail_receipt_index):
            first = self.wait(self.upload([self.source(data=b"immutable result")]))
        self.assertTrue(failed_once)
        original_files = first["files"]
        self.manager = self.new_manager()
        recovered = self.manager.get_job(first["job_id"])
        self.assertTrue(recovered["resumable"], recovered)
        self.assertEqual(recovered["files"], original_files)
        self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(self.manager.get_job(first["job_id"])["files"], original_files)


def _run_crash_worker(root, cancel=False):
    """Run only the isolated test service, which the parent intentionally kills."""

    root = Path(root)
    backend = FakeBackend(root / "remote")
    backend.write_gate = threading.Event()
    backend.write_marker = root / "write-started"
    manager = FileTransferJobManager(
        config_store=MemoryConfigStore(root / "config"), backend=backend
    )
    started = manager.start("upload", [str(root / "local" / "data.bin")], "/uploads")
    try:
        if cancel:
            deadline = time.monotonic() + 10
            while not (root / "cancel-requested").exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            original_persist = manager._persist

            def persist_then_block(record, **kwargs):
                original_persist(record, **kwargs)
                if record["stop_reason"] == "user_cancel":
                    (root / "cancel-persisted").write_text("persisted", encoding="utf-8")
                    threading.Event().wait(10)

            manager._persist = persist_then_block
            manager.cancel_job(started["job_id"])
        else:
            backend.write_gate.wait(10)
    finally:
        manager.shutdown(timeout=1)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] in {"--crash-worker", "--cancel-crash-worker"}:
        _run_crash_worker(sys.argv[2], cancel=sys.argv[1] == "--cancel-crash-worker")
    else:
        unittest.main()
