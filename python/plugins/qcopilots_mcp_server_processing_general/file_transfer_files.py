"""Path validation and immutable source inventories for SSH file transfers."""

from __future__ import annotations

import errno
import hashlib
import os
import re
import stat
from pathlib import Path
from typing import Any, Callable

from qcopilots_common.security_policy import FilesystemPolicy

from .file_transfer_errors import FileTransferError


_WINDOWS_DRIVE = re.compile(r"^/?([A-Za-z]):/")
_WINDOWS_RESERVED = re.compile(
    r"^(?:CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[1-9¹²³]|LPT[1-9¹²³])$",
    re.IGNORECASE,
)
_HASH_CHUNK_SIZE = 1024 * 1024


def _check_cancel(cancel_check: Callable[[], None] | None) -> None:
    if cancel_check is not None:
        cancel_check()


def _component(name: str, *, windows: bool = False) -> str:
    if (
        not isinstance(name, str)
        or not name
        or name in {".", ".."}
        or any(character in name for character in "/\\*?")
        or any(ord(character) < 32 or ord(character) == 127 for character in name)
    ):
        raise FileTransferError("invalid_path", "A path component is empty or unsafe")
    if windows and (
        any(character in name for character in '<>:"|')
        or name.endswith((".", " "))
        or _WINDOWS_RESERVED.fullmatch(name.split(".", 1)[0].rstrip(" ."))
    ):
        raise FileTransferError("invalid_path", "A path component is not a safe Windows name")
    return name


def normalize_remote(path: str) -> str:
    """Return a lexical absolute SFTP path, without resolving server links."""

    if not isinstance(path, str) or not path:
        raise FileTransferError("invalid_path", "An absolute remote path is required")
    if any(ord(character) < 32 or ord(character) == 127 for character in path):
        raise FileTransferError("invalid_path", "Control characters are not allowed in paths")
    slashed = path.replace("\\", "/")
    if slashed.startswith(("//", "/??/")):
        raise FileTransferError("invalid_path", "UNC and device paths are not supported")
    match = _WINDOWS_DRIVE.match(slashed)
    if match:
        root = f"{match.group(1).upper()}:/"
        tail = slashed[match.end() :]
        windows = True
    elif re.match(r"^/?[A-Za-z]:", slashed):
        raise FileTransferError("invalid_path", "Drive-relative remote paths are not supported")
    elif path.startswith("/") and "\\" not in path:
        root, tail, windows = "/", path[1:], False
    else:
        raise FileTransferError("invalid_path", "An absolute remote path is required")
    parts = []
    for component in tail.split("/"):
        if component in {"", "."}:
            continue
        parts.append(_component(component, windows=windows))
    return root + "/".join(parts)


def remote_join(parent: str, name: str) -> str:
    """Append one validated directory-entry name to an absolute remote path."""

    parent = normalize_remote(parent)
    _component(name, windows=bool(_WINDOWS_DRIVE.match(parent)))
    return normalize_remote(parent.rstrip("/") + "/" + name)


def remote_not_found(exception: BaseException) -> bool:
    """Recognize only the protocol/backend's explicit ENOENT result."""

    return isinstance(exception, OSError) and exception.errno == errno.ENOENT


def _regular_kind(metadata: Any, path: str) -> str:
    mode = getattr(metadata, "st_mode", None)
    if not isinstance(mode, int):
        raise FileTransferError("unsupported_file_type", f"File type metadata is missing: {path}")
    if stat.S_ISLNK(mode) or (
        getattr(metadata, "st_file_attributes", 0)
        & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    ):
        raise FileTransferError("unsafe_path", f"Links and reparse points are not supported: {path}")
    if stat.S_ISDIR(mode):
        return "directory"
    if stat.S_ISREG(mode):
        return "file"
    raise FileTransferError("unsupported_file_type", f"Only ordinary files and directories are supported: {path}")


def _remote_components(path: str) -> list[str]:
    match = _WINDOWS_DRIVE.match(path)
    root = path[: match.end()] if match else "/"
    result = [root]
    current = root
    for name in path[len(root) :].split("/"):
        if name:
            current = current.rstrip("/") + "/" + name
            result.append(current)
    return result


