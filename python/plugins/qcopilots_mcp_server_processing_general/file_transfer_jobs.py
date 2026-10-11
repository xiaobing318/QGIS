"""Persistent, cancellable SSH file transfer jobs for the General MCP service.

Transfer checkpoints outlive individual attempts. No credential is written to a
checkpoint and a terminal AsyncJobStore entry is never restarted.
"""

from __future__ import annotations

import copy
import errno
import hashlib
import json
import math
import os
import stat
import threading
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath
from typing import Any

from qcopilots_common.async_jobs import AsyncJobStore, TERMINAL_JOB_STATES, utc_now_rfc3339
from qcopilots_common.security_policy import filesystem_policy_from_environment

from .file_transfer_config import FileTransferConfigStore
from .file_transfer_cancellation import CancellationController
from .file_transfer_errors import FileTransferError, public_value
from .file_transfer_storage import DiskTransferStore
from .file_transfer_files import (
    build_manifest,
    _local_path,
    normalize_remote,
    remote_join,
    remote_not_found,
    sha256_stream,
    validate_remote_path,
)


_CHUNK = 256 * 1024
_SCOPE = "file_transfer"
_ACTIVE = {"queued", "running", "cancelling"}
_SCHEMA = 2


class _Stopped(Exception):
    pass


class _TransferLease:
    """An OS-owned byte lock, released even after a process is killed."""

    def __init__(self, path: Path):
        self.path = path
        self.handle = None

    def acquire(self) -> bool:
        handle = self.path.open("a+b")
        try:
            handle.seek(0, os.SEEK_END)
            if handle.tell() == 0:
                handle.write(b"\0")
                handle.flush()
            handle.seek(0)
            if os.name == "nt":
                import msvcrt

                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl

                fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except (OSError, IOError):
            handle.close()
            return False
        self.handle = handle
        return True

    def close(self) -> None:
        if self.handle is not None:
            self.handle.close()
            self.handle = None


@dataclass
class _Runtime:
    record: dict[str, Any]
    job_id: str
    config: dict[str, Any]
    lease: _TransferLease
    stop: threading.Event = field(default_factory=threading.Event)
    session: Any = None
    thread: threading.Thread | None = None
    started: float = field(default_factory=time.monotonic)
    transmitted: int = 0
    last_checkpoint: float = 0.0
    cancel_control: Any = field(default_factory=CancellationController)
    contributions: dict[int, tuple[int, bool]] = field(default_factory=dict)
    progress_totals: dict[str, int] = field(default_factory=dict)


