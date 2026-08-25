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
        from qcopilots_common.constants import (
            CORS_ORIGINS_ENV,
            encode_cors_origins,
        )
        from qcopilots_common.process_controller import _merged_cors_origins

        old_origins = os.environ.get(CORS_ORIGINS_ENV)
        try:
            os.environ[CORS_ORIGINS_ENV] = encode_cors_origins(
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

    def test_cors_origin_environment_codec_is_cross_platform_and_legacy_compatible(self):
        from qcopilots_common.constants import (
            decode_cors_origins,
            encode_cors_origins,
        )

        origins = [
            "https://qcopilots.example",
            "http://127.0.0.1:8282",
        ]
        encoded = encode_cors_origins(origins)

        self.assertEqual(
            encoded,
            '["https://qcopilots.example","http://127.0.0.1:8282"]',
        )
        self.assertEqual(encode_cors_origins([]), "[]")
        self.assertEqual(
            decode_cors_origins(encoded, legacy_path_separator=":"),
            origins,
        )
        self.assertEqual(
            decode_cors_origins(
                "https://legacy.example;http://127.0.0.1:8282",
                legacy_path_separator=";",
            ),
            ["https://legacy.example", "http://127.0.0.1:8282"],
        )
        self.assertEqual(
            decode_cors_origins(
                '["https://invalid.example"',
                legacy_path_separator=":",
            ),
            [],
        )

    def test_start_rejects_non_loopback_and_wildcard_network_settings(self):
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plugin_dir = root / "qcopilots_dummy_service"
            plugin_dir.mkdir()
            controller = ProcessController(root / "state")
            cases = (
                ("0.0.0.0", "", []),
                ("192.168.1.50", "", []),
                ("127.0.0.1", "qgis-client-01.local", []),
                ("127.0.0.1", "", ["*"]),
            )
            for index, (host, advertised_host, cors_origins) in enumerate(cases):
                with self.subTest(
                    host=host,
                    advertised_host=advertised_host,
                    cors_origins=cors_origins,
                ):
                    manifest = ServiceManifest(
                        service_id=f"qcopilots.dummy_service_{index}",
                        display_name="QCopilots Dummy Service",
                        description="Dummy service for ProcessController tests.",
                        plugin_name="qcopilots_dummy_service",
                        plugin_dir=plugin_dir,
                        manifest_path=plugin_dir / "qcopilots_service.json",
                        transport=ServiceTransport(
                            host=host,
                            port=49531,
                            path="/mcp",
                            advertised_host=advertised_host,
                        ),
                        cors_origins=cors_origins,
                    )
                    with self.assertRaises(ValueError):
                        controller.start(manifest)

    def test_start_strict_port_policy_fails_without_falling_back(self):
        import re
        import socket

        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import (
            PORT_CONFLICT_POLICY_FAIL,
            ProcessController,
        )

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            occupied_port = int(listener.getsockname()[1])

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
                        host="127.0.0.1",
                        port=occupied_port,
                        path="/mcp",
                    ),
                )
                controller = ProcessController(root / "state")

                expected = re.escape(
                    f"Port 127.0.0.1:{occupied_port} is unavailable"
                )
                with self.assertRaisesRegex(RuntimeError, f"^{expected}$"):
                    controller.start(
                        manifest,
                        port_conflict_policy=PORT_CONFLICT_POLICY_FAIL,
                    )

                self.assertEqual(controller._service_reserved_ports, {})
                self.assertFalse((root / "state" / "qcopilots_dummy_service.json").exists())
        finally:
            listener.close()

    def test_strict_port_failure_does_not_block_another_service(self):
        import socket

        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import (
            PORT_CONFLICT_POLICY_FAIL,
            ProcessController,
            find_available_port,
        )

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            occupied_port = int(listener.getsockname()[1])

            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                controller = ProcessController(root / "state")

                occupied_manifest = ServiceManifest(
                    service_id="qcopilots.occupied_service",
                    display_name="Occupied service",
                    description="Service using an occupied port.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(port=occupied_port),
                )
                with self.assertRaises(RuntimeError):
                    controller._reserve_available_port(
                        occupied_manifest,
                        port_conflict_policy=PORT_CONFLICT_POLICY_FAIL,
                    )

                available_port = find_available_port("127.0.0.1", 0)
                available_manifest = ServiceManifest(
                    service_id="qcopilots.available_service",
                    display_name="Available service",
                    description="Service using an available port.",
                    plugin_name="qcopilots_dummy_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(port=available_port),
                )
                try:
                    self.assertEqual(
                        controller._reserve_available_port(
                            available_manifest,
                            port_conflict_policy=PORT_CONFLICT_POLICY_FAIL,
                        ),
                        available_port,
                    )
                finally:
                    controller._release_reserved_port(available_manifest)
        finally:
            listener.close()

    def test_port_conflict_policy_defaults_to_fallback_and_rejects_unknown_values(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

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
                transport=ServiceTransport(port=49531),
            )
            controller = ProcessController(root / "state")
            original_find_available_port = process_controller.find_available_port
            calls = []
            try:
                process_controller.find_available_port = (
                    lambda host, port: calls.append((host, port)) or port + 1
                )
                self.assertEqual(controller._reserve_available_port(manifest), 49532)
                self.assertEqual(calls, [("127.0.0.1", 49531)])
            finally:
                controller._release_reserved_port(manifest)
                process_controller.find_available_port = original_find_available_port

            with self.assertRaisesRegex(ValueError, "Unsupported port conflict policy"):
                controller._reserve_available_port(
                    manifest,
                    port_conflict_policy="unknown",
                )

    def test_start_keeps_auth_tokens_out_of_state_and_logs(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.constants import (
            MCP_AUTH_TOKEN_ENV,
            QGIS_BRIDGE_AUTH_TOKEN_ENV,
        )
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 501
            returncode = None

            def poll(self):
                return self.returncode

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                return [str(python_executable), str(manifest.entry_path)]

        originals = {
            "popen": process_controller.subprocess.Popen,
            "find_available_port": process_controller.find_available_port,
            "process_identity": process_controller.process_identity,
            "is_process_running": process_controller.is_process_running,
            "process_tree_pids": process_controller.process_tree_pids,
            "health_ok": process_controller.ProcessController._health_ok,
            "service_log_file": process_controller.service_log_file,
        }
        captured = {}
        mcp_token = "unit-test-mcp-secret"
        bridge_token = "unit-test-bridge-secret"
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_dummy_service"
                plugin_dir.mkdir()
                qgis_exe, _ = _write_qgis_package_layout(
                    root / "QGIS40200-RelWithDebInfo"
                )
                manifest = ServiceManifest(
                    service_id="qcopilots.auth_service",
                    display_name="QCopilots Auth Service",
                    description="Auth token ProcessController test.",
                    plugin_name="qcopilots_auth_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(
                        host="127.0.0.1",
                        port=49531,
                        path="/mcp",
                    ),
                )

                def fake_popen(command, *args, **kwargs):
                    del args
                    captured["command"] = command
                    captured["env"] = kwargs["env"]
                    return FakeProcess()

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.is_process_running = lambda pid: pid == 501
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: True
                )
                log_file = root / "logs" / "auth-service.log"
                process_controller.service_log_file = lambda service_id: log_file
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                )

                status = controller.start(
                    manifest,
                    extra_env={QGIS_BRIDGE_AUTH_TOKEN_ENV: bridge_token},
                    auth_token=mcp_token,
                )

                self.assertTrue(status.running)
                self.assertEqual(captured["env"][MCP_AUTH_TOKEN_ENV], mcp_token)
                self.assertEqual(
                    captured["env"][QGIS_BRIDGE_AUTH_TOKEN_ENV],
                    bridge_token,
                )
                self.assertEqual(
                    controller.current_auth_token(manifest.service_id),
                    mcp_token,
                )
                persisted_state = controller._state_path(manifest).read_text(
                    encoding="utf-8"
                )
                persisted_log = log_file.read_text(encoding="utf-8")
                command_text = json.dumps(captured["command"])
                for token in (mcp_token, bridge_token):
                    self.assertNotIn(token, persisted_state)
                    self.assertNotIn(token, persisted_log)
                    self.assertNotIn(token, command_text)
        finally:
            process_controller.subprocess.Popen = originals["popen"]
            process_controller.find_available_port = originals["find_available_port"]
            process_controller.process_identity = originals["process_identity"]
            process_controller.is_process_running = originals["is_process_running"]
            process_controller.process_tree_pids = originals["process_tree_pids"]
            process_controller.ProcessController._health_ok = originals["health_ok"]
            process_controller.service_log_file = originals["service_log_file"]

    def test_start_scrubs_inherited_bridge_credentials_for_non_bridge_service(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.constants import (
            BRIDGE_URL_ENV,
            QGIS_BRIDGE_AUTH_TOKEN_ENV,
        )
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 502
            returncode = None

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                if self.returncode is None:
                    self.returncode = 0
                return self.returncode

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                return [str(python_executable), str(manifest.entry_path)]

        originals = {
            "popen": process_controller.subprocess.Popen,
            "find_available_port": process_controller.find_available_port,
            "process_identity": process_controller.process_identity,
            "is_process_running": process_controller.is_process_running,
            "process_tree_pids": process_controller.process_tree_pids,
            "health_ok": process_controller.ProcessController._health_ok,
            "service_log_file": process_controller.service_log_file,
        }
        old_bridge_url = os.environ.get(BRIDGE_URL_ENV)
        old_bridge_token = os.environ.get(QGIS_BRIDGE_AUTH_TOKEN_ENV)
        captured = {}
        fake_process = FakeProcess()
        try:
            os.environ[BRIDGE_URL_ENV] = "http://127.0.0.1:49998"
            os.environ[QGIS_BRIDGE_AUTH_TOKEN_ENV] = "stale-parent-bridge-token"
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_non_bridge_service"
                plugin_dir.mkdir()
                qgis_exe, _ = _write_qgis_package_layout(
                    root / "QGIS40200-RelWithDebInfo"
                )
                manifest = ServiceManifest(
                    service_id="qcopilots.non_bridge_service",
                    display_name="QCopilots Non Bridge Service",
                    description="Inherited bridge credential scrub test.",
                    plugin_name="qcopilots_non_bridge_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(
                        host="127.0.0.1",
                        port=49531,
                        path="/mcp",
                    ),
                )

                def fake_popen(command, *args, **kwargs):
                    del command, args
                    captured["env"] = dict(kwargs["env"])
                    return fake_process

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.is_process_running = (
                    lambda pid: pid == fake_process.pid
                    and fake_process.returncode is None
                )
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: True
                )
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / f"{service_id}.log"
                )
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                )

                status = controller.start(manifest, auth_token="service-token")

                self.assertTrue(status.running)
                self.assertNotIn(BRIDGE_URL_ENV, captured["env"])
                self.assertNotIn(QGIS_BRIDGE_AUTH_TOKEN_ENV, captured["env"])

                fake_process.returncode = 0
                stopped = controller.stop(manifest)
                self.assertFalse(stopped.running)
        finally:
            if old_bridge_url is None:
                os.environ.pop(BRIDGE_URL_ENV, None)
            else:
                os.environ[BRIDGE_URL_ENV] = old_bridge_url
            if old_bridge_token is None:
                os.environ.pop(QGIS_BRIDGE_AUTH_TOKEN_ENV, None)
            else:
                os.environ[QGIS_BRIDGE_AUTH_TOKEN_ENV] = old_bridge_token
            process_controller.subprocess.Popen = originals["popen"]
            process_controller.find_available_port = originals["find_available_port"]
            process_controller.process_identity = originals["process_identity"]
            process_controller.is_process_running = originals["is_process_running"]
            process_controller.process_tree_pids = originals["process_tree_pids"]
            process_controller.ProcessController._health_ok = originals["health_ok"]
            process_controller.service_log_file = originals["service_log_file"]

    def test_health_check_uses_in_memory_bearer_token(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeResponse:
            status = 200

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                del exc_type, exc_value, traceback

        original_urlopen = process_controller.urlopen
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_health_auth"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.health_auth",
                    display_name="QCopilots Health Auth",
                    description="Health auth test.",
                    plugin_name="qcopilots_health_auth",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(
                        host="127.0.0.1",
                        port=49531,
                        path="/mcp",
                    ),
                )
                controller = ProcessController(root / "state")
                controller._remember_health_auth_token(
                    manifest,
                    49531,
                    "unit-test-health-secret",
                )
                captured = []

                def fake_urlopen(request, timeout):
                    captured.append((request, timeout))
                    return FakeResponse()

                process_controller.urlopen = fake_urlopen

                self.assertTrue(controller._health_ok("127.0.0.1", 49531))
                request, timeout = captured[0]
                self.assertEqual(request.full_url, "http://127.0.0.1:49531/health")
                self.assertEqual(
                    request.get_header("Authorization"),
                    "Bearer unit-test-health-secret",
                )
                self.assertEqual(timeout, 1.5)
        finally:
            process_controller.urlopen = original_urlopen

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
        original_process_tree_pids = process_controller.process_tree_pids
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
                process_controller.process_tree_pids = lambda pid: [pid]
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
            process_controller.process_tree_pids = original_process_tree_pids
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
        original_process_tree_pids = process_controller.process_tree_pids
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
                process_controller.process_tree_pids = lambda pid: [pid]
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
            process_controller.process_tree_pids = original_process_tree_pids
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

    @unittest.skipUnless(os.name == "nt", "Windows process query transport is platform-specific")
    def test_windows_cim_process_queries_use_binary_base64_transport_with_cp936_stderr(self):
        import base64
        import json

        import qcopilots_common.process_controller as process_controller

        original_is_process_running = process_controller.is_process_running
        original_run = process_controller.subprocess.run
        calls = []

        identity_payload = base64.b64encode(
            json.dumps(
                [
                    {
                        "ProcessId": 123,
                        "CreationDate": "20260730083000.000000+480",
                        "CommandLine": "python 服务.py",
                    }
                ],
                ensure_ascii=False,
            ).encode("utf-8")
        )
        tree_payload = base64.b64encode(
            json.dumps(
                [
                    {"ProcessId": 123, "ParentProcessId": 10},
                    {"ProcessId": 456, "ParentProcessId": 123},
                    {"ProcessId": 789, "ParentProcessId": 456},
                ]
            ).encode("utf-8")
        )

        def fake_run(command, *args, **kwargs):
            del args
            calls.append((command, kwargs))
            self.assertNotIn("text", kwargs)
            self.assertNotIn("encoding", kwargs)
            self.assertNotIn("errors", kwargs)
            script = command[-1]
            payload = identity_payload if "CreationDate,CommandLine" in script else tree_payload
            return types.SimpleNamespace(
                returncode=0,
                stdout=payload,
                stderr="没有可用实例。".encode("cp936"),
            )

        try:
            process_controller.is_process_running = lambda pid: pid == 123
            process_controller.subprocess.run = fake_run

            self.assertEqual(
                process_controller.process_identity(123),
                {
                    "pid": "123",
                    "creation_date": "20260730083000.000000+480",
                    "command_line": "python 服务.py",
                },
            )
            self.assertEqual(process_controller.process_tree_pids(123), [123, 456, 789])
            self.assertEqual(len(calls), 2)
            self.assertTrue(all("Get-CimInstance Win32_Process" in call[0][-1] for call in calls))
            self.assertTrue(all("wmic" not in " ".join(call[0]).lower() for call in calls))
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.subprocess.run = original_run

    @unittest.skipUnless(os.name == "nt", "Windows process query transport is platform-specific")
    def test_windows_cim_process_query_rejects_unframed_cp936_output_without_decoding_error(self):
        import qcopilots_common.process_controller as process_controller

        original_is_process_running = process_controller.is_process_running
        original_run = process_controller.subprocess.run

        def fake_run(command, *args, **kwargs):
            del command, args
            self.assertNotIn("text", kwargs)
            self.assertNotIn("errors", kwargs)
            return types.SimpleNamespace(
                returncode=0,
                stdout="没有可用实例。".encode("cp936"),
                stderr=b"",
            )

        try:
            process_controller.is_process_running = lambda pid: pid == 123
            process_controller.subprocess.run = fake_run

            self.assertEqual(process_controller.process_tree_pids(123), [123])
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

    def test_uv_runtime_long_preparation_honors_cancellation(self):
        import threading
        import time

        from qcopilots_common.uv_runtime import _run_runtime_command

        cancel_event = threading.Event()
        timer = threading.Timer(0.1, cancel_event.set)
        self.addCleanup(timer.cancel)
        timer.start()

        started_at = time.monotonic()
        with self.assertRaisesRegex(RuntimeError, "cancelled"):
            _run_runtime_command(
                [sys.executable, "-c", "import time; time.sleep(30)"],
                timeout=30,
                cancel_event=cancel_event,
            )

        self.assertLess(time.monotonic() - started_at, 5)

    def test_runtime_cancellation_is_forwarded_without_breaking_legacy_runtime(self):
        import threading

        from qcopilots_common.process_controller import _ensure_runtime

        cancel_event = threading.Event()
        modern_calls = []
        legacy_calls = []

        class ModernRuntime:
            def ensure_runtime(self, manifest, python_executable, cancel_event=None):
                modern_calls.append((manifest, python_executable, cancel_event))
                return Path(python_executable)

        class LegacyRuntime:
            def ensure_runtime(self, manifest, python_executable):
                legacy_calls.append((manifest, python_executable))
                return Path(python_executable)

        manifest = object()
        expected = Path(sys.executable)
        self.assertEqual(
            _ensure_runtime(
                ModernRuntime(),
                manifest,
                sys.executable,
                cancel_event=cancel_event,
            ),
            expected,
        )
        self.assertEqual(
            _ensure_runtime(
                LegacyRuntime(),
                manifest,
                sys.executable,
                cancel_event=cancel_event,
            ),
            expected,
        )
        self.assertEqual(modern_calls, [(manifest, sys.executable, cancel_event)])
        self.assertEqual(legacy_calls, [(manifest, sys.executable)])

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

    def test_manifest_controller_status_snapshot_does_not_wait_for_active_start_lock(self):
        import json
        import threading
        import time

        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

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
            state_path.write_text(
                json.dumps(
                    {
                        "pid": None,
                        "port": 49531,
                        "owner_token": controller.owner_token,
                        "startup_phase": "dependency-preparation",
                    }
                ),
                encoding="utf-8",
            )

            lock_acquired = threading.Event()
            release_lock = threading.Event()

            def hold_service_lock():
                with controller._service_lock(manifest.service_id):
                    lock_acquired.set()
                    release_lock.wait(5)

            thread = threading.Thread(target=hold_service_lock, daemon=True)
            thread.start()
            self.assertTrue(lock_acquired.wait(2))
            try:
                started_at = time.monotonic()
                status = controller.status_snapshot(manifest)
                elapsed = time.monotonic() - started_at
            finally:
                release_lock.set()
                thread.join(2)

            self.assertFalse(thread.is_alive())
            self.assertLess(elapsed, 0.5)
            self.assertFalse(status.running)
            self.assertEqual(status.startup_phase, "dependency-preparation")

    def test_manifest_controller_keeps_separate_process_handles_per_service(self):
        import threading

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
                return [str(python_executable), manifest.service_id]

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
                process_by_service = {
                    "qcopilots.service_one": FakeProcess(101),
                    "qcopilots.service_two": FakeProcess(102),
                }
                process_by_pid = {
                    process.pid: process for process in process_by_service.values()
                }
                launch_barrier = threading.Barrier(2)

                def fake_popen(command, *args, **kwargs):
                    del args, kwargs
                    launch_barrier.wait(timeout=5)
                    return process_by_service[command[1]]

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
                results = {}
                errors = []

                def start_manifest(manifest, auth_token):
                    try:
                        results[manifest.service_id] = controller.start(
                            manifest,
                            auth_token=auth_token,
                        )
                    except Exception as err:
                        errors.append(err)

                threads = [
                    threading.Thread(
                        target=start_manifest,
                        args=(manifest, f"token-{index}"),
                    )
                    for index, manifest in enumerate(manifests)
                ]
                for thread in threads:
                    thread.start()
                for thread in threads:
                    thread.join(timeout=10)
                    self.assertFalse(thread.is_alive())

                self.assertEqual(errors, [])
                self.assertEqual(set(results), {item.service_id for item in manifests})
                self.assertTrue(all(status.running for status in results.values()))
                self.assertEqual(
                    controller.current_auth_token("qcopilots.service_one"),
                    "token-0",
                )
                self.assertEqual(
                    controller.current_auth_token("qcopilots.service_two"),
                    "token-1",
                )

                self.assertEqual(
                    set(controller._manifest_processes),
                    {"qcopilots.service_one", "qcopilots.service_two"},
                )

                stopped_one = controller.stop(manifests[0], timeout_seconds=1)
                self.assertFalse(stopped_one.running)
                self.assertNotIn("qcopilots.service_one", controller._manifest_processes)
                self.assertIn("qcopilots.service_two", controller._manifest_processes)
                self.assertIsNone(
                    controller.current_auth_token("qcopilots.service_one")
                )
                self.assertEqual(process_by_pid[101].wait_calls, 1)
                self.assertEqual(process_by_pid[102].wait_calls, 0)

                stopped_two = controller.stop(manifests[1], timeout_seconds=1)
                self.assertFalse(stopped_two.running)
                self.assertEqual(controller._manifest_processes, {})
                self.assertIsNone(
                    controller.current_auth_token("qcopilots.service_two")
                )
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

    def test_concurrent_services_reserve_distinct_fallback_ports_and_tokens(self):
        import socket
        import threading

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.constants import MCP_AUTH_TOKEN_ENV
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            def __init__(self, pid):
                self.pid = pid
                self.returncode = None

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                if self.returncode is None:
                    self.returncode = 0
                return self.returncode

            def kill(self):
                self.returncode = -9

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                return [str(python_executable), manifest.service_id]

        originals = {
            "popen": process_controller.subprocess.Popen,
            "process_identity": process_controller.process_identity,
            "is_process_running": process_controller.is_process_running,
            "process_tree_pids": process_controller.process_tree_pids,
            "terminate_process_tree": process_controller.terminate_process_tree,
            "health_ok": process_controller.ProcessController._health_ok,
            "service_log_file": process_controller.service_log_file,
        }
        occupied_socket = None
        try:
            for candidate_port in range(49100, 49200):
                candidate_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                try:
                    candidate_socket.bind(("127.0.0.1", candidate_port))
                except OSError:
                    candidate_socket.close()
                    continue
                occupied_socket = candidate_socket
                preferred_port = candidate_port
                break
            self.assertIsNotNone(occupied_socket, "Could not reserve a busy test port")
            occupied_socket.listen(1)

            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                qgis_exe, _ = _write_qgis_package_layout(
                    root / "QGIS40200-RelWithDebInfo"
                )
                manifests = [
                    ServiceManifest(
                        service_id="qcopilots.same_port_one",
                        display_name="QCopilots Same Port One",
                        description="First shared preferred port service.",
                        plugin_name="qcopilots_same_port_one",
                        plugin_dir=plugin_dir,
                        manifest_path=plugin_dir / "same_port_one.json",
                        transport=ServiceTransport(
                            host="127.0.0.1",
                            port=preferred_port,
                            path="/mcp",
                        ),
                    ),
                    ServiceManifest(
                        service_id="qcopilots.same_port_two",
                        display_name="QCopilots Same Port Two",
                        description="Second shared preferred port service.",
                        plugin_name="qcopilots_same_port_two",
                        plugin_dir=plugin_dir,
                        manifest_path=plugin_dir / "same_port_two.json",
                        transport=ServiceTransport(
                            host="127.0.0.1",
                            port=preferred_port,
                            path="/mcp",
                        ),
                    ),
                ]
                process_by_service = {
                    manifests[0].service_id: FakeProcess(701),
                    manifests[1].service_id: FakeProcess(702),
                }
                process_by_pid = {
                    process.pid: process for process in process_by_service.values()
                }
                launch_barrier = threading.Barrier(2)
                capture_lock = threading.Lock()
                captured_env = {}

                def fake_popen(command, *args, **kwargs):
                    del args
                    service_id = command[1]
                    with capture_lock:
                        captured_env[service_id] = dict(kwargs["env"])
                    launch_barrier.wait(timeout=5)
                    return process_by_service[service_id]

                process_controller.subprocess.Popen = fake_popen
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.is_process_running = (
                    lambda pid: pid in process_by_pid
                    and process_by_pid[pid].returncode is None
                )
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.terminate_process_tree = lambda pid, force=False: None
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: True
                )
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / f"{service_id}.log"
                )
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                )
                results = {}
                errors = []
                tokens = {
                    manifests[0].service_id: "independent-token-one",
                    manifests[1].service_id: "independent-token-two",
                }

                def start_manifest(manifest):
                    try:
                        results[manifest.service_id] = controller.start(
                            manifest,
                            auth_token=tokens[manifest.service_id],
                        )
                    except Exception as err:
                        errors.append(err)

                threads = [
                    threading.Thread(target=start_manifest, args=(manifest,))
                    for manifest in manifests
                ]
                for thread in threads:
                    thread.start()
                for thread in threads:
                    thread.join(timeout=10)
                    self.assertFalse(thread.is_alive())

                self.assertEqual(errors, [])
                self.assertEqual(set(results), {item.service_id for item in manifests})
                selected_ports = {
                    int(captured_env[manifest.service_id]["QCOPILOTS_SERVICE_PORT"])
                    for manifest in manifests
                }
                self.assertEqual(len(selected_ports), 2)
                self.assertNotIn(preferred_port, selected_ports)
                for manifest in manifests:
                    service_id = manifest.service_id
                    self.assertEqual(
                        captured_env[service_id][MCP_AUTH_TOKEN_ENV],
                        tokens[service_id],
                    )
                    self.assertEqual(
                        controller.current_auth_token(service_id),
                        tokens[service_id],
                    )
                    self.assertEqual(results[service_id].port, int(
                        captured_env[service_id]["QCOPILOTS_SERVICE_PORT"]
                    ))
                self.assertEqual(controller._reserved_ports, {})
                self.assertEqual(controller._service_reserved_ports, {})

                for manifest in manifests:
                    stopped = controller.stop(manifest, timeout_seconds=1)
                    self.assertFalse(stopped.running)
                    self.assertIsNone(
                        controller.current_auth_token(manifest.service_id)
                    )
                self.assertEqual(controller._reserved_ports, {})
                self.assertEqual(controller._service_reserved_ports, {})
        finally:
            if occupied_socket is not None:
                occupied_socket.close()
            process_controller.subprocess.Popen = originals["popen"]
            process_controller.process_identity = originals["process_identity"]
            process_controller.is_process_running = originals["is_process_running"]
            process_controller.process_tree_pids = originals["process_tree_pids"]
            process_controller.terminate_process_tree = originals[
                "terminate_process_tree"
            ]
            process_controller.ProcessController._health_ok = originals["health_ok"]
            process_controller.service_log_file = originals["service_log_file"]

    def test_cancelled_service_waiting_for_runtime_lock_skips_legacy_runtime(self):
        import threading
        import time

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        runtime_entered = threading.Event()
        release_runtime = threading.Event()

        class BlockingLegacyRuntime:
            def __init__(self):
                self.calls = []

            def ensure_runtime(self, manifest, python_executable):
                self.calls.append(manifest.service_id)
                runtime_entered.set()
                if not release_runtime.wait(timeout=5):
                    raise TimeoutError("Legacy runtime release was not signaled")
                return python_executable

        class FakeProcess:
            def __init__(self, pid):
                self.pid = pid
                self.returncode = None

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                self.returncode = 0
                return self.returncode

            def kill(self):
                self.returncode = -9

        originals = {
            "popen": process_controller.subprocess.Popen,
            "find_available_port": process_controller.find_available_port,
            "process_identity": process_controller.process_identity,
            "is_process_running": process_controller.is_process_running,
            "process_tree_pids": process_controller.process_tree_pids,
            "terminate_process_tree": process_controller.terminate_process_tree,
            "health_ok": process_controller.ProcessController._health_ok,
            "service_log_file": process_controller.service_log_file,
        }
        first_thread = None
        second_thread = None
        first_cancel = threading.Event()
        second_cancel = threading.Event()
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                qgis_exe, _ = _write_qgis_package_layout(
                    root / "QGIS40200-RelWithDebInfo"
                )
                manifests = []
                for index in range(2):
                    plugin_dir = root / f"plugin_{index}"
                    plugin_dir.mkdir()
                    (plugin_dir / "server.py").write_text(
                        "raise SystemExit(0)\n",
                        encoding="utf-8",
                    )
                    manifests.append(
                        ServiceManifest(
                            service_id=f"qcopilots.runtime_queue_{index}",
                            display_name=f"QCopilots Runtime Queue {index}",
                            description="Runtime lock cancellation test service.",
                            plugin_name=f"qcopilots_runtime_queue_{index}",
                            plugin_dir=plugin_dir,
                            manifest_path=plugin_dir / "qcopilots_service.json",
                            transport=ServiceTransport(
                                host="127.0.0.1",
                                port=49610 + index,
                                path="/mcp",
                            ),
                        )
                    )

                runtime = BlockingLegacyRuntime()
                processes = []

                def fake_popen(command, *args, **kwargs):
                    del command, args, kwargs
                    process = FakeProcess(810 + len(processes))
                    processes.append(process)
                    return process

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.is_process_running = lambda pid: any(
                    process.pid == pid and process.returncode is None
                    for process in processes
                )
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.terminate_process_tree = lambda pid, force=False: None
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: True
                )
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / f"{service_id}.log"
                )
                controller = ProcessController(
                    root / "state",
                    runtime=runtime,
                    qgis_executable=qgis_exe,
                )
                results = {}
                errors = []

                def start_manifest(manifest, cancel_event):
                    try:
                        results[manifest.service_id] = controller.start(
                            manifest,
                            cancel_event=cancel_event,
                        )
                    except Exception as err:
                        errors.append(err)

                first_thread = threading.Thread(
                    target=start_manifest,
                    args=(manifests[0], first_cancel),
                )
                first_thread.start()
                self.assertTrue(runtime_entered.wait(timeout=3))

                second_thread = threading.Thread(
                    target=start_manifest,
                    args=(manifests[1], second_cancel),
                )
                second_thread.start()
                time.sleep(0.2)
                self.assertTrue(second_thread.is_alive())

                second_cancel.set()
                second_thread.join(timeout=3)
                self.assertFalse(second_thread.is_alive())
                self.assertTrue(first_thread.is_alive())
                self.assertEqual(runtime.calls, [manifests[0].service_id])
                self.assertFalse(results[manifests[1].service_id].running)

                release_runtime.set()
                first_thread.join(timeout=5)
                self.assertFalse(first_thread.is_alive())
                self.assertEqual(errors, [])
                self.assertTrue(results[manifests[0].service_id].running)
                self.assertEqual(len(processes), 1)
                self.assertFalse(controller.stop(manifests[0], timeout_seconds=1).running)
        finally:
            first_cancel.set()
            second_cancel.set()
            release_runtime.set()
            for thread in (first_thread, second_thread):
                if thread is not None:
                    thread.join(timeout=5)
            process_controller.subprocess.Popen = originals["popen"]
            process_controller.find_available_port = originals["find_available_port"]
            process_controller.process_identity = originals["process_identity"]
            process_controller.is_process_running = originals["is_process_running"]
            process_controller.process_tree_pids = originals["process_tree_pids"]
            process_controller.terminate_process_tree = originals[
                "terminate_process_tree"
            ]
            process_controller.ProcessController._health_ok = originals["health_ok"]
            process_controller.service_log_file = originals["service_log_file"]

    def test_failed_start_releases_token_and_port_reservation(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.constants import MCP_AUTH_TOKEN_ENV
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import (
            HTTP_HEALTH_TIMEOUT,
            PROCESS_LAUNCH_FAILED,
            ProcessController,
        )

        class FakeProcess:
            pid = 703
            returncode = None

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                if self.returncode is None:
                    self.returncode = 0
                return self.returncode

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                return [str(python_executable), manifest.service_id]

        originals = {
            "popen": process_controller.subprocess.Popen,
            "find_available_port": process_controller.find_available_port,
            "process_identity": process_controller.process_identity,
            "is_process_running": process_controller.is_process_running,
            "process_tree_pids": process_controller.process_tree_pids,
            "terminate_process_tree": process_controller.terminate_process_tree,
            "health_ok": process_controller.ProcessController._health_ok,
            "service_log_file": process_controller.service_log_file,
        }
        fake_process = FakeProcess()
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                qgis_exe, _ = _write_qgis_package_layout(
                    root / "QGIS40200-RelWithDebInfo"
                )
                preferred_port = 49531
                failed_manifest = ServiceManifest(
                    service_id="qcopilots.failed_reservation",
                    display_name="QCopilots Failed Reservation",
                    description="Failed reservation cleanup service.",
                    plugin_name="qcopilots_failed_reservation",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "failed.json",
                    transport=ServiceTransport(
                        host="127.0.0.1",
                        port=preferred_port,
                        path="/mcp",
                    ),
                )
                retry_manifest = ServiceManifest(
                    service_id="qcopilots.reused_reservation",
                    display_name="QCopilots Reused Reservation",
                    description="Reused reservation service.",
                    plugin_name="qcopilots_reused_reservation",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "reused.json",
                    transport=ServiceTransport(
                        host="127.0.0.1",
                        port=preferred_port,
                        path="/mcp",
                    ),
                )
                timeout_manifest = ServiceManifest(
                    service_id="qcopilots.timeout_reservation",
                    display_name="QCopilots Timeout Reservation",
                    description="Timed out reservation cleanup service.",
                    plugin_name="qcopilots_timeout_reservation",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "timeout.json",
                    transport=ServiceTransport(
                        host="127.0.0.1",
                        port=preferred_port,
                        path="/mcp",
                    ),
                )
                captured_env = {}

                def fake_popen(command, *args, **kwargs):
                    del args
                    service_id = command[1]
                    captured_env[service_id] = dict(kwargs["env"])
                    if service_id == failed_manifest.service_id:
                        raise OSError("intentional process launch failure")
                    return fake_process

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.is_process_running = (
                    lambda pid: pid == fake_process.pid
                    and fake_process.returncode is None
                )
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.terminate_process_tree = lambda pid, force=False: None
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: True
                )
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / f"{service_id}.log"
                )
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                )

                failed = controller.start(
                    failed_manifest,
                    auth_token="failed-service-token",
                )

                self.assertFalse(failed.running)
                self.assertEqual(
                    controller._read_state(failed_manifest)["startup_failure"],
                    PROCESS_LAUNCH_FAILED,
                )
                self.assertIsNone(
                    controller.current_auth_token(failed_manifest.service_id)
                )
                self.assertEqual(controller._reserved_ports, {})
                self.assertEqual(controller._service_reserved_ports, {})

                retried = controller.start(
                    retry_manifest,
                    auth_token="retry-service-token",
                )

                self.assertTrue(retried.running)
                self.assertEqual(
                    int(
                        captured_env[failed_manifest.service_id][
                            "QCOPILOTS_SERVICE_PORT"
                        ]
                    ),
                    preferred_port,
                )
                self.assertEqual(
                    int(
                        captured_env[retry_manifest.service_id][
                            "QCOPILOTS_SERVICE_PORT"
                        ]
                    ),
                    preferred_port,
                )
                self.assertEqual(
                    captured_env[retry_manifest.service_id][MCP_AUTH_TOKEN_ENV],
                    "retry-service-token",
                )
                self.assertEqual(
                    controller.current_auth_token(retry_manifest.service_id),
                    "retry-service-token",
                )
                self.assertEqual(controller._reserved_ports, {})
                self.assertEqual(controller._service_reserved_ports, {})

                fake_process.returncode = 0
                stopped = controller.stop(retry_manifest)
                self.assertFalse(stopped.running)
                self.assertIsNone(
                    controller.current_auth_token(retry_manifest.service_id)
                )
                self.assertEqual(controller._reserved_ports, {})
                self.assertEqual(controller._service_reserved_ports, {})

                fake_process.returncode = None
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: False
                )
                timed_out = controller.start(
                    timeout_manifest,
                    auth_token="timeout-service-token",
                    startup_timeout_seconds=0.01,
                )

                self.assertTrue(timed_out.running)
                self.assertEqual(
                    controller._read_state(timeout_manifest)["startup_failure"],
                    HTTP_HEALTH_TIMEOUT,
                )
                self.assertEqual(
                    controller.current_auth_token(timeout_manifest.service_id),
                    "timeout-service-token",
                )
                self.assertEqual(
                    controller._service_reserved_ports[timeout_manifest.service_id][1],
                    preferred_port,
                )
                self.assertEqual(len(controller._reserved_ports), 1)

                stopped = controller.stop(timeout_manifest, timeout_seconds=1)
                self.assertFalse(stopped.running)
                self.assertIsNone(
                    controller.current_auth_token(timeout_manifest.service_id)
                )
                self.assertEqual(controller._reserved_ports, {})
                self.assertEqual(controller._service_reserved_ports, {})
        finally:
            process_controller.subprocess.Popen = originals["popen"]
            process_controller.find_available_port = originals["find_available_port"]
            process_controller.process_identity = originals["process_identity"]
            process_controller.is_process_running = originals["is_process_running"]
            process_controller.process_tree_pids = originals["process_tree_pids"]
            process_controller.terminate_process_tree = originals[
                "terminate_process_tree"
            ]
            process_controller.ProcessController._health_ok = originals["health_ok"]
            process_controller.service_log_file = originals["service_log_file"]

    def test_same_service_concurrent_start_is_idempotent_and_keeps_original_token(self):
        import threading

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 301
            returncode = None

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                self.returncode = 0
                return 0

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                return [str(python_executable), manifest.service_id]

        originals = {
            "popen": process_controller.subprocess.Popen,
            "find_available_port": process_controller.find_available_port,
            "process_identity": process_controller.process_identity,
            "is_process_running": process_controller.is_process_running,
            "process_tree_pids": process_controller.process_tree_pids,
            "health_ok": process_controller.ProcessController._health_ok,
            "service_log_file": process_controller.service_log_file,
        }
        release_popen = threading.Event()
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "qcopilots_idempotent_service"
                plugin_dir.mkdir()
                qgis_exe, _ = _write_qgis_package_layout(
                    root / "QGIS40200-RelWithDebInfo"
                )
                manifest = ServiceManifest(
                    service_id="qcopilots.idempotent_service",
                    display_name="QCopilots Idempotent Service",
                    description="Concurrent start idempotency test.",
                    plugin_name="qcopilots_idempotent_service",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "qcopilots_service.json",
                    transport=ServiceTransport(
                        host="127.0.0.1",
                        port=49531,
                        path="/mcp",
                    ),
                )
                fake_process = FakeProcess()
                popen_entered = threading.Event()
                popen_calls = []

                def fake_popen(command, *args, **kwargs):
                    del args, kwargs
                    popen_calls.append(command)
                    popen_entered.set()
                    if not release_popen.wait(timeout=5):
                        raise TimeoutError("Popen release was not signaled")
                    return fake_process

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": f"created-{pid}",
                }
                process_controller.is_process_running = (
                    lambda pid: pid == fake_process.pid
                    and fake_process.returncode is None
                )
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: True
                )
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / f"{service_id}.log"
                )
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                )
                results = []
                errors = []
                second_started = threading.Event()

                def start_service(auth_token, started_event=None):
                    if started_event is not None:
                        started_event.set()
                    try:
                        results.append(
                            controller.start(manifest, auth_token=auth_token)
                        )
                    except Exception as err:
                        errors.append(err)

                first_thread = threading.Thread(
                    target=start_service,
                    args=("original-token",),
                )
                first_thread.start()
                self.assertTrue(popen_entered.wait(timeout=5))
                second_thread = threading.Thread(
                    target=start_service,
                    args=("replacement-token", second_started),
                )
                second_thread.start()
                self.assertTrue(second_started.wait(timeout=5))
                release_popen.set()
                for thread in (first_thread, second_thread):
                    thread.join(timeout=10)
                    self.assertFalse(thread.is_alive())

                self.assertEqual(errors, [])
                self.assertEqual(len(popen_calls), 1)
                self.assertEqual(len(results), 2)
                self.assertTrue(all(status.running for status in results))
                self.assertEqual(
                    {status.pid for status in results},
                    {fake_process.pid},
                )
                self.assertEqual(
                    controller.current_auth_token(manifest.service_id),
                    "original-token",
                )
                persisted_state = controller._state_path(manifest).read_text(
                    encoding="utf-8"
                )
                self.assertNotIn("original-token", persisted_state)
                self.assertNotIn("replacement-token", persisted_state)

                fake_process.returncode = 0
                stopped = controller.stop(manifest)
                self.assertFalse(stopped.running)
                self.assertIsNone(
                    controller.current_auth_token(manifest.service_id)
                )
        finally:
            release_popen.set()
            process_controller.subprocess.Popen = originals["popen"]
            process_controller.find_available_port = originals["find_available_port"]
            process_controller.process_identity = originals["process_identity"]
            process_controller.is_process_running = originals["is_process_running"]
            process_controller.process_tree_pids = originals["process_tree_pids"]
            process_controller.ProcessController._health_ok = originals["health_ok"]
            process_controller.service_log_file = originals["service_log_file"]

    def test_manifest_controller_startup_timeout_defaults_to_30_seconds_and_is_configurable(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.process_controller import ProcessController

        old_timeout = os.environ.get(process_controller.STARTUP_TIMEOUT_ENV)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                os.environ.pop(process_controller.STARTUP_TIMEOUT_ENV, None)
                default_controller = ProcessController(Path(tmp) / "default")
                self.assertEqual(default_controller.startup_timeout_seconds, 30.0)

                os.environ[process_controller.STARTUP_TIMEOUT_ENV] = "42.5"
                configured_controller = ProcessController(Path(tmp) / "configured")
                self.assertEqual(configured_controller.startup_timeout_seconds, 42.5)

                explicit_controller = ProcessController(
                    Path(tmp) / "explicit",
                    startup_timeout_seconds=3,
                )
                self.assertEqual(explicit_controller.startup_timeout_seconds, 3.0)

                os.environ[process_controller.STARTUP_TIMEOUT_ENV] = "not-a-number"
                fallback_controller = ProcessController(Path(tmp) / "fallback")
                self.assertEqual(fallback_controller.startup_timeout_seconds, 30.0)
        finally:
            if old_timeout is None:
                os.environ.pop(process_controller.STARTUP_TIMEOUT_ENV, None)
            else:
                os.environ[process_controller.STARTUP_TIMEOUT_ENV] = old_timeout

    def test_manifest_controller_reports_early_process_exit_with_bounded_log_tail(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import (
            MAX_STARTUP_LOG_TAIL_CHARS,
            PROCESS_EXITED_BEFORE_HEALTHY,
            ProcessController,
        )

        class FakeProcess:
            pid = 501
            returncode = 23

            def poll(self):
                return self.returncode

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                del manifest
                return [str(python_executable), "-c", "raise SystemExit(23)"]

        original_popen = process_controller.subprocess.Popen
        original_find_available_port = process_controller.find_available_port
        original_health_ok = process_controller.ProcessController._health_ok
        original_service_log_file = process_controller.service_log_file
        health_calls = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.early_exit",
                    display_name="QCopilots Early Exit",
                    description="Early exit test service.",
                    plugin_name="qcopilots_early_exit",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                log_file = root / "logs" / "early-exit.log"

                def fake_popen(command, *args, **kwargs):
                    del command, args
                    kwargs["stdout"].write(
                        "BEGIN-SHOULD-BE-TRUNCATED\n"
                        + ("x" * (MAX_STARTUP_LOG_TAIL_CHARS * 5))
                        + "\nFINAL EARLY EXIT\n"
                    )
                    kwargs["stdout"].flush()
                    return FakeProcess()

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: health_calls.append((host, port)) or False
                )
                process_controller.service_log_file = lambda service_id: log_file
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                    startup_timeout_seconds=0.1,
                    startup_poll_interval_seconds=0.001,
                )

                status = controller.start(
                    manifest,
                    auth_token="failed-service-token",
                )

                self.assertFalse(status.running)
                self.assertEqual(status.health, PROCESS_EXITED_BEFORE_HEALTHY)
                self.assertEqual(status.exit_code, 23)
                self.assertEqual(status.startup_phase, "process-exit")
                self.assertIn("Exit code: 23", status.diagnostic)
                self.assertIn(f"Log file: {log_file}", status.diagnostic)
                self.assertIn("FINAL EARLY EXIT", status.log_tail)
                self.assertNotIn("BEGIN-SHOULD-BE-TRUNCATED", status.log_tail)
                self.assertLessEqual(len(status.log_tail), MAX_STARTUP_LOG_TAIL_CHARS)
                self.assertEqual(health_calls, [])
                self.assertEqual(controller._manifest_processes, {})
                self.assertIsNone(controller.current_auth_token(manifest.service_id))
                state = json.loads(
                    (root / "state" / "qcopilots_early_exit.json").read_text(encoding="utf-8")
                )
                self.assertIsNone(state["pid"])
                self.assertEqual(state["last_pid"], 501)
                self.assertEqual(state["exit_code"], 23)
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.find_available_port = original_find_available_port
            process_controller.ProcessController._health_ok = original_health_ok
            process_controller.service_log_file = original_service_log_file

    def test_manifest_controller_classifies_uv_overlay_resolution_exit_as_dependency_failure(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import (
            DEPENDENCY_PREPARATION_FAILED,
            ProcessController,
        )

        class FakeProcess:
            pid = 502
            returncode = 1

            def poll(self):
                return self.returncode

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                return [
                    str(python_executable),
                    "-m",
                    "uv",
                    "run",
                    "--with-requirements",
                    str(manifest.requirements_path),
                    str(manifest.entry_path),
                ]

        original_popen = process_controller.subprocess.Popen
        original_find_available_port = process_controller.find_available_port
        original_service_log_file = process_controller.service_log_file

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.dependency_failure",
                    display_name="QCopilots Dependency Failure",
                    description="Dependency preparation test service.",
                    plugin_name="qcopilots_dependency_failure",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                log_file = root / "logs" / "dependency-failure.log"

                def fake_popen(command, *args, **kwargs):
                    del command, args
                    kwargs["stdout"].write(
                        "No solution found when resolving dependencies\n"
                        "PyYAML was not found in the cache and the network is disabled\n"
                    )
                    kwargs["stdout"].flush()
                    return FakeProcess()

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.service_log_file = lambda service_id: log_file
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                    startup_timeout_seconds=0.1,
                    startup_poll_interval_seconds=0.001,
                )

                status = controller.start(
                    manifest,
                    auth_token="failed-dependency-token",
                )

                self.assertFalse(status.running)
                self.assertEqual(status.health, DEPENDENCY_PREPARATION_FAILED)
                self.assertEqual(status.exit_code, 1)
                self.assertEqual(status.startup_phase, "dependency-preparation")
                self.assertIn("Dependency preparation failed", status.diagnostic)
                self.assertIn("PyYAML was not found in the cache", status.log_tail)
                self.assertIsNone(controller.current_auth_token(manifest.service_id))
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.find_available_port = original_find_available_port
            process_controller.service_log_file = original_service_log_file

    def test_manifest_controller_accepts_delayed_health_within_configured_timeout(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 503
            returncode = None

            def poll(self):
                return self.returncode

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                del manifest
                return [str(python_executable), "-c", "print('starting')"]

        original_popen = process_controller.subprocess.Popen
        original_find_available_port = process_controller.find_available_port
        original_process_identity = process_controller.process_identity
        original_process_matches_state = process_controller.process_matches_state
        original_is_process_running = process_controller.is_process_running
        original_process_tree_pids = process_controller.process_tree_pids
        original_health_ok = process_controller.ProcessController._health_ok
        original_service_log_file = process_controller.service_log_file
        health_calls = []

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.delayed_health",
                    display_name="QCopilots Delayed Health",
                    description="Delayed health test service.",
                    plugin_name="qcopilots_delayed_health",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                fake_process = FakeProcess()

                process_controller.subprocess.Popen = lambda *args, **kwargs: fake_process
                process_controller.find_available_port = lambda host, port: port
                process_controller.process_identity = lambda pid: {
                    "pid": str(pid),
                    "creation_date": "created-503",
                }
                process_controller.process_matches_state = lambda pid, state: True
                process_controller.is_process_running = (
                    lambda pid: pid == 503 and fake_process.returncode is None
                )
                process_controller.process_tree_pids = lambda pid: [pid]

                def delayed_health(self, host, port):
                    del self, host, port
                    health_calls.append(True)
                    return len(health_calls) >= 3

                process_controller.ProcessController._health_ok = delayed_health
                process_controller.service_log_file = (
                    lambda service_id: root / "logs" / "delayed-health.log"
                )
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                    startup_timeout_seconds=0.2,
                    startup_poll_interval_seconds=0.001,
                )

                status = controller.start(manifest)

                self.assertTrue(status.running)
                self.assertEqual(status.health, "ok")
                self.assertEqual(status.startup_phase, "ready")
                self.assertEqual(status.diagnostic, "")
                self.assertGreaterEqual(len(health_calls), 4)
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.find_available_port = original_find_available_port
            process_controller.process_identity = original_process_identity
            process_controller.process_matches_state = original_process_matches_state
            process_controller.is_process_running = original_is_process_running
            process_controller.process_tree_pids = original_process_tree_pids
            process_controller.ProcessController._health_ok = original_health_ok
            process_controller.service_log_file = original_service_log_file

    def test_manifest_controller_health_timeout_retains_diagnostic_and_tracked_stop_is_safe(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import HTTP_HEALTH_TIMEOUT, ProcessController

        class FakeProcess:
            pid = 504
            returncode = None

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                return self.returncode

            def kill(self):
                self.returncode = -9

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                del manifest
                return [str(python_executable), "-c", "print('waiting for health')"]

        original_popen = process_controller.subprocess.Popen
        original_find_available_port = process_controller.find_available_port
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
                manifest = ServiceManifest(
                    service_id="qcopilots.health_timeout",
                    display_name="QCopilots Health Timeout",
                    description="Health timeout test service.",
                    plugin_name="qcopilots_health_timeout",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "service.json",
                    transport=ServiceTransport(host="127.0.0.1", port=49531, path="/mcp"),
                )
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                fake_process = FakeProcess()
                log_file = root / "logs" / "health-timeout.log"

                def fake_popen(command, *args, **kwargs):
                    del command, args
                    kwargs["stdout"].write("service process remains alive\n")
                    kwargs["stdout"].flush()
                    return fake_process

                def fake_terminate(pid, force=False):
                    terminated.append((pid, force))
                    fake_process.returncode = 0

                process_controller.subprocess.Popen = fake_popen
                process_controller.find_available_port = lambda host, port: port
                process_controller.is_process_running = (
                    lambda pid: pid == 504 and fake_process.returncode is None
                )
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.terminate_process_tree = fake_terminate
                process_controller.ProcessController._health_ok = lambda self, host, port: False
                process_controller.service_log_file = lambda service_id: log_file
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                    startup_timeout_seconds=0.01,
                    startup_poll_interval_seconds=0.001,
                )

                timed_out = controller.start(manifest)

                self.assertTrue(timed_out.running)
                self.assertEqual(timed_out.health, HTTP_HEALTH_TIMEOUT)
                self.assertIsNone(timed_out.exit_code)
                self.assertEqual(timed_out.startup_phase, "http-health")
                self.assertIn("Startup timeout: 0.01 seconds", timed_out.diagnostic)
                self.assertIn(f"Log file: {log_file}", timed_out.diagnostic)
                self.assertIn("service process remains alive", timed_out.log_tail)
                state_path = root / "state" / "qcopilots_health_timeout.json"
                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertNotIn("process_identity", state)
                self.assertIn(manifest.service_id, controller._manifest_processes)

                stopped = controller.stop(manifest, timeout_seconds=1)

                self.assertFalse(stopped.running)
                self.assertEqual(stopped.health, HTTP_HEALTH_TIMEOUT)
                self.assertEqual(stopped.diagnostic, timed_out.diagnostic)
                self.assertEqual(terminated, [(504, False)])
                self.assertEqual(controller._manifest_processes, {})
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.find_available_port = original_find_available_port
            process_controller.is_process_running = original_is_process_running
            process_controller.process_tree_pids = original_process_tree_pids
            process_controller.terminate_process_tree = original_terminate_process_tree
            process_controller.ProcessController._health_ok = original_health_ok
            process_controller.service_log_file = original_service_log_file

    def test_manifest_controller_reconciles_tracked_handle_before_restart(self):
        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            def __init__(self, pid, exit_on_poll=None):
                self.pid = pid
                self.exit_on_poll = exit_on_poll
                self.returncode = None
                self.poll_calls = 0
                self.wait_calls = 0

            def poll(self):
                self.poll_calls += 1
                if self.returncode is None and self.exit_on_poll is not None:
                    self.returncode = self.exit_on_poll
                return self.returncode

            def wait(self, timeout=None):
                del timeout
                self.wait_calls += 1
                return self.returncode

            def kill(self):
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

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")

                for case_name, old_exit_code in (("exited", 17), ("live", None)):
                    with self.subTest(case=case_name):
                        plugin_dir = root / f"plugin-{case_name}"
                        plugin_dir.mkdir()
                        manifest = ServiceManifest(
                            service_id=f"qcopilots.reconcile_{case_name}",
                            display_name=f"QCopilots Reconcile {case_name}",
                            description="Tracked process reconciliation test service.",
                            plugin_name=f"qcopilots_reconcile_{case_name}",
                            plugin_dir=plugin_dir,
                            manifest_path=plugin_dir / "service.json",
                            transport=ServiceTransport(
                                host="127.0.0.1",
                                port=49531,
                                path="/mcp",
                            ),
                        )
                        old_process = FakeProcess(610 if case_name == "exited" else 611, old_exit_code)
                        new_process = FakeProcess(710 if case_name == "exited" else 711)
                        processes = {
                            old_process.pid: old_process,
                            new_process.pid: new_process,
                        }
                        terminated = []
                        controller = ProcessController(
                            root / f"state-{case_name}",
                            runtime=FakeRuntime(),
                            qgis_executable=qgis_exe,
                            startup_timeout_seconds=0.1,
                            startup_poll_interval_seconds=0.001,
                        )
                        controller._manifest_processes[manifest.service_id] = old_process

                        def fake_terminate(pid, force=False):
                            terminated.append((pid, force))
                            processes[pid].returncode = 0

                        process_controller.subprocess.Popen = (
                            lambda *args, **kwargs: new_process
                        )
                        process_controller.find_available_port = lambda host, port: port
                        process_controller.process_identity = lambda pid: {
                            "pid": str(pid),
                            "creation_date": f"created-{pid}",
                        }
                        process_controller.is_process_running = lambda pid: (
                            pid in processes and processes[pid].returncode is None
                        )
                        process_controller.process_tree_pids = lambda pid: [pid]
                        process_controller.terminate_process_tree = fake_terminate
                        process_controller.ProcessController._health_ok = (
                            lambda self, host, port: True
                        )
                        process_controller.service_log_file = (
                            lambda service_id: root / "logs" / f"{case_name}.log"
                        )

                        status = controller.start(manifest)

                        self.assertTrue(status.running)
                        self.assertEqual(status.health, "ok")
                        self.assertGreater(old_process.poll_calls, 0)
                        self.assertIs(
                            controller._manifest_processes[manifest.service_id],
                            new_process,
                        )
                        if case_name == "exited":
                            self.assertEqual(old_process.returncode, 17)
                            self.assertEqual(terminated, [])
                        else:
                            self.assertEqual(old_process.returncode, 0)
                            self.assertEqual(terminated, [(old_process.pid, False)])
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.find_available_port = original_find_available_port
            process_controller.process_identity = original_process_identity
            process_controller.is_process_running = original_is_process_running
            process_controller.process_tree_pids = original_process_tree_pids
            process_controller.terminate_process_tree = original_terminate_process_tree
            process_controller.ProcessController._health_ok = original_health_ok
            process_controller.service_log_file = original_service_log_file

    def test_manifest_controller_trusts_matching_live_handle_when_pid_probe_fails(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        class FakeProcess:
            pid = 612
            returncode = None

            def poll(self):
                return self.returncode

        class FakeRuntime:
            def service_command(self, manifest, python_executable):
                del manifest
                return [str(python_executable), "-c", "print('must not launch')"]

        original_popen = process_controller.subprocess.Popen
        original_is_process_running = process_controller.is_process_running
        original_process_tree_pids = process_controller.process_tree_pids
        original_health_ok = process_controller.ProcessController._health_ok

        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                plugin_dir = root / "plugin"
                plugin_dir.mkdir()
                manifest = ServiceManifest(
                    service_id="qcopilots.live_handle",
                    display_name="QCopilots Live Handle",
                    description="Matching live process handle test service.",
                    plugin_name="qcopilots_live_handle",
                    plugin_dir=plugin_dir,
                    manifest_path=plugin_dir / "service.json",
                    transport=ServiceTransport(
                        host="127.0.0.1",
                        port=49531,
                        path="/mcp",
                    ),
                )
                qgis_exe, _ = _write_qgis_package_layout(root / "QGIS40200-RelWithDebInfo")
                controller = ProcessController(
                    root / "state",
                    runtime=FakeRuntime(),
                    qgis_executable=qgis_exe,
                )
                fake_process = FakeProcess()
                controller.process = fake_process
                controller._manifest_processes[manifest.service_id] = fake_process
                state_path = root / "state" / "qcopilots_live_handle.json"
                state_path.write_text(
                    json.dumps(
                        {
                            "pid": fake_process.pid,
                            "port": manifest.default_port,
                            "owner_token": controller.owner_token,
                        }
                    ),
                    encoding="utf-8",
                )

                def fail_popen(*args, **kwargs):
                    del args, kwargs
                    raise AssertionError("A matching live process handle must not be replaced")

                process_controller.subprocess.Popen = fail_popen
                process_controller.is_process_running = lambda pid: False
                process_controller.process_tree_pids = lambda pid: [pid]
                process_controller.ProcessController._health_ok = (
                    lambda self, host, port: True
                )

                status = controller.start(manifest)

                self.assertTrue(status.running)
                self.assertEqual(status.health, "ok")
                self.assertEqual(status.pid, fake_process.pid)
                self.assertIs(
                    controller._manifest_processes[manifest.service_id],
                    fake_process,
                )
        finally:
            process_controller.subprocess.Popen = original_popen
            process_controller.is_process_running = original_is_process_running
            process_controller.process_tree_pids = original_process_tree_pids
            process_controller.ProcessController._health_ok = original_health_ok

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
                    self.assertEqual(state["manifest"]["advertised_host"], "")
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
            wait_calls = []

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                self.wait_calls.append(timeout)
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
                self.assertEqual(controller.exit_code, 0)
                self.assertEqual(controller.process.wait_calls, [1])
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.terminate_process_tree = original_terminate_process_tree

    def test_stop_keeps_running_state_when_process_tree_does_not_exit(self):
        import json

        import qcopilots_common.process_controller as process_controller
        from qcopilots_common.manifest import ServiceManifest, ServiceTransport
        from qcopilots_common.process_controller import ProcessController

        original_is_process_running = process_controller.is_process_running
        original_process_matches_state = process_controller.process_matches_state
        original_process_tree_pids = process_controller.process_tree_pids
        original_terminate_process_tree = process_controller.terminate_process_tree
        termination_calls = []

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
                        host="127.0.0.1",
                        port=49531,
                        path="/mcp",
                    ),
                )
                controller = ProcessController(root / "state")
                state_path = root / "state" / "qcopilots_dummy_service.json"
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

                process_controller.is_process_running = lambda pid: pid in (123, 124)
                process_controller.process_matches_state = lambda pid, state: True
                process_controller.process_tree_pids = lambda pid: [123, 124]

                def fake_terminate(pid, force=False, timeout_seconds=30):
                    termination_calls.append((pid, force, timeout_seconds))
                    return False

                process_controller.terminate_process_tree = fake_terminate

                with self.assertRaisesRegex(RuntimeError, "did not stop"):
                    controller.stop(manifest, timeout_seconds=0.01)

                state = json.loads(state_path.read_text(encoding="utf-8"))
                self.assertEqual(state["pid"], 123)
                self.assertNotIn("stopped_at", state)
                self.assertEqual(
                    termination_calls,
                    [
                        (123, False, 0.01),
                        (123, True, 0.01),
                        (124, True, 0.01),
                    ],
                )
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.process_matches_state = original_process_matches_state
            process_controller.process_tree_pids = original_process_tree_pids
            process_controller.terminate_process_tree = original_terminate_process_tree

    @unittest.skipUnless(os.name == "nt", "Windows taskkill is platform-specific")
    def test_windows_taskkill_failure_is_reported(self):
        import qcopilots_common.process_controller as process_controller

        original_is_process_running = process_controller.is_process_running
        original_run = process_controller.subprocess.run
        calls = []
        try:
            process_controller.is_process_running = lambda pid: pid == 123

            def fake_run(command, **kwargs):
                calls.append((command, kwargs))
                return types.SimpleNamespace(returncode=5)

            process_controller.subprocess.run = fake_run

            self.assertFalse(
                process_controller.terminate_process_tree(
                    123,
                    force=True,
                    timeout_seconds=0.25,
                )
            )
            self.assertEqual(calls[0][0], ["taskkill", "/PID", "123", "/T", "/F"])
            self.assertEqual(calls[0][1]["timeout"], 0.25)
        finally:
            process_controller.is_process_running = original_is_process_running
            process_controller.subprocess.run = original_run


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
