"""Path and source-inventory contracts without QGIS or a network connection.

Every filesystem fixture belongs to a TemporaryDirectory. The SFTP double only
exposes metadata operations so inventory construction cannot silently write data.
"""

from __future__ import annotations

import errno
import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path, PurePosixPath
from types import SimpleNamespace
from unittest import mock


PLUGINS_ROOT = Path(__file__).resolve().parents[3] / "python" / "plugins"
if str(PLUGINS_ROOT) not in sys.path:
    sys.path.insert(0, str(PLUGINS_ROOT))

from qcopilots_common.security_policy import FilesystemPolicy
from qcopilots_mcp_server_processing_general.file_transfer_errors import FileTransferError
from qcopilots_mcp_server_processing_general.file_transfer_files import (
    _local_path,
    build_manifest,
    normalize_remote,
    remote_join,
    validate_remote_path,
)


class ReadOnlySftp:
    """A local, isolated namespace implementing only SFTP metadata reads."""

    def __init__(self, root):
        self.root = root
        self.root.mkdir()
        self.metadata = {}
        self.listings = {}
        self.observed = []

    def local(self, remote):
        parts = PurePosixPath(remote).parts
        if ".." in parts or not remote.startswith(("/", "C:/")):
            raise AssertionError(f"Unexpected fake-server path: {remote}")
        path = self.root.joinpath(
            *(part.replace(":", "_") for part in parts if part != "/")
        )
        path.relative_to(self.root)
        return path

    @staticmethod
    def attrs(path):
        value = path.lstat()
        return SimpleNamespace(
            filename=path.name,
            st_mode=value.st_mode,
            st_size=value.st_size,
            st_mtime=value.st_mtime,
            st_file_attributes=getattr(value, "st_file_attributes", 0),
        )

    def lstat(self, path):
        self.observed.append(("lstat", path))
        if path in self.metadata:
            value = self.metadata[path]
            if isinstance(value, BaseException):
                raise value
            return value
        return self.attrs(self.local(path))

    def listdir_attr(self, path):
        self.observed.append(("listdir_attr", path))
        if path in self.listings:
            return [SimpleNamespace(filename=name) for name in self.listings[path]]
        return [self.attrs(child) for child in self.local(path).iterdir()]


class TestFileTransferRemotePaths(unittest.TestCase):
    def test_windows_absolute_spellings_have_one_canonical_form(self):
        expected = "C:/Data/测试 数据/roads.gpkg"
        for value in (
            r"c:\Data\测试 数据\roads.gpkg",
            "C:/Data/测试 数据/roads.gpkg",
            "/c:/Data/测试 数据/roads.gpkg",
            "c:/Data//测试 数据/./roads.gpkg/",
        ):
            with self.subTest(value=value):
                self.assertEqual(normalize_remote(value), expected)

    def test_absolute_posix_paths_and_filesystem_roots(self):
        for value, expected in (
            ("/srv//data/./地图.tif/", "/srv/data/地图.tif"),
            ("/srv/a:b.txt", "/srv/a:b.txt"),
            ("/", "/"),
            ("C:/", "C:/"),
            ("/c:/", "C:/"),
        ):
            with self.subTest(value=value):
                self.assertEqual(normalize_remote(value), expected)

    def test_non_absolute_unc_device_and_traversal_paths_are_rejected(self):
        invalid = (
            "", None, 22, "relative/file.txt", "C:", "C:relative", "/C:relative",
            r"\\server\share\file", "//server/share/file", r"\\?\C:\data",
            r"\\.\C:\data", r"\??\C:\data", r"\data\file",
            "C:/data/../secret", "/srv/data/../../secret", "/../secret",
            "C:/data/a\x00b", "C:/data/a\nb", "/srv/a\x7fb",
        )
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(FileTransferError) as error:
                normalize_remote(value)
            self.assertEqual(error.exception.code, "invalid_path")

    def test_windows_unsafe_names_are_rejected(self):
        names = (
            "CON", "con.txt", "NUL.tif", "AUX", "PRN", "CONIN$", "CONOUT$.txt",
            "COM1", "com9.csv", "LPT1", "LPT9.tif", "COM¹", "LPT².txt",
            "ending.", "ending ", 'a"b', "a<b", "a>b", "a|b", "file:stream",
            "wild*.tif", "wild?.tif", "a\tb",
        )
        for name in names:
            with self.subTest(name=name), self.assertRaises(FileTransferError) as error:
                normalize_remote("C:/data/" + name)
            self.assertEqual(error.exception.code, "invalid_path")

    def test_valid_windows_names_preserve_spelling(self):
        for name in (
            "COM0.csv", "COM10.csv", "LPT0", "CONTOUR.tif", ".metadata.json",
            "测绘成果 📍.gpkg", "café.txt", "cafe\u0301.txt", "a;b (1) [x] #%.txt",
        ):
            with self.subTest(name=name):
                self.assertEqual(normalize_remote("C:/data/" + name), "C:/data/" + name)

    def test_join_adds_exactly_one_validated_component(self):
        self.assertEqual(remote_join("c:/data/", "测试.txt"), "C:/data/测试.txt")
        self.assertEqual(remote_join("/", "data"), "/data")
        for name in ("", ".", "..", "/absolute", "a/b", r"a\b", "CON", "a:b"):
            with self.subTest(name=name), self.assertRaises(FileTransferError) as error:
                remote_join("C:/data", name)
            self.assertEqual(error.exception.code, "invalid_path")


