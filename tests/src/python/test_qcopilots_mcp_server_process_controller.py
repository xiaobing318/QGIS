"""QGIS unit tests for QCopilots MCP process control.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import os
import sys
import tempfile
import types
import unittest
from pathlib import Path


class TestQCopilotsMcpServerProcessController(unittest.TestCase):
    def test_process_command_builds_argv_and_records_environment(self):
        from qcopilots_common.process_controller import ProcessCommand

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            command = ProcessCommand(
                executable=sys.executable,
                arguments=["-c", "print('ready')"],
                cwd=root,
                env={"QCOPILOTS_TEST": "1"},
                log_file=root / "service.log",
            )

            self.assertEqual(
                command.to_argv(),
                [sys.executable, "-c", "print('ready')"],
            )
            self.assertEqual(command.cwd, root)
            self.assertEqual(command.log_file, root / "service.log")
            environment = command.resolved_environment()
            self.assertEqual(environment["QCOPILOTS_TEST"], "1")

    def test_process_controller_merges_configured_cors_origins(self):
        from qcopilots_common.constants import CORS_ORIGINS_ENV
        from qcopilots_common.process_controller import _merged_cors_origins

        old_origins = os.environ.get(CORS_ORIGINS_ENV)
        try:
            os.environ[CORS_ORIGINS_ENV] = os.pathsep.join(
                [
                    "https://qcopilots-example.tailnet.ts.net",
                    "http://127.0.0.1:8282",
                ]
            )
            self.assertEqual(
                _merged_cors_origins(["http://127.0.0.1:8282", "http://localhost:8282"]),
                [
                    "http://127.0.0.1:8282",
                    "http://localhost:8282",
                    "https://qcopilots-example.tailnet.ts.net",
                ],
            )
        finally:
            if old_origins is None:
                os.environ.pop(CORS_ORIGINS_ENV, None)
            else:
                os.environ[CORS_ORIGINS_ENV] = old_origins

    def test_start_passes_lan_network_settings_to_service_environment(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.constants import CORS_ORIGINS_ENV
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            def __init__(self, pid):
                self.pid = pid
                self.returncode = None

            def poll(self):
                return self.returncode

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                return [str(python_executable), str(manifest.entry_path)]

        original_popen = process_controller.subprocess.Popen
        original_find_available_port = process_controller.find_available_port
        original_process_identity = process_controller.process_identity
        original_process_matches_state = process_controller.process_matches_state
        original_is_process_running = process_controller.is_process_running
        original_process_tree_pids = process_controller.process_tree_pids
        original_health_ok = process_controller.ProcessController._health_ok
        original_service_log_file = process_controller.service_log_file
        captured = {}

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                manifest = ServiceManifest(
                    service_id="qcopilots.dummy_service",
                    display_name="QCopilots Dummy Service",
                    description="Dummy service for ProcessController tests.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(
                        host="0.0.0.0",
                        port=49531,
                        path="/mcp",
                        advertised_host="192.168.1.50",
                    ),
                    cors_origins=["*"],
                )
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                )

                def fake_popen(command, *args, **kwargs):
                    del args
                    captured["command"] = command
                    captured["cwd"] = kwargs["cwd"]
                    captured["env"] = kwargs["env"]
                    return FakeProcess(501)

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.process_matches_state = lambda pid, state: pid == 501
                process_controller.is_process_running = lambda pid: pid == 501
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.ProcessController._health_ok = lambda self, host, port: True
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / f"{service_id}.log"
                )

                status = controller.start(manifest)

                self.assertTrue(status.running)
                self.assertEqual(captured["env"]["QCOPILOTS_SERVICE_HOST"], "0.0.0.0")
                self.assertEqual(captured["env"]["QCOPILOTS_SERVICE_PORT"], "49531")
                self.assertEqual(captured["env"]["QCOPILOTS_MCP_PATH"], "/mcp")
                self.assertEqual(captured["env"][CORS_ORIGINS_ENV], "*")
                state_path = root / "state" / "qcopilots_dummy_service.json"
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertEqual(state["manifest"]["bind_host"], "0.0.0.0")
                self.assertEqual(state["manifest"]["advertised_host"], "192.168.1.50")
                self.assertEqual(state["manifest"]["cors_origins"], ["*"])
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.find_available_port = original_find_available_port
            process_controller.process_identity = original_process_identity
            process_controller.process_matches_state = original_process_matches_state
            process_controller.is_process_running = original_is_process_running
            process_controller.process_tree_pids = original_process_tree_pids
            process_controller.ProcessController._health_ok = original_health_ok
            process_controller.service_log_file = original_service_log_file

    def test_status_uses_advertised_url_and_wildcard_health_check_host(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeResponse:
            status = 200

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                del exc_type, exc_value, traceback

        original_is_process_running = process_controller.is_process_running
        original_urlopen = process_controller.urlopen
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.dummy_service",
                    display_name="QCopilots Dummy Service",
                    description="Dummy service for ProcessController tests.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(
                        host="0.0.0.0",
                        port=49531,
                        path="/mcp",
                        advertised_host="qgis-client-01.local",
                    ),
                )
                controller = ProcessController(root / "state")
                captured_urls = []

                def fake_urlopen(url, timeout):
                    captured_urls.append((url, timeout))
                    return FakeResponse()

                process_controller.urlopen = fake_urlopen
                self.assertTrue(controller._health_ok("0.0.0.0", 49531))
                self.assertEqual(
                    captured_urls,
                    [("http://127.0.0.1:49531/health", 1.5)],
                )
                captured_urls.clear()

                state_path = controller._state_path(manifest)
                state_path.parent.mkdir(parents=True, exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 123,
                            "port": 49532,
                            "owner_token": controller.owner_token,
                        }
                    ),
                    encoding="utf-8",
                )
                process_controller.is_process_running = lambda pid: pid == 123

                status = controller.status(manifest)

                self.assertTrue(status.running)
                self.assertEqual(status.health, "ok")
                self.assertEqual(status.url, "http://qgis-client-01.local:49532/mcp")
                self.assertEqual(
                    captured_urls,
                    [("http://127.0.0.1:49532/health", 1.5)],
                )
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.urlopen = original_urlopen

    def test_start_restarts_owned_service_when_network_configuration_changes(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.constants import DEFAULT_CORS_ORIGINS
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport, manifest_to_dict
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            def __init__(self, pid):
                self.pid = pid
                self.returncode = None

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                self.returncode = 0
                return 0

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                return [str(python_executable), str(manifest.entry_path)]

        original_popen = process_controller.subprocess.Popen
        original_find_available_port = process_controller.find_available_port
        original_process_identity = process_controller.process_identity
        original_process_matches_state = process_controller.process_matches_state
        original_is_process_running = process_controller.is_process_running
        original_terminate_process_tree = process_controller.terminate_process_tree
        original_health_ok = process_controller.ProcessController._health_ok
        original_service_log_file = process_controller.service_log_file
        live_pids = {101}
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                old_manifest = ServiceManifest(
                    service_id="qcopilots.dummy_service",
                    display_name="QCopilots Dummy Service",
                    description="Dummy service for ProcessController tests.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(
                        host="0.0.0.0",
                        port=49531,
                        path="/mcp",
                        advertised_host="qgis-client-01.local",
                    ),
                    cors_origins=["http://llama-server:8282"],
                )
                new_manifest = ServiceManifest(
                    service_id="qcopilots.dummy_service",
                    display_name="QCopilots Dummy Service",
                    description="Dummy service for ProcessController tests.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                controller = ProcessController(root / "state", runtime=FakeRuntime(), qgis_executable=qgis_exe)
                state_path = root / "state" / "qcopilots_dummy_service.json"
                state_path.parent.mkdir(exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 101,
                            "port": 49531,
                            "owner_token": controller.owner_token,
                            "manifest": manifest_to_dict(old_manifest, 49531),
                            "process_identity": {
                                "pid": "101",
                                "creation_date": "old",
                            },
                        }
                    ),
                    encoding="utf-8",
                )

                def fake_popen(command, *args, **kwargs):
                    del command, args, kwargs
                    live_pids.add(102)
                    return FakeProcess(102)

                def fake_terminate(pid, force=False):
                    terminated.append((pid, force))
                    live_pids.discard(pid)

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.process_matches_state = lambda pid, state: pid in live_pids
                process_controller.is_process_running = lambda pid: pid in live_pids
                process_controller.terminate_process_tree = fake_terminate
                process_controller.ProcessController._health_ok = lambda self, host, port: True
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / f"{service_id}.log"
                )

                status = controller.start(new_manifest)

                self.assertTrue(status.running)
                self.assertEqual(status.pid, 102)
                self.assertEqual(status.health, "ok")
                self.assertEqual(terminated, [(101, False)])
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertEqual(state["pid"], 102)
                self.assertEqual(state["manifest"]["bind_host"], "127.0.0.1")
                self.assertEqual(state["manifest"]["advertised_host"], "")
                self.assertEqual(state["manifest"]["cors_origins"], list(DEFAULT_CORS_ORIGINS))
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.find_available_port = original_find_available_port
            process_controller.process_identity = original_process_identity
            process_controller.process_matches_state = original_process_matches_state
            process_controller.is_process_running = original_is_process_running
            process_controller.terminate_process_tree = original_terminate_process_tree
            process_controller.ProcessController._health_ok = original_health_ok
            process_controller.service_log_file = original_service_log_file

    def test_start_reports_configuration_mismatch_for_unowned_running_service(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport, manifest_to_dict
        from qcopilots_common.process_controller import ProcessController

        old_manifest = ServiceManifest(
            service_id="qcopilots.dummy_service",
            display_name="QCopilots Dummy Service",
            description="Dummy service for ProcessController tests.",
            plugin_name="qcopilots_dummy_service",
            plugin_dir=Path("qcopilots_dummy_service"),
            manifest_path=Path("qcopilots_dummy_service/qcopilots_service.json"),
            transport=ServiceTransport(
                host="0.0.0.0",
                port=49531,
                path="/mcp",
                advertised_host="qgis-client-01.local",
            ),
            cors_origins=["http://llama-server:8282"],
        )
        new_manifest = ServiceManifest(
            service_id="qcopilots.dummy_service",
            display_name="QCopilots Dummy Service",
            description="Dummy service for ProcessController tests.",
            plugin_name="qcopilots_dummy_service",
            plugin_dir=Path("qcopilots_dummy_service"),
            manifest_path=Path("qcopilots_dummy_service/qcopilots_service.json"),
            transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
        )
        original_popen = process_controller.subprocess.Popen
        original_process_matches_state = process_controller.process_matches_state
        original_is_process_running = process_controller.is_process_running
        original_terminate_process_tree = process_controller.terminate_process_tree
        original_health_ok = process_controller.ProcessController._health_ok
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                controller = ProcessController(root / "state")
                state_path = root / "state" / "qcopilots_dummy_service.json"
                state_path.parent.mkdir(exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 123,
                            "port": 49531,
                            "owner_token": "other-manager",
                            "manifest": manifest_to_dict(old_manifest, 49531),
                            "process_identity": {
                                "pid": "123",
                                "creation_date": "old",
                            },
                        }
                    ),
                    encoding="utf-8",
                )

                def fail_popen(*args, **kwargs):
                    del args, kwargs
                    raise AssertionError("unowned configuration mismatch must not start a new process")

                process_controller.subprocess.Popen = fail_popen
                process_controller.process_matches_state = lambda pid, state: pid == 123
                process_controller.is_process_running = lambda pid: pid == 123
                process_controller.terminate_process_tree = lambda pid, force=False: terminated.append(
                    (pid, force)
                )
                process_controller.ProcessController._health_ok = lambda self, host, port: True

                status = controller.start(new_manifest)

                self.assertTrue(status.running)
                self.assertFalse(status.owner_match)
                self.assertEqual(status.health, "configuration-mismatch")
                self.assertEqual(status.url, "http://qgis-client-01.local:49531/mcp")
                self.assertEqual(terminated, [])
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertEqual(state["stop_refused"], "configuration_mismatch")
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.process_matches_state = original_process_matches_state
            process_controller.is_process_running = original_is_process_running
            process_controller.terminate_process_tree = original_terminate_process_tree
            process_controller.ProcessController._health_ok = original_health_ok

    def test_qgis_executable_resolves_to_packaged_python(self):
        from qcopilots_common.process_controller import resolve_service_python_executable

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "QGIS40200-RelWithDebInfo"
            qgis_exe, python_exe = _write_qgis_package_layout(root)

            self.assertEqual(resolve_service_python_executable(qgis_exe), python_exe)

    def test_python_resolution_ignores_external_python_and_osgeo4w_root(self):
        from qcopilots_common.process_controller import resolve_service_python_executable

        old_osgeo4w_root = os.environ.get("OSGEO4W_ROOT")
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                external_python = root / "python.exe"
                external_python.write_text("", encoding="utf-8")
                external_osgeo = root / "external-osgeo"
                _write_qgis_package_layout(external_osgeo)
                os.environ["OSGEO4W_ROOT"] = str(external_osgeo)

                with self.assertRaises(RuntimeError):
                    resolve_service_python_executable(external_python)
        finally:
            if old_osgeo4w_root is None:
                os.environ.pop("OSGEO4W_ROOT", None)
            else:
                os.environ["OSGEO4W_ROOT"] = old_osgeo4w_root

    def test_python_resolution_requires_package_context(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.process_controller import resolve_service_python_executable

        old_package_root = os.environ.get(process_controller.QGIS_PACKAGE_ROOT_ENV)
        try:
            os.environ.pop(process_controller.QGIS_PACKAGE_ROOT_ENV, None)

            with self.assertRaises(RuntimeError):
                resolve_service_python_executable()
        finally:
            if old_package_root is None:
                os.environ.pop(process_controller.QGIS_PACKAGE_ROOT_ENV, None)
            else:
                os.environ[process_controller.QGIS_PACKAGE_ROOT_ENV] = old_package_root

    def test_python_resolution_accepts_package_root_environment(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.process_controller import resolve_service_python_executable

        old_package_root = os.environ.get(process_controller.QGIS_PACKAGE_ROOT_ENV)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "QGIS40200-RelWithDebInfo"
                _, python_exe = _write_qgis_package_layout(root)
                os.environ[process_controller.QGIS_PACKAGE_ROOT_ENV] = str(root)

                self.assertEqual(resolve_service_python_executable(), python_exe)
                self.assertEqual(resolve_service_python_executable(Path(tmp) / "python.exe"), python_exe)
        finally:
            if old_package_root is None:
                os.environ.pop(process_controller.QGIS_PACKAGE_ROOT_ENV, None)
            else:
                os.environ[process_controller.QGIS_PACKAGE_ROOT_ENV] = old_package_root

    @unittest.skipUnless(os.name == "nt", "Windows service creation flags are platform-specific")
    def test_windows_service_creationflags_hide_service_console(self):
        import subprocess

        from qcopilots_common.process_controller import _service_creationflags

        creationflags = _service_creationflags()

        self.assertTrue(creationflags & subprocess.CREATE_NEW_PROCESS_GROUP)
        self.assertTrue(creationflags & subprocess.CREATE_NO_WINDOW)

    @unittest.skipUnless(os.name == "nt", "Windows helper subprocess flags are platform-specific")
    def test_windows_helper_subprocesses_hide_console(self):
        import subprocess

        import qcopilots_common.process_controller as process_controller

        original_is_process_running = process_controller.is_process_running
        original_run = process_controller.subprocess.run
        calls = []

        def fake_run(command, *args, **kwargs):
            calls.append((command, kwargs))
            return types.SimpleNamespace(stdout="")

        try:
            process_controller.is_process_running = lambda pid: pid == 123
            process_controller.subprocess.run = fake_run

            process_controller.process_identity(123)
            process_controller.process_tree_pids(123)
            process_controller.terminate_process_tree(123)

            self.assertGreaterEqual(len(calls), 4)
            for command, kwargs in calls:
                self.assertIn("creationflags", kwargs, command)
                self.assertTrue(kwargs["creationflags"] & subprocess.CREATE_NO_WINDOW, command)
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.subprocess.run = original_run

    @unittest.skipUnless(os.name == "nt", "Windows helper subprocess flags are platform-specific")
    def test_windows_process_command_popen_hides_console(self):
        import subprocess

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.process_controller import ProcessCommand, ProcessController

        original_popen = process_controller.subprocess.Popen
        calls = []

        class FakeProcess:
            pid = 123

            def poll(self):
                return None

        def fake_popen(command, *args, **kwargs):
            del args
            calls.append((command, kwargs))
            return FakeProcess()

        try:
            process_controller.subprocess.Popen = fake_popen
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                command = ProcessCommand(
                    executable=sys.executable,
                    arguments=["-c", "print('ready')"],
                    cwd=root,
                )
                controller = ProcessController(command, service_id="unit-test-service")

                controller.start()

            self.assertEqual(len(calls), 1)
            self.assertTrue(calls[0][1]["creationflags"] & subprocess.CREATE_NO_WINDOW)
        finally:
            process_controller.subprocess.Popen = original_popen

    @unittest.skipUnless(os.name == "nt", "Windows helper subprocess flags are platform-specific")
    def test_uv_runtime_ensure_runtime_hides_console(self):
        import subprocess

        import qcopilots_common.uv_runtime as uv_runtime
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.uv_runtime import UvRuntime

        original_run = uv_runtime.subprocess.run
        calls = []

        def fake_run(command, *args, **kwargs):
            del args
            calls.append((command, kwargs))
            return types.SimpleNamespace(returncode=0)

        try:
            uv_runtime.subprocess.run = fake_run
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                requirements_path = plugin_dir / "requirements.lock"
                requirements_path.write_text("idna==3.7\n", encoding="utf-8")
                python_exe = root / "python.exe"
                python_exe.write_text("", encoding="utf-8")
                manifest = ServiceManifest(
                    service_id="qcopilots.dummy_service",
                    display_name="QCopilots Dummy Service",
                    description="Dummy service for uv runtime tests.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                    runtime={"requirements": requirements_path.name},
                )

                UvRuntime(root / "runtime").ensure_runtime(manifest, python_executable=str(python_exe))

            self.assertEqual(len(calls), 2)
            for command, kwargs in calls:
                self.assertIn("creationflags", kwargs, command)
                self.assertTrue(kwargs["creationflags"] & subprocess.CREATE_NO_WINDOW, command)
        finally:
            uv_runtime.subprocess.run = original_run

    def test_uv_runtime_requires_python_executable(self):
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.uv_runtime import UvRuntime

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plugin_dir = root / "qcopilots_dummy_service"
            plugin_dir.mkdir()
            manifest = ServiceManifest(
                service_id="qcopilots.dummy_service",
                display_name="QCopilots Dummy Service",
                description="Dummy service for uv runtime tests.",
                plugin_name="qcopilots_dummy_service",
                plugin_dir=plugin_dir,
                manifest_path=plugin_dir / "qcopilots_service.json",
                transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
            )

            with self.assertRaises(RuntimeError):
                UvRuntime(root / "runtime").ensure_runtime(manifest)

    def test_uv_runtime_builds_python_module_run_command(self):
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.uv_runtime import UvRuntime

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plugin_dir = root / "qcopilots_dummy_service"
            plugin_dir.mkdir()
            entry_path = plugin_dir / "server.py"
            requirements_path = plugin_dir / "requirements.lock"
            python_exe = root / "apps" / "Python312" / "python.exe"
            entry_path.write_text("print('ready')", encoding="utf-8")
            requirements_path.write_text("idna==3.7\n", encoding="utf-8")
            python_exe.parent.mkdir(parents=True)
            python_exe.write_text("", encoding="utf-8")

            manifest = ServiceManifest(
                service_id="qcopilots.dummy_service",
                display_name="QCopilots Dummy Service",
                description="Dummy service for uv command tests.",
                plugin_name="qcopilots_dummy_service",
                plugin_dir=plugin_dir,
                manifest_path=plugin_dir / "qcopilots_service.json",
                transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
            )

            command = UvRuntime(root / "runtime").service_command(manifest, python_exe)

            self.assertEqual(command[:3], [str(python_exe), "-m", "uv"])
            self.assertIn("run", command)
            self.assertIn("--directory", command)
            self.assertIn(str(plugin_dir), command)
            self.assertIn("--python", command)
            self.assertIn(str(python_exe), command)
            self.assertIn("--with-requirements", command)
            self.assertIn(str(requirements_path), command)
            self.assertEqual(command[-1], str(entry_path))

    def test_process_controller_captures_exit_status_and_log_output(self):
        from qcopilots_common.process_controller import ProcessCommand, ProcessController

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            log_file = root / "service.log"
            command = ProcessCommand(
                executable=sys.executable,
                arguments=["-c", "print('ready')"],
                cwd=root,
                env={"QCOPILOTS_TEST": "1"},
                log_file=log_file,
            )
            controller = ProcessController(command, service_id="unit-test-service")

            try:
                self.assertEqual(controller.snapshot()["status"], "stopped")
                controller.start()
                running_snapshot = controller.snapshot()
                self.assertEqual(running_snapshot["service_id"], "unit-test-service")
                self.assertEqual(running_snapshot["status"], "running")
                self.assertIsInstance(running_snapshot["pid"], int)

                exit_code = controller.wait(timeout_seconds=5)
                self.assertEqual(exit_code, 0)
                exited_snapshot = controller.snapshot()
                self.assertEqual(exited_snapshot["status"], "exited")
                self.assertEqual(exited_snapshot["exit_code"], 0)
                self.assertEqual(log_file.read_text(encoding="utf-8").strip(), "ready")
            finally:
                controller.stop(timeout_seconds=1)
                self.assertIn(
                    controller.snapshot()["status"],
                    ["stopped", "exited"],
                )

    def test_process_controller_rebuilds_manifests_from_state_snapshots(self):
        import json

        from qcopilots_common.process_controller import ProcessController

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plugin_dir = root / "qcopilots_disabled_service"
            plugin_dir.mkdir()
            state_root = root / "state"
            state_root.mkdir()
            state_path = state_root / "qcopilots_disabled_service.json"
            state_path.write_text(
                json.dumps(
                    {
                        "pid": 123,
                        "port": 49541,
                        "manifest": {
                            "service_id": "qcopilots.disabled_service",
                            "display_name": "QCopilots Disabled Service",
                            "description": "Service recovered from a state snapshot.",
                            "plugin_name": "qcopilots_disabled_service",
                            "plugin_dir": str(plugin_dir),
                            "entry_point": str(plugin_dir / "server.py"),
                            "host": "0.0.0.0",
                            "bind_host": "0.0.0.0",
                            "advertised_host": "qgis-client-01.local",
                            "port": 49541,
                            "mcp_path": "/mcp",
                            "category": "qcopilots",
                            "capabilities": ["tools"],
                            "cors_origins": ["http://llama-server:8282"],
                        },
                    }
                ),
                encoding="utf-8",
            )

            manifests = ProcessController(state_root).stored_manifests()

            self.assertEqual(len(manifests), 1)
            manifest = manifests[0]
            self.assertEqual(manifest.service_id, "qcopilots.disabled_service")
            self.assertEqual(manifest.plugin_dir, plugin_dir)
            self.assertEqual(manifest.host, "0.0.0.0")
            self.assertEqual(manifest.advertised_host, "qgis-client-01.local")
            self.assertEqual(manifest.default_port, 49541)
            self.assertEqual(manifest.mcp_path, "/mcp")
            self.assertEqual(manifest.url(), "http://qgis-client-01.local:49541/mcp")
            self.assertEqual(manifest.cors_origins, ["http://llama-server:8282"])
            self.assertEqual(manifest.entry_path, plugin_dir / "server.py")

    def test_process_controller_ignores_unsafe_state_snapshot_service_ids(self):
        import json

        from qcopilots_common.process_controller import ProcessController

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            state_root = root / "state"
            state_root.mkdir()
            unsafe_ids = [
                "../outside",
                "..\\outside",
                "C:\\outside",
                "qcopilots/escape",
                "qcopilots..escape",
            ]
            for index, service_id in enumerate(unsafe_ids):
                (state_root / f"unsafe_{index}.json").write_text(
                    json.dumps(
                        {
                            "pid": 123,
                            "port": 49541,
                            "manifest": {
                                "service_id": service_id,
                                "plugin_dir": str(root / "plugin"),
                                "port": 49541,
                            },
                        }
                    ),
                    encoding="utf-8",
                )

            self.assertEqual(ProcessController(state_root).stored_manifests(), [])

    def test_process_controller_rejects_unsafe_service_id_state_paths(self):
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plugin_dir = root / "plugin"
            plugin_dir.mkdir()
            controller = ProcessController(root / "state")
            manifest = ServiceManifest(
                service_id="../outside",
                display_name="Unsafe",
                description="Unsafe service id.",
                plugin_name="plugin",
                plugin_dir=plugin_dir,
                manifest_path=plugin_dir / "qcopilots_service.json",
                transport=ServiceTransport(host="127.0.0.1", port=49541, path="/mcp"),
            )

            with self.assertRaises(ValueError):
                controller.status(manifest)
            self.assertFalse((root / "outside.json").exists())

    def test_manifest_controller_lightweight_status_skips_expensive_process_inspection(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        original_is_process_running = process_controller.is_process_running
        original_process_matches_state = process_controller.process_matches_state
        original_process_tree_pids = process_controller.process_tree_pids
        calls = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.dummy_service",
                    display_name="QCopilots Dummy Service",
                    description="Dummy service for ProcessController tests.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                controller = ProcessController(root / "state")
                state_path = root / "state" / "qcopilots_dummy_service.json"
                state_path.parent.mkdir(exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 123,
                            "port": 49531,
                            "owner_token": controller.owner_token,
                            "process_identity": {
                                "pid": "123",
                                "creation_date": "old",
                                "command_line": "python service.py",
                            },
                        }
                    ),
                    encoding="utf-8",
                )

                process_controller.is_process_running = lambda pid: pid == 123
                process_controller.process_matches_state = lambda pid, state: calls.append(
                    ("identity", pid, state)
                ) or True
                process_controller.process_tree_pids = lambda pid: calls.append(("tree", pid)) or [pid]

                status = controller.status(manifest, deep=False)

                self.assertTrue(status.running)
                self.assertEqual(status.health, "running")
                self.assertEqual(status.process_tree, [])
                self.assertEqual(calls, [])
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.process_matches_state = original_process_matches_state
            process_controller.process_tree_pids = original_process_tree_pids

    def test_manifest_controller_keeps_separate_process_handles_per_service(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            def __init__(self, pid):
                self.pid = pid
                self.returncode = None
                self.wait_calls = 0
                self.kill_calls = 0

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                self.wait_calls += 1
                self.returncode = 0
                return 0

            def kill(self):
                self.kill_calls += 1
                self.returncode = -9

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                del manifest
                return [str(python_executable), "-c", "print('ready')"]

        original_popen = process_controller.subprocess.Popen
        original_find_available_port = process_controller.find_available_port
        original_process_identity = process_controller.process_identity
        original_is_process_running = process_controller.is_process_running
        original_process_tree_pids = process_controller.process_tree_pids
        original_terminate_process_tree = process_controller.terminate_process_tree
        original_health_ok = process_controller.ProcessController._health_ok
        original_service_log_file = process_controller.service_log_file
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                manifests = [
                    ServiceManifest(
                        service_id="qcopilots.service_one",
                        display_name="QCopilots Service One",
                        description="First fake service.",
                        plugin_name="qcopilots_service_one",
                        plugin_dir=plugin_dir,
                        manifest_path=plugin_dir / "service_one.json",
                        transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                    ),
                    ServiceManifest(
                        service_id="qcopilots.service_two",
                        display_name="QCopilots Service Two",
                        description="Second fake service.",
                        plugin_name="qcopilots_service_two",
                        plugin_dir=plugin_dir,
                        manifest_path=plugin_dir / "service_two.json",
                        transport=ServiceTransport(host="127.0.0.1", port=49532, path="/mcp"),
                    ),
                ]
                fake_processes = [FakeProcess(101), FakeProcess(102)]
                process_by_pid = {process.pid: process for process in fake_processes}

                def fake_popen(command, *args, **kwargs):
                    del command, args, kwargs
                    return fake_processes.pop(0)

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.is_process_running = (
                    lambda pid: pid in process_by_pid and process_by_pid[pid].returncode is None
                )
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.terminate_process_tree = lambda pid, force=False: terminated.append(
                    (pid, force)
                )
                process_controller.ProcessController._health_ok = lambda self, host, port: True
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / f"{service_id}.log"
                )

                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                controller = ProcessController(root / "state", runtime=FakeRuntime(), qgis_executable=qgis_exe)
                for manifest in manifests:
                    status = controller.start(manifest)
                    self.assertTrue(status.running)

                self.assertEqual(
                    set(controller._manifest_processes),
                    {"qcopilots.service_one", "qcopilots.service_two"},
                )

                stopped_one = controller.stop(manifests[0], timeout_seconds=1)
                self.assertFalse(stopped_one.running)
                self.assertNotIn("qcopilots.service_one", controller._manifest_processes)
                self.assertIn("qcopilots.service_two", controller._manifest_processes)
                self.assertEqual(process_by_pid[101].wait_calls, 1)
                self.assertEqual(process_by_pid[102].wait_calls, 0)

                stopped_two = controller.stop(manifests[1], timeout_seconds=1)
                self.assertFalse(stopped_two.running)
                self.assertEqual(controller._manifest_processes, {})
                self.assertEqual(process_by_pid[102].wait_calls, 1)
                self.assertEqual(
                    terminated,
                    [(101, False), (101, True), (102, False), (102, True)],
                )
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.find_available_port = original_find_available_port
            process_controller.process_identity = original_process_identity
            process_controller.is_process_running = original_is_process_running
            process_controller.process_tree_pids = original_process_tree_pids
            process_controller.terminate_process_tree = original_terminate_process_tree
            process_controller.ProcessController._health_ok = original_health_ok
            process_controller.service_log_file = original_service_log_file

    def test_manifest_controller_clears_exited_tracked_process_state(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 123
            returncode = 7

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                return self.returncode

        original_is_process_running = process_controller.is_process_running
        original_terminate_process_tree = process_controller.terminate_process_tree
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.exited_service",
                    display_name="QCopilots Exited Service",
                    description="Exited fake service.",
                    plugin_name="qcopilots_exited_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                controller = ProcessController(root / "state")
                controller._manifest_processes[manifest.service_id] = FakeProcess()
                state_path = root / "state" / "qcopilots_exited_service.json"
                state_path.parent.mkdir(exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 123,
                            "port": 49531,
                            "owner_token": controller.owner_token,
                            "process_identity": {
                                "pid": "123",
                                "creation_date": "created-123",
                            },
                        }
                    ),
                    encoding="utf-8",
                )

                process_controller.is_process_running = lambda pid: False
                process_controller.terminate_process_tree = lambda pid, force=False: terminated.append(
                    (pid, force)
                )

                status = controller.stop(manifest, timeout_seconds=1)

                self.assertFalse(status.running)
                self.assertEqual(controller.exit_code, 7)
                self.assertEqual(controller._manifest_processes, {})
                self.assertEqual(terminated, [])
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertIsNone(state["pid"])
                self.assertNotIn("stop_refused", state)
                self.assertIn("stopped_at", state)
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.terminate_process_tree = original_terminate_process_tree

    def test_manifest_controller_clears_exited_tracked_process_after_pid_reuse(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 123
            returncode = 7

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                return self.returncode

        original_is_process_running = process_controller.is_process_running
        original_process_identity = process_controller.process_identity
        original_terminate_process_tree = process_controller.terminate_process_tree
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.reused_pid_service",
                    display_name="QCopilots Reused PID Service",
                    description="Reused PID fake service.",
                    plugin_name="qcopilots_reused_pid_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                controller = ProcessController(root / "state")
                controller._manifest_processes[manifest.service_id] = FakeProcess()
                state_path = root / "state" / "qcopilots_reused_pid_service.json"
                state_path.parent.mkdir(exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 123,
                            "port": 49531,
                            "owner_token": controller.owner_token,
                            "process_identity": {
                                "pid": "123",
                                "creation_date": "created-old",
                            },
                        }
                    ),
                    encoding="utf-8",
                )

                process_controller.is_process_running = lambda pid: pid == 123
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": "created-reused",
                }
                process_controller.terminate_process_tree = lambda pid, force=False: terminated.append(
                    (pid, force)
                )

                status = controller.stop(manifest, timeout_seconds=1)

                self.assertFalse(status.running)
                self.assertEqual(controller.exit_code, 7)
                self.assertEqual(controller._manifest_processes, {})
                self.assertEqual(terminated, [])
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertIsNone(state["pid"])
                self.assertNotIn("stop_refused", state)
                self.assertIn("stopped_at", state)
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.process_identity = original_process_identity
            process_controller.terminate_process_tree = original_terminate_process_tree

    def test_manifest_controller_keeps_running_state_for_mismatched_tracked_handle(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 202
            returncode = None
            wait_calls = 0

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                self.wait_calls += 1
                self.returncode = 0
                return 0

            def kill(self):
                self.returncode = -9

        original_is_process_running = process_controller.is_process_running
        original_process_identity = process_controller.process_identity
        original_process_tree_pids = process_controller.process_tree_pids
        original_terminate_process_tree = process_controller.terminate_process_tree
        original_health_ok = process_controller.ProcessController._health_ok
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.mismatch_service",
                    display_name="QCopilots Mismatch Service",
                    description="Mismatched fake service.",
                    plugin_name="qcopilots_mismatch_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                controller = ProcessController(root / "state")
                fake_process = FakeProcess()
                controller._manifest_processes[manifest.service_id] = fake_process
                state_path = root / "state" / "qcopilots_mismatch_service.json"
                state_path.parent.mkdir(exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 201,
                            "port": 49531,
                            "owner_token": controller.owner_token,
                            "process_identity": {
                                "pid": "201",
                                "creation_date": "created-201",
                            },
                        }
                    ),
                    encoding="utf-8",
                )

                process_controller.is_process_running = (
                    lambda pid: pid == 201 or (pid == 202 and fake_process.returncode is None)
                )
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.terminate_process_tree = lambda pid, force=False: terminated.append(
                    (pid, force)
                )
                process_controller.ProcessController._health_ok = lambda self, host, port: False

                status = controller.stop(manifest, timeout_seconds=1)

                self.assertTrue(status.running)
                self.assertEqual(status.pid, 201)
                self.assertEqual(fake_process.wait_calls, 1)
                self.assertEqual(controller._manifest_processes, {})
                self.assertEqual(terminated, [(202, False), (202, True)])
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertEqual(state["pid"], 201)
                self.assertNotIn("stop_refused", state)
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.process_identity = original_process_identity
            process_controller.process_tree_pids = original_process_tree_pids
            process_controller.terminate_process_tree = original_terminate_process_tree
            process_controller.ProcessController._health_ok = original_health_ok

    def test_service_id_validation_allows_qcopilots_ids_without_alternate_separator(self):
        import qcopilots_common.service_id as service_id

        original_altsep = service_id.os.altsep
        try:
            service_id.os.altsep = None
            self.assertTrue(service_id.is_safe_service_id("qcopilots.mcp_server_builtin_tools"))
            self.assertTrue(service_id.is_safe_service_id("qcopilots.mcp-server-skills"))
        finally:
            service_id.os.altsep = original_altsep

    def test_manifest_controller_tracks_owned_process_tree(self):
        import json

        from qcopilots_common.constants import QCOPILOTS_HOME_ENV
        from qcopilots_common.manifest import load_service_manifest
        from qcopilots_common.process_controller import (
            ProcessController,
            is_process_running,
            process_identity,
        )

        class FakeRuntime:
            def ensure_runtime(self, manifest, python_executable):
                del manifest, python_executable
                return sys.executable

        old_home = os.environ.get(QCOPILOTS_HOME_ENV)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                os.environ[QCOPILOTS_HOME_ENV] = str(root / "home")
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                (plugin_dir / "child.py").write_text(
                    "\n".join(
                        [
                            "import time",
                            "",
                            "time.sleep(120)",
                        ]
                    ),
                    encoding="utf-8",
                )
                (plugin_dir / "server.py").write_text(
                    "\n".join(
                        [
                            "import os",
                            "import subprocess",
                            "import sys",
                            "from http import HTTPStatus",
                            "from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer",
                            "from pathlib import Path",
                            "",
                            "class Handler(BaseHTTPRequestHandler):",
                            "    def log_message(self, fmt, *args):",
                            "        return",
                            "    def do_GET(self):",
                            "        if self.path.rstrip('/') == '/health':",
                            "            self.send_response(HTTPStatus.OK)",
                            "            self.end_headers()",
                            "            self.wfile.write(b'ok')",
                            "            return",
                            "        self.send_response(HTTPStatus.NOT_FOUND)",
                            "        self.end_headers()",
                            "",
                            "child = subprocess.Popen([sys.executable, str(Path(__file__).with_name('child.py'))])",
                            "pid_file = os.environ.get('QCOPILOTS_CHILD_PID_FILE')",
                            "if pid_file:",
                            "    Path(pid_file).write_text(str(child.pid), encoding='utf-8')",
                            "host = os.environ['QCOPILOTS_SERVICE_HOST']",
                            "port = int(os.environ['QCOPILOTS_SERVICE_PORT'])",
                            "server = ThreadingHTTPServer((host, port), Handler)",
                            "server.serve_forever()",
                        ]
                    ),
                    encoding="utf-8",
                )
                manifest_path = plugin_dir / "qcopilots_service.json"
                manifest_path.write_text(
                    json.dumps(
                        {
                            "service_id": "qcopilots.dummy_service",
                            "display_name": "QCopilots Dummy Service",
                            "description": "Dummy service for ProcessController tests.",
                            "plugin_name": "qcopilots_dummy_service",
                            "entry_point": "server.py",
                            "transport": {
                                "type": "http",
                                "host": "127.0.0.1",
                                "advertised_host": "qgis-client-01.local",
                                "port": 49531,
                                "path": "/mcp",
                            },
                            "cors_origins": ["http://llama-server:8282"],
                            "runtime": {"requirements": "requirements.lock"},
                        }
                    ),
                    encoding="utf-8",
                )

                manifest = load_service_manifest(manifest_path)
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                controller = ProcessController(root / "state", runtime=FakeRuntime(), qgis_executable=qgis_exe)
                child_pid_file = root / "child.pid"

                try:
                    status = controller.start(
                        manifest,
                        bridge_url="http://127.0.0.1:48200",
                        extra_env={
                            "QCOPILOTS_CHILD_PID_FILE": str(child_pid_file),
                        },
                    )
                    self.assertTrue(status.running)
                    self.assertEqual(status.health, "ok")
                    self.assertTrue(status.owner_match)
                    self.assertIsInstance(status.pid, int)
                    parent_pid = status.pid
                    child_pid = _wait_for_pid(child_pid_file)
                    self.assertTrue(is_process_running(child_pid))
                    status = controller.status(manifest)
                    self.assertIn(status.pid, status.process_tree)
                    if os.name == "nt" and len(status.process_tree) > 1:
                        self.assertIn(child_pid, status.process_tree)
                    self.assertEqual(status.runtime_python, sys.executable)

                    state_path = root / "state" / "qcopilots_dummy_service.json"
                    state = json.loads(state_path.read_text(encoding="utf-8"))
                    self.assertEqual(state["runtime_python"], sys.executable)
                    self.assertEqual(state["manifest"]["url"], status.url)
                    self.assertEqual(state["manifest"]["advertised_host"], "qgis-client-01.local")
                    self.assertEqual(state["manifest"]["cors_origins"], ["http://llama-server:8282"])
                    self.assertIn("owner_token", state)
                    self.assertIn("manager_pid", state)
                    self.assertIn("process_identity", state)

                    unowned_controller = ProcessController(
                        root / "state",
                        runtime=FakeRuntime(),
                        qgis_executable=qgis_exe,
                    )
                    refused = unowned_controller.stop(manifest, timeout_seconds=1)
                    self.assertFalse(refused.owner_match)
                    self.assertTrue(is_process_running(status.pid))

                    state = json.loads(state_path.read_text(encoding="utf-8"))
                    self.assertIn(
                        state["stop_refused"],
                        [
                            "owner_mismatch",
                            "process_identity_mismatch",
                            "missing_process_identity",
                        ],
                    )
                    state["manager_pid"] = 999999
                    state["process_identity"] = process_identity(status.pid)
                    self.assertTrue(state["process_identity"])
                    state.pop("stop_refused", None)
                    state_path.write_text(json.dumps(state), encoding="utf-8")
                    recovered = unowned_controller.stop(manifest, timeout_seconds=5)
                    self.assertFalse(recovered.running)
                    self.assertFalse(is_process_running(parent_pid))
                    self.assertFalse(is_process_running(child_pid))
                    state = json.loads(state_path.read_text(encoding="utf-8"))
                    self.assertIn("stale_owner_recovered_at", state)
                finally:
                    stopped = controller.stop(manifest, timeout_seconds=5)
                    self.assertFalse(stopped.running)
        finally:
            if old_home is None:
                os.environ.pop(QCOPILOTS_HOME_ENV, None)
            else:
                os.environ[QCOPILOTS_HOME_ENV] = old_home

    def test_process_identity_mismatch_is_not_treated_as_owned_process(self):
        import qcopilots_common.process_controller as process_controller

        original_process_identity = process_controller.process_identity
        try:
            process_controller.process_identity = lambda pid: {
                "pid": str(pid),
                "creation_date": "new",
                "command_line": "python service.py",
            }
            self.assertFalse(
                process_controller.process_matches_state(
                    123,
                    {
                        "process_identity": {
                            "pid": "123",
                            "creation_date": "old",
                            "command_line": "python service.py",
                        }
                    },
                )
            )
            self.assertTrue(
                process_controller.process_matches_state(
                    123,
                    {
                        "process_identity": {
                            "pid": "123",
                            "creation_date": "new",
                            "command_line": "python service.py",
                        }
                    },
                )
            )
            self.assertFalse(process_controller.process_matches_state(123, {}))
        finally:
            process_controller.process_identity = original_process_identity

    def test_owned_controller_refuses_to_stop_reused_pid(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import load_service_manifest
        from qcopilots_common.process_controller import ProcessController

        original_is_process_running = process_controller.is_process_running
        original_process_identity = process_controller.process_identity
        original_terminate_process_tree = process_controller.terminate_process_tree
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                manifest_path = plugin_dir / "qcopilots_service.json"
                manifest_path.write_text(
                    json.dumps(
                        {
                            "service_id": "qcopilots.dummy_service",
                            "display_name": "QCopilots Dummy Service",
                            "description": "Dummy service for ProcessController tests.",
                            "entry_point": "server.py",
                            "transport": {
                                "type": "http",
                                "host": "127.0.0.1",
                                "port": 49531,
                                "path": "/mcp",
                            },
                        }
                    ),
                    encoding="utf-8",
                )
                manifest = load_service_manifest(manifest_path)
                controller = ProcessController(root / "state")
                state_path = root / "state" / "qcopilots_dummy_service.json"
                state_path.parent.mkdir(exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 123,
                            "port": 49531,
                            "owner_token": controller.owner_token,
                            "process_identity": {
                                "pid": "123",
                                "creation_date": "old",
                                "command_line": "python old-service.py",
                            },
                        }
                    ),
                    encoding="utf-8",
                )

                process_controller.is_process_running = lambda pid: True
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": "new",
                    "command_line": "python unrelated-process.py",
                }
                process_controller.terminate_process_tree = lambda pid, force=False: terminated.append(
                    (pid, force)
                )

                controller.stop(manifest, timeout_seconds=1)

                self.assertEqual(terminated, [])
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertEqual(state["stop_refused"], "process_identity_mismatch")
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.process_identity = original_process_identity
            process_controller.terminate_process_tree = original_terminate_process_tree

    def test_owned_controller_refuses_missing_identity_without_live_handle(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import load_service_manifest
        from qcopilots_common.process_controller import ProcessController

        original_is_process_running = process_controller.is_process_running
        original_terminate_process_tree = process_controller.terminate_process_tree
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                manifest_path = plugin_dir / "qcopilots_service.json"
                manifest_path.write_text(
                    json.dumps(
                        {
                            "service_id": "qcopilots.dummy_service",
                            "display_name": "QCopilots Dummy Service",
                            "description": "Dummy service for ProcessController tests.",
                            "entry_point": "server.py",
                            "transport": {
                                "type": "http",
                                "host": "127.0.0.1",
                                "port": 49531,
                                "path": "/mcp",
                            },
                        }
                    ),
                    encoding="utf-8",
                )
                manifest = load_service_manifest(manifest_path)
                controller = ProcessController(root / "state")
                state_path = root / "state" / "qcopilots_dummy_service.json"
                state_path.parent.mkdir(exist_ok=True)
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": 123,
                            "port": 49531,
                            "owner_token": controller.owner_token,
                        }
                    ),
                    encoding="utf-8",
                )

                process_controller.is_process_running = lambda pid: True
                process_controller.terminate_process_tree = lambda pid, force=False: terminated.append(
                    (pid, force)
                )

                controller.stop(manifest, timeout_seconds=1)

                self.assertEqual(terminated, [])
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertEqual(state["stop_refused"], "missing_process_identity")
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.terminate_process_tree = original_terminate_process_tree

    def test_controller_stops_live_handle_when_state_is_missing(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 123
            returncode = None

            def poll(self):
                return None

            def wait(self, timeout=None):
                del timeout
                self.returncode = 0
                return 0

            def kill(self):
                self.returncode = -9

        original_is_process_running = process_controller.is_process_running
        original_terminate_process_tree = process_controller.terminate_process_tree
        terminated = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.dummy_service",
                    display_name="QCopilots Dummy Service",
                    description="Dummy service for ProcessController tests.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                controller = ProcessController(root / "state")
                controller.process = FakeProcess()

                process_controller.is_process_running = lambda pid: pid == 123
                process_controller.terminate_process_tree = lambda pid, force=False: terminated.append(
                    (pid, force)
                )

                status = controller.stop(manifest, timeout_seconds=1)

                self.assertFalse(status.running)
                self.assertEqual(terminated, [(123, False), (123, True)])
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.terminate_process_tree = original_terminate_process_tree


def _wait_for_pid(path: Path, timeout_seconds: float = 5) -> int:
    import time

    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if path.exists():
            value = path.read_text(encoding="utf-8").strip()
            if value.isdigit():
                return int(value)
        time.sleep(0.05)
    raise AssertionError(f"Timed out waiting for child pid file: {path}")


def _write_qgis_package_layout(root: Path) -> tuple[Path, Path]:
    qgis_exe = root / "bin" / "qgis-qt6-bin.exe"
    python_exe = root / "apps" / "Python312" / "python.exe"
    qgis_exe.parent.mkdir(parents=True)
    python_exe.parent.mkdir(parents=True)
    qgis_exe.write_text("", encoding="utf-8")
    python_exe.write_text("", encoding="utf-8")
    return qgis_exe, python_exe


if __name__ == "__main__":
    unittest.main()
