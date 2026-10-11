"""Resume identity and target-write failure boundaries without a live server.

The existing jobs fixture owns every local, remote and checkpoint file under a
TemporaryDirectory. Write failures happen after an actual partial write, so
recovery must validate and reuse data beyond the last acknowledged checkpoint.
"""

import errno
import hashlib
import json
import types
import unittest
from pathlib import Path
from unittest import mock

import test_qcopilots_mcp_server_file_transfer_backend as backend_tests
import test_qcopilots_mcp_server_file_transfer_jobs as jobs_tests

from qcopilots_mcp_server_processing_general import file_transfer_backend
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError


class _DiskFullWriter:
    """Accept a partial second write before reporting exhausted storage."""

    def __init__(self, stream, evidence):
        self.stream = stream
        self.evidence = evidence

    def __getattr__(self, name):
        return getattr(self.stream, name)

    def __enter__(self):
        self.stream.__enter__()
        return self

    def __exit__(self, *args):
        return self.stream.__exit__(*args)

    def write(self, value):
        self.evidence["calls"] += 1
        if self.evidence["calls"] == 2:
            accepted = self.stream.write(value[:8191])
            self.stream.flush()
            self.evidence.update(raised=True, accepted_before_error=accepted)
            raise OSError(errno.ENOSPC, "Injected target storage exhaustion")
        return self.stream.write(value)