class TestFileTransferSourceManifest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qcopilots-file-transfer-paths-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.local = self.root / "local"
        self.local.mkdir()
        self.sftp = ReadOnlySftp(self.root / "remote")
        self.sftp.local("/uploads").mkdir()
        self.sftp.local("C:/").mkdir()
        self.policy = FilesystemPolicy()

    def local_file(self, name, content=b"source payload"):
        path = self.local / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def remote_file(self, name, content=b"remote payload"):
        path = self.sftp.local(name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def upload(self, sources, destination="/uploads", **kwargs):
        return build_manifest(
            "upload", [str(source) for source in sources], destination,
            self.sftp, self.policy, **kwargs,
        )

    def download(self, sources, destination=None, **kwargs):
        return build_manifest(
            "download", sources, str(destination or self.local / "download"),
            self.sftp, self.policy, **kwargs,
        )

    def assert_file_transfer_error(self, code, callback, *args, **kwargs):
        with self.assertRaises(FileTransferError) as error:
            callback(*args, **kwargs)
        self.assertEqual(error.exception.code, code)

    def test_upload_mixed_sources_preserves_basenames_and_empty_directories(self):
        single = self.local_file("standalone.csv", b"x,y\n1,2\n")
        content = self.local_file("测绘成果/影像 tile.tif", b"II*\x00payload")
        empty = self.local / "测绘成果" / "empty" / "nested"
        empty.mkdir(parents=True)
        zero = self.local_file("测绘成果/zero.bin", b"")
        manifest = self.upload([single, self.local / "测绘成果"])
        entries = {entry["target"]: entry for entry in manifest}
        self.assertEqual(set(entries), {
            "/uploads/standalone.csv", "/uploads/测绘成果",
            "/uploads/测绘成果/影像 tile.tif", "/uploads/测绘成果/empty",
            "/uploads/测绘成果/empty/nested", "/uploads/测绘成果/zero.bin",
        })
        self.assertEqual(entries["/uploads/测绘成果/empty/nested"]["kind"], "directory")
        self.assertEqual(entries["/uploads/测绘成果/empty/nested"]["size"], 0)
        self.assertEqual(entries["/uploads/测绘成果/zero.bin"]["size"], 0)
        self.assertEqual(entries["/uploads/测绘成果/影像 tile.tif"]["size"], content.stat().st_size)
        self.assertEqual(content.read_bytes(), b"II*\x00payload")
        self.assertEqual(zero.read_bytes(), b"")
        self.assertEqual(list(self.sftp.local("/uploads").iterdir()), [])

    def test_download_mixed_sources_preserves_hierarchy_without_writes(self):
        self.remote_file("C:/results/单个.csv", b"a,b\n")
        self.remote_file("C:/results/terrain/坡度.tif", b"raster data")
        self.remote_file("C:/results/terrain/zero.bin", b"")
        self.sftp.local("C:/results/terrain/empty/deep").mkdir(parents=True)
        destination = self.local / "not_created"
        manifest = self.download(
            [r"C:\results\单个.csv", "/C:/results/terrain"], destination,
        )
        entries = {
            Path(entry["target"]).relative_to(destination).as_posix(): entry
            for entry in manifest
        }
        self.assertEqual(set(entries), {
            "单个.csv", "terrain", "terrain/坡度.tif", "terrain/zero.bin",
            "terrain/empty", "terrain/empty/deep",
        })
        self.assertEqual(entries["terrain/坡度.tif"]["source"], "C:/results/terrain/坡度.tif")
        self.assertEqual(entries["terrain/empty/deep"]["kind"], "directory")
        self.assertFalse(destination.exists())

    def test_windows_upload_destination_is_normalized(self):
        source = self.local_file("地图.gpkg")
        manifest = self.upload([source], r"c:\missing\deep\folder")
        self.assertEqual(manifest[0]["target"], "C:/missing/deep/folder/地图.gpkg")
        self.assertFalse(self.sftp.local("C:/missing").exists())

    def test_order_and_progress_are_deterministic(self):
        left = self.local_file("bundle/z.txt")
        self.local_file("bundle/A.txt")
        single = self.local_file("separate.txt")
        progress = []
        manifest = self.upload(
            [single, left.parent], progress_callback=lambda count, source: progress.append((count, source)),
        )
        reversed_manifest = self.upload([left.parent, single])
        self.assertEqual(manifest, reversed_manifest)
        self.assertEqual(progress, [(index, entry["source"]) for index, entry in enumerate(manifest, 1)])
        self.assertEqual([entry["target"] for entry in manifest], [
            "/uploads/bundle", "/uploads/bundle/A.txt", "/uploads/bundle/z.txt",
            "/uploads/separate.txt",
        ])

    def test_repeated_local_source_and_parent_child_selection_are_rejected(self):
        source = self.local_file("bundle/data.csv")
        for sources in ([source, source], [source.parent, source], [source, source.parent]):
            with self.subTest(sources=sources):
                self.assert_file_transfer_error("overlapping_sources", self.upload, sources)

    def test_repeated_remote_sources_include_windows_case_aliases(self):
        self.remote_file("C:/results/data.csv")
        for sources in (
            ["C:/results/data.csv", r"c:\results\data.csv"],
            ["C:/results/data.csv", "C:/RESULTS/DATA.CSV"],
            ["C:/results", "C:/results/data.csv"],
            ["/srv/data", "/srv/data"],
        ):
            with self.subTest(sources=sources):
                self.assert_file_transfer_error("overlapping_sources", self.download, sources)

    def test_prefix_siblings_are_not_overlapping(self):
        one = self.local_file("area/value.csv")
        two = self.local_file("area2/value.csv")
        manifest = self.upload([one.parent, two.parent])
        self.assertEqual(len(manifest), 4)

    def test_upload_same_basename_and_windows_case_collision_are_rejected(self):
        left = self.local_file("left/report.csv")
        right = self.local_file("right/report.csv")
        self.assert_file_transfer_error("target_collision", self.upload, [left, right])
        upper = self.local_file("upper/REPORT.csv")
        self.assert_file_transfer_error("target_collision", self.upload, [left, upper], "C:/uploads")

    def test_download_same_basename_and_case_collision_are_rejected(self):
        self.remote_file("/left/report.csv")
        self.remote_file("/right/report.csv")
        self.remote_file("/upper/REPORT.csv")
        self.assert_file_transfer_error("target_collision", self.download, ["/left/report.csv", "/right/report.csv"])
        self.assert_file_transfer_error("target_collision", self.download, ["/left/report.csv", "/upper/REPORT.csv"])

    def test_upload_to_posix_preserves_distinct_case_names(self):
        lower = self.local_file("lower/report.csv")
        upper = self.local_file("upper/REPORT.csv")
        manifest = self.upload([lower, upper])
        self.assertEqual({entry["target"] for entry in manifest}, {"/uploads/report.csv", "/uploads/REPORT.csv"})

    def test_destination_must_be_a_directory(self):
        source = self.local_file("source.csv")
        remote = "/existing.csv"
        self.remote_file(remote)
        self.assert_file_transfer_error("invalid_destination", self.upload, [source], remote)
        self.assert_file_transfer_error("invalid_destination", self.download, [remote], source)

    def test_filesystem_roots_are_not_transfer_sources(self):
        self.assert_file_transfer_error("invalid_source", self.upload, [self.local.anchor])
        for source in ("/", "C:/", "/c:/"):
            with self.subTest(source=source):
                self.assert_file_transfer_error("invalid_source", self.download, [source])

    def test_invalid_direction_and_source_container_are_rejected(self):
        self.assert_file_transfer_error("invalid_direction", build_manifest, "copy", ["/source"], "/uploads", self.sftp, self.policy)
        for sources in ([], (), "file.txt", [None], [22], None):
            with self.subTest(sources=sources):
                self.assert_file_transfer_error("invalid_sources", build_manifest, "upload", sources, "/uploads", self.sftp, self.policy)

    def test_missing_sources_are_not_silently_omitted(self):
        with self.assertRaises(FileNotFoundError):
            self.upload([self.local / "missing.csv"])
        with self.assertRaises(FileNotFoundError):
            self.download(["/missing.csv"])

    def test_remote_path_checks_each_ancestor_without_following_links(self):
        self.remote_file("C:/results/nested/data.csv")
        self.sftp.observed.clear()
        validate_remote_path(self.sftp, r"c:\results\nested\data.csv")
        self.assertEqual(self.sftp.observed, [
            ("lstat", "C:/"), ("lstat", "C:/results"),
            ("lstat", "C:/results/nested"), ("lstat", "C:/results/nested/data.csv"),
        ])

    def test_allow_missing_does_not_swallow_permissions_or_io_errors(self):
        self.assertIsNone(validate_remote_path(self.sftp, "/missing/child", allow_missing=True))
        for error in (PermissionError(errno.EACCES, "denied"), OSError(errno.EIO, "I/O failed")):
            self.sftp.metadata["/blocked"] = error
            with self.subTest(error=error), self.assertRaises(type(error)) as caught:
                validate_remote_path(self.sftp, "/blocked/child", allow_missing=True)
            self.assertIs(caught.exception, error)

    def test_file_ancestor_is_rejected_even_when_destination_may_be_missing(self):
        self.remote_file("/parent-file")
        self.assert_file_transfer_error("invalid_path", validate_remote_path, self.sftp, "/parent-file/child", allow_missing=True)
        parent = self.local_file("parent-file")
        self.assert_file_transfer_error("invalid_path", _local_path, self.policy, parent / "child", "write")

    def test_remote_symlinks_and_reparse_points_stop_before_child_lookup(self):
        for mode, attributes in ((stat.S_IFLNK | 0o777, 0), (stat.S_IFDIR | 0o755, 0x400)):
            self.sftp.metadata["/linked"] = SimpleNamespace(st_mode=mode, st_file_attributes=attributes)
            self.sftp.observed.clear()
            with self.subTest(mode=mode, attributes=attributes):
                self.assert_file_transfer_error("unsafe_path", validate_remote_path, self.sftp, "/linked/private.txt", allow_missing=True)
            self.assertNotIn(("lstat", "/linked/private.txt"), self.sftp.observed)

    def test_remote_link_within_source_tree_is_rejected_before_listing(self):
        self.sftp.local("/tree/link").mkdir(parents=True)
        self.sftp.metadata["/tree/link"] = SimpleNamespace(st_mode=stat.S_IFLNK | 0o777)
        self.assert_file_transfer_error("unsafe_path", self.download, ["/tree"])
        self.assertNotIn(("listdir_attr", "/tree/link"), self.sftp.observed)

    def test_remote_directory_entries_cannot_escape_or_create_unsafe_windows_names(self):
        self.sftp.local("/tree").mkdir()
        for name in ("..", ".", "/outside", r"a\b", "a/b", "NUL.txt", "bad:name", "trailing.", "a\x00b"):
            self.sftp.listings["/tree"] = [name]
            with self.subTest(name=name):
                self.assert_file_transfer_error("invalid_path", self.download, ["/tree"])
        self.assertFalse((self.local / "download").exists())

    def test_duplicate_server_entries_cannot_silently_overwrite_manifest_entries(self):
        self.remote_file("/tree/data.csv")
        self.sftp.listings["/tree"] = ["data.csv", "data.csv"]
        self.assert_file_transfer_error("target_collision", self.download, ["/tree"])

    def test_unsupported_or_incomplete_remote_metadata_is_rejected(self):
        cases = (
            (SimpleNamespace(st_mode=stat.S_IFIFO | 0o600), "unsupported_file_type"),
            (SimpleNamespace(st_size=1, st_mtime=1), "unsupported_file_type"),
            (SimpleNamespace(st_mode=stat.S_IFREG | 0o600, st_size=-1, st_mtime=1), "invalid_metadata"),
            (SimpleNamespace(st_mode=stat.S_IFREG | 0o600, st_size=1, st_mtime=None), "invalid_metadata"),
        )
        for metadata, code in cases:
            self.sftp.metadata["/source.csv"] = metadata
            with self.subTest(metadata=metadata):
                self.assert_file_transfer_error(code, self.download, ["/source.csv"])

    def test_cancellation_before_scanning_and_during_progress_propagates(self):
        source = self.local_file("tree/data.csv")
        sentinel = RuntimeError("cancelled by test")
        with self.assertRaises(RuntimeError) as error:
            self.upload([source], cancel_check=mock.Mock(side_effect=sentinel))
        self.assertIs(error.exception, sentinel)
        self.assertEqual(self.sftp.observed, [])
        progress = []

        def check_cancel():
            if progress:
                raise sentinel

        with self.assertRaises(RuntimeError) as error:
            self.upload([source.parent], cancel_check=check_cancel, progress_callback=lambda *_args: progress.append(True))
        self.assertIs(error.exception, sentinel)
        self.assertEqual(progress, [True])
        self.assertEqual(list(self.sftp.local("/uploads").iterdir()), [])

    @unittest.skipUnless(os.name == "nt", "Windows local path validation")
    def test_windows_local_path_rejects_unsafe_syntax_before_policy_resolution(self):
        policy = mock.Mock(spec=FilesystemPolicy)
        invalid = (
            "", "relative.csv", "C:", "C:relative.csv", r"\drive-relative.csv",
            r"\\server\share\file.csv", r"\\?\C:\data.csv", r"\\.\C:\data.csv",
            r"\??\C:\data.csv", str(self.local / ".." / "escape.csv"),
            str(self.local / "CON.txt"), str(self.local / "data.csv:stream"),
            str(self.local / "ending."), str(self.local / "ending "),
            str(self.local / "bad\nname.csv"), str(self.local / "wild*.csv"),
        )
        for value in invalid:
            with self.subTest(value=value):
                self.assert_file_transfer_error("invalid_path", _local_path, policy, value, "read")
        policy.resolve_path.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "Windows local path validation")
    def test_windows_local_absolute_spellings_and_policy_access(self):
        source = self.local_file("测试 📍/source.csv")
        for spelling in (str(source), source.as_posix()):
            with self.subTest(spelling=spelling):
                self.assertEqual(_local_path(self.policy, spelling, "read"), source.resolve())
        policy = mock.Mock(spec=FilesystemPolicy)
        policy.resolve_path.return_value = source
        self.assertEqual(_local_path(policy, source, "read"), source)
        policy.resolve_path.assert_called_once_with(source, access="read")

    @unittest.skipUnless(os.name == "nt", "Windows mapped drive validation")
    def test_mapped_drive_is_rejected_before_filesystem_lookup(self):
        import ctypes

        with mock.patch.object(ctypes.windll.kernel32, "GetDriveTypeW", return_value=4), mock.patch.object(Path, "lstat") as lookup:
            self.assert_file_transfer_error("unsafe_path", _local_path, self.policy, r"Z:\remote\data.csv", "read")
        lookup.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "Windows junction behavior")
    def test_actual_junction_is_not_followed_for_source_or_destination(self):
        target = self.root / "junction-target"
        target.mkdir()
        sentinel = target / "sentinel.txt"
        sentinel.write_text("must remain untouched", encoding="utf-8")
        junction = self.local / "junction"
        result = subprocess.run(
            ["cmd.exe", "/d", "/c", "mklink", "/J", str(junction), str(target)],
            capture_output=True, text=True, check=False,
        )
        if result.returncode:
            self.skipTest(f"Junction creation is unavailable (exit {result.returncode})")

        def remove_only_junction():
            # RemoveDirectory removes the link itself, never its target contents.
            # Both link and target are owned by this test's TemporaryDirectory.
            junction.relative_to(self.root)
            junction.rmdir()
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "must remain untouched")

        self.addCleanup(remove_only_junction)
        self.assertTrue(junction.is_junction())
        self.assert_file_transfer_error("unsafe_path", self.upload, [junction])
        self.assert_file_transfer_error("unsafe_path", self.upload, [junction / "sentinel.txt"])
        self.assert_file_transfer_error("unsafe_path", self.upload, [self.local])
        self.remote_file("/download.csv")
        self.assert_file_transfer_error("unsafe_path", self.download, ["/download.csv"], junction / "child")
        self.assertEqual(list(target.iterdir()), [sentinel])


if __name__ == "__main__":
    unittest.main()