class FileTransferJobManager:
    """Own transfer checkpoints and at most two concurrent worker attempts."""

    def __init__(self, config_store=None, backend=None, policy=None):
        self.config_store = config_store or FileTransferConfigStore()
        self.policy = policy or filesystem_policy_from_environment()
        if backend is None:
            from .file_transfer_backend import FileTransferBackend

            backend = FileTransferBackend(self.config_store, policy=self.policy)
        self.backend = backend
        self.root = self.policy.resolve_path(
            Path(self.config_store.root) / "file_transfers", access="write"
        )
        self.root.mkdir(parents=True, exist_ok=True)
        self.history_root = self.root / "history"
        self.history_root.mkdir(exist_ok=True)
        self._disk = DiskTransferStore(self.root)
        self._lock = threading.RLock()
        self._records: dict[str, dict[str, Any]] = {}
        self._job_index: dict[str, str] = {}
        self._request_index: dict[str, tuple[str, str, str]] = {}
        self._summary_stats: dict[str, tuple[int, int]] = {}
        self._runtimes: dict[str, _Runtime] = {}
        self._store = AsyncJobStore(scope_field="category")
        self._stopping = False
        self._load_records()

    def start(self, direction, sources=None, destination=None, overwrite=False,
              client_request_id=None):
        if direction not in {"upload", "download"}:
            raise FileTransferError("invalid_direction", "Direction must be upload or download")
        if not isinstance(overwrite, bool):
            raise FileTransferError("invalid_overwrite", "overwrite must be a boolean")
        config = self.config_store.load(require_connection=True)
        self._require_network()
        selected_sources = config.get(direction + "_sources", []) if sources is None else sources
        selected_destination = config.get(direction + "_destination", "") if destination is None else destination
        if not isinstance(selected_sources, list) or not selected_sources or any(
            not isinstance(value, str) or not value.strip() for value in selected_sources
        ):
            raise FileTransferError("invalid_sources", "Provide a non-empty list of source paths")
        if not isinstance(selected_destination, str) or not selected_destination.strip():
            raise FileTransferError("invalid_destination", "Provide an absolute destination directory")
        if direction == "upload":
            normalized_sources = [self._local(value, "read", absolute=True) for value in selected_sources]
            normalized_destination = normalize_remote(selected_destination)
        else:
            normalized_sources = [normalize_remote(value) for value in selected_sources]
            normalized_destination = self._local(selected_destination, "write", absolute=True)
        self._validate_request_id(client_request_id)
        request = {
            "direction": direction, "sources": normalized_sources,
            "destination": normalized_destination, "overwrite": overwrite,
            "endpoint": self._endpoint(config),
        }
        fingerprint = self._digest(request)
        with self._lock:
            self._refresh_summaries()
            existing = self._idempotent(client_request_id, fingerprint)
            if existing is not None:
                return existing
            self._check_capacity()
            transfer_id = str(uuid.uuid4())
            record = {
                "schema_version": _SCHEMA, "transfer_id": transfer_id,
                **request, "host_fingerprint": None, "created_at": utc_now_rfc3339(),
                "updated_at": utc_now_rfc3339(), "files": [], "manifest_ready": False,
                "created_directories": [], "attempts": [], "latest_job_id": None,
                "directory_intents": [],
                "stop_reason": None, "discarded": False, "completed": False,
                "cleanup_pending": False,
                "archived": False,
                "creation_root": normalized_destination, "revision": 0,
                "manifest_count": 0, "_details_loaded": True,
            }
            self._records[transfer_id] = record
            try:
                return self._schedule(record, config, client_request_id, fingerprint)
            except BaseException:
                if not record["attempts"]:
                    self._records.pop(transfer_id, None)
                raise

    def get_job(self, job_id, files_offset=0, files_limit=100):
        if isinstance(files_offset, bool) or not isinstance(files_offset, int) or files_offset < 0:
            raise FileTransferError("invalid_files_offset", "files_offset must be a non-negative integer")
        if isinstance(files_limit, bool) or not isinstance(files_limit, int) or not 1 <= files_limit <= 200:
            raise FileTransferError("invalid_files_limit", "files_limit must be an integer from 1 through 200")
        with self._lock:
            record, snapshot = self._find_job(job_id)
            result = self._public_summary(record, snapshot)
            if "files" in snapshot:
                values = snapshot["files"]
                files, total = copy.deepcopy(values[files_offset:files_offset + files_limit]), len(values)
            elif snapshot.get("files_receipt"):
                files, total = self._disk.read_receipt_page(job_id, files_offset, files_limit)
            elif record["latest_job_id"] == job_id and record.get("_details_loaded"):
                values = record["files"]
                files = [self._file_result(item) for item in values[files_offset:files_offset + files_limit]]
                total = len(values)
            elif record["latest_job_id"] == job_id and record.get("manifest_ready"):
                values, total = self._disk.read_manifest_page(record, files_offset, files_limit)
                files = [self._file_result(item) for item in values]
            else:
                files, total = [], 0
            result.update(files=files, files_offset=files_offset, files_limit=files_limit,
                          files_total=total, next_files_offset=files_offset + len(files)
                          if files_offset + len(files) < total else None)
            return result

    def list_jobs(self, states=None, limit=100):
        if isinstance(limit, bool) or not isinstance(limit, int) or not 1 <= limit <= 200:
            raise FileTransferError("invalid_limit", "limit must be an integer from 1 through 200")
        if states is not None and (
            not isinstance(states, list) or any(value not in _ACTIVE | TERMINAL_JOB_STATES for value in states)
        ):
            raise FileTransferError("invalid_states", "states contains an unsupported job state")
        with self._lock:
            self._refresh_summaries()
            jobs = [self._public_summary(record, snapshot) for record in self._records.values()
                    for snapshot in record["attempts"]
                    if states is None or snapshot["state"] in states]
        jobs.sort(key=lambda value: (value["created_at"], value["job_id"]), reverse=True)
        return {"jobs": jobs[:limit]}

    def cancel_job(self, job_id):
        with self._lock:
            record, snapshot = self._find_job(job_id)
            if snapshot["state"] in TERMINAL_JOB_STATES:
                return self._public_summary(record, snapshot)
            runtime = self._runtimes.get(job_id)
            if runtime is None:
                raise FileTransferError("job_busy", "This attempt is owned by another service process")
            if record["manifest_ready"] and all(item["phase"] == "published" for item in record["files"]):
                return {**copy.deepcopy(snapshot), "cancel_too_late": True}
            # Persist the user's intent before interrupting any network operation.
            record["stop_reason"] = "user_cancel"
            snapshot.update(state="cancelling", cancel_requested=True, resumable=False)
            self._persist(record)
            self._store.request_cancel(job_id, _SCOPE)
            runtime.stop.set()
            runtime.cancel_control.cancel()
            session = runtime.session
        if session is not None:
            self._close_session(session)
        return self.get_job(job_id)

    def resume_job(self, job_id, client_request_id=None):
        self._validate_request_id(client_request_id)
        config = self.config_store.load(require_connection=True)
        self._require_network()
        with self._lock:
            record, prior = self._find_job(job_id)
            fingerprint = self._digest({"resume_transfer": record["transfer_id"], "job_id": job_id})
            existing = self._idempotent(client_request_id, fingerprint)
            if existing is not None:
                return existing
            if record["discarded"] or record["stop_reason"] == "discard" or self._legacy_cancel(record):
                raise FileTransferError("transfer_not_resumable", "Discarded transfers and legacy cancellations cannot be resumed")
            if record["completed"]:
                raise FileTransferError("transfer_complete", "This transfer has already completed")
            current = self._latest(record)
            if current["state"] not in TERMINAL_JOB_STATES:
                raise FileTransferError("transfer_busy", "A transfer attempt is already active",
                               details={"job_id": current["job_id"]})
            if self._endpoint(config) != record["endpoint"]:
                raise FileTransferError("connection_changed", "Restore the original host, port and username before resuming")
            self._check_capacity()
            record["stop_reason"] = None
            return self._schedule(record, config, client_request_id, fingerprint,
                                  resumes_job_id=prior["job_id"])

    def discard_transfer(self, transfer_id):
        with self._lock:
            record = self._get_record(transfer_id)
            lease = self._lease(record)
            if not lease.acquire():
                raise FileTransferError("transfer_busy", "This transfer is owned by another service process")
            try:
                record = self._fresh_record(record, details=True)
                self._records[transfer_id] = record
                current = self._latest(record)
                if current["state"] in _ACTIVE:
                    raise FileTransferError("transfer_busy", "The persisted attempt is still active. Restart recovery before discarding")
                record["stop_reason"] = "discard"
                record["discarded"] = True
                record["cleanup_pending"] = True
                current["resumable"] = False
                current["cleanup_pending"] = True
                current["cleanup"] = {"complete": False, "residual_paths": [item["stage"] for item in record["files"] if item.get("stage") and item["phase"] != "published"], "errors": []}
                self._persist(record)
            except BaseException:
                lease.close()
                raise
        try:
            config = self.config_store.load(require_connection=False)
            cleanup = self._cleanup(record, config)
            with self._lock:
                record["cleanup_pending"] = not cleanup["complete"]
                # Cleanup evidence may be amended after terminal, not its state.
                current["cleanup"] = cleanup
                current["resumable"] = False
                current["cleanup_pending"] = record["cleanup_pending"]
                if cleanup["complete"]:
                    self._compact_checkpoint(record)
                self._persist(record)
            return {"transfer_id": transfer_id, "discarded": True, "cleanup": cleanup,
                    "cleanup_pending": record["cleanup_pending"]}
        finally:
            lease.close()

    def shutdown(self, timeout=5):
        timeout = float(timeout)
        if not math.isfinite(timeout) or timeout < 0:
            raise ValueError("timeout must be finite and non-negative")
        deadline = time.monotonic() + timeout
        with self._lock:
            self._stopping = True
            self._store.shutdown()
            runtimes = list(self._runtimes.values())
            for runtime in runtimes:
                if runtime.record["stop_reason"] != "user_cancel":
                    runtime.record["stop_reason"] = "service_shutdown"
                try:
                    self._persist(runtime.record)
                except OSError:
                    # A hard-stop recovery treats the older active checkpoint
                    # as interrupted as well. Always interrupt live I/O now.
                    pass
                runtime.stop.set()
                runtime.cancel_control.cancel()
        for runtime in runtimes:
            if runtime.session is not None:
                self._close_session(runtime.session)
        for runtime in runtimes:
            if runtime.thread is not None:
                runtime.thread.join(max(0.0, deadline - time.monotonic()))
        return all(runtime.thread is None or not runtime.thread.is_alive() for runtime in runtimes)

    def _schedule(self, record, config, request_id, fingerprint, resumes_job_id=None):
        scheduler = _TransferLease(self.root / "scheduler.lock")
        if not scheduler.acquire():
            raise FileTransferError("scheduler_busy", "Another service is scheduling a transfer. Retry shortly", retryable=True)
        try:
            self._check_destination_reservations(record)
            return self._schedule_locked(record, config, request_id, fingerprint, resumes_job_id)
        finally:
            scheduler.close()

    def _schedule_locked(self, record, config, request_id, fingerprint, resumes_job_id=None):
        lease = self._lease(record)
        if not lease.acquire():
            raise FileTransferError("transfer_busy", "This transfer is owned by another service process")
        if record["attempts"]:
            try:
                durable = self._fresh_record(record, details=True)
                if durable["completed"] or durable["discarded"] or durable["stop_reason"] == "discard" or self._legacy_cancel(durable):
                    self._records[record["transfer_id"]] = durable
                    raise FileTransferError("transfer_not_resumable", "The persisted transfer has already completed or was cancelled")
                if durable["latest_job_id"] != record["latest_job_id"] or self._latest(durable)["state"] in _ACTIVE:
                    self._records[record["transfer_id"]] = durable
                    raise FileTransferError("transfer_changed", "Another service changed this transfer. Read the latest job before resuming")
                # Use all durable offsets and publication evidence, including
                # updates made by a manager created after this instance.
                durable["stop_reason"] = None
                record = durable
                self._records[record["transfer_id"]] = record
            except BaseException:
                lease.close()
                raise
        job_id = str(uuid.uuid4())
        snapshot = {
            "job_id": job_id, "transfer_id": record["transfer_id"], "category": _SCOPE,
            "attempt": len(record["attempts"]) + 1, "direction": record["direction"],
            "sources": list(record["sources"]), "destination": record["destination"],
            "endpoint": copy.deepcopy(record["endpoint"]), "overwrite": record["overwrite"],
            "state": "queued", "phase": "queued", "created_at": utc_now_rfc3339(),
            "updated_at": utc_now_rfc3339(), "finished_at": None,
            "cancellable": True, "cancel_requested": False, "resumable": False,
            "client_request_id": request_id, "request_fingerprint": fingerprint,
            "resumes_job_id": resumes_job_id, "progress": {
                "bytes_total": 0, "bytes_completed": 0, "bytes_transferred": 0,
                "bytes_reused": 0, "files_total": 0, "files_completed": 0,
            },
            "errors": [], "cleanup": {"complete": True, "residual_paths": [], "errors": []},
            "cleanup_pending": False, "files_total": record.get("manifest_count", 0),
        }
        runtime = _Runtime(record, job_id, copy.deepcopy(config), lease)
        previous_latest = record["latest_job_id"]
        record["attempts"].append(snapshot)
        record["latest_job_id"] = job_id
        try:
            self._persist(record)
            # Durable manager indexes own idempotency across restarts. The
            # in-memory store must not retain a failed scheduler reservation.
            self._store.create(_SCOPE, snapshot, runtime=runtime)
            self._runtimes[job_id] = runtime
            runtime.thread = threading.Thread(target=self._run, args=(runtime,),
                                              name="qcopilots-file-transfer-" + job_id[:8], daemon=True)
            runtime.thread.start()
        except BaseException:
            self._runtimes.pop(job_id, None)
            try:
                self._store.transition(job_id, "failed", _SCOPE)
                self._store.set_runtime(job_id, None, _SCOPE)
            except Exception:
                pass
            self._job_index.pop(job_id, None)
            if request_id and self._request_index.get(request_id, (None, None, None))[2] == job_id:
                self._request_index.pop(request_id, None)
            record["attempts"].remove(snapshot)
            record["latest_job_id"] = previous_latest
            if not record["attempts"]:
                self._records.pop(record["transfer_id"], None)
                try:
                    self._record_path(record["transfer_id"]).unlink(missing_ok=True)
                except OSError:
                    pass
            else:
                try:
                    self._persist(record)
                except OSError:
                    pass
            lease.close()
            raise
        return copy.deepcopy(snapshot)

    def _run(self, runtime):
        record = runtime.record
        try:
            self._check_stop(runtime)
            self._set_snapshot(runtime, state="running", phase="connecting")
            runtime.session = self.backend.connect(
                runtime.config, expected_fingerprint=record["host_fingerprint"],
                cancel_control=runtime.cancel_control,
            )
            self._check_stop(runtime)
            record["host_fingerprint"] = runtime.session.fingerprint
            self._persist(record)
            sftp = runtime.session.sftp
            if not record["manifest_ready"]:
                # A crash while committing the initial inventory may leave an
                # unreferenced payload. No transfer I/O precedes its commit.
                self._disk.remove_active_data(record["transfer_id"])
                self._set_snapshot(runtime, phase="planning")
                manifest = build_manifest(
                    record["direction"], record["sources"], record["destination"],
                    sftp, self.policy, cancel_check=lambda: self._check_stop(runtime),
                    progress_callback=lambda count, path: self._scan_progress(runtime, count, path),
                )
                entries = []
                for index, item in enumerate(manifest):
                    entries.append({
                        **item, "source_sha256": None, "target_before": None,
                        "verified": False,
                        "stage": self._stage_name(record, item["target"], index) if item["kind"] == "file" else None,
                        "phase": "pending", "committed_offset": 0,
                        "committed_prefix_sha256": hashlib.sha256(b"").hexdigest(),
                    })
                record["files"] = entries
                self._preflight(runtime)
                self._prepare_creation_root(runtime)
                record["manifest_ready"] = True
                record["manifest_count"] = len(entries)
                self._persist(record, manifest=True)
            else:
                self._validate_entries(record)
            for index, entry in enumerate(record["files"]):
                entry["_entry_index"] = index
            self._progress(runtime)
            self._set_snapshot(runtime, phase="verifying_sources")
            for entry in record["files"]:
                self._check_stop(runtime)
                if entry["kind"] == "file":
                    digest = self._hash_source(runtime, entry)
                    if entry["source_sha256"] is not None and entry["source_sha256"] != digest:
                        raise FileTransferError("source_changed", "Source content changed since the transfer was created",
                                       details={"source": entry["source"]})
                    entry["source_sha256"] = digest
                    self._persist(record, entry=entry)
            self._progress(runtime)
            for entry in record["files"]:
                self._check_stop(runtime)
                if entry["kind"] == "directory":
                    self._ensure_directory(runtime, entry["target"])
                    entry["phase"] = "published"
                    self._persist(record, entry=entry)
                else:
                    self._transfer_file(runtime, entry)
            self._finish(runtime, "succeeded", "complete", resumable=False)
        except BaseException as error:
            reason = record["stop_reason"]
            if reason == "discard":
                cleanup = self._cleanup(record, runtime.config)
                record["cleanup_pending"] = not cleanup["complete"]
                self._finish(runtime, "cancelled", "cancelled", resumable=False, cleanup=cleanup)
            elif reason == "user_cancel":
                self._finish(runtime, "cancelled", "cancelled", resumable=True)
            elif reason == "service_shutdown" or isinstance(error, _Stopped):
                self._finish(runtime, "failed", "interrupted", resumable=True,
                             error={"code": "interrupted", "message": "Service stopped before the transfer completed", "retryable": True})
            else:
                self._finish(runtime, "failed", "failed", resumable=True,
                             error=self._error(error, (runtime.config.get("password"),)))
        finally:
            if runtime.session is not None:
                self._close_session(runtime.session)
            runtime.config.clear()
            runtime.lease.close()
            with self._lock:
                self._runtimes.pop(runtime.job_id, None)

    def _prepare_creation_root(self, runtime):
        """Freeze the highest missing destination ancestor before any mkdir."""
        record = runtime.record
        current = record["destination"]
        highest_missing = current
        while True:
            self._check_stop(runtime)
            metadata = self._target_stat(runtime, current)
            if metadata is not None:
                if not stat.S_ISDIR(metadata.st_mode):
                    raise FileTransferError("destination_conflict", "A destination ancestor is not a directory")
                record["creation_root"] = highest_missing
                return
            highest_missing = current
            parent = self._parent(record, current)
            if parent == current:
                raise FileTransferError("invalid_destination", "Destination filesystem root does not exist")
            current = parent

    def _preflight(self, runtime):
        record = runtime.record
        collisions = []
        for entry in record["files"]:
            self._check_stop(runtime)
            attrs = self._target_stat(runtime, entry["target"])
            if attrs is None:
                continue
            directory = stat.S_ISDIR(attrs.st_mode)
            if entry["kind"] == "directory":
                if not directory:
                    collisions.append(entry["target"])
            elif directory or not record["overwrite"]:
                collisions.append(entry["target"])
            else:
                digest, size = self._hash_target(runtime, entry["target"])
                entry["target_before"] = {"size": size, "sha256": digest}
        if collisions:
            raise FileTransferError("destination_conflict", "Destination entries already exist or have incompatible types",
                           details={"paths": collisions})

    def _transfer_file(self, runtime, entry):
        record = runtime.record
        self._set_snapshot(runtime, phase="verifying_destination")
        if entry["phase"] == "published":
            self._verify_target(runtime, entry["target"], entry["source_sha256"], entry["size"])
            entry["verified"] = True
            self._progress(runtime, entry)
            return
        stage = entry["stage"]
        # An interrupted rename may have committed without its response or journal.
        if entry["phase"] == "publish_intent" and self._target_stat(runtime, stage) is None:
            self._verify_target(runtime, entry["target"], entry["source_sha256"], entry["size"])
            entry["phase"] = "published"
            entry["verified"] = True
            entry["committed_offset"] = entry["size"]
            self._persist(record, entry=entry)
            self._progress(runtime, entry)
            return
        self._check_destination_version(runtime, entry)
        self._ensure_directory(runtime, self._parent(record, entry["target"]))
        stage_attrs = self._target_stat(runtime, stage)
        if stage_attrs is None:
            entry["phase"] = "creating_stage"
            entry["committed_offset"] = 0
            entry["committed_prefix_sha256"] = hashlib.sha256(b"").hexdigest()
            self._persist(record, entry=entry)
            with self._open_target(runtime, stage, exclusive=True):
                pass
            stage_attrs = self._target_stat(runtime, stage)
        if stage_attrs is None or not stat.S_ISREG(stage_attrs.st_mode):
            raise FileTransferError("invalid_partial", "Transfer staging path must be a regular file")
        offset = int(stage_attrs.st_size)
        if offset < 0 or offset > entry["size"]:
            raise FileTransferError("invalid_partial", "Partial file is larger than its frozen source")
        self._set_snapshot(runtime, phase="verifying_prefix")
        with self._open_source(runtime, entry["source"]) as source:
            source_prefix, count = self._hash_stream(runtime, source, offset, entry["source"], offset)
        partial_prefix, partial_count = self._hash_target(runtime, stage, limit=offset)
        if count != offset or partial_count != offset or source_prefix != partial_prefix:
            raise FileTransferError("partial_mismatch", "Partial file does not match the source prefix",
                           details={"path": stage})
        checkpoint_offset = entry["committed_offset"]
        if checkpoint_offset and offset >= checkpoint_offset:
            digest, count = self._hash_target(runtime, stage, limit=checkpoint_offset)
            if count != checkpoint_offset or digest != entry["committed_prefix_sha256"]:
                raise FileTransferError("partial_mismatch", "Partial file differs from its saved checkpoint")
        # Reconstruct the hasher instead of serializing an implementation-specific
        # hashlib object. Prefix validation never trusts only a byte count.
        digest = hashlib.sha256()
        with self._open_source(runtime, entry["source"]) as source:
            remaining = offset
            while remaining:
                self._check_stop(runtime)
                value = source.read(min(_CHUNK, remaining))
                if not value:
                    raise FileTransferError("source_changed", "Source ended while validating its prefix")
                digest.update(value)
                remaining -= len(value)
            with self._open_target(runtime, stage, resume=True) as target:
                target.seek(offset)
                entry["phase"] = "transferring"
                entry["committed_offset"] = offset
                entry["committed_prefix_sha256"] = digest.hexdigest()
                self._persist(record, entry=entry)
                self._set_snapshot(runtime, phase="transferring")
                self._progress(runtime, entry)
                while offset < entry["size"]:
                    self._check_stop(runtime)
                    value = source.read(min(_CHUNK, entry["size"] - offset))
                    if not value:
                        raise FileTransferError("source_changed", "Source ended before its expected size")
                    target.write(value)
                    target.flush()
                    digest.update(value)
                    offset += len(value)
                    runtime.transmitted += len(value)
                    entry["committed_offset"] = offset
                    entry["committed_prefix_sha256"] = digest.hexdigest()
                    self._progress(runtime, entry)
                    if time.monotonic() - runtime.last_checkpoint >= 1 or offset == entry["size"]:
                        if record["direction"] == "download":
                            os.fsync(target.fileno())
                        self._persist(record, entry=entry)
                        runtime.last_checkpoint = time.monotonic()
                    self._throttle(runtime)
                if source.read(1):
                    raise FileTransferError("source_changed", "Source grew during transfer")
        if digest.hexdigest() != entry["source_sha256"]:
            raise FileTransferError("source_changed", "Source content changed during transfer")
        self._set_snapshot(runtime, phase="verifying_transfer")
        self._verify_target(runtime, stage, entry["source_sha256"], entry["size"])
        entry["phase"] = "ready"
        self._persist(record, entry=entry)
        self._publish(runtime, entry)

    def _publish(self, runtime, entry):
        record = runtime.record
        self._check_stop(runtime)
        self._check_destination_version(runtime, entry)
        entry["phase"] = "publish_intent"
        self._persist(record, entry=entry)
        self._set_snapshot(runtime, phase="publishing")
        # Cancellation is serialized against the irreversible publication step.
        with self._lock:
            self._check_stop(runtime)
            if record["direction"] == "upload":
                sftp = runtime.session.sftp
                if record["overwrite"] and entry["target_before"] is not None:
                    try:
                        sftp.posix_rename(entry["stage"], entry["target"])
                    except (AttributeError, OSError) as error:
                        if isinstance(error, AttributeError) or self._unsupported_rename(error):
                            raise FileTransferError("overwrite_unsupported", "Server does not support atomic replacement. The original target was preserved") from error
                        raise
                else:
                    sftp.rename(entry["stage"], entry["target"])
            else:
                if record["overwrite"] and entry["target_before"] is not None:
                    os.replace(entry["stage"], entry["target"])
                elif os.name == "nt":
                    os.rename(entry["stage"], entry["target"])
                else:
                    # link is an atomic no-replace publication on POSIX. The
                    # production target is Windows, this also protects tests.
                    os.link(entry["stage"], entry["target"])
                    os.unlink(entry["stage"])
            entry["phase"] = "published"
            self._persist(record, entry=entry)
        self._verify_target(runtime, entry["target"], entry["source_sha256"], entry["size"])
        entry["verified"] = True
        self._persist(record, entry=entry)
        self._progress(runtime, entry)

    def _check_destination_version(self, runtime, entry):
        attrs = self._target_stat(runtime, entry["target"])
        before = entry["target_before"]
        if before is None:
            if attrs is not None:
                raise FileTransferError("destination_conflict", "Destination appeared after preflight",
                               details={"path": entry["target"]})
        elif attrs is None:
            raise FileTransferError("destination_changed", "Existing destination disappeared after preflight")
        else:
            self._verify_target(runtime, entry["target"], before["sha256"], before["size"],
                                code="destination_changed")

    def _hash_source(self, runtime, entry):
        if runtime.record["direction"] == "upload":
            path = self._local(entry["source"], "read")
            before = Path(path).stat()
        else:
            before = validate_remote_path(runtime.session.sftp, entry["source"])
        if before is None or not stat.S_ISREG(before.st_mode) or int(before.st_size) != entry["size"]:
            raise FileTransferError("source_changed", "Source size or file type changed")
        if entry.get("mtime") is not None and before.st_mtime != entry["mtime"]:
            raise FileTransferError("source_changed", "Source modification time changed")
        with self._open_source(runtime, entry["source"]) as handle:
            digest, count = self._hash_stream(runtime, handle, None, entry["source"], entry["size"])
        if runtime.record["direction"] == "upload":
            after = Path(self._local(entry["source"], "read")).stat()
        else:
            after = validate_remote_path(runtime.session.sftp, entry["source"])
        if count != entry["size"] or after.st_size != before.st_size or after.st_mtime != before.st_mtime:
            raise FileTransferError("source_changed", "Source changed while its checksum was calculated")
        return digest

    def _open_source(self, runtime, path):
        if runtime.record["direction"] == "upload":
            return open(self._local(path, "read"), "rb")
        validate_remote_path(runtime.session.sftp, path)
        return runtime.session.sftp.open(path, "rb")

    def _open_target(self, runtime, path, exclusive=False, resume=False):
        if runtime.record["direction"] == "download":
            return open(self._local(path, "write"), "xb" if exclusive else "r+b" if resume else "rb")
        validate_remote_path(runtime.session.sftp, path, allow_missing=exclusive)
        return runtime.session.sftp.open(path, "wx" if exclusive else "r+" if resume else "rb")

    def _target_stat(self, runtime, path):
        if runtime.record["direction"] == "upload":
            attrs = validate_remote_path(runtime.session.sftp, path, allow_missing=True)
        else:
            path = self._local(path, "write")
            try:
                attrs = Path(path).lstat()
            except FileNotFoundError:
                return None
        if attrs is not None and not (stat.S_ISREG(attrs.st_mode) or stat.S_ISDIR(attrs.st_mode)):
            raise FileTransferError("unsafe_path", "Transfer paths must be ordinary files or directories")
        return attrs

    def _hash_target(self, runtime, path, limit=None):
        attrs = self._target_stat(runtime, path)
        if attrs is None or not stat.S_ISREG(attrs.st_mode):
            raise FileTransferError("destination_changed", "Expected destination file is missing or changed type",
                           details={"path": path})
        with self._open_target(runtime, path) as handle:
            return self._hash_stream(runtime, handle, limit, path, int(attrs.st_size) if limit is None else limit)

    def _scan_progress(self, runtime, count, path):
        with self._lock:
            progress = dict(self._latest(runtime.record)["progress"])
            progress.update(items_scanned=count, current_path=path)
            self._set_snapshot(runtime, progress=progress, persist=False)

    def _hash_stream(self, runtime, handle, limit, path, total):
        manager = self
        class Reader:
            count = 0

            def read(self, size):
                value = handle.read(size)
                self.count += len(value)
                with manager._lock:
                    progress = dict(manager._latest(runtime.record)["progress"])
                    progress.update(verification_bytes_completed=self.count,
                                    verification_bytes_total=total, current_path=path)
                    manager._set_snapshot(runtime, progress=progress, persist=False)
                return value

        with self._lock:
            progress = dict(self._latest(runtime.record)["progress"])
            progress.update(verification_bytes_completed=0, verification_bytes_total=total,
                            current_path=path)
            self._set_snapshot(runtime, progress=progress, persist=False)
        return sha256_stream(Reader(), limit, lambda: self._check_stop(runtime))

    def _verify_target(self, runtime, path, expected_hash, expected_size, code="verification_failed"):
        digest, count = self._hash_target(runtime, path)
        if digest != expected_hash or count != expected_size:
            raise FileTransferError(code, "Destination checksum or size differs from its expected content",
                           details={"path": path})

    def _ensure_directory(self, runtime, path):
        record = runtime.record
        attrs = self._target_stat(runtime, path)
        if attrs is not None:
            if not stat.S_ISDIR(attrs.st_mode):
                raise FileTransferError("destination_conflict", "Destination parent is not a directory")
            return
        parent = self._parent(record, path)
        if parent == path:
            raise FileTransferError("invalid_destination", "Destination filesystem root does not exist")
        if not self._within(path, record["creation_root"], remote=record["direction"] == "upload"):
            raise FileTransferError("destination_parent_missing", "A previously existing destination ancestor disappeared")
        self._ensure_directory(runtime, parent)
        self._check_stop(runtime)
        # Intent does not confer ownership. Only a successful mkdir response
        # adds a directory to the set which cleanup is allowed to remove.
        if path not in record["directory_intents"]:
            record["directory_intents"].append(path)
        self._persist(record, directory=(path, False))
        try:
            if record["direction"] == "upload":
                runtime.session.sftp.mkdir(path)
            else:
                Path(self._local(path, "write")).mkdir()
        except OSError as error:
            if not isinstance(error, FileExistsError) and getattr(error, "errno", None) != errno.EEXIST:
                # A lost mkdir response does not prove another writer created
                # the directory. Preserve its unresolved ownership intent.
                raise
            attrs = self._target_stat(runtime, path)
            if attrs is None:
                raise
            if not stat.S_ISDIR(attrs.st_mode):
                raise FileTransferError("destination_changed", "Destination directory changed during creation") from None
            record["directory_intents"].remove(path)
            self._persist(record, directory=(path, None))
            return
        record["created_directories"].append(path)
        record["directory_intents"].remove(path)
        self._persist(record, directory=(path, True))

    def _cleanup(self, record, config):
        paths = [entry["stage"] for entry in record["files"]
                 if entry["kind"] == "file" and entry["phase"] != "published" and entry.get("stage")]
        residual = []
        errors = []
        session = None
        try:
            self._validate_entries(record)
            if record["direction"] == "upload" and (paths or record["created_directories"] or record.get("directory_intents")):
                self._require_network()
                if self._endpoint(config) != record["endpoint"]:
                    raise FileTransferError("connection_changed", "Restore the original connection to clean remote partial files")
                session = self.backend.connect(config, expected_fingerprint=record["host_fingerprint"])
            for path in paths:
                try:
                    if record["direction"] == "upload":
                        attrs = validate_remote_path(session.sftp, path, allow_missing=True)
                        if attrs is not None:
                            if not stat.S_ISREG(attrs.st_mode):
                                raise FileTransferError("unsafe_path", "Cleanup refuses a staging path whose type changed")
                            session.sftp.remove(path)
                    else:
                        target = Path(self._local(path, "write"))
                        if target.exists():
                            if not target.is_file():
                                raise FileTransferError("unsafe_path", "Cleanup refuses a staging path whose type changed")
                            target.unlink()
                except Exception as error:
                    residual.append(path)
                    errors.append(self._error(error, (config.get("password"),)))
            for path in reversed(record["created_directories"]):
                try:
                    if record["direction"] == "upload":
                        attrs = validate_remote_path(session.sftp, path, allow_missing=True)
                        if attrs is not None:
                            if not session.sftp.listdir_attr(path):
                                try:
                                    session.sftp.rmdir(path)
                                except OSError:
                                    if not session.sftp.listdir_attr(path):
                                        raise
                    else:
                        target = Path(self._local(path, "write"))
                        if target.exists():
                            target.rmdir()
                except OSError as error:
                    # A directory containing published/user files must remain.
                    if getattr(error, "errno", None) not in {errno.ENOTEMPTY, errno.EEXIST}:
                        residual.append(path)
                        errors.append(self._error(error, (config.get("password"),)))
                except Exception as error:
                    residual.append(path)
                    errors.append(self._error(error, (config.get("password"),)))
            for path in record.get("directory_intents", []):
                if record["direction"] == "upload":
                    exists = validate_remote_path(session.sftp, path, allow_missing=True) is not None
                else:
                    exists = Path(self._local(path, "write")).exists()
                if exists:
                    residual.append(path)
                    errors.append({"code": "directory_ownership_unknown", "message": "Directory creation was interrupted before ownership could be confirmed", "retryable": False})
        except Exception as error:
            residual = list(dict.fromkeys([*residual, *paths, *record["created_directories"],
                                          *record.get("directory_intents", [])]))
            errors.append(self._error(error, (config.get("password"),)))
        finally:
            if session is not None:
                self._close_session(session)
        return {"complete": not errors and not residual, "residual_paths": residual, "errors": errors}

    def _progress(self, runtime, entry=None):
        if not runtime.progress_totals:
            files = [item for item in runtime.record["files"] if item["kind"] == "file"]
            runtime.progress_totals = {"bytes_total": sum(item["size"] for item in files),
                                       "bytes_completed": 0, "files_total": len(files), "files_completed": 0}
            for item in files:
                index = item["_entry_index"]
                published = item["phase"] == "published"
                count = item["size"] if published else item["committed_offset"]
                runtime.contributions[index] = (count, published)
                runtime.progress_totals["bytes_completed"] += count
                runtime.progress_totals["files_completed"] += int(published)
        if entry is not None and entry["kind"] == "file":
            index = entry["_entry_index"]
            old_count, old_published = runtime.contributions.get(index, (0, False))
            published = entry["phase"] == "published"
            count = entry["size"] if published else entry["committed_offset"]
            runtime.progress_totals["bytes_completed"] += count - old_count
            runtime.progress_totals["files_completed"] += int(published) - int(old_published)
            runtime.contributions[index] = (count, published)
        completed = runtime.progress_totals["bytes_completed"]
        progress = dict(self._latest(runtime.record)["progress"])
        progress.update({
            **runtime.progress_totals,
            "bytes_transferred": runtime.transmitted, "bytes_reused": max(0, completed - runtime.transmitted),
        })
        self._set_snapshot(runtime, progress=progress, files_total=len(runtime.record["files"]), persist=False)

    def _set_snapshot(self, runtime, *, state=None, persist=True, **changes):
        with self._lock:
            snapshot = self._latest(runtime.record)
            if snapshot["state"] in TERMINAL_JOB_STATES:
                return
            changes["updated_at"] = utc_now_rfc3339()
            snapshot.update(changes)
            if state is not None:
                snapshot["state"] = state
                self._store.transition(runtime.job_id, state, _SCOPE, **changes)
            else:
                self._store.patch(runtime.job_id, _SCOPE, **changes)
            if persist:
                self._persist(runtime.record)

    def _finish(self, runtime, state, phase, *, resumable, cleanup=None, error=None):
        if runtime.session is not None:
            self._close_session(runtime.session)
            runtime.session = None
        with self._lock:
            record = runtime.record
            snapshot = self._latest(record)
            record["completed"] = state == "succeeded"
            snapshot.update(state=state, phase=phase, resumable=resumable,
                            finished_at=utc_now_rfc3339(), updated_at=utc_now_rfc3339(),
                            cancellable=False)
            snapshot["files_total"] = len(record["files"])
            snapshot["cleanup_pending"] = bool(record["cleanup_pending"])
            if cleanup is not None:
                snapshot["cleanup"] = cleanup
            if error is not None:
                snapshot["errors"].append(error)
            try:
                self._disk.write_receipt(record, runtime.job_id,
                                         (self._file_result(item) for item in record["files"]))
                snapshot["files_receipt"] = True
                if state == "succeeded" or (record["discarded"] and cleanup and cleanup["complete"]):
                    self._compact_checkpoint(record)
                self._persist(record)
            except (OSError, FileTransferError):
                record["_unsaved_final"] = True
                snapshot["errors"].append({"code": "checkpoint_write_failed", "message": "Final checkpoint could not be saved. Recovery will recheck the last durable state", "retryable": True})
            fields = {key: value for key, value in snapshot.items() if key not in {"state", "job_id"}}
            self._store.transition(runtime.job_id, state, _SCOPE, **fields)
            runtime.lease.close()
            self._runtimes.pop(runtime.job_id, None)

    @staticmethod
    def _file_result(item):
        return {"source": item["source"], "target": item["target"], "kind": item["kind"],
                "state": item["phase"], "size": item["size"], "sha256": item["source_sha256"],
                "verified": bool(item.get("verified", False)), "bytes_completed": item["committed_offset"]}

    @staticmethod
    def _public_summary(record, snapshot):
        result = copy.deepcopy({key: value for key, value in snapshot.items()
                                if key not in {"files", "files_receipt"} and not key.startswith("_")})
        # Attempt results remain historical, while this flag describes whether
        # the transfer can currently be resumed through that attempt's ID.
        result["resumable"] = bool(
            snapshot.get("resumable", False)
            and not record.get("completed", False)
            and not record.get("discarded", False)
            and record.get("stop_reason") != "discard"
            and not FileTransferJobManager._legacy_cancel(record)
            and FileTransferJobManager._latest(record)["state"] in TERMINAL_JOB_STATES
        )
        result["cleanup_pending"] = bool(record.get("cleanup_pending", False))
        result.setdefault("files_total", len(snapshot.get("files", [])))
        return result

    @staticmethod
    def _legacy_cancel(record):
        return record.get("schema_version") == 1 and record.get("stop_reason") == "user_cancel"

    def _throttle(self, runtime):
        rate = runtime.config.get("max_bytes_per_second", 0)
        if rate:
            delay = runtime.transmitted / rate - (time.monotonic() - runtime.started)
            if delay > 0:
                runtime.stop.wait(delay)
                self._check_stop(runtime)

    def _check_stop(self, runtime):
        if runtime.stop.is_set():
            raise _Stopped()

    def _require_network(self):
        if not self.policy.network_enabled:
            raise FileTransferError("network_disabled", "File transfer network access is disabled by security policy")
        if self.policy.restricted:
            raise FileTransferError("file_transfer_restricted", "File transfer is unavailable in formal_restricted mode")

    def _local(self, value, access, absolute=False):
        if absolute and not Path(value).is_absolute():
            raise FileTransferError("invalid_path", "Local transfer paths must be absolute")
        return str(_local_path(self.policy, value, access))

    def _stage_name(self, record, target, index):
        name = ".qcopilots-file-transfer-" + record["transfer_id"] + "-" + str(index) + ".part"
        if record["direction"] == "upload":
            return remote_join(self._parent(record, target), name)
        return str(Path(target).parent / name)

    @staticmethod
    def _parent(record, path):
        if record["direction"] == "download":
            return str(Path(path).parent)
        normalized = normalize_remote(path)
        if len(normalized) == 3 and normalized[1:] == ":/":
            return normalized
        parent = str(PurePosixPath(normalized).parent)
        if parent.endswith(":"):
            parent += "/"
        return parent

    def _validate_entries(self, record):
        if not isinstance(record, dict):
            raise FileTransferError("invalid_checkpoint", "Checkpoint must be an object")
        self._uuid(record.get("transfer_id"))
        direction = record.get("direction")
        if direction not in {"upload", "download"}:
            raise FileTransferError("invalid_checkpoint", "Checkpoint direction is invalid")
        if record.get("schema_version") not in {1, _SCHEMA}:
            raise FileTransferError("invalid_checkpoint", "Checkpoint version is unsupported")
        for name in ("overwrite", "manifest_ready", "discarded", "completed", "cleanup_pending"):
            if not isinstance(record.get(name), bool):
                raise FileTransferError("invalid_checkpoint", "Checkpoint flags are invalid")
        if record.get("stop_reason") not in {None, "user_cancel", "service_shutdown", "discard"}:
            raise FileTransferError("invalid_checkpoint", "Checkpoint stop reason is invalid")
        endpoint = record.get("endpoint")
        if (not isinstance(endpoint, dict) or not isinstance(endpoint.get("host"), str)
                or not endpoint["host"] or not isinstance(endpoint.get("username"), str)
                or not endpoint["username"] or isinstance(endpoint.get("port"), bool)
                or not isinstance(endpoint.get("port"), int) or not 1 <= endpoint["port"] <= 65535):
            raise FileTransferError("invalid_checkpoint", "Checkpoint endpoint is invalid")
        sources = record.get("sources")
        if not isinstance(sources, list) or not sources or any(not isinstance(item, str) for item in sources):
            raise FileTransferError("invalid_checkpoint", "Checkpoint source paths are invalid")
        destination = record.get("destination")
        if direction == "upload":
            if normalize_remote(destination) != destination:
                raise FileTransferError("invalid_checkpoint", "Checkpoint destination is not canonical")
        else:
            if not isinstance(destination, str) or not Path(destination).is_absolute():
                raise FileTransferError("invalid_checkpoint", "Checkpoint destination must be absolute")
        self._path_key(destination, remote=direction == "upload")
        creation_root = record.get("creation_root", destination)
        if not isinstance(creation_root, str) or not self._within(destination, creation_root, remote=direction == "upload"):
            raise FileTransferError("invalid_checkpoint", "Checkpoint creation root is not a destination ancestor")
        if record.get("schema_version") == _SCHEMA:
            for key in ("revision", "manifest_count"):
                value = record.get(key)
                if isinstance(value, bool) or not isinstance(value, int) or value < 0:
                    raise FileTransferError("invalid_checkpoint", "Checkpoint counters are invalid")
        for source in sources:
            self._path_key(source, remote=direction == "download")
        if not isinstance(record.get("files"), list):
            raise FileTransferError("invalid_checkpoint", "Checkpoint file entries are invalid")
        targets = set()
        for index, entry in enumerate(record["files"]):
            if not isinstance(entry, dict) or entry.get("kind") not in {"file", "directory"}:
                raise FileTransferError("invalid_checkpoint", "Checkpoint contains an invalid file kind")
            expected = self._mapped_target(record, entry.get("source"))
            if entry.get("target") != expected:
                raise FileTransferError("invalid_checkpoint", "Checkpoint target does not match its source mapping")
            target_key = self._path_key(expected, remote=direction == "upload")
            if target_key in targets:
                raise FileTransferError("invalid_checkpoint", "Checkpoint has duplicate destinations")
            targets.add(target_key)
            if entry["kind"] == "file" and entry["stage"] != self._stage_name(record, entry["target"], index):
                raise FileTransferError("invalid_checkpoint", "Checkpoint staging path does not belong to this transfer")
            size = entry.get("size")
            offset = entry.get("committed_offset")
            if (isinstance(size, bool) or not isinstance(size, int) or size < 0
                    or isinstance(offset, bool) or not isinstance(offset, int) or not 0 <= offset <= size):
                raise FileTransferError("invalid_checkpoint", "Checkpoint file size or offset is invalid")
            for key in ("source_sha256", "committed_prefix_sha256"):
                value = entry.get(key)
                if value is not None and (not isinstance(value, str) or len(value) != 64 or any(char not in "0123456789abcdef" for char in value)):
                    raise FileTransferError("invalid_checkpoint", "Checkpoint checksum is invalid")
            if entry.get("phase") not in {"pending", "creating_stage", "transferring", "ready", "publish_intent", "published"}:
                raise FileTransferError("invalid_checkpoint", "Checkpoint file phase is invalid")
        for field in ("created_directories", "directory_intents"):
            values = record.get(field, [])
            if not isinstance(values, list) or any(not isinstance(path, str) or not self._within(
                path, creation_root, remote=direction == "upload") for path in values):
                raise FileTransferError("invalid_checkpoint", "Checkpoint cleanup directory is outside its creation root")
            for path in values:
                if self._parent(record, path) == path:
                    raise FileTransferError("invalid_checkpoint", "A filesystem root cannot be owned by a transfer")
                if not any(self._within(entry["target"], path, remote=direction == "upload") for entry in record["files"]):
                    raise FileTransferError("invalid_checkpoint", "Checkpoint directory is not used by the transfer")
        attempts = record.get("attempts")
        if not isinstance(attempts, list) or not attempts:
            raise FileTransferError("invalid_checkpoint", "Checkpoint attempts are missing")
        for attempt in attempts:
            if not isinstance(attempt, dict) or attempt.get("state") not in _ACTIVE | TERMINAL_JOB_STATES:
                raise FileTransferError("invalid_checkpoint", "Checkpoint attempt state is invalid")
            self._uuid(attempt.get("job_id"))
            if attempt.get("transfer_id") != record["transfer_id"]:
                raise FileTransferError("invalid_checkpoint", "Checkpoint attempt belongs to another transfer")
            if (not isinstance(attempt.get("created_at"), str)
                    or not isinstance(attempt.get("errors"), list)
                    or not isinstance(attempt.get("progress"), dict)
                    or not isinstance(attempt.get("cleanup"), dict)):
                raise FileTransferError("invalid_checkpoint", "Checkpoint attempt metadata is invalid")
        if record.get("latest_job_id") not in {item["job_id"] for item in attempts}:
            raise FileTransferError("invalid_checkpoint", "Checkpoint latest attempt is missing")

    @staticmethod
    def _path_key(value, *, remote):
        if not remote and (not isinstance(value, str) or not Path(value).is_absolute() or ".." in Path(value).parts
                           or value.replace("\\", "/").startswith(("//", "/??/"))
                           or any(ord(char) < 32 for char in value)):
            raise FileTransferError("invalid_checkpoint", "Local checkpoint paths must be absolute without parent traversal")
        normalized = normalize_remote(value) if remote else str(Path(value))
        normalized = normalized.replace("\\", "/").rstrip("/")
        if not remote or (len(normalized) >= 2 and normalized[1] == ":"):
            normalized = normalized.casefold()
        return normalized

    def _within(self, value, root, *, remote):
        child = self._path_key(value, remote=remote)
        parent = self._path_key(root, remote=remote)
        return child == parent or child.startswith(parent + "/")

    def _mapped_target(self, record, source):
        if not isinstance(source, str):
            raise FileTransferError("invalid_checkpoint", "Checkpoint source is invalid")
        remote = record["direction"] == "download"
        for root in record["sources"]:
            if self._within(source, root, remote=remote):
                root_parts = (PurePosixPath(normalize_remote(root)).parts if remote else Path(root).parts)
                source_parts = (PurePosixPath(normalize_remote(source)).parts if remote else Path(source).parts)
                relative = source_parts[len(root_parts):]
                if not root_parts or root_parts[-1] in {"/", "\\"} or root_parts[-1].endswith((":\\", ":")):
                    raise FileTransferError("invalid_checkpoint", "A source filesystem root is not supported")
                if record["direction"] == "upload":
                    target = remote_join(record["destination"], root_parts[-1])
                    for component in relative:
                        target = remote_join(target, component)
                    return target
                return str(Path(record["destination"]).joinpath(root_parts[-1], *relative))
        raise FileTransferError("invalid_checkpoint", "Checkpoint source is outside the selected sources")

    def _check_destination_reservations(self, record):
        active_count = 0
        for path in self.root.glob("*.json"):
            try:
                other = self._read_record(path)
                self._validate_entries(other)
                if other["transfer_id"] == record["transfer_id"] or self._latest(other)["state"] not in _ACTIVE:
                    continue
                lease = self._lease(other)
                if lease.acquire():
                    lease.close()
                    continue
                active_count += 1
                if other["direction"] != record["direction"]:
                    continue
                if record["direction"] == "upload" and other["endpoint"] != record["endpoint"]:
                    continue
                remote = record["direction"] == "upload"
                if self._within(record["destination"], other["destination"], remote=remote) or self._within(other["destination"], record["destination"], remote=remote):
                    raise FileTransferError("destination_busy", "Another active transfer owns this destination or an overlapping directory")
            except (ValueError, TypeError, KeyError, OSError, AttributeError):
                continue
            except FileTransferError as error:
                if error.code == "destination_busy":
                    raise
                continue
        if active_count >= 2:
            raise FileTransferError("too_many_jobs", "At most two file transfer attempts can run simultaneously", retryable=True)

    @staticmethod
    def _compact_checkpoint(record):
        record["files"] = []
        record["created_directories"] = []
        record["directory_intents"] = []
        record["manifest_ready"] = False
        record["manifest_count"] = 0
        record["archived"] = True

    def _record_path(self, transfer_id, archived=False):
        self._uuid(transfer_id)
        return (self.history_root if archived else self.root) / (transfer_id + ".json")

    def _lease(self, record):
        self._uuid(record["transfer_id"])
        return _TransferLease(self.root / (record["transfer_id"] + ".lock"))

    def _persist(self, record, *, entry=None, directory=None, manifest=False):
        """Commit one changed entry and a small summary, never rewrite the tree."""
        with self._lock:
            legacy = copy.deepcopy(record) if record.get("schema_version") == 1 else None
            try:
                if legacy is not None:
                    self._upgrade_legacy(record)
                    manifest = bool(record["manifest_ready"])
                record["updated_at"] = utc_now_rfc3339()
                record["revision"] = record.get("revision", 0) + 1
                if manifest:
                    record["manifest_count"] = len(record["files"])
                    self._disk.write_manifest(record)
                if entry is not None:
                    self._disk.write_entry(record, entry["_entry_index"], entry)
                if directory is not None:
                    self._disk.write_directory(record, *directory)
                self._disk.write_summary(record)
            except BaseException:
                if legacy is not None:
                    record.clear()
                    record.update(legacy)
                elif manifest:
                    # Never publish a summary which claims a partially written
                    # inventory is usable. A subsequent attempt replans it.
                    record["manifest_ready"] = False
                    record["manifest_count"] = 0
                raise
            record.pop("_unsaved_final", None)
            self._index_record(record)
            if record.get("archived"):
                self._record_path(record["transfer_id"]).unlink(missing_ok=True)
                self._disk.remove_active_data(record["transfer_id"])

    def _upgrade_legacy(self, record):
        """Called only while the caller owns the transfer lease and is writing."""
        self._validate_entries(record)
        legacy_cancel = self._legacy_cancel(record)
        for snapshot in record["attempts"]:
            if "files" in snapshot and snapshot["state"] in TERMINAL_JOB_STATES:
                self._disk.write_receipt(record, snapshot["job_id"], snapshot["files"])
                snapshot["files_total"] = len(snapshot.pop("files"))
                snapshot["files_receipt"] = True
            else:
                snapshot.pop("files", None)
        if legacy_cancel:
            record["stop_reason"] = "discard"
            record["discarded"] = True
            record["cleanup_pending"] = not record.get("archived", False)
            for snapshot in record["attempts"]:
                snapshot["resumable"] = False
        record.update(schema_version=_SCHEMA, creation_root=record["destination"],
                      manifest_count=len(record["files"]) if record["manifest_ready"] else 0,
                      revision=0, _details_loaded=True)
        for index, item in enumerate(record["files"]):
            item["_entry_index"] = index
        # These ledger records must exist before the schema 2 summary commits.
        for path in record["created_directories"]:
            self._disk.write_directory(record, path, True)
        for path in record.get("directory_intents", []):
            self._disk.write_directory(record, path, False)

    def _read_record(self, path, *, details=False):
        record = self._disk.read_summary(path)
        if not isinstance(record, dict):
            raise FileTransferError("invalid_checkpoint", "Checkpoint must be an object")
        record = {key: value for key, value in record.items() if not key.startswith("_")}
        if record.get("schema_version") == _SCHEMA:
            record.update(files=[], created_directories=[], directory_intents=[], _details_loaded=False)
            self._validate_entries(record)
            if details and record.get("manifest_ready"):
                record = self._disk.load_details(record)
                for index, item in enumerate(record["files"]):
                    item["_entry_index"] = index
            elif details:
                record["_details_loaded"] = True
        else:
            record["_details_loaded"] = True
        self._validate_entries(record)
        return record

    def _index_record(self, record):
        transfer_id = record["transfer_id"]
        for snapshot in record["attempts"]:
            self._job_index[snapshot["job_id"]] = transfer_id
            request_id = snapshot.get("client_request_id")
            if request_id:
                self._request_index[request_id] = (snapshot.get("request_fingerprint"), transfer_id, snapshot["job_id"])

    @staticmethod
    def _record_version(record):
        return (record.get("revision", 0), record.get("updated_at", ""), bool(record.get("archived")))

    def _prefer_memory(self, previous, candidate):
        if previous.get("_unsaved_final"):
            return (previous.get("latest_job_id") == candidate.get("latest_job_id")
                    and self._latest(candidate)["state"] in _ACTIVE
                    and self._record_version(previous) >= self._record_version(candidate))
        return self._record_version(previous) > self._record_version(candidate)

    def _refresh_summaries(self):
        for path in [*self.history_root.glob("*.json"), *self.root.glob("*.json")]:
            try:
                self._uuid(path.stem)
                metadata = path.stat()
                stamp = (metadata.st_mtime_ns, metadata.st_size)
                if self._summary_stats.get(str(path)) == stamp:
                    continue
                record = self._read_record(path)
                if record["transfer_id"] != path.stem:
                    continue
                previous = self._records.get(record["transfer_id"])
                if previous is not None and (previous.get("latest_job_id") in self._runtimes
                        or self._prefer_memory(previous, record)):
                    continue
                self._records[record["transfer_id"]] = record
                self._index_record(record)
                self._summary_stats[str(path)] = stamp
            except (ValueError, TypeError, KeyError, OSError, FileTransferError, AttributeError, StopIteration):
                continue

    def _load_records(self):
        for directory in (self.root, self.history_root):
            for temporary in directory.glob("*.tmp-*"):
                lease = None
                try:
                    transfer_id, suffix = temporary.name.split(".tmp-", 1)
                    if transfer_id.endswith(".json"):
                        transfer_id = transfer_id[:-5]
                        # Schema 2 includes the destination name and repeats its
                        # owner ID. Match both before acquiring that owner's lease.
                        owner_prefix = transfer_id + "-"
                        if not suffix.startswith(owner_prefix):
                            continue
                        suffix = suffix[len(owner_prefix):]
                    self._uuid(transfer_id)
                    if len(suffix) != 32 or any(char not in "0123456789abcdef" for char in suffix):
                        continue
                    lease = self._lease({"transfer_id": transfer_id})
                    if lease.acquire():
                        path = Path(self._local(str(temporary), "write"))
                        if path.is_file():
                            path.unlink()
                except (OSError, FileTransferError, ValueError):
                    pass
                finally:
                    if lease is not None:
                        lease.close()
        self._refresh_summaries()
        for original in list(self._records.values()):
            try:
                lease = self._lease(original)
                if not lease.acquire():
                    continue
                try:
                    self._disk.cleanup_temporary(original["transfer_id"])
                    record = self._fresh_record(original)
                    self._records[record["transfer_id"]] = record
                    snapshot = self._latest(record)
                    if snapshot["state"] in _ACTIVE:
                        record = self._fresh_record(record, details=True)
                        self._records[record["transfer_id"]] = record
                        snapshot = self._latest(record)
                        discarded = record.get("stop_reason") == "discard" or self._legacy_cancel(record)
                        cancelled = record.get("stop_reason") == "user_cancel" or discarded
                        snapshot.update(state="cancelled" if cancelled else "failed", phase="interrupted",
                                        resumable=not discarded, cancellable=False, finished_at=utc_now_rfc3339())
                        snapshot["errors"].append({"code": "interrupted", "message": "Previous service process stopped before this attempt completed", "retryable": not discarded})
                        if discarded:
                            record["cleanup_pending"] = True
                            snapshot["cleanup"] = {"complete": False, "residual_paths": [entry["stage"] for entry in record["files"] if entry.get("stage") and entry["phase"] != "published"], "errors": []}
                        try:
                            receipt = self.history_root / (snapshot["job_id"] + ".files.jsonl")
                            if receipt.exists():
                                self._disk.recover_receipt_index(record, snapshot["job_id"])
                                _page, count = self._disk.read_receipt_page(snapshot["job_id"], 0, 1)
                            else:
                                self._disk.write_receipt(record, snapshot["job_id"],
                                                         (self._file_result(item) for item in record["files"]))
                                count = len(record["files"])
                            snapshot.pop("files", None)
                            snapshot["files_receipt"] = True
                            snapshot["files_total"] = count
                        except (OSError, FileTransferError):
                            # An incomplete historical receipt must not leave a
                            # dead writer permanently marked as active.
                            snapshot["errors"].append({"code": "receipt_recovery_failed",
                                                       "message": "The interrupted attempt's file receipt is unavailable",
                                                       "retryable": False})
                        self._persist(record)
                    if (record.get("stop_reason") == "discard" or self._legacy_cancel(record)) and record.get("cleanup_pending"):
                        record = self._fresh_record(record, details=True)
                        self._records[record["transfer_id"]] = record
                        snapshot = self._latest(record)
                        cleanup = self._cleanup(record, self.config_store.load(require_connection=False))
                        record["cleanup_pending"] = not cleanup["complete"]
                        snapshot["cleanup"] = cleanup
                        snapshot["resumable"] = False
                        if cleanup["complete"]:
                            self._compact_checkpoint(record)
                        self._persist(record)
                    elif record.get("archived"):
                        self._record_path(record["transfer_id"]).unlink(missing_ok=True)
                        self._disk.remove_active_data(record["transfer_id"])
                finally:
                    lease.close()
            except (ValueError, TypeError, KeyError, OSError, FileTransferError, AttributeError, StopIteration):
                # Invalid external JSON must never authorize cleanup or transfer.
                continue

    def _check_capacity(self):
        if self._stopping:
            raise FileTransferError("service_stopping", "The file transfer service is stopping")
        if len(self._runtimes) >= 2:
            raise FileTransferError("too_many_jobs", "At most two file transfer attempts can run simultaneously", retryable=True)

    def _idempotent(self, request_id, fingerprint):
        if request_id is None:
            return None
        indexed = self._request_index.get(request_id)
        if indexed is not None:
            previous_fingerprint, _transfer_id, job_id = indexed
            if previous_fingerprint != fingerprint:
                raise FileTransferError("idempotency_conflict", "client_request_id belongs to a different request")
            record, snapshot = self._find_job(job_id)
            return self._public_summary(record, snapshot)
        return None

    def _find_job(self, job_id):
        self._uuid(job_id)
        if job_id not in self._job_index:
            self._refresh_summaries()
        transfer_id = self._job_index.get(job_id)
        if transfer_id is not None:
            record = self._records[transfer_id]
            if record.get("latest_job_id") not in self._runtimes:
                record = self._fresh_record(record)
                self._records[transfer_id] = record
                self._index_record(record)
            for snapshot in record["attempts"]:
                if snapshot["job_id"] == job_id:
                    return record, snapshot
        raise FileTransferError("job_not_found", "The file transfer job was not found")

    def _fresh_record(self, record, *, details=False):
        candidates = []
        for archived in (False, True):
            path = self._record_path(record["transfer_id"], archived)
            try:
                self.policy.resolve_path(path, access="read")
                candidate = self._read_record(path)
                if candidate["transfer_id"] == record["transfer_id"]:
                    candidates.append((candidate, path))
            except FileNotFoundError:
                continue
            except (ValueError, TypeError, KeyError, FileTransferError, AttributeError) as error:
                raise FileTransferError("invalid_checkpoint", "The persisted transfer record is invalid") from error
        if not candidates:
            return record
        # A terminal result may be newer in memory when the final disk write
        # failed. Do not make that attempt appear active again to its caller.
        newest, newest_path = max(candidates, key=lambda item: self._record_version(item[0]))
        if self._prefer_memory(record, newest):
            return record
        if details:
            # Compare durable headers before reading mutable data. An older
            # active header may outlive the data retired by a newer archive.
            return self._read_record(newest_path, details=True)
        return newest

    def _get_record(self, transfer_id):
        self._uuid(transfer_id)
        if transfer_id not in self._records:
            self._refresh_summaries()
            if transfer_id not in self._records:
                raise FileTransferError("transfer_not_found", "The file transfer was not found")
        return self._records[transfer_id]

    @staticmethod
    def _latest(record):
        return next(item for item in reversed(record["attempts"]) if item["job_id"] == record["latest_job_id"])

    @staticmethod
    def _endpoint(config):
        return {"host": str(config.get("host", "")).casefold(), "port": config.get("port", 22),
                "username": config.get("username", "")}

    @staticmethod
    def _digest(value):
        return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False).encode("utf-8")).hexdigest()

    @staticmethod
    def _validate_request_id(value):
        if value is not None and (not isinstance(value, str) or not value.strip() or len(value) > 256):
            raise FileTransferError("invalid_request_id", "client_request_id must contain 1 through 256 characters")

    @staticmethod
    def _uuid(value):
        try:
            if not isinstance(value, str) or str(uuid.UUID(value)) != value or uuid.UUID(value).version != 4:
                raise ValueError()
        except (ValueError, TypeError, AttributeError) as error:
            raise FileTransferError("invalid_id", "Expected a canonical UUIDv4 identifier") from error

    @staticmethod
    def _close_session(session):
        try:
            session.close()
        except Exception:
            pass

    @staticmethod
    def _unsupported_rename(error):
        return getattr(error, "errno", None) in {errno.EOPNOTSUPP, errno.ENOSYS} or str(error).strip().casefold() in {
            "operation unsupported", "operation not supported", "unsupported operation"
        }

    @staticmethod
    def _error(error, secrets=()):
        if isinstance(error, FileTransferError):
            result = {"code": error.code, "message": str(error), "retryable": bool(getattr(error, "retryable", False))}
            details = getattr(error, "details", None)
            if details:
                result["details"] = copy.deepcopy(details)
            return public_value(result, secrets)
        if isinstance(error, PermissionError):
            return {"code": "permission_denied", "message": "A transfer path or operation was denied", "retryable": False}
        if isinstance(error, (OSError, EOFError)):
            return {"code": "transfer_io_error", "message": "File transfer I/O failed. Check the connection and file access, then resume", "retryable": True}
        return {"code": "transfer_failed", "message": "File transfer failed before completion", "retryable": False}
