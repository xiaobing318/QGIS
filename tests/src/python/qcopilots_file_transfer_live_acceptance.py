"""Opt-in real SSH/SFTP acceptance for the General service transfer manager.

Run with the General service's locked Python environment. Credentials are read
only from QCOPILOTS_FILE_TRANSFER_TEST_PASSWORD. The runner creates a unique owned child
of each supplied base directory and never edits the user's file transfer configuration.
An unknown server requires --host-key-fingerprint with an independently
verified fingerprint, or a pre-existing entry in --known-hosts.

Every scenario uses the real FileTransferJobManager and FileTransferBackend. Disconnect scenarios
decorate a real SFTP connection and interrupt it only after transfer bytes
have reached the destination. Both directions exercise cancel, disconnect,
manager replacement, resume, discard and SHA256 validation. Optional
--corpus-root reads a persistent corpus without modifying it, checks its
greater-than-4-GiB file and 4096 small files, and transfers it in both directions.
Use --scenario-group baseline or corpus for separate runs and report files.
Every invocation gets its own UUID roots. A corpus-only run skips lifecycle
fixture generation and its seed upload.
This runner does not simulate a QGIS process restart or replace GUI tests.

Only scenario names, progress counters and controlled result codes are printed.
Failure reports include redacted exception metadata, stack locations without
locals, and the failing test file's transfer state. Cleanup failures produce a
failing exit status. Credentials and raw tracebacks are never printed.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import argparse
import errno
import hashlib
import json
import logging
import os
import platform
import re
import shutil
import stat
import sys
import tempfile
import threading
import time
import uuid
from pathlib import Path


MARKER_NAME = ".qcopilots-file-transfer-live-owner"
TERMINAL_STATES = frozenset({"succeeded", "failed", "cancelled"})
CHUNK_SIZE = 256 * 1024


class AcceptanceFailure(RuntimeError):
    """A controlled assertion whose code contains no user-supplied data."""

    def __init__(self, code):
        super().__init__(code)
        self.code = code


def require(condition, code):
    if not condition:
        raise AcceptanceFailure(code)


def _safe_result_code(error):
    code = getattr(error, "code", "unexpected_failure")
    if not isinstance(code, str) or not re.fullmatch(r"[a-z][a-z0-9_]{0,79}", code):
        return "unexpected_failure"
    return code


def _regular_kind(metadata):
    mode = getattr(metadata, "st_mode", None)
    require(isinstance(mode, int), "missing_file_type")
    require(not stat.S_ISLNK(mode), "cleanup_link_refused")
    require(
        not getattr(metadata, "st_file_attributes", 0)
        & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400),
        "cleanup_reparse_refused",
    )
    if stat.S_ISDIR(mode):
        return "directory"
    require(stat.S_ISREG(mode), "cleanup_special_file_refused")
    return "file"


def _sha256(handle):
    digest = hashlib.sha256()
    size = 0
    while True:
        block = handle.read(CHUNK_SIZE)
        if not block:
            return size, digest.hexdigest()
        digest.update(block)
        size += len(block)


def _local_inventory(root):
    """Include directory entries, so empty directories are also compared."""
    entries = {}

    def visit(path, relative):
        kind = _regular_kind(path.lstat())
        if kind == "directory":
            if relative:
                entries[relative] = {"kind": kind}
            for child in sorted(path.iterdir(), key=lambda item: item.name):
                visit(child, f"{relative}/{child.name}".lstrip("/"))
        else:
            with path.open("rb") as handle:
                size, digest = _sha256(handle)
            entries[relative] = {"kind": kind, "size": size, "sha256": digest}

    visit(root, "")
    return entries


def _write_binary(path, size):
    block = bytes(range(256)) * (CHUNK_SIZE // 256)
    with path.open("xb") as handle:
        while size:
            chunk = block[: min(size, len(block))]
            handle.write(chunk)
            size -= len(chunk)


class _DisconnectFile:
    def __init__(self, handle, owner):
        self._handle = handle
        self._owner = owner

    def __getattr__(self, name):
        return getattr(self._handle, name)

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self._handle.close()

    def write(self, data):
        result = self._handle.write(data)
        # Ensure the server acknowledged these writes before dropping SSH.
        self._handle.flush()
        self._owner.written += len(data)
        self._owner.interrupt_if_ready()
        return result

    def read(self, size=-1):
        # This check runs before the next read. Previously returned bytes have
        # already been written locally. Source-hash and prefix reads are never
        # interrupted, so a download failure really leaves reusable content.
        if self._owner.direction == "download":
            self._owner.interrupt_if_ready()
        return self._handle.read(size)


class _DisconnectSftp:
    def __init__(self, sftp, owner):
        self._sftp = sftp
        self._owner = owner

    def __getattr__(self, name):
        return getattr(self._sftp, name)

    def open(self, filename, mode="r", *args, **kwargs):
        handle = self._sftp.open(filename, mode, *args, **kwargs)
        return _DisconnectFile(handle, self._owner)

    file = open


class _DisconnectOnceBackend:
    """Preserve the actual connection and file I/O, injecting one disconnect."""

    def __init__(self, backend, direction="upload", threshold=512 * 1024):
        self._backend = backend
        self.direction = direction
        self.threshold = threshold
        self.written = 0
        self.observed_bytes = 0
        self.observe_transfer = lambda: 0
        self.triggered = False
        self.session = None
        self._armed = True
        self._lock = threading.Lock()

    def __getattr__(self, name):
        return getattr(self._backend, name)

    def interrupt_if_ready(self):
        observed = self.written if self.direction == "upload" else self.observe_transfer()
        if observed >= self.threshold and not self.triggered:
            self.observed_bytes = observed
            self.triggered = True
            self.session.close()
            raise OSError(errno.ECONNRESET, "Acceptance connection interruption")

    def connect(self, config, expected_fingerprint=None, **kwargs):
        session = self._backend.connect(config, expected_fingerprint, **kwargs)
        with self._lock:
            if self._armed:
                self._armed = False
                self.session = session
                session.sftp = _DisconnectSftp(session.sftp, self)
        return session


class _CleanupFailOnceBackend:
    """Fail the isolated cleanup connection, without changing server state."""

    def __init__(self, backend):
        self._backend = backend
        self.triggered = False

    def __getattr__(self, name):
        return getattr(self._backend, name)

    def connect(self, config, expected_fingerprint=None, **kwargs):
        if not self.triggered:
            self.triggered = True
            from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError

            raise FileTransferError("connection_failed", "Acceptance cleanup connection interruption", retryable=True)
        return self._backend.connect(config, expected_fingerprint, **kwargs)


class AcceptanceRunner:
    def __init__(self, args, password):
        # Source-tree execution works without importing the QGIS GUI runtime.
        plugin_root = Path(__file__).resolve().parents[3] / "python" / "plugins"
        if plugin_root.is_dir():
            sys.path.insert(0, str(plugin_root))
        from qcopilots_common.security_policy import FilesystemPolicy
        from qcopilots_mcp_server_processing_general.file_transfer_backend import FileTransferBackend
        from qcopilots_mcp_server_processing_general.file_transfer_config import FileTransferConfigStore
        from qcopilots_mcp_server_processing_general.file_transfer_files import (
            normalize_remote,
            remote_join,
            validate_remote_path,
        )
        from qcopilots_mcp_server_processing_general.file_transfer_jobs import FileTransferJobManager
        from qcopilots_mcp_server_processing_general.file_transfer_errors import public_value

        class ObservedManager(FileTransferJobManager):
            """Observe failure evidence without changing transfer behaviour."""

            def __init__(manager, *arguments, **keywords):
                manager.acceptance_diagnostics = []
                super().__init__(*arguments, **keywords)

            def _error(manager, error, secrets=()):
                result = FileTransferJobManager._error(error, secrets)
                try:
                    diagnostic = _exception_evidence(error)
                    runtime = next((item for item in manager._runtimes.values()
                                    if item.thread is not None and item.thread.ident == threading.get_ident()), None)
                    if runtime is not None:
                        current = manager._latest(runtime.record)
                        diagnostic.update(job_id=current["job_id"], transfer_id=current["transfer_id"],
                                          direction=current["direction"], phase=current["phase"],
                                          progress=dict(current.get("progress") or {}))
                        path = diagnostic["progress"].get("current_path")
                        entries = runtime.record.get("files") or []
                        active = [entry for entry in entries if entry.get("phase") in {"creating_stage", "transferring", "ready", "publish_intent"}]
                        if not active:
                            active = [entry for entry in entries if path and path in (entry.get("source"), entry.get("target"), entry.get("stage"))]
                        diagnostic["files"] = []
                        for entry in active[:5]:
                            evidence = {key: entry.get(key) for key in (
                                "source", "target", "stage", "kind", "size", "phase", "committed_offset", "verified", "source_sha256",
                            )}
                            evidence["observed_paths"] = {}
                            for name in ("source", "target", "stage"):
                                path = entry.get(name)
                                if path:
                                    local = (name == "source") == (runtime.record["direction"] == "upload")
                                    evidence["observed_paths"][name] = _path_stat_evidence(
                                        path, local=local,
                                        sftp=runtime.session.sftp if runtime.session is not None else None,
                                    )
                            diagnostic["files"].append(evidence)
                    manager.acceptance_diagnostics.append(public_value(diagnostic, secrets))
                except Exception:
                    # Instrumentation must never replace the product failure.
                    pass
                return result

        self.args = args
        self.password = password
        self.policy = FilesystemPolicy()
        self.normalize_remote = normalize_remote
        self.remote_join = remote_join
        self.validate_remote_path = validate_remote_path
        self.manager_class = ObservedManager
        self.run_id = uuid.uuid4().hex
        self.run_name = f"file-transfer-live-{self.run_id}"
        self.marker = f"QCopilots file transfer acceptance {self.run_id}\n".encode("ascii")
        self.local_base = self.policy.resolve_path(args.workspace_base, access="write")
        self.local_root = self.local_base / self.run_name
        self.corpus_source = (
            self.policy.resolve_path(args.corpus_root, access="read")
            if args.corpus_root is not None else None
        )
        if self.corpus_source is not None:
            require(not self.local_base.is_relative_to(self.corpus_source), "workspace_inside_persistent_corpus")
            require(not self.corpus_source.is_relative_to(self.local_base), "corpus_inside_temporary_workspace")
            if args.report is not None:
                report_path = self.policy.resolve_path(args.report, access="write")
                require(not report_path.is_relative_to(self.corpus_source), "report_inside_persistent_corpus")
        self.remote_base = normalize_remote(args.remote_base)
        self.remote_root = remote_join(self.remote_base, self.run_name)
        # Both roots are registered before either mkdir or network write.
        self.local_created = False
        self.remote_created = False
        self.remote_mkdir_uncertain = False
        self.local_marker_complete = False
        self.remote_marker_complete = False
        self.remote_finalizing = False
        self.local_finalizing = False
        self.managers = []
        self.jobs = []
        self.store = FileTransferConfigStore(root=self.local_root / "configuration")
        self.backend = FileTransferBackend(self.store, policy=self.policy)
        self.manager = None
        self.current_scenario = "setup"
        self.current_details = {}
        self.results = []
        self.started_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        self.cleanup_record = self.local_base / f".qcopilots-file-transfer-cleanup-{self.run_id}.json"
        self.cleanup_retry_prepared = False
        self.corpus_inventory = None

    def report(self, name, result, *, duration=None, details=None):
        print(f"{name}: {result}", flush=True)
        record = {"scenario": name, "result": result}
        if duration is not None:
            record["duration_seconds"] = round(duration, 3)
        if details:
            self.assert_redacted(details)
            record["details"] = details
        self.results.append(record)

    def assert_redacted(self, value, *additional_secrets):
        serialized = json.dumps(value, ensure_ascii=False, default=lambda _value: None)
        for secret in (self.password, *additional_secrets):
            # A one-character password can legitimately occur in UUIDs,
            # statuses or paths. Check password fields structurally in that
            # case instead of declaring every machine identifier a leak.
            if secret and len(secret) >= 4:
                require(secret not in serialized, "credential_disclosed")
        def check_fields(item):
            if isinstance(item, dict):
                for key, child in item.items():
                    if str(key).casefold() in {"password", "passwd", "secret"}:
                        require(child in (None, "", "***", "[REDACTED]"), "credential_disclosed")
                    check_fields(child)
            elif isinstance(item, list):
                for child in item:
                    check_fields(child)
        check_fields(value)

    def new_manager(self, backend=None):
        manager = self.manager_class(
            config_store=self.store,
            backend=backend or self.backend,
            policy=self.policy,
        )
        self.managers.append(manager)
        return manager

    def connect(self):
        return self.backend.connect(self.store.load(require_connection=True))

    def _setup_local(self, *, recover=False):
        require(self.local_base.is_dir(), "workspace_base_missing")
        require(_regular_kind(self.local_base.lstat()) == "directory", "workspace_base_invalid")
        require(self.local_root.parent == self.local_base, "local_root_outside_base")
        if recover and self.local_root.exists():
            require(_regular_kind(self.local_root.lstat()) == "directory", "cleanup_local_root_invalid")
            marker = self.local_root / MARKER_NAME
            if marker.exists():
                require(_regular_kind(marker.lstat()) == "file" and marker.read_bytes() == self.marker, "cleanup_local_marker_mismatch")
                self.local_created = True
                self.local_marker_complete = True
            else:
                require(self.local_finalizing and not any(self.local_root.iterdir()), "cleanup_local_marker_missing")
                self.local_created = True
                with marker.open("xb") as handle:
                    handle.write(self.marker)
                self.local_marker_complete = True
        else:
            self.local_root.mkdir()
            self.local_created = True
            with (self.local_root / MARKER_NAME).open("xb") as handle:
                handle.write(self.marker)
            self.local_marker_complete = True
        self.local_created = True
        self.local_finalizing = False
        self.store.update(
            {
                "host": self.args.host,
                "port": self.args.port,
                "username": self.args.username,
                "password": self.password,
                "known_hosts_file": str(Path(self.args.known_hosts).expanduser()),
                "connect_timeout": 15,
                "io_timeout": 30,
            }
        )
        self.assert_redacted(self.store.public())

    def setup(self):
        self._setup_local()
        config = self.store.load(require_connection=True)
        try:
            result = self.backend.test_connection(config)
        except Exception as error:
            if getattr(error, "code", None) != "host_key_unknown":
                raise
            require(bool(self.args.host_key_fingerprint), "host_key_confirmation_required")
            require(error.details.get("fingerprint") == self.args.host_key_fingerprint, "host_key_fingerprint_mismatch")
            self.backend.trust_host(
                config,
                self.args.host_key_fingerprint,
                error.details["confirmation_token"],
            )
            result = self.backend.test_connection(config)
        self.assert_redacted(result)
        with self.connect() as session:
            sftp = session.sftp
            metadata = self.validate_remote_path(sftp, self.remote_base)
            require(_regular_kind(metadata) == "directory", "remote_base_invalid")
            require(self.validate_remote_path(sftp, self.remote_root, allow_missing=True) is None, "remote_root_exists")
            self.remote_mkdir_uncertain = True
            sftp.mkdir(self.remote_root)
            self.remote_created = True
            self.remote_mkdir_uncertain = False
            with sftp.open(self.remote_join(self.remote_root, MARKER_NAME), "wx") as handle:
                handle.write(self.marker)
            self._verify_remote_owner(sftp)
            self.remote_marker_complete = True
        self.manager = self.new_manager()
        if self.args.scenario_group != "corpus":
            self._create_data()
        if self.corpus_source is not None and self.args.scenario_group != "baseline":
            require(self.corpus_source.is_dir(), "persistent_corpus_missing")
            self.corpus_inventory = _local_inventory(self.corpus_source)

    def _create_data(self):
        self.source = self.local_root / "sources"
        self.source.mkdir()
        self.tree = self.source / "中文 目录"
        (self.tree / "nested space" / "空目录").mkdir(parents=True)
        (self.tree / "empty").mkdir()
        (self.tree / "zero.txt").write_bytes(b"")
        (self.tree / "nested space" / "内容.txt").write_text("完整往返验证\n", encoding="utf-8")
        _write_binary(self.tree / "payload.bin", 2 * 1024 * 1024 + 137)
        self.loose = self.source / "standalone 文件.txt"
        self.loose.write_bytes(b"independent source\r\n")
        self.large = self.local_root / "large.bin"
        _write_binary(self.large, self.args.large_mib * 1024 * 1024)
        self.published = self.local_root / "001-published.txt"
        self.published.write_bytes(b"Previously published content survives discard.\n")
        with self.published.open("rb") as handle:
            size, digest = _sha256(handle)
        self.published_entry = {"kind": "file", "size": size, "sha256": digest}
        self.expected = _local_inventory(self.source)
        with self.large.open("rb") as handle:
            size, digest = _sha256(handle)
        self.large_entry = {"kind": "file", "size": size, "sha256": digest}
        self.remote_lifecycle = self.remote_join(self.remote_root, "lifecycle-source")
        self.successful(self.start("upload", [self.large, self.published], self.remote_lifecycle))
        self.remote_large = self.remote_join(self.remote_lifecycle, self.large.name)
        self.remote_published = self.remote_join(self.remote_lifecycle, self.published.name)

    def _remote_inventory(self, sftp, root):
        entries = {}

        def visit(path, relative):
            metadata = self.validate_remote_path(sftp, path)
            kind = _regular_kind(metadata)
            if kind == "directory":
                if relative:
                    entries[relative] = {"kind": kind}
                for name in sorted(sftp.listdir(path)):
                    child = self.remote_join(path, name)
                    visit(child, f"{relative}/{name}".lstrip("/"))
            else:
                with sftp.open(path, "rb") as handle:
                    size, digest = _sha256(handle)
                entries[relative] = {"kind": kind, "size": size, "sha256": digest}

        visit(root, "")
        return entries

    def _remember(self, manager, snapshot):
        self.assert_redacted(snapshot)
        self.jobs.append((manager, snapshot["job_id"], snapshot["transfer_id"]))
        return snapshot

    def start(self, direction, sources, destination, *, overwrite=False, manager=None):
        manager = manager or self.manager
        return self._remember(
            manager,
            manager.start(direction, sources=[str(path) for path in sources], destination=str(destination), overwrite=overwrite),
        )

    def wait(self, snapshot, *, manager=None, predicate=None):
        manager = manager or self.manager
        deadline = time.monotonic() + self.args.timeout
        next_observation = time.monotonic() + 30
        while time.monotonic() < deadline:
            current = manager.get_job(snapshot["job_id"], files_limit=1)
            self.assert_redacted(current)
            if predicate is not None and predicate(current):
                return current
            if current.get("state") in TERMINAL_STATES:
                if predicate is not None:
                    self.failure_details(manager, current)
                require(predicate is None, "job_ended_before_observation")
                return current
            if time.monotonic() >= next_observation:
                phase = current.get("phase", "unknown")
                if not isinstance(phase, str) or not re.fullmatch(r"[a-z_]+", phase):
                    phase = "unknown"
                print(
                    f"transfer_progress: phase={phase} bytes_completed={self._bytes(current, 'bytes_completed')} "
                    f"bytes_total={self._bytes(current, 'bytes_total')} files_completed={self._bytes(current, 'files_completed')} "
                    f"verification_bytes_completed={self._bytes(current, 'verification_bytes_completed')} "
                    f"verification_bytes_total={self._bytes(current, 'verification_bytes_total')}",
                    flush=True,
                )
                next_observation = time.monotonic() + 30
            time.sleep(0.1)
        raise AcceptanceFailure("job_timeout")

    @staticmethod
    def _error_code(snapshot):
        errors = snapshot.get("errors") or []
        error = errors[-1] if errors else {}
        return error.get("code") if isinstance(error, dict) else None

    @staticmethod
    def _bytes(snapshot, name):
        for container in (snapshot, snapshot.get("progress"), snapshot.get("result")):
            if isinstance(container, dict) and isinstance(container.get(name), (int, float)):
                return container[name]
        return 0

    def successful(self, snapshot, manager=None):
        manager = manager or self.manager
        final = self.wait(snapshot, manager=manager)
        if final.get("state") != "succeeded":
            self.failure_details(manager, final)
        require(final.get("state") == "succeeded", self._error_code(final) or "transfer_failed")
        return final

    def failure_details(self, manager, snapshot):
        details = {key: snapshot.get(key) for key in ("job_id", "transfer_id", "direction", "state", "phase", "progress", "errors")}
        details["diagnostics"] = [item for item in getattr(manager, "acceptance_diagnostics", [])
                                  if item.get("job_id") == snapshot.get("job_id")][-5:]
        self.assert_redacted(details)
        self.current_details["failed_job"] = details

    def all_files(self, manager, job_id):
        """Read all terminal file results and prove pagination has no writes."""
        before = self.checkpoint_stats(manager)
        started = time.monotonic()
        files = []
        offset = 0
        total = None
        queries = 0
        longest = 0
        while True:
            query_started = time.monotonic()
            page = manager.get_job(job_id, files_offset=offset, files_limit=200)
            longest = max(longest, time.monotonic() - query_started)
            queries += 1
            self.assert_redacted(page)
            require(page.get("files_offset") == offset, "pagination_offset_mismatch")
            require(isinstance(page.get("files_total"), int), "pagination_total_missing")
            if total is None:
                total = page["files_total"]
            require(page["files_total"] == total, "pagination_total_changed")
            entries = page.get("files")
            require(isinstance(entries, list) and len(entries) <= 200, "pagination_page_invalid")
            files.extend(entries)
            following = page.get("next_files_offset")
            if following is None:
                break
            require(following == offset + len(entries) and following > offset, "pagination_did_not_advance")
            offset = following
        require(len(files) == total, "pagination_entries_missing")
        targets = [entry.get("target") or entry.get("destination") for entry in files]
        require(all(isinstance(value, str) for value in targets), "pagination_target_missing")
        require(len(set(targets)) == len(files), "pagination_entries_duplicated")
        require(self.checkpoint_stats(manager) == before, "pagination_wrote_checkpoint")
        return files, {
            "queries": queries,
            "entries": total,
            "duration_seconds": round(time.monotonic() - started, 3),
            "maximum_query_seconds": round(longest, 3),
        }

    @staticmethod
    def checkpoint_stats(manager):
        stats = getattr(getattr(manager, "_disk", None), "write_stats", None)
        require(isinstance(stats, dict), "checkpoint_instrumentation_missing")
        return {name: int(stats[name]) for name in ("write_count", "bytes_written")}

    @staticmethod
    def checkpoint_delta(before, after):
        return {name: after[name] - before[name] for name in before}

    def verify_lifecycle_destination(self, direction, destination):
        if direction == "upload":
            with self.connect() as session:
                actual = self._remote_inventory(session.sftp, destination)
        else:
            actual = _local_inventory(Path(destination))
        require(actual == {self.large.name: self.large_entry}, "resume_content_mismatch")

    def lifecycle_paths(self, direction, name):
        if direction == "upload":
            return [self.large], self.remote_join(self.remote_root, name)
        return [self.remote_large], self.local_root / name

    def roundtrip(self):
        self.remote_roundtrip = self.remote_join(self.remote_root, "roundtrip")
        uploaded = self.successful(self.start("upload", [self.tree, self.loose], self.remote_roundtrip))
        with self.connect() as session:
            require(self._remote_inventory(session.sftp, self.remote_roundtrip) == self.expected, "upload_inventory_mismatch")
        target = self.local_root / "roundtrip-download"
        remote_sources = [self.remote_join(self.remote_roundtrip, path.name) for path in (self.tree, self.loose)]
        downloaded = self.successful(self.start("download", remote_sources, target))
        require(_local_inventory(target) == self.expected, "download_inventory_mismatch")
        serialized_inventory = json.dumps(self.expected, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
        self.current_details.update(
            files=sum(entry["kind"] == "file" for entry in self.expected.values()),
            directories=sum(entry["kind"] == "directory" for entry in self.expected.values()),
            directory_inventory_sha256=hashlib.sha256(serialized_inventory).hexdigest(),
            upload_inventory_matched=True,
            download_inventory_matched=True,
            uploaded_bytes=self._bytes(uploaded, "bytes_transferred"),
            downloaded_bytes=self._bytes(downloaded, "bytes_transferred"),
        )

    def conflicts_and_overwrite(self):
        before = self.expected
        conflict = self.wait(self.start("upload", [self.tree, self.loose], self.remote_roundtrip))
        require(conflict.get("state") == "failed", "upload_conflict_was_not_rejected")
        with self.connect() as session:
            require(self._remote_inventory(session.sftp, self.remote_roundtrip) == before, "conflict_changed_remote")
        self.loose.write_bytes(b"explicit replacement\n")
        changed = _local_inventory(self.source)
        replaced = self.wait(self.start("upload", [self.loose], self.remote_roundtrip, overwrite=True))
        with self.connect() as session:
            actual = self._remote_inventory(session.sftp, self.remote_roundtrip)
        if replaced.get("state") == "succeeded":
            require(actual == changed, "remote_overwrite_mismatch")
            self.current_details["upload_atomic_overwrite"] = "supported"
        else:
            require(self._error_code(replaced) == "overwrite_unsupported", "unexpected_overwrite_failure")
            require(actual == before, "unsupported_overwrite_changed_remote")
            self.current_details["upload_atomic_overwrite"] = "unsupported_original_preserved"
        remote_loose = self.remote_join(self.remote_roundtrip, self.loose.name)
        destination = self.local_root / "download-overwrite"
        destination.mkdir()
        old = destination / self.loose.name
        old.write_bytes(b"local original\n")
        conflict = self.wait(self.start("download", [remote_loose], destination))
        require(conflict.get("state") == "failed", "download_conflict_was_not_rejected")
        require(old.read_bytes() == b"local original\n", "conflict_changed_local")
        self.successful(self.start("download", [remote_loose], destination, overwrite=True))
        require(_local_inventory(destination)[self.loose.name] == actual[self.loose.name], "local_overwrite_mismatch")
        self.current_details.update(default_upload_conflict_preserved=True, default_download_conflict_preserved=True, download_atomic_overwrite="supported")

    def authentication_and_redaction(self):
        wrong = f"invalid-{uuid.uuid4().hex}"
        self.store.update({"password": wrong})
        try:
            try:
                self.backend.test_connection(self.store.load(require_connection=True))
            except Exception as error:
                require(getattr(error, "code", None) == "authentication_failed", "unexpected_authentication_result")
                from qcopilots_mcp_server_processing_general.file_transfer_errors import public_error

                self.assert_redacted(public_error(error, (self.password, wrong)), wrong)
            else:
                raise AcceptanceFailure("incorrect_password_accepted")
        finally:
            self.store.update({"password": self.password})

    def cancel(self, direction):
        self.store.update({"max_bytes_per_second": 1024 * 1024})
        sources, destination = self.lifecycle_paths(direction, f"{direction}-cancel")
        try:
            started = self.start(direction, sources, destination)
            observed = self.wait(started, predicate=lambda job: self._bytes(job, "bytes_transferred") > 0)
            cancel_started = time.monotonic()
            self.assert_redacted(self.manager.cancel_job(started["job_id"]))
            cancelled = self.wait(started)
            cancel_seconds = time.monotonic() - cancel_started
            require(cancelled.get("state") == "cancelled", "cancel_failed")
            require(cancelled.get("resumable"), "cancel_not_resumable")
            require(cancelled.get("cleanup_pending") is False, "cancel_unexpected_cleanup")
            require(cancel_seconds < 2, "cancel_exceeded_two_seconds")
            self.store.update({"max_bytes_per_second": 0})
            resumed = self.resume_attempt(self.manager, started)
            final = self.successful(resumed)
            require(self._bytes(final, "bytes_reused") > 0, "cancel_resume_did_not_reuse_bytes")
            self.verify_lifecycle_destination(direction, destination)
            self.current_details.update(
                direction=direction,
                observed_bytes_before_cancel=self._bytes(observed, "bytes_transferred"),
                cancellation_seconds=round(cancel_seconds, 3),
                resumed_bytes_reused=self._bytes(final, "bytes_reused"),
                resumed_bytes_transferred=self._bytes(final, "bytes_transferred"),
                file_sha256=self.large_entry["sha256"],
                final_sha256_matched=True,
            )
        finally:
            self.store.update({"max_bytes_per_second": 0})

    def resume_attempt(self, manager, started):
        resumed = self._remember(manager, manager.resume_job(started["job_id"]))
        require(resumed["job_id"] != started["job_id"], "resume_reused_job_id")
        require(resumed["transfer_id"] == started["transfer_id"], "resume_changed_transfer_id")
        require(resumed.get("resumes_job_id") == started["job_id"], "resume_predecessor_missing")
        return resumed

    def resume(self, direction, *, restart=False):
        injected = _DisconnectOnceBackend(self.backend, direction)
        manager = self.new_manager(backend=injected)
        name = f"{direction}-" + ("restart-resume" if restart else "disconnect-resume")
        sources, destination = self.lifecycle_paths(direction, name)
        started = self.start(direction, sources, destination, manager=manager)
        def observed_transfer():
            current = manager.get_job(started["job_id"], files_limit=1)
            return self._bytes(current, "bytes_transferred") if current.get("phase") == "transferring" else 0
        injected.observe_transfer = observed_transfer
        interrupted = self.wait(started, manager=manager)
        require(injected.triggered and injected.observed_bytes > 0, "disconnect_not_triggered")
        require(interrupted.get("state") == "failed", "disconnect_not_failed")
        require(interrupted.get("resumable"), "disconnect_not_resumable")
        if restart:
            manager.shutdown()
            manager = self.new_manager()
            jobs = manager.list_jobs(limit=200)
            self.assert_redacted(jobs)
            listed = jobs.get("jobs", []) if isinstance(jobs, dict) else jobs
            require(any(job.get("job_id") == started["job_id"] for job in listed), "restart_lost_interrupted_job")
        # The injected backend wraps only its first connection. A retry always
        # uses the real, undecorated backend even in the same-manager scenario.
        manager.backend = self.backend
        resumed = self.resume_attempt(manager, started)
        final = self.successful(resumed, manager=manager)
        require(self._bytes(final, "bytes_reused") > 0, "resume_did_not_reuse_bytes")
        self.verify_lifecycle_destination(direction, destination)
        self.current_details.update(
            direction=direction,
            injected_after_destination_bytes=injected.observed_bytes,
            resumed_bytes_reused=self._bytes(final, "bytes_reused"),
            resumed_bytes_transferred=self._bytes(final, "bytes_transferred"),
            file_bytes=self.large_entry["size"],
            file_sha256=self.large_entry["sha256"],
            final_sha256_matched=True,
            recovered_by_new_manager=restart,
        )

    def discard(self, direction, *, retry=False):
        """Assert product cleanup before the runner removes its owned roots."""
        self.store.update({"max_bytes_per_second": 1024 * 1024})
        sources, destination = self.lifecycle_paths(direction, f"{direction}-discard-{'retry' if retry else 'once'}")
        sources.insert(0, self.published if direction == "upload" else self.remote_published)
        original_backend = self.manager.backend
        try:
            started = self.start(direction, sources, destination)
            self.wait(started, predicate=lambda job: self._bytes(job, "bytes_transferred") > self.published_entry["size"])
            self.manager.cancel_job(started["job_id"])
            stopped = self.wait(started)
            require(stopped.get("state") == "cancelled" and stopped.get("resumable"), "discard_cancel_not_resumable")
            if retry:
                require(direction == "upload", "cleanup_retry_direction_invalid")
                injected = _CleanupFailOnceBackend(self.backend)
                self.manager.backend = injected
                first = self.manager.discard_transfer(started["transfer_id"])
                self.assert_redacted(first)
                require(injected.triggered, "cleanup_failure_not_triggered")
                require(first.get("cleanup_pending") is True, "cleanup_failure_not_pending")
                pending = self.manager.get_job(started["job_id"])
                require(pending.get("cleanup_pending") is True and not pending.get("resumable"), "discard_pending_state_invalid")
                self.manager.backend = original_backend
            result = self.manager.discard_transfer(started["transfer_id"])
            self.assert_redacted(result)
            require(result.get("discarded") is True, "discard_not_recorded")
            require(result.get("cleanup_pending") is False, "discard_cleanup_pending")
            require((result.get("cleanup") or {}).get("complete") is True, "discard_cleanup_incomplete")
            require(not (result.get("cleanup") or {}).get("residual_paths"), "discard_cleanup_residuals")
            repeated = self.manager.discard_transfer(started["transfer_id"])
            require(repeated == result, "discard_not_idempotent")
            try:
                self.manager.resume_job(started["job_id"])
            except Exception as error:
                require(getattr(error, "code", None) in {"not_resumable", "transfer_not_resumable", "checkpoint_not_found", "transfer_not_found"}, "unexpected_discard_resume_error")
            else:
                raise AcceptanceFailure("discarded_transfer_resumed")
            if direction == "upload":
                with self.connect() as session:
                    attrs = self.validate_remote_path(session.sftp, destination, allow_missing=True)
                    require(attrs is not None, "discard_removed_published_directory")
                    inventory = self._remote_inventory(session.sftp, destination)
            else:
                target = Path(destination)
                require(target.exists(), "discard_removed_published_directory")
                inventory = _local_inventory(target)
            require(inventory == {self.published.name: self.published_entry}, "discard_changed_published_or_left_partial")
            self.current_details.update(direction=direction, failure_injected=retry, cleanup_complete=True, repeat_idempotent=True, resume_refused=True, published_sha256_preserved=True)
        finally:
            self.manager.backend = original_backend
            self.store.update({"max_bytes_per_second": 0})

    def corpus_roundtrip(self):
        require(self.corpus_source is not None and self.corpus_source.is_dir(), "persistent_corpus_missing")
        expected = self.corpus_inventory
        require(isinstance(expected, dict), "persistent_corpus_inventory_missing")
        regular = [entry for entry in expected.values() if entry["kind"] == "file"]
        require(any(entry["size"] > 4 * 1024**3 for entry in regular), "corpus_large_file_missing")
        require(sum(name.startswith("small_files/") and entry["kind"] == "file" and entry["size"] == 24576 for name, entry in expected.items()) == 4096, "corpus_small_files_missing")
        remote_destination = self.remote_root
        for component in ("corpus", "new", "parents"):
            remote_destination = self.remote_join(remote_destination, component)
        before = self.checkpoint_stats(self.manager)
        upload_started = time.monotonic()
        uploaded = self.successful(self.start("upload", [self.corpus_source], remote_destination))
        upload_seconds = time.monotonic() - upload_started
        upload_checkpoint = self.checkpoint_delta(before, self.checkpoint_stats(self.manager))
        upload_files, upload_queries = self.all_files(self.manager, uploaded["job_id"])
        remote_corpus = self.remote_join(remote_destination, self.corpus_source.name)
        verify_started = time.monotonic()
        with self.connect() as session:
            actual = self._remote_inventory(session.sftp, remote_corpus)
        require(actual == expected, "corpus_upload_inventory_mismatch")
        remote_verify_seconds = time.monotonic() - verify_started
        target = self.local_root / "corpus-download/new/parents"
        before = self.checkpoint_stats(self.manager)
        download_started = time.monotonic()
        downloaded = self.successful(self.start("download", [remote_corpus], target))
        download_seconds = time.monotonic() - download_started
        download_checkpoint = self.checkpoint_delta(before, self.checkpoint_stats(self.manager))
        download_files, download_queries = self.all_files(self.manager, downloaded["job_id"])
        require(_local_inventory(target / self.corpus_source.name) == expected, "corpus_download_inventory_mismatch")
        require(_local_inventory(self.corpus_source) == expected, "persistent_corpus_changed")
        require(len(upload_files) == len(download_files) == len(expected) + 1, "corpus_manifest_entries_mismatch")
        encoded = json.dumps(expected, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
        self.current_details.update(
            files=len(regular), total_bytes=sum(entry["size"] for entry in regular),
            largest_file_bytes=max(entry["size"] for entry in regular),
            directory_inventory_sha256=hashlib.sha256(encoded).hexdigest(),
            upload_sha256_matched=True, download_sha256_matched=True, persistent_source_unchanged=True,
            upload_seconds=round(upload_seconds, 3), download_seconds=round(download_seconds, 3),
            remote_reverification_seconds=round(remote_verify_seconds, 3),
            upload_checkpoint=upload_checkpoint, download_checkpoint=download_checkpoint,
            upload_pagination=upload_queries, download_pagination=download_queries,
        )

    def small_file_scaling(self):
        require(self.corpus_source is not None, "persistent_corpus_missing")
        source = self.corpus_source / "small_files"
        items = sorted(path for path in source.rglob("*") if path.is_file())
        require(len(items) == 4096, "corpus_small_files_missing")
        measurements = []
        for count in (512, 4096):
            staged = self.local_root / f"small-benchmark-{count}"
            staged.mkdir()
            for index, item in enumerate(items[:count]):
                require(_regular_kind(item.lstat()) == "file" and item.stat().st_size == 24576, "small_fixture_invalid")
                shutil.copyfile(item, staged / f"sample-{index:04d}{item.suffix}")
            before = self.checkpoint_stats(self.manager)
            started = time.monotonic()
            final = self.successful(self.start("upload", [staged], self.remote_join(self.remote_root, f"small-scaling-{count}")))
            elapsed = time.monotonic() - started
            checkpoint = self.checkpoint_delta(before, self.checkpoint_stats(self.manager))
            entries, pagination = self.all_files(self.manager, final["job_id"])
            require(len(entries) == count + 1, "small_file_manifest_count_mismatch")
            require(self._bytes(final, "files_completed") == count, "small_file_completed_count_mismatch")
            measurements.append({"files": count, "duration_seconds": round(elapsed, 3), "checkpoint": checkpoint, "pagination": pagination})
        small, large = measurements
        ratio = large["checkpoint"]["bytes_written"] / max(1, small["checkpoint"]["bytes_written"])
        require(ratio < 10, "checkpoint_writes_not_linear")
        self.current_details.update(measurements=measurements, file_count_ratio=8, checkpoint_bytes_ratio=round(ratio, 3))

    def _verify_remote_owner(self, sftp):
        require(self.remote_root == self.remote_join(self.remote_base, self.run_name), "cleanup_remote_root_mismatch")
        require(re.fullmatch(r"file-transfer-live-[0-9a-f]{32}", self.run_name) is not None, "cleanup_invalid_run_name")
        require(_regular_kind(self.validate_remote_path(sftp, self.remote_root)) == "directory", "cleanup_remote_root_invalid")
        marker = self.remote_join(self.remote_root, MARKER_NAME)
        require(_regular_kind(self.validate_remote_path(sftp, marker)) == "file", "cleanup_remote_marker_invalid")
        with sftp.open(marker, "rb") as handle:
            require(handle.read(len(self.marker) + 1) == self.marker, "cleanup_remote_marker_mismatch")

    def _remove_remote_owned(self):
        with self.connect() as session:
            sftp = session.sftp
            metadata = self.validate_remote_path(sftp, self.remote_root, allow_missing=True)
            if metadata is None:
                self.remote_created = False
                self.remote_mkdir_uncertain = False
                return
            require(not self.remote_mkdir_uncertain, "remote_ownership_unconfirmed")
            if self.remote_finalizing and not sftp.listdir(self.remote_root):
                # The marker was removed only after a complete owned cleanup.
                # An interrupted final rmdir may be retried as an empty-root
                # removal, without reopening permission to recursively delete.
                require(_regular_kind(metadata) == "directory", "cleanup_remote_root_invalid")
                sftp.rmdir(self.remote_root)
                require(self.validate_remote_path(sftp, self.remote_root, allow_missing=True) is None, "remote_cleanup_incomplete")
                self.remote_created = False
                return
            if not self.remote_marker_complete:
                # Setup owns this successfully created exclusive UUID root.
                # Before the marker completes, only an empty root or our exact
                # partial marker can exist. Never recurse in this state.
                require(_regular_kind(metadata) == "directory", "cleanup_remote_root_invalid")
                names = sftp.listdir(self.remote_root)
                require(set(names) <= {MARKER_NAME}, "cleanup_unmarked_remote_not_empty")
                if MARKER_NAME in names:
                    marker = self.remote_join(self.remote_root, MARKER_NAME)
                    require(_regular_kind(self.validate_remote_path(sftp, marker)) == "file", "cleanup_remote_marker_invalid")
                    with sftp.open(marker, "rb") as handle:
                        content = handle.read(len(self.marker) + 1)
                    require(self.marker.startswith(content), "cleanup_partial_remote_marker_mismatch")
                    sftp.remove(marker)
                sftp.rmdir(self.remote_root)
                require(self.validate_remote_path(sftp, self.remote_root, allow_missing=True) is None, "remote_cleanup_incomplete")
                self.remote_created = False
                return
            self._verify_remote_owner(sftp)

            def remove(path):
                # Revalidate ancestors and the item immediately before every
                # deletion. Remote directory names never become shell input.
                kind = _regular_kind(self.validate_remote_path(sftp, path))
                if kind == "directory":
                    for name in sftp.listdir(path):
                        remove(self.remote_join(path, name))
                    self.validate_remote_path(sftp, path)
                    sftp.rmdir(path)
                else:
                    sftp.remove(path)

            for name in sftp.listdir(self.remote_root):
                if name != MARKER_NAME:
                    remove(self.remote_join(self.remote_root, name))
            self._verify_remote_owner(sftp)
            self.remote_finalizing = True
            sftp.remove(self.remote_join(self.remote_root, MARKER_NAME))
            sftp.rmdir(self.remote_root)
            require(self.validate_remote_path(sftp, self.remote_root, allow_missing=True) is None, "remote_cleanup_incomplete")
        self.remote_created = False

    def _remove_local_owned(self):
        root = self.policy.resolve_path(self.local_root, access="write")
        require(root == self.local_root and root.parent == self.local_base, "cleanup_local_root_mismatch")
        require(_regular_kind(root.lstat()) == "directory", "cleanup_local_root_invalid")
        if self.local_finalizing and not any(root.iterdir()):
            root.rmdir()
            self.local_created = False
            return
        marker = root / MARKER_NAME
        if not self.local_marker_complete:
            children = list(root.iterdir())
            require({child.name for child in children} <= {MARKER_NAME}, "cleanup_unmarked_local_not_empty")
            if children:
                require(_regular_kind(marker.lstat()) == "file", "cleanup_local_marker_invalid")
                require(self.marker.startswith(marker.read_bytes()), "cleanup_partial_local_marker_mismatch")
                marker.unlink()
            root.rmdir()
            require(not root.exists(), "local_cleanup_incomplete")
            self.local_created = False
            return
        require(_regular_kind(marker.lstat()) == "file", "cleanup_local_marker_invalid")
        require(marker.read_bytes() == self.marker, "cleanup_local_marker_mismatch")

        def remove(path):
            resolved = self.policy.resolve_path(path, access="write")
            require(resolved.is_relative_to(root) and resolved != root, "cleanup_local_outside_root")
            if _regular_kind(path.lstat()) == "directory":
                for child in path.iterdir():
                    remove(child)
                path.rmdir()
            else:
                path.unlink()

        for child in root.iterdir():
            if child.name != MARKER_NAME:
                remove(child)
        require(marker.read_bytes() == self.marker, "cleanup_local_marker_mismatch")
        self.local_finalizing = True
        marker.unlink()
        root.rmdir()
        require(not root.exists(), "local_cleanup_incomplete")
        self.local_created = False

    def cleanup(self):
        failures = []
        if self.args.retry_cleanup is not None and not self.cleanup_retry_prepared:
            # Invalid recovery metadata never authorizes remote removal or
            # deletion of the supplied record. Remove only a verified local
            # root created/adopted while preparing this retry.
            if self.local_created:
                self._remove_local_owned()
            return
        # Stop every worker before touching its files. Each manager owns only
        # this invocation's temporary configuration and transfer root.
        for manager in reversed(self.managers):
            try:
                require(manager.shutdown(timeout=30) is not False, "worker_shutdown_incomplete")
                require(
                    not manager.list_jobs(states=["queued", "running", "cancelling"], limit=200)["jobs"],
                    "worker_shutdown_incomplete",
                )
            except Exception:
                failures.append("worker_shutdown_failed")
        # A still-active worker can recreate files after removal. Leave the
        # owned roots intact and fail instead of racing a recursive deletion.
        require(not failures, "worker_shutdown_failed")
        if self.remote_created or self.remote_mkdir_uncertain:
            # Retry bounded connection failures before declaring incomplete
            # cleanup. Unsafe type/ownership errors never trigger a retry.
            for attempt in range(3):
                try:
                    self._remove_remote_owned()
                    break
                except Exception as error:
                    code = _safe_result_code(error)
                    retryable = code in {"connection_failed", "connection_timeout"} or isinstance(error, (OSError, EOFError))
                    if not retryable or attempt == 2:
                        failures.append(code)
                        break
                    time.sleep(attempt + 1)
        # Retain only enough non-secret evidence to retry remote cleanup. The
        # local configuration contains the test password and must not survive
        # an unreachable server together with gigabytes of temporary copies.
        if self.local_created:
            for attempt in range(3):
                try:
                    self._remove_local_owned()
                    break
                except OSError as error:
                    if attempt == 2:
                        failures.append(_safe_result_code(error))
                    else:
                        time.sleep(attempt + 1)
                except Exception as error:
                    failures.append(_safe_result_code(error))
                    break
        if failures:
            self._save_cleanup_record()
        else:
            self.cleanup_record.unlink(missing_ok=True)
        require(not failures, "cleanup_failed")

    def _save_cleanup_record(self):
        payload = {
            "schema_version": 1, "run_id": self.run_id,
            "host": self.args.host, "port": self.args.port, "username": self.args.username,
            "remote_base": self.remote_base,
            "remote_created": self.remote_created,
            "remote_marker_complete": self.remote_marker_complete,
            "remote_finalizing": self.remote_finalizing,
            "remote_mkdir_uncertain": self.remote_mkdir_uncertain,
            "local_created": self.local_created,
            "local_marker_complete": self.local_marker_complete,
            "local_finalizing": self.local_finalizing,
        }
        self.assert_redacted(payload)
        _atomic_json(self.cleanup_record, payload)

    def prepare_cleanup_retry(self):
        """Validate the residue record before using any of its identifiers."""
        record_path = self.policy.resolve_path(self.args.retry_cleanup, access="read")
        require(record_path.parent == self.local_base, "cleanup_record_outside_workspace")
        require(_regular_kind(record_path.lstat()) == "file", "cleanup_record_invalid")
        record = json.loads(record_path.read_text(encoding="utf-8"))
        require(isinstance(record, dict) and record.get("schema_version") == 1, "cleanup_record_invalid")
        run_id = record.get("run_id")
        require(isinstance(run_id, str) and re.fullmatch(r"[0-9a-f]{32}", run_id) is not None, "cleanup_record_invalid")
        require(record_path.name == f".qcopilots-file-transfer-cleanup-{run_id}.json", "cleanup_record_name_mismatch")
        require((record.get("host"), record.get("port"), record.get("username"), record.get("remote_base")) == (self.args.host, self.args.port, self.args.username, self.remote_base), "cleanup_endpoint_mismatch")
        flags = ("remote_created", "remote_marker_complete", "remote_finalizing", "remote_mkdir_uncertain", "local_created", "local_marker_complete", "local_finalizing")
        require(all(type(record.get(name)) is bool for name in flags), "cleanup_record_invalid")
        self.run_id = run_id
        self.run_name = f"file-transfer-live-{run_id}"
        self.marker = f"QCopilots file transfer acceptance {run_id}\n".encode("ascii")
        self.local_root = self.local_base / self.run_name
        self.remote_root = self.remote_join(self.remote_base, self.run_name)
        self.cleanup_record = record_path
        for name in flags:
            if name != "local_created":
                setattr(self, name, record[name])
        self.store.root = self.local_root / "configuration"
        self.store.path = self.store.root / self.store.path.name
        self._setup_local(recover=record["local_created"])
        self.cleanup_retry_prepared = True
        if not (self.remote_created or self.remote_mkdir_uncertain):
            return
        # Reuse an existing trusted entry or the explicitly supplied fingerprint
        # under the new temporary configuration, never blindly trust the record.
        config = self.store.load(require_connection=True)
        try:
            self.backend.test_connection(config)
        except Exception as error:
            if getattr(error, "code", None) != "host_key_unknown":
                raise
            require(bool(self.args.host_key_fingerprint) and error.details.get("fingerprint") == self.args.host_key_fingerprint, "host_key_confirmation_required")
            self.backend.trust_host(config, self.args.host_key_fingerprint, error.details["confirmation_token"])

    def run(self):
        successful = False
        scenario_started = time.monotonic()
        try:
            scenarios = [("setup", self.setup)]
            if self.args.scenario_group != "corpus":
                scenarios.extend([
                    ("mixed_upload_download_sha256", self.roundtrip),
                    ("conflict_and_atomic_overwrite", self.conflicts_and_overwrite),
                    ("authentication_and_redaction", self.authentication_and_redaction),
                ])
                for direction in ("upload", "download"):
                    scenarios.extend([
                        (f"{direction}_cancel_resume_reused_bytes", lambda direction=direction: self.cancel(direction)),
                        (f"{direction}_disconnect_resume_reused_bytes", lambda direction=direction: self.resume(direction)),
                        (f"{direction}_manager_restart_resume_reused_bytes", lambda direction=direction: self.resume(direction, restart=True)),
                        (f"{direction}_discard_preserves_published", lambda direction=direction: self.discard(direction)),
                    ])
                scenarios.append(("upload_discard_cleanup_retry", lambda: self.discard("upload", retry=True)))
            if self.corpus_source is not None and self.args.scenario_group != "baseline":
                scenarios.extend([("small_file_checkpoint_scaling", self.small_file_scaling), ("persistent_corpus_roundtrip_sha256", self.corpus_roundtrip)])
            if self.args.retry_cleanup is not None:
                scenarios = [("prepare_owned_cleanup_retry", self.prepare_cleanup_retry)]
            for name, callback in scenarios:
                self.current_scenario = name
                self.current_details = {}
                scenario_started = time.monotonic()
                callback()
                if self.manager is not None:
                    for record in self.manager.root.rglob("*.json"):
                        self.assert_redacted(json.loads(record.read_text(encoding="utf-8")))
                    for record in self.manager.root.rglob("*.jsonl"):
                        with record.open(encoding="utf-8") as handle:
                            for line in handle:
                                self.assert_redacted(json.loads(line))
                result = "PASS"
                if "upload_atomic_overwrite" in self.current_details:
                    result += f" (upload_atomic_overwrite_{self.current_details['upload_atomic_overwrite']})"
                self.report(name, result, duration=time.monotonic() - scenario_started, details=self.current_details)
            successful = True
        except Exception as error:
            self.report(self.current_scenario, f"FAIL ({_safe_result_code(error)})", duration=time.monotonic() - scenario_started, details=self.current_details)
        finally:
            if self.corpus_inventory is not None:
                verification_started = time.monotonic()
                try:
                    require(_local_inventory(self.corpus_source) == self.corpus_inventory, "persistent_corpus_changed")
                    self.report("persistent_corpus_preservation", "PASS", duration=time.monotonic() - verification_started)
                except Exception as error:
                    self.report("persistent_corpus_preservation", f"FAIL ({_safe_result_code(error)})", duration=time.monotonic() - verification_started)
                    successful = False
            cleanup_started = time.monotonic()
            try:
                self.cleanup()
                self.report("owned_resource_cleanup", "PASS", duration=time.monotonic() - cleanup_started)
            except Exception as error:
                self.report("owned_resource_cleanup", f"FAIL ({_safe_result_code(error)} run={self.run_name})", duration=time.monotonic() - cleanup_started)
                successful = False
        if self.args.report is not None:
            try:
                self.save_report(successful)
            except Exception:
                print("acceptance_report: FAIL (report_write_failed)", flush=True)
                successful = False
        return 0 if successful else 1

    def save_report(self, successful):
        destination = self.policy.resolve_path(self.args.report, access="write")
        require(not destination.is_relative_to(self.local_root), "report_inside_temporary_root")
        require(self.corpus_source is None or not destination.is_relative_to(self.corpus_source), "report_inside_persistent_corpus")
        payload = {
            "schema_version": 2,
            "run_id": self.run_id,
            "started_at": self.started_at,
            "finished_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "platform": platform.platform(),
            "scenario_group": self.args.scenario_group,
            "large_fixture_bytes": self.args.large_mib * 1024 * 1024 if self.args.scenario_group != "corpus" else 0,
            "persistent_corpus_requested": self.corpus_source is not None and self.args.scenario_group != "baseline",
            "passed": successful,
            "scenarios": self.results,
        }
        self.assert_redacted(payload)
        _atomic_json(destination, payload)


def _atomic_json(destination, payload):
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", prefix=f".{destination.name}.", suffix=".tmp", dir=destination.parent, delete=False) as handle:
            temporary = Path(handle.name)
            json.dump(payload, handle, ensure_ascii=False, allow_nan=False, indent=2)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def _exception_evidence(error):
    """Return exception metadata and source locations, never frame locals."""
    frames = []
    current = error.__traceback__
    while current is not None:
        code = current.tb_frame.f_code
        frame = {"file": Path(code.co_filename).name, "function": code.co_name, "line": current.tb_lineno}
        # Explicitly allowlist only the operation's open flags. Never serialize
        # arbitrary frame locals, which can contain the connection password.
        if code.co_name in {"open", "_open_target", "_open_source"}:
            mode = current.tb_frame.f_locals.get("mode")
            if code.co_name == "_open_target":
                mode = "wx" if current.tb_frame.f_locals.get("exclusive") else "r+" if current.tb_frame.f_locals.get("resume") else "rb"
            elif code.co_name == "_open_source":
                mode = "rb"
            if mode in {"r", "rb", "r+", "r+b", "w", "wb", "w+", "wx", "x", "xb", "a", "ab"}:
                frame["open_mode"] = mode
        frames.append(frame)
        current = current.tb_next
    return {
        "exception_type": type(error).__name__,
        "errno": getattr(error, "errno", None),
        "winerror": getattr(error, "winerror", None),
        "filename": getattr(error, "filename", None),
        "filename2": getattr(error, "filename2", None),
        "message": str(error),
        "stack": frames[-12:],
    }


def _path_stat_evidence(path, *, local, sftp):
    result = {"location": "local" if local else "remote", "path_characters": len(str(path))}
    if not local and sftp is None:
        return {**result, "observed": False}
    try:
        metadata = Path(path).lstat() if local else sftp.lstat(path)
        return {**result, "observed": True, "size": metadata.st_size, "mode": metadata.st_mode,
                "mtime": metadata.st_mtime, "attributes": getattr(metadata, "st_file_attributes", None)}
    except Exception as error:
        return {**result, "observed": False, "exception_type": type(error).__name__,
                "errno": getattr(error, "errno", None), "winerror": getattr(error, "winerror", None),
                "message": str(error)}


def build_argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=22)
    parser.add_argument("--username", required=True)
    parser.add_argument("--remote-base", required=True, help="Existing remote base directory. Its pre-existing contents are preserved.")
    parser.add_argument("--workspace-base", type=Path, required=True, help="Existing temporary workspace, separate from the persistent corpus.")
    parser.add_argument("--known-hosts", default="~/.ssh/known_hosts")
    parser.add_argument("--host-key-fingerprint", help="Independently confirmed SHA256 host fingerprint, required only for an unknown host.")
    parser.add_argument("--timeout", type=float, default=300, help="Maximum seconds per asynchronous operation.")
    parser.add_argument("--large-mib", type=int, default=256, help="Deterministic cancellation/resume fixture size, at least 8 MiB.")
    parser.add_argument("--corpus-root", type=Path, help="Read-only persistent corpus with a >4 GiB file and small_files containing 4096 24-KiB files.")
    parser.add_argument("--scenario-group", choices=("baseline", "corpus", "all"), default="all", help="Run lifecycle baseline, full persistent corpus including small-file scaling, or both. Use a separate --report for each invocation.")
    parser.add_argument("--retry-cleanup", type=Path, help="Retry only an earlier owned-remote cleanup using its non-secret JSON residue record.")
    parser.add_argument("--report", type=Path, help="Optional persistent JSON evidence report. Contains results, hashes and redacted failure diagnostics, including test paths, never credentials.")
    return parser


def main(argv=None):
    args = build_argument_parser().parse_args(argv)
    password = os.environ.get("QCOPILOTS_FILE_TRANSFER_TEST_PASSWORD", "")
    if not password:
        print("setup: FAIL (test_password_environment_required)", flush=True)
        return 2
    if (not 1 <= args.port <= 65535 or not 8 <= args.large_mib <= 4096 or not 0 < args.timeout <= 86400
            or (args.retry_cleanup and args.corpus_root)
            or (args.scenario_group == "corpus" and args.corpus_root is None)):
        print("setup: FAIL (invalid_arguments)", flush=True)
        return 2
    # Paramiko may report remote errors through logging independently of the
    # manager's controlled errors. The acceptance never emits transport logs.
    transport_logger = logging.getLogger("paramiko")
    transport_logger.handlers[:] = [logging.NullHandler()]
    transport_logger.propagate = False
    try:
        return AcceptanceRunner(args, password).run()
    except Exception as error:
        print(f"setup: FAIL ({_safe_result_code(error)})", flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