def validate_remote_path(sftp: Any, path: str, allow_missing: bool = False) -> Any:
    """Use lstat on every existing component and return the final attributes.

    An absent destination returns None only when allow_missing is true. Permission,
    transport and other server failures remain visible to the caller.
    """

    components = _remote_components(normalize_remote(path))
    metadata = None
    for index, component in enumerate(components):
        try:
            metadata = sftp.lstat(component)
        except OSError as error:
            if allow_missing and remote_not_found(error):
                return None
            raise
        kind = _regular_kind(metadata, component)
        if index < len(components) - 1 and kind != "directory":
            raise FileTransferError("invalid_path", f"A path ancestor is not a directory: {component}")
    return metadata


def sha256_stream(
    handle: Any,
    limit: int | None = None,
    cancel_check: Callable[[], None] | None = None,
) -> tuple[str, int]:
    """Hash binary data from the handle's current position, at most limit bytes."""

    if limit is not None and (isinstance(limit, bool) or not isinstance(limit, int) or limit < 0):
        raise ValueError("The hash byte limit must be a non-negative integer")
    digest = hashlib.sha256()
    count = 0
    _check_cancel(cancel_check)
    while limit is None or count < limit:
        _check_cancel(cancel_check)
        length = _HASH_CHUNK_SIZE if limit is None else min(_HASH_CHUNK_SIZE, limit - count)
        chunk = handle.read(length)
        if not chunk:
            break
        if not isinstance(chunk, (bytes, bytearray, memoryview)):
            raise TypeError("File hashes require a binary stream")
        if len(chunk) > length:
            raise ValueError("The stream returned more data than requested")
        digest.update(chunk)
        count += len(chunk)
    _check_cancel(cancel_check)
    return digest.hexdigest(), count


def _local_path(policy: FilesystemPolicy, value: str | Path, access: str) -> Path:
    raw = str(value)
    slashed = raw.replace("\\", "/")
    if (
        slashed.startswith(("//", "/??/"))
        or any(ord(character) < 32 or ord(character) == 127 for character in raw)
    ):
        raise FileTransferError("invalid_path", "UNC, device and control-character paths are not supported")
    path = Path(raw)
    if not raw or not path.is_absolute():
        raise FileTransferError("invalid_path", "An absolute local path is required")
    if os.name == "nt":
        for component in path.parts[1:]:
            _component(component, windows=True)
        # Query the local drive table before lstat could touch a mapped network drive.
        import ctypes

        drive_type = ctypes.windll.kernel32.GetDriveTypeW
        drive_type.argtypes = [ctypes.c_wchar_p]
        drive_type.restype = ctypes.c_uint
        if drive_type(path.anchor) == 4:
            raise FileTransferError("unsafe_path", "Mapped network drives are not supported")
    # Inspect the original spelling before Path.resolve can follow a reparse point.
    components = list(reversed(path.parents)) + [path]
    for index, component in enumerate(components):
        try:
            metadata = component.lstat()
        except FileNotFoundError:
            break
        kind = _regular_kind(metadata, str(component))
        if index < len(components) - 1 and kind != "directory":
            raise FileTransferError("invalid_path", f"A path ancestor is not a directory: {component}")
    return policy.resolve_path(path, access=access)


def _source_key(path: str, *, local: bool) -> str:
    if local:
        return path.replace("\\", "/").casefold()
    return path.casefold() if _WINDOWS_DRIVE.match(path) else path


def _check_source_overlap(sources: list[str], *, local: bool) -> None:
    keys = sorted(_source_key(source, local=local).rstrip("/") for source in sources)
    for index, key in enumerate(keys):
        for previous in keys[:index]:
            if key == previous or key.startswith(previous + "/"):
                raise FileTransferError("overlapping_sources", "Source paths overlap or are repeated")


