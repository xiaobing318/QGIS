"""SSH backend contract tests without Paramiko, QGIS or a live SSH server.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

import base64
import errno
import hashlib
import hmac
import json
import socket
import sys
import tempfile
import threading
import time
import types
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python" / "plugins"))

from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general import file_transfer_backend
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError
from qcopilots_mcp_server_processing_general.file_transfer_cancellation import CancellationController, ConnectionCancelled


class FakeKey:
    def __init__(self, value=b"test-key-one", algorithm="ssh-ed25519"):
        self.value, self.algorithm = value, algorithm

    def get_name(self):
        return self.algorithm

    def asbytes(self):
        return self.value

    def line(self, host="test.example"):
        return f"{host} {self.algorithm} {base64.b64encode(self.value).decode('ascii')}\n"


class FakeHostKeyEntry:
    @staticmethod
    def from_line(line, _number):
        _hosts, algorithm, encoded, *_rest = line.split()
        return types.SimpleNamespace(key=FakeKey(base64.b64decode(encoded, validate=True), algorithm))


class FakeHostKeys:
    @staticmethod
    def hash_host(hostname, salt):
        assert salt.startswith("|1|")
        _empty, _version, salt64, _hash64 = salt.split("|")
        salt_bytes = base64.b64decode(salt64, validate=True)
        digest = hmac.new(salt_bytes, hostname.encode(), hashlib.sha1).digest()
        return "|1|" + salt64 + "|" + base64.b64encode(digest).decode()


class FakeSSHException(Exception):
    pass


class FakeAuthenticationException(FakeSSHException):
    pass


class TestFileTransferBackend(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-backend-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.user_known = self.root / "user_known_hosts"
        self.own_known = self.root / "file_transfer_known_hosts"
        self.config = {
            "host": "test.example", "port": 22, "username": "test-account",
            "password": "unit-test-secret", "connect_timeout": 15, "io_timeout": 30,
            "known_hosts_file": str(self.user_known),
        }
        self.key = FakeKey()
        self.transport = mock.Mock()
        self.options = types.SimpleNamespace(key_types=("ssh-ed25519", "rsa-sha2-512", "ssh-rsa"))
        self.transport.get_security_options.return_value = self.options
        self.transport.get_remote_server_key.side_effect = lambda: self.key
        self.transport.is_authenticated.return_value = True
        self.sftp = mock.Mock()
        self.socket = mock.Mock()
        self.paramiko = types.SimpleNamespace(
            Transport=mock.Mock(return_value=self.transport),
            SFTPClient=types.SimpleNamespace(from_transport=mock.Mock(return_value=self.sftp)),
            HostKeys=FakeHostKeys,
            hostkeys=types.SimpleNamespace(HostKeyEntry=FakeHostKeyEntry),
            AuthenticationException=FakeAuthenticationException, SSHException=FakeSSHException,
        )
        self.load = mock.patch.object(file_transfer_backend, "_paramiko", return_value=self.paramiko).start()
        self.addCleanup(mock.patch.stopall)
        self.create_socket = mock.patch.object(file_transfer_backend.socket, "create_connection", return_value=self.socket).start()
        self.backend = file_transfer_backend.FileTransferBackend(types.SimpleNamespace(root=self.root), FilesystemPolicy())

    def assert_error(self, code, call, *args, **kwargs):
        with self.assertRaises(FileTransferError) as caught:
            call(*args, **kwargs)
        self.assertEqual(caught.exception.code, code)
        self.assertNotIn(self.config["password"], str(caught.exception))
        self.assertNotIn(self.config["password"], json.dumps(caught.exception.details))
        return caught.exception

    def unknown(self):
        return self.assert_error("host_key_unknown", self.backend.connect, self.config).details

    def test_unknown_key_is_rejected_before_authentication(self):
        details = self.unknown()
        self.assertEqual(details["fingerprint"], file_transfer_backend._fingerprint(self.key))
        self.assertEqual(details["algorithm"], "ssh-ed25519")
        self.assertEqual(details["expires_in"], 600)
        self.assertTrue(details["confirmation_token"])
        self.transport.auth_password.assert_not_called()
        self.paramiko.SFTPClient.from_transport.assert_not_called()
        self.transport.close.assert_called_once()
        self.assertFalse(self.own_known.exists())

    def test_trust_reprobes_and_only_writes_plugin_file(self):
        self.user_known.write_text("# preserved user file\n" + FakeKey(b"elsewhere").line("another.example"))
        before = self.user_known.read_bytes()
        details = self.unknown()
        result = self.backend.trust_host(self.config, details["fingerprint"], details["confirmation_token"])
        self.assertTrue(result["trusted"])
        self.assertEqual(self.own_known.read_text(), self.key.line())
        self.assertEqual(self.user_known.read_bytes(), before)
        self.assertEqual(self.transport.start_client.call_count, 2)
        self.transport.auth_password.assert_not_called()
        self.assertEqual(list(self.root.glob(".file_transfer_known_hosts-*")), [])
        self.assert_error("host_key_confirmation_invalid", self.backend.trust_host, self.config, details["fingerprint"], details["confirmation_token"])

    def test_trusted_connection_and_idempotent_close(self):
        self.user_known.write_text(self.key.line())
        session = self.backend.connect(self.config)
        self.assertEqual(session.fingerprint, file_transfer_backend._fingerprint(self.key))
        self.assertIs(session.sftp._raw, self.sftp)
        self.transport.auth_password.assert_called_once_with("test-account", "unit-test-secret", fallback=False)
        self.create_socket.assert_called_once_with(("test.example", 22), timeout=15)
        self.sftp.get_channel.return_value.settimeout.assert_called_once_with(30)
        self.assertEqual(self.transport.auth_timeout, 15)
        self.assertEqual(self.transport.channel_timeout, 30)
        session.close()
        session.close()
        self.transport.close.assert_called_once()
        self.sftp.close.assert_called_once()

    def test_connection_result_has_no_credentials_and_closes(self):
        self.own_known.write_text(self.key.line())
        result = self.backend.test_connection(self.config)
        self.assertTrue(result["connected"])
        self.assertEqual(result["protocol"], "sftp")
        self.assertNotIn(self.config["password"], json.dumps(result))
        self.transport.close.assert_called_once()

    def test_changed_host_never_authenticates(self):
        self.user_known.write_text(FakeKey(b"old-key").line())
        self.assert_error("host_key_changed", self.backend.connect, self.config)
        self.transport.auth_password.assert_not_called()

    def test_conflicting_known_files_rejected_before_network(self):
        self.user_known.write_text(self.key.line())
        self.own_known.write_text(FakeKey(b"conflict").line())
        self.assert_error("host_key_changed", self.backend.connect, self.config)
        self.create_socket.assert_not_called()

    def test_conflicting_duplicates_in_single_file_rejected(self):
        self.user_known.write_text(self.key.line() + FakeKey(b"conflict").line())
        self.assert_error("host_key_changed", self.backend.connect, self.config)
        self.create_socket.assert_not_called()

    def test_matching_duplicate_keys_allowed(self):
        self.user_known.write_text(self.key.line() * 2)
        self.own_known.write_text(self.key.line())
        self.backend.connect(self.config).close()
        self.transport.auth_password.assert_called_once()

    def test_saved_fingerprint_must_match_even_when_current_host_trusted(self):
        self.user_known.write_text(self.key.line())
        self.assert_error("host_key_changed", self.backend.connect, self.config, expected_fingerprint="SHA256:old")
        self.transport.auth_password.assert_not_called()

    def test_pinned_unknown_key_still_requires_explicit_trust(self):
        self.assert_error("host_key_unknown", self.backend.connect, self.config, expected_fingerprint=file_transfer_backend._fingerprint(self.key))
        self.transport.auth_password.assert_not_called()

    def test_confirmation_bound_to_endpoint_account_and_trust_path(self):
        details = self.unknown()
        for updates in ({"host": "other.example"}, {"port": 2222}, {"username": "other"}, {"known_hosts_file": str(self.root / "other_hosts")}):
            with self.subTest(updates=updates):
                self.assert_error("host_key_confirmation_invalid", self.backend.trust_host, {**self.config, **updates}, details["fingerprint"], details["confirmation_token"])
        self.assertEqual(self.transport.start_client.call_count, 1)

    def test_confirmation_expiry_checked_before_network(self):
        details = self.unknown()
        with mock.patch.object(file_transfer_backend.time, "monotonic", return_value=float("inf")):
            self.assert_error("host_key_confirmation_invalid", self.backend.trust_host, self.config, details["fingerprint"], details["confirmation_token"])
        self.assertEqual(self.transport.start_client.call_count, 1)

    def test_confirmation_expiry_during_probe_prevents_save(self):
        details = self.unknown()
        deadline = self.backend._pending[details["confirmation_token"]]["deadline"]
        with mock.patch.object(file_transfer_backend.time, "monotonic", side_effect=[deadline - 1, deadline + 1]):
            self.assert_error("host_key_confirmation_invalid", self.backend.trust_host, self.config, details["fingerprint"], details["confirmation_token"])
        self.assertFalse(self.own_known.exists())

    def test_changed_key_during_confirmation_not_saved(self):
        details = self.unknown()
        self.key = FakeKey(b"changed-after-prompt")
        self.assert_error("host_key_changed", self.backend.trust_host, self.config, details["fingerprint"], details["confirmation_token"])
        self.assertFalse(self.own_known.exists())
        self.transport.auth_password.assert_not_called()

    def test_confirmation_token_cannot_be_reused_during_active_attempt(self):
        details = self.unknown()
        def concurrent_attempt(*_args, **_kwargs):
            self.assert_error(
                "host_key_confirmation_invalid", self.backend.trust_host,
                self.config, details["fingerprint"], details["confirmation_token"],
            )
        self.transport.start_client.side_effect = concurrent_attempt
        result = self.backend.trust_host(self.config, details["fingerprint"], details["confirmation_token"])
        self.assertTrue(result["trusted"])
        self.assertEqual(self.own_known.read_text(), self.key.line())

    def test_conflicting_manual_trust_edit_during_probe_rejected(self):
        details = self.unknown()
        def change_trust(*_args, **_kwargs):
            self.own_known.write_text(FakeKey(b"manual-edit").line())
        self.transport.start_client.side_effect = change_trust
        self.assert_error("host_key_changed", self.backend.trust_host, self.config, details["fingerprint"], details["confirmation_token"])
        self.assertEqual(self.own_known.read_text(), FakeKey(b"manual-edit").line())

    def test_failed_atomic_save_preserves_prior_file_and_removes_temporary(self):
        before = FakeKey(b"unrelated").line("other.example")
        self.own_known.write_text(before)
        details = self.unknown()
        with mock.patch.object(file_transfer_backend.os, "replace", side_effect=OSError(self.config["password"])):
            self.assert_error("known_hosts_write_failed", self.backend.trust_host, self.config, details["fingerprint"], details["confirmation_token"])
        self.assertEqual(self.own_known.read_text(), before)
        self.assertEqual(list(self.root.glob(".file_transfer_known_hosts-*")), [])

    def test_port_specific_and_hashed_hosts(self):
        self.config["port"] = 2222
        hostname = "[test.example]:2222"
        salt = "|1|" + base64.b64encode(b"1" * 20).decode() + "|ignored"
        self.user_known.write_text(self.key.line(FakeHostKeys.hash_host(hostname, salt)))
        self.backend.connect(self.config).close()
        self.transport.auth_password.assert_called_once()
        self.user_known.write_text(self.key.line(hostname))
        self.backend.connect(self.config).close()
        self.assertEqual(self.transport.auth_password.call_count, 2)

    def test_default_port_trust_does_not_apply_to_other_port(self):
        self.user_known.write_text(self.key.line())
        self.config["port"] = 2222
        self.unknown()

    def test_host_patterns_and_negation(self):
        self.user_known.write_text(self.key.line("*.example,!blocked.example"))
        self.backend.connect(self.config).close()
        self.config["host"] = "blocked.example"
        self.unknown()

    def test_revoked_host_rejected(self):
        self.user_known.write_text("@revoked " + self.key.line())
        self.assert_error("host_key_changed", self.backend.connect, self.config)
        self.create_socket.assert_not_called()

    def test_malformed_trust_data_rejected(self):
        for content in ("not-a-valid-entry", "test.example ssh-ed25519 !!!", "@cert-authority " + self.key.line()):
            with self.subTest(content=content):
                self.user_known.write_text(content)
                self.assert_error("known_hosts_invalid", self.backend.connect, self.config)
        self.create_socket.assert_not_called()

    def test_known_algorithm_is_preferred(self):
        self.key = FakeKey(algorithm="ssh-rsa")
        self.user_known.write_text(self.key.line())
        self.backend.connect(self.config).close()
        self.assertEqual(self.options.key_types, ("rsa-sha2-512", "ssh-rsa", "ssh-ed25519"))

    def test_authentication_failure_is_sanitized_and_closes(self):
        self.user_known.write_text(self.key.line())
        self.transport.auth_password.side_effect = FakeAuthenticationException(self.config["password"])
        self.assert_error("authentication_failed", self.backend.connect, self.config)
        self.transport.close.assert_called_once()

    def test_partial_authentication_not_accepted(self):
        self.user_known.write_text(self.key.line())
        self.transport.is_authenticated.return_value = False
        self.assert_error("authentication_failed", self.backend.connect, self.config)
        self.paramiko.SFTPClient.from_transport.assert_not_called()

    def test_connection_failure_types_are_sanitized(self):
        for exception, code in ((OSError, "connection_failed"), (EOFError, "connection_failed"), (FakeSSHException, "connection_failed"), (socket.timeout, "connection_timeout"), (RuntimeError, "ssh_failed")):
            with self.subTest(exception=exception):
                self.create_socket.side_effect = exception(self.config["password"])
                self.assert_error(code, self.backend.connect, self.config)

    def test_handshake_failure_closes_socket_and_transport(self):
        self.transport.start_client.side_effect = FakeSSHException(self.config["password"])
        self.assert_error("connection_failed", self.backend.connect, self.config)
        self.transport.close.assert_called_once()
        self.socket.close.assert_called_once()

    def test_sftp_setup_failure_closes_transport(self):
        self.user_known.write_text(self.key.line())
        self.sftp.get_channel.side_effect = OSError(self.config["password"])
        self.assert_error("connection_failed", self.backend.connect, self.config)
        self.transport.close.assert_called_once()
        self.sftp.close.assert_called_once()

    def test_policy_denies_all_ssh_paths_before_dependency_import(self):
        for policy in (FilesystemPolicy(network_enabled=False), FilesystemPolicy(mode="formal_restricted")):
            with self.subTest(policy=policy):
                backend = file_transfer_backend.FileTransferBackend(types.SimpleNamespace(root=self.root), policy)
                self.assert_error("network_disallowed", backend.connect, self.config)
                self.assert_error("network_disallowed", backend.test_connection, self.config)
                self.assert_error("network_disallowed", backend.trust_host, self.config, "fingerprint", "token")
        self.load.assert_not_called()
        self.create_socket.assert_not_called()

    def test_invalid_endpoint_and_timeouts_rejected(self):
        for updates in ({"host": "bad\nhost"}, {"host": "bad,host"}, {"port": True}, {"port": 0}, {"port": 65536}, {"username": ""}, {"password": None}, {"connect_timeout": float("nan")}, {"io_timeout": 0}):
            with self.subTest(updates=updates):
                self.assert_error("invalid_configuration", self.backend.connect, {**self.config, **updates})
        self.load.assert_not_called()

    def test_missing_paramiko_is_controlled_and_import_is_lazy(self):
        # Construction has not imported Paramiko. Exercise the real lazy loader.
        self.load.assert_not_called()
        with mock.patch.object(file_transfer_backend.importlib, "import_module", side_effect=ImportError(self.config["password"])):
            self.assert_error("dependency_unavailable", _REAL_PARAMIKO_LOADER)

    def test_pre_cancelled_connection_never_opens_socket_or_authenticates(self):
        control = CancellationController()
        control.cancel()
        with self.assertRaises(ConnectionCancelled):
            self.backend.connect(self.config, cancel_control=control)
        self.create_socket.assert_not_called()
        self.load.assert_not_called()
        self.transport.auth_password.assert_not_called()

    def test_socket_returning_after_cancellation_is_closed_before_handshake(self):
        control = CancellationController()
        def late_socket(*_args):
            control.cancel()
            return self.socket
        with mock.patch.object(file_transfer_backend, "_connect_socket", side_effect=late_socket):
            with self.assertRaises(ConnectionCancelled):
                self.backend.connect(self.config, cancel_control=control)
        self.socket.close.assert_called()
        self.paramiko.Transport.assert_not_called()

    def test_cancellation_after_key_read_prevents_authentication(self):
        control = CancellationController()
        self.user_known.write_text(self.key.line())
        def key_after_cancel():
            control.cancel()
            return self.key
        self.transport.get_remote_server_key.side_effect = key_after_cancel
        with mock.patch.object(file_transfer_backend, "_connect_socket", return_value=self.socket):
            with self.assertRaises(ConnectionCancelled):
                self.backend.connect(self.config, cancel_control=control)
        self.transport.auth_password.assert_not_called()
        self.paramiko.SFTPClient.from_transport.assert_not_called()

    def test_handshake_authentication_and_sftp_cancellation_finish_within_two_seconds(self):
        self.user_known.write_text(self.key.line())
        for phase in ("handshake", "authentication", "sftp"):
            with self.subTest(phase=phase):
                control = CancellationController()
                started, released = threading.Event(), threading.Event()
                results = []
                self.transport.reset_mock(side_effect=True)
                self.transport.get_remote_server_key.return_value = self.key
                self.transport.get_security_options.return_value = self.options
                self.transport.is_authenticated.return_value = True
                self.socket.shutdown.side_effect = lambda *_args: released.set()
                blocker = self.transport.start_client if phase == "handshake" else (
                    self.transport.auth_password if phase == "authentication" else self.paramiko.SFTPClient.from_transport)
                def blocked(*_args, **_kwargs):
                    started.set()
                    if not released.wait(5):
                        raise AssertionError("Registered socket was not interrupted")
                    raise OSError("Connection closed")
                blocker.side_effect = blocked
                def connect():
                    try:
                        results.append(self.backend.connect(self.config, cancel_control=control))
                    except Exception as error:
                        results.append(error)
                with mock.patch.object(file_transfer_backend, "_connect_socket", return_value=self.socket):
                    worker = threading.Thread(target=connect)
                    worker.start()
                    try:
                        self.assertTrue(started.wait(2), phase)
                        before = time.monotonic()
                        control.cancel()
                        worker.join(2)
                        self.assertFalse(worker.is_alive(), phase)
                        self.assertLess(time.monotonic() - before, 2, phase)
                        self.assertEqual(len(results), 1)
                        self.assertIsInstance(results[0], ConnectionCancelled)
                        if phase == "handshake":
                            self.transport.auth_password.assert_not_called()
                    finally:
                        released.set()
                        control.cancel()
                        worker.join(5)
                        blocker.side_effect = None

    def test_sftp_returning_after_cancellation_is_closed_not_delivered(self):
        self.user_known.write_text(self.key.line())
        control = CancellationController()
        def late_sftp(*_args):
            control.cancel()
            return self.sftp
        self.paramiko.SFTPClient.from_transport.side_effect = late_sftp
        with mock.patch.object(file_transfer_backend, "_connect_socket", return_value=self.socket):
            with self.assertRaises(ConnectionCancelled):
                self.backend.connect(self.config, cancel_control=control)
        self.sftp.close.assert_called()

    def test_session_close_unregisters_all_resources(self):
        self.user_known.write_text(self.key.line())
        control = CancellationController()
        with mock.patch.object(file_transfer_backend, "_connect_socket", return_value=self.socket):
            session = self.backend.connect(self.config, cancel_control=control)
        session.close()
        control.cancel()
        self.transport.close.assert_called_once()
        self.sftp.close.assert_called_once()
        self.socket.close.assert_called_once()

    def test_successful_session_remains_cancellable_until_closed(self):
        self.user_known.write_text(self.key.line())
        control = CancellationController()
        with mock.patch.object(file_transfer_backend, "_connect_socket", return_value=self.socket):
            session = self.backend.connect(self.config, cancel_control=control)
        control.cancel()
        self.socket.shutdown.assert_called_once_with(socket.SHUT_RDWR)
        self.transport.close.assert_called_once()
        self.sftp.close.assert_called_once()
        session.close()
        session.close()


class TestCancellableTcp(unittest.TestCase):
    def setUp(self):
        self.control = CancellationController()
        self.addCleanup(self.control.cancel)
        self.sock = mock.Mock()
        self.sock.connect_ex.return_value = 0
        self.sock.getsockopt.return_value = 0
        self.factory = mock.patch.object(file_transfer_backend.socket, "socket", return_value=self.sock).start()
        self.dns = mock.patch.object(file_transfer_backend.socket, "getaddrinfo").start()
        self.select = mock.patch.object(file_transfer_backend.select, "select", return_value=([], [self.sock], [])).start()
        self.addCleanup(mock.patch.stopall)

    def test_numeric_ipv4_and_ipv6_skip_dns_and_restore_socket_timeout(self):
        for host, family in (("192.0.2.1", socket.AF_INET), ("2001:db8::1", socket.AF_INET6)):
            with self.subTest(host=host):
                self.assertIs(file_transfer_backend._connect_socket(host, 2222, 15, self.control), self.sock)
                self.factory.assert_called_with(family, socket.SOCK_STREAM, 0)
                self.sock.connect_ex.assert_called_with((host, 2222))
                self.sock.setblocking.assert_called_with(False)
                self.sock.settimeout.assert_called_with(15)
        self.dns.assert_not_called()

    def test_pending_windows_connection_polls_at_most_100ms(self):
        self.sock.connect_ex.return_value = 10035
        self.assertIs(file_transfer_backend._connect_socket("192.0.2.1", 22, 15, self.control), self.sock)
        self.assertLessEqual(self.select.call_args.args[3], 0.1)
        self.sock.getsockopt.assert_called_once_with(socket.SOL_SOCKET, socket.SO_ERROR)

    def test_scoped_numeric_ipv6_preserves_scope_without_dns(self):
        self.dns.return_value = [(socket.AF_INET6, socket.SOCK_STREAM, 0, "", ("fe80::1", 22, 0, 19))]
        self.assertIs(file_transfer_backend._connect_socket("fe80::1%19", 22, 15, self.control), self.sock)
        self.dns.assert_called_once_with("fe80::1%19", 22, socket.AF_INET6, socket.SOCK_STREAM, 0, socket.AI_NUMERICHOST)
        self.sock.connect_ex.assert_called_once_with(("fe80::1", 22, 0, 19))

    def test_tcp_connection_cancels_within_two_seconds(self):
        self.sock.connect_ex.return_value = errno.EINPROGRESS
        def cancel_during_poll(*_args):
            self.control.cancel()
            return [], [], []
        self.select.side_effect = cancel_during_poll
        before = time.monotonic()
        with self.assertRaises(ConnectionCancelled):
            file_transfer_backend._connect_socket("192.0.2.1", 22, 15, self.control)
        self.assertLess(time.monotonic() - before, 2)
        self.sock.shutdown.assert_called_with(socket.SHUT_RDWR)
        self.sock.close.assert_called()

    def test_tcp_deadline_and_socket_errors_are_not_accepted_as_connected(self):
        self.sock.connect_ex.return_value = errno.EINPROGRESS
        with mock.patch.object(file_transfer_backend.time, "monotonic", side_effect=[0, 0, 2]):
            with self.assertRaises(socket.timeout):
                file_transfer_backend._connect_socket("192.0.2.1", 22, 1, self.control)
        self.sock.close.assert_called()
        self.sock.getsockopt.return_value = errno.ECONNREFUSED
        with self.assertRaises(OSError):
            file_transfer_backend._connect_socket("192.0.2.1", 22, 15, self.control)

    def test_dns_returning_after_cancellation_never_creates_socket(self):
        def resolve(*_args, **_kwargs):
            self.control.cancel()
            return [(socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.0.2.1", 22))]
        self.dns.side_effect = resolve
        with self.assertRaises(ConnectionCancelled):
            file_transfer_backend._connect_socket("example.test", 22, 15, self.control)
        self.factory.assert_not_called()


class TestCancellationController(unittest.TestCase):
    def test_cancel_closes_socket_before_transport_and_sftp(self):
        calls = []
        control = CancellationController()
        for kind in ("sftp", "transport", "socket"):
            resource = mock.Mock()
            resource.close.side_effect = lambda item=kind: calls.append(item)
            if kind == "socket":
                resource.shutdown.side_effect = lambda *_args: calls.append("shutdown")
            control.register(resource, kind=kind)
        control.cancel()
        control.cancel()
        self.assertEqual(calls, ["shutdown", "socket", "transport", "sftp"])

    def test_resource_close_never_holds_controller_lock(self):
        entered, release = threading.Event(), threading.Event()
        control = CancellationController()
        resource = mock.Mock()
        def blocked_close():
            entered.set()
            release.wait(5)
        resource.close.side_effect = blocked_close
        control.register(resource)
        worker = threading.Thread(target=control.cancel)
        worker.start()
        try:
            self.assertTrue(entered.wait(2))
            with self.assertRaises(ConnectionCancelled):
                control.check()
            late = mock.Mock()
            with self.assertRaises(ConnectionCancelled):
                control.register(late)
            late.close.assert_called_once()
            self.assertFalse(control.unregister(late))
        finally:
            release.set()
            worker.join(2)
        self.assertFalse(worker.is_alive())


_REAL_PARAMIKO_LOADER = file_transfer_backend._paramiko


if __name__ == "__main__":
    unittest.main()
