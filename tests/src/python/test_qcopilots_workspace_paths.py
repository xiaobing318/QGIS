"""Workspace path regression tests for the QCopilots QGIS bridge.

The bridge write tools must not resolve relative output paths against the
QGIS process working folder, which is the QGIS program folder for the GUI.
These tests replace qgis.core with small fakes, so they need no QGIS
application and never write through QGIS.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "Ethan.Yang"
__date__ = "2026-11-01"
__copyright__ = "Copyright 2026, The QGIS Project"

import contextlib
import os
import sys
import tempfile
import types
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python" / "plugins"))

from qcopilots_common import bridge
from qcopilots_common.security_policy import (
    FORMAL_RESTRICTED_MODE,
    FilesystemPolicy,
)


RESTRICTED_POLICY = FilesystemPolicy(
    mode=FORMAL_RESTRICTED_MODE,
    shell_enabled=False,
    network_enabled=False,
)


class _UnexpectedQgisUse(AssertionError):
    pass


def _unexpected(*args, **kwargs):
    raise _UnexpectedQgisUse("QGIS must not be used after a rejected output path")


class _FakeLayout:
    def name(self):
        return "Main layout"

    def pageCollection(self):
        _unexpected()


class _FakeLayoutManager:
    def layoutByName(self, name):
        return _FakeLayout() if name == "Main layout" else None


class _FakeProject:
    def __init__(self):
        self.writes = []

    def fileName(self):
        return ""

    def write(self, path=None):
        self.writes.append(path)
        _unexpected()

    def layoutManager(self):
        return _FakeLayoutManager()

    def addMapLayer(self, layer):
        _unexpected()


@contextlib.contextmanager
def _fake_qgis_modules(project):
    qgis_module = types.ModuleType("qgis")
    core_module = types.ModuleType("qgis.core")
    core_module.QgsProject = types.SimpleNamespace(instance=lambda: project)
    core_module.QgsVectorLayer = _unexpected
    core_module.QgsLayoutExporter = _unexpected
    qgis_module.core = core_module
    fakes = {"qgis": qgis_module, "qgis.core": core_module}
    originals = {name: sys.modules.get(name) for name in fakes}
    sys.modules.update(fakes)
    try:
        yield
    finally:
        for name, original in originals.items():
            if original is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = original


class _FakeCanvas:
    def __init__(self):
        self.calls = []

    def saveAsImage(self, path, pixmap, encoder):
        self.calls.append((path, pixmap, encoder))
        Path(path).write_bytes(b"png-image")
        bridge._map_image_world_file_path(Path(path)).write_bytes(b"world-file")


class TestQCopilotsWorkspacePaths(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="qcopilots-workspace-paths-")
        self.root = Path(self.temp.name).resolve()
        # Stand in for the QGIS program folder, which is the working folder of
        # the QGIS GUI process.
        self.program_folder = self.root / "program"
        self.program_folder.mkdir()
        self.previous_directory = Path.cwd()
        os.chdir(self.program_folder)

    def tearDown(self):
        os.chdir(self.previous_directory)
        self.temp.cleanup()

    def assert_nothing_written(self):
        self.assertEqual(list(self.program_folder.iterdir()), [])
        self.assertEqual(list(self.root.iterdir()), [self.program_folder])

    def assert_relative_output_rejected(self, call, relative):
        with self.assertRaises(ValueError) as caught:
            call()
        message = str(caught.exception)
        self.assertIn(
            f"Output path must be an absolute file path (got '{relative}')",
            message,
        )
        self.assertIn(f"QGIS program folder {Path.cwd()}", message)

    def test_write_tools_reject_relative_output_paths(self):
        for policy in (FilesystemPolicy(), RESTRICTED_POLICY):
            canvas = _FakeCanvas()
            tools = bridge.QgisBridgeTools(
                types.SimpleNamespace(mapCanvas=lambda canvas=canvas: canvas),
                filesystem_policy=policy,
            )
            project = _FakeProject()
            with _fake_qgis_modules(project):
                for stem in ("result", "outputs/result", "..\\result", "./result"):
                    cases = (
                        (
                            "save_project",
                            f"{stem}.qgz",
                            lambda path: tools.save_project({"path": path}),
                        ),
                        (
                            "export_map_image",
                            f"{stem}.png",
                            lambda path: tools.export_map_image({"path": path}),
                        ),
                        (
                            "export_print_layout",
                            f"{stem}.pdf",
                            lambda path: tools.interactive_layout_export(
                                {"layout_name": "Main layout", "path": path}
                            ),
                        ),
                        (
                            "create_vector_layer",
                            f"{stem}.gpkg",
                            lambda path: tools.create_vector_layer(
                                {"name": "Roads", "path": path}
                            ),
                        ),
                    )
                    for tool, relative, call in cases:
                        with self.subTest(mode=policy.mode, tool=tool, path=relative):
                            self.assert_relative_output_rejected(
                                lambda: call(relative), relative
                            )
            self.assertEqual(project.writes, [])
            self.assertEqual(canvas.calls, [])
        self.assert_nothing_written()

    def test_safe_workspace_path_rejects_relative_write_paths(self):
        for policy in (FilesystemPolicy(), RESTRICTED_POLICY):
            tools = bridge.QgisBridgeTools(None, filesystem_policy=policy)
            for relative in (
                "map.png",
                "outputs/map.png",
                "..\\map.png",
                Path("outputs") / "map.png",
            ):
                with self.subTest(mode=policy.mode, path=str(relative)):
                    self.assert_relative_output_rejected(
                        lambda: tools._safe_workspace_path(relative, access="write"),
                        str(relative),
                    )
            # Existing policy rejections keep their own message.
            with self.subTest(mode=policy.mode, path="C:map.png"):
                with self.assertRaisesRegex(PermissionError, "drive-relative"):
                    tools._safe_workspace_path("C:map.png", access="write")
        self.assert_nothing_written()

    def test_absolute_write_paths_are_accepted(self):
        output_folder = self.root / "outputs"
        target = output_folder / "map.png"
        for policy in (FilesystemPolicy(), RESTRICTED_POLICY):
            tools = bridge.QgisBridgeTools(None, filesystem_policy=policy)
            with self.subTest(mode=policy.mode):
                self.assertEqual(
                    tools._safe_workspace_path(str(target), access="write"),
                    str(target.resolve()),
                )
                self.assertEqual(
                    tools._safe_workspace_path(target, access="write"),
                    str(target.resolve()),
                )

        canvas = _FakeCanvas()
        tools = bridge.QgisBridgeTools(types.SimpleNamespace(mapCanvas=lambda: canvas))
        result = tools.export_map_image({"path": str(target), "format": "png"})
        self.assertEqual(Path(result["path"]), target.resolve())
        self.assertEqual(target.read_bytes(), b"png-image")
        self.assertEqual(len(canvas.calls), 1)
        self.assertEqual(list(self.program_folder.iterdir()), [])

    def test_read_paths_still_resolve_against_the_working_folder(self):
        working_folder = Path.cwd()
        expected = str((working_folder / "data" / "roads.gpkg").resolve())
        for policy in (FilesystemPolicy(), RESTRICTED_POLICY):
            tools = bridge.QgisBridgeTools(None, filesystem_policy=policy)
            with self.subTest(mode=policy.mode):
                self.assertEqual(
                    tools._safe_workspace_path("data/roads.gpkg"),
                    expected,
                )
                self.assertEqual(
                    tools._safe_workspace_path("data/roads.gpkg", access="read"),
                    expected,
                )
                with self.assertRaisesRegex(PermissionError, "drive-relative"):
                    tools._safe_workspace_path("C:roads.gpkg", access="read")

        # A local layer source is stricter than the path helper: a relative
        # source would be read from the QGIS program folder, so it is refused.
        tools = bridge.QgisBridgeTools(None)
        with self.assertRaisesRegex(ValueError, "absolute file path"):
            tools._validate_layer_source_path("data/roads.gpkg|layername=roads")
        self.assertEqual(
            tools._validate_layer_source_path(expected + "|layername=roads"),
            expected + "|layername=roads",
        )
        self.assert_nothing_written()

    def test_embedded_local_layer_paths_must_be_absolute(self):
        data = self.root / "data.gpkg"
        data.write_bytes(b"data")
        compatible = bridge.QgisBridgeTools(None)
        relative_sources = (
            "file:data.gpkg",
            "dbname='data.gpkg' table=roads",
            "GPKG:data.gpkg",
            # QGIS does not expand "~" in a provider URI it receives as is.
            "dbname='~/data.gpkg' table=roads",
            # A url without a scheme is a local file, for example mbtiles.
            "type=mbtiles&url=data/tiles.mbtiles",
        )
        for source in relative_sources:
            with self.subTest(source=source), self.assertRaisesRegex(
                ValueError, "absolute file path"
            ):
                compatible._validate_layer_source_path(source)
        for options in (
            {"url": "file:data.gpkg"},
            {"url": "data.gpkg"},
            {"dbname": "data.gpkg"},
        ):
            with self.subTest(options=options), self.assertRaisesRegex(
                ValueError, "absolute file path"
            ):
                compatible._validate_layer_uri_options(
                    options, layer_type="vector", provider="ogr"
                )
        self.assertEqual(
            compatible._validate_layer_source_path(data.as_uri()),
            str(data.resolve()),
        )
        absolute_provider = f"dbname='{data}' table=roads"
        self.assertEqual(
            compatible._validate_layer_source_path(absolute_provider),
            absolute_provider,
        )

        restricted = bridge.QgisBridgeTools(None, filesystem_policy=RESTRICTED_POLICY)
        with self.assertRaisesRegex(ValueError, "absolute file path"):
            restricted._validate_layer_source_path("dbname=data.gpkg&table=roads")
        self.assertEqual(
            restricted._validate_layer_source_path(f"dbname={data}&table=roads"),
            f"dbname={data}&table=roads",
        )
        self.assertEqual(list(self.program_folder.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