def build_manifest(
    direction: str,
    sources: list[str],
    destination: str,
    sftp: Any,
    policy: FilesystemPolicy,
    cancel_check: Callable[[], None] | None = None,
    *,
    progress_callback: Callable[[int, str], None] | None = None,
) -> list[dict[str, Any]]:
    """Freeze mixed source trees beneath destination, preserving empty directories.

    The function performs no writes or hashing. Every directory is represented so
    callers can create the recorded hierarchy before publishing individual files.
    Callers must revalidate paths before later I/O because lstat is not an atomic
    no-follow open operation, particularly on a remote SFTP server.
    The optional progress callback receives each accepted entry's running count
    and source path after that entry has been added to the inventory.
    """

    if direction not in {"upload", "download"}:
        raise FileTransferError("invalid_direction", "Transfer direction must be upload or download")
    if not isinstance(sources, (list, tuple)) or not sources or any(not isinstance(item, str) for item in sources):
        raise FileTransferError("invalid_sources", "At least one absolute source path is required")
    _check_cancel(cancel_check)
    upload = direction == "upload"
    if upload:
        destination = normalize_remote(destination)
        destination_metadata = validate_remote_path(sftp, destination, allow_missing=True)
        normalized_sources = [str(_local_path(policy, source, "read")) for source in sources]
    else:
        destination = str(_local_path(policy, destination, "write"))
        try:
            destination_metadata = Path(destination).lstat()
        except FileNotFoundError:
            destination_metadata = None
        normalized_sources = [normalize_remote(source) for source in sources]
    if destination_metadata is not None and _regular_kind(destination_metadata, destination) != "directory":
        raise FileTransferError("invalid_destination", "The destination must be a directory")
    _check_source_overlap(normalized_sources, local=upload)
    manifest: list[dict[str, Any]] = []
    target_keys: set[str] = set()

    def append_entry(source: str, target: str, metadata: Any) -> str:
        kind = _regular_kind(metadata, source)
        key = _source_key(target, local=not upload)
        if key in target_keys:
            raise FileTransferError("target_collision", f"Multiple sources map to the same target: {target}")
        target_keys.add(key)
        size = getattr(metadata, "st_size", None) if kind == "file" else 0
        mtime = getattr(metadata, "st_mtime", None)
        if not isinstance(size, int) or size < 0 or not isinstance(mtime, (int, float)):
            raise FileTransferError("invalid_metadata", f"File size or timestamp metadata is missing: {source}")
        manifest.append({"kind": kind, "source": source, "target": target, "size": size, "mtime": mtime})
        if progress_callback is not None:
            progress_callback(len(manifest), source)
        return kind

    def visit_upload(source: str, target: str) -> None:
        _check_cancel(cancel_check)
        local = _local_path(policy, source, "read")
        metadata = local.lstat()
        validate_remote_path(sftp, target, allow_missing=True)
        if append_entry(str(local), target, metadata) == "directory":
            _check_cancel(cancel_check)
            # iterdir yields lexical children, whose types are checked with lstat.
            for child in sorted(local.iterdir(), key=lambda entry: (entry.name.casefold(), entry.name)):
                visit_upload(str(child), remote_join(target, child.name))

    def visit_download(source: str, target: str) -> None:
        _check_cancel(cancel_check)
        metadata = validate_remote_path(sftp, source)
        local = _local_path(policy, target, "write")
        if append_entry(source, str(local), metadata) == "directory":
            _check_cancel(cancel_check)
            names = [entry.filename for entry in sftp.listdir_attr(source)]
            for name in sorted(names):
                _component(name, windows=True)
                visit_download(remote_join(source, name), str(local / name))

    for source in sorted(normalized_sources, key=lambda item: (_source_key(item, local=upload), item)):
        _check_cancel(cancel_check)
        if upload:
            local = Path(source)
            if local == local.parent:
                raise FileTransferError("invalid_source", "A filesystem root cannot be transferred as a source")
            visit_upload(source, remote_join(destination, local.name))
        else:
            if source == "/" or re.fullmatch(r"[A-Za-z]:/", source):
                raise FileTransferError("invalid_source", "A filesystem root cannot be transferred as a source")
            name = source.rsplit("/", 1)[-1]
            _component(name, windows=True)
            visit_download(source, str(Path(destination) / name))
    return manifest
