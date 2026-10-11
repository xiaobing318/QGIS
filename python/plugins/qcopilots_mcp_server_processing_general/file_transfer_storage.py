"""Atomic, indexed storage for resumable SSH transfers and immutable results.

The job manager owns transfer leases. This module never acquires a lease and
never interprets a remote path as a local filesystem path.
"""

from __future__ import annotations

import copy
import errno
import hashlib
import json
import math
import os
import re
import shutil
import stat
import threading
import time
import uuid
from pathlib import Path

from .file_transfer_errors import FileTransferError


_STATIC = ("kind", "source", "target", "size", "mtime", "stage")
_DYNAMIC = (
    "source_sha256", "target_before", "verified", "phase", "committed_offset",
    "committed_prefix_sha256",
)
_PHASES = {"pending", "creating_stage", "transferring", "ready", "publish_intent", "published"}
_HEX64 = re.compile(r"[0-9a-f]{64}\Z")


class DiskTransferStore:
    """Keep summaries small while making each entry update independently durable."""

    def __init__(self, root):
        self.root = Path(os.path.abspath(root))
        self.history_root = self.root / "history"
        self.write_stats = {"write_count": 0, "bytes_written": 0}
        self._stats_lock = threading.Lock()
        self._ensure_directory(self.root)
        self._ensure_directory(self.history_root)

    @staticmethod
    def _invalid(code="invalid_checkpoint"):
        return FileTransferError(code, "The persisted transfer data is missing, invalid or unsafe")

    @classmethod
    def _uuid(cls, value, code="invalid_checkpoint"):
        try:
            if not isinstance(value, str) or str(uuid.UUID(value)) != value:
                raise ValueError
        except (ValueError, AttributeError, TypeError) as error:
            raise cls._invalid(code) from error
        return value

    @staticmethod
    def _clean(value):
        if isinstance(value, dict):
            return {key: DiskTransferStore._clean(item) for key, item in value.items()
                    if isinstance(key, str) and not key.startswith("_")}
        if isinstance(value, (list, tuple)):
            return [DiskTransferStore._clean(item) for item in value]
        return value

    @classmethod
    def _encode(cls, value, code="invalid_checkpoint"):
        try:
            return json.dumps(value, ensure_ascii=False, allow_nan=False,
                              sort_keys=True, separators=(",", ":")).encode("utf-8")
        except (ValueError, TypeError, UnicodeError) as error:
            raise cls._invalid(code) from error

    @classmethod
    def _decode(cls, value, code="invalid_checkpoint"):
        def reject_constant(_value):
            raise ValueError("Non-finite JSON value")

        def unique_keys(pairs):
            result = {}
            for key, item in pairs:
                if key in result:
                    raise ValueError("Duplicate JSON key")
                result[key] = item
            return result

        try:
            return json.loads(value, parse_constant=reject_constant, object_pairs_hook=unique_keys)
        except (ValueError, TypeError, UnicodeError) as error:
            raise cls._invalid(code) from error

    @classmethod
    def _ordinary(cls, metadata, directory, code="invalid_checkpoint"):
        if (stat.S_ISLNK(metadata.st_mode)
                or getattr(metadata, "st_file_attributes", 0)
                & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
                or not (stat.S_ISDIR(metadata.st_mode) if directory else stat.S_ISREG(metadata.st_mode))):
            raise cls._invalid(code)

    def _guard(self, path, *, directory=False, missing=False, code="invalid_checkpoint"):
        """Check lexical containment and every existing component without resolving links."""
        path = Path(path)
        if not path.is_absolute() or ".." in path.parts:
            raise self._invalid(code)
        try:
            path.relative_to(self.root)
        except ValueError as error:
            raise self._invalid(code) from error
        for component in [*reversed(path.parents), path]:
            try:
                metadata = component.lstat()
            except FileNotFoundError:
                if missing:
                    continue
                raise
            self._ordinary(metadata, component != path or directory, code)
        return path

    def _ensure_directory(self, path, code="invalid_checkpoint"):
        path = self._guard(path, directory=True, missing=True, code=code)
        path.mkdir(parents=True, exist_ok=True)
        self._guard(path, directory=True, code=code)

    @classmethod
    def _read_descriptor(cls, path, code):
        if os.name != "nt":
            return os.open(path, os.O_RDONLY | getattr(os, "O_BINARY", 0)
                           | getattr(os, "O_NOFOLLOW", 0))
        import ctypes
        import msvcrt

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        create = kernel.CreateFileW
        create.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                           ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        create.restype = ctypes.c_void_p
        close = kernel.CloseHandle
        close.argtypes = [ctypes.c_void_p]
        close.restype = ctypes.c_int
        information = kernel.GetFileInformationByHandleEx
        information.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
        information.restype = ctypes.c_int
        # Share deletion for checkpoint cleanup and native rename operations.
        # OPEN_REPARSE_POINT and handle attributes preserve the no-links rule.
        handle = create(str(path), 0x80000000, 0x1 | 0x2 | 0x4, None, 3, 0x00200000, None)
        if handle == ctypes.c_void_p(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            attributes = (ctypes.c_uint32 * 2)()  # FILE_ATTRIBUTE_TAG_INFO
            if not information(handle, 9, attributes, ctypes.sizeof(attributes)):
                raise ctypes.WinError(ctypes.get_last_error())
            if attributes[0] & (0x400 | 0x10):  # Reparse point or directory
                raise cls._invalid(code)
            return msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
        except BaseException:
            close(handle)
            raise

    def _open_read(self, path, code="invalid_checkpoint"):
        path = self._guard(path, code=code)
        descriptor = self._read_descriptor(path, code)
        try:
            self._ordinary(os.fstat(descriptor), False, code)
            self._guard(path, code=code)
            return os.fdopen(descriptor, "rb")
        except BaseException:
            os.close(descriptor)
            raise

    def _read_json(self, path, code="invalid_checkpoint", *, allow_missing=False):
        try:
            with self._open_read(path, code) as handle:
                encoded = handle.read()
            value = self._decode(encoded, code)
            if not isinstance(value, dict):
                raise self._invalid(code)
            return value
        except FileNotFoundError as error:
            if allow_missing:
                raise
            raise self._invalid(code) from error
        except OSError as error:
            raise self._invalid(code) from error

    def _atomic(self, path, transfer_id, writer, *, immutable=False, code="invalid_checkpoint"):
        self._uuid(transfer_id, code)
        self._ensure_directory(path.parent, code)
        self._guard(path, missing=True, code=code)
        temporary = path.with_name(path.name + ".tmp-" + transfer_id + "-" + uuid.uuid4().hex)
        try:
            with temporary.open("xb") as handle:
                try:
                    result = writer(handle)
                    handle.flush()
                    os.fsync(handle.fileno())
                finally:
                    # Count physical staging writes too, including retries which
                    # compare equal and replacements that fail after the write.
                    size = handle.tell()
                    with self._stats_lock:
                        self.write_stats["write_count"] += 1
                        self.write_stats["bytes_written"] += size
            self._guard(path, missing=True, code=code)
            if immutable and path.exists():
                with self._open_read(path, code) as previous, self._open_read(temporary, code) as proposed:
                    while True:
                        old = previous.read(1024 * 1024)
                        new = proposed.read(1024 * 1024)
                        if old != new:
                            raise self._invalid(code)
                        if not old:
                            return result
            # Windows can briefly refuse replacement while a reader still has
            # the destination open. Keep the original and bound the retry time.
            deadline = time.monotonic() + 0.5
            while True:
                try:
                    os.replace(temporary, path)
                    break
                except OSError as error:
                    remaining = deadline - time.monotonic()
                    if (os.name != "nt" or getattr(error, "winerror", None) not in {5, 32, 33}
                            or remaining <= 0):
                        raise
                    time.sleep(min(0.02, remaining))
                    self._guard(temporary, code=code)
                    self._guard(path, missing=True, code=code)
            return result
        finally:
            if temporary.exists():
                self._guard(temporary, code=code)
                temporary.unlink()

    def _write_json(self, path, value, transfer_id, *, immutable=False, code="invalid_checkpoint"):
        encoded = self._encode(value, code)
        self._atomic(path, transfer_id, lambda handle: self._write_all(handle, encoded),
                     immutable=immutable, code=code)

    @staticmethod
    def _write_all(handle, encoded):
        remaining = memoryview(encoded)
        while remaining:
            count = handle.write(remaining)
            if type(count) is not int or not 0 < count <= len(remaining):
                raise OSError(errno.EIO, "Checkpoint write made no progress")
            remaining = remaining[count:]

    def _active_root(self, transfer_id):
        return self.root / self._uuid(transfer_id)

    def _state_path(self, transfer_id, index):
        if isinstance(index, bool) or not isinstance(index, int) or index < 0:
            raise self._invalid()
        return self._active_root(transfer_id) / "states" / (str(index) + ".json")

    def write_summary(self, record):
        transfer_id = self._uuid(record.get("transfer_id"))
        # Do not even copy a manifest while writing a small progress summary.
        # Traversing it here would restore quadratic work for many small files.
        excluded = {"files", "created_directories", "directory_intents", "attempts"}
        summary = {key: value for key, value in record.items() if key not in excluded}
        attempts = []
        for attempt in record.get("attempts", []):
            if not isinstance(attempt, dict):
                raise self._invalid()
            attempts.append({key: value for key, value in attempt.items() if key != "files"})
        summary["attempts"] = attempts
        summary = self._clean(summary)
        destination = self.history_root if record.get("archived", False) else self.root
        self._write_json(destination / (transfer_id + ".json"), summary, transfer_id)

    def read_summary(self, path):
        path = Path(path)
        if path.parent not in (self.root, self.history_root) or path.suffix != ".json":
            raise self._invalid()
        transfer_id = self._uuid(path.stem)
        summary = self._read_json(path, allow_missing=True)
        if (summary.get("transfer_id") != transfer_id or type(summary.get("schema_version")) is not int
                or summary["schema_version"] not in (1, 2)):
            raise self._invalid()
        return summary

    @classmethod
    def _static_entry(cls, entry):
        if not isinstance(entry, dict) or any(key not in entry for key in _STATIC):
            raise cls._invalid()
        value = {key: entry[key] for key in _STATIC}
        try:
            valid_mtime = (not isinstance(value["mtime"], bool)
                           and isinstance(value["mtime"], (int, float))
                           and math.isfinite(value["mtime"]))
        except OverflowError:
            valid_mtime = False
        if (not isinstance(value["kind"], str) or value["kind"] not in {"file", "directory"}
                or any(not isinstance(value[key], str) or not value[key] for key in ("source", "target"))
                or isinstance(value["size"], bool) or not isinstance(value["size"], int) or value["size"] < 0
                or not valid_mtime
                or (value["kind"] == "file" and (not isinstance(value["stage"], str) or not value["stage"]))
                or (value["kind"] == "directory" and value["stage"] is not None)):
            raise cls._invalid()
        try:
            for key in ("source", "target", "stage"):
                if value[key] is not None:
                    value[key].encode("utf-8")
        except UnicodeError as error:
            raise cls._invalid() from error
        return value

    @classmethod
    def _dynamic_entry(cls, entry):
        if not isinstance(entry, dict) or any(key not in entry for key in _DYNAMIC):
            raise cls._invalid()
        value = {key: copy.deepcopy(entry[key]) for key in _DYNAMIC}
        if (not isinstance(value["verified"], bool) or not isinstance(value["phase"], str)
                or value["phase"] not in _PHASES
                or isinstance(value["committed_offset"], bool)
                or not isinstance(value["committed_offset"], int) or value["committed_offset"] < 0):
            raise cls._invalid()
        for key in ("source_sha256", "committed_prefix_sha256"):
            if value[key] is not None and (not isinstance(value[key], str) or not _HEX64.fullmatch(value[key])):
                raise cls._invalid()
        before = value["target_before"]
        if before is not None:
            if (not isinstance(before, dict) or isinstance(before.get("size"), bool)
                    or not isinstance(before.get("size"), int) or before["size"] < 0
                    or not isinstance(before.get("sha256"), str) or not _HEX64.fullmatch(before["sha256"])):
                raise cls._invalid()
            value["target_before"] = {"size": before["size"], "sha256": before["sha256"]}
        return value

    def _write_lines(self, path, index_path, transfer_id, rows, identity, code="invalid_checkpoint"):
        def write(handle):
            offsets, digests = [0], []
            for row in rows:
                encoded = self._encode(row, code) + b"\n"
                self._write_all(handle, encoded)
                offsets.append(handle.tell())
                digests.append(hashlib.sha256(encoded).hexdigest())
            return {"version": 1, **identity, "count": len(digests),
                    "offsets": offsets, "sha256": digests}

        index = self._atomic(path, transfer_id, write, immutable=True, code=code)
        self._write_json(index_path, index, transfer_id, immutable=True, code=code)
        return index["count"]

    def write_manifest(self, record):
        transfer_id = self._uuid(record.get("transfer_id"))
        entries = record.get("files")
        if not isinstance(entries, list):
            raise self._invalid()
        count = record.get("manifest_count", len(entries))
        if isinstance(count, bool) or not isinstance(count, int) or count != len(entries):
            raise self._invalid()
        root = self._active_root(transfer_id)
        self._write_lines(root / "manifest.jsonl", root / "offsets.json", transfer_id,
                          (self._static_entry(entry) for entry in entries), {"transfer_id": transfer_id})
        for index, entry in enumerate(entries):
            path = self._state_path(transfer_id, index)
            if path.exists():
                self._read_state(transfer_id, index, entry)
            else:
                self.write_entry(record, index, entry)
        record["manifest_count"] = count

    def write_entry(self, record, index, entry):
        transfer_id = self._uuid(record.get("transfer_id"))
        path = self._state_path(transfer_id, index)
        static_entry = self._static_entry(entry)
        values = self._dynamic_entry(entry)
        count = record.get("manifest_count", len(record.get("files", [])))
        if (isinstance(count, bool) or not isinstance(count, int) or index >= count
                or values["committed_offset"] > static_entry["size"]):
            raise self._invalid()
        self._write_json(path, {"version": 1, "transfer_id": transfer_id,
                               "index": index, "values": values}, transfer_id)

    def _read_state(self, transfer_id, index, entry):
        state = self._read_json(self._state_path(transfer_id, index))
        if (type(state.get("version")) is not int or state["version"] != 1
                or state.get("transfer_id") != transfer_id
                or isinstance(state.get("index"), bool) or state.get("index") != index):
            raise self._invalid()
        values = self._dynamic_entry(state.get("values"))
        if values["committed_offset"] > entry["size"]:
            raise self._invalid()
        return {**entry, **values}

    @classmethod
    def _page_arguments(cls, offset, limit, code):
        if (isinstance(offset, bool) or not isinstance(offset, int) or offset < 0
                or isinstance(limit, bool) or not isinstance(limit, int) or limit < 1):
            raise cls._invalid(code)

    def _read_lines(self, path, index_path, identity, offset, limit, code="invalid_checkpoint"):
        self._page_arguments(offset, limit, code)
        index = self._read_json(index_path, code)
        count, positions, digests = index.get("count"), index.get("offsets"), index.get("sha256")
        if (type(index.get("version")) is not int or index["version"] != 1
                or any(index.get(key) != value for key, value in identity.items())
                or isinstance(count, bool) or not isinstance(count, int) or count < 0
                or not isinstance(positions, list) or len(positions) != count + 1 or positions[0] != 0
                or any(isinstance(item, bool) or not isinstance(item, int) or item < 0 for item in positions)
                or any(left >= right for left, right in zip(positions, positions[1:]))
                or not isinstance(digests, list) or len(digests) != count
                or any(not isinstance(item, str) or not _HEX64.fullmatch(item) for item in digests)):
            raise self._invalid(code)
        self._uuid(index.get("transfer_id"), code)
        try:
            with self._open_read(path, code) as handle:
                if os.fstat(handle.fileno()).st_size != positions[-1]:
                    raise self._invalid(code)
                rows = []
                for position in range(offset, min(offset + limit, count)):
                    handle.seek(positions[position])
                    encoded = handle.read(positions[position + 1] - positions[position])
                    if not encoded.endswith(b"\n") or hashlib.sha256(encoded).hexdigest() != digests[position]:
                        raise self._invalid(code)
                    row = self._decode(encoded, code)
                    if not isinstance(row, dict):
                        raise self._invalid(code)
                    rows.append(row)
                return rows, count
        except OSError as error:
            raise self._invalid(code) from error

    def read_manifest_page(self, record, offset, limit):
        transfer_id = self._uuid(record.get("transfer_id"))
        root = self._active_root(transfer_id)
        rows, count = self._read_lines(root / "manifest.jsonl", root / "offsets.json",
                                       {"transfer_id": transfer_id}, offset, limit)
        expected_count = record.get("manifest_count", count)
        if type(expected_count) is not int or expected_count != count:
            raise self._invalid()
        return [self._read_state(transfer_id, offset + index, self._static_entry(row))
                for index, row in enumerate(rows)], count

    def write_directory(self, record, path, owned):
        transfer_id = self._uuid(record.get("transfer_id"))
        if not isinstance(path, str) or not path or (owned is not None and not isinstance(owned, bool)):
            raise self._invalid()
        name = self._directory_name(path)
        target = self._active_root(transfer_id) / "directories" / name
        if owned is None:
            self._guard(target, missing=True)
            target.unlink(missing_ok=True)
        else:
            self._write_json(target, {"version": 1, "transfer_id": transfer_id,
                                      "path": path, "owned": owned}, transfer_id)

    @classmethod
    def _directory_name(cls, path):
        try:
            return hashlib.sha256(path.encode("utf-8")).hexdigest() + ".json"
        except UnicodeError as error:
            raise cls._invalid() from error

    def load_details(self, record):
        transfer_id = self._uuid(record.get("transfer_id"))
        result = dict(record)
        count = result.get("manifest_count", 0)
        if isinstance(count, bool) or not isinstance(count, int) or count < 0:
            raise self._invalid()
        if result.get("manifest_ready", False) or count:
            files, _total = self.read_manifest_page(record, 0, max(count, 1))
        else:
            files = []
        created, intents = [], []
        directory = self._active_root(transfer_id) / "directories"
        self._guard(directory, directory=True, missing=True)
        if directory.exists():
            for path in directory.iterdir():
                if ".tmp-" in path.name:
                    continue
                self._guard(path)
                value = self._read_json(path)
                remote_path = value.get("path")
                if (type(value.get("version")) is not int or value["version"] != 1
                        or value.get("transfer_id") != transfer_id
                        or not isinstance(remote_path, str) or not remote_path
                        or not isinstance(value.get("owned"), bool)
                        or path.name != self._directory_name(remote_path)):
                    raise self._invalid()
                (created if value["owned"] else intents).append(remote_path)
        key = lambda value: (len(value.replace("\\", "/").split("/")), value)
        result.update(files=files, created_directories=sorted(created, key=key),
                      directory_intents=sorted(intents, key=key), _details_loaded=True)
        return result

    def write_receipt(self, record, job_id, files):
        transfer_id = self._uuid(record.get("transfer_id"), "invalid_receipt")
        job_id = self._uuid(job_id, "invalid_receipt")

        def rows():
            try:
                iterator = iter(files)
            except TypeError as error:
                raise self._invalid("invalid_receipt") from error
            for item in iterator:
                if not isinstance(item, dict):
                    raise self._invalid("invalid_receipt")
                yield self._clean(item)

        self._write_lines(self.history_root / (job_id + ".files.jsonl"),
                          self.history_root / (job_id + ".files.offsets.json"), transfer_id,
                          rows(),
                          {"transfer_id": transfer_id, "job_id": job_id}, "invalid_receipt")

    def read_receipt_page(self, job_id, offset, limit):
        job_id = self._uuid(job_id, "invalid_receipt")
        return self._read_lines(self.history_root / (job_id + ".files.jsonl"),
                                self.history_root / (job_id + ".files.offsets.json"),
                                {"job_id": job_id}, offset, limit, "invalid_receipt")

    def recover_receipt_index(self, record, job_id):
        """Finish an interrupted receipt commit using only its immutable bytes.

        The caller holds the transfer lease. An existing index, including a
        corrupt one, is never replaced or repaired by this recovery path.
        """
        code = "invalid_receipt"
        transfer_id = self._uuid(record.get("transfer_id"), code)
        job_id = self._uuid(job_id, code)
        path = self.history_root / (job_id + ".files.jsonl")
        index_path = self.history_root / (job_id + ".files.offsets.json")
        self._guard(index_path, missing=True, code=code)
        if index_path.exists():
            return
        positions, digests = [0], []
        try:
            with self._open_read(path, code) as handle:
                before = os.fstat(handle.fileno())
                for encoded in handle:
                    if not encoded.endswith(b"\n") or not isinstance(self._decode(encoded, code), dict):
                        raise self._invalid(code)
                    positions.append(positions[-1] + len(encoded))
                    digests.append(hashlib.sha256(encoded).hexdigest())
                after = os.fstat(handle.fileno())
                if (positions[-1] != before.st_size or after.st_size != before.st_size
                        or after.st_mtime_ns != before.st_mtime_ns):
                    raise self._invalid(code)
        except OSError as error:
            raise self._invalid(code) from error
        attempt = next((item for item in record.get("attempts", [])
                        if isinstance(item, dict) and item.get("job_id") == job_id), {})
        if record.get("manifest_ready") and "files_total" in attempt:
            expected = attempt["files_total"]
            if job_id == record.get("latest_job_id"):
                # Progress may still contain its initial zero immediately after
                # inventory commit. The frozen manifest is authoritative then.
                expected = record.get("manifest_count", expected)
            if type(expected) is not int or expected < 0 or expected != len(digests):
                raise self._invalid(code)
        self._write_json(index_path, {"version": 1, "transfer_id": transfer_id,
                                      "job_id": job_id, "count": len(digests),
                                      "offsets": positions, "sha256": digests},
                         transfer_id, immutable=True, code=code)

    def _walk(self, directory):
        self._guard(directory, directory=True, missing=True)
        if not directory.exists():
            return
        for path in directory.iterdir():
            metadata = path.lstat()
            if stat.S_ISDIR(metadata.st_mode):
                self._guard(path, directory=True)
                yield from self._walk(path)
            else:
                self._guard(path)
                yield path

    def remove_active_data(self, transfer_id):
        directory = self._active_root(transfer_id)
        self._guard(directory, directory=True, missing=True)
        if directory.exists():
            # Validate the entire tree before deleting anything, including empty
            # directories. shutil.rmtree must never be given a reparse target.
            list(self._walk(directory))
            self._guard(directory, directory=True)
            resolved = directory.resolve(strict=True)
            if resolved.parent != self.root.resolve(strict=True):
                raise self._invalid()
            shutil.rmtree(resolved)

    def cleanup_temporary(self, transfer_id):
        transfer_id = self._uuid(transfer_id)
        current = re.compile(r"(.+)\.tmp-" + re.escape(transfer_id) + r"-[0-9a-f]{32}\Z")
        legacy = re.compile(re.escape(transfer_id) + r"\.tmp-[0-9a-f]{32}\Z")
        receipt = re.compile(r"[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}"
                             r"\.files\.(?:jsonl|offsets\.json)\Z")
        active = self._active_root(transfer_id)

        def owned_temporary(path):
            match = current.fullmatch(path.name)
            if match is None:
                return path.parent in (self.root, self.history_root) and legacy.fullmatch(path.name)
            name = match.group(1)
            if path.parent == self.root:
                return name == transfer_id + ".json"
            if path.parent == self.history_root:
                return name == transfer_id + ".json" or receipt.fullmatch(name)
            if path.parent == active:
                return name in {"manifest.jsonl", "offsets.json"}
            if path.parent == active / "states":
                return re.fullmatch(r"(?:0|[1-9][0-9]*)\.json", name)
            if path.parent == active / "directories":
                return re.fullmatch(r"[0-9a-f]{64}\.json", name)
            return False

        paths = []
        for directory in (self.root, self.history_root):
            self._guard(directory, directory=True)
            paths.extend(path for path in directory.iterdir()
                         if owned_temporary(path))
        paths.extend(path for path in self._walk(active) if owned_temporary(path))
        for path in paths:
            self._guard(path)
            path.unlink()
