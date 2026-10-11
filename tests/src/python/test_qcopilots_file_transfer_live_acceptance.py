"""Offline regression checks for the opt-in file transfer acceptance runner.

All files belong to TemporaryDirectory contexts. No test opens SSH or changes
the user's configuration, known hosts or persistent GIS corpus.
"""

from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import qcopilots_file_transfer_live_acceptance as live


class TestFileTransferLiveAcceptance(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="qcopilots-live-unit-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def args(self, **changes):
        args = live.build_argument_parser().parse_args([
            "--host", "192.0.2.1", "--username", "acceptance",
            "--remote-base", "C:/test", "--workspace-base", str(self.root),
            "--large-mib", "8",
        ])
        for name, value in changes.items():
            setattr(args, name, value)
        return args

    def runner(self, **changes):
        return live.AcceptanceRunner(self.args(**changes), "unit-test-private-password")

    def test_inventory_includes_empty_directory_and_exact_binary_size(self):
        (self.root / "中文 空目录").mkdir()
        live._write_binary(self.root / "payload.bin", live.CHUNK_SIZE + 1)
        (self.root / "empty.bin").write_bytes(b"")
        inventory = live._local_inventory(self.root)
        self.assertEqual(inventory["中文 空目录"], {"kind": "directory"})
        self.assertEqual(inventory["payload.bin"]["size"], live.CHUNK_SIZE + 1)
        self.assertEqual(inventory["empty.bin"]["sha256"], live.hashlib.sha256(b"").hexdigest())

    def test_download_injection_does_not_interrupt_source_hash(self):
        owner = live._DisconnectOnceBackend(None, "download", threshold=4)
        closed = []
        owner.session = SimpleNamespace(close=lambda: closed.append(True))
        handle = live._DisconnectFile(io.BytesIO(b"abcdefgh"), owner)
        self.assertEqual(handle.read(4), b"abcd")
        self.assertFalse(owner.triggered)
        owner.observe_transfer = lambda: 4
        with self.assertRaises(OSError):
            handle.read(4)
        self.assertEqual(owner.observed_bytes, 4)
        self.assertEqual(closed, [True])

    def test_injected_connection_forwards_cancellation_hooks(self):
        received = []
        backend = SimpleNamespace(connect=lambda *args, **kwargs: (received.append((args, kwargs)) or SimpleNamespace(sftp=object())))
        injected = live._DisconnectOnceBackend(backend)
        callback = lambda: None
        injected.connect({"host": "example"}, "pin", cancel_check=callback, register_resource=callback)
        self.assertEqual(received[0][0][1], "pin")
        self.assertIs(received[0][1]["cancel_check"], callback)
        self.assertIs(received[0][1]["register_resource"], callback)

    def test_pagination_collects_all_entries_without_writes(self):
        entries = [{"target": f"target-{index}"} for index in range(401)]
        def get_job(_job_id, files_offset, files_limit):
            page = entries[files_offset:files_offset + files_limit]
            next_offset = files_offset + len(page)
            return {"files": page, "files_total": len(entries), "files_offset": files_offset,
                    "next_files_offset": next_offset if next_offset < len(entries) else None}
        manager = SimpleNamespace(get_job=get_job, _disk=SimpleNamespace(write_stats={"write_count": 7, "bytes_written": 91}))
        found, stats = self.runner().all_files(manager, "job")
        self.assertEqual(found, entries)
        self.assertEqual(stats["queries"], 3)

    def test_pagination_rejects_duplicate_targets(self):
        manager = SimpleNamespace(
            get_job=lambda *_args, **_kwargs: {"files": [{"target": "duplicate"}] * 2, "files_total": 2, "files_offset": 0, "next_files_offset": None},
            _disk=SimpleNamespace(write_stats={"write_count": 0, "bytes_written": 0}),
        )
        with self.assertRaisesRegex(live.AcceptanceFailure, "pagination_entries_duplicated"):
            self.runner().all_files(manager, "job")

    def test_workspace_cannot_contain_persistent_corpus(self):
        corpus = self.root / "corpus"
        corpus.mkdir()
        with self.assertRaisesRegex(live.AcceptanceFailure, "corpus_inside_temporary_workspace"):
            self.runner(corpus_root=corpus)

    def test_workspace_cannot_be_inside_persistent_corpus(self):
        workspace = self.root / "work"
        workspace.mkdir()
        with self.assertRaisesRegex(live.AcceptanceFailure, "workspace_inside_persistent_corpus"):
            self.runner(corpus_root=self.root, workspace_base=workspace)

    def test_report_cannot_overwrite_persistent_corpus(self):
        with tempfile.TemporaryDirectory(prefix="qcopilots-persistent-corpus-") as corpus:
            with self.assertRaisesRegex(live.AcceptanceFailure, "report_inside_persistent_corpus"):
                self.runner(corpus_root=Path(corpus), report=Path(corpus) / "report.json")

    def test_short_password_does_not_corrupt_machine_field_checks(self):
        runner = self.runner()
        runner.password = "a"
        runner.assert_redacted({"job_id": "a123", "state": "cancelled", "password_configured": True})
        with self.assertRaisesRegex(live.AcceptanceFailure, "credential_disclosed"):
            runner.assert_redacted({"password": "a"})

    def test_local_final_rmdir_can_be_retried_without_marker(self):
        runner = self.runner()
        runner._setup_local()
        original = Path.rmdir
        failed = []
        def fail_final_once(path):
            if path == runner.local_root and not failed:
                failed.append(True)
                raise PermissionError("locked root")
            return original(path)
        with mock.patch.object(Path, "rmdir", fail_final_once):
            with self.assertRaises(PermissionError):
                runner._remove_local_owned()
            self.assertTrue(runner.local_finalizing)
            self.assertFalse((runner.local_root / live.MARKER_NAME).exists())
            runner._remove_local_owned()
        self.assertFalse(runner.local_root.exists())

    def test_remote_failure_keeps_only_non_secret_cleanup_record(self):
        runner = self.runner()
        runner._setup_local()
        runner.remote_created = True
        runner.remote_marker_complete = True
        (runner.local_root / "download.bin").write_bytes(b"temporary contents")
        runner.connect = mock.Mock(side_effect=OSError("offline"))
        with mock.patch.object(live.time, "sleep"):
            with self.assertRaisesRegex(live.AcceptanceFailure, "cleanup_failed"):
                runner.cleanup()
        self.assertFalse(runner.local_root.exists())
        self.assertTrue(runner.cleanup_record.exists())
        record = runner.cleanup_record.read_text(encoding="utf-8")
        self.assertNotIn(runner.password, record)
        self.assertEqual(json.loads(record)["run_id"], runner.run_id)
        self.assertEqual(list(self.root.iterdir()), [runner.cleanup_record])

    def test_uncertain_remote_mkdir_is_never_deleted(self):
        runner = self.runner()
        runner.remote_mkdir_uncertain = True
        sftp = SimpleNamespace(rmdir=mock.Mock(), remove=mock.Mock())
        runner.connect = lambda: contextlib.nullcontext(SimpleNamespace(sftp=sftp))
        runner.validate_remote_path = lambda *_args, **_kwargs: SimpleNamespace(st_mode=0o40755)
        with self.assertRaisesRegex(live.AcceptanceFailure, "remote_ownership_unconfirmed"):
            runner._remove_remote_owned()
        sftp.rmdir.assert_not_called()
        sftp.remove.assert_not_called()

    def test_cleanup_retry_rejects_endpoint_mismatch(self):
        runner = self.runner()
        runner.remote_created = True
        runner.remote_marker_complete = True
        runner._save_cleanup_record()
        retry = self.runner(retry_cleanup=runner.cleanup_record, host="192.0.2.2")
        with self.assertRaisesRegex(live.AcceptanceFailure, "cleanup_endpoint_mismatch"):
            retry.prepare_cleanup_retry()
        self.assertFalse(retry.local_created)

    def test_local_cleanup_failure_record_can_recover_existing_owned_root(self):
        runner = self.runner()
        runner._setup_local()
        with mock.patch.object(runner, "_remove_local_owned", side_effect=PermissionError("locked")), mock.patch.object(live.time, "sleep"):
            with self.assertRaisesRegex(live.AcceptanceFailure, "cleanup_failed"):
                runner.cleanup()
        self.assertTrue(runner.local_root.exists())
        self.assertTrue(runner.cleanup_record.exists())
        retry = self.runner(retry_cleanup=runner.cleanup_record)
        retry.prepare_cleanup_retry()
        self.assertTrue(retry.cleanup_retry_prepared)
        retry.cleanup()
        self.assertFalse(runner.local_root.exists())
        self.assertFalse(runner.cleanup_record.exists())

    def test_malformed_cleanup_record_is_preserved_without_partial_adoption(self):
        runner = self.runner()
        runner.remote_created = True
        runner._save_cleanup_record()
        record = json.loads(runner.cleanup_record.read_text(encoding="utf-8"))
        record["remote_mkdir_uncertain"] = "invalid"
        runner.cleanup_record.write_text(json.dumps(record), encoding="utf-8")
        retry = self.runner(retry_cleanup=runner.cleanup_record)
        with self.assertRaisesRegex(live.AcceptanceFailure, "cleanup_record_invalid"):
            retry.prepare_cleanup_retry()
        retry.cleanup()
        self.assertTrue(runner.cleanup_record.exists())
        self.assertFalse(retry.remote_created)

    def test_failed_atomic_report_write_removes_its_temporary_file(self):
        with mock.patch.object(live.os, "replace", side_effect=OSError("disk failure")):
            with self.assertRaises(OSError):
                live._atomic_json(self.root / "report.json", {"passed": True})
        self.assertEqual(list(self.root.iterdir()), [])

    def test_corpus_group_runs_only_corpus_scenarios(self):
        with tempfile.TemporaryDirectory(prefix="qcopilots-persistent-corpus-") as corpus:
            runner = self.runner(corpus_root=Path(corpus), scenario_group="corpus")
            runner.setup = mock.Mock()
            runner.small_file_scaling = mock.Mock()
            runner.corpus_roundtrip = mock.Mock()
            runner.cleanup = mock.Mock()
            runner.roundtrip = mock.Mock(side_effect=AssertionError("baseline unexpectedly executed"))
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner.run(), 0)
            self.assertEqual([result["scenario"] for result in runner.results], [
                "setup", "small_file_checkpoint_scaling", "persistent_corpus_roundtrip_sha256", "owned_resource_cleanup",
            ])
            runner.small_file_scaling.assert_called_once_with()
            runner.corpus_roundtrip.assert_called_once_with()
            runner.roundtrip.assert_not_called()

    def test_corpus_group_requires_explicit_corpus_root(self):
        arguments = ["--host", "192.0.2.1", "--username", "acceptance", "--remote-base", "C:/test", "--workspace-base", str(self.root), "--scenario-group", "corpus"]
        with mock.patch.dict(live.os.environ, {"QCOPILOTS_FILE_TRANSFER_TEST_PASSWORD": "private"}), contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(live.main(arguments), 2)
        self.assertIn("invalid_arguments", output.getvalue())

    def test_password_environment_uses_only_file_transfer_name(self):
        arguments = [
            "--host", "192.0.2.1", "--username", "acceptance",
            "--remote-base", "C:/test", "--workspace-base", str(self.root),
        ]
        with mock.patch.dict(live.os.environ, {"QCOPILOTS_SCP_TEST_PASSWORD": "legacy"}, clear=True), \
                mock.patch.object(live, "AcceptanceRunner") as runner, \
                contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(live.main(arguments), 2)
            runner.assert_not_called()
        self.assertIn("test_password_environment_required", output.getvalue())

        with mock.patch.dict(live.os.environ, {"QCOPILOTS_FILE_TRANSFER_TEST_PASSWORD": "current"}, clear=True), \
                mock.patch.object(live, "AcceptanceRunner") as runner:
            runner.return_value.run.return_value = 0
            self.assertEqual(live.main(arguments), 0)
            self.assertEqual(runner.call_args.args[1], "current")
            runner.return_value.run.assert_called_once_with()

    def test_failure_diagnostics_redact_exception_and_exclude_frame_locals(self):
        runner = self.runner()
        manager = runner.new_manager()
        try:
            password = runner.password
            local_private_value = "not-to-be-recorded"
            try:
                raise PermissionError(13, "denied " + password, str(self.root / "target.part"))
            except PermissionError as error:
                result = manager._error(error, (password,))
            self.assertEqual(result["code"], "permission_denied")
            evidence = manager.acceptance_diagnostics[-1]
            self.assertEqual(evidence["errno"], 13)
            self.assertEqual(evidence["filename"], str(self.root / "target.part"))
            self.assertTrue(evidence["stack"])
            serialized = json.dumps(evidence)
            self.assertNotIn(password, serialized)
            self.assertNotIn(local_private_value, serialized)
            self.assertIn("[redacted]", serialized)
        finally:
            manager.shutdown()

    def test_failed_job_report_retains_safe_evidence_before_cleanup(self):
        runner = self.runner()
        snapshot = {"job_id": "job", "transfer_id": "transfer", "direction": "upload", "state": "failed", "phase": "failed",
                    "progress": {"files_completed": 43}, "errors": [{"code": "permission_denied", "message": "Denied"}]}
        manager = SimpleNamespace(acceptance_diagnostics=[{"job_id": "job", "phase": "verifying_prefix", "errno": 13}])
        runner.failure_details(manager, snapshot)
        recorded = runner.current_details["failed_job"]
        self.assertEqual(recorded["progress"]["files_completed"], 43)
        self.assertEqual(recorded["diagnostics"][0]["phase"], "verifying_prefix")
        self.assertEqual(recorded["diagnostics"][0]["errno"], 13)

    def test_lifecycle_suite_against_isolated_filesystem_backend(self):
        # Exercise the runner and real transfer state machine together. Only
        # the transport is replaced, keeping this opt-in script out of SSH.
        from test_qcopilots_mcp_server_file_transfer_jobs import FakeBackend
        from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError

        class Session:
            def __init__(self, session):
                self.sftp = session.sftp
                self.fingerprint = session.fingerprint
                self.close = session.close

            def __enter__(self):
                return self

            def __exit__(self, *_args):
                self.close()

        class Backend(FakeBackend):
            def connect(self, config, expected_fingerprint=None, **_kwargs):
                return Session(super().connect(config, expected_fingerprint))

            def test_connection(self, config):
                if config.get("password") != "unit-test-private-password":
                    raise FileTransferError("authentication_failed", "Injected authentication rejection")
                return {"connected": True, "fingerprint": self.fingerprint}

        with tempfile.TemporaryDirectory(prefix="qcopilots-live-server-") as remote:
            runner = self.runner(timeout=30, scenario_group="baseline")
            runner.backend = Backend(remote)
            runner.backend.local(runner.remote_base).mkdir(parents=True)
            with contextlib.redirect_stdout(io.StringIO()) as output:
                result = runner.run()
            self.assertEqual(result, 0, output.getvalue())
            self.assertFalse(runner.local_root.exists())
            self.assertFalse(runner.backend.local(runner.remote_root).exists())
            self.assertTrue(all(item["result"].startswith("PASS") for item in runner.results))


if __name__ == "__main__":
    unittest.main()