class TestFileTransferResumeBoundaries(unittest.TestCase):
    # Reuse only fixture helpers, avoiding rerunning the original test methods.
    setUp = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.setUp
    new_manager = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.new_manager
    close_managers = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.close_managers
    assert_no_worker_exceptions = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.assert_no_worker_exceptions
    source = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.source
    wait = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.wait
    upload = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.upload
    assert_success = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.assert_success
    interrupted_upload = jobs_tests.TestQCopilotsMcpServerFileTransferJobs.interrupted_upload

    def _restart_manager(self):
        self.manager.shutdown(timeout=2)
        self.manager = self.new_manager()

    def _assert_changed_identity_refused(self, field, replacement):
        source, first = self.interrupted_upload()
        self._restart_manager()
        original = self.store.config[field]
        previous_connections = len(self.backend.connections)
        previous_jobs = self.manager.list_jobs()
        self.store.config[field] = replacement
        with self.assertRaises(FileTransferError) as caught:
            self.manager.resume_job(first["job_id"])
        self.assertEqual(caught.exception.code, "connection_changed")
        self.assertEqual(len(self.backend.connections), previous_connections)
        self.assertEqual(self.manager.list_jobs(), previous_jobs)
        self.assertTrue(self.manager.get_job(first["job_id"])["resumable"])
        self.store.config[field] = original
        recovered = self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertGreater(recovered["progress"]["bytes_reused"], 0)
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())

    def test_changed_host_refuses_resume_before_connection(self):
        self._assert_changed_identity_refused("host", "different.invalid")

    def test_changed_port_refuses_resume_before_connection(self):
        self._assert_changed_identity_refused("port", 2222)

    def test_changed_username_refuses_resume_before_connection(self):
        self._assert_changed_identity_refused("username", "different-user")

    def test_changed_password_is_used_after_persisted_resume(self):
        source, first = self.interrupted_upload()
        old_password = self.store.config["password"]
        new_password = "replacement-password-only-in-runtime"
        self._restart_manager()
        self.store.config["password"] = new_password
        recovered = self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(self.backend.connections[-1][0]["password"], new_password)
        self.assertEqual(self.backend.connections[0][0]["password"], old_password)
        self.assertGreater(recovered["progress"]["bytes_reused"], 0)
        self.assertEqual(self.backend.local("/uploads/data.bin").read_bytes(), source.read_bytes())
        public = json.dumps(self.manager.list_jobs()) + json.dumps(recovered)
        persisted = "".join(path.read_text(encoding="utf-8") for path in self.store.root.rglob("*.json"))
        for password in (old_password, new_password):
            self.assertNotIn(password, public)
            self.assertNotIn(password, persisted)

    def test_persisted_fingerprint_mismatch_refuses_resume_before_authentication(self):
        _source, first = self.interrupted_upload()
        saved_fingerprint = self.backend.fingerprint
        self._restart_manager()
        # Even a newly trusted server key must not replace this transfer's pin.
        changed_key = backend_tests.FakeKey(b"different-server-key")
        Path(self.store.config["known_hosts_file"]).write_text(
            changed_key.line(self.store.config["host"]), encoding="utf-8"
        )
        transport, sock = mock.Mock(), mock.Mock()
        paramiko = types.SimpleNamespace(
            HostKeys=backend_tests.FakeHostKeys,
            hostkeys=types.SimpleNamespace(HostKeyEntry=backend_tests.FakeHostKeyEntry),
            AuthenticationException=backend_tests.FakeAuthenticationException,
            SSHException=backend_tests.FakeSSHException,
            SFTPClient=types.SimpleNamespace(from_transport=mock.Mock()),
        )
        backend = file_transfer_backend.FileTransferBackend(self.store, self.manager.policy)
        self.manager.backend = backend
        with mock.patch.object(file_transfer_backend, "_paramiko", return_value=paramiko), \
                mock.patch.object(backend, "_probe", return_value=(transport, changed_key, sock)), \
                mock.patch.object(backend, "connect", wraps=backend.connect) as connect:
            failed = self.wait(self.manager.resume_job(first["job_id"]))
            self.manager.shutdown(timeout=2)
        self.assertEqual(failed["state"], "failed", failed)
        self.assertEqual([error["code"] for error in failed["errors"]], ["host_key_changed"])
        self.assertTrue(failed["resumable"])
        self.assertEqual(connect.call_args.kwargs["expected_fingerprint"], saved_fingerprint)
        transport.auth_password.assert_not_called()
        paramiko.SFTPClient.from_transport.assert_not_called()
        transport.close.assert_called_once()
        sock.close.assert_called_once()
        self.assertEqual(len(self.backend.partials()), 1)
        self.assertFalse(self.backend.local("/uploads/data.bin").exists())

    def _assert_target_disk_full_recovers(self, direction):
        payload = bytes(range(256)) * 4096
        old_payload = b"existing destination must survive storage exhaustion"
        if direction == "upload":
            source = self.source(data=payload)
            destination = "/uploads"
            target = self.backend.local("/uploads/data.bin")
        else:
            source = "/uploads/data.bin"
            self.backend.local(source).write_bytes(payload)
            destination = self.local / "download"
            destination.mkdir()
            target = destination / "data.bin"
        target.write_bytes(old_payload)
        evidence = {"calls": 0, "raised": False}
        open_target = self.manager._open_target

        def target_with_fault(runtime, path, exclusive=False, resume=False):
            handle = open_target(runtime, path, exclusive=exclusive, resume=resume)
            return _DiskFullWriter(handle, evidence) if resume else handle

        with mock.patch.object(self.manager, "_open_target", side_effect=target_with_fault):
            first = self.wait(self.manager.start(
                direction, [str(source)], str(destination), overwrite=True
            ))
            self.manager.shutdown(timeout=2)
        self.assertTrue(evidence["raised"])
        self.assertEqual(evidence["accepted_before_error"], 8191)
        self.assertEqual(first["state"], "failed", first)
        self.assertEqual([error["code"] for error in first["errors"]], ["transfer_io_error"])
        self.assertTrue(first["resumable"])
        self.assertFalse(first["cleanup_pending"])
        self.assertFalse(first["files"][0]["verified"])
        self.assertNotEqual(first["files"][0]["state"], "published")
        self.assertEqual(target.read_bytes(), old_payload)
        partials = self.backend.partials() if direction == "upload" else list(destination.glob(".qcopilots-file-transfer-*.part"))
        self.assertEqual(len(partials), 1)
        partial = partials[0].read_bytes()
        self.assertGreater(len(partial), first["files"][0]["bytes_completed"])
        self.assertEqual(partial, payload[:len(partial)])

        self.manager = self.new_manager()
        recovered = self.assert_success(self.manager.resume_job(first["job_id"]))
        self.assertEqual(recovered["progress"]["bytes_reused"], len(partial))
        self.assertEqual(recovered["progress"]["bytes_transferred"], len(payload) - len(partial))
        self.assertEqual(recovered["files"][0]["sha256"], hashlib.sha256(payload).hexdigest())
        self.assertTrue(recovered["files"][0]["verified"])
        self.assertEqual(target.read_bytes(), payload)
        self.assertFalse(partials[0].exists())
        self.assertEqual(self.manager.get_job(first["job_id"])["state"], "failed")

    def test_upload_target_write_enospc_preserves_original_and_resumes_partial(self):
        self._assert_target_disk_full_recovers("upload")

    def test_download_target_write_enospc_preserves_original_and_resumes_partial(self):
        self._assert_target_disk_full_recovers("download")


if __name__ == "__main__":
    unittest.main()
