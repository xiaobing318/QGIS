"""Isolated configuration and MCP contract tests for SSH transfers."""

import builtins
import json
import logging
import tempfile
import threading
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from qcopilots_common.mcp_http import McpJsonRpcServer
from qcopilots_mcp_server_processing_general import file_transfer_config
from qcopilots_mcp_server_processing_general.file_transfer_backend import FileTransferBackend
from qcopilots_mcp_server_processing_general.file_transfer_config import DEFAULT_CONFIG, FileTransferConfigStore
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError, public_value
from qcopilots_mcp_server_processing_general.file_transfer_tools import build_file_transfer_tools


SENTINEL = "FILE-TRANSFER-SECRET-SENTINEL-81574"


class TestFileTransferConfig(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="qgis-file-transfer-config-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.store = FileTransferConfigStore(root=self.root / "settings")

    def test_defaults_and_public_configuration_do_not_create_user_files(self):
        self.assertEqual(self.store.load(), DEFAULT_CONFIG)
        public = self.store.public()
        self.assertFalse(public["password_configured"])
        self.assertNotIn("password", public)
        self.assertEqual(public["config_path"], str(self.store.path))
        self.assertFalse(self.store.root.exists())

    def test_template_matches_defaults(self):
        template = json.loads(Path(file_transfer_config.__file__).with_name("file_transfer_config.json").read_text(encoding="utf-8"))
        template.pop("$schema")
        self.assertEqual(template, DEFAULT_CONFIG)
        schema = json.loads(Path(file_transfer_config.__file__).with_name("file_transfer_config.schema.json").read_text(encoding="utf-8"))
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(set(schema["properties"]) - {"$schema"}, set(DEFAULT_CONFIG))

    def test_configuration_uses_new_filename_and_ignores_legacy_file(self):
        self.store.root.mkdir()
        legacy_path = self.store.root / "scp_config.json"
        legacy_contents = b'{"host": "legacy.invalid", "password": "legacy-secret"}'
        legacy_path.write_bytes(legacy_contents)

        self.assertEqual(self.store.path, self.store.root / "file_transfer_config.json")
        self.assertEqual(self.store.load(), DEFAULT_CONFIG)
        public = self.store.update({"host": "current.test"})

        self.assertEqual(public["config_path"], str(self.store.path))
        self.assertEqual(self.store.load()["host"], "current.test")
        self.assertFalse(public["password_configured"])
        self.assertEqual(legacy_path.read_bytes(), legacy_contents)

    def test_default_configuration_root_uses_qcopilots_home(self):
        with mock.patch.object(file_transfer_config, "qcopilots_home", return_value=self.root):
            store = FileTransferConfigStore()
        self.assertEqual(store.path, self.root / "processing_general" / "file_transfer_config.json")
        self.assertFalse(store.root.exists())

    def test_plaintext_password_is_saved_but_never_returned(self):
        public = self.store.update({"host": "example.test", "username": "tester", "password": SENTINEL})
        self.assertNotIn(SENTINEL, json.dumps(public))
        self.assertNotIn("password", public)
        self.assertTrue(public["password_configured"])
        persisted = json.loads(self.store.path.read_text(encoding="utf-8"))
        self.assertEqual(persisted["password"], SENTINEL)
        self.assertEqual(self.store.load(require_connection=True)["host"], "example.test")
        self.assertEqual(list(self.store.root.glob(".*.tmp")), [])

    def test_omitted_password_is_retained_and_empty_password_clears(self):
        self.store.update({"password": "  " + SENTINEL + "  "})
        self.store.update({"port": 2222})
        self.assertEqual(self.store.load()["password"], "  " + SENTINEL + "  ")
        public = self.store.update({"password": ""})
        self.assertFalse(public["password_configured"])
        self.assertEqual(self.store.load()["password"], "")

    def test_hand_edits_are_loaded_before_partial_updates(self):
        self.store.update({"host": "before.test", "password": SENTINEL})
        config = self.store.load()
        config["host"] = "after.test"
        config["upload_sources"] = ["C:/data/中文 data.txt"]
        self.store.path.write_text(json.dumps(config), encoding="utf-8")
        self.store.update({"port": 2222})
        reloaded = self.store.load()
        self.assertEqual(reloaded["host"], "after.test")
        self.assertEqual(reloaded["upload_sources"], ["C:/data/中文 data.txt"])
        self.assertEqual(reloaded["password"], SENTINEL)
        self.assertEqual(reloaded["port"], 2222)

    def test_unknown_fields_and_invalid_values_are_rejected_without_values(self):
        changes = [
            {SENTINEL: True},
            {"config_version": 2},
            {"port": True},
            {"port": 0},
            {"port": 65536},
            {"port": SENTINEL},
            {"connect_timeout": float("nan")},
            {"connect_timeout": 0},
            {"io_timeout": float("inf")},
            {"io_timeout": True},
            {"max_bytes_per_second": -1},
            {"max_bytes_per_second": True},
            {"host": "ssh://" + SENTINEL},
            {"host": "user@" + SENTINEL},
            {"username": SENTINEL + "\n"},
            {"password": [SENTINEL]},
            {"upload_sources": [SENTINEL, None]},
            {"download_sources": [""]},
            {"known_hosts_file": None},
        ]
        for change in changes:
            with self.subTest(change=list(change)):
                with self.assertRaises(FileTransferError) as error:
                    self.store.update(change)
                self.assertNotIn(SENTINEL, str(error.exception))
                self.assertFalse(self.store.path.exists())

    def test_empty_known_hosts_uses_default(self):
        self.store.update({"known_hosts_file": ""})
        self.assertEqual(self.store.load()["known_hosts_file"], "~/.ssh/known_hosts")

    def test_connection_validation_matches_backend_and_rejects_hand_edits(self):
        valid = {**DEFAULT_CONFIG, "host": "example.test", "username": "tester", "password": SENTINEL}
        invalid = [
            {"host": value} for value in ("bad,host", "host#comment", "[::1]", "*.example", "bad?host", "user@host", "bad\x7fhost", "bad\x01host")
        ] + [{"username": value} for value in ("user\t", "user\x7f", "user\r")] + [
            {"connect_timeout": 3601}, {"connect_timeout": 10**400}, {"io_timeout": 10**400}
        ]
        self.store.root.mkdir(parents=True)
        for changes in invalid:
            with self.subTest(changes=changes):
                config = {**valid, **changes}
                with self.assertRaises(FileTransferError):
                    FileTransferBackend._endpoint(config)
                self.store.path.write_text(json.dumps(config), encoding="utf-8")
                original = self.store.path.read_bytes()
                with self.assertRaises(FileTransferError):
                    self.store.load()
                self.assertEqual(self.store.path.read_bytes(), original)
                self.store.path.unlink()
                with self.assertRaises(FileTransferError):
                    self.store.update(changes)
                self.assertFalse(self.store.path.exists())

    def test_ipv4_ipv6_and_windows_accounts_pass_shared_validation(self):
        for host in ("192.0.2.10", "::1", "2001:db8::1", "server.example"):
            with self.subTest(host=host):
                self.store.update({"host": host, "username": "DOMAIN\\upload-user", "password": SENTINEL})
                config = self.store.load(require_connection=True)
                self.assertEqual(FileTransferBackend._endpoint(config), (host, 22, "DOMAIN\\upload-user"))

    def test_incomplete_connection_has_controlled_error(self):
        with self.assertRaises(FileTransferError) as error:
            self.store.load(require_connection=True)
        self.assertEqual(error.exception.code, "connection_not_configured")

    def test_broken_json_is_preserved_and_only_position_is_reported(self):
        self.store.root.mkdir(parents=True)
        original = '{"password": "' + SENTINEL + '", broken}'
        self.store.path.write_text(original, encoding="utf-8")
        with self.assertRaises(FileTransferError) as error:
            self.store.update({"host": "example.test"})
        self.assertEqual(error.exception.code, "invalid_config")
        self.assertEqual(set(error.exception.details), {"line", "column"})
        self.assertNotIn(SENTINEL, str(error.exception))
        self.assertEqual(self.store.path.read_text(encoding="utf-8"), original)

    def test_existing_invalid_structure_is_not_overwritten(self):
        self.store.root.mkdir(parents=True)
        for bad in ([SENTINEL], {"config_version": False}, {"config_version": 99}, {SENTINEL: "value"}):
            original = json.dumps(bad)
            self.store.path.write_text(original, encoding="utf-8")
            with self.assertRaises(FileTransferError):
                self.store.update({"host": "example.test"})
            self.assertEqual(self.store.path.read_text(encoding="utf-8"), original)

    def test_atomic_replace_failure_preserves_original_and_cleans_temporary(self):
        self.store.update({"host": "before.test", "password": SENTINEL})
        original = self.store.path.read_bytes()
        with mock.patch.object(file_transfer_config.os, "replace", side_effect=OSError(SENTINEL)):
            with self.assertRaises(FileTransferError) as error:
                self.store.update({"host": "after.test"})
        self.assertEqual(error.exception.code, "config_write_failed")
        self.assertNotIn(SENTINEL, str(error.exception))
        self.assertEqual(self.store.path.read_bytes(), original)
        self.assertEqual(list(self.store.root.glob(".*.tmp")), [])

    def test_concurrent_partial_updates_preserve_both_changes(self):
        barrier = threading.Barrier(2)
        errors = []

        def update(changes):
            try:
                barrier.wait(timeout=5)
                self.store.update(changes)
            except Exception as error:
                errors.append(type(error).__name__)

        threads = [
            threading.Thread(target=update, args=({"host": "example.test"},)),
            threading.Thread(target=update, args=({"port": 2222},)),
        ]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(timeout=10)
            self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        self.assertEqual(self.store.load()["host"], "example.test")
        self.assertEqual(self.store.load()["port"], 2222)


class TestFileTransferToolContract(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="qgis-file-transfer-api-")
        self.addCleanup(self.temporary.cleanup)
        self.store = FileTransferConfigStore(root=Path(self.temporary.name) / "settings")
        self.store.update({"host": "example.test", "username": "tester", "password": SENTINEL})
        self.backend = SimpleNamespace(
            test_connection=mock.Mock(return_value={"connected": True}),
            trust_host=mock.Mock(return_value={"trusted": True}),
        )
        self.manager = SimpleNamespace(config_store=self.store, backend=self.backend)
        for method in ("start", "get_job", "list_jobs", "cancel_job", "resume_job", "discard_transfer"):
            setattr(self.manager, method, mock.Mock(return_value={"job_id": "job-1", "state": "queued"}))
        self.tools = build_file_transfer_tools(manager=self.manager)
        self.logger = mock.Mock(spec=logging.Logger)
        self.dispatcher = McpJsonRpcServer("file transfer contract", "1", self.tools, logger=self.logger)

    def call(self, name, arguments=None):
        return self.dispatcher.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": name, "arguments": arguments or {}}}
        )

    def test_descriptors_do_not_import_paramiko_or_create_manager(self):
        original = builtins.__import__

        def without_transport(name, *args, **kwargs):
            if "paramiko" in name or name.endswith("file_transfer_jobs"):
                raise AssertionError("Descriptor construction must not import a transport or job manager")
            return original(name, *args, **kwargs)

        with mock.patch.object(builtins, "__import__", side_effect=without_transport):
            tools = build_file_transfer_tools()
        self.assertEqual(len(tools), 11)
        self.assertEqual(len({tool.name for tool in tools}), 11)

    def test_public_tools_use_one_file_transfer_prefix(self):
        expected_names = {
            "file_transfer_get_config",
            "file_transfer_update_config",
            "file_transfer_test_connection",
            "file_transfer_trust_host_key",
            "file_transfer_start_upload",
            "file_transfer_start_download",
            "file_transfer_get_job",
            "file_transfer_list_jobs",
            "file_transfer_cancel_job",
            "file_transfer_resume_job",
            "file_transfer_discard",
        }
        response = self.dispatcher.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}}
        )
        names = [tool["name"] for tool in response["result"]["tools"]]
        self.assertEqual(set(names), expected_names)
        self.assertEqual(len(names), 11)
        self.assertTrue(all(name.startswith("file_transfer_") for name in names))

    def test_legacy_tool_names_are_unknown_without_dispatching(self):
        legacy_names = (
            "get_scp_config", "update_scp_config", "test_scp_connection",
            "trust_scp_host_key", "start_scp_upload", "start_scp_download",
            "get_scp_job", "list_scp_jobs", "cancel_scp_job", "resume_scp_job",
            "discard_scp_transfer",
        )
        before = self.store.path.read_bytes()
        for name in legacy_names:
            with self.subTest(name=name):
                response = self.call(name)
                self.assertNotIn("result", response)
                self.assertEqual(response["error"]["code"], -32602)
                self.assertEqual(response["error"]["message"], f"Unknown tool: {name}")
        self.assertEqual(self.store.path.read_bytes(), before)
        self.backend.test_connection.assert_not_called()
        self.backend.trust_host.assert_not_called()
        for method in ("start", "get_job", "list_jobs", "cancel_job", "resume_job", "discard_transfer"):
            getattr(self.manager, method).assert_not_called()

    def test_config_queries_and_updates_never_return_password(self):
        for name, arguments in (
            ("file_transfer_get_config", {}),
            ("file_transfer_update_config", {"password": SENTINEL, "port": 2222}),
        ):
            response = self.call(name, arguments)
            self.assertNotIn(SENTINEL, json.dumps(response))
            public = response["result"]["structuredContent"]
            self.assertNotIn("password", public)
            self.assertTrue(public["password_configured"])

    def test_password_schema_error_does_not_echo_supplied_value(self):
        response = self.call("file_transfer_update_config", {"password": [SENTINEL]})
        self.assertIn("error", response)
        self.assertNotIn(SENTINEL, json.dumps(response))
        self.logger.exception.assert_not_called()

    def test_upload_download_defaults_and_overrides_are_forwarded(self):
        self.call("file_transfer_start_upload")
        self.manager.start.assert_called_with("upload", sources=None, destination=None, overwrite=False, client_request_id=None)
        self.call("file_transfer_start_download", {"sources": ["C:/remote/a"], "destination": "C:/local", "overwrite": True, "client_request_id": "request-1"})
        self.manager.start.assert_called_with("download", sources=["C:/remote/a"], destination="C:/local", overwrite=True, client_request_id="request-1")

    def test_connection_and_host_confirmation_are_forwarded(self):
        self.call("file_transfer_test_connection")
        self.assertEqual(self.backend.test_connection.call_args.args[0]["password"], SENTINEL)
        self.call("file_transfer_trust_host_key", {"fingerprint": "SHA256:expected", "confirmation_token": "one-time"})
        self.assertEqual(self.backend.trust_host.call_args.args[1:], ("SHA256:expected", "one-time"))

    def test_job_operations_are_forwarded(self):
        for tool, method, args, expected_args, expected_kwargs in (
            ("file_transfer_get_job", "get_job", {"job_id": "job-1"}, ("job-1",), {"files_offset": 0, "files_limit": 100}),
            ("file_transfer_list_jobs", "list_jobs", {"states": ["failed"], "limit": 10}, (), {"states": ["failed"], "limit": 10}),
            ("file_transfer_cancel_job", "cancel_job", {"job_id": "job-1"}, ("job-1",), {}),
            ("file_transfer_resume_job", "resume_job", {"job_id": "job-1", "client_request_id": "retry-1"}, ("job-1",), {"client_request_id": "retry-1"}),
            ("file_transfer_discard", "discard_transfer", {"transfer_id": "transfer-1"}, ("transfer-1",), {}),
        ):
            self.call(tool, args)
            getattr(self.manager, method).assert_called_with(*expected_args, **expected_kwargs)

    def test_job_file_pagination_is_forwarded(self):
        self.call("file_transfer_get_job", {"job_id": "job-1", "files_offset": 200, "files_limit": 50})
        self.manager.get_job.assert_called_once_with("job-1", files_offset=200, files_limit=50)

    def test_job_file_pagination_schema_rejects_invalid_ranges(self):
        for changes in ({"files_offset": -1}, {"files_offset": True}, {"files_limit": 0}, {"files_limit": 201}, {"files_limit": 1.5}):
            with self.subTest(changes=changes):
                response = self.call("file_transfer_get_job", {"job_id": "job-1", **changes})
                self.assertIn("error", response)
        self.manager.get_job.assert_not_called()

    def test_short_password_does_not_damage_job_and_host_control_values(self):
        self.store.update({"password": "a"})
        snapshot = {"job_id": "a123", "transfer_id": "abc", "state": "cancelled", "phase": "transferring",
                    "fingerprint": "SHA256:abc", "confirmation_token": "abc", "sha256": "a" * 64,
                    "source": "C:/data/a.txt", "sources": ["C:/data/a.txt"],
                    "cleanup": {"residual_paths": ["C:/data/partial"]},
                    "errors": [{"code": "authentication_failed", "message": "password=a"}]}
        self.manager.get_job.return_value = snapshot
        result = self.call("file_transfer_get_job", {"job_id": "a123"})["result"]["structuredContent"]
        for key in set(snapshot) - {"errors"}:
            self.assertEqual(result[key], snapshot[key])
        self.assertEqual(result["errors"][0]["code"], "authentication_failed")
        self.assertIn("[redacted]", result["errors"][0]["message"])

    def test_fingerprint_confirmation_survives_password_prefix_collision(self):
        self.store.update({"password": "SHA256"})
        self.backend.test_connection.side_effect = FileTransferError(
            "host_key_unknown", "Confirm the SSH fingerprint.",
            details={"fingerprint": "SHA256:abcdef", "confirmation_token": "SHA256-token"},
        )
        result = self.call("file_transfer_test_connection")["result"]["structuredContent"]["error"]
        self.assertEqual(result["details"]["fingerprint"], "SHA256:abcdef")
        self.assertEqual(result["details"]["confirmation_token"], "SHA256-token")

    def test_redaction_filters_credential_fields_but_keeps_protocol_values(self):
        value = {"password": "a", "nested": {"PaSsWd": "a", "passphrase": "a", "message": "a", "unknown": "a"},
                 "state": "failed", "code": "authentication_failed", "client_request_id": "a"}
        public = public_value(value, ("a",))
        self.assertNotIn("password", public)
        self.assertEqual(public["nested"], {"message": "[redacted]", "unknown": "[redacted]"})
        self.assertEqual(public["state"], "failed")
        self.assertEqual(public["code"], "authentication_failed")
        self.assertEqual(public["client_request_id"], "a")

    def test_redaction_is_idempotent_for_short_passwords_and_multiple_secrets(self):
        value = {"message": "a and old-secret", "details": {"reason": "old-secret a"}, "state": "failed"}
        first = public_value(value, ("a", "old-secret"))
        self.assertEqual(public_value(first, ("a", "old-secret")), first)
        self.assertNotIn("[red[redacted]cted]", json.dumps(first))

    def test_expected_errors_and_unexpected_exceptions_are_redacted(self):
        for error in (
            FileTransferError("authentication_failed", "Authentication failed: " + SENTINEL, details={"password": SENTINEL, "reason": SENTINEL}),
            RuntimeError("Failure with " + SENTINEL),
        ):
            self.manager.start.side_effect = error
            response = self.call("file_transfer_start_upload")
            self.assertTrue(response["result"]["isError"])
            self.assertNotIn(SENTINEL, json.dumps(response))
        self.logger.exception.assert_not_called()

    def test_nested_password_fields_and_values_are_removed_from_public_jobs(self):
        self.manager.get_job.return_value = {"job_id": "job-1", "nested": {"password": SENTINEL, "message": SENTINEL}}
        response = self.call("file_transfer_get_job", {"job_id": "job-1"})
        public = response["result"]["structuredContent"]
        self.assertNotIn(SENTINEL, json.dumps(response))
        self.assertNotIn("password", public["nested"])


if __name__ == "__main__":
    unittest.main()
