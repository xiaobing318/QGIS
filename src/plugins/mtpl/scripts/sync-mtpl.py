#!/usr/bin/env python3
# -----------------------------------------------------------
#
# Copyright (C) 2026 QGIS contributors
#
# -----------------------------------------------------------
#
# licensed under the terms of GNU GPL 2
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# ---------------------------------------------------------------------

"""Synchronize committed MTPL sources into the QGIS MTPL plugin."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import errno
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile
from typing import NoReturn


SCRIPT_PATH = Path(__file__).resolve()
CONFIG_PATH = SCRIPT_PATH.with_suffix(".json")
PLUGIN_ROOT = SCRIPT_PATH.parent.parent
VENDOR_ROOT = PLUGIN_ROOT / "vendor" / "mtpl"

STATIC_EXPORT_HEADER = b"""#ifndef MTPL_EXPORT_H
#define MTPL_EXPORT_H

/*
 * QGIS vendors MTPL as a private static library.  Upstream normally creates
 * this header with CMake's GenerateExportHeader module.  Keeping the static
 * form in the snapshot avoids a generated include directory and, more
 * importantly, prevents MTPL symbols from becoming part of the QGIS plugin
 * ABI.
 */
#define MTPL_API
#define MTPL_NO_EXPORT

#endif
"""

GENERATORS = {
    "qgis-static-export-header": STATIC_EXPORT_HEADER,
}

WINDOWS_RESERVED_NAMES = {
    "CON",
    "PRN",
    "AUX",
    "NUL",
    *(f"COM{number}" for number in range(1, 10)),
    *(f"LPT{number}" for number in range(1, 10)),
}


class SyncError(RuntimeError):
    pass


@dataclass(frozen=True)
class SyncConfig:
    source_roots: tuple[str, ...]
    generated_files: tuple[tuple[str, str], ...]


def fail(message: str) -> NoReturn:
    raise SyncError(message)


def checked_relative_path(value: object, *, field: str) -> str:
    if not isinstance(value, str) or not value:
        fail(f"{field} must be a non-empty string")

    posix_path = PurePosixPath(value)
    if (
        posix_path.is_absolute()
        or "\\" in value
        or posix_path.as_posix() != value
        or any(part in ("", ".", "..") for part in posix_path.parts)
    ):
        fail(f"unsafe {field}: {value!r}")

    for part in posix_path.parts:
        if (
            ":" in part
            or any(ord(character) < 32 for character in part)
            or part.rstrip(" .") != part
            or part.split(".", 1)[0].upper() in WINDOWS_RESERVED_NAMES
        ):
            fail(f"unsafe {field}: {value!r}")

    native_path = Path(*posix_path.parts)
    if native_path.is_absolute() or native_path.drive:
        fail(f"unsafe {field}: {value!r}")
    return value


def destination_key(relative: str) -> str:
    return "/".join(part.casefold() for part in PurePosixPath(relative).parts)


def reject_duplicate_json_keys(pairs: list[tuple[str, object]]) -> dict:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            fail(f"duplicate key in {CONFIG_PATH.name}: {key}")
        result[key] = value
    return result


def read_config() -> SyncConfig:
    try:
        raw = json.loads(
            CONFIG_PATH.read_text(encoding="utf-8"),
            object_pairs_hook=reject_duplicate_json_keys,
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        fail(f"cannot read {CONFIG_PATH}: {error}")

    if not isinstance(raw, dict):
        fail(f"{CONFIG_PATH.name} must contain a JSON object")
    expected_fields = {"schemaVersion", "sourceRoots", "generatedFiles"}
    if set(raw) != expected_fields:
        missing = sorted(expected_fields - set(raw))
        unexpected = sorted(set(raw) - expected_fields)
        details = []
        if missing:
            details.append("missing " + ", ".join(missing))
        if unexpected:
            details.append("unexpected " + ", ".join(unexpected))
        fail(f"invalid {CONFIG_PATH.name} fields: {', '.join(details)}")
    if type(raw["schemaVersion"]) is not int or raw["schemaVersion"] != 2:
        fail(f"unsupported {CONFIG_PATH.name} schema")

    raw_roots = raw["sourceRoots"]
    if not isinstance(raw_roots, list) or not raw_roots:
        fail("sourceRoots must be a non-empty array")
    source_roots = tuple(
        checked_relative_path(value, field=f"sourceRoots[{index}]")
        for index, value in enumerate(raw_roots)
    )
    if len(set(source_roots)) != len(source_roots):
        fail("sourceRoots contains duplicate paths")
    for index, root in enumerate(source_roots):
        for other in source_roots[index + 1 :]:
            if root.startswith(other + "/") or other.startswith(root + "/"):
                fail(f"sourceRoots overlap: {root!r} and {other!r}")

    raw_generated = raw["generatedFiles"]
    if not isinstance(raw_generated, list):
        fail("generatedFiles must be an array")
    generated_files: list[tuple[str, str]] = []
    generated_keys: set[str] = set()
    for index, entry in enumerate(raw_generated):
        if not isinstance(entry, dict) or set(entry) != {"path", "generator"}:
            fail(
                f"generatedFiles[{index}] must contain only path and generator"
            )
        relative = checked_relative_path(
            entry["path"], field=f"generatedFiles[{index}].path"
        )
        generator = entry["generator"]
        if not isinstance(generator, str) or generator not in GENERATORS:
            fail(f"unsupported generator for {relative}: {generator!r}")
        key = destination_key(relative)
        if key in generated_keys:
            fail(f"generatedFiles contains a destination collision: {relative}")
        generated_keys.add(key)
        generated_files.append((relative, generator))

    return SyncConfig(source_roots, tuple(generated_files))


def run_git(source: Path, arguments: list[str]) -> bytes:
    command = ["git", "-C", os.fspath(source), *arguments]
    environment = os.environ.copy()
    environment["GIT_OPTIONAL_LOCKS"] = "0"
    try:
        result = subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=environment,
        )
    except OSError as error:
        fail(f"cannot execute git: {error}")
    if result.returncode != 0:
        stderr = result.stderr.decode("utf-8", errors="replace").strip()
        detail = stderr or f"exit code {result.returncode}"
        fail(f"git command failed ({' '.join(arguments)}): {detail}")
    return result.stdout


def decode_ascii_git_output(data: bytes, *, description: str) -> str:
    try:
        return data.decode("ascii").strip()
    except UnicodeDecodeError:
        fail(f"git returned a non-ASCII {description}")


def validate_source(
    source: Path, config: SyncConfig, requested_commit: str
) -> tuple[Path, str]:
    try:
        source = source.resolve()
    except OSError as error:
        fail(f"cannot resolve MTPL source directory {source}: {error}")
    if not source.is_dir():
        fail(f"MTPL source directory does not exist: {source}")

    inside_work_tree = decode_ascii_git_output(
        run_git(source, ["rev-parse", "--is-inside-work-tree"]),
        description="work tree status",
    )
    if inside_work_tree != "true":
        fail(f"MTPL source is not a Git working tree: {source}")

    repository_output = run_git(source, ["rev-parse", "--show-toplevel"]).strip()
    try:
        repository = Path(os.fsdecode(repository_output)).resolve()
    except OSError as error:
        fail(f"cannot resolve the MTPL repository root: {error}")

    pathspecs = [f":(literal){root}" for root in config.source_roots]
    dirty = run_git(
        repository,
        [
            "status",
            "--porcelain=v1",
            "-z",
            "--untracked-files=all",
            "--",
            *pathspecs,
        ],
    )
    if dirty:
        fail(
            "MTPL source roots contain tracked or untracked changes. "
            "Commit or remove those changes before synchronizing"
        )

    resolved_commit = decode_ascii_git_output(
        run_git(
            repository,
            [
                "rev-parse",
                "--verify",
                "--end-of-options",
                f"{requested_commit}^{{commit}}",
            ],
        ),
        description="commit ID",
    )
    return repository, resolved_commit


def decode_git_path(data: bytes) -> str:
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        fail("MTPL contains a source path that is not valid UTF-8")


def list_source_blobs(
    source: Path, commit: str, config: SyncConfig
) -> dict[str, str]:
    for root in config.source_roots:
        object_type = decode_ascii_git_output(
            run_git(source, ["cat-file", "-t", f"{commit}:{root}"]),
            description=f"object type for {root}",
        )
        if object_type != "tree":
            fail(f"configured source root is not a Git tree: {root}")

    pathspecs = [f":(literal){root}" for root in config.source_roots]
    output = run_git(
        source,
        ["ls-tree", "-r", "-z", "--full-tree", commit, "--", *pathspecs],
    )
    blobs: dict[str, str] = {}
    destination_paths: dict[str, str] = {}
    for record in output.split(b"\0"):
        if not record:
            continue
        try:
            metadata, raw_path = record.split(b"\t", 1)
            raw_mode, raw_type, raw_object = metadata.split(b" ", 2)
            mode = raw_mode.decode("ascii")
            object_type = raw_type.decode("ascii")
            object_id = raw_object.decode("ascii")
        except (ValueError, UnicodeDecodeError):
            fail("git ls-tree returned an invalid record")

        relative = checked_relative_path(
            decode_git_path(raw_path), field="MTPL source path"
        )
        if not any(
            relative.startswith(root + "/") for root in config.source_roots
        ):
            fail(f"git returned a path outside the configured roots: {relative}")
        if object_type != "blob" or mode not in {"100644", "100755"}:
            fail(
                f"MTPL source entry is not an ordinary file: "
                f"{relative} ({mode} {object_type})"
            )

        key = destination_key(relative)
        if key in destination_paths:
            fail(
                "MTPL source paths collide on the destination filesystem: "
                f"{destination_paths[key]} and {relative}"
            )
        destination_paths[key] = relative
        blobs[relative] = object_id

    if not blobs:
        fail("the configured MTPL source roots contain no files")

    for relative, _generator in config.generated_files:
        key = destination_key(relative)
        if key in destination_paths:
            fail(
                "an upstream MTPL file conflicts with a generated file: "
                f"{destination_paths[key]}"
            )
        destination_paths[key] = relative
    return blobs


def write_staged_file(root: Path, relative: str, data: bytes) -> None:
    destination = root / Path(*PurePosixPath(relative).parts)
    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    except OSError as error:
        fail(f"cannot stage {relative}: {error}")


def create_candidate(
    source: Path,
    config: SyncConfig,
    blobs: dict[str, str],
    staged: Path,
) -> set[str]:
    expected_paths: set[str] = set()
    for relative in sorted(blobs):
        data = run_git(source, ["cat-file", "blob", blobs[relative]])
        write_staged_file(staged, relative, data)
        expected_paths.add(relative)
    for relative, generator in config.generated_files:
        write_staged_file(staged, relative, GENERATORS[generator])
        expected_paths.add(relative)

    actual_paths = {
        path.relative_to(staged).as_posix()
        for path in staged.rglob("*")
        if path.is_file()
    }
    if actual_paths != expected_paths:
        fail("the staged MTPL file set does not match the synchronization policy")
    return expected_paths


def read_compatibility_file(root: Path, relative: str) -> str:
    path = root / Path(*PurePosixPath(relative).parts)
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        fail(f"cannot read required MTPL file {relative}: {error}")


def validate_compatibility(root: Path) -> None:
    required_tokens = {
        "include/mtpl/mtpl.h": (
            '#define MTPL_VERSION_STRING "2.1.0"',
            "#include <mtpl/package.h>",
        ),
        "include/mtpl/package.h": (
            "MTPL_PACKAGE_INFO_INIT",
            "MTPL_TRANSCODE_OPTIONS_INIT",
            "mtpl_package_probe",
            "mtpl_package_validate_key",
            "mtpl_package_transcode",
        ),
        "include/mtpl/ptp.h": (
            "mtpl_ptp_reader_get_range_count",
            "mtpl_ptp_reader_get_range_info",
            "mtpl_ptp_reader_get_entry_info",
        ),
        "include/mtpl/sfp.h": (
            "mtpl_sfp_reader_get_entry_count",
            "mtpl_sfp_reader_get_entry_info",
        ),
        "src/sfp/sfp_internal.h": (
            "mtpl_sfp_reader_read_file_chunks_internal",
            "mtpl_sfp_internal_read_chunk_callback_t",
        ),
        "src/common/package.c": (
            "MTPL_STATUS_CANCELED",
            "mtpl_commit_file_utf8",
        ),
    }
    for relative, tokens in required_tokens.items():
        content = read_compatibility_file(root, relative)
        for token in tokens:
            if token not in content:
                fail(
                    f"MTPL 2.1 compatibility token is missing in "
                    f"{relative}: {token}"
                )


def is_directory_link(path: Path) -> bool:
    if path.is_symlink():
        return True
    is_junction = getattr(path, "is_junction", None)
    return bool(is_junction and is_junction())


def preflight_vendor_destination(expected_paths: set[str]) -> None:
    if is_directory_link(VENDOR_ROOT):
        fail(f"vendor root must not be a link: {VENDOR_ROOT}")
    if VENDOR_ROOT.exists() and not VENDOR_ROOT.is_dir():
        fail(f"vendor root is not a directory: {VENDOR_ROOT}")

    for relative in sorted(expected_paths):
        destination = VENDOR_ROOT / Path(*PurePosixPath(relative).parts)
        current = VENDOR_ROOT
        for part in PurePosixPath(relative).parts[:-1]:
            current /= part
            if is_directory_link(current):
                fail(f"vendor destination contains a directory link: {current}")
            if current.exists() and not current.is_dir():
                fail(f"vendor destination parent is not a directory: {current}")
        if is_directory_link(destination) or (
            destination.exists() and destination.is_dir()
        ):
            fail(f"vendor file destination is a directory: {destination}")


def atomically_copy_file(source: Path, destination: Path) -> None:
    temporary: Path | None = None
    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        with source.open("rb") as source_file:
            with tempfile.NamedTemporaryFile(
                mode="wb",
                prefix=f".{destination.name}.qgis-mtpl-sync-",
                dir=destination.parent,
                delete=False,
            ) as temporary_file:
                temporary = Path(temporary_file.name)
                shutil.copyfileobj(source_file, temporary_file)
                temporary_file.flush()
                os.fsync(temporary_file.fileno())
        os.replace(temporary, destination)
        temporary = None
    except OSError as error:
        fail(f"cannot replace vendor file {destination}: {error}")
    finally:
        if temporary is not None:
            try:
                temporary.unlink()
            except FileNotFoundError:
                pass
            except OSError:
                pass


def collect_vendor_entries(root: Path) -> set[str]:
    entries: set[str] = set()
    if not root.exists():
        return entries
    for current_text, directories, files in os.walk(root, followlinks=False):
        current = Path(current_text)
        for name in list(directories):
            path = current / name
            if is_directory_link(path):
                entries.add(path.relative_to(root).as_posix())
                directories.remove(name)
        for name in files:
            path = current / name
            entries.add(path.relative_to(root).as_posix())
    return entries


def remove_unmanaged_entries(expected_paths: set[str]) -> None:
    actual_paths = collect_vendor_entries(VENDOR_ROOT)
    for relative in sorted(actual_paths - expected_paths, reverse=True):
        path = VENDOR_ROOT / Path(*PurePosixPath(relative).parts)
        try:
            path.unlink()
        except OSError as error:
            fail(f"cannot remove unmanaged vendor entry {path}: {error}")

    for current_text, directories, _files in os.walk(
        VENDOR_ROOT, topdown=False, followlinks=False
    ):
        current = Path(current_text)
        for name in directories:
            directory = current / name
            if is_directory_link(directory):
                continue
            try:
                directory.rmdir()
            except OSError as error:
                if error.errno not in {errno.ENOTEMPTY, errno.EEXIST}:
                    fail(f"cannot remove empty vendor directory {directory}: {error}")


def verify_vendor(staged: Path, expected_paths: set[str]) -> None:
    actual_paths = collect_vendor_entries(VENDOR_ROOT)
    if actual_paths != expected_paths:
        missing = sorted(expected_paths - actual_paths)
        unmanaged = sorted(actual_paths - expected_paths)
        details = []
        if missing:
            details.append("missing " + ", ".join(missing))
        if unmanaged:
            details.append("unmanaged " + ", ".join(unmanaged))
        fail("MTPL vendor file set mismatch: " + ", ".join(details))

    for relative in sorted(expected_paths):
        staged_path = staged / Path(*PurePosixPath(relative).parts)
        vendor_path = VENDOR_ROOT / Path(*PurePosixPath(relative).parts)
        try:
            if staged_path.read_bytes() != vendor_path.read_bytes():
                fail(f"MTPL vendor file differs after replacement: {relative}")
        except OSError as error:
            fail(f"cannot verify MTPL vendor file {relative}: {error}")


def replace_vendor(staged: Path, expected_paths: set[str]) -> None:
    preflight_vendor_destination(expected_paths)
    try:
        VENDOR_ROOT.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        fail(f"cannot create vendor root {VENDOR_ROOT}: {error}")

    for relative in sorted(expected_paths):
        source = staged / Path(*PurePosixPath(relative).parts)
        destination = VENDOR_ROOT / Path(*PurePosixPath(relative).parts)
        atomically_copy_file(source, destination)
    remove_unmanaged_entries(expected_paths)
    verify_vendor(staged, expected_paths)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog=SCRIPT_PATH.name,
        description=__doc__,
        allow_abbrev=False,
    )
    parser.add_argument(
        "--source",
        type=Path,
        required=True,
        help="path to a local MTPL Git checkout",
    )
    parser.add_argument(
        "--commit",
        default="HEAD",
        metavar="REF",
        help="local MTPL commit or ref to synchronize (default: HEAD)",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    config = read_config()
    source, commit = validate_source(arguments.source, config, arguments.commit)
    blobs = list_source_blobs(source, commit, config)

    with tempfile.TemporaryDirectory(prefix="qgis-mtpl-sync-") as directory:
        staged = Path(directory)
        expected_paths = create_candidate(source, config, blobs, staged)
        validate_compatibility(staged)
        replace_vendor(staged, expected_paths)

    print(f"MTPL vendor sources updated from {source} at {commit}")
    print("MTPL 2.1 compatibility checks passed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except SyncError as error:
        print(f"{SCRIPT_PATH.name}: {error}", file=sys.stderr)
        sys.exit(1)
