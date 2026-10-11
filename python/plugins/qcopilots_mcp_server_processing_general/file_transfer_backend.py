"""Password-authenticated SSH/SFTP with explicit, pinned host-key trust.

Paramiko is imported only when an SSH operation is requested. Host-key
verification happens before authentication, including when probing an unknown
host. User OpenSSH files are read-only and new trust belongs to this plugin.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import base64
import errno
import fnmatch
import hashlib
import importlib
import ipaddress
import os
import secrets
import select
import socket
import tempfile
import threading
import time
from pathlib import Path

from qcopilots_common.security_policy import filesystem_policy_from_environment

from .file_transfer_errors import FileTransferError
from .file_transfer_connection import validate_connection_fields
from .file_transfer_io import SftpClientAdapter


_CONFIRMATION_TTL = 600
_TRUST_LOCK = threading.RLock()


def _paramiko():
    try:
        return importlib.import_module("paramiko")
    except (ImportError, OSError):
        raise FileTransferError(
            "dependency_unavailable",
            "The file transfer service environment must install its locked SSH dependencies.",
        ) from None


def _fingerprint(key):
    digest = hashlib.sha256(key.asbytes()).digest()
    return "SHA256:" + base64.b64encode(digest).decode("ascii").rstrip("=")


def _close_quietly(value):
    if value is not None:
        try:
            value.close()
        except Exception:
            # Closing is best effort. Never log credential-bearing exceptions.
            pass


def _check_cancel(control):
    if control is not None:
        control.check()


def _release(resource, control):
    if resource is not None:
        if control is not None:
            control.unregister(resource)
        _close_quietly(resource)


def _connect_socket(host, port, timeout, control):
    """Connect with cancellable TCP I/O, using no DNS for numeric addresses.

    With a controller, hostname resolution happens before the socket loop. The
    operating system's DNS call may finish after a cancellation request, but its
    result is checked before any socket or authentication is started.
    """
    if control is None:
        return socket.create_connection((host, port), timeout=timeout)
    control.check()
    deadline = time.monotonic() + timeout
    try:
        address = ipaddress.ip_address(host)
    except ValueError:
        addresses = socket.getaddrinfo(host, port, type=socket.SOCK_STREAM)
        control.check()
    else:
        family = socket.AF_INET6 if address.version == 6 else socket.AF_INET
        if address.version == 6 and address.scope_id is not None:
            # IPv6 zones belong in sockaddr's scope_id, not the host string.
            # AI_NUMERICHOST resolves the zone while prohibiting DNS lookup.
            addresses = socket.getaddrinfo(host, port, family, socket.SOCK_STREAM, 0, socket.AI_NUMERICHOST)
            control.check()
        else:
            addresses = [(family, socket.SOCK_STREAM, 0, "", (host, port))]
    pending = {errno.EINPROGRESS, errno.EWOULDBLOCK, errno.EALREADY, errno.EINTR,
               10035, 10036, 10037}
    last_error = None
    for family, socktype, protocol, _name, endpoint in addresses:
        control.check()
        if time.monotonic() >= deadline:
            raise socket.timeout()
        sock = None
        try:
            sock = socket.socket(family, socktype, protocol)
            control.register(sock, kind="socket")
            sock.setblocking(False)
            control.check()
            result = sock.connect_ex(endpoint)
            if result not in {0, errno.EISCONN, 10056}:
                if result not in pending:
                    raise OSError(result, "The SSH TCP connection failed.")
                while True:
                    control.check()
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise socket.timeout()
                    _, writable, exceptional = select.select([], [sock], [sock], min(0.1, remaining))
                    control.check()
                    if writable or exceptional:
                        result = sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
                        if result:
                            raise OSError(result, "The SSH TCP connection failed.")
                        break
            control.check()
            sock.settimeout(timeout)
            return sock
        except Exception as error:
            _release(sock, control)
            control.check()
            if isinstance(error, (OSError, ValueError)):
                last_error = error
                continue
            raise
    if last_error is not None:
        raise last_error
    raise OSError("The SSH host did not resolve to a TCP address.")


class FileTransferSession:
    """Owned SSH connection. Closing also interrupts blocked SFTP operations."""

    def __init__(self, transport, sftp, fingerprint, sock=None, cancel_control=None):
        self._transport = transport
        self._raw_sftp = sftp
        self.sftp = SftpClientAdapter(sftp, cancel_control)
        self.fingerprint = fingerprint
        self._socket = sock
        self._cancel_control = cancel_control
        self._close_lock = threading.Lock()
        self._closed = False

    def close(self):
        with self._close_lock:
            if self._closed:
                return
            self._closed = True
        _release(self._transport, self._cancel_control)
        _release(self._raw_sftp, self._cancel_control)
        _release(self._socket, self._cancel_control)

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()


class FileTransferBackend:
    def __init__(self, config_store, policy=None):
        self.config_store = config_store
        self.policy = policy if policy is not None else filesystem_policy_from_environment()
        self._pending = {}
        self._pending_lock = threading.RLock()

    def _check_policy(self):
        if not self.policy.network_enabled or self.policy.restricted:
            raise FileTransferError(
                "network_disallowed",
                "SSH transfers require enabled networking in compatible mode.",
            )

    @staticmethod
    def _endpoint(config):
        return validate_connection_fields(config, required=True, code="invalid_configuration")

    @staticmethod
    def _host_name(host, port):
        return host if port == 22 else f"[{host}]:{port}"

    def _paths(self, config):
        try:
            own = self.policy.resolve_path(
                Path(self.config_store.root) / "file_transfer_known_hosts", access="write"
            )
            configured = config.get("known_hosts_file") or str(Path.home() / ".ssh" / "known_hosts")
            user = self.policy.resolve_path(configured, access="read")
        except (ValueError, OSError):
            raise FileTransferError("known_hosts_invalid", "The configured host trust path is not a supported local file.") from None
        return [user, own] if user != own else [own]

    @staticmethod
    def _matches(paramiko, hostname, patterns):
        matched = False
        for pattern in patterns.split(","):
            negate = pattern.startswith("!")
            pattern = pattern[1:] if negate else pattern
            if pattern.startswith("|1|"):
                try:
                    hit = secrets.compare_digest(paramiko.HostKeys.hash_host(hostname, pattern), pattern)
                except (ValueError, AssertionError, TypeError):
                    raise FileTransferError("known_hosts_invalid", "A known-hosts entry contains an invalid host hash.") from None
            else:
                # OpenSSH supports glob host patterns. Brackets in an endpoint
                # are literal port delimiters, not fnmatch character classes.
                escaped = pattern.replace("[", "[[]")
                hit = fnmatch.fnmatchcase(hostname.lower(), escaped.lower())
            if hit and negate:
                return False
            matched = matched or hit
        return matched

    def _trusted_keys(self, paramiko, config):
        host, port, _username = self._endpoint(config)
        hostname = self._host_name(host, port)
        keys = {}
        for path in self._paths(config):
            try:
                content = path.read_text(encoding="utf-8-sig")
            except FileNotFoundError:
                continue
            except (OSError, UnicodeError):
                raise FileTransferError("known_hosts_invalid", "The known-hosts file could not be read.") from None
            for line_number, line in enumerate(content.splitlines(), 1):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                fields = line.split()
                marker = fields.pop(0) if fields[0].startswith("@") else None
                if len(fields) < 3:
                    raise FileTransferError("known_hosts_invalid", "The known-hosts file contains an incomplete entry.")
                if not self._matches(paramiko, hostname, fields[0]):
                    continue
                if marker is not None:
                    raise FileTransferError(
                        "host_key_changed" if marker == "@revoked" else "known_hosts_invalid",
                        "The host has a revoked or unsupported marked trust entry.",
                    )
                try:
                    entry = paramiko.hostkeys.HostKeyEntry.from_line(" ".join(fields), line_number)
                    key = entry.key if entry else None
                    if key is None:
                        raise ValueError("Unsupported key")
                    algorithm = key.get_name()
                    encoded = key.asbytes()
                except Exception:
                    raise FileTransferError("known_hosts_invalid", "A matching known-hosts entry has an invalid SSH key.") from None
                if algorithm in keys and keys[algorithm].asbytes() != encoded:
                    raise FileTransferError("host_key_changed", "Known-hosts files contain conflicting keys for this host.")
                keys[algorithm] = key
        return keys

    def _probe(self, paramiko, config, keys, cancel_control=None):
        host, port, _username = self._endpoint(config)
        transport = sock = None
        try:
            timeout = config.get("connect_timeout", 15)
            _check_cancel(cancel_control)
            sock = _connect_socket(host, port, timeout, cancel_control)
            if cancel_control is not None:
                cancel_control.register(sock, kind="socket")
            _check_cancel(cancel_control)
            transport = paramiko.Transport(sock)
            if cancel_control is not None:
                cancel_control.register(transport, kind="transport")
            transport.banner_timeout = timeout
            transport.auth_timeout = timeout
            transport.channel_timeout = config.get("io_timeout", 30)
            options = transport.get_security_options()
            preferred = tuple(
                name for name in options.key_types
                if name in keys or (name.startswith("rsa-sha2-") and "ssh-rsa" in keys)
            )
            if preferred:
                options.key_types = preferred + tuple(name for name in options.key_types if name not in preferred)
            _check_cancel(cancel_control)
            transport.start_client(timeout=timeout)
            _check_cancel(cancel_control)
            key = transport.get_remote_server_key()
            _check_cancel(cancel_control)
            return transport, key, sock
        except Exception:
            _release(transport, cancel_control)
            _release(sock, cancel_control)
            _check_cancel(cancel_control)
            raise

    @staticmethod
    def _verify_known(keys, key):
        if not keys:
            return False
        expected = keys.get(key.get_name())
        if expected is None or not secrets.compare_digest(expected.asbytes(), key.asbytes()):
            raise FileTransferError("host_key_changed", "The SSH server key does not match the trusted host identity.")
        return True

    def _binding(self, config):
        return (*self._endpoint(config), str(self._paths(config)[0]))

    def _issue_confirmation(self, config, key):
        fingerprint = _fingerprint(key)
        now = time.monotonic()
        with self._pending_lock:
            self._pending = {k: v for k, v in self._pending.items() if v["deadline"] > now}
            while len(self._pending) >= 128:
                self._pending.pop(next(iter(self._pending)))
            token = secrets.token_urlsafe(32)
            self._pending[token] = {
                "binding": self._binding(config),
                "fingerprint": fingerprint,
                "algorithm": key.get_name(),
                "deadline": now + _CONFIRMATION_TTL,
            }
        host, port, _username = self._endpoint(config)
        return {
            "host": host,
            "port": port,
            "algorithm": key.get_name(),
            "fingerprint": fingerprint,
            "confirmation_token": token,
            "expires_in": _CONFIRMATION_TTL,
        }

    @staticmethod
    def _controlled_error(exc, paramiko):
        if isinstance(exc, FileTransferError):
            return exc
        if isinstance(exc, paramiko.AuthenticationException):
            return FileTransferError("authentication_failed", "SSH password authentication failed.")
        if isinstance(exc, (TimeoutError, socket.timeout)):
            return FileTransferError("connection_timeout", "The SSH connection timed out.", retryable=True)
        if isinstance(exc, (OSError, EOFError, paramiko.SSHException)):
            return FileTransferError("connection_failed", "The SSH connection could not be completed.", retryable=True)
        return FileTransferError("ssh_failed", "The SSH operation failed.")

    def connect(self, config, expected_fingerprint=None, cancel_control=None):
        """Open verified SFTP. Unknown and changed keys stop before password auth."""
        self._check_policy()
        _check_cancel(cancel_control)
        _host, _port, username = self._endpoint(config)
        paramiko = _paramiko()
        transport = sftp = sock = None
        try:
            keys = self._trusted_keys(paramiko, config)
            _check_cancel(cancel_control)
            transport, key, sock = self._probe(paramiko, config, keys, cancel_control)
            fingerprint = _fingerprint(key)
            known = self._verify_known(keys, key)
            if expected_fingerprint is not None and not secrets.compare_digest(fingerprint, expected_fingerprint):
                raise FileTransferError("host_key_changed", "The SSH key differs from the transfer's pinned host identity.")
            if not known:
                raise FileTransferError(
                    "host_key_unknown",
                    "Confirm the SSH host fingerprint before trusting this server.",
                    details=self._issue_confirmation(config, key),
                )
            _check_cancel(cancel_control)
            transport.auth_password(username, config.get("password", ""), fallback=False)
            _check_cancel(cancel_control)
            if not transport.is_authenticated():
                raise FileTransferError("authentication_failed", "SSH password authentication was not completed.")
            sftp = paramiko.SFTPClient.from_transport(transport)
            if cancel_control is not None:
                cancel_control.register(sftp, kind="sftp")
            _check_cancel(cancel_control)
            sftp.get_channel().settimeout(config.get("io_timeout", 30))
            _check_cancel(cancel_control)
            return FileTransferSession(transport, sftp, fingerprint, sock, cancel_control)
        except Exception as exc:
            _release(transport, cancel_control)
            _release(sftp, cancel_control)
            _release(sock, cancel_control)
            _check_cancel(cancel_control)
            raise self._controlled_error(exc, paramiko) from None

    def test_connection(self, config):
        with self.connect(config) as session:
            host, port, username = self._endpoint(config)
            return {
                "connected": True,
                "host": host,
                "port": port,
                "username": username,
                "fingerprint": session.fingerprint,
                "protocol": "sftp",
            }

    def trust_host(self, config, fingerprint, confirmation_token):
        """Save a freshly re-probed key after an explicit, short-lived confirmation."""
        self._check_policy()
        self._endpoint(config)
        if not isinstance(fingerprint, str) or not isinstance(confirmation_token, str):
            raise FileTransferError("host_key_confirmation_invalid", "A host fingerprint and confirmation token are required.")
        with self._pending_lock:
            pending = self._pending.get(confirmation_token)
            if (
                not pending
                or pending["deadline"] <= time.monotonic()
                or pending["binding"] != self._binding(config)
                or not secrets.compare_digest(pending["fingerprint"], fingerprint)
            ):
                raise FileTransferError("host_key_confirmation_invalid", "Probe the host again to obtain a valid fingerprint confirmation.")
            # A token authorizes exactly one trust attempt, including concurrent
            # calls. A failed attempt can be retried after probing again.
            self._pending.pop(confirmation_token)
        paramiko = _paramiko()
        transport = sock = None
        try:
            keys = self._trusted_keys(paramiko, config)
            transport, key, sock = self._probe(paramiko, config, keys)
            self._verify_known(keys, key)
            if (
                not secrets.compare_digest(_fingerprint(key), fingerprint)
                or key.get_name() != pending["algorithm"]
            ):
                raise FileTransferError("host_key_changed", "The SSH host key changed after the confirmation was issued.")
            with _TRUST_LOCK:
                if pending["deadline"] <= time.monotonic():
                    raise FileTransferError("host_key_confirmation_invalid", "The host fingerprint confirmation has expired.")
                # Re-read after the handshake, in case another trust action or
                # manual edit changed either known-hosts file in the meantime.
                self._verify_known(self._trusted_keys(paramiko, config), key)
                self._save_key(config, key)
            with self._pending_lock:
                self._pending.pop(confirmation_token, None)
            host, port, _username = self._endpoint(config)
            return {"trusted": True, "host": host, "port": port, "algorithm": key.get_name(), "fingerprint": fingerprint}
        except Exception as exc:
            raise self._controlled_error(exc, paramiko) from None
        finally:
            _close_quietly(transport)
            _close_quietly(sock)

    def _save_key(self, config, key):
        target = self._paths(config)[-1]
        temporary = None
        try:
            target.parent.mkdir(parents=True, exist_ok=True)
            previous = target.read_text(encoding="utf-8") if target.exists() else ""
            host, port, _username = self._endpoint(config)
            line = f"{self._host_name(host, port)} {key.get_name()} {base64.b64encode(key.asbytes()).decode('ascii')}\n"
            with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=target.parent, prefix=".file_transfer_known_hosts-", delete=False) as handle:
                temporary = Path(handle.name)
                handle.write(previous + ("\n" if previous and not previous.endswith("\n") else "") + line)
                handle.flush()
                os.fsync(handle.fileno())
            os.replace(temporary, target)
            temporary = None
        except (OSError, UnicodeError):
            raise FileTransferError("known_hosts_write_failed", "The plugin's host trust file could not be saved.") from None
        finally:
            if temporary is not None:
                try:
                    temporary.unlink(missing_ok=True)
                except OSError:
                    pass
