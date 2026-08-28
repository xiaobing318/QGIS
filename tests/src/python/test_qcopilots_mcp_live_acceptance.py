"""Focused tests for the Launcher-based QCopilots live acceptance runner.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import json
import shutil
import tempfile
import traceback
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.error import URLError

import qcopilots_mcp_live_acceptance as live


SYNTHETIC_TOKEN = "A" * 43


class _FakeResponse:
    def __init__(self, status, body=b"", headers=None):
        self.status = status
        self._body = body
        self.headers = headers or {}

    def read(self, *args):
        del args
        return self._body

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, traceback):
        del exc_type, exc, traceback
        return False


class _ProtocolOpener:
    def __init__(
        self,
        *,
        tool_result=None,
        rpc_error=None,
        protocol_version=live.MCP_PROTOCOL_VERSION,
    ):
        self.tool_result = tool_result or {
            "content": [{"type": "text", "text": "expected policy rejection"}],
            "isError": True,
        }
        self.rpc_error = rpc_error
        self.protocol_version = protocol_version
        self.methods = []
        self.saw_authorization = False
        self.saw_session_header = False

    def __call__(self, request, timeout):
        self.assert_safe_timeout(timeout)
        self.saw_authorization = bool(request.headers.get("Authorization"))
        if request.get_method() == "DELETE":
            self.methods.append("DELETE")
            self.saw_session_header = bool(
                request.headers.get("Mcp-session-id")
                or request.headers.get("MCP-Session-Id")
            )
            return _FakeResponse(204)

        payload = json.loads(request.data.decode("utf-8"))
        method = payload["method"]
        self.methods.append(method)
        if method == "initialize":
            return self._rpc_response(
                payload,
                {"protocolVersion": self.protocol_version},
                headers={"Mcp-Session-Id": "synthetic-session"},
            )
        if method == "notifications/initialized":
            self.saw_session_header = bool(
                request.headers.get("Mcp-session-id")
                or request.headers.get("MCP-Session-Id")
            )
            return _FakeResponse(202)
        if method == "tools/list":
            return self._rpc_response(
                payload,
                {"tools": [{"name": "synthetic_tool", "inputSchema": {}}]},
            )
        if method == "tools/call":
            if self.rpc_error is not None:
                body = {
                    "jsonrpc": "2.0",
                    "id": payload["id"],
                    "error": self.rpc_error,
                }
                return _FakeResponse(200, json.dumps(body).encode("utf-8"))
            return self._rpc_response(payload, self.tool_result)
        raise AssertionError(f"Unexpected protocol method: {method}")

    @staticmethod
    def assert_safe_timeout(timeout):
        if timeout <= 0:
            raise AssertionError("timeout must be positive")

    @staticmethod
    def _rpc_response(payload, result, headers=None):
        body = {
            "jsonrpc": "2.0",
            "id": payload["id"],
            "result": result,
        }
        return _FakeResponse(
            200,
            json.dumps(body).encode("utf-8"),
            headers=headers,
        )


class _FailingOpener:
    def __call__(self, request, timeout):
        del request, timeout
        raise URLError(f"transport included secret {SYNTHETIC_TOKEN}")


class _FlakyDeleteOpener(_ProtocolOpener):
    def __init__(self):
        super().__init__()
        self.delete_attempts = 0

    def __call__(self, request, timeout):
        if request.get_method() == "DELETE":
            self.methods.append("DELETE")
            self.delete_attempts += 1
            if self.delete_attempts < 3:
                raise URLError("synthetic transient delete failure")
            return _FakeResponse(204)
        return super().__call__(request, timeout)


class _FakeSkillsSession:
    service = live.SERVICES[1]

    def __init__(self):
        self.called = []

    def call_tool(self, name, arguments):
        self.called.append((name, arguments))
        payloads = {
            "list_skills": {
                "skills": [{"slug": "qgis-skills-creator"}],
            },
            "read_skill": {
                "slug": "qgis-skills-creator",
                "content": "real instructions",
                "skill_content": "formatted instructions",
            },
            "list_skill_resources": {
                "resources": ["references/qgis-tools.catalog.schema.json"],
            },
            "read_skill_resource": {
                "resource": "references/qgis-tools.catalog.schema.json",
                "content": "{}",
            },
            "qgis-skills-creator": {
                "skill": "qgis-skills-creator",
                "skill_content": "formatted instructions",
            },
        }
        return {"structuredContent": payloads[name], "isError": False}


class _FakeCrsSession:
    service = live.SERVICES[2]

    def __init__(self, auth_id):
        self.auth_id = auth_id
        self.calls = []

    def call_tool(self, name, arguments):
        self.calls.append((name, arguments))
        return {
            "structuredContent": {
                "crs": {"auth_id": self.auth_id, "wkt": "synthetic WKT"}
            },
            "isError": False,
        }


class _FakeDisposableProjectSession:
    service = live.SERVICES[2]

    def __init__(self):
        self.calls = []
        self.layout_name = ""

    def call_tool(self, name, arguments):
        self.calls.append((name, arguments))
        if name == "create_print_layout":
            self.layout_name = arguments["name"]
            payload = {
                "created": True,
                "layout": {"name": self.layout_name, "page_count": 1},
            }
        elif name == "list_print_layouts":
            payload = {
                "layouts": [{"name": self.layout_name, "page_count": 1}],
            }
        elif name == "export_print_layout":
            path = Path(arguments["path"])
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"%PDF-1.4\nsynthetic\n")
            payload = {
                "saved": True,
                "path": str(path),
                "cleanup": {"complete": True},
            }
        elif name == "save_project":
            path = Path(arguments["path"])
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"synthetic qgz")
            payload = {
                "saved": True,
                "path": str(path),
                "cleanup": {"complete": True},
            }
        else:
            raise AssertionError(f"Unexpected disposable project call: {name}")
        return {"structuredContent": payload, "isError": False}


class _FakePendingJobSession:
    service = live.SERVICES[3]

    def call_tool(self, name, arguments):
        if name != "list_vector_processing_jobs" or arguments != {"limit": 200}:
            raise AssertionError("Unexpected pending-job reconciliation call")
        return {
            "structuredContent": {
                "jobs": [
                    {
                        "job_id": "recovered-job",
                        "client_request_id": "pending-request",
                    }
                ]
            },
            "isError": False,
        }


class TestQCopilotsMcpLiveAcceptance(unittest.TestCase):
    def test_inventory_is_exactly_seventy_one_tools_on_seven_ports(self):
        live.validate_expected_inventory()
        self.assertEqual(
            [service.port for service in live.SERVICES],
            list(range(48211, 48218)),
        )
        names = [
            name
            for service in live.SERVICES
            for name in live.EXPECTED_TOOLS[service.key]
        ]
        self.assertEqual(len(names), 71)
        self.assertEqual(len(set(names)), 71)
        self.assertEqual(
            [len(live.EXPECTED_TOOLS[service.key]) for service in live.SERVICES],
            [9, 5, 33, 6, 6, 6, 6],
        )

    def test_manager_config_path_and_token_loading_are_secret_safe(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            config = root / live.MANAGER_CONFIG_FILENAME
            config.write_text(
                json.dumps(
                    {
                        "browser_access": {"auth_token": SYNTHETIC_TOKEN},
                        "security_policy": {
                            "mode": "formal_restricted",
                            "shell": {"enabled": False},
                        },
                    }
                ),
                encoding="utf-8",
            )
            self.assertEqual(
                live.default_manager_config_path(
                    {"QCOPILOTS_HOME": str(root), "APPDATA": "ignored"}
                ),
                config.resolve(),
            )
            loaded_config = live.load_manager_acceptance_config(config)
            loaded = loaded_config.auth_token
            self.assertTrue(loaded == SYNTHETIC_TOKEN)
            self.assertEqual(loaded_config.security_mode, "formal_restricted")
            self.assertFalse(loaded_config.shell_enabled)
            session = live.McpSession(
                live.ServiceSpec("synthetic", "synthetic.service", 49999),
                loaded,
                timeout_seconds=1,
                opener=_FailingOpener(),
            )
            self.assertNotIn(SYNTHETIC_TOKEN, repr(session))
            with self.assertRaises(live.AcceptanceFailure) as caught:
                session.start()
            self.assertNotIn(SYNTHETIC_TOKEN, str(caught.exception))
            self.assertIn("<redacted>", str(caught.exception))
            formatted = "".join(
                traceback.format_exception(
                    type(caught.exception),
                    caught.exception,
                    caught.exception.__traceback__,
                )
            )
            self.assertNotIn(SYNTHETIC_TOKEN, formatted)
            self.assertIsNone(caught.exception.__cause__)
            self.assertIsNone(caught.exception.__context__)

    def test_mcp_session_runs_complete_lifecycle_and_business_rejection(self):
        opener = _ProtocolOpener()
        service = live.ServiceSpec("synthetic", "synthetic.service", 49999)
        with live.McpSession(
            service,
            SYNTHETIC_TOKEN,
            timeout_seconds=1,
            opener=opener,
        ) as session:
            tools = session.list_tools()
            self.assertEqual([tool["name"] for tool in tools], ["synthetic_tool"])
            result = session.call_tool("synthetic_tool", {})
            message = live.require_business_error(
                result,
                "synthetic_tool",
                ("policy rejection",),
            )
            self.assertIn("policy rejection", message)
        self.assertEqual(
            opener.methods,
            [
                "initialize",
                "notifications/initialized",
                "tools/list",
                "tools/call",
                "DELETE",
            ],
        )
        self.assertTrue(opener.saw_authorization)
        self.assertTrue(opener.saw_session_header)

    def test_json_rpc_error_cannot_be_accepted_as_business_rejection(self):
        opener = _ProtocolOpener(
            rpc_error={"code": -32000, "message": "permission rejected"}
        )
        service = live.ServiceSpec("synthetic", "synthetic.service", 49999)
        with live.McpSession(
            service,
            SYNTHETIC_TOKEN,
            timeout_seconds=1,
            opener=opener,
        ) as session:
            session.list_tools()
            with self.assertRaisesRegex(
                live.AcceptanceFailure,
                "JSON-RPC error",
            ):
                session.call_tool("synthetic_tool", {})

    def test_partial_initialize_session_is_deleted_on_protocol_failure(self):
        opener = _ProtocolOpener(protocol_version="unexpected-version")
        session = live.McpSession(
            live.ServiceSpec("synthetic", "synthetic.service", 49999),
            SYNTHETIC_TOKEN,
            timeout_seconds=1,
            opener=opener,
        )
        with self.assertRaisesRegex(
            live.AcceptanceFailure,
            "unexpected protocol version",
        ):
            session.start()
        self.assertEqual(opener.methods, ["initialize", "DELETE"])
        self.assertEqual(session.session_id, "")

    def test_session_delete_retries_transient_failures(self):
        opener = _FlakyDeleteOpener()
        session = live.McpSession(
            live.ServiceSpec("synthetic", "synthetic.service", 49999),
            SYNTHETIC_TOKEN,
            timeout_seconds=1,
            opener=opener,
        )
        session.start()
        session.list_tools()
        session.close()
        self.assertEqual(opener.delete_attempts, 3)
        self.assertEqual(session.session_id, "")

    def test_skills_scenario_uses_real_public_result_keys(self):
        runner = live.AcceptanceRunner(SYNTHETIC_TOKEN, output=lambda message: None)
        session = _FakeSkillsSession()
        try:
            runner._exercise_skills(session)
            self.assertEqual(
                [name for name, arguments in session.called],
                list(live.EXPECTED_TOOLS[session.service.key]),
            )
        finally:
            errors = runner.cleanup.cleanup_filesystem()
        self.assertEqual(errors, [])

    def test_project_state_uses_typed_restore_and_process_exit_boundaries(self):
        runner = live.AcceptanceRunner(SYNTHETIC_TOKEN, output=lambda message: None)
        session = _FakeCrsSession("EPSG:4326")
        try:
            runner.cleanup.register_crs_restore(
                "EPSG:4326",
                "auth:EPSG:4326",
            )
            runner._restore_project_crs(session, with_coverage=False)
            self.assertEqual(
                session.calls,
                [("set_project_crs", {"crs": "EPSG:4326"})],
            )
            self.assertEqual(runner.cleanup.original_crs_selector, "")
            self.assertEqual(runner.cleanup.original_crs_identity, "")

            disposable_session = _FakeDisposableProjectSession()
            runner._exercise_disposable_project_outputs(disposable_session)
            self.assertEqual(
                [name for name, arguments in disposable_session.calls],
                [
                    "create_print_layout",
                    "list_print_layouts",
                    "export_print_layout",
                    "save_project",
                ],
            )
            self.assertEqual(
                set(runner.cleanup.requires_process_exit),
                {"print_layout", "project_filename"},
            )
            self.assertEqual(len(runner.cleanup.requires_process_exit), 2)
        finally:
            errors = runner.cleanup.cleanup_filesystem()
        self.assertEqual(errors, [])

    def test_pending_job_is_recovered_by_client_request_id(self):
        registration = live.JobRegistration(
            service=live.SERVICES[3],
            get_tool="get_vector_processing_job",
            cancel_tool="cancel_vector_processing_job",
            list_tool="list_vector_processing_jobs",
            client_request_id="pending-request",
        )
        recovered = live.AcceptanceRunner._resolve_pending_job(
            _FakePendingJobSession(),
            registration,
        )
        self.assertTrue(recovered)
        self.assertEqual(registration.job_id, "recovered-job")

    def test_default_loopback_opener_disables_proxy_and_redirects(self):
        proxy_handlers = [
            handler
            for handler in live._LOOPBACK_HTTP_OPENER.handlers
            if isinstance(handler, live.ProxyHandler)
        ]
        self.assertEqual(proxy_handlers, [])
        self.assertTrue(
            any(
                isinstance(handler, live._RejectRedirectHandler)
                for handler in live._LOOPBACK_HTTP_OPENER.handlers
            )
        )

    def test_cleanup_registry_detects_and_removes_every_registered_path(self):
        registry = live.CleanupRegistry.create()
        file_path = registry.reserve_path("nested/fixture.txt")
        file_path.parent.mkdir(parents=True)
        file_path.write_text("fixture", encoding="utf-8")
        self.assertEqual(registry.cleanup_filesystem(), [])
        self.assertFalse(registry.root.exists())
        self.assertEqual(registry.cleanup_filesystem(), [])

    def test_registered_binary_output_prepares_its_parent_directory(self):
        registry = live.CleanupRegistry.create()
        output_path = registry.reserve_output_path("binary/output.tif")
        try:
            self.assertTrue(output_path.parent.is_dir())
            self.assertTrue(output_path.is_relative_to(registry.root))
        finally:
            errors = registry.cleanup_filesystem()
        self.assertEqual(errors, [])

    def test_cleanup_registry_uses_workspace_base_and_preserves_it(self):
        with tempfile.TemporaryDirectory() as temporary:
            workspace_base = Path(temporary) / "workspace"
            workspace_base.mkdir()
            first = live.CleanupRegistry.create(workspace_base)
            second = live.CleanupRegistry.create(workspace_base)
            try:
                self.assertEqual(first.workspace_base, workspace_base.resolve())
                self.assertEqual(first.root.parent, workspace_base.resolve())
                self.assertEqual(second.root.parent, workspace_base.resolve())
                self.assertNotEqual(first.root, second.root)
                first.reserve_output_path("nested/first.txt").write_text(
                    "first",
                    encoding="utf-8",
                )
                second.reserve_output_path("nested/second.txt").write_text(
                    "second",
                    encoding="utf-8",
                )
            finally:
                first_errors = first.cleanup_filesystem()
                second_errors = second.cleanup_filesystem()
            self.assertEqual(first_errors, [])
            self.assertEqual(second_errors, [])
            self.assertTrue(workspace_base.is_dir())
            self.assertEqual(list(workspace_base.iterdir()), [])

    def test_runner_failure_cleans_workspace_run_but_preserves_base(self):
        with tempfile.TemporaryDirectory() as temporary:
            workspace_base = Path(temporary) / "workspace"
            workspace_base.mkdir()
            runner = live.AcceptanceRunner(
                SYNTHETIC_TOKEN,
                workspace_base=workspace_base,
                output=lambda message: None,
            )
            run_root = runner.cleanup.root
            runner.cleanup.reserve_output_path("fixture.txt").write_text(
                "fixture",
                encoding="utf-8",
            )
            with patch.object(
                runner,
                "_exercise_service",
                side_effect=RuntimeError("synthetic acceptance failure"),
            ):
                with self.assertRaisesRegex(
                    live.AcceptanceFailure,
                    "synthetic acceptance failure",
                ):
                    runner.run()
            self.assertFalse(run_root.exists())
            self.assertTrue(workspace_base.is_dir())
            self.assertEqual(list(workspace_base.iterdir()), [])

    def test_cleanup_registry_rejects_invalid_workspace_paths_and_escape(self):
        with self.assertRaisesRegex(live.AcceptanceFailure, "absolute path"):
            live.CleanupRegistry.create("relative-workspace")
        with self.assertRaisesRegex(live.AcceptanceFailure, "absolute paths"):
            live.CleanupRegistry(Path("relative-run"), Path("relative-workspace"))

        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            workspace_file = parent / "workspace.txt"
            workspace_file.write_text("not a directory", encoding="utf-8")
            with self.assertRaisesRegex(live.AcceptanceFailure, "directory"):
                live.CleanupRegistry.create(workspace_file)
            with self.assertRaisesRegex(live.AcceptanceFailure, "does not exist"):
                live.CleanupRegistry.create(parent / "missing")

            workspace_base = parent / "workspace"
            workspace_base.mkdir()
            with self.assertRaisesRegex(live.AcceptanceFailure, "direct child"):
                live.CleanupRegistry(workspace_base, workspace_base)
            unowned_child = workspace_base / "unowned-child"
            unowned_child.mkdir()
            with self.assertRaisesRegex(live.AcceptanceFailure, "owned direct child"):
                live.CleanupRegistry(unowned_child, workspace_base)

            foreign_root = workspace_base / (
                live.CleanupRegistry.RUN_ROOT_PREFIX + "foreign"
            )
            foreign_root.mkdir()
            foreign_sentinel = foreign_root / "sentinel.txt"
            foreign_sentinel.write_text("preserve", encoding="utf-8")
            with self.assertRaisesRegex(
                live.AcceptanceFailure,
                "created through CleanupRegistry.create",
            ):
                live.CleanupRegistry(foreign_root, workspace_base)
            self.assertEqual(
                foreign_sentinel.read_text(encoding="utf-8"),
                "preserve",
            )

            registry = live.CleanupRegistry.create(workspace_base)
            try:
                with self.assertRaisesRegex(live.AcceptanceFailure, "escapes"):
                    registry.reserve_path("../outside.txt")
            finally:
                errors = registry.cleanup_filesystem()
            self.assertEqual(errors, [])
            self.assertTrue(workspace_base.is_dir())

    def test_cleanup_registry_refuses_tampered_owner_marker(self):
        with tempfile.TemporaryDirectory() as temporary:
            workspace_base = Path(temporary) / "workspace"
            workspace_base.mkdir()
            registry = live.CleanupRegistry.create(workspace_base)
            sentinel = registry.reserve_output_path("sentinel.txt")
            sentinel.write_text("preserve", encoding="utf-8")
            owner_marker = registry.root / live.CleanupRegistry.OWNER_MARKER_NAME
            owner_marker.write_text("tampered", encoding="utf-8")

            errors = registry.cleanup_filesystem()

            self.assertEqual(len(errors), 1)
            self.assertIn("ownership validation failed", errors[0])
            self.assertTrue(registry.root.is_dir())
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserve")

    def test_cleanup_registry_compensates_for_owner_marker_creation_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            workspace_base = Path(temporary) / "workspace"
            workspace_base.mkdir()
            with patch.object(
                Path,
                "open",
                side_effect=OSError("synthetic marker failure"),
            ):
                with self.assertRaisesRegex(
                    live.AcceptanceFailure,
                    "ownership could not be established",
                ):
                    live.CleanupRegistry.create(workspace_base)

            self.assertEqual(list(workspace_base.iterdir()), [])

    def test_main_passes_workspace_base_to_acceptance_runner(self):
        with tempfile.TemporaryDirectory() as temporary:
            workspace_base = str(Path(temporary).resolve())
            configuration = live.ManagerAcceptanceConfig(
                auth_token=SYNTHETIC_TOKEN,
                security_mode="formal_restricted",
                shell_enabled=False,
            )
            with patch.object(
                live,
                "load_manager_acceptance_config",
                return_value=configuration,
            ), patch.object(live, "AcceptanceRunner") as runner_class:
                result = live.main(["--workspace-base", workspace_base])
            self.assertEqual(result, 0)
            runner_class.assert_called_once_with(
                SYNTHETIC_TOKEN,
                request_timeout_seconds=60.0,
                job_timeout_seconds=120.0,
                security_mode="formal_restricted",
                shell_enabled=False,
                workspace_base=workspace_base,
            )
            runner_class.return_value.run.assert_called_once_with()

    def test_general_processing_scenario_packages_the_registered_vector(self):
        runner = live.AcceptanceRunner(
            SYNTHETIC_TOKEN,
            output=lambda message: None,
        )
        try:
            algorithm_id, parameters, output_path = runner._processing_scenario(
                "general"
            )
            self.assertEqual(algorithm_id, "native:package")
            self.assertEqual(
                parameters["LAYERS"],
                [str(runner.cleanup.root / "interactive" / "acceptance.geojson")],
            )
            self.assertEqual(parameters["OUTPUT"], str(output_path))
            self.assertEqual(output_path.suffix, ".gpkg")
        finally:
            errors = runner.cleanup.cleanup_filesystem()
        self.assertEqual(errors, [])

    def test_cleanup_registry_reports_incomplete_cleanup(self):
        registry = live.CleanupRegistry.create()
        file_path = registry.reserve_path("fixture.txt")
        file_path.write_text("fixture", encoding="utf-8")
        try:
            with patch.object(live.shutil, "rmtree", return_value=None):
                errors = registry.cleanup_filesystem()
            self.assertTrue(errors)
            self.assertTrue(registry.root.exists())
        finally:
            shutil.rmtree(registry.root, ignore_errors=True)

    def test_coverage_ledger_requires_every_public_tool(self):
        ledger = live.CoverageLedger()
        with self.assertRaises(live.AcceptanceFailure):
            ledger.assert_all_complete()
        for service in live.SERVICES:
            for name in live.EXPECTED_TOOLS[service.key]:
                ledger.mark(service.key, name)
        ledger.assert_all_complete()


if __name__ == "__main__":
    unittest.main()
