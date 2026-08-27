"""QGIS unit tests for asynchronous QCopilots QGIS binary jobs.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-08-02"
__copyright__ = "Copyright 2026, The QGIS Project"

import copy
import io
import json
import os
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from types import ModuleType, SimpleNamespace
from unittest import mock


_QGIS_DLL_DIRECTORY_HANDLES = []


def _prepare_windows_qgis_dll_search_path():
    if sys.platform != "win32":
        return
    prefix_path = os.environ.get("QGIS_PREFIX_PATH", "")
    bin_root = Path(prefix_path).parent if prefix_path else None
    if bin_root is None or not bin_root.is_dir():
        return
    for candidate in sorted(bin_root.iterdir()):
        if not (candidate / "qgis_core.dll").is_file():
            continue
        os.environ["PATH"] = f"{candidate}{os.pathsep}{os.environ.get('PATH', '')}"
        if hasattr(os, "add_dll_directory"):
            _QGIS_DLL_DIRECTORY_HANDLES.append(os.add_dll_directory(str(candidate)))
        return


_prepare_windows_qgis_dll_search_path()

from qcopilots_common.async_jobs import AsyncJobStore, IdempotencyConflictError
from qcopilots_common.bridge import QgisBridgeController, QgisBridgeTools
from qcopilots_common.qgis_binary_jobs import (
    BinaryCatalogError,
    BinaryExecutionError,
    BinarySubprocessExecution,
    QGISBinaryJobManager,
    _root_matches_markers,
)
from qcopilots_common.security_policy import (
    FilesystemPolicy,
    filesystem_policy_from_config,
)


class FakeSignal:
    def __init__(self):
        self._callbacks = []
        self._lock = threading.Lock()

    def connect(self, callback):
        with self._lock:
            self._callbacks.append(callback)

    def emit(self, *arguments):
        with self._lock:
            callbacks = list(self._callbacks)
        for callback in callbacks:
            callback(*arguments)


class RecordingPipe(io.BytesIO):
    def __init__(self, value=b""):
        super().__init__(value)
        self.written = bytearray()

    def write(self, value):
        self.written.extend(value)
        return len(value)


class FakeProcess:
    def __init__(
        self,
        *,
        stdout=b"",
        stderr=b"",
        exit_code=0,
        running=False,
        graceful_signal=True,
    ):
        self.stdout = RecordingPipe(stdout)
        self.stderr = RecordingPipe(stderr)
        self.stdin = RecordingPipe()
        self.exit_code = exit_code
        self.running = running
        self.graceful_signal = graceful_signal
        self.pid = 4321
        self.signals = []
        self.terminated = False
        self.killed = False

    def poll(self):
        return None if self.running else self.exit_code

    def wait(self, timeout=None):
        if self.running:
            raise subprocess.TimeoutExpired("fake", timeout)
        return self.exit_code

    def send_signal(self, value):
        self.signals.append(value)
        if not self.graceful_signal:
            raise OSError("graceful stop rejected")
        self.running = False
        self.exit_code = -2

    def terminate(self):
        self.terminated = True
        self.running = False
        self.exit_code = -15

    def kill(self):
        self.killed = True
        self.running = False
        self.exit_code = -9


class StepClock:
    def __init__(self, values):
        self._values = iter(values)
        self._last = 0.0

    def __call__(self):
        try:
            self._last = float(next(self._values))
        except StopIteration:
            self._last += 1.0
        return self._last


class PausingCreateStore(AsyncJobStore):
    """Makes the post-create, pre-task-registration window deterministic."""

    def __init__(self):
        super().__init__(scope_field="category")
        self.created = threading.Event()
        self.release = threading.Event()

    def create(self, *args, **kwargs):
        result = super().create(*args, **kwargs)
        self.created.set()
        if not self.release.wait(5):
            raise RuntimeError("Timed out waiting to release the test Job Store")
        return result


class ControlledExecution:
    def __init__(self, **values):
        self.values = values
        self.cancelled = False
        self.outcome = {
            "exit_code": 0,
            "stdout": "ok",
            "stderr": "",
            "stdout_truncated": False,
            "stderr_truncated": False,
            "stdout_total_bytes": 2,
            "stderr_total_bytes": 0,
            "duration_seconds": 0.25,
            "timed_out": False,
            "cancelled": False,
            "cancel_requested": False,
            "process_alive": False,
            "start_error": None,
            "environment_error": None,
            "termination_error": None,
        }

    def cancel(self):
        self.cancelled = True
        self.outcome["cancel_requested"] = True

    def is_cancelled(self):
        return self.cancelled

    def outcome_snapshot(self):
        return copy.deepcopy(self.outcome)


class FakeTask:
    def __init__(self, *, description, execution, binary):
        self.description = description
        self.execution = execution
        self.binary = binary
        self.begun = FakeSignal()
        self.taskCompleted = FakeSignal()
        self.taskTerminated = FakeSignal()
        self.destroyed = FakeSignal()
        self.progress_values = []
        self.wait_arguments = []
        self.wait_result = True

    def cancel(self):
        self.execution.cancel()

    def isCanceled(self):
        return self.execution.is_cancelled()

    def waitForFinished(self, milliseconds):
        self.wait_arguments.append(milliseconds)
        return self.wait_result

    def setProgress(self, value):
        self.progress_values.append(value)

    def outcome_snapshot(self):
        return self.execution.outcome_snapshot()

    def begin(self):
        self.begun.emit()

    def finish(self, success):
        if success:
            self.taskCompleted.emit()
        else:
            self.taskTerminated.emit()


class FakeTaskManager:
    def __init__(self, accept=True):
        self.accept = accept
        self.tasks = []

    def addTask(self, task):
        if not self.accept:
            return 0
        self.tasks.append(task)
        return len(self.tasks)


def _argv_probe(arguments=None):
    return {
        "mode": "argv",
        "arguments": list(arguments or ["--version"]),
        "success_exit_codes": [0],
        "timeout_seconds": 5,
    }


def _static_probe():
    return {
        "mode": "static",
        "checks": ["exists", "readable", "pe_machine", "sha256"],
        "sha256": "0" * 64,
    }


def _catalog_document():
    defaults = {
        "environment": "direct",
        "enabled": True,
        "disabled_reason": None,
        "risk": "R1",
        "success_exit_codes": [0],
        "timeout_seconds": 30,
        "max_output_bytes": 64,
        "max_stdin_bytes": 32,
        "progress_parser": None,
        "probe": _argv_probe(),
    }
    return {
        "version": 1,
        "package_root_resolution": {
            "environment_variables": ["QCOPILOTS_TEST_PACKAGE_ROOT"],
            "markers": ["bin/qgis-qt6-env.bat"],
            "max_parent_levels": 4,
        },
        "environment_profiles": {
            "direct": {
                "inherit_environment": False,
                "setup_scripts": [],
                "variables": {"PACKAGE": "${package_root}"},
            },
            "qgis": {
                "inherit_environment": False,
                "setup_scripts": ["bin/qgis-qt6-env.bat"],
                "variables": {"EXPANDED": "${package_root}/profile"},
            },
        },
        "groups": {
            "cli": {"defaults": defaults},
            "blocked": {"defaults": defaults},
        },
        "binaries": [
            {
                "id": "alpha",
                "name": "Alpha",
                "description": "First safe CLI",
                "path": "bin/alpha.exe",
                "group": "cli",
                "progress_parser": {"type": "gdal_dotted"},
            },
            {
                "id": "bravo",
                "name": "Bravo",
                "description": "Confirmed CLI",
                "path": "bin/bravo.exe",
                "group": "cli",
                "risk": "R2",
                "progress_parser": {
                    "type": "percent_regex",
                    "pattern": r"(?P<percent>\d+(?:\.\d+)?)%",
                },
            },
            {
                "id": "blocked",
                "name": "Blocked",
                "description": "Policy blocked CLI",
                "path": "bin/blocked.exe",
                "group": "blocked",
                "enabled": False,
                "disabled_reason": "R3 policy",
                "risk": "R3",
                "probe": _static_probe(),
            },
        ],
    }


class BinaryFixture:
    def __init__(
        self,
        test_case,
        *,
        document=None,
        dependencies=None,
        store=None,
        unicode_root=False,
    ):
        self.test_case = test_case
        self.temporary = tempfile.TemporaryDirectory()
        temporary_root = Path(self.temporary.name)
        self.root = temporary_root / "发布 包" if unicode_root else temporary_root
        self.root.mkdir(parents=True, exist_ok=True)
        self.document = copy.deepcopy(document or _catalog_document())
        for item in self.document["binaries"]:
            executable = self.root.joinpath(*item["path"].split("/"))
            executable.parent.mkdir(parents=True, exist_ok=True)
            executable.write_bytes(b"MZ fake executable")
        setup_script = self.root / "bin" / "qgis-qt6-env.bat"
        setup_script.write_text("@echo off\n", encoding="utf-8")
        self.catalog_path = self.root / "qgis_binaries.json"
        self.catalog_path.write_text(
            json.dumps(self.document, ensure_ascii=False), encoding="utf-8"
        )
        self.task_manager = FakeTaskManager()
        self.executions = []

        def execution_factory(**values):
            execution = ControlledExecution(**values)
            self.executions.append(execution)
            return execution

        manager_dependencies = {
            "task_manager": self.task_manager,
            "task_factory": FakeTask,
            "execution_factory": execution_factory,
            "environment": {"PARENT_ONLY": "must not leak"},
        }
        manager_dependencies.update(dependencies or {})
        self.manager = QGISBinaryJobManager(
            catalog_path=self.catalog_path,
            package_root=self.root,
            store=store,
            dependencies=manager_dependencies,
        )
        test_case.addCleanup(self.close)

    def close(self):
        manager = getattr(self, "manager", None)
        if manager is not None and manager.accepting:
            manager.shutdown(timeout_seconds=0)
        self.manager = None
        self.temporary.cleanup()


class TestQCopilotsQGISBinaryCatalogLoader(unittest.TestCase):
    def _assert_invalid(self, mutate, message):
        document = _catalog_document()
        mutate(document)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for item in document.get("binaries", []):
                path = item.get("path", "bin/fallback.exe")
                if ".." in path or Path(path).is_absolute():
                    continue
                executable = root.joinpath(*path.replace("\\", "/").split("/"))
                executable.parent.mkdir(parents=True, exist_ok=True)
                executable.write_bytes(b"MZ")
            setup_script = root / "bin" / "qgis-qt6-env.bat"
            setup_script.parent.mkdir(parents=True, exist_ok=True)
            setup_script.write_text("@echo off\n", encoding="utf-8")
            catalog_path = root / "catalog.json"
            catalog_path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(BinaryCatalogError, message):
                QGISBinaryJobManager(
                    catalog_path=catalog_path,
                    package_root=root,
                    dependencies={"task_manager": FakeTaskManager()},
                )

    def test_rejects_duplicate_ids_paths_and_path_escape(self):
        cases = [
            (
                lambda value: value["binaries"][1].update(id="alpha"),
                "Duplicate binary id",
            ),
            (
                lambda value: value["binaries"][1].update(path="bin/alpha.exe"),
                "Duplicate binary path",
            ),
            (
                lambda value: value["binaries"][0].update(path="../escape.exe"),
                "Invalid binary path",
            ),
        ]
        for mutate, message in cases:
            with self.subTest(message=message):
                self._assert_invalid(mutate, message)

    def test_rejects_policy_and_contract_errors(self):
        cases = [
            (
                lambda value: value.update(binaries=[]),
                "binaries must be a non-empty array",
            ),
            (
                lambda value: value["binaries"][0].update(group="undefined"),
                "Unknown group",
            ),
            (
                lambda value: value["binaries"][0].update(probe={"mode": "argv"}),
                "Invalid argv probe",
            ),
            (
                lambda value: value["binaries"][2].update(enabled=True),
                "R3 binary must be disabled",
            ),
            (
                lambda value: value["environment_profiles"]["qgis"].update(
                    setup_scripts=["qgis-qt6-env.bat"]
                ),
                "Untrusted environment setup script",
            ),
            (
                lambda value: value["package_root_resolution"].update(
                    max_parent_levels=33
                ),
                "max_parent_levels",
            ),
            (
                lambda value: value.update(version=2),
                "catalog version must be 1",
            ),
            (
                lambda value: value["package_root_resolution"].update(
                    strategy="qgis_prefix_ancestor"
                ),
                "strategy is no longer supported",
            ),
        ]
        for mutate, message in cases:
            with self.subTest(message=message):
                self._assert_invalid(mutate, message)

    def test_rejects_missing_and_extra_executables(self):
        fixture = BinaryFixture(self)
        missing = fixture.root / "bin" / "alpha.exe"
        missing.unlink()
        with self.assertRaisesRegex(BinaryCatalogError, "missing_count=1"):
            QGISBinaryJobManager(
                catalog_path=fixture.catalog_path,
                package_root=fixture.root,
            )
        missing.write_bytes(b"MZ")
        extra = fixture.root / "bin" / "extra.exe"
        extra.write_bytes(b"MZ")
        with self.assertRaisesRegex(BinaryCatalogError, "extra_count=1"):
            QGISBinaryJobManager(
                catalog_path=fixture.catalog_path,
                package_root=fixture.root,
            )
        extra.unlink()

    def test_catalog_counts_are_derived(self):
        fixture = BinaryFixture(self)
        catalog = fixture.manager.load_catalog()
        self.assertEqual(
            catalog["counts"],
            {"configured": 3, "enabled": 2, "disabled": 1},
        )

        changed_document = _catalog_document()
        changed_document["binaries"][0].update(
            enabled=False,
            disabled_reason="disabled for dynamic count test",
            probe=_static_probe(),
        )
        changed_fixture = BinaryFixture(self, document=changed_document)
        self.assertEqual(
            changed_fixture.manager.load_catalog()["counts"],
            {"configured": 3, "enabled": 1, "disabled": 2},
        )

    def test_all_package_root_sources_require_markers(self):
        fixture = BinaryFixture(self)

        resolved = QGISBinaryJobManager(
            catalog_path=fixture.catalog_path,
            package_root=None,
            dependencies={
                "package_root_resolver": lambda *_: fixture.root,
                "task_manager": FakeTaskManager(),
            },
        )
        self.addCleanup(resolved.shutdown, 0)
        self.assertEqual(resolved.package_root, fixture.root.resolve())

        with mock.patch.dict(
            os.environ,
            {"QCOPILOTS_TEST_PACKAGE_ROOT": str(fixture.root)},
            clear=False,
        ):
            manager = QGISBinaryJobManager(
                catalog_path=fixture.catalog_path,
                package_root=None,
                dependencies={"task_manager": FakeTaskManager()},
            )
            self.addCleanup(manager.shutdown, 0)
            self.assertEqual(manager.package_root, fixture.root.resolve())

        prefix = fixture.root / "apps" / "qgis-qt6"
        prefix.mkdir(parents=True)
        qgis_module = ModuleType("qgis")
        qgis_core_module = ModuleType("qgis.core")
        qgis_core_module.QgsApplication = SimpleNamespace(
            prefixPath=lambda: str(prefix)
        )
        qgis_module.core = qgis_core_module
        qgis_modules = {
            "qgis": qgis_module,
            "qgis.core": qgis_core_module,
        }
        with mock.patch.dict(
            os.environ,
            {"QCOPILOTS_TEST_PACKAGE_ROOT": ""},
            clear=False,
        ), mock.patch.dict(
            sys.modules,
            qgis_modules,
        ):
            discovered = QGISBinaryJobManager(
                catalog_path=fixture.catalog_path,
                package_root=None,
                dependencies={"task_manager": FakeTaskManager()},
            )
            self.addCleanup(discovered.shutdown, 0)
            self.assertEqual(discovered.package_root, fixture.root.resolve())

        marker = fixture.root / "bin" / "qgis-qt6-env.bat"
        marker.unlink()

        with self.assertRaisesRegex(BinaryCatalogError, "does not satisfy"):
            QGISBinaryJobManager(
                catalog_path=fixture.catalog_path,
                package_root=fixture.root,
            )

        with self.assertRaisesRegex(BinaryCatalogError, "does not satisfy"):
            QGISBinaryJobManager(
                catalog_path=fixture.catalog_path,
                package_root=None,
                dependencies={
                    "package_root_resolver": lambda *_: fixture.root,
                },
            )

        with mock.patch.dict(
            os.environ,
            {"QCOPILOTS_TEST_PACKAGE_ROOT": str(fixture.root)},
            clear=False,
        ):
            with self.assertRaisesRegex(BinaryCatalogError, "does not satisfy"):
                QGISBinaryJobManager(
                    catalog_path=fixture.catalog_path,
                    package_root=None,
                )

        with mock.patch.dict(
            os.environ,
            {"QCOPILOTS_TEST_PACKAGE_ROOT": ""},
            clear=False,
        ), mock.patch.dict(
            sys.modules,
            qgis_modules,
        ):
            with self.assertRaisesRegex(BinaryCatalogError, "found 0"):
                QGISBinaryJobManager(
                    catalog_path=fixture.catalog_path,
                    package_root=None,
                )

    def test_package_root_marker_must_be_a_regular_file(self):
        fixture = BinaryFixture(self)
        marker = fixture.root / "bin" / "qgis-qt6-env.bat"
        marker.unlink()
        marker.mkdir()
        with self.assertRaisesRegex(BinaryCatalogError, "does not satisfy"):
            QGISBinaryJobManager(
                catalog_path=fixture.catalog_path,
                package_root=fixture.root,
            )

    def test_package_root_marker_link_resolution(self):
        fixture = BinaryFixture(self, unicode_root=True)
        root = fixture.root.resolve()
        marker = root / "bin" / "qgis-qt6-env.bat"
        inside = root / "bin" / "inside-marker-target.exe"
        inside.write_bytes(b"MZ inside")
        outside = root.parent / "outside-marker.exe"
        outside.write_bytes(b"MZ outside")
        outside = outside.resolve()
        path_type = type(marker)
        original_resolve = path_type.resolve

        def marker_matches(resolved_target):
            def resolve_path(path, *arguments, **keywords):
                if path == marker:
                    if isinstance(resolved_target, BaseException):
                        raise resolved_target
                    return resolved_target
                return original_resolve(path, *arguments, **keywords)

            with mock.patch.object(
                path_type,
                "resolve",
                autospec=True,
                side_effect=resolve_path,
            ):
                return _root_matches_markers(
                    root,
                    ["bin/qgis-qt6-env.bat"],
                )

        self.assertTrue(marker_matches(inside.resolve()))
        self.assertFalse(marker_matches(FileNotFoundError("broken marker link")))
        self.assertFalse(marker_matches(outside))


class TestQCopilotsQGISBinaryManager(unittest.TestCase):
    def setUp(self):
        self.fixture = BinaryFixture(self)
        self.manager = self.fixture.manager

    def test_catalog_filters_pagination_and_public_details(self):
        first = self.manager.list_binaries({"limit": 1})
        self.assertEqual(first["total"], 3)
        self.assertEqual(first["binaries"][0]["id"], "alpha")
        self.assertEqual(first["next_cursor"], "1")
        second = self.manager.list_binaries(
            {"limit": 1, "cursor": first["next_cursor"]}
        )
        self.assertEqual(second["binaries"][0]["id"], "blocked")
        enabled_r2 = self.manager.list_binaries({"enabled": True, "risk": "R2"})
        self.assertEqual([item["id"] for item in enabled_r2["binaries"]], ["bravo"])
        queried = self.manager.list_binaries({"query": "first safe"})
        self.assertEqual([item["id"] for item in queried["binaries"]], ["alpha"])
        details = self.manager.get_binary_details({"binary_id": "alpha"})
        self.assertNotIn("resolved_path", details)
        public_catalog = self.manager.load_catalog()
        self.assertTrue(
            all("resolved_path" not in item for item in public_catalog["binaries"])
        )

    def test_bridge_dispatch_rejects_invalid_query_and_state_filters(self):
        bridge = QgisBridgeTools(
            iface=None,
            qgis_binary_job_manager=self.manager,
        )
        with self.assertRaisesRegex(ValueError, "query must be a string"):
            bridge.dispatch("qgis_binary_list_binaries", {"query": 42})
        with self.assertRaisesRegex(ValueError, "non-empty array"):
            bridge.dispatch("qgis_binary_list_jobs", {"states": []})
        with self.assertRaisesRegex(ValueError, "must not contain duplicates"):
            bridge.dispatch(
                "qgis_binary_list_jobs",
                {"states": ["queued", "queued"]},
            )

    def test_risk_confirmation_disabled_policy_and_argv_limit(self):
        with self.assertRaisesRegex(BinaryExecutionError, "confirmation_required"):
            self.manager.start({"binary_id": "bravo"})
        accepted = self.manager.start(
            {"binary_id": "bravo", "confirmed_risk": True}
        )
        self.assertEqual(accepted["state"], "queued")
        with self.assertRaisesRegex(BinaryExecutionError, "qgis_binary_risk_blocked"):
            self.manager.start({"binary_id": "blocked", "confirmed_risk": True})
        with self.assertRaisesRegex(ValueError, "command line limits"):
            self.manager.start(
                {"binary_id": "alpha", "arguments": ["x"] * 257}
            )

    def test_idempotent_replay_and_conflict(self):
        arguments = {
            "binary_id": "alpha",
            "arguments": ["--version"],
            "client_request_id": "same-request",
        }
        first = self.manager.start(arguments)
        replay = self.manager.start(copy.deepcopy(arguments))
        self.assertEqual(replay["job_id"], first["job_id"])
        self.assertEqual(len(self.fixture.task_manager.tasks), 1)
        with self.assertRaises(IdempotencyConflictError):
            self.manager.start(
                {
                    **arguments,
                    "arguments": ["--help"],
                }
            )

    def test_success_lifecycle_progress_and_summary_redaction(self):
        queued = self.manager.start(
            {
                "binary_id": "alpha",
                "arguments": ["--info"],
                "stdin": "abc",
                "working_directory": str(self.fixture.root.parent),
            }
        )
        self.assertEqual(queued["execution_mode"], "qgs_task")
        self.assertTrue(queued["cancellable"])
        self.assertNotIn("result", queued)
        task = self.fixture.task_manager.tasks[-1]
        task.begin()
        self.assertEqual(self.manager.get(queued["job_id"])["state"], "running")
        self.manager._on_progress(queued["job_id"], 37.5)
        self.assertEqual(task.progress_values, [37.5])
        task.execution.outcome["stdout"] = "large output" * 100
        task.finish(True)
        succeeded = self.manager.get({"job_id": queued["job_id"]})
        self.assertEqual(succeeded["state"], "succeeded")
        self.assertEqual(succeeded["progress_percent"], 100.0)
        self.assertIn("result", succeeded)
        self.assertIsNone(succeeded["error"])
        execution_values = self.fixture.executions[-1].values
        self.assertEqual(execution_values["arguments"], ["--info"])
        self.assertEqual(execution_values["stdin_bytes"], b"abc")
        self.assertEqual(
            Path(execution_values["working_directory"]).resolve(),
            self.fixture.root.parent.resolve(),
        )
        summary = self.manager.list_jobs({"states": ["succeeded"]})["jobs"][0]
        self.assertNotIn("result", summary)
        self.assertNotIn("stdout", json.dumps(summary))

    def test_restricted_policy_allows_normal_local_working_directories(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        outside_root = Path(policy_workspace.name) / "outside"
        relative_root = self.fixture.root / "relative"
        runs_root.mkdir()
        relative_root.mkdir()
        outside_root.mkdir()
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy

        default_job = self.manager.start(
            {"binary_id": "alpha", "client_request_id": "restricted-default"}
        )
        explicit_job = self.manager.start(
            {
                "binary_id": "alpha",
                "working_directory": str(runs_root),
                "client_request_id": "restricted-explicit",
            }
        )
        relative_job = self.manager.start(
            {
                "binary_id": "alpha",
                "working_directory": "relative",
                "client_request_id": "restricted-relative",
            }
        )
        outside_job = self.manager.start(
            {
                "binary_id": "alpha",
                "working_directory": str(outside_root),
                "client_request_id": "restricted-outside",
            }
        )
        self.assertEqual(default_job["state"], "queued")
        self.assertEqual(explicit_job["state"], "queued")
        self.assertEqual(relative_job["state"], "queued")
        self.assertEqual(outside_job["state"], "queued")
        self.assertEqual(
            Path(self.fixture.executions[-4].values["working_directory"]),
            self.fixture.root.resolve(),
        )
        self.assertEqual(
            Path(self.fixture.executions[-3].values["working_directory"]),
            runs_root.resolve(),
        )
        self.assertEqual(
            Path(self.fixture.executions[-2].values["working_directory"]),
            relative_root.resolve(),
        )
        self.assertEqual(
            Path(self.fixture.executions[-1].values["working_directory"]),
            outside_root.resolve(),
        )

    def test_restricted_default_working_directory_revalidates_package_root(self):
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        original_is_symlink = Path.is_symlink

        def simulated_root_replacement(candidate):
            return candidate == self.fixture.root or original_is_symlink(candidate)

        with mock.patch.object(Path, "is_symlink", simulated_root_replacement):
            with self.assertRaisesRegex(
                PermissionError, "symbolic links and junctions"
            ):
                self.manager.start(
                    {
                        "binary_id": "alpha",
                        "client_request_id": "replaced-default-package-root",
                    }
                )
        self.assertEqual(self.manager.list_jobs({})["jobs"], [])
        self.assertEqual(self.fixture.executions, [])
        self.assertEqual(self.fixture.task_manager.tasks, [])

    def test_restricted_policy_allows_approved_argument_paths_and_nonpaths(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        input_directory = runs_root / "input"
        output_directory = runs_root / "outputs"
        runs_root.mkdir()
        input_directory.mkdir()
        output_directory.mkdir()
        source = input_directory / "source.tif"
        source.write_bytes(b"source")
        second_source = input_directory / "second-source.gpkg"
        second_source.write_bytes(b"second source")
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {
                    "enabled": True,
                    "allowed_origins": ["http://127.0.0.1:49180"],
                },
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        argv = [
            str(source),
            f"--output={output_directory / 'result.tif'}",
            f"INPUT={source}",
            f"LAYERS={source};{second_source}",
            (
                f"LAYERS={source}|layername=first;"
                f"{second_source}|layername=second"
            ),
            "outputs/relative-result.tif",
            "--format=GTiff",
            "EPSG:4326",
            "urn:ogc:def:crs:EPSG::4326",
            "field:value",
            "field:CON",
            "--sql=SELECT value FROM roads WHERE field = 'a:b';",
            "--sql=SELECT 'CON' AS field;",
            "3.14",
            "foo.bar",
            "http://127.0.0.1:49180/data.tif",
            "http://127.0.0.1:49180/CON",
            "--source=http://127.0.0.1:49180/input.tif?part=1",
            "/vsimem/result.tif",
            "/vsimem/CON",
        ]

        queued = self.manager.start(
            {
                "binary_id": "alpha",
                "arguments": argv,
                "working_directory": str(runs_root),
            }
        )

        self.assertEqual(queued["state"], "queued")
        self.assertEqual(self.fixture.executions[-1].values["arguments"], argv)

    def test_restricted_policy_allows_normal_local_argument_paths_outside_cwd(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        runs_root.mkdir()
        read_source = self.fixture.root / "read-only-source.tif"
        read_source.write_bytes(b"source")
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        allowed_argvs = (
            [str(read_source)],
            [f"--input={read_source}"],
            [f"INPUT={read_source}"],
            [f"LAYERS={runs_root / 'staged.tif'};{read_source}"],
        )

        for index, argv in enumerate(allowed_argvs):
            with self.subTest(argv=argv):
                queued = self.manager.start(
                    {
                        "binary_id": "alpha",
                        "arguments": argv,
                        "working_directory": str(runs_root),
                        "client_request_id": f"global-local-argv-{index}",
                    }
                )
                self.assertEqual(queued["state"], "queued")
                self.assertEqual(self.fixture.executions[-1].values["arguments"], argv)

        task_count = len(self.fixture.task_manager.tasks)
        with self.assertRaisesRegex(PermissionError, "prohibited local path"):
            self.manager.start(
                {
                    "binary_id": "alpha",
                    "arguments": [read_source.as_uri()],
                    "working_directory": str(runs_root),
                    "client_request_id": "file-uri-is-not-a-local-path",
                }
            )
        self.assertEqual(len(self.fixture.task_manager.tasks), task_count)

    @unittest.skipUnless(os.name == "nt", "Windows reserved device names")
    def test_restricted_policy_rejects_device_arguments_before_path_probes(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        runs_root.mkdir()
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        blocked_argvs = (
            ["CON"],
            ["--output=CON.txt"],
            ["--output:CONOUT$"],
            ["OUTPUT=COM1"],
            ["LAYERS=staged.tif;LPT1|layername=blocked"],
            ["--config=OUTPUT=AUX.txt"],
            ["COM1:stream"],
            ["--output=PRN."],
        )

        with mock.patch(
            "qcopilots_common.qgis_binary_jobs._binary_argument_local_path_candidates",
            side_effect=AssertionError("path metadata must not be probed"),
        ) as path_candidates:
            for index, argv in enumerate(blocked_argvs):
                with self.subTest(argv=argv), mock.patch.object(
                    self.manager.store,
                    "lookup_idempotent",
                    wraps=self.manager.store.lookup_idempotent,
                ) as lookup_idempotent, mock.patch.object(
                    self.manager.store,
                    "create",
                    wraps=self.manager.store.create,
                ) as create_job:
                    with self.assertRaisesRegex(
                        PermissionError, "prohibited Windows device path"
                    ):
                        self.manager.start(
                            {
                                "binary_id": "alpha",
                                "arguments": argv,
                                "working_directory": str(runs_root),
                                "client_request_id": f"device-argv-{index}",
                            }
                        )
                    lookup_idempotent.assert_not_called()
                    create_job.assert_not_called()
        path_candidates.assert_not_called()
        self.assertEqual(self.manager.list_jobs({})["jobs"], [])
        self.assertEqual(self.fixture.executions, [])
        self.assertEqual(self.fixture.task_manager.tasks, [])

    def test_restricted_policy_rejects_hardlinked_argument_and_ads_base(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        runs_root.mkdir()
        source = self.fixture.root / "hardlink-source.gpkg"
        source.write_bytes(b"source remains unchanged")
        alias = runs_root / "alias.gpkg"
        os.link(source, alias)
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        blocked_argvs = [([str(alias)], "multiple hard links")]
        if os.name == "nt":
            blocked_argvs.append(
                ([f"--output={alias}:stream"], "alternate data streams")
            )

        for index, (argv, expected_error) in enumerate(blocked_argvs):
            with self.subTest(argv=argv), mock.patch.object(
                self.manager.store,
                "lookup_idempotent",
                wraps=self.manager.store.lookup_idempotent,
            ) as lookup_idempotent, mock.patch.object(
                self.manager.store,
                "create",
                wraps=self.manager.store.create,
            ) as create_job:
                with self.assertRaisesRegex(PermissionError, expected_error):
                    self.manager.start(
                        {
                            "binary_id": "alpha",
                            "arguments": argv,
                            "working_directory": str(runs_root),
                            "client_request_id": f"hardlink-argv-{index}",
                        }
                    )
                lookup_idempotent.assert_not_called()
                create_job.assert_not_called()
                self.assertEqual(source.read_bytes(), b"source remains unchanged")

        self.assertEqual(self.manager.list_jobs({})["jobs"], [])
        self.assertEqual(self.fixture.executions, [])
        self.assertEqual(self.fixture.task_manager.tasks, [])

    def test_restricted_policy_rejects_nonempty_stdin_before_job_creation(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        runs_root.mkdir()
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy

        for index, stdin_value in enumerate(
            (
                str(self.fixture.root / "source.tif"),
                "PG:host=example.invalid dbname=outside",
                '<include href="file:///C:/outside.xml"/>',
                "   ",
                "\t\r\n",
            )
        ):
            with self.subTest(stdin=stdin_value), mock.patch.object(
                self.manager.store,
                "lookup_idempotent",
                wraps=self.manager.store.lookup_idempotent,
            ) as lookup_idempotent, mock.patch.object(
                self.manager.store,
                "create",
                wraps=self.manager.store.create,
            ) as create_job:
                task_count = len(self.fixture.task_manager.tasks)
                execution_count = len(self.fixture.executions)
                with self.assertRaisesRegex(PermissionError, "non-empty binary stdin"):
                    self.manager.start(
                        {
                            "binary_id": "alpha",
                            "working_directory": str(runs_root),
                            "stdin": stdin_value,
                            "client_request_id": f"restricted-stdin-{index}",
                        }
                    )
                lookup_idempotent.assert_not_called()
                create_job.assert_not_called()
                self.assertEqual(len(self.fixture.task_manager.tasks), task_count)
                self.assertEqual(len(self.fixture.executions), execution_count)

        for index, stdin_value in enumerate((None, "")):
            queued = self.manager.start(
                {
                    "binary_id": "alpha",
                    "stdin": stdin_value,
                    "client_request_id": f"restricted-empty-stdin-{index}",
                }
            )
            self.assertEqual(queued["state"], "queued")
            expected = None if stdin_value is None else b""
            self.assertEqual(self.fixture.executions[-1].values["stdin_bytes"], expected)

        self.assertEqual(len(self.manager.list_jobs({})["jobs"]), 2)

    def test_restricted_policy_rejects_indirect_argv_before_idempotency_and_store(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        runs_root.mkdir()
        option_file = self.fixture.root / "approved-root-options.txt"
        option_file.write_text(
            "PG:host=example.invalid dbname=outside\n",
            encoding="utf-8",
        )
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        blocked_argvs = (
            [f"@{option_file}"],
            [f"--response=@{option_file}"],
            ["--optfile", str(option_file)],
            [f"--optfile={option_file}"],
            [f"--optfile:{option_file}"],
            ["--OPTFILE", str(option_file)],
            ["--options-file", str(option_file)],
        )

        for index, argv in enumerate(blocked_argvs):
            with self.subTest(argv=argv), mock.patch.object(
                self.manager.store,
                "lookup_idempotent",
                wraps=self.manager.store.lookup_idempotent,
            ) as lookup_idempotent, mock.patch.object(
                self.manager.store,
                "create",
                wraps=self.manager.store.create,
            ) as create_job, mock.patch.object(
                Path,
                "read_text",
                side_effect=AssertionError("option file content must not be read"),
            ), mock.patch.object(
                Path,
                "read_bytes",
                side_effect=AssertionError("option file content must not be read"),
            ):
                task_count = len(self.fixture.task_manager.tasks)
                execution_count = len(self.fixture.executions)
                with self.assertRaisesRegex(PermissionError, "indirect argument file"):
                    self.manager.start(
                        {
                            "binary_id": "alpha",
                            "arguments": argv,
                            "working_directory": str(runs_root),
                            "client_request_id": f"indirect-argv-{index}",
                        }
                    )
                lookup_idempotent.assert_not_called()
                create_job.assert_not_called()
                self.assertEqual(len(self.fixture.task_manager.tasks), task_count)
                self.assertEqual(len(self.fixture.executions), execution_count)

        self.assertEqual(self.manager.list_jobs({})["jobs"], [])

    def test_restricted_working_directory_syntax_rejects_without_path_probes(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        runs_root.mkdir()
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        blocked_working_directories = (
            r"\\server.invalid\share",
            r"\\?\C:\outside",
            r"\\.\C:\outside",
            r"\??\C:\outside",
            r"C:outside",
            str(runs_root / "NUL:stream"),
            str(runs_root / "COM\u00b9.txt"),
            str(runs_root / "CONOUT$"),
            str(runs_root) + "\0outside",
            runs_root.as_uri(),
        )

        with mock.patch.object(
            FilesystemPolicy,
            "resolve_path",
            side_effect=AssertionError("policy.resolve_path must not be called"),
        ) as resolve_path, mock.patch.object(
            Path,
            "resolve",
            side_effect=AssertionError("Path.resolve must not be called"),
        ), mock.patch.object(
            Path,
            "is_symlink",
            side_effect=AssertionError("path metadata must not be probed"),
        ), mock.patch.object(
            Path,
            "is_dir",
            side_effect=AssertionError("path metadata must not be probed"),
        ):
            for index, working_directory in enumerate(blocked_working_directories):
                with self.subTest(working_directory=working_directory):
                    with self.assertRaises(PermissionError):
                        self.manager.start(
                            {
                                "binary_id": "alpha",
                                "working_directory": working_directory,
                                "client_request_id": f"blocked-cwd-{index}",
                            }
                        )
        resolve_path.assert_not_called()
        self.assertEqual(self.manager.list_jobs({})["jobs"], [])
        self.assertEqual(self.fixture.executions, [])
        self.assertEqual(self.fixture.task_manager.tasks, [])

    def test_restricted_policy_allows_global_paths_and_rejects_unsafe_forms(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        outside_root = Path(policy_workspace.name) / "outside"
        runs_root.mkdir()
        outside_root.mkdir()
        outside_file = outside_root / "outside.tif"
        outside_file.write_bytes(b"outside")
        source = self.fixture.root / "source.tif"
        source.write_bytes(b"source")
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        allowed_arguments = [
            str(outside_file),
            f"--output={outside_file}",
            f"INPUT={outside_file}",
            f"--config=PATH={outside_file}",
            f"--output:{outside_file}",
            f"INPUT={source};{outside_file}",
            (
                f"INPUT={source}|layername=inside;"
                f"{outside_file}|layername=outside"
            ),
            f"--inputs={source};{outside_file}",
        ]
        for index, allowed_argument in enumerate(allowed_arguments):
            with self.subTest(allowed_argument=allowed_argument):
                queued = self.manager.start(
                    {
                        "binary_id": "alpha",
                        "arguments": [allowed_argument],
                        "working_directory": str(runs_root),
                        "client_request_id": f"global-argument-{index}",
                    }
                )
                self.assertEqual(queued["state"], "queued")

        blocked_arguments = [
            f"@{outside_file}",
            f"--response=@{outside_file}",
            outside_file.as_uri(),
            f"--input={outside_file.as_uri()}",
            "../escape.tif",
            r"..\escape.tif",
            r"C:escape.tif",
            r"\\server\share\escape.tif",
            r"\\?\C:\escape.tif",
            r"\\.\C:\escape.tif",
            r"\??\C:\escape.tif",
            f"GPKG:{source}",
            f"SQLite:{source}",
        ]

        for index, blocked_argument in enumerate(blocked_arguments):
            with self.subTest(argument=blocked_argument):
                task_count = len(self.fixture.task_manager.tasks)
                execution_count = len(self.fixture.executions)
                job_count = len(self.manager.list_jobs({})["jobs"])
                with self.assertRaisesRegex(
                    PermissionError, "QGIS binary argument 0"
                ):
                    self.manager.start(
                        {
                            "binary_id": "alpha",
                            "arguments": [blocked_argument],
                            "working_directory": str(runs_root),
                            "client_request_id": f"blocked-argument-{index}",
                        }
                    )
                self.assertEqual(len(self.fixture.task_manager.tasks), task_count)
                self.assertEqual(len(self.fixture.executions), execution_count)
                self.assertEqual(len(self.manager.list_jobs({})["jobs"]), job_count)

    def test_restricted_policy_rejects_reparse_argument_escape(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        reparse_directory = runs_root / "outside-link"
        runs_root.mkdir()
        reparse_directory.mkdir()
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        original_is_symlink = Path.is_symlink

        def simulated_reparse(candidate):
            return candidate == reparse_directory or original_is_symlink(candidate)

        with mock.patch.object(Path, "is_symlink", simulated_reparse):
            with self.assertRaisesRegex(PermissionError, "symbolic links and junctions"):
                self.manager.start(
                    {
                        "binary_id": "alpha",
                        "arguments": ["--output=outside-link/result.tif"],
                        "working_directory": str(runs_root),
                    }
                )
        self.assertEqual(self.manager.list_jobs({})["jobs"], [])
        self.assertEqual(self.fixture.executions, [])
        self.assertEqual(self.fixture.task_manager.tasks, [])

    def test_restricted_policy_enforces_argument_network_boundary(self):
        policy_workspace = tempfile.TemporaryDirectory()
        self.addCleanup(policy_workspace.cleanup)
        runs_root = Path(policy_workspace.name) / "runs"
        runs_root.mkdir()
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {
                    "enabled": True,
                    "allowed_origins": ["http://127.0.0.1:49180"],
                },
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy
        blocked_sources = [
            "http://example.com/data.tif",
            "--url=https://127.0.0.1:49180/data.tif",
            "ftp://127.0.0.1:49180/data.tif",
            "SOURCE=ftp://127.0.0.1:49180/data.tif",
            "/vsicurl/http://127.0.0.1:49180/data.tif",
            "--source=/vsis3/bucket/data.tif",
            "PG:host=example.com port=5432 dbname=outside",
            "MYSQL:host=example.com,db=outside",
            "MSSQL:server=example.com;database=outside",
            "OCI:user/password@example.com/service",
            "ODBC:DSN=outside",
            "WFS:http://example.com/wfs",
            "CouchDB:http://example.com/database",
            "Elasticsearch:http://example.com/index",
            "MongoDBv3:mongodb://example.com/database",
            "--datasource=PG:host=example.com dbname=outside",
            "--datasource:PG:host=127.0.0.1 dbname=outside",
        ]

        for blocked_source in blocked_sources:
            with self.subTest(source=blocked_source):
                with self.assertRaisesRegex(PermissionError, "prohibited source"):
                    self.manager.start(
                        {
                            "binary_id": "alpha",
                            "arguments": [blocked_source],
                            "working_directory": str(runs_root),
                        }
                    )
        disabled_network_policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = disabled_network_policy
        for disabled_network_source in (
            "http://127.0.0.1:49180/data.tif",
            "PG:host=127.0.0.1 port=5432 dbname=outside",
            "--datasource=MYSQL:host=127.0.0.1,db=outside",
        ):
            with self.subTest(disabled_network_source=disabled_network_source):
                with self.assertRaisesRegex(PermissionError, "prohibited source"):
                    self.manager.start(
                        {
                            "binary_id": "alpha",
                            "arguments": [disabled_network_source],
                            "working_directory": str(runs_root),
                        }
                    )
        self.assertEqual(self.manager.list_jobs({})["jobs"], [])
        self.assertEqual(self.fixture.executions, [])
        self.assertEqual(self.fixture.task_manager.tasks, [])

    def test_restricted_argument_validation_precedes_idempotent_replay(self):
        outside = self.fixture.root.parent / "idempotent-outside.tif"
        request = {
            "binary_id": "alpha",
            "arguments": [outside.as_uri()],
            "client_request_id": "policy-transition",
        }
        original = self.manager.start(request)
        policy = filesystem_policy_from_config(
            {
                "mode": "formal_restricted",
                "shell": {"enabled": False, "executables": []},
                "network": {"enabled": False, "allowed_origins": []},
            }
        )
        self.manager._dependencies["filesystem_policy"] = policy

        with self.assertRaisesRegex(PermissionError, "prohibited local path"):
            self.manager.start(request)

        self.assertEqual(
            [item["job_id"] for item in self.manager.list_jobs({})["jobs"]],
            [original["job_id"]],
        )
        self.assertEqual(len(self.fixture.executions), 1)
        self.assertEqual(len(self.fixture.task_manager.tasks), 1)

    def test_compatible_policy_keeps_binary_argument_behavior(self):
        outside = self.fixture.root.parent / "compatible-outside.tif"
        argv = [
            str(outside),
            f"--output={outside}",
            outside.as_uri(),
            "../escape.tif",
            "http://example.com/data.tif",
            "/vsicurl/http://example.com/data.tif",
            f"GPKG:{outside}",
            f"--source=SQLite:{outside}",
            f"CSV:{outside}",
            "EPSG:4326",
            "PG:host=example.com dbname=qgis",
        ]

        stdin_value = r"C:\outside.tif"
        queued = self.manager.start(
            {"binary_id": "alpha", "arguments": argv, "stdin": stdin_value}
        )

        self.assertEqual(queued["state"], "queued")
        self.assertEqual(self.fixture.executions[-1].values["arguments"], argv)
        self.assertEqual(
            self.fixture.executions[-1].values["stdin_bytes"],
            stdin_value.encode("utf-8"),
        )
        self.assertEqual(
            Path(self.fixture.executions[-1].values["working_directory"]),
            self.fixture.root.resolve(),
        )

        task_count = len(self.fixture.task_manager.tasks)
        execution_count = len(self.fixture.executions)
        for indirect_argument in (
            f"@{outside}",
            "--optfile",
            f"--response-file={outside}",
        ):
            with self.subTest(indirect_argument=indirect_argument):
                with self.assertRaisesRegex(PermissionError, "indirect argument file"):
                    self.manager.start(
                        {
                            "binary_id": "alpha",
                            "arguments": [indirect_argument],
                        }
                    )
        self.assertEqual(len(self.fixture.task_manager.tasks), task_count)
        self.assertEqual(len(self.fixture.executions), execution_count)

    @unittest.skipUnless(os.name == "nt", "Windows local path safety")
    def test_compatible_policy_rejects_unsafe_binary_local_paths(self):
        base_file = self.fixture.root / "compatible-base.tif"
        base_file.write_bytes(b"data")
        blocked_arguments = (
            r"\\server.invalid\share\outside.tif",
            r"\\?\C:\outside.tif",
            f"--output={base_file}:stream",
            r"GPKG:\\server.invalid\share\outside.gpkg",
            r"SQLite:\\?\C:\outside.sqlite",
            f"--source=PGEO:{base_file}:stream",
        )
        for blocked_argument in blocked_arguments:
            with self.subTest(blocked_argument=blocked_argument), self.assertRaises(
                PermissionError
            ):
                self.manager.start(
                    {
                        "binary_id": "alpha",
                        "arguments": [blocked_argument],
                    }
                )
        reparse_directory = self.fixture.root / "compatible-prefix-reparse"
        reparse_directory.mkdir()
        original_is_symlink = Path.is_symlink

        def simulated_reparse(candidate):
            return candidate == reparse_directory or original_is_symlink(candidate)

        with mock.patch.object(Path, "is_symlink", simulated_reparse):
            with self.assertRaisesRegex(PermissionError, "symbolic links and junctions"):
                self.manager.start(
                    {
                        "binary_id": "alpha",
                        "arguments": [f"GPKG:{reparse_directory / 'outside.gpkg'}"],
                    }
                )
        self.assertEqual(self.manager.list_jobs({})["jobs"], [])
        self.assertEqual(self.fixture.executions, [])

    def test_binary_argument_path_with_equals_is_validated_as_a_whole(self):
        from qcopilots_common.security_policy import FilesystemPolicy

        argument_path = self.fixture.root / "data=a.tif"
        original_resolve_path = FilesystemPolicy.resolve_path
        resolved_values = []

        def resolve_path(policy, value, *, access, base=None):
            resolved_values.append((str(value), access))
            return original_resolve_path(policy, value, access=access, base=base)

        with mock.patch.object(FilesystemPolicy, "resolve_path", resolve_path):
            queued = self.manager.start(
                {
                    "binary_id": "alpha",
                    "arguments": [str(argument_path)],
                }
            )

        self.assertEqual(queued["state"], "queued")
        self.assertIn((str(argument_path), "write"), resolved_values)

    def test_compatible_working_directory_uses_shared_filesystem_policy(self):
        runs_root = self.fixture.root / "compatible-runs"
        runs_root.mkdir()
        original_resolve_path = FilesystemPolicy.resolve_path
        resolve_calls = []

        def resolve_path(policy, value, *, access, base=None):
            resolve_calls.append((value, access, base))
            return original_resolve_path(
                policy,
                value,
                access=access,
                base=base,
            )

        with mock.patch.object(
            FilesystemPolicy,
            "resolve_path",
            autospec=True,
            side_effect=resolve_path,
        ):
            queued = self.manager.start(
                {
                    "binary_id": "alpha",
                    "working_directory": "compatible-runs",
                    "client_request_id": "compatible-safe-cwd",
                }
            )

        self.assertEqual(queued["state"], "queued")
        self.assertIn(
            ("compatible-runs", "write", self.fixture.root),
            resolve_calls,
        )
        self.assertEqual(
            Path(self.fixture.executions[-1].values["working_directory"]),
            runs_root.resolve(),
        )

        task_count = len(self.fixture.task_manager.tasks)
        execution_count = len(self.fixture.executions)
        with self.assertRaisesRegex(PermissionError, "device paths"):
            self.manager.start(
                {
                    "binary_id": "alpha",
                    "working_directory": r"\\?\C:\outside",
                    "client_request_id": "compatible-unsafe-cwd",
                }
            )
        self.assertEqual(len(self.fixture.task_manager.tasks), task_count)
        self.assertEqual(len(self.fixture.executions), execution_count)

    def test_indeterminate_success_keeps_null_progress(self):
        document = _catalog_document()
        document["binaries"][0]["progress_parser"] = None
        fixture = BinaryFixture(self, document=document)
        queued = fixture.manager.start({"binary_id": "alpha"})
        task = fixture.task_manager.tasks[-1]
        task.begin()
        task.finish(True)
        snapshot = fixture.manager.get(queued["job_id"])
        self.assertEqual(snapshot["state"], "succeeded")
        self.assertIsNone(snapshot["progress_percent"])

        configured = self.manager.start(
            {"binary_id": "alpha", "client_request_id": "configured-progress"}
        )
        configured_task = self.fixture.task_manager.tasks[-1]
        configured_task.begin()
        configured_task.finish(True)
        configured_snapshot = self.manager.get(configured["job_id"])
        self.assertEqual(configured_snapshot["progress_percent"], 100.0)

    def test_late_cancel_request_does_not_override_completed_outcome(self):
        success = self.manager.start(
            {"binary_id": "alpha", "client_request_id": "late-cancel-success"}
        )
        success_task = self.fixture.task_manager.tasks[-1]
        success_task.begin()
        self.manager.cancel(success["job_id"])
        success_task.execution.outcome["cancelled"] = False
        success_task.finish(True)
        succeeded = self.manager.get(success["job_id"])
        self.assertEqual(succeeded["state"], "succeeded")
        self.assertTrue(succeeded["cancel_requested"])
        self.assertIn("result", succeeded)

        failures = [
            ({"exit_code": 17}, "process_exit_failed"),
            ({"timed_out": True}, "process_timed_out"),
            ({"environment_error": "bad environment"}, "environment_setup_failed"),
            ({"start_error": "bad start"}, "process_start_failed"),
        ]
        for index, (changes, error_code) in enumerate(failures):
            queued = self.manager.start(
                {
                    "binary_id": "alpha",
                    "client_request_id": f"late-cancel-failure-{index}",
                }
            )
            task = self.fixture.task_manager.tasks[-1]
            task.begin()
            task.execution.outcome.update(changes)
            self.manager.cancel(queued["job_id"])
            task.execution.outcome["cancelled"] = False
            task.finish(False)
            snapshot = self.manager.get(queued["job_id"])
            self.assertEqual(snapshot["state"], "failed")
            self.assertTrue(snapshot["cancel_requested"])
            self.assertEqual(snapshot["error"]["code"], error_code)
            self.assertNotIn("result", snapshot)

    def test_failure_outcomes_have_structured_error_and_no_result(self):
        cases = [
            ({"environment_error": "environment broke"}, "environment_setup_failed"),
            ({"start_error": "start broke"}, "process_start_failed"),
            ({"timed_out": True}, "process_timed_out"),
            ({"exit_code": 9}, "process_exit_failed"),
        ]
        for index, (changes, expected_code) in enumerate(cases):
            queued = self.manager.start(
                {
                    "binary_id": "alpha",
                    "client_request_id": f"failure-{index}",
                }
            )
            task = self.fixture.task_manager.tasks[-1]
            task.begin()
            task.execution.outcome.update(changes)
            task.execution.outcome["stdout"] = "x" * 1000
            task.execution.outcome["stderr"] = "y" * 1000
            task.finish(False)
            snapshot = self.manager.get(queued["job_id"])
            self.assertEqual(snapshot["state"], "failed")
            self.assertEqual(snapshot["error"]["code"], expected_code)
            self.assertIn("details", snapshot["error"])
            self.assertNotIn("result", snapshot)
        summaries = self.manager.list_jobs({"states": ["failed"]})["jobs"]
        self.assertEqual(len(summaries), 4)
        for summary in summaries:
            self.assertEqual(set(summary["error"]), {"code", "stage", "message"})
            self.assertNotIn("result", summary)
            self.assertNotIn("stdout", json.dumps(summary))

    def test_api_and_external_task_cancellation(self):
        queued = self.manager.start({"binary_id": "alpha"})
        task = self.fixture.task_manager.tasks[-1]
        task.begin()
        cancelling = self.manager.cancel(queued["job_id"])
        self.assertEqual(cancelling["state"], "cancelling")
        self.assertTrue(cancelling["cancel_requested"])
        task.execution.outcome["cancelled"] = True
        task.finish(False)
        cancelled = self.manager.get(queued["job_id"])
        self.assertEqual(cancelled["state"], "cancelled")
        self.assertNotIn("result", cancelled)

        external = self.manager.start(
            {"binary_id": "alpha", "client_request_id": "external-cancel"}
        )
        external_task = self.fixture.task_manager.tasks[-1]
        external_task.begin()
        external_task.cancel()
        external_task.execution.outcome["cancelled"] = True
        external_task.finish(False)
        external_snapshot = self.manager.get(external["job_id"])
        self.assertEqual(external_snapshot["state"], "cancelled")
        self.assertTrue(external_snapshot["cancel_requested"])

    def test_queued_external_task_cancel_never_runs_process(self):
        queued = self.manager.start(
            {"binary_id": "alpha", "client_request_id": "queued-external-cancel"}
        )
        task = self.fixture.task_manager.tasks[-1]
        task.execution.outcome["exit_code"] = None
        task.cancel()
        outcome = task.execution.outcome_snapshot()
        self.assertTrue(outcome["cancel_requested"])
        self.assertFalse(outcome["cancelled"])
        task.finish(False)
        snapshot = self.manager.get(queued["job_id"])
        self.assertEqual(snapshot["state"], "cancelled")
        self.assertTrue(snapshot["cancel_requested"])
        self.assertIsNone(snapshot["started_at"])
        self.assertNotIn("result", snapshot)

    def test_process_tree_termination_failure_is_structured_failure(self):
        queued = self.manager.start(
            {"binary_id": "alpha", "client_request_id": "termination-failed"}
        )
        task = self.fixture.task_manager.tasks[-1]
        task.begin()
        task.execution.outcome.update(
            {
                "exit_code": None,
                "cancelled": False,
                "termination_error": "CTRL_BREAK, taskkill, and kill all failed",
            }
        )
        self.manager.cancel(queued["job_id"])
        task.finish(False)
        snapshot = self.manager.get(queued["job_id"])
        self.assertEqual(snapshot["state"], "failed")
        self.assertEqual(snapshot["error"]["code"], "process_termination_failed")
        self.assertEqual(snapshot["error"]["stage"], "cancel")
        self.assertIn("termination_error", snapshot["error"]["details"])
        self.assertNotIn("result", snapshot)

    def test_shutdown_cancels_active_jobs_and_rejects_new_jobs(self):
        queued = self.manager.start({"binary_id": "alpha"})
        task = self.fixture.task_manager.tasks[-1]
        task.begin()
        task.execution.outcome["exit_code"] = None
        self.manager.shutdown(timeout_seconds=0.1)
        self.assertFalse(self.manager.accepting)
        self.assertTrue(task.execution.cancelled)
        self.assertEqual(self.manager.get(queued["job_id"])["state"], "cancelled")
        with self.assertRaisesRegex(RuntimeError, "stopping"):
            self.manager.start({"binary_id": "alpha"})

    def test_shutdown_wait_timeout_keeps_live_process_cancelling(self):
        queued = self.manager.start(
            {"binary_id": "alpha", "client_request_id": "shutdown-live-process"}
        )
        task = self.fixture.task_manager.tasks[-1]
        task.begin()
        task.wait_result = False
        task.execution.outcome.update(
            {
                "exit_code": None,
                "process_alive": True,
                "cancelled": False,
                "termination_error": None,
            }
        )

        self.manager.shutdown(timeout_seconds=0.1)

        snapshot = self.manager.get(queued["job_id"])
        self.assertEqual(snapshot["state"], "cancelling")
        self.assertTrue(snapshot["cancel_requested"])
        self.assertIsNone(snapshot["finished_at"])
        self.assertIsNone(snapshot["error"])
        self.assertTrue(task.wait_arguments)

    def test_shutdown_reports_live_process_termination_failure(self):
        queued = self.manager.start(
            {
                "binary_id": "alpha",
                "client_request_id": "shutdown-termination-failure",
            }
        )
        task = self.fixture.task_manager.tasks[-1]
        task.begin()
        task.wait_result = False
        task.execution.outcome.update(
            {
                "exit_code": None,
                "process_alive": True,
                "cancelled": False,
                "termination_error": "process tree is still alive",
            }
        )

        self.manager.shutdown(timeout_seconds=0.1)

        snapshot = self.manager.get(queued["job_id"])
        self.assertEqual(snapshot["state"], "failed")
        self.assertTrue(snapshot["cancel_requested"])
        self.assertEqual(snapshot["error"]["code"], "process_termination_failed")
        self.assertEqual(snapshot["error"]["stage"], "cancel")
        self.assertTrue(snapshot["error"]["details"]["process_alive"])

    def test_shutdown_cannot_race_post_create_task_registration(self):
        store = PausingCreateStore()
        fixture = BinaryFixture(self, store=store)
        results = []
        errors = []

        def start_job():
            try:
                results.append(fixture.manager.start({"binary_id": "alpha"}))
            except Exception as error:
                errors.append(error)

        thread = threading.Thread(target=start_job)
        thread.start()
        try:
            self.assertTrue(store.created.wait(2))
            fixture.manager.shutdown(timeout_seconds=0)
            self.assertFalse(fixture.manager.accepting)
            self.assertEqual(fixture.task_manager.tasks, [])
        finally:
            store.release.set()
            thread.join(2)

        self.assertFalse(thread.is_alive())
        self.assertFalse(errors)
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]["state"], "cancelled")
        self.assertTrue(results[0]["cancel_requested"])
        self.assertEqual(fixture.task_manager.tasks, [])

    def test_cancel_cannot_race_post_create_task_registration(self):
        store = PausingCreateStore()
        fixture = BinaryFixture(self, store=store)
        results = []
        errors = []

        def start_job():
            try:
                results.append(fixture.manager.start({"binary_id": "alpha"}))
            except Exception as error:
                errors.append(error)

        thread = threading.Thread(target=start_job)
        thread.start()
        try:
            self.assertTrue(store.created.wait(2))
            visible = store.list("qgis_binary", limit=1)[0]
            cancelling = fixture.manager.cancel(visible["job_id"])
            self.assertEqual(cancelling["state"], "cancelling")
            self.assertEqual(fixture.task_manager.tasks, [])
        finally:
            store.release.set()
            thread.join(2)

        self.assertFalse(thread.is_alive())
        self.assertFalse(errors)
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]["state"], "cancelled")
        self.assertTrue(results[0]["cancel_requested"])
        self.assertEqual(fixture.task_manager.tasks, [])

    def test_environment_capture_is_cached_concurrently_and_does_not_inherit(self):
        calls = []
        entered = threading.Event()
        release = threading.Event()

        def run_factory(command, **keywords):
            capture_script = keywords["env"]["QCOPILOTS_CAPTURE_SCRIPT_0"]
            calls.append((command, keywords, capture_script))
            entered.set()
            release.wait(2)
            return SimpleNamespace(
                returncode=0,
                stdout="CAPTURED=value\r\n".encode("utf-16-le"),
                stderr="".encode("utf-16-le"),
            )

        fixture = BinaryFixture(
            self,
            dependencies={"run_factory": run_factory},
            unicode_root=True,
        )
        results = []
        errors = []

        def load_environment():
            try:
                results.append(fixture.manager._environment_for("qgis"))
            except Exception as error:
                errors.append(error)

        threads = [threading.Thread(target=load_environment) for _ in range(8)]
        for thread in threads:
            thread.start()
        self.assertTrue(entered.wait(2))
        release.set()
        for thread in threads:
            thread.join(2)
        self.assertFalse(errors)
        self.assertEqual(len(results), 8)
        self.assertEqual(len(calls), 1)
        command, keywords, capture_script = calls[0]
        trusted_cmd = str(Path(os.environ["SystemRoot"]) / "System32" / "cmd.exe")
        self.assertIsInstance(command, str)
        self.assertTrue(command.casefold().startswith(trusted_cmd.casefold()))
        self.assertIn(" /u /d /s /c ", command)
        self.assertFalse(keywords["shell"])
        self.assertTrue(
            os.path.samefile(
                capture_script,
                fixture.root / "bin" / "qgis-qt6-env.bat",
            )
        )
        self.assertNotIn("PARENT_ONLY", keywords["env"])
        self.assertTrue(all("PARENT_ONLY" not in result for result in results))
        self.assertTrue(all(result["CAPTURED"] == "value" for result in results))
        self.assertTrue(
            all(
                os.path.samefile(Path(result["EXPANDED"]).parent, fixture.root)
                for result in results
            )
        )


class TestBinarySubprocessExecution(unittest.TestCase):
    def _execution(self, **overrides):
        values = {
            "executable": r"C:\package\bin\tool.exe",
            "arguments": ["--version"],
            "working_directory": r"C:\work",
            "environment_loader": lambda: {"A": "B"},
            "stdin_bytes": None,
            "timeout_seconds": 10,
            "success_exit_codes": [0],
            "max_output_bytes": 64,
            "progress_parser": None,
            "os_name": "posix",
            "sleep": lambda _seconds: None,
        }
        values.update(overrides)
        return BinarySubprocessExecution(**values)

    def test_argv_shell_false_dual_stream_stdin_and_ring_buffers(self):
        process = FakeProcess(stdout=b"0123456789", stderr=b"error!")
        calls = []

        def popen_factory(command, **keywords):
            calls.append((command, keywords))
            return process

        execution = self._execution(
            arguments=["one", "two words", "&literal"],
            stdin_bytes=b"stdin value",
            max_output_bytes=5,
            popen_factory=popen_factory,
        )
        self.assertTrue(execution.run())
        command, keywords = calls[0]
        self.assertEqual(
            command,
            [r"C:\package\bin\tool.exe", "one", "two words", "&literal"],
        )
        self.assertFalse(keywords["shell"])
        self.assertEqual(keywords["cwd"], r"C:\work")
        self.assertEqual(keywords["env"], {"A": "B"})
        self.assertEqual(bytes(process.stdin.written), b"stdin value")
        outcome = execution.outcome_snapshot()
        self.assertEqual(outcome["stdout"], "56789")
        self.assertEqual(outcome["stderr"], "rror!")
        self.assertTrue(outcome["stdout_truncated"])
        self.assertTrue(outcome["stderr_truncated"])
        self.assertEqual(outcome["stdout_total_bytes"], 10)
        self.assertEqual(outcome["stderr_total_bytes"], 6)

    def test_progress_parsers_are_safe_across_both_reader_threads(self):
        dotted_values = []
        dotted = self._execution(
            popen_factory=lambda *_args, **_kwargs: FakeProcess(
                stdout=b"0...10...50...",
                stderr=b"75...100... done",
            ),
            progress_parser={"type": "gdal_dotted"},
            progress_callback=dotted_values.append,
        )
        self.assertTrue(dotted.run())
        self.assertIn(100.0, dotted_values)
        self.assertTrue(all(0 <= value <= 100 for value in dotted_values))

        percent_values = []
        percent = self._execution(
            popen_factory=lambda *_args, **_kwargs: FakeProcess(
                stdout=b"Untwine progress: 42.5%",
                stderr=b"Untwine progress: 107%",
            ),
            progress_parser={
                "type": "percent_regex",
                "pattern": r"(?P<percent>\d+(?:\.\d+)?)%",
            },
            progress_callback=percent_values.append,
        )
        self.assertTrue(percent.run())
        self.assertIn(42.5, percent_values)
        self.assertIn(100.0, percent_values)

    def test_nonzero_start_and_environment_failures(self):
        nonzero = self._execution(
            popen_factory=lambda *_args, **_kwargs: FakeProcess(exit_code=7)
        )
        self.assertFalse(nonzero.run())
        self.assertEqual(nonzero.outcome_snapshot()["exit_code"], 7)

        def fail_start(*_args, **_kwargs):
            raise OSError("could not start")

        start = self._execution(popen_factory=fail_start)
        self.assertFalse(start.run())
        self.assertEqual(start.outcome_snapshot()["start_error"], "could not start")

        def fail_environment():
            raise RuntimeError("could not prepare environment")

        environment = self._execution(environment_loader=fail_environment)
        self.assertFalse(environment.run())
        self.assertEqual(
            environment.outcome_snapshot()["environment_error"],
            "could not prepare environment",
        )

    def test_timeout_terminates_process(self):
        process = FakeProcess(running=True)
        execution = self._execution(
            popen_factory=lambda *_args, **_kwargs: process,
            timeout_seconds=0.5,
            monotonic=StepClock([0, 1, 2]),
        )
        self.assertFalse(execution.run())
        outcome = execution.outcome_snapshot()
        self.assertTrue(outcome["timed_out"])
        self.assertTrue(process.terminated)
        self.assertNotEqual(outcome["exit_code"], 0)

    def test_windows_cancel_uses_direct_taskkill_without_cmd(self):
        process = FakeProcess(running=True, graceful_signal=False)
        taskkill_calls = []
        execution = None

        def run_factory(command, **keywords):
            taskkill_calls.append((command, keywords))
            process.running = False
            process.exit_code = -9
            return SimpleNamespace(returncode=0)

        def request_cancel(_seconds):
            execution.cancel()

        execution = self._execution(
            popen_factory=lambda *_args, **_kwargs: process,
            run_factory=run_factory,
            os_name="nt",
            sleep=request_cancel,
        )
        self.assertFalse(execution.run())
        self.assertEqual(len(taskkill_calls), 1)
        command, keywords = taskkill_calls[0]
        self.assertTrue(Path(command[0]).is_absolute())
        self.assertEqual(Path(command[0]).name.casefold(), "taskkill.exe")
        self.assertEqual(Path(command[0]).parent.name.casefold(), "system32")
        self.assertEqual(command[1:], ["/PID", str(process.pid), "/T", "/F"])
        self.assertFalse(keywords["shell"])
        self.assertNotIn("cmd", command[0].casefold())
        self.assertTrue(execution.outcome_snapshot()["cancelled"])
        self.assertTrue(execution.outcome_snapshot()["cancel_requested"])

        injected_process = FakeProcess(running=True, graceful_signal=False)
        injected_calls = []
        injected = None

        def injected_run(command, **keywords):
            injected_calls.append((command, keywords))
            injected_process.running = False
            injected_process.exit_code = -9
            return SimpleNamespace(returncode=0)

        def request_injected_cancel(_seconds):
            injected.cancel()

        injected = self._execution(
            popen_factory=lambda *_args, **_kwargs: injected_process,
            run_factory=injected_run,
            os_name="nt",
            taskkill_executable=r"C:\trusted\taskkill.exe",
            sleep=request_injected_cancel,
        )
        self.assertFalse(injected.run())
        self.assertEqual(injected_calls[0][0][0], r"C:\trusted\taskkill.exe")

    def test_cancel_before_environment_or_popen_short_circuits_execution(self):
        calls = {"environment": 0, "popen": 0}

        def environment_loader():
            calls["environment"] += 1
            return {"A": "B"}

        def popen_factory(*_args, **_keywords):
            calls["popen"] += 1
            return FakeProcess()

        execution = self._execution(
            environment_loader=environment_loader,
            popen_factory=popen_factory,
        )
        execution.cancel()
        self.assertFalse(execution.run())
        self.assertEqual(calls, {"environment": 0, "popen": 0})
        outcome = execution.outcome_snapshot()
        self.assertTrue(outcome["cancel_requested"])
        self.assertTrue(outcome["cancelled"])
        self.assertIsNone(outcome["exit_code"])

    def test_cancel_during_environment_setup_never_calls_popen(self):
        calls = {"environment": 0, "popen": 0}
        execution = None

        def environment_loader():
            calls["environment"] += 1
            execution.cancel()
            return {"A": "B"}

        def popen_factory(*_args, **_keywords):
            calls["popen"] += 1
            return FakeProcess()

        execution = self._execution(
            environment_loader=environment_loader,
            popen_factory=popen_factory,
        )
        self.assertFalse(execution.run())
        self.assertEqual(calls, {"environment": 1, "popen": 0})
        outcome = execution.outcome_snapshot()
        self.assertTrue(outcome["cancel_requested"])
        self.assertTrue(outcome["cancelled"])
        self.assertIsNone(outcome["exit_code"])

    def test_failed_process_tree_termination_is_not_reported_cancelled(self):
        class UnkillableProcess(FakeProcess):
            def kill(self):
                self.killed = True
                raise OSError("kill rejected")

        process = UnkillableProcess(running=True, graceful_signal=False)
        execution = None

        def fail_taskkill(_command, **_keywords):
            raise OSError("taskkill rejected")

        def request_cancel(_seconds):
            execution.cancel()

        execution = self._execution(
            popen_factory=lambda *_args, **_kwargs: process,
            run_factory=fail_taskkill,
            os_name="nt",
            force_kill_seconds=0.1,
            sleep=request_cancel,
        )
        self.assertFalse(execution.run())
        outcome = execution.outcome_snapshot()
        self.assertTrue(outcome["cancel_requested"])
        self.assertFalse(outcome["cancelled"])
        self.assertIsNone(outcome["exit_code"])
        self.assertIn("taskkill rejected", outcome["termination_error"])
        self.assertIn("kill rejected", outcome["termination_error"])


class TestQGISBinaryBridgeLifetime(unittest.TestCase):
    def test_bridge_reuses_manager_and_rejects_creation_after_stop(self):
        instances = []

        class FakeManager:
            def __init__(self, iface, dependencies=None):
                self.iface = iface
                self.dependencies = dict(dependencies or {})
                self.shutdown_calls = []
                instances.append(self)

            def shutdown(self, timeout_seconds):
                self.shutdown_calls.append(timeout_seconds)

        with mock.patch(
            "qcopilots_common.bridge._make_main_thread_dispatcher",
            return_value=None,
        ), mock.patch(
            "qcopilots_common.qgis_binary_jobs.QGISBinaryJobManager",
            FakeManager,
        ):
            policy = FilesystemPolicy()
            controller = QgisBridgeController(
                iface=object(), port=0, filesystem_policy=policy
            )
            first = controller._get_qgis_binary_job_manager()
            self.assertIs(controller._get_qgis_binary_job_manager(), first)
            self.assertIs(first.dependencies["filesystem_policy"], policy)
            self.assertEqual(len(instances), 1)
            controller.stop(timeout_seconds=0.25)
            self.assertEqual(first.shutdown_calls, [0.25])
            with self.assertRaisesRegex(RuntimeError, "bridge is stopping"):
                controller._get_qgis_binary_job_manager()

            restarted = QgisBridgeController(iface=object(), port=0)
            second = restarted._get_qgis_binary_job_manager()
            self.assertIsNot(second, first)
            self.assertEqual(len(instances), 2)
            restarted.stop(timeout_seconds=0)

    def test_direct_bridge_tools_passes_binary_filesystem_policy(self):
        instances = []

        class FakeManager:
            def __init__(self, iface, dependencies=None):
                self.iface = iface
                self.dependencies = dict(dependencies or {})
                instances.append(self)

        policy = FilesystemPolicy()
        with mock.patch(
            "qcopilots_common.qgis_binary_jobs.QGISBinaryJobManager",
            FakeManager,
        ):
            tools = QgisBridgeTools(iface=object(), filesystem_policy=policy)
            manager = tools._qgis_binary_jobs()
        self.assertEqual(len(instances), 1)
        self.assertIs(manager.dependencies["filesystem_policy"], policy)

    def test_bridge_stop_shares_one_timeout_budget(self):
        class FakeManager:
            def __init__(self):
                self.timeouts = []

            def shutdown(self, timeout_seconds):
                self.timeouts.append(timeout_seconds)

        with mock.patch(
            "qcopilots_common.bridge._make_main_thread_dispatcher",
            return_value=None,
        ):
            controller = QgisBridgeController(iface=object(), port=0)
        processing = FakeManager()
        binary = FakeManager()
        controller._processing_job_manager = processing
        controller._qgis_binary_job_manager = binary
        with mock.patch(
            "qcopilots_common.bridge.time.monotonic",
            side_effect=[100.0, 100.2, 100.6],
        ):
            controller.stop(timeout_seconds=1.0)
        self.assertAlmostEqual(processing.timeouts[0], 0.8)
        self.assertAlmostEqual(binary.timeouts[0], 0.4)


if __name__ == "__main__":
    unittest.main()
