"""QGIS unit tests for QCopilots skill discovery.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import base64
import builtins
import hashlib
import json
import os
import shutil
import tempfile
import threading
import unittest
import warnings
import zipfile
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
from pathlib import Path
from unittest import mock


class TestQCopilotsMcpServerSkills(unittest.TestCase):
    def setUp(self):
        super().setUp()
        original_tempdir = tempfile.tempdir
        self.addCleanup(setattr, tempfile, "tempdir", original_tempdir)
        tempfile.tempdir = str(Path(tempfile.gettempdir()).resolve())

        environment = mock.patch.dict(
            os.environ,
            {
                "QCOPILOTS_SKILLS_ROOTS": "",
                "CODEX_HOME": "",
            },
        )
        environment.start()
        self.addCleanup(environment.stop)

    def _write_skill(
        self,
        root,
        name="qgis-example",
        description="Expose QGIS sample behaviour",
        body="# QGIS Example\n\nUse this skill for unit tests.\n",
        resources=None,
        allowed_tools="read_file",
        metadata=None,
        compatibility=None,
    ):
        skill_dir = Path(root) / name
        skill_dir.mkdir(parents=True)
        metadata = metadata or {"owner": "qcopilots-tests"}
        front_matter = [
            "---",
            f"name: {name}",
            f"description: {description}",
        ]
        if compatibility:
            front_matter.append(f"compatibility: {compatibility}")
        if isinstance(allowed_tools, str):
            front_matter.append(f"allowed-tools: {allowed_tools}")
        elif allowed_tools:
            front_matter.append("allowed-tools:")
            front_matter.extend(f"  - {tool}" for tool in allowed_tools)
        if metadata:
            front_matter.append("metadata:")
            front_matter.extend(f"  {key}: {value}" for key, value in metadata.items())
        front_matter.extend(["---", body])
        (skill_dir / "SKILL.md").write_text(
            "\n".join(front_matter),
            encoding="utf-8",
            newline="\n",
        )
        for rel_path, content in (resources or {}).items():
            path = skill_dir / rel_path
            path.parent.mkdir(parents=True, exist_ok=True)
            if isinstance(content, bytes):
                path.write_bytes(content)
            else:
                path.write_text(content, encoding="utf-8", newline="\n")
        return skill_dir

    def _write_raw_skill(self, root, name, content):
        skill_dir = Path(root) / name
        skill_dir.mkdir(parents=True)
        (skill_dir / "SKILL.md").write_text(
            content,
            encoding="utf-8",
            newline="\n",
        )
        return skill_dir

    def _canonical_document(
        self,
        name,
        description="Canonical skill",
        extra=(),
        body="# Body\n",
    ):
        return "\n".join(
            [
                "---",
                f"name: {name}",
                f"description: {description}",
                *extra,
                "---",
                body,
            ]
        )

    def _diagnostic_codes(self, registry):
        return {diagnostic.code for diagnostic in registry.diagnostics}

    def _assert_diagnostic(
        self,
        registry,
        *,
        code,
        source,
        message,
        severity="error",
        exact_message=False,
    ):
        matches = [
            diagnostic
            for diagnostic in registry.diagnostics
            if diagnostic.code == code and diagnostic.source == str(source)
        ]
        self.assertEqual(len(matches), 1, registry.diagnostics)
        diagnostic = matches[0]
        self.assertEqual(diagnostic.severity, severity)
        if exact_message:
            self.assertEqual(diagnostic.message, message)
        else:
            self.assertIn(message, diagnostic.message)
        self.assertEqual(
            set(diagnostic.as_dict()),
            {"code", "severity", "source", "message"},
        )
        return diagnostic

    def test_skill_repository_discovers_skill_metadata_and_resources(self):
        from qcopilots_common.skills import SkillRepository

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            skill_dir = self._write_skill(
                root / "Skills",
                resources={
                    "references/details.md": "# Details\n\nOnly local resources are readable.\n",
                },
            )

            repository = SkillRepository([root / "Skills"])
            skills = repository.discover_skills()

            self.assertEqual([skill.name for skill in skills], ["qgis-example"])
            self.assertEqual(
                skills[0].description,
                "Expose QGIS sample behaviour",
            )
            self.assertEqual(skills[0].root_path, skill_dir.resolve())
            self.assertEqual(skills[0].skill_path, (skill_dir / "SKILL.md").resolve())

            skill = repository.get_skill("qgis-example")
            self.assertEqual(skill.body.lstrip().splitlines()[0], "# QGIS Example")
            self.assertEqual(
                repository.read_resource("qgis-example", "references/details.md"),
                "# Details\n\nOnly local resources are readable.\n",
            )

            with self.assertRaises(PermissionError):
                repository.read_resource("qgis-example", "../outside.md")

    def test_repository_ignores_directories_without_skill_file(self):
        from qcopilots_common.skills import SkillRepository

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "Skills" / "not-a-skill").mkdir(parents=True)

            repository = SkillRepository([root / "Skills"])
            self.assertEqual(repository.discover_skills(), [])
            with self.assertRaises(KeyError):
                repository.get_skill("not-a-skill")

    def test_missing_configured_skill_root_fails_initial_load(self):
        from qcopilots_common.skills import SkillError, SkillRegistry, SkillService

        with tempfile.TemporaryDirectory() as tmp:
            missing_root = Path(tmp) / "missing-skills"
            registry = SkillRegistry([missing_root])

            with self.assertRaises(SkillError) as registry_error:
                registry.load()
            self.assertIn("could not be inspected", str(registry_error.exception))
            self._assert_diagnostic(
                registry,
                code="skills.root.invalid",
                source=missing_root.resolve(),
                message="could not be inspected",
            )

            with self.assertRaises(SkillError) as service_error:
                SkillService([missing_root])
            self.assertIn("Skills root", str(service_error.exception))
            self.assertIn(str(missing_root.resolve()), str(service_error.exception))

    def test_service_exposes_skillz_style_tools_resources_and_prompts(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(
                root,
                body="# QGIS Example\n\nUse this skill through MCP.\n",
                resources={
                    "references/details.md": "# Details\n\nOnly local resources are readable.\n",
                    "assets/data.bin": b"\xff\x00\x01",
                },
            )
            service = SkillService([root], refresh_interval_seconds=0)
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
                prompts=service.prompts,
            )

            initialize = server.handle_json_rpc(
                {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}}
            )
            self.assertIn("resources", initialize["result"]["capabilities"])
            self.assertIn("prompts", initialize["result"]["capabilities"])
            self.assertNotIn(
                "listChanged",
                initialize["result"]["capabilities"]["prompts"],
            )

            tool_names = [
                tool["name"]
                for tool in server.handle_json_rpc(
                    {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}}
                )["result"]["tools"]
            ]
            self.assertIn("list_skills", tool_names)
            self.assertIn("read_skill_resource", tool_names)
            self.assertIn("qgis-example", tool_names)

            skills = self._call_tool(server, "list_skills", {})["skills"]
            self.assertEqual(
                skills,
                [
                    {
                        "slug": "qgis-example",
                        "name": "qgis-example",
                        "description": "Expose QGIS sample behaviour",
                        "conformance": "strict",
                    }
                ],
            )

            skill = self._call_tool(
                server,
                "qgis-example",
                {"task": "Use the QGIS example skill"},
            )
            self.assertEqual(skill["skill"], "qgis-example")
            self.assertEqual(skill["metadata"]["allowed_tools"], ["read_file"])
            self.assertEqual(
                skill["metadata"]["metadata"],
                {"owner": "qcopilots-tests"},
            )
            self.assertIn("# QGIS Example", skill["instructions"])
            self.assertEqual(
                skill["resources"][0]["uri"],
                "resource://skillz/qgis-example/assets/data.bin",
            )

            resources = server.handle_json_rpc(
                {"jsonrpc": "2.0", "id": 3, "method": "resources/list", "params": {}}
            )["result"]["resources"]
            resource_uris = {resource["uri"] for resource in resources}
            self.assertIn(
                "resource://skillz/qgis-example/references/details.md",
                resource_uris,
            )

            details = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 4,
                    "method": "resources/read",
                    "params": {
                        "uri": "resource://skillz/qgis-example/references/details.md"
                    },
                }
            )["result"]["contents"][0]
            self.assertEqual(
                details["text"],
                "# Details\n\nOnly local resources are readable.\n",
            )
            binary = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 5,
                    "method": "resources/read",
                    "params": {"uri": "resource://skillz/qgis-example/assets/data.bin"},
                }
            )["result"]["contents"][0]
            self.assertEqual(
                binary["blob"],
                base64.b64encode(b"\xff\x00\x01").decode("ascii"),
            )

            prompts = server.handle_json_rpc(
                {"jsonrpc": "2.0", "id": 7, "method": "prompts/list", "params": {}}
            )["result"]["prompts"]
            self.assertEqual([prompt["name"] for prompt in prompts], ["qgis-example"])
            self.assertEqual(prompts[0]["arguments"][0]["name"], "task")

            prompt = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 8,
                    "method": "prompts/get",
                    "params": {
                        "name": "qgis-example",
                        "arguments": {"task": "Use the QGIS example skill"},
                    },
                }
            )["result"]
            prompt_text = prompt["messages"][0]["content"]["text"]
            self.assertIn('<skill_content name="qgis-example"', prompt_text)
            self.assertIn("<activation_task>", prompt_text)
            self.assertIn("Use the QGIS example skill", prompt_text)
            self.assertIn("<skill_metadata>", prompt_text)
            self.assertIn("description: Expose QGIS sample behaviour", prompt_text)
            self.assertIn("allowed_tools: read_file", prompt_text)
            self.assertIn("# QGIS Example", prompt_text)
            self.assertIn("<skill_resources>", prompt_text)
            self.assertIn("references/details.md", prompt_text)
            self.assertIn("resources/read", prompt_text)
            self.assertIn("read_skill_resource", prompt_text)

            compatible_text_resource = self._call_tool(
                server,
                "read_skill_resource",
                {"slug": "qgis-example", "resource": "references/details.md"},
            )
            self.assertEqual(compatible_text_resource["encoding"], "utf-8")
            self.assertEqual(
                compatible_text_resource["content"],
                "# Details\n\nOnly local resources are readable.\n",
            )
            self.assertIn(
                "allowed_tools: read_file",
                skill["skill_content"],
            )
            self.assertIn("resources/read", skill["skill_content"])
            compatible_binary_resource = self._call_tool(
                server,
                "read_skill_resource",
                {"slug": "qgis-example", "resource": "assets/data.bin"},
            )
            self.assertEqual(compatible_binary_resource["encoding"], "base64")
            self.assertEqual(compatible_binary_resource["blob"], binary["blob"])

            empty_task = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 6,
                    "method": "tools/call",
                    "params": {"name": "qgis-example", "arguments": {"task": "   "}},
                }
            )
            self.assertTrue(empty_task["result"]["isError"])

    def test_management_tool_descriptions_include_current_skill_catalog(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(root, name="alpha-skill")
            self._write_skill(root, name="beta-skill")

            service = SkillService([root], refresh_interval_seconds=0)
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
            )
            tools = self._tools_by_name(server)

            for tool_name in (
                "read_skill",
                "list_skill_resources",
                "read_skill_resource",
            ):
                with self.subTest(tool=tool_name):
                    description = tools[tool_name]["description"]
                    self.assertIn("Current skill slugs", description)
                    self.assertIn("alpha-skill", description)
                    self.assertIn("beta-skill", description)
                    self.assertNotIn(
                        '"enum"',
                        json.dumps(tools[tool_name]["inputSchema"]),
                    )
                    for selector in ("name", "slug", "path"):
                        self.assertEqual(
                            tools[tool_name]["inputSchema"]["properties"][
                                selector
                            ],
                            {"type": "string"},
                        )

            self.assertEqual(
                tools["read_skill_resource"]["inputSchema"]["required"],
                ["resource"],
            )

    def test_management_tool_catalog_note_is_bounded(self):
        import qcopilots_common.skills as skills_module
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            for index in range(25):
                self._write_skill(root, name=f"catalog-skill-{index:02d}")
            service = SkillService([root], refresh_interval_seconds=0)
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
            )
            read_skill_description = self._tools_by_name(server)["read_skill"][
                "description"
            ]
            self.assertIn("first 20 of 25", read_skill_description)
            self.assertIn("catalog-skill-00", read_skill_description)
            self.assertIn("catalog-skill-19", read_skill_description)
            self.assertNotIn("catalog-skill-20", read_skill_description)
            self.assertIn("Call list_skills for the full catalog", read_skill_description)

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            long_name = "long" + ("a" * 2048)
            self._write_raw_skill(
                root,
                "long-skill",
                self._canonical_document(
                    long_name,
                    description="Compatible long-name skill",
                ),
            )
            service = SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
            )
            description = self._tools_by_name(server)["read_skill"]["description"]
            base_description = (
                "Read a skill's instructions and metadata. After refreshing with "
                "list_skills, use a current or newly discovered slug. Scripts remain "
                "resources: run them from skill_root only when a user or host explicitly "
                "approves the execution tool. allowed-tools does not automatically grant "
                "permission."
            )
            self.assertLessEqual(
                len(description) - len(base_description),
                skills_module.MAX_SKILL_SELECTOR_CATALOG_CHARS,
            )

    def test_empty_skill_catalog_omits_management_tool_catalog_note(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir()
            service = SkillService([root], refresh_interval_seconds=0)
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
            )

            self.assertEqual(self._call_tool(server, "list_skills", {})["skills"], [])
            for tool_name in (
                "read_skill",
                "list_skill_resources",
                "read_skill_resource",
            ):
                with self.subTest(tool=tool_name):
                    descriptor = self._tools_by_name(server)[tool_name]
                    self.assertNotIn(
                        "Current skill slugs",
                        descriptor["description"],
                    )
                    self.assertNotIn(
                        '"enum"',
                        json.dumps(descriptor["inputSchema"]),
                    )

    def test_unknown_skill_selectors_are_tool_results_not_jsonrpc_errors(self):
        import qcopilots_common.skills as skills_module

        from qcopilots_common.mcp_http import McpJsonRpcServer, ToolError
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(root, name="known-skill")
            service = SkillService([root])
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
            )
            read_skill_tool = {
                tool.name: tool for tool in service.tools()
            }["read_skill"]
            selector_cases = (
                ("slug", {"slug": "missing-slug"}),
                ("name", {"name": "Missing Name"}),
                ("path", {"path": str(Path(tmp) / "outside-skill")}),
            )

            for offset, (selector, arguments) in enumerate(selector_cases, start=1):
                with self.subTest(selector=selector):
                    with self.assertRaisesRegex(
                        ToolError,
                        r"^The requested skill is unavailable\.$",
                    ):
                        read_skill_tool.handler(arguments)

                    response = server.handle_json_rpc(
                        {
                            "jsonrpc": "2.0",
                            "id": offset,
                            "method": "tools/call",
                            "params": {
                                "name": "read_skill",
                                "arguments": arguments,
                            },
                        }
                    )
                    self.assertNotIn("error", response)
                    self.assertEqual(response["jsonrpc"], "2.0")
                    self.assertEqual(response["id"], offset)
                    self.assertTrue(response["result"]["isError"])
                    self.assertEqual(
                        response["result"]["content"],
                        [
                            {
                                "type": "text",
                                "text": "The requested skill is unavailable.",
                            }
                        ],
                    )
                    self.assertNotIn("-32000", json.dumps(response, sort_keys=True))

            management_tool_cases = (
                (
                    "list_skill_resources",
                    {"slug": "missing-slug"},
                    skills_module._SKILL_UNAVAILABLE_MESSAGE,
                ),
                (
                    "read_skill_resource",
                    {"slug": "missing-slug", "resource": "references/missing.md"},
                    skills_module._SKILL_RESOURCE_UNAVAILABLE_MESSAGE,
                ),
            )
            for offset, (tool_name, arguments, expected_message) in enumerate(
                management_tool_cases,
                start=len(selector_cases) + 1,
            ):
                with self.subTest(tool=tool_name):
                    response = server.handle_json_rpc(
                        {
                            "jsonrpc": "2.0",
                            "id": offset,
                            "method": "tools/call",
                            "params": {
                                "name": tool_name,
                                "arguments": arguments,
                            },
                        }
                    )
                    self.assertNotIn("error", response)
                    self.assertEqual(response["jsonrpc"], "2.0")
                    self.assertEqual(response["id"], offset)
                    self.assertTrue(response["result"]["isError"])
                    self.assertEqual(
                        response["result"]["content"],
                        [{"type": "text", "text": expected_message}],
                    )
                    self.assertNotIn("-32000", json.dumps(response, sort_keys=True))

    def test_internal_key_errors_in_skill_tool_responses_are_jsonrpc_errors(self):
        import qcopilots_common.skills as skills_module
        from qcopilots_common.mcp_http import McpJsonRpcServer

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(root, name="known-skill")
            service = skills_module.SkillService([root])
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
            )
            response_cases = (
                (
                    "management-tool",
                    "read_skill",
                    {"slug": "known-skill"},
                    "management response construction failed",
                ),
                (
                    "dynamic-tool",
                    "known-skill",
                    {"task": "Use the known skill"},
                    "dynamic response construction failed",
                ),
            )

            for request_id, (
                case_name,
                tool_name,
                arguments,
                failure_message,
            ) in enumerate(response_cases, start=1):
                with self.subTest(case=case_name):
                    with mock.patch.object(
                        skills_module,
                        "_metadata_dict",
                        side_effect=KeyError(failure_message),
                    ):
                        response = server.handle_json_rpc(
                            {
                                "jsonrpc": "2.0",
                                "id": request_id,
                                "method": "tools/call",
                                "params": {
                                    "name": tool_name,
                                    "arguments": arguments,
                                },
                            }
                        )

                    self.assertEqual(
                        response,
                        {
                            "jsonrpc": "2.0",
                            "id": request_id,
                            "error": {
                                "code": -32000,
                                "message": repr(failure_message),
                            },
                        },
                    )
                    self.assertNotIn("result", response)
                    self.assertNotIn("isError", json.dumps(response, sort_keys=True))

    def test_read_skill_resource_rejects_invalid_paths_as_tool_errors(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(
                root,
                resources={"references/details.md": "# Details\n"},
            )
            service = SkillService([root])
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
            )

            private_sources = (
                root.resolve(),
                (root / "qgis-example").resolve(),
                (root / "qgis-example" / "SKILL.md").resolve(),
            )
            private_server_paths = tuple(
                path_form
                for source in private_sources
                for path_form in (str(source), source.as_posix())
            )
            invalid_resources = (
                (
                    "../PRIVATE-TRAVERSAL-PATH-MUST-NOT-LEAK.md",
                    "PRIVATE-TRAVERSAL-PATH-MUST-NOT-LEAK",
                ),
                (
                    "/PRIVATE-ABSOLUTE-PATH-MUST-NOT-LEAK.md",
                    "PRIVATE-ABSOLUTE-PATH-MUST-NOT-LEAK",
                ),
                (
                    "C:/PRIVATE-DRIVE-PATH-MUST-NOT-LEAK.md",
                    "PRIVATE-DRIVE-PATH-MUST-NOT-LEAK",
                ),
                (
                    "references\\PRIVATE-BACKSLASH-PATH-MUST-NOT-LEAK.md",
                    "PRIVATE-BACKSLASH-PATH-MUST-NOT-LEAK",
                ),
            )
            for request_id, (resource, private_marker) in enumerate(
                invalid_resources,
                start=1,
            ):
                response = server.handle_json_rpc(
                    {
                        "jsonrpc": "2.0",
                        "id": request_id,
                        "method": "tools/call",
                        "params": {
                            "name": "read_skill_resource",
                            "arguments": {
                                "slug": "qgis-example",
                                "resource": resource,
                            },
                        },
                    }
                )
                self.assertNotIn("error", response, resource)
                self.assertEqual(response["id"], request_id)
                self.assertTrue(response["result"]["isError"], resource)
                self.assertEqual(
                    response["result"]["content"][0]["type"],
                    "text",
                )
                public_text = response["result"]["content"][0]["text"]
                self.assertIn("Invalid resource path", public_text)
                self.assertNotIn(private_marker, public_text)
                for private_path in private_server_paths:
                    self.assertNotIn(private_path, public_text)

            missing_response = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": len(invalid_resources) + 1,
                    "method": "tools/call",
                    "params": {
                        "name": "read_skill_resource",
                        "arguments": {
                            "slug": "qgis-example",
                            "resource": "references/missing-private.md",
                        },
                    },
                }
            )
            self.assertNotIn("error", missing_response)
            self.assertTrue(missing_response["result"]["isError"])
            self.assertEqual(
                missing_response["result"]["content"],
                [
                    {
                        "type": "text",
                        "text": "The requested skill resource is unavailable.",
                    }
                ],
            )
            missing_text = missing_response["result"]["content"][0]["text"]
            self.assertNotIn("missing-private.md", missing_text)
            for private_path in private_server_paths:
                self.assertNotIn(private_path, missing_text)

    def test_testskills_shaped_fixture_preserves_metadata_and_resources(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(
                root,
                name="qgis-vector-buffer-spatial-join",
                description="矢量缓冲区空间连接。",
                body="# 矢量缓冲区空间连接\n\n围绕道路生成缓冲区。\n",
                allowed_tools=["exec_shell_command", "read_file", "write_file"],
                metadata={
                    "domain": "qgis",
                    "data-kind": "vector",
                    "qgis-package": "QGIS34407-Release",
                },
                compatibility="Skillz HTTP MCP Server, llama-ui, QGIS Qt6.",
                resources={
                    "assets/tool-contract.json": json.dumps(
                        {"inputs": ["line_or_polygon_layer", "join_layer"]},
                        ensure_ascii=False,
                    ),
                    "references/workflow-checklist.md": "# Checklist\n\n- 准备输入。\n",
                    "assets/icon.png": b"\x89PNG\r\n\x1a\n",
                },
            )

            service = SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
            )

            skill = self._call_tool(
                server,
                "qgis-vector-buffer-spatial-join",
                {"task": "读取中文技能"},
            )

            self.assertIn("矢量缓冲区空间连接", skill["instructions"])
            self.assertEqual(
                skill["metadata"]["allowed_tools"],
                ["exec_shell_command", "read_file", "write_file"],
            )
            self.assertEqual(skill["metadata"]["metadata"]["domain"], "qgis")
            self.assertIn("Skillz HTTP MCP Server", skill["metadata"]["compatibility"])
            self.assertEqual(skill["conformance"], "compatible")
            diagnostics = self._call_tool(server, "list_skills", {})["diagnostics"]
            allowed_list = [
                diagnostic
                for diagnostic in diagnostics
                if diagnostic["code"] == "skills.compat.allowed_tools_list"
            ]
            self.assertEqual(
                allowed_list,
                [
                    {
                        "code": "skills.compat.allowed_tools_list",
                        "severity": "warning",
                        "source": str(
                            root
                            / "qgis-vector-buffer-spatial-join"
                            / "SKILL.md"
                        ),
                        "message": (
                            "Accepted a YAML list for allowed-tools in compatible mode."
                        ),
                    }
                ],
            )
            self.assertEqual(
                {
                    resource["name"].split("/", 1)[1]
                    for resource in skill["resources"]
                },
                {
                    "assets/icon.png",
                    "assets/tool-contract.json",
                    "references/workflow-checklist.md",
                },
            )

    def test_zip_skill_package_is_discovered(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillError, SkillRegistry, SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with zipfile.ZipFile(root / "root-zip.zip", "w") as archive:
                archive.writestr(
                    "SKILL.md",
                    "\n".join(
                        [
                            "---",
                            "name: root-zip",
                            "description: Root zip skill",
                            "---",
                            "Root zip body.",
                        ]
                    ),
                )
                archive.writestr("assets/pixel.bin", b"\x89PNG")

            with zipfile.ZipFile(root / "zip-skill.skill", "w") as archive:
                archive.writestr(
                    "zip-skill/SKILL.md",
                    "\n".join(
                        [
                            "---",
                            "name: zip-skill",
                            "description: Skill from a package",
                            "---",
                            "Packaged body.",
                        ]
                    ),
                )
                archive.writestr("zip-skill/references/guide.md", "Zip guide.")
                archive.writestr("zip-skill/scripts/run.py", "print('resource only')")
            with zipfile.ZipFile(root / "missing-skill.zip", "w") as archive:
                archive.writestr("readme.md", "No skill here.")
            (root / "broken.skill").write_bytes(b"not a zip")

            registry = SkillRegistry(root, validation_mode="compatible")
            registry.load()

            self.assertEqual(
                [skill.slug for skill in registry.skills],
                ["root-zip", "zip-skill"],
            )
            skill = registry.get("zip-skill")
            self.assertTrue(skill.is_zip)
            self.assertEqual(
                list(skill.iter_resource_paths()),
                ["references/guide.md", "scripts/run.py"],
            )

            service = SkillService([root], validation_mode="compatible")
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
            )
            binary = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "resources/read",
                    "params": {"uri": "resource://skillz/root-zip/assets/pixel.bin"},
                }
            )["result"]["contents"][0]
            self.assertEqual(
                binary["blob"],
                base64.b64encode(b"\x89PNG").decode("ascii"),
            )
            activation = self._call_tool(
                server,
                "zip-skill",
                {"task": "Read the packaged skill"},
            )
            self.assertEqual(activation["source_kind"], "archive")
            self.assertIsNone(activation["skill_root"])
            scripts = [
                resource
                for resource in activation["resources"]
                if resource["kind"] == "script"
            ]
            self.assertEqual(
                [resource["path"] for resource in scripts],
                ["scripts/run.py"],
            )
            self.assertIn("Archive resources are read-only", activation["usage"])
            with self.assertRaisesRegex(SkillError, "Unknown skill 'missing-skill'"):
                registry.get("missing-skill")

    def test_invalid_front_matter_is_ignored(self):
        from qcopilots_common.skills import SkillRegistry

        invalid_cases = (
            (
                "missing-description",
                "---\nname: missing-description\n---\nBody\n",
                "skills.skill.invalid",
                "must contain a non-empty string 'description'",
            ),
            (
                "bad_name",
                "---\nname: bad_name\ndescription: Bad name\n---\nBody\n",
                "skills.skill.invalid",
                "must be 1-64 lowercase ASCII letters",
            ),
            (
                "mismatch",
                "---\nname: other-name\ndescription: Mismatch\n---\nBody\n",
                "skills.skill.invalid",
                "must match container 'mismatch'",
            ),
            (
                "metadata-list",
                "---\nname: metadata-list\ndescription: Bad metadata\n"
                "metadata:\n  - nope\n---\nBody\n",
                "skills.skill.invalid",
                "Metadata in",
            ),
            (
                "bad-yaml",
                "---\nname: bad-yaml\ndescription: [unterminated\n---\nBody\n",
                "skills.skill.invalid",
                "YAML front matter",
            ),
            (
                "list-skills",
                "---\nname: list-skills\ndescription: Reserved\n---\nBody\n",
                "skills.skill.reserved_name",
                "conflicts with a reserved MCP tool name",
            ),
        )
        for name, document, code, message in invalid_cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                skill_dir = self._write_raw_skill(root, name, document)
                registry = SkillRegistry(root)
                registry.load()

                self.assertEqual(registry.skills, ())
                self._assert_diagnostic(
                    registry,
                    code=code,
                    source=skill_dir / "SKILL.md",
                    message=message,
                )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(root, name="valid-skill")
            registry = SkillRegistry(root)
            registry.load()
            self.assertEqual([skill.slug for skill in registry.skills], ["valid-skill"])

    def test_lowercase_skill_markdown_is_accepted_only_in_compatible_mode(self):
        from qcopilots_common.skills import SkillRegistry

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            lowercase = root / "lowercase"
            lowercase.mkdir(parents=True)
            (lowercase / "skill.md").write_text(
                "---\n"
                "name: lowercase\n"
                "description: Lowercase skill file\n"
                "---\n"
                "# Lowercase\n",
                encoding="utf-8",
            )

            strict_registry = SkillRegistry(root)
            strict_registry.load()
            self.assertEqual(strict_registry.skills, ())
            self._assert_diagnostic(
                strict_registry,
                code="skills.instructions.noncanonical_name",
                source=lowercase / "skill.md",
                message="Strict mode requires the canonical file name SKILL.md.",
                exact_message=True,
            )

            registry = SkillRegistry(root, validation_mode="compatible")
            registry.load()

            self.assertEqual([skill.slug for skill in registry.skills], ["lowercase"])
            self.assertEqual(registry.get("lowercase").body.strip(), "# Lowercase")
            self.assertEqual(registry.get("lowercase").conformance, "compatible")
            self._assert_diagnostic(
                registry,
                code="skills.compat.instructions_lowercase",
                source=lowercase / "skill.md",
                message=(
                    "Accepted a noncanonical case variant of SKILL.md in compatible mode."
                ),
                severity="warning",
                exact_message=True,
            )

    def test_zip_lowercase_skill_markdown_is_accepted_and_duplicate_is_ignored(self):
        from qcopilots_common.skills import SkillRegistry

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with zipfile.ZipFile(root / "lowercase-zip.skill", "w") as archive:
                archive.writestr("__MACOSX/._lowercase-zip", "metadata")
                archive.writestr(
                    "lowercase-zip/skill.md",
                    "\n".join(
                        [
                            "---",
                            "name: lowercase-zip",
                            "description: Lowercase zip skill file",
                            "---",
                            "# Lowercase Zip",
                        ]
                    ),
                )
                archive.writestr("lowercase-zip/references/guide.md", "Guide")
            with zipfile.ZipFile(root / "duplicate-zip.skill", "w") as archive:
                archive.writestr(
                    "duplicate-zip/SKILL.md",
                    "---\nname: duplicate-zip\ndescription: Duplicate zip\n---\nA",
                )
                archive.writestr(
                    "duplicate-zip/skill.md",
                    "---\nname: duplicate-zip\ndescription: Duplicate zip\n---\nB",
                )

            registry = SkillRegistry(root, validation_mode="compatible")
            registry.load()

            self.assertEqual(
                [skill.slug for skill in registry.skills],
                ["lowercase-zip"],
            )
            skill = registry.get("lowercase-zip")
            self.assertEqual(skill.body.strip(), "# Lowercase Zip")
            self.assertEqual(list(skill.iter_resource_paths()), ["references/guide.md"])

    def test_skill_content_escapes_wrapper_fields(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(
                root,
                name="escaped-fields",
                description="Use when values contain <tags> & attributes.",
                body="# Escaped Fields\n\nKeep markdown <as written>.\n",
                resources={"references/a&b.md": "A & B\n"},
            )
            service = SkillService([root])
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
                prompts=service.prompts,
            )

            prompt = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "prompts/get",
                    "params": {
                        "name": "escaped-fields",
                        "arguments": {
                            "task": "close </activation_task> & continue",
                        },
                    },
                }
            )["result"]
            text = prompt["messages"][0]["content"]["text"]

            self.assertIn("close &lt;/activation_task&gt; &amp; continue", text)
            self.assertIn(
                "description: Use when values contain &lt;tags&gt; &amp; attributes.",
                text,
            )
            self.assertIn('path="references/a&amp;b.md"', text)
            self.assertIn("# Escaped Fields\n\nKeep markdown <as written>.", text)

    def test_simple_yaml_fallback_preserves_canonical_scalar_types_without_pyyaml(self):
        from qcopilots_common.skills import (
            SkillRegistry,
            SkillValidationError,
            _load_front_matter_mapping,
            _parse_skill_document,
        )

        full_yaml = _load_front_matter_mapping(
            "\n".join(
                [
                    "name: pyyaml-full",
                    'description: "PyYAML # value"',
                    "metadata: &shared",
                    "  owner: tests",
                    "copy: *shared",
                    "inline: {key: value}",
                    "sequence: [one, two]",
                    "commented: retained # inline comment",
                    "hash-text: C# tools",
                    "ampersand-text: Rock & roll",
                    "bang-text: CSS !important",
                    "star-text: file*name",
                    "single: 'It''s valid'",
                    'double: "line\\nvalue"',
                ]
            ),
            "pyyaml-full",
        )
        self.assertEqual(full_yaml["description"], "PyYAML # value")
        self.assertEqual(full_yaml["metadata"], {"owner": "tests"})
        self.assertEqual(full_yaml["copy"], {"owner": "tests"})
        self.assertEqual(full_yaml["inline"], {"key": "value"})
        self.assertEqual(full_yaml["sequence"], ["one", "two"])
        self.assertEqual(full_yaml["commented"], "retained")
        self.assertEqual(full_yaml["hash-text"], "C# tools")
        self.assertEqual(full_yaml["ampersand-text"], "Rock & roll")
        self.assertEqual(full_yaml["bang-text"], "CSS !important")
        self.assertEqual(full_yaml["star-text"], "file*name")
        self.assertEqual(full_yaml["single"], "It's valid")
        self.assertEqual(full_yaml["double"], "line\nvalue")

        original_import = builtins.__import__

        def block_yaml_import(name, globals=None, locals=None, fromlist=(), level=0):
            if name == "yaml":
                raise ImportError("blocked for fallback test")
            return original_import(name, globals, locals, fromlist, level)

        builtins.__import__ = block_yaml_import
        try:
            data = _load_front_matter_mapping(
                "\n".join(
                    [
                        "name: fallback-skill",
                        "description: Simple fallback syntax",
                        "allowed-tools: read_file write_file",
                        "integer: 123",
                        "null-value: null",
                        "true-value: true",
                        "false-value: false",
                        "float-value: 1.5",
                        "quoted-number: '123'",
                        "hash-text: C# tools",
                        "ampersand-text: Rock & roll",
                        "bang-text: CSS !important",
                        "star-text: file*name",
                        "metadata:",
                        "  owner: tests",
                    ]
                ),
                "fallback-test",
            )
            self.assertEqual(data["allowed-tools"], "read_file write_file")
            self.assertEqual(data["metadata"], {"owner": "tests"})
            self.assertEqual(data["integer"], 123)
            self.assertIsNone(data["null-value"])
            self.assertIs(data["true-value"], True)
            self.assertIs(data["false-value"], False)
            self.assertEqual(data["float-value"], 1.5)
            self.assertEqual(data["quoted-number"], "123")
            self.assertEqual(data["hash-text"], "C# tools")
            self.assertEqual(data["ampersand-text"], "Rock & roll")
            self.assertEqual(data["bang-text"], "CSS !important")
            self.assertEqual(data["star-text"], "file*name")

            quoted = _load_front_matter_mapping(
                "single: 'It''s valid'\ndouble: \"line\\nvalue\"\n",
                "fallback-quotes",
            )
            self.assertEqual(quoted["single"], "It's valid")
            self.assertEqual(quoted["double"], "line\nvalue")

            invalid_scalars = (
                ("description", "123", "non-empty string 'description'"),
                ("description", "null", "non-empty string 'description'"),
                ("description", "true", "non-empty string 'description'"),
                ("description", "false", "non-empty string 'description'"),
                ("description", "1.5", "non-empty string 'description'"),
                ("compatibility", "123", "field 'compatibility'"),
                ("compatibility", "null", "field 'compatibility'"),
                ("compatibility", "true", "field 'compatibility'"),
                ("compatibility", "false", "field 'compatibility'"),
                ("compatibility", "1.5", "field 'compatibility'"),
                ("license", "null", "field 'license'"),
                ("allowed-tools", "true", "field 'allowed-tools'"),
            )
            for field, scalar, expected_message in invalid_scalars:
                with self.subTest(field=field, scalar=scalar):
                    lines = [
                        "---",
                        "name: fallback-strict",
                        "description: Valid description",
                    ]
                    if field == "description":
                        lines[-1] = f"description: {scalar}"
                    else:
                        lines.append(f"{field}: {scalar}")
                    lines.extend(["---", "# Body", ""])
                    with self.assertRaisesRegex(
                        SkillValidationError,
                        expected_message,
                    ):
                        _parse_skill_document(
                            "\n".join(lines),
                            source="fallback-strict/SKILL.md",
                            container_name="fallback-strict",
                            validation_mode="strict",
                            diagnostics=[],
                        )

            metadata, body, conformance = _parse_skill_document(
                "---\n"
                "name: fallback-quoted\n"
                "description: '123'\n"
                "license: 'null'\n"
                "compatibility: 'true'\n"
                "allowed-tools: 'read_file'\n"
                "---\n"
                "# Quoted\n",
                source="fallback-quoted/SKILL.md",
                container_name="fallback-quoted",
                validation_mode="strict",
                diagnostics=[],
            )
            self.assertEqual(metadata.description, "123")
            self.assertEqual(metadata.license, "null")
            self.assertEqual(metadata.compatibility, "true")
            self.assertEqual(metadata.allowed_tools, ("read_file",))
            self.assertEqual(body, "# Quoted\n")
            self.assertEqual(conformance, "strict")

            with self.assertRaisesRegex(
                SkillValidationError,
                "missing 'description'",
            ):
                _parse_skill_document(
                    "---\nname: fallback-compatible\ndescription: null\n---\nBody\n",
                    source="fallback-compatible/SKILL.md",
                    container_name="fallback-compatible",
                    validation_mode="compatible",
                    diagnostics=[],
                )

            unsupported_cases = (
                (
                    "multilevel-indent",
                    "metadata:\n  owner: tests\n    nested: value\n",
                    "Inconsistent metadata indentation in fallback-test:3.",
                ),
                (
                    "unexpected-indent",
                    "  name: unexpected\n",
                    "Unsupported YAML indentation in fallback-test:1.",
                ),
                (
                    "metadata-list",
                    "metadata:\n  - owner\n",
                    "YAML sequences in fallback-test:2 require PyYAML.",
                ),
                (
                    "inline-list",
                    "extension: [one, two]\n",
                    "YAML sequences in fallback-test:1 require PyYAML.",
                ),
                (
                    "sequence-indicator",
                    "description: - item\n",
                    "YAML sequences in fallback-test:1 require PyYAML.",
                ),
                (
                    "block-scalar",
                    "description: |\n",
                    "Block YAML scalars in fallback-test:1 require PyYAML.",
                ),
                (
                    "inline-mapping",
                    "extension: {owner: tests}\n",
                    "Inline YAML mappings in fallback-test:1 require PyYAML.",
                ),
                (
                    "anchor",
                    "description: &desc Shared\n",
                    "YAML anchors, aliases, and tags in fallback-test:1 require PyYAML.",
                ),
                (
                    "alias",
                    "description: *desc\n",
                    "YAML anchors, aliases, and tags in fallback-test:1 require PyYAML.",
                ),
                (
                    "tag",
                    "description: !!str Shared\n",
                    "YAML anchors, aliases, and tags in fallback-test:1 require PyYAML.",
                ),
                (
                    "duplicate-top-level",
                    "name: first\nname: second\n",
                    "Duplicate YAML key 'name' in fallback-test:2.",
                ),
                (
                    "duplicate-metadata",
                    "metadata:\n  owner: first\n  owner: second\n",
                    "Duplicate metadata key 'owner' in fallback-test:3.",
                ),
                (
                    "inline-comment",
                    "description: value # comment\n",
                    "Inline YAML comments in fallback-test:1 require PyYAML.",
                ),
                (
                    "complex-key-indicator",
                    "description: ? key\n",
                    "Nested YAML mappings in fallback-test:1 require PyYAML.",
                ),
                (
                    "nested-mapping",
                    "extension: child: value\n",
                    "Nested YAML mappings in fallback-test:1 require PyYAML.",
                ),
                (
                    "invalid-single-quote",
                    "description: 'It's invalid'\n",
                    "Invalid single-quoted YAML scalar in fallback-test:1.",
                ),
                (
                    "invalid-double-quote",
                    'description: "bad\\q"\n',
                    "Invalid double-quoted YAML scalar in fallback-test:1.",
                ),
            )
            for case_name, yaml_text, expected_message in unsupported_cases:
                with self.subTest(unsupported=case_name):
                    with self.assertRaises(SkillValidationError) as error_context:
                        _load_front_matter_mapping(yaml_text, "fallback-test")
                    self.assertEqual(str(error_context.exception), expected_message)

            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                skill_dir = self._write_raw_skill(
                    root,
                    "fallback-diagnostic",
                    "---\n"
                    "name: fallback-diagnostic\n"
                    "description: Fallback diagnostic\n"
                    "extension: {owner: tests}\n"
                    "---\n"
                    "# Body\n",
                )
                registry = SkillRegistry(root)
                registry.load()
                self.assertEqual(registry.skills, ())
                self._assert_diagnostic(
                    registry,
                    code="skills.skill.invalid",
                    source=skill_dir / "SKILL.md",
                    message=(
                        f"Inline YAML mappings in {skill_dir / 'SKILL.md'}:3 require "
                        "PyYAML."
                    ),
                    exact_message=True,
                )
        finally:
            builtins.__import__ = original_import

    def test_real_pyyaml_rejects_duplicate_mapping_keys_without_value_leaks(self):
        import yaml

        import qcopilots_common.skills as skills_module

        self.assertTrue(callable(yaml.safe_load))
        duplicate_cases = (
            (
                "top-level-name",
                "duplicate-name",
                (
                    "name: duplicate-name",
                    "name: PRIVATE-DUPLICATE-NAME-MUST-NOT-LEAK",
                    "description: Duplicate name",
                ),
                "PRIVATE-DUPLICATE-NAME-MUST-NOT-LEAK",
            ),
            (
                "top-level-description",
                "duplicate-description",
                (
                    "name: duplicate-description",
                    "description: First public description",
                    "description: PRIVATE-DUPLICATE-DESCRIPTION-MUST-NOT-LEAK",
                ),
                "PRIVATE-DUPLICATE-DESCRIPTION-MUST-NOT-LEAK",
            ),
            (
                "metadata-key",
                "duplicate-metadata",
                (
                    "name: duplicate-metadata",
                    "description: Duplicate metadata key",
                    "metadata:",
                    "  owner: public-owner",
                    "  owner: PRIVATE-DUPLICATE-METADATA-MUST-NOT-LEAK",
                ),
                "PRIVATE-DUPLICATE-METADATA-MUST-NOT-LEAK",
            ),
            (
                "nested-mapping-key",
                "duplicate-nested",
                (
                    "name: duplicate-nested",
                    "description: Duplicate nested mapping key",
                    "extension:",
                    "  nested: public-value",
                    "  nested: PRIVATE-DUPLICATE-NESTED-MUST-NOT-LEAK",
                ),
                "PRIVATE-DUPLICATE-NESTED-MUST-NOT-LEAK",
            ),
        )

        for validation_mode in ("strict", "compatible"):
            for case_name, skill_name, front_matter, private_marker in duplicate_cases:
                with (
                    self.subTest(mode=validation_mode, case=case_name),
                    tempfile.TemporaryDirectory() as tmp,
                ):
                    root = Path(tmp) / "Skills"
                    skill_dir = self._write_raw_skill(
                        root,
                        skill_name,
                        "\n".join(
                            (
                                "---",
                                *front_matter,
                                "---",
                                "# Duplicate mapping must be rejected",
                                "",
                            )
                        ),
                    )
                    registry = skills_module.SkillRegistry(
                        root,
                        validation_mode=validation_mode,
                    )
                    with mock.patch.object(
                        skills_module,
                        "_parse_simple_yaml",
                        side_effect=AssertionError(
                            "真实 PyYAML 分支不得回退到简单 YAML 解析器"
                        ),
                    ) as simple_fallback:
                        registry.load()
                    simple_fallback.assert_not_called()

                    self.assertEqual(registry.skills, ())
                    diagnostic = self._assert_diagnostic(
                        registry,
                        code="skills.skill.invalid",
                        source=skill_dir / "SKILL.md",
                        message="",
                    )
                    self.assertIn("duplicate", diagnostic.message.lower())
                    self.assertNotIn(
                        private_marker,
                        json.dumps(
                            [entry.as_dict() for entry in registry.diagnostics],
                            ensure_ascii=False,
                        ),
                    )

    def test_json_safe_budget_bounds_alias_expansion_and_is_shared_per_document(
        self,
    ):
        import qcopilots_common.skills as skills_module

        source = "budget-skill/SKILL.md"
        document = self._canonical_document(
            "budget-skill",
            body="# Budget skill\n",
        )

        def parse_mapping(data, **limits):
            diagnostics = []
            with ExitStack() as stack:
                stack.enter_context(
                    mock.patch.object(
                        skills_module,
                        "_load_front_matter_mapping",
                        return_value=data,
                    )
                )
                for name, value in limits.items():
                    stack.enter_context(mock.patch.object(skills_module, name, value))
                result = skills_module._parse_skill_document(
                    document,
                    source=source,
                    container_name="budget-skill",
                    validation_mode="compatible",
                    diagnostics=diagnostics,
                )
            return result, diagnostics

        shared_alias = ["read_file"]
        legal_mapping = {
            "name": "budget-skill",
            "description": "Budget skill",
            "metadata": {"owner": shared_alias},
            "allowed-tools": shared_alias,
            "extra-a": shared_alias,
            "extra-b": shared_alias,
        }
        (metadata, body, conformance), diagnostics = parse_mapping(legal_mapping)
        self.assertEqual(metadata.metadata, {"owner": '["read_file"]'})
        self.assertEqual(metadata.allowed_tools, ("read_file",))
        self.assertEqual(metadata.allowed_tools_declaration, ["read_file"])
        self.assertEqual(
            metadata.extra,
            {
                "extra-a": ["read_file"],
                "extra-b": ["read_file"],
            },
        )
        self.assertEqual(body, "# Budget skill\n")
        self.assertEqual(conformance, "compatible")
        self.assertEqual(
            {diagnostic.code for diagnostic in diagnostics},
            {
                "skills.compat.metadata_value",
                "skills.compat.allowed_tools_list",
                "skills.metadata.unknown_field",
            },
        )

        with self.assertRaises(skills_module.SkillValidationError) as shared_error:
            parse_mapping(
                legal_mapping,
                MAX_JSON_SAFE_VISITED_NODES=11,
            )
        self.assertEqual(
            str(shared_error.exception),
            "Field 'extra-b' in budget-skill/SKILL.md exceeds the JSON-safe "
            "visited node budget.",
        )

        deeply_nested = "leaf"
        for _ in range(3):
            deeply_nested = [deeply_nested]
        with self.assertRaises(skills_module.SkillValidationError) as depth_error:
            parse_mapping(
                {
                    "name": "budget-skill",
                    "description": "Budget skill",
                    "deep": deeply_nested,
                },
                MAX_JSON_SAFE_RECURSION_DEPTH=2,
            )
        self.assertEqual(
            str(depth_error.exception),
            "Field 'deep[0][0][0]' in budget-skill/SKILL.md exceeds the JSON-safe "
            "recursion depth budget.",
        )

        with self.assertRaises(skills_module.SkillValidationError) as generated_error:
            parse_mapping(
                {
                    "name": "budget-skill",
                    "description": "Budget skill",
                    "extra": ["first", "second"],
                },
                MAX_JSON_SAFE_GENERATED_NODES=6,
            )
        self.assertEqual(
            str(generated_error.exception),
            "Field 'extra[1]' in budget-skill/SKILL.md exceeds the JSON-safe "
            "generated node budget.",
        )

        with self.assertRaises(skills_module.SkillValidationError) as output_error:
            parse_mapping(
                {
                    "name": "budget-skill",
                    "description": "Budget skill",
                    "payload": "x" * 100,
                },
                MAX_JSON_SAFE_OUTPUT_BYTES=100,
            )
        self.assertEqual(
            str(output_error.exception),
            "Field 'payload' in budget-skill/SKILL.md exceeds the JSON-safe "
            "output byte budget.",
        )

        shared_dag = ["leaf"]
        for _ in range(6):
            shared_dag = [shared_dag, shared_dag]
        with self.assertRaises(skills_module.SkillValidationError) as dag_error:
            parse_mapping(
                {
                    "name": "budget-skill",
                    "description": "Budget skill",
                    "dag": shared_dag,
                },
                MAX_JSON_SAFE_VISITED_NODES=20,
            )
        self.assertRegex(
            str(dag_error.exception),
            r"^Field 'dag(?:\[[01]\])+' in budget-skill/SKILL\.md exceeds the "
            r"JSON-safe visited node budget\.$",
        )
        self.assertNotIsInstance(dag_error.exception, RecursionError)

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(root, name="budget-skill")
            with (
                mock.patch.object(
                    skills_module,
                    "_load_front_matter_mapping",
                    return_value={
                        "name": "budget-skill",
                        "description": "Budget skill",
                        "deep": deeply_nested,
                    },
                ),
                mock.patch.object(
                    skills_module,
                    "MAX_JSON_SAFE_RECURSION_DEPTH",
                    2,
                ),
            ):
                registry = skills_module.SkillRegistry(
                    root,
                    validation_mode="compatible",
                )
                registry.load()
            self.assertEqual(registry.skills, ())
            self._assert_diagnostic(
                registry,
                code="skills.skill.invalid",
                source=skill_dir / "SKILL.md",
                message=(
                    f"Field 'deep[0][0][0]' in {skill_dir / 'SKILL.md'} exceeds "
                    "the JSON-safe recursion depth budget."
                ),
                exact_message=True,
            )

    def test_cyclic_yaml_aliases_are_diagnostic_and_keep_lkg_without_retry(self):
        import sys

        import qcopilots_common.skills as skills_module

        def cyclic_mapping(kind, name):
            if kind == "list":
                cycle = []
                cycle.append(cycle)
            else:
                cycle = {}
                cycle["self"] = cycle
            return {
                "name": name,
                "description": "Cyclic YAML alias",
                "loop": cycle,
            }

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            for kind, expected_path in (("list", "loop[0]"), ("map", "loop.self")):
                with self.subTest(kind=kind):
                    case_root = root / kind
                    skill_dir = self._write_skill(
                        case_root,
                        name=f"cycle-{kind}",
                    )
                    yaml_module = mock.Mock()
                    yaml_module.safe_load.return_value = cyclic_mapping(
                        kind,
                        f"cycle-{kind}",
                    )
                    with mock.patch.dict(sys.modules, {"yaml": yaml_module}):
                        registry = skills_module.SkillRegistry(
                            case_root,
                            validation_mode="compatible",
                        )
                        registry.load()
                    self.assertNotIn(
                        f"cycle-{kind}",
                        {skill.slug for skill in registry.skills},
                    )
                    self._assert_diagnostic(
                        registry,
                        code="skills.skill.invalid",
                        source=skill_dir / "SKILL.md",
                        message=(
                            f"Field '{expected_path}' in {skill_dir / 'SKILL.md'} "
                            "contains a cyclic YAML value."
                        ),
                        exact_message=True,
                    )
                    yaml_module.safe_load.assert_called_once()

            lkg_root = Path(tmp) / "LkgSkills"
            lkg_dir = self._write_skill(
                lkg_root,
                name="cycle-lkg",
                body="# Trusted LKG\n",
                resources={"references/data.txt": "trusted data\n"},
            )
            service = skills_module.SkillService(
                [lkg_root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            before_registry = service._registry
            before_skill = before_registry.get("cycle-lkg")
            before_tool_names = tuple(tool.name for tool in service._tools)
            before_resource_uris = tuple(
                resource.uri for resource in service._resources
            )
            before_prompt_names = tuple(prompt.name for prompt in service._prompts)

            (lkg_dir / "SKILL.md").write_text(
                self._canonical_document(
                    "cycle-lkg",
                    body="# Cyclic candidate must not commit\n",
                ),
                encoding="utf-8",
            )
            yaml_module = mock.Mock()
            yaml_module.safe_load.return_value = cyclic_mapping("list", "cycle-lkg")
            with mock.patch.dict(sys.modules, {"yaml": yaml_module}):
                service.ensure_current()
                self.assertIs(service.registry, before_registry)
                self.assertIs(service.registry.get("cycle-lkg"), before_skill)
                self.assertEqual(
                    tuple(tool.name for tool in service.tools()),
                    before_tool_names,
                )
                self.assertEqual(
                    tuple(resource.uri for resource in service.resources()),
                    before_resource_uris,
                )
                self.assertEqual(
                    tuple(prompt.name for prompt in service.prompts()),
                    before_prompt_names,
                )
                diagnostics = service.diagnostics
                yaml_module.safe_load.assert_called_once()

            invalid_diagnostics = [
                diagnostic
                for diagnostic in diagnostics
                if diagnostic.code == "skills.skill.invalid"
                and diagnostic.source == str(lkg_dir / "SKILL.md")
            ]
            self.assertEqual(len(invalid_diagnostics), 1)
            self.assertEqual(invalid_diagnostics[0].severity, "error")
            self.assertEqual(
                invalid_diagnostics[0].message,
                f"Field 'loop[0]' in {lkg_dir / 'SKILL.md'} contains a cyclic "
                "YAML value.",
            )
            reload_diagnostics = [
                diagnostic
                for diagnostic in diagnostics
                if diagnostic.code == "skills.reload.failed"
            ]
            self.assertEqual(len(reload_diagnostics), 1)
            self.assertIn(
                "last-known-good snapshot remains active",
                reload_diagnostics[0].message,
            )
            current_tools = {tool.name: tool for tool in service.tools()}
            current_prompts = {prompt.name: prompt for prompt in service.prompts()}
            self.assertEqual(
                current_tools["cycle-lkg"].handler({"task": "Use LKG"})[
                    "instructions"
                ],
                "# Trusted LKG\n",
            )
            self.assertEqual(
                current_tools["read_skill"].handler({"slug": "cycle-lkg"})[
                    "content"
                ],
                "# Trusted LKG\n",
            )
            self.assertIn(
                "# Trusted LKG",
                current_prompts["cycle-lkg"].handler({"task": "Use LKG"})[
                    "messages"
                ][0]["content"]["text"],
            )
            current_resources = {
                resource.uri: resource for resource in service.resources()
            }
            self.assertEqual(
                next(iter(current_resources.values())).handler(),
                "trusted data\n",
            )
            self.assertEqual(service.diagnostics, diagnostics)

    def test_recursion_error_wrapper_keeps_lkg_and_deduplicates_reload(self):
        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="recursion-lkg",
                body="# Trusted recursion LKG\n",
                resources={
                    "references/data.txt": "trusted recursion data\n",
                },
            )
            instructions = skill_dir / "SKILL.md"
            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            before_registry = service._registry
            before_skill = before_registry.get("recursion-lkg")
            before_fingerprint = service._fingerprint
            before_diagnostics = service.diagnostics
            self.assertEqual(before_diagnostics, ())
            before_tool_names = tuple(tool.name for tool in service._tools)
            before_prompt_names = tuple(
                prompt.name for prompt in service._prompts
            )
            before_resource_uris = tuple(
                resource.uri for resource in service._resources
            )

            instructions.write_text(
                self._canonical_document(
                    "recursion-lkg",
                    description="Recursion candidate",
                    body="# Candidate forces a real recursion wrapper failure\n",
                ),
                encoding="utf-8",
            )
            with mock.patch.object(
                skills_module,
                "_json_safe_value_impl",
                side_effect=RecursionError("injected parser recursion"),
            ) as recursion_impl:
                service.ensure_current()

                self.assertIs(service._registry, before_registry)
                self.assertIs(service._registry.get("recursion-lkg"), before_skill)
                self.assertEqual(service._fingerprint, before_fingerprint)
                failed_fingerprint = service._failed_fingerprint
                self.assertIsNotNone(failed_fingerprint)
                self.assertNotEqual(failed_fingerprint, before_fingerprint)
                self.assertEqual(before_registry.diagnostics, before_diagnostics)
                self.assertEqual(
                    tuple(tool.name for tool in service._tools),
                    before_tool_names,
                )
                self.assertEqual(
                    tuple(prompt.name for prompt in service._prompts),
                    before_prompt_names,
                )
                self.assertEqual(
                    tuple(resource.uri for resource in service._resources),
                    before_resource_uris,
                )

                diagnostics = service.diagnostics
                invalid_diagnostics = [
                    diagnostic
                    for diagnostic in diagnostics
                    if diagnostic.code == "skills.skill.invalid"
                ]
                self.assertEqual(len(invalid_diagnostics), 1)
                self.assertEqual(
                    invalid_diagnostics[0].to_dict(),
                    {
                        "code": "skills.skill.invalid",
                        "severity": "error",
                        "source": str(instructions),
                        "message": (
                            f"Field 'name' in {instructions} exceeds the JSON-safe "
                            "recursion depth budget."
                        ),
                    },
                )
                reload_diagnostics = [
                    diagnostic
                    for diagnostic in diagnostics
                    if diagnostic.code == "skills.reload.failed"
                ]
                self.assertEqual(len(reload_diagnostics), 1)
                self.assertEqual(
                    reload_diagnostics[0].to_dict(),
                    {
                        "code": "skills.reload.failed",
                        "severity": "error",
                        "source": str(root.resolve()),
                        "message": (
                            "Skill reload failed; the last-known-good snapshot "
                            "remains active: Existing skill 'recursion-lkg' became "
                            "invalid; keeping previous snapshot."
                        ),
                    },
                )
                self.assertEqual(
                    diagnostics,
                    (invalid_diagnostics[0], reload_diagnostics[0]),
                )

                current_tools = {tool.name: tool for tool in service.tools()}
                current_prompts = {
                    prompt.name: prompt for prompt in service.prompts()
                }
                current_resources = {
                    resource.uri: resource for resource in service.resources()
                }
                self.assertEqual(
                    current_tools["recursion-lkg"].handler(
                        {"task": "Use recursion LKG"}
                    )["instructions"],
                    "# Trusted recursion LKG\n",
                )
                self.assertIn(
                    "# Trusted recursion LKG",
                    current_prompts["recursion-lkg"].handler(
                        {"task": "Use recursion LKG"}
                    )["messages"][0]["content"]["text"],
                )

                resource_uri = (
                    "resource://skillz/recursion-lkg/references/data.txt"
                )
                # 目录 Skill 的说明缓存与各 resource 独立绑定身份：重载失败时
                # Tool/Prompt 继续使用可信 LKG；未变化 resource 仍可信可读。
                # ZIP 的 resource 共用 archive 身份，容器变化后资源必须失效。
                self.assertEqual(
                    current_resources[resource_uri].handler(),
                    "trusted recursion data\n",
                )
                self.assertEqual(
                    current_tools["read_skill_resource"].handler(
                        {
                            "slug": "recursion-lkg",
                            "resource": "references/data.txt",
                        }
                    )["content"],
                    "trusted recursion data\n",
                )
                self.assertEqual(
                    [
                        entry["slug"]
                        for entry in current_tools["list_skills"].handler({})[
                            "skills"
                        ]
                    ],
                    ["recursion-lkg"],
                )

                service.ensure_current()
                self.assertEqual(service._failed_fingerprint, failed_fingerprint)
                self.assertEqual(service.diagnostics, diagnostics)
                recursion_impl.assert_called_once()

    def test_bridge_metadata_budget_counts_canonical_utf8_document_exactly(self):
        import sys

        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="byte-budget",
                description="Placeholder",
                body="# UTF-8 budget body\n",
            )
            description = 'Café "quoted" \\path\nline\tcontrol'
            exact_mapping = {
                "name": "byte-budget",
                "description": description,
                "license": "MIT-界",
                "compatibility": "QGIS ≥ 4\\Windows",
                "allowed-tools": ["read_file", "工具\\path"],
                "metadata": {
                    "owner": 'Zoë "Q"\\ops',
                    "note": "line1\nline2\t界",
                },
                "payload": {
                    "text": '界"\\\n\t',
                    "nested": ["é", True, None],
                },
            }
            over_mapping = {
                **exact_mapping,
                "description": description + "a",
            }
            expected_payload = {
                "name": "byte-budget",
                "description": description,
                "license": "MIT-界",
                "compatibility": "QGIS ≥ 4\\Windows",
                "allowed_tools": ["read_file", "工具\\path"],
                "allowed_tools_declaration": ["read_file", "工具\\path"],
                "metadata": {
                    "owner": 'Zoë "Q"\\ops',
                    "note": "line1\nline2\t界",
                },
                "extra": {
                    "payload": {
                        "text": '界"\\\n\t',
                        "nested": ["é", True, None],
                    }
                },
                "conformance": "compatible",
            }
            expected_serialized = json.dumps(
                expected_payload,
                ensure_ascii=False,
                sort_keys=True,
                separators=(",", ":"),
                allow_nan=False,
            ).encode("utf-8")
            exact_bytes = len(expected_serialized)
            self.assertIn("é".encode("utf-8"), expected_serialized)
            self.assertIn("界".encode("utf-8"), expected_serialized)
            self.assertIn(b"\\n", expected_serialized)
            self.assertIn(b"\\t", expected_serialized)
            self.assertIn(b'\\"', expected_serialized)
            self.assertIn(b"\\\\", expected_serialized)
            over_payload = {
                **expected_payload,
                "description": description + "a",
            }
            over_serialized = json.dumps(
                over_payload,
                ensure_ascii=False,
                sort_keys=True,
                separators=(",", ":"),
                allow_nan=False,
            ).encode("utf-8")
            self.assertEqual(
                len(over_serialized),
                exact_bytes + 1,
            )

            yaml_module = mock.Mock()
            yaml_module.safe_load.side_effect = [
                exact_mapping,
                over_mapping,
                exact_mapping,
            ]
            with (
                mock.patch.dict(sys.modules, {"yaml": yaml_module}),
                mock.patch.object(
                    skills_module,
                    "MAX_JSON_SAFE_OUTPUT_BYTES",
                    exact_bytes,
                ),
            ):
                exact_registry = skills_module.SkillRegistry(
                    root,
                    validation_mode="compatible",
                )
                exact_registry.load()
                over_registry = skills_module.SkillRegistry(
                    root,
                    validation_mode="compatible",
                )
                over_registry.load()

            exact_skill = exact_registry.get("byte-budget")
            self.assertEqual(
                tuple(skill.slug for skill in exact_registry.skills),
                ("byte-budget",),
            )
            actual_payload = skills_module._metadata_dict(exact_skill)
            self.assertEqual(actual_payload, expected_payload)
            self.assertEqual(
                json.dumps(
                    actual_payload,
                    ensure_ascii=False,
                    sort_keys=True,
                    separators=(",", ":"),
                    allow_nan=False,
                ).encode("utf-8"),
                expected_serialized,
            )
            self.assertEqual(exact_skill.body, "# UTF-8 budget body\n")
            self.assertNotIn(
                "skills.skill.invalid",
                self._diagnostic_codes(exact_registry),
            )
            self.assertEqual(over_registry.skills, ())
            self._assert_diagnostic(
                over_registry,
                code="skills.skill.invalid",
                source=skill_dir / "SKILL.md",
                message=(
                    f"Front matter in {skill_dir / 'SKILL.md'} serialized metadata "
                    "document exceeds the JSON-safe output byte budget."
                ),
                exact_message=True,
            )

            real_json_dumps = skills_module.json.dumps

            def fail_canonical_serialization(value, *args, **kwargs):
                if kwargs.get("separators") == (",", ":"):
                    raise TypeError("injected canonical serialization failure")
                return real_json_dumps(value, *args, **kwargs)

            with (
                mock.patch.dict(sys.modules, {"yaml": yaml_module}),
                mock.patch.object(
                    skills_module.json,
                    "dumps",
                    side_effect=fail_canonical_serialization,
                ),
            ):
                unserializable_registry = skills_module.SkillRegistry(
                    root,
                    validation_mode="compatible",
                )
                unserializable_registry.load()
            self.assertEqual(unserializable_registry.skills, ())
            self._assert_diagnostic(
                unserializable_registry,
                code="skills.skill.invalid",
                source=skill_dir / "SKILL.md",
                message=(
                    f"Front matter in {skill_dir / 'SKILL.md'} could not be "
                    "serialized as a JSON-safe metadata document."
                ),
                exact_message=True,
            )
            self.assertEqual(yaml_module.safe_load.call_count, 3)

    def test_archive_metadata_budget_uses_final_compatible_conformance(self):
        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir(parents=True)
            archive_path = root / "archive-budget.skill"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr(
                    "archive-budget/SKILL.md",
                    self._canonical_document("archive-budget"),
                )

            compatible_payload = {
                "name": "archive-budget",
                "description": "Canonical skill",
                "license": None,
                "compatibility": None,
                "allowed_tools": [],
                "allowed_tools_declaration": None,
                "metadata": {},
                "extra": {},
                "conformance": "compatible",
            }
            compatible_bytes = len(
                json.dumps(
                    compatible_payload,
                    ensure_ascii=False,
                    sort_keys=True,
                    separators=(",", ":"),
                    allow_nan=False,
                ).encode("utf-8")
            )
            strict_payload = {
                **compatible_payload,
                "conformance": "strict",
            }
            strict_bytes = len(
                json.dumps(
                    strict_payload,
                    ensure_ascii=False,
                    sort_keys=True,
                    separators=(",", ":"),
                    allow_nan=False,
                ).encode("utf-8")
            )
            self.assertGreater(compatible_bytes, strict_bytes)

            with mock.patch.object(
                skills_module,
                "MAX_JSON_SAFE_OUTPUT_BYTES",
                strict_bytes,
            ):
                rejected_registry = skills_module.SkillRegistry(
                    root,
                    validation_mode="compatible",
                )
                rejected_registry.load()
            self.assertEqual(rejected_registry.skills, ())
            self._assert_diagnostic(
                rejected_registry,
                code="skills.archive.invalid",
                source=archive_path,
                message=(
                    f"Front matter in {archive_path}!/archive-budget/SKILL.md "
                    "serialized metadata document exceeds the JSON-safe output "
                    "byte budget."
                ),
                exact_message=True,
            )

            with mock.patch.object(
                skills_module,
                "MAX_JSON_SAFE_OUTPUT_BYTES",
                compatible_bytes,
            ):
                exact_registry = skills_module.SkillRegistry(
                    root,
                    validation_mode="compatible",
                )
                exact_registry.load()
            archive_skill = exact_registry.get("archive-budget")
            self.assertEqual(archive_skill.conformance, "compatible")
            self.assertEqual(
                skills_module._metadata_dict(archive_skill),
                compatible_payload,
            )

    def test_json_safe_budget_reload_preserves_lkg_and_deduplicates_failure(self):
        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="budget-lkg",
                body="# Last known good\n",
                resources={"references/data.txt": "trusted data\n"},
            )
            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            before_registry = service._registry
            before_skill = before_registry.get("budget-lkg")
            before_skills = tuple(skill.slug for skill in service.registry.skills)
            before_tools = tuple(tool.name for tool in service.tools())
            before_resources = tuple(resource.uri for resource in service.resources())
            before_prompts = tuple(prompt.name for prompt in service.prompts())

            (skill_dir / "SKILL.md").write_text(
                self._canonical_document(
                    "budget-lkg",
                    body="# Candidate must not commit\n",
                ),
                encoding="utf-8",
            )
            shared_alias = ["read_file"]
            invalid_mapping = {
                "name": "budget-lkg",
                "description": "Budget LKG",
                "metadata": {"owner": shared_alias},
                "allowed-tools": shared_alias,
                "extra-a": shared_alias,
                "extra-b": shared_alias,
            }

            with (
                mock.patch.object(
                    skills_module,
                    "_load_front_matter_mapping",
                    return_value=invalid_mapping,
                ) as loader,
                mock.patch.object(
                    skills_module,
                    "MAX_JSON_SAFE_VISITED_NODES",
                    11,
                ),
            ):
                service.ensure_current()
                self.assertEqual(loader.call_count, 1)
                self.assertEqual(
                    tuple(skill.slug for skill in service.registry.skills),
                    before_skills,
                )
                self.assertEqual(
                    tuple(tool.name for tool in service.tools()),
                    before_tools,
                )
                self.assertEqual(
                    tuple(resource.uri for resource in service.resources()),
                    before_resources,
                )
                self.assertEqual(
                    tuple(prompt.name for prompt in service.prompts()),
                    before_prompts,
                )
                self.assertIs(service.registry, before_registry)
                self.assertIs(
                    service.registry.get("budget-lkg"),
                    before_skill,
                )
                self.assertEqual(loader.call_count, 1)

                diagnostics = service.diagnostics
                self.assertEqual(loader.call_count, 1)

            budget_diagnostics = [
                diagnostic
                for diagnostic in diagnostics
                if diagnostic.code == "skills.skill.invalid"
            ]
            self.assertEqual(len(budget_diagnostics), 1)
            self.assertEqual(
                budget_diagnostics[0].message,
                f"Field 'extra-b' in {skill_dir / 'SKILL.md'} exceeds the JSON-safe "
                "visited node budget.",
            )
            reload_diagnostics = [
                diagnostic
                for diagnostic in diagnostics
                if diagnostic.code == "skills.reload.failed"
            ]
            self.assertEqual(len(reload_diagnostics), 1)
            self.assertIn(
                "last-known-good snapshot remains active",
                reload_diagnostics[0].message,
            )
            current_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                current_tools["budget-lkg"].handler({"task": "Use LKG"})[
                    "instructions"
                ],
                "# Last known good\n",
            )
            current_prompts = {prompt.name: prompt for prompt in service.prompts()}
            self.assertIn(
                "# Last known good",
                current_prompts["budget-lkg"].handler({"task": "Use LKG"})[
                    "messages"
                ][0]["content"]["text"],
            )
            current_resources = {
                resource.uri: resource for resource in service.resources()
            }
            self.assertEqual(
                next(iter(current_resources.values())).handler(),
                "trusted data\n",
            )

    def test_refresh_interval_uses_fake_clock_and_limits_full_fingerprints(self):
        import qcopilots_common.skills as skills_module

        class FakeClock:
            def __init__(self):
                self.value = 0.0

            def __call__(self):
                return self.value

            def advance(self, seconds):
                self.value += seconds

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="refresh-clock",
                body="# Refresh before\n",
            )
            clock = FakeClock()
            service = skills_module.SkillService(
                [root],
                refresh_interval_seconds=10,
                clock=clock,
            )
            real_fingerprint = skills_module._build_roots_fingerprint

            with mock.patch.object(
                skills_module,
                "_build_roots_fingerprint",
                wraps=real_fingerprint,
            ) as fingerprint:
                initial_tools = {tool.name: tool for tool in service.tools()}
                service.resources()
                service.prompts()
                service.registry
                self.assertEqual(fingerprint.call_count, 1)
                self.assertEqual(
                    initial_tools["refresh-clock"].handler({"task": "Before"})[
                        "instructions"
                    ],
                    "# Refresh before\n",
                )

                clock.advance(9.999)
                service.tools()
                self.assertEqual(fingerprint.call_count, 1)
                clock.value = service._next_refresh_check
                service.tools()
                self.assertEqual(fingerprint.call_count, 2)

                (skill_dir / "SKILL.md").write_text(
                    self._canonical_document(
                        "refresh-clock",
                        body="# Refresh after with a different size\n",
                    ),
                    encoding="utf-8",
                )
                within_interval = {
                    tool.name: tool for tool in service.tools()
                }
                self.assertEqual(fingerprint.call_count, 2)
                self.assertIs(
                    within_interval["refresh-clock"],
                    initial_tools["refresh-clock"],
                )

                clock.advance(10)
                refreshed_tools = {tool.name: tool for tool in service.tools()}
                self.assertEqual(fingerprint.call_count, 4)
                self.assertEqual(
                    refreshed_tools["refresh-clock"].handler({"task": "After"})[
                        "instructions"
                    ],
                    "# Refresh after with a different size\n",
                )
                service.resources()
                service.prompts()
                service.registry
                self.assertEqual(fingerprint.call_count, 4)

            for invalid_interval in (-1, float("inf"), float("nan"), True):
                with self.subTest(invalid_interval=invalid_interval):
                    with self.assertRaisesRegex(
                        ValueError,
                        "^refresh_interval_seconds must be a non-negative "
                        "finite number$",
                    ):
                        skills_module.SkillService(
                            [root],
                            refresh_interval_seconds=invalid_interval,
                        )
            with self.assertRaisesRegex(ValueError, "^clock must be callable$"):
                skills_module.SkillService([root], clock=None)
            with self.assertRaisesRegex(
                ValueError,
                "^clock must return a finite number$",
            ):
                skills_module.SkillService([root], clock=lambda: float("nan"))

    def test_refresh_deadline_uses_completion_clock_and_recovers_from_clock_errors(
        self,
    ):
        import qcopilots_common.skills as skills_module

        class FakeClock:
            def __init__(self, value):
                self.value = value
                self.failure = None

            def __call__(self):
                failure = self.failure
                self.failure = None
                if failure == "exception":
                    raise RuntimeError("completion clock failed")
                if failure == "nan":
                    return float("nan")
                return self.value

            def advance(self, seconds):
                self.value += seconds

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(root, name="completion-clock")
            clock = FakeClock(100.0)
            service = skills_module.SkillService(
                [root],
                refresh_interval_seconds=10,
                clock=clock,
            )
            real_fingerprint = skills_module._build_roots_fingerprint
            delayed_calls = []

            def delayed_fingerprint(roots, *, validation_mode):
                delayed_calls.append(roots)
                if len(delayed_calls) == 1:
                    clock.advance(50)
                return real_fingerprint(
                    roots,
                    validation_mode=validation_mode,
                )

            with mock.patch.object(
                skills_module,
                "_build_roots_fingerprint",
                side_effect=delayed_fingerprint,
            ) as fingerprint:
                service.ensure_current()
                self.assertEqual(fingerprint.call_count, 1)
                self.assertEqual(clock.value, 150.0)
                self.assertEqual(service._next_refresh_check, 160.0)

                service.tools()
                service.resources()
                service.prompts()
                service.registry
                self.assertEqual(fingerprint.call_count, 1)

                clock.advance(9.999)
                service.tools()
                self.assertEqual(fingerprint.call_count, 1)
                clock.value = service._next_refresh_check
                service.tools()
                self.assertEqual(fingerprint.call_count, 2)

            zero_clock = FakeClock(200.0)
            zero_service = skills_module.SkillService(
                [root],
                refresh_interval_seconds=0,
                clock=zero_clock,
            )
            with mock.patch.object(
                skills_module,
                "_build_roots_fingerprint",
                wraps=real_fingerprint,
            ) as zero_fingerprint:
                zero_service.tools()
                zero_service.resources()
                zero_service.prompts()
                zero_service.registry
            self.assertEqual(zero_fingerprint.call_count, 4)
            self.assertEqual(zero_service._next_refresh_check, zero_clock.value)

            rollback_clock = FakeClock(500.0)
            rollback_service = skills_module.SkillService(
                [root],
                refresh_interval_seconds=10,
                clock=rollback_clock,
            )

            def rollback_on_completion(roots, *, validation_mode):
                rollback_clock.value = 490.0
                return real_fingerprint(
                    roots,
                    validation_mode=validation_mode,
                )

            with mock.patch.object(
                skills_module,
                "_build_roots_fingerprint",
                side_effect=rollback_on_completion,
            ) as rollback_fingerprint:
                rollback_service.ensure_current()
            self.assertEqual(rollback_fingerprint.call_count, 1)
            self.assertEqual(rollback_service._next_refresh_check, 510.0)
            self.assertFalse(rollback_service._refresh_in_progress)

            for completion_failure in ("nan", "exception"):
                with self.subTest(completion_failure=completion_failure):
                    failure_clock = FakeClock(300.0)
                    failure_service = skills_module.SkillService(
                        [root],
                        refresh_interval_seconds=5,
                        clock=failure_clock,
                    )
                    before_diagnostics = (
                        failure_service._registry.diagnostics
                        + failure_service._reload_diagnostics
                    )
                    completion_calls = []

                    def fail_completion(roots, *, validation_mode):
                        completion_calls.append(roots)
                        if len(completion_calls) == 1:
                            failure_clock.failure = completion_failure
                        return real_fingerprint(
                            roots,
                            validation_mode=validation_mode,
                        )

                    with mock.patch.object(
                        skills_module,
                        "_build_roots_fingerprint",
                        side_effect=fail_completion,
                    ) as failure_fingerprint:
                        failure_service.ensure_current()
                        self.assertFalse(failure_service._refresh_in_progress)
                        self.assertEqual(
                            failure_service._next_refresh_check,
                            305.0,
                        )
                        self.assertEqual(
                            failure_service.diagnostics,
                            before_diagnostics,
                        )
                        self.assertEqual(failure_fingerprint.call_count, 1)

                        failure_clock.advance(5)
                        failure_service.ensure_current()
                        self.assertEqual(failure_fingerprint.call_count, 2)
                        self.assertFalse(failure_service._refresh_in_progress)
                        self.assertEqual(
                            failure_service._next_refresh_check,
                            310.0,
                        )

            body_clock = FakeClock(400.0)
            body_service = skills_module.SkillService(
                [root],
                refresh_interval_seconds=5,
                clock=body_clock,
            )

            def fail_body_and_completion(roots, *, validation_mode):
                del roots, validation_mode
                body_clock.failure = "exception"
                raise RuntimeError("refresh body failed")

            with mock.patch.object(
                skills_module,
                "_build_roots_fingerprint",
                side_effect=fail_body_and_completion,
            ):
                with self.assertRaisesRegex(RuntimeError, "^refresh body failed$"):
                    body_service.ensure_current()
            self.assertFalse(body_service._refresh_in_progress)
            self.assertEqual(body_service._next_refresh_check, 405.0)

    def test_directory_rename_blocks_cross_source_takeover_and_invalid_keeps_lkg(
        self,
    ):
        import qcopilots_common.skills as skills_module

        class FakeClock:
            def __init__(self):
                self.value = 0.0

            def __call__(self):
                return self.value

            def advance(self, seconds):
                self.value += seconds

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_raw_skill(
                root,
                "source-slot",
                self._canonical_document(
                    "old-name",
                    description="Old description",
                    extra=("metadata:", "  owner: before"),
                    body="# Old body\n",
                ),
            )
            resource_path = skill_dir / "references" / "guide.txt"
            resource_path.parent.mkdir(parents=True)
            resource_path.write_text("trusted guide\n", encoding="utf-8")
            contender_dir = self._write_raw_skill(
                root,
                "z-contender",
                self._canonical_document(
                    "old-name",
                    description="Contending old slug",
                    body="# Contender must never take over\n",
                ),
            )
            contender_resource = contender_dir / "references" / "guide.txt"
            contender_resource.parent.mkdir(parents=True)
            contender_resource.write_text(
                "contender data must not leak\n",
                encoding="utf-8",
            )
            clock = FakeClock()
            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=1,
                clock=clock,
            )
            old_registry = service._registry
            old_skill = old_registry.get("old-name")
            old_fingerprint = service._fingerprint
            self._assert_diagnostic(
                old_registry,
                code="skills.skill.slug_collision",
                source=contender_dir / "SKILL.md",
                message=(
                    "Skill slug 'old-name' was already registered; the first "
                    "configured root wins."
                ),
                exact_message=True,
                severity="warning",
            )
            self.assertEqual(
                old_skill.instructions_path,
                (skill_dir / "SKILL.md").resolve(),
            )

            (skill_dir / "SKILL.md").write_text(
                self._canonical_document(
                    "new-name",
                    description="Renamed description",
                    extra=(
                        "allowed-tools: read_file",
                        "metadata:",
                        "  owner: after",
                        "extension: committed",
                    ),
                    body="# Renamed body with a different size\n",
                ),
                encoding="utf-8",
            )
            clock.advance(1)
            service.ensure_current()

            self.assertIs(service._registry, old_registry)
            self.assertIs(service._registry.get("old-name"), old_skill)
            self.assertEqual(
                service._registry.get("old-name").instructions_path,
                (skill_dir / "SKILL.md").resolve(),
            )
            self.assertIsNotNone(service._failed_fingerprint)
            takeover_reload = [
                diagnostic
                for diagnostic in service._reload_diagnostics
                if diagnostic.code == "skills.reload.failed"
            ]
            self.assertEqual(
                len(takeover_reload),
                1,
            )
            self.assertEqual(
                takeover_reload[0].message,
                "Skill reload failed; the last-known-good snapshot remains "
                "active: Existing skill 'old-name' was renamed to 'new-name' "
                "at the same source while its old slug was claimed by a "
                "different source; keeping previous snapshot.",
            )
            lkg_tools = {tool.name: tool for tool in service.tools()}
            lkg_prompts = {prompt.name: prompt for prompt in service.prompts()}
            lkg_resources = {
                resource.uri: resource for resource in service.resources()
            }
            old_resource_uri = (
                "resource://skillz/old-name/references/guide.txt"
            )
            self.assertIn("old-name", lkg_tools)
            self.assertNotIn("new-name", lkg_tools)
            self.assertIn("old-name", lkg_prompts)
            self.assertNotIn("new-name", lkg_prompts)
            self.assertEqual(set(lkg_resources), {old_resource_uri})
            self.assertEqual(
                lkg_tools["old-name"].handler({"task": "Keep the LKG"})[
                    "instructions"
                ],
                "# Old body\n",
            )
            self.assertIn(
                "# Old body",
                lkg_prompts["old-name"].handler({"task": "Keep the LKG"})[
                    "messages"
                ][0]["content"]["text"],
            )
            self.assertEqual(
                lkg_resources[old_resource_uri].handler(),
                "trusted guide\n",
            )
            self.assertEqual(
                lkg_tools["read_skill_resource"].handler(
                    {
                        "slug": "old-name",
                        "resource": "references/guide.txt",
                    }
                )["content"],
                "trusted guide\n",
            )
            lkg_list = lkg_tools["list_skills"].handler({})
            self.assertEqual(
                [entry["slug"] for entry in lkg_list["skills"]],
                ["old-name"],
            )
            self.assertEqual(
                service._registry.get("old-name").directory,
                skill_dir.resolve(),
            )

            (contender_dir / "SKILL.md").unlink()
            clock.advance(1)
            service.ensure_current()

            renamed_registry = service._registry
            renamed_skill = renamed_registry.get("new-name")
            self.assertIsNot(renamed_registry, old_registry)
            self.assertIsNot(renamed_skill, old_skill)
            self.assertEqual(
                renamed_skill.instructions_path,
                old_skill.instructions_path,
            )
            self.assertNotEqual(service._fingerprint, old_fingerprint)
            self.assertIsNone(service._failed_fingerprint)
            self.assertNotIn(
                "skills.reload.failed",
                {diagnostic.code for diagnostic in service.diagnostics},
            )
            with self.assertRaises(skills_module.SkillError):
                renamed_registry.get("old-name")

            renamed_tools = {tool.name: tool for tool in service.tools()}
            renamed_resources = {
                resource.uri: resource for resource in service.resources()
            }
            renamed_prompts = {prompt.name: prompt for prompt in service.prompts()}
            self.assertNotIn("old-name", renamed_tools)
            self.assertIn("new-name", renamed_tools)
            self.assertNotIn("old-name", renamed_prompts)
            self.assertIn("new-name", renamed_prompts)
            self.assertNotIn(
                "resource://skillz/old-name/references/guide.txt",
                renamed_resources,
            )
            new_resource_uri = "resource://skillz/new-name/references/guide.txt"
            self.assertIn(new_resource_uri, renamed_resources)
            self.assertEqual(
                renamed_resources[new_resource_uri].handler(),
                "trusted guide\n",
            )
            tool_result = renamed_tools["new-name"].handler({"task": "Use renamed"})
            self.assertEqual(
                tool_result["instructions"],
                "# Renamed body with a different size\n",
            )
            self.assertEqual(
                tool_result["metadata"]["description"],
                "Renamed description",
            )
            self.assertEqual(tool_result["metadata"]["metadata"], {"owner": "after"})
            self.assertEqual(
                tool_result["metadata"]["extra"],
                {"extension": "committed"},
            )
            prompt_result = renamed_prompts["new-name"].handler({"task": "Use renamed"})
            self.assertIn(
                "# Renamed body with a different size",
                prompt_result["messages"][0]["content"]["text"],
            )

            stable_registry = service._registry
            stable_diagnostics = service.diagnostics
            clock.advance(1)
            service.ensure_current()
            self.assertIs(service._registry, stable_registry)
            self.assertEqual(service.diagnostics, stable_diagnostics)
            self.assertIsNone(service._failed_fingerprint)

            (skill_dir / "SKILL.md").write_text(
                "---\nname: invalid-renamed-candidate\n---\n# Invalid candidate\n",
                encoding="utf-8",
            )
            clock.advance(1)
            service.ensure_current()
            self.assertIs(service._registry, stable_registry)
            self.assertIs(service._registry.get("new-name"), renamed_skill)
            self.assertIsNotNone(service._failed_fingerprint)
            invalid_diagnostics = service.diagnostics
            self._assert_diagnostic(
                mock.Mock(diagnostics=invalid_diagnostics),
                code="skills.skill.invalid",
                source=skill_dir / "SKILL.md",
                message=(
                    f"Front matter in {skill_dir / 'SKILL.md'} is missing "
                    "'description'."
                ),
                exact_message=True,
            )
            self.assertEqual(
                sum(
                    diagnostic.code == "skills.reload.failed"
                    for diagnostic in invalid_diagnostics
                ),
                1,
            )
            invalid_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                invalid_tools["new-name"].handler({"task": "Stale candidate"})[
                    "instructions"
                ],
                "# Renamed body with a different size\n",
            )
            invalid_prompts = {prompt.name: prompt for prompt in service.prompts()}
            self.assertIn(
                "# Renamed body with a different size",
                invalid_prompts["new-name"].handler({"task": "Stale candidate"})[
                    "messages"
                ][0]["content"]["text"],
            )
            clock.advance(1)
            service.ensure_current()
            self.assertEqual(service.diagnostics, invalid_diagnostics)

            (skill_dir / "SKILL.md").unlink()
            clock.advance(1)
            service.ensure_current()
            self.assertEqual(service.registry.skills, ())
            self.assertIsNone(service._failed_fingerprint)
            self.assertNotIn(
                "skills.reload.failed",
                {diagnostic.code for diagnostic in service.diagnostics},
            )
            self.assertNotIn("new-name", {tool.name for tool in service.tools()})
            self.assertNotIn("new-name", {prompt.name for prompt in service.prompts()})
            self.assertNotIn(
                new_resource_uri,
                {resource.uri for resource in service.resources()},
            )

    def test_same_source_invalid_reload_keeps_lkg_without_retry(self):
        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            first_dir = self._write_raw_skill(
                root,
                "source-a",
                self._canonical_document("shared-name", body="# Original source\n"),
            )
            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            original_registry = service._registry
            original_skill = original_registry.get("shared-name")
            (first_dir / "SKILL.md").write_text(
                "---\nname: shared-name\n---\n# Invalid original source\n",
                encoding="utf-8",
            )
            service.ensure_current()
            self.assertIs(service._registry, original_registry)
            self.assertIs(service._registry.get("shared-name"), original_skill)
            self.assertEqual(
                original_skill.instructions_path,
                (first_dir / "SKILL.md").resolve(),
            )
            self.assertIsNotNone(service._failed_fingerprint)
            invalid_diagnostics = service.diagnostics
            self._assert_diagnostic(
                mock.Mock(diagnostics=invalid_diagnostics),
                code="skills.skill.invalid",
                source=first_dir / "SKILL.md",
                message=(
                    f"Front matter in {first_dir / 'SKILL.md'} is missing "
                    "'description'."
                ),
                exact_message=True,
            )
            self.assertEqual(
                sum(
                    diagnostic.code == "skills.reload.failed"
                    for diagnostic in invalid_diagnostics
                ),
                1,
            )
            service.ensure_current()
            self.assertEqual(service.diagnostics, invalid_diagnostics)

    def test_archive_rename_lifecycle_blocks_cross_source_slug_takeover(self):
        import qcopilots_common.skills as skills_module
        from qcopilots_common.mcp_http import ToolError

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir(parents=True)
            archive_path = root / "a-primary.skill"
            contender_archive = root / "z-contender.skill"

            def write_archive(path, name, body, raw=None):
                with zipfile.ZipFile(path, "w") as archive:
                    archive.writestr(
                        "archive-slot/SKILL.md",
                        raw
                        if raw is not None
                        else self._canonical_document(name, body=body),
                    )
                    archive.writestr(
                        "archive-slot/references/guide.txt",
                        "archive guide\n",
                    )

            write_archive(archive_path, "archive-old", "# Archive old\n")
            write_archive(
                contender_archive,
                "archive-old",
                "# Contending archive must not take over\n",
            )
            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            old_archive_registry = service._registry
            old_archive_skill = old_archive_registry.get("archive-old")
            archive_source = old_archive_skill.instructions_path
            self.assertEqual(archive_source, archive_path.resolve())
            self._assert_diagnostic(
                old_archive_registry,
                code="skills.skill.slug_collision",
                source=f"{contender_archive}!/archive-slot/SKILL.md",
                message=(
                    "Skill slug 'archive-old' was already registered; the first "
                    "configured root wins."
                ),
                exact_message=True,
                severity="warning",
            )
            write_archive(
                archive_path,
                "archive-new",
                "# Archive renamed with a different size\n",
            )
            service.ensure_current()

            self.assertIs(service._registry, old_archive_registry)
            self.assertIs(
                service._registry.get("archive-old"),
                old_archive_skill,
            )
            self.assertEqual(
                service._registry.get("archive-old").instructions_path,
                archive_path.resolve(),
            )
            self.assertIsNotNone(service._failed_fingerprint)
            archive_takeover_reload = [
                diagnostic
                for diagnostic in service._reload_diagnostics
                if diagnostic.code == "skills.reload.failed"
            ]
            self.assertEqual(
                len(archive_takeover_reload),
                1,
            )
            self.assertEqual(
                archive_takeover_reload[0].message,
                "Skill reload failed; the last-known-good snapshot remains "
                "active: Existing skill 'archive-old' was renamed to "
                "'archive-new' at the same source while its old slug was "
                "claimed by a different source; keeping previous snapshot.",
            )
            archive_lkg_tools = {tool.name: tool for tool in service.tools()}
            archive_lkg_prompts = {
                prompt.name: prompt for prompt in service.prompts()
            }
            archive_lkg_resources = {
                resource.uri: resource for resource in service.resources()
            }
            old_archive_uri = (
                "resource://skillz/archive-old/references/guide.txt"
            )
            self.assertIn("archive-old", archive_lkg_tools)
            self.assertNotIn("archive-new", archive_lkg_tools)
            self.assertIn("archive-old", archive_lkg_prompts)
            self.assertNotIn("archive-new", archive_lkg_prompts)
            self.assertEqual(set(archive_lkg_resources), {old_archive_uri})
            self.assertEqual(
                archive_lkg_tools["archive-old"].handler(
                    {"task": "Keep archive LKG"}
                )["instructions"],
                "# Archive old\n",
            )
            self.assertIn(
                "# Archive old",
                archive_lkg_prompts["archive-old"].handler(
                    {"task": "Keep archive LKG"}
                )["messages"][0]["content"]["text"],
            )
            with self.assertRaises(ToolError) as archive_resource_unavailable:
                archive_lkg_resources[old_archive_uri].handler()
            self.assertEqual(
                str(archive_resource_unavailable.exception),
                "The requested skill resource is unavailable.",
            )
            with self.assertRaises(ToolError) as archive_management_unavailable:
                archive_lkg_tools["read_skill_resource"].handler(
                    {
                        "slug": "archive-old",
                        "resource": "references/guide.txt",
                    }
                )
            self.assertEqual(
                str(archive_management_unavailable.exception),
                "The requested skill resource is unavailable.",
            )
            self.assertEqual(
                [
                    entry["slug"]
                    for entry in archive_lkg_tools["list_skills"].handler({})[
                        "skills"
                    ]
                ],
                ["archive-old"],
            )

            contender_archive.unlink()
            service.ensure_current()
            archive_skill = service._registry.get("archive-new")
            self.assertEqual(archive_skill.instructions_path, archive_source)
            with self.assertRaises(skills_module.SkillError):
                service._registry.get("archive-old")
            self.assertNotIn(
                "skills.reload.failed",
                {diagnostic.code for diagnostic in service.diagnostics},
            )
            archive_tools = {tool.name: tool for tool in service.tools()}
            archive_prompts = {
                prompt.name: prompt for prompt in service.prompts()
            }
            self.assertNotIn("archive-old", archive_tools)
            self.assertIn("archive-new", archive_tools)
            self.assertNotIn("archive-old", archive_prompts)
            self.assertIn("archive-new", archive_prompts)
            self.assertEqual(
                archive_tools["archive-new"].handler({"task": "Renamed archive"})[
                    "instructions"
                ],
                "# Archive renamed with a different size\n",
            )
            archive_resources = {
                resource.uri: resource for resource in service.resources()
            }
            archive_uri = "resource://skillz/archive-new/references/guide.txt"
            self.assertNotIn(old_archive_uri, archive_resources)
            self.assertIn(archive_uri, archive_resources)
            self.assertEqual(
                archive_resources[archive_uri].handler(),
                "archive guide\n",
            )
            self.assertIn(
                "# Archive renamed with a different size",
                archive_prompts["archive-new"].handler(
                    {"task": "Renamed archive"}
                )["messages"][0]["content"]["text"],
            )
            self.assertEqual(
                [
                    entry["slug"]
                    for entry in archive_tools["list_skills"].handler({})[
                        "skills"
                    ]
                ],
                ["archive-new"],
            )

            committed_archive_registry = service._registry
            committed_archive_skill = archive_skill
            write_archive(
                archive_path,
                "archive-new",
                "# Invalid archive\n",
                raw=(
                    "---\n"
                    "name: archive-new\n"
                    "---\n"
                    "# Invalid archive candidate\n"
                ),
            )
            service.ensure_current()
            self.assertIs(service._registry, committed_archive_registry)
            self.assertIs(
                service._registry.get("archive-new"),
                committed_archive_skill,
            )
            self.assertIsNotNone(service._failed_fingerprint)
            bad_archive_diagnostics = service.diagnostics
            self._assert_diagnostic(
                mock.Mock(diagnostics=bad_archive_diagnostics),
                code="skills.archive.invalid",
                source=archive_path,
                message=(
                    f"Front matter in {archive_path}!/archive-slot/SKILL.md is "
                    "missing 'description'."
                ),
                exact_message=True,
            )
            self.assertEqual(
                sum(
                    diagnostic.code == "skills.reload.failed"
                    for diagnostic in bad_archive_diagnostics
                ),
                1,
            )
            bad_archive_tools = {tool.name: tool for tool in service.tools()}
            bad_archive_prompts = {
                prompt.name: prompt for prompt in service.prompts()
            }
            bad_archive_resources = {
                resource.uri: resource for resource in service.resources()
            }
            self.assertEqual(
                bad_archive_tools["archive-new"].handler(
                    {"task": "Invalid archive"}
                )["instructions"],
                "# Archive renamed with a different size\n",
            )
            self.assertIn(
                "# Archive renamed with a different size",
                bad_archive_prompts["archive-new"].handler(
                    {"task": "Invalid archive"}
                )["messages"][0]["content"]["text"],
            )
            with self.assertRaises(ToolError) as bad_archive_resource:
                bad_archive_resources[archive_uri].handler()
            self.assertEqual(
                str(bad_archive_resource.exception),
                "The requested skill resource is unavailable.",
            )
            with self.assertRaises(ToolError) as bad_archive_management:
                bad_archive_tools["read_skill_resource"].handler(
                    {
                        "slug": "archive-new",
                        "resource": "references/guide.txt",
                    }
                )
            self.assertEqual(
                str(bad_archive_management.exception),
                "The requested skill resource is unavailable.",
            )
            service.ensure_current()
            self.assertEqual(service.diagnostics, bad_archive_diagnostics)

            archive_path.unlink()
            service.ensure_current()
            self.assertEqual(service.registry.skills, ())
            self.assertIsNone(service._failed_fingerprint)
            self.assertNotIn(
                "skills.reload.failed",
                {diagnostic.code for diagnostic in service.diagnostics},
            )
            final_archive_tools = {
                tool.name: tool for tool in service.tools()
            }
            self.assertNotIn("archive-new", final_archive_tools)
            self.assertNotIn(
                "archive-new",
                {prompt.name for prompt in service.prompts()},
            )
            self.assertNotIn(
                archive_uri,
                {resource.uri for resource in service.resources()},
            )
            self.assertEqual(
                final_archive_tools["list_skills"].handler({})["count"],
                0,
            )

    def test_real_directory_skill_deletion_commits_without_prior_invalid_reload(
        self,
    ):
        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="direct-delete",
                resources={"references/guide.txt": "delete me\n"},
            )
            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            before_registry = service._registry
            self.assertEqual(
                tuple(skill.slug for skill in before_registry.skills),
                ("direct-delete",),
            )

            (skill_dir / "SKILL.md").unlink()
            service.ensure_current()

            self.assertIsNot(service._registry, before_registry)
            self.assertEqual(service.registry.skills, ())
            self.assertIsNone(service._failed_fingerprint)
            self.assertNotIn(
                "skills.reload.failed",
                {diagnostic.code for diagnostic in service.diagnostics},
            )
            tools = {tool.name: tool for tool in service.tools()}
            self.assertNotIn("direct-delete", tools)
            self.assertNotIn(
                "direct-delete",
                {prompt.name for prompt in service.prompts()},
            )
            self.assertNotIn(
                "resource://skillz/direct-delete/references/guide.txt",
                {resource.uri for resource in service.resources()},
            )
            self.assertEqual(tools["list_skills"].handler({})["count"], 0)

    def test_stable_snapshot_retries_initial_race_and_keeps_lkg_on_reload_race(
        self,
    ):
        import qcopilots_common.skills as skills_module

        class FakeClock:
            def __init__(self):
                self.value = 0.0

            def __call__(self):
                return self.value

            def advance(self, seconds):
                self.value += seconds

        real_load = skills_module.SkillRegistry.load
        real_fingerprint = skills_module._build_roots_fingerprint

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "InitialSkills"
            skill_dir = self._write_skill(
                root,
                name="initial-race",
                body="# Initial candidate\n",
            )
            initial_loads = []

            def load_then_change_once(registry):
                real_load(registry)
                initial_loads.append(tuple(skill.slug for skill in registry.skills))
                if len(initial_loads) == 1:
                    (skill_dir / "SKILL.md").write_text(
                        self._canonical_document(
                            "initial-race",
                            body="# Stable replacement with a different size\n",
                        ),
                        encoding="utf-8",
                    )

            with mock.patch.object(
                skills_module.SkillRegistry,
                "load",
                new=load_then_change_once,
            ), mock.patch.object(
                skills_module,
                "_build_roots_fingerprint",
                wraps=real_fingerprint,
            ) as initial_fingerprints:
                initial_service = skills_module.SkillService([root])

            self.assertEqual(initial_loads, [("initial-race",), ("initial-race",)])
            self.assertEqual(initial_fingerprints.call_count, 4)
            self.assertEqual(
                initial_service._registry.get("initial-race").read_body(),
                "# Stable replacement with a different size\n",
            )
            self.assertEqual(
                initial_service._fingerprint,
                real_fingerprint(
                    initial_service.roots,
                    validation_mode=initial_service.validation_mode,
                ),
            )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "ReloadSkills"
            skill_dir = self._write_skill(
                root,
                name="reload-race",
                body="# Last known good\n",
                resources={"references/stable.txt": "stable\n"},
            )
            clock = FakeClock()
            service = skills_module.SkillService(
                [root],
                refresh_interval_seconds=10,
                clock=clock,
            )
            before_registry = service._registry
            before_skill = before_registry.get("reload-race")
            before_fingerprint = service._fingerprint
            before_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                before_tools["reload-race"].handler({"task": "Before"})[
                    "instructions"
                ],
                "# Last known good\n",
            )

            (skill_dir / "SKILL.md").write_text(
                self._canonical_document(
                    "reload-race",
                    body="# Unstable candidate zero\n",
                ),
                encoding="utf-8",
            )
            clock.advance(10)
            reload_loads = []

            def load_then_change_every_time(registry):
                real_load(registry)
                reload_loads.append(tuple(skill.slug for skill in registry.skills))
                attempt = len(reload_loads)
                (skill_dir / "SKILL.md").write_text(
                    self._canonical_document(
                        "reload-race",
                        body=f"# Unstable candidate {attempt} {'x' * attempt}\n",
                    ),
                    encoding="utf-8",
                )

            with mock.patch.object(
                skills_module.SkillRegistry,
                "load",
                new=load_then_change_every_time,
            ), mock.patch.object(
                skills_module,
                "_build_roots_fingerprint",
                wraps=real_fingerprint,
            ) as reload_fingerprints:
                service.ensure_current()
                self.assertEqual(
                    len(reload_loads),
                    skills_module.MAX_SKILL_SNAPSHOT_ATTEMPTS,
                )
                self.assertEqual(reload_fingerprints.call_count, 6)
                self.assertIs(service.registry, before_registry)
                self.assertIs(
                    service.registry.get("reload-race"),
                    before_skill,
                )
                self.assertEqual(service._fingerprint, before_fingerprint)
                current_tools = {tool.name: tool for tool in service.tools()}
                self.assertEqual(
                    tuple(current_tools),
                    tuple(before_tools),
                )
                service.resources()
                service.prompts()
                diagnostics = service.diagnostics
                self.assertEqual(
                    len(reload_loads),
                    skills_module.MAX_SKILL_SNAPSHOT_ATTEMPTS,
                )
                self.assertEqual(reload_fingerprints.call_count, 6)

            self.assertEqual(
                [diagnostic.code for diagnostic in diagnostics],
                ["skills.scan.filesystem_changed", "skills.reload.failed"],
            )
            self.assertEqual(
                diagnostics[0].message,
                "Skills changed while a stable snapshot was being built.",
            )
            self.assertEqual(diagnostics[0].source, str(root.resolve()))
            self.assertEqual(
                diagnostics[1].message,
                "Skill reload failed; the last-known-good snapshot remains active: "
                "Skills changed while a stable snapshot was being built.",
            )
            self.assertEqual(
                current_tools["reload-race"].handler(
                    {"task": "After failed reload"}
                )["instructions"],
                "# Last known good\n",
            )
            current_prompts = {prompt.name: prompt for prompt in service.prompts()}
            self.assertIn(
                "# Last known good",
                current_prompts["reload-race"].handler(
                    {"task": "After failed reload"}
                )["messages"][0]["content"]["text"],
            )

    def test_hot_reload_adds_skills_and_keeps_old_snapshot_on_bad_reload(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(root, name="alpha")
            service = SkillService([root], refresh_interval_seconds=0)
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
                prompts=service.prompts,
            )

            def assert_catalog_note(expected, unexpected=()):
                tools = self._tools_by_name(server)
                for tool_name in (
                    "read_skill",
                    "list_skill_resources",
                    "read_skill_resource",
                ):
                    with self.subTest(tool=tool_name, expected=expected):
                        description = tools[tool_name]["description"]
                        for slug in expected:
                            self.assertIn(slug, description)
                        for slug in unexpected:
                            self.assertNotIn(slug, description)

            initialize = server.handle_json_rpc(
                {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}}
            )
            self.assertNotIn(
                "listChanged",
                initialize["result"]["capabilities"]["prompts"],
            )
            assert_catalog_note(("alpha",), ("bravo",))

            self._write_skill(root, name="bravo")
            tools = self._tool_names(server)
            self.assertIn("bravo", tools)
            self.assertIn("bravo", self._prompt_names(server))
            assert_catalog_note(("alpha", "bravo"))

            self._write_skill(root, name="list-skills")
            tools = self._tool_names(server)
            self.assertIn("alpha", tools)
            self.assertIn("bravo", tools)
            self.assertNotIn("list-skills", tools)
            prompts = self._prompt_names(server)
            self.assertIn("alpha", prompts)
            self.assertIn("bravo", prompts)
            self.assertNotIn("list-skills", prompts)

            shutil.rmtree(root / "list-skills")
            self._write_skill(root, name="charlie")
            tools = self._tool_names(server)
            self.assertIn("charlie", tools)
            self.assertIn("charlie", self._prompt_names(server))

            (root / "alpha" / "SKILL.md").write_text(
                "\n".join(
                    [
                        "---",
                        "name: alpha",
                        "description: Updated alpha",
                        "---",
                        "# Updated Alpha",
                    ]
                ),
                encoding="utf-8",
            )
            updated = self._call_tool(server, "alpha", {"task": "Read alpha"})
            self.assertIn("# Updated Alpha", updated["instructions"])
            prompt = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "method": "prompts/get",
                    "params": {"name": "alpha", "arguments": {"task": "Read alpha"}},
                }
            )
            self.assertIn(
                "# Updated Alpha",
                prompt["result"]["messages"][0]["content"]["text"],
            )

            shutil.rmtree(root / "bravo")
            tools = self._tool_names(server)
            self.assertNotIn("bravo", tools)
            self.assertNotIn("bravo", self._prompt_names(server))
            assert_catalog_note(("alpha", "charlie"), ("bravo",))

            shutil.rmtree(root)
            tools = self._tool_names(server)
            self.assertIn("alpha", tools)
            self.assertIn("alpha", self._prompt_names(server))
            root.mkdir()
            self._write_skill(root, name="delta")
            tools = self._tool_names(server)
            self.assertIn("delta", tools)
            self.assertNotIn("alpha", tools)
            prompts = self._prompt_names(server)
            self.assertIn("delta", prompts)
            self.assertNotIn("alpha", prompts)
            assert_catalog_note(("delta",), ("alpha", "bravo", "charlie"))

    def test_hot_reload_accepts_utf8_bom_only_in_compatible_mode(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(root, name="alpha")
            service = SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
                prompts=service.prompts,
            )

            self._write_raw_skill(
                root,
                "bravo",
                "\ufeff---\n"
                "name: bravo\n"
                "description: Bravo written with a UTF-8 BOM\n"
                "---\n"
                "# Bravo\n",
            )

            tools = self._tool_names(server)
            self.assertIn("bravo", tools)
            bravo = self._call_tool(server, "bravo", {"task": "Read bravo"})
            self.assertIn("# Bravo", bravo["instructions"])
            self.assertEqual(bravo["conformance"], "compatible")
            diagnostics = self._call_tool(server, "list_skills", {})["diagnostics"]
            self.assertTrue(
                any(
                    diagnostic["code"] == "skills.compat.frontmatter_format"
                    for diagnostic in diagnostics
                )
            )

    def test_service_reloads_when_manifest_skill_roots_change(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import build_skill_service

        with tempfile.TemporaryDirectory() as tmp:
            plugin_dir = Path(tmp) / "plugin"
            plugin_dir.mkdir()
            root_a = plugin_dir / "SkillsA"
            root_b = plugin_dir / "SkillsB"
            (plugin_dir / "Skills").mkdir()
            self._write_skill(root_a, name="alpha")
            self._write_skill(root_b, name="bravo")
            manifest = plugin_dir / "qcopilots_service.json"
            manifest.write_text(
                json.dumps({"skillsRoot": "SkillsA"}),
                encoding="utf-8",
            )

            service = build_skill_service(plugin_dir)
            service.refresh_interval_seconds = 0
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
                prompts=service.prompts,
            )

            def read_skill_description():
                return self._tools_by_name(server)["read_skill"]["description"]

            tools = self._tool_names(server)
            self.assertIn("alpha", tools)
            self.assertNotIn("bravo", tools)
            self.assertIn("alpha", self._prompt_names(server))
            self.assertNotIn("bravo", self._prompt_names(server))
            self.assertIn("alpha", read_skill_description())
            self.assertNotIn("bravo", read_skill_description())

            manifest.write_text(
                json.dumps({"skillsRoot": "SkillsB"}),
                encoding="utf-8",
            )
            tools = self._tool_names(server)
            self.assertIn("bravo", tools)
            self.assertNotIn("alpha", tools)
            self.assertIn("bravo", self._prompt_names(server))
            self.assertNotIn("alpha", self._prompt_names(server))
            self.assertIn("bravo", read_skill_description())
            self.assertNotIn("alpha", read_skill_description())

    def test_bad_existing_skill_reload_keeps_callable_lkg_and_repair_recovers(self):
        from qcopilots_common.mcp_http import ToolError
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="alpha",
                body="# Original Alpha\n",
                resources={"references/state.txt": "original resource\n"},
            )
            instructions_path = skill_dir / "SKILL.md"
            resource_path = skill_dir / "references" / "state.txt"
            service = SkillService([root], refresh_interval_seconds=0)
            original_registry = service._registry
            original_skill = original_registry.get("alpha")

            def assert_callable_lkg():
                self.assertIs(service._registry, original_registry)
                self.assertIs(
                    service._registry.get("alpha"),
                    original_skill,
                )
                tools = {tool.name: tool for tool in service.tools()}
                prompts = {prompt.name: prompt for prompt in service.prompts()}
                self.assertIn("alpha", tools)
                self.assertIn("alpha", prompts)
                listing = tools["list_skills"].handler({})
                self.assertEqual(
                    [entry["slug"] for entry in listing["skills"]],
                    ["alpha"],
                )
                self.assertEqual(
                    tools["alpha"].handler({"task": "Read cached alpha"})[
                        "instructions"
                    ],
                    "# Original Alpha\n",
                )
                read_result = tools["read_skill"].handler({"slug": "alpha"})
                self.assertEqual(read_result["content"], "# Original Alpha\n")
                self.assertIn("# Original Alpha", read_result["skill_content"])
                prompt_result = prompts["alpha"].handler({"task": "Read cached alpha"})
                prompt_text = prompt_result["messages"][0]["content"]["text"]
                self.assertIn("# Original Alpha", prompt_text)
                self.assertNotIn("Broken Alpha", prompt_text)
                reload_failures = [
                    diagnostic
                    for diagnostic in listing["diagnostics"]
                    if diagnostic["code"] == "skills.reload.failed"
                ]
                self.assertEqual(len(reload_failures), 1)
                return tools

            instructions_path.write_text(
                "---\nname: alpha\n---\n# Broken Alpha\n",
                encoding="utf-8",
            )
            resource_path.write_text("changed resource\n", encoding="utf-8")

            lkg_tools = assert_callable_lkg()
            with self.assertRaises(ToolError):
                lkg_tools["read_skill_resource"].handler(
                    {"slug": "alpha", "resource": "references/state.txt"}
                )
            resource_path.unlink()
            with self.assertRaises(ToolError):
                lkg_tools["read_skill_resource"].handler(
                    {"slug": "alpha", "resource": "references/state.txt"}
                )

            instructions_path.write_text(
                self._canonical_document("alpha", body="# Repaired Alpha\n"),
                encoding="utf-8",
            )
            resource_path.write_text("repaired resource\n", encoding="utf-8")
            repaired_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                repaired_tools["alpha"].handler({"task": "Read repaired alpha"})[
                    "instructions"
                ],
                "# Repaired Alpha\n",
            )
            self.assertNotIn(
                "skills.reload.failed",
                {
                    diagnostic["code"]
                    for diagnostic in repaired_tools["list_skills"].handler({})[
                        "diagnostics"
                    ]
                },
            )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir(parents=True)
            archive_path = root / "archive-alpha.skill"

            def write_archive(body):
                with zipfile.ZipFile(
                    archive_path,
                    "w",
                    compression=zipfile.ZIP_STORED,
                ) as archive:
                    archive.writestr(
                        "archive-alpha/SKILL.md",
                        self._canonical_document("archive-alpha", body=body),
                    )

            write_archive("# Original Archive Alpha\n")
            service = SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            archive_path.write_bytes(b"not a zip archive")
            lkg_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                lkg_tools["archive-alpha"].handler({"task": "Use archive LKG"})[
                    "instructions"
                ],
                "# Original Archive Alpha\n",
            )
            self.assertIn(
                "skills.reload.failed",
                {
                    diagnostic["code"]
                    for diagnostic in lkg_tools["list_skills"].handler({})[
                        "diagnostics"
                    ]
                },
            )

            write_archive("# Repaired Archive Alpha\n")
            repaired_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                repaired_tools["archive-alpha"].handler(
                    {"task": "Use repaired archive"}
                )["instructions"],
                "# Repaired Archive Alpha\n",
            )
            self.assertNotIn(
                "skills.reload.failed",
                {
                    diagnostic["code"]
                    for diagnostic in repaired_tools["list_skills"].handler({})[
                        "diagnostics"
                    ]
                },
            )

    def test_fingerprint_and_discovery_io_races_keep_lkg_but_programming_errors_escape(
        self,
    ):
        import qcopilots_common.skills as skills_module

        race_cases = (
            ("stat", "instructions", FileNotFoundError),
            ("is_dir", "directory", OSError),
            ("is_file", "instructions", OSError),
        )
        for method_name, target_kind, error_type in race_cases:
            with self.subTest(method=method_name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                skill_dir = self._write_skill(
                    root,
                    name="race-skill",
                    body="# Original\n",
                )
                instructions = (skill_dir / "SKILL.md").resolve()
                service = skills_module.SkillService(
                    [root],
                    refresh_interval_seconds=0,
                )
                original_registry = service._registry
                original_skill = original_registry.get("race-skill")
                instructions.write_text(
                    self._canonical_document("race-skill", body="# Candidate\n"),
                    encoding="utf-8",
                )
                target = instructions if target_kind == "instructions" else skill_dir
                original = getattr(Path, method_name)

                def raise_for_target(path, *args, **kwargs):
                    if path == target:
                        raise error_type(f"race-{method_name}")
                    return original(path, *args, **kwargs)

                with mock.patch.object(
                    Path,
                    method_name,
                    autospec=True,
                    side_effect=raise_for_target,
                ):
                    service.ensure_current()
                    self.assertIs(service._registry, original_registry)
                    self.assertIs(
                        service._registry.get("race-skill"),
                        original_skill,
                    )
                    reload_diagnostics = [
                        diagnostic
                        for diagnostic in service._reload_diagnostics
                        if diagnostic.code == "skills.reload.failed"
                    ]
                    self.assertEqual(len(reload_diagnostics), 1)
                    diagnostic = reload_diagnostics[0]
                    self.assertEqual(diagnostic.code, "skills.reload.failed")
                    self.assertEqual(diagnostic.severity, "error")
                    self.assertEqual(diagnostic.source, str(root.resolve()))
                    self.assertIn(
                        "last-known-good snapshot remains active",
                        diagnostic.message,
                    )
                    self.assertIn(f"race-{method_name}", diagnostic.message)
                    if method_name in {"is_dir", "is_file"}:
                        self.assertIn(
                            "skills.scan.entry_unreadable",
                            {
                                item.code
                                for item in service._reload_diagnostics
                            },
                        )
                with self.assertRaises(skills_module.SkillError) as stale_source:
                    original_skill.read_body()
                self.assertEqual(
                    str(stale_source.exception),
                    f"Bound skill source '{instructions}' changed since discovery",
                )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(root, name="programming-error")
            service = skills_module.SkillService([root])
            with mock.patch.object(
                skills_module,
                "_build_roots_fingerprint",
                side_effect=RuntimeError("programming defect"),
            ):
                with self.assertRaisesRegex(RuntimeError, "programming defect"):
                    service.ensure_current()
            self.assertEqual(service._reload_diagnostics, ())
            self.assertEqual(
                [skill.slug for skill in service._registry.skills],
                ["programming-error"],
            )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir()
            registry = skills_module.SkillRegistry(root)
            with mock.patch.object(
                registry,
                "_scan_directory",
                side_effect=OSError("disappeared during discovery"),
            ):
                with self.assertRaisesRegex(
                    skills_module.SkillError,
                    "Skill scan cannot inspect",
                ):
                    registry.load()
            self._assert_diagnostic(
                registry,
                code="skills.scan.filesystem_changed",
                source=root.resolve(),
                message="disappeared during discovery",
            )

    def test_link_and_snapshot_io_failures_preserve_lkg_but_real_deletion_commits(self):
        import qcopilots_common.skills as skills_module

        link_error_cases = (
            ("is_symlink", PermissionError),
            ("is_junction", OSError),
        )
        for method_name, error_type in link_error_cases:
            with self.subTest(method=method_name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                skill_dir = self._write_skill(
                    root,
                    name="link-io",
                    body="# Original link snapshot\n",
                )
                instructions = skill_dir / "SKILL.md"
                service = skills_module.SkillService([root])
                original_registry = service._registry
                original_skill = original_registry.get("link-io")
                instructions.write_text(
                    self._canonical_document(
                        "link-io",
                        body="# Candidate link snapshot\n",
                    ),
                    encoding="utf-8",
                )
                original_method = getattr(Path, method_name, lambda path: False)

                def raise_for_skill_directory(path):
                    if path == skill_dir:
                        raise error_type(f"{method_name}-metadata-race")
                    return original_method(path)

                with mock.patch.object(
                    Path,
                    method_name,
                    new=raise_for_skill_directory,
                    create=method_name == "is_junction",
                ):
                    service.ensure_current()

                self.assertIs(service._registry, original_registry)
                self.assertIs(service._registry.get("link-io"), original_skill)
                with self.assertRaises(skills_module.SkillError) as stale_source:
                    original_skill.read_body()
                self.assertEqual(
                    str(stale_source.exception),
                    f"Bound skill source '{instructions}' changed since discovery",
                )
                filesystem_diagnostics = [
                    diagnostic
                    for diagnostic in service._reload_diagnostics
                    if diagnostic.code == "skills.scan.filesystem_changed"
                ]
                self.assertEqual(len(filesystem_diagnostics), 1)
                self.assertEqual(filesystem_diagnostics[0].severity, "error")
                self.assertEqual(filesystem_diagnostics[0].source, str(skill_dir))
                self.assertIn(
                    "Link or junction metadata could not be inspected during the skill scan.",
                    filesystem_diagnostics[0].message,
                )
                self.assertIn(
                    f"{method_name}-metadata-race",
                    filesystem_diagnostics[0].message,
                )
                reload_diagnostics = [
                    diagnostic
                    for diagnostic in service._reload_diagnostics
                    if diagnostic.code == "skills.reload.failed"
                ]
                self.assertEqual(len(reload_diagnostics), 1)
                self.assertIn(
                    "last-known-good snapshot remains active",
                    reload_diagnostics[0].message,
                )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(root, name="registry-link-io")
            registry = skills_module.SkillRegistry(root)
            original_is_junction = getattr(Path, "is_junction", lambda path: False)

            def fail_registry_junction_check(path):
                if path == skill_dir:
                    raise OSError("registry-junction-race")
                return original_is_junction(path)

            with mock.patch.object(
                Path,
                "is_junction",
                new=fail_registry_junction_check,
                create=True,
            ):
                with self.assertRaises(skills_module.SkillError) as error_context:
                    registry.load()
            self.assertIn(
                "Link or junction metadata could not be inspected during the skill scan.",
                str(error_context.exception),
            )
            self.assertIn(
                "registry-junction-race",
                str(error_context.exception),
            )
            diagnostic = self._assert_diagnostic(
                registry,
                code="skills.scan.filesystem_changed",
                source=skill_dir,
                message=(
                    "Link or junction metadata could not be inspected during the skill scan."
                ),
            )
            self.assertIn("registry-junction-race", diagnostic.message)

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="link-programming-error",
                body="# Original programming snapshot\n",
            )
            instructions = skill_dir / "SKILL.md"
            service = skills_module.SkillService([root])
            original_registry = service._registry
            original_skill = original_registry.get("link-programming-error")
            instructions.write_text(
                self._canonical_document(
                    "link-programming-error",
                    body="# Candidate programming snapshot\n",
                ),
                encoding="utf-8",
            )
            original_is_symlink = Path.is_symlink

            def raise_programming_error(path):
                if path == skill_dir:
                    raise RuntimeError("link detector programming defect")
                return original_is_symlink(path)

            with mock.patch.object(
                Path,
                "is_symlink",
                new=raise_programming_error,
            ):
                with self.assertRaisesRegex(
                    RuntimeError,
                    "link detector programming defect",
                ):
                    service.ensure_current()
            self.assertEqual(service._reload_diagnostics, ())
            self.assertIs(service._registry, original_registry)
            self.assertIs(
                service._registry.get("link-programming-error"),
                original_skill,
            )
            with self.assertRaises(skills_module.SkillError) as stale_source:
                original_skill.read_body()
            self.assertEqual(
                str(stale_source.exception),
                f"Bound skill source '{instructions}' changed since discovery",
            )

        for error_type in (PermissionError, OSError):
            with self.subTest(
                snapshot_stat=error_type.__name__
            ), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                skill_dir = self._write_skill(
                    root,
                    name="snapshot-io",
                    body="# Original snapshot\n",
                )
                instructions = (skill_dir / "SKILL.md").resolve()
                service = skills_module.SkillService(
                    [root],
                    refresh_interval_seconds=0,
                )
                original_registry = service._registry
                original_skill = original_registry.get("snapshot-io")
                instructions.write_text(
                    "---\nname: snapshot-io\n---\n# Invalid candidate\n",
                    encoding="utf-8",
                )
                original_load = skills_module.SkillRegistry.load
                original_stat = Path.stat
                snapshot_validation = {"active": False}

                def track_candidate_load(registry):
                    result = original_load(registry)
                    if registry is not service._registry:
                        snapshot_validation["active"] = True
                    return result

                def fail_snapshot_stat(path, *args, **kwargs):
                    if snapshot_validation["active"] and path == instructions:
                        raise error_type("snapshot-stat-race")
                    return original_stat(path, *args, **kwargs)

                with mock.patch.object(
                    skills_module.SkillRegistry,
                    "load",
                    autospec=True,
                    side_effect=track_candidate_load,
                ), mock.patch.object(
                    Path,
                    "stat",
                    autospec=True,
                    side_effect=fail_snapshot_stat,
                ):
                    service.ensure_current()

                self.assertIs(service._registry, original_registry)
                self.assertIs(
                    service._registry.get("snapshot-io"),
                    original_skill,
                )
                with self.assertRaises(skills_module.SkillError) as stale_source:
                    original_skill.read_body()
                self.assertEqual(
                    str(stale_source.exception),
                    f"Bound skill source '{instructions}' changed since discovery",
                )
                reload_diagnostics = [
                    diagnostic
                    for diagnostic in service._reload_diagnostics
                    if diagnostic.code == "skills.reload.failed"
                ]
                self.assertEqual(len(reload_diagnostics), 1)
                self.assertEqual(reload_diagnostics[0].source, str(root.resolve()))
                self.assertIn(
                    "Skill scan cannot inspect",
                    reload_diagnostics[0].message,
                )
                self.assertEqual(
                    reload_diagnostics[0].message.count("snapshot-stat-race"),
                    1,
                )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(root, name="deleted-skill")
            service = skills_module.SkillService([root])
            instructions = skill_dir / "SKILL.md"
            self.assertTrue(instructions.is_file())
            instructions.unlink()

            service.ensure_current()

            self.assertNotIn(
                "deleted-skill",
                [skill.slug for skill in service._registry.skills],
            )
            self.assertNotIn(
                "skills.reload.failed",
                {diagnostic.code for diagnostic in service._reload_diagnostics},
            )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir(parents=True)
            archive_path = root / "deleted-archive.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr(
                    "SKILL.md",
                    self._canonical_document("deleted-archive"),
                )
            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
            )
            self.assertIn(
                "deleted-archive",
                [skill.slug for skill in service._registry.skills],
            )
            archive_path.unlink()

            service.ensure_current()

            self.assertNotIn(
                "deleted-archive",
                [skill.slug for skill in service._registry.skills],
            )
            self.assertNotIn(
                "skills.reload.failed",
                {diagnostic.code for diagnostic in service._reload_diagnostics},
            )

    def test_large_skill_resources_are_rejected_before_reading(self):
        import qcopilots_common.skills as skills_module
        from qcopilots_common.constants import MAX_FILE_READ_BYTES
        from qcopilots_common.mcp_http import ToolError
        from qcopilots_common.skills import (
            SKILL_VALIDATION_COMPATIBLE,
            SkillRegistry,
            SkillService,
        )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            large_payload = b"x" * (MAX_FILE_READ_BYTES + 1)
            skill_dir = self._write_skill(
                root,
                name="big-dir",
                resources={"assets/big.bin": large_payload},
            )
            service = SkillService([root])
            resource = next(
                resource
                for resource in service.resources()
                if resource.name == "big-dir/assets/big.bin"
            )
            with mock.patch.object(
                skills_module.os,
                "open",
                side_effect=AssertionError(
                    "Known oversized resource must not be opened"
                ),
            ) as resource_open:
                with self.assertRaises(skills_module.SkillError) as direct_error:
                    service._registry.get("big-dir").open_bytes("assets/big.bin")
                self.assertEqual(
                    str(direct_error.exception),
                    f"Resource 'assets/big.bin' is too large "
                    f"({MAX_FILE_READ_BYTES + 1} bytes, max {MAX_FILE_READ_BYTES})",
                )
                with self.assertRaisesRegex(
                    ToolError,
                    r"^The requested skill resource is unavailable\.$",
                ):
                    resource.handler()
            resource_open.assert_not_called()
            self.assertEqual(
                (skill_dir / "assets" / "big.bin").stat().st_size,
                MAX_FILE_READ_BYTES + 1,
            )

        class TrackingHandle:
            def __init__(self, handle, forged=None):
                self.handle = handle
                self.forged = forged
                self.read_sizes = []

            @property
            def closed(self):
                return self.handle.closed

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                self.close()
                return False

            def __getattr__(self, name):
                return getattr(self.handle, name)

            def close(self):
                self.handle.close()

            def fileno(self):
                return self.handle.fileno()

            def read(self, size=-1):
                self.read_sizes.append(size)
                data = self.handle.read(size)
                return self.forged if self.forged is not None else data

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="bounded-read",
                resources={
                    "assets/exact.bin": b"ABCD",
                    "assets/forged.bin": b"WXYZ",
                },
            )
            skill = SkillService([root])._registry.get("bounded-read")
            real_open = skills_module.os.open
            real_fdopen = skills_module.os.fdopen

            exact_handles = []

            def track_exact_fdopen(descriptor, *args, **kwargs):
                handle = TrackingHandle(real_fdopen(descriptor, *args, **kwargs))
                exact_handles.append(handle)
                return handle

            with (
                mock.patch.object(skills_module, "MAX_FILE_READ_BYTES", 4),
                mock.patch.object(
                    skills_module.os,
                    "open",
                    wraps=real_open,
                ) as exact_open,
                mock.patch.object(
                    skills_module.os,
                    "fdopen",
                    side_effect=track_exact_fdopen,
                ),
            ):
                self.assertEqual(skill.open_bytes("assets/exact.bin"), b"ABCD")
            self.assertEqual(exact_open.call_count, 1)
            self.assertEqual(
                exact_open.call_args.args[0],
                skill_dir / "assets" / "exact.bin",
            )
            self.assertEqual([handle.read_sizes for handle in exact_handles], [[5]])
            self.assertTrue(all(handle.closed for handle in exact_handles))

            forged_handles = []

            def forge_fdopen(descriptor, *args, **kwargs):
                handle = TrackingHandle(
                    real_fdopen(descriptor, *args, **kwargs),
                    forged=b"12345",
                )
                forged_handles.append(handle)
                return handle

            with (
                mock.patch.object(skills_module, "MAX_FILE_READ_BYTES", 4),
                mock.patch.object(
                    skills_module.os,
                    "open",
                    wraps=real_open,
                ) as forged_open,
                mock.patch.object(
                    skills_module.os,
                    "fdopen",
                    side_effect=forge_fdopen,
                ),
            ):
                with self.assertRaises(skills_module.SkillError) as forged_error:
                    skill.open_bytes("assets/forged.bin")
            self.assertEqual(
                str(forged_error.exception),
                "Resource 'assets/forged.bin' is too large (max 4 bytes)",
            )
            self.assertEqual(forged_open.call_count, 1)
            self.assertEqual(
                forged_open.call_args.args[0],
                skill_dir / "assets" / "forged.bin",
            )
            self.assertEqual([handle.read_sizes for handle in forged_handles], [[5]])
            self.assertTrue(all(handle.closed for handle in forged_handles))

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_raw_skill(
                root,
                "too-big-dir",
                "---\nname: too-big-dir\ndescription: Too big dir\n---\n"
                + ("x" * MAX_FILE_READ_BYTES),
            )
            instructions = skill_dir / "SKILL.md"
            real_read_text = Path.read_text
            read_paths = []

            def guarded_read_text(path, *args, **kwargs):
                read_paths.append(path)
                if path == instructions:
                    raise AssertionError("Oversized instructions were read")
                return real_read_text(path, *args, **kwargs)

            registry = SkillRegistry(root)
            with mock.patch.object(
                Path,
                "read_text",
                autospec=True,
                side_effect=guarded_read_text,
            ):
                registry.load()
            self.assertNotIn(instructions, read_paths)
            self._assert_diagnostic(
                registry,
                code="skills.skill.invalid",
                source=instructions,
                message=f"Instructions exceed {MAX_FILE_READ_BYTES} bytes.",
                exact_message=True,
            )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir(parents=True)
            archive_path = root / "archive-big.skill"
            member_name = "archive-big/assets/big.bin"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr(
                    "archive-big/SKILL.md",
                    "---\nname: archive-big\ndescription: Too big archive\n---\nBody\n",
                )
                archive.writestr(member_name, b"x" * (MAX_FILE_READ_BYTES + 1))

            real_archive_read = zipfile.ZipFile.read
            read_members = []

            def guarded_archive_read(archive, name, *args, **kwargs):
                read_members.append(
                    name.filename if isinstance(name, zipfile.ZipInfo) else name
                )
                return real_archive_read(archive, name, *args, **kwargs)

            registry = SkillRegistry(
                root,
                validation_mode=SKILL_VALIDATION_COMPATIBLE,
            )
            with mock.patch.object(
                zipfile.ZipFile,
                "read",
                autospec=True,
                side_effect=guarded_archive_read,
            ):
                registry.load()
            self.assertEqual(read_members, [])
            self.assertEqual(registry.skills, ())
            self._assert_diagnostic(
                registry,
                code="skills.archive.invalid",
                source=archive_path,
                message=(
                    f"Archive member '{member_name}' exceeds "
                    f"{MAX_FILE_READ_BYTES} bytes."
                ),
                exact_message=True,
            )

    def test_bound_resource_reads_recheck_identity_after_read_for_files_and_archives(
        self,
    ):
        import qcopilots_common.skills as skills_module
        from qcopilots_common.mcp_http import McpJsonRpcServer, ToolError

        class TrackingHandle:
            def __init__(self, handle):
                self.handle = handle
                self.read_sizes = []

            @property
            def closed(self):
                return self.handle.closed

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                self.close()
                return False

            def __getattr__(self, name):
                return getattr(self.handle, name)

            def close(self):
                self.handle.close()

            def fileno(self):
                return self.handle.fileno()

            def read(self, size=-1):
                self.read_sizes.append(size)
                return self.handle.read(size)

        class ChangedSizeStat:
            def __init__(self, path_stat):
                self.path_stat = path_stat

            @property
            def st_size(self):
                return self.path_stat.st_size + 1

            def __getattr__(self, name):
                return getattr(self.path_stat, name)

        real_open = skills_module.os.open
        real_fdopen = skills_module.os.fdopen
        real_fstat = skills_module.os.fstat

        def run_with_post_read_change(
            callback,
            expected_path,
            expected_read_size=None,
        ):
            handles = []
            opened_paths = []

            def tracked_open(path, flags):
                opened_paths.append(Path(path))
                return real_open(path, flags)

            def tracked_fdopen(descriptor, *args, **kwargs):
                handle = TrackingHandle(real_fdopen(descriptor, *args, **kwargs))
                handles.append(handle)
                return handle

            def changed_after_read(descriptor):
                path_stat = real_fstat(descriptor)
                if any(handle.read_sizes for handle in handles):
                    return ChangedSizeStat(path_stat)
                return path_stat

            with (
                mock.patch.object(
                    skills_module.os,
                    "open",
                    side_effect=tracked_open,
                ) as tracked_open_call,
                mock.patch.object(
                    skills_module.os,
                    "fdopen",
                    side_effect=tracked_fdopen,
                ),
                mock.patch.object(
                    skills_module.os,
                    "fstat",
                    side_effect=changed_after_read,
                ) as tracked_fstat,
            ):
                try:
                    return callback()
                finally:
                    self.assertEqual(tracked_open_call.call_count, 1)
                    self.assertEqual(opened_paths, [expected_path])
                    self.assertGreaterEqual(tracked_fstat.call_count, 2)
                    self.assertEqual(len(handles), 1)
                    self.assertTrue(handles[0].read_sizes)
                    if expected_read_size is not None:
                        self.assertEqual(
                            handles[0].read_sizes,
                            [expected_read_size],
                        )
                    self.assertTrue(handles[0].closed)

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            directory = self._write_skill(
                root,
                name="post-read-directory",
                resources={"references/guide.txt": "directory bytes\n"},
            )
            archive_path = root / "post-read-archive.skill"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr(
                    "post-read-archive/SKILL.md",
                    self._canonical_document("post-read-archive"),
                )
                archive.writestr(
                    "post-read-archive/references/guide.txt",
                    "archive bytes\n",
                )

            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=3600,
            )
            directory_skill = service._registry.get("post-read-directory")
            archive_skill = service._registry.get("post-read-archive")
            resources = {resource.name: resource for resource in service._resources}
            directory_resource = resources[
                "post-read-directory/references/guide.txt"
            ]
            archive_resource = resources[
                "post-read-archive/references/guide.txt"
            ]
            management_tool = {
                tool.name: tool for tool in service._tools
            }["read_skill_resource"]
            server = McpJsonRpcServer(
                server_name="qcopilots-post-read-test",
                server_version="1.0.0",
                tools=lambda: list(service._tools),
                resources=lambda: list(service._resources),
            )
            directory_resource_path = directory / "references" / "guide.txt"
            directory_identity_error = (
                f"Bound skill source '{directory_resource_path}' changed since "
                "discovery"
            )

            with self.assertRaises(skills_module.SkillError) as direct_error:
                run_with_post_read_change(
                    lambda: directory_skill.open_bytes("references/guide.txt"),
                    directory_resource_path,
                    skills_module.MAX_FILE_READ_BYTES + 1,
                )
            self.assertEqual(str(direct_error.exception), directory_identity_error)

            with self.assertRaises(ToolError) as resource_error:
                run_with_post_read_change(
                    directory_resource.handler,
                    directory_resource_path,
                    skills_module.MAX_FILE_READ_BYTES + 1,
                )
            self.assertEqual(
                str(resource_error.exception),
                "The requested skill resource is unavailable.",
            )

            with self.assertRaises(ToolError) as management_error:
                run_with_post_read_change(
                    lambda: management_tool.handler(
                        {
                            "slug": "post-read-directory",
                            "resource": "references/guide.txt",
                        }
                    ),
                    directory_resource_path,
                    skills_module.MAX_FILE_READ_BYTES + 1,
                )
            self.assertEqual(
                str(management_error.exception),
                "The requested skill resource is unavailable.",
            )

            response = run_with_post_read_change(
                lambda: server.handle_json_rpc(
                    {
                        "jsonrpc": "2.0",
                        "id": 71,
                        "method": "tools/call",
                        "params": {
                            "name": "read_skill_resource",
                            "arguments": {
                                "slug": "post-read-directory",
                                "resource": "references/guide.txt",
                            },
                        },
                    }
                ),
                directory_resource_path,
                skills_module.MAX_FILE_READ_BYTES + 1,
            )
            self.assertNotIn("error", response)
            self.assertIs(response["result"]["isError"], True)
            self.assertEqual(
                response["result"]["content"],
                [
                    {
                        "type": "text",
                        "text": "The requested skill resource is unavailable.",
                    }
                ],
            )

            archive_identity_error = (
                f"Bound skill source '{archive_path}' changed since discovery"
            )
            with self.assertRaises(skills_module.SkillError) as archive_error:
                run_with_post_read_change(
                    lambda: archive_skill.open_bytes("references/guide.txt"),
                    archive_path,
                )
            self.assertEqual(str(archive_error.exception), archive_identity_error)

            with self.assertRaises(ToolError) as archive_resource_error:
                run_with_post_read_change(
                    archive_resource.handler,
                    archive_path,
                )
            self.assertEqual(
                str(archive_resource_error.exception),
                "The requested skill resource is unavailable.",
            )

            with self.assertRaises(ToolError) as archive_management_error:
                run_with_post_read_change(
                    lambda: management_tool.handler(
                        {
                            "slug": "post-read-archive",
                            "resource": "references/guide.txt",
                        }
                    ),
                    archive_path,
                )
            self.assertEqual(
                str(archive_management_error.exception),
                "The requested skill resource is unavailable.",
            )

            archive_response = run_with_post_read_change(
                lambda: server.handle_json_rpc(
                    {
                        "jsonrpc": "2.0",
                        "id": 72,
                        "method": "tools/call",
                        "params": {
                            "name": "read_skill_resource",
                            "arguments": {
                                "slug": "post-read-archive",
                                "resource": "references/guide.txt",
                            },
                        },
                    }
                ),
                archive_path,
            )
            self.assertNotIn("error", archive_response)
            self.assertIs(archive_response["result"]["isError"], True)
            self.assertEqual(
                archive_response["result"]["content"],
                [
                    {
                        "type": "text",
                        "text": "The requested skill resource is unavailable.",
                    }
                ],
            )

    def test_bound_resource_reads_recheck_path_after_stable_handle_read(self):
        import stat

        import qcopilots_common.skills as skills_module
        from qcopilots_common.mcp_http import McpJsonRpcServer, ToolError

        class TrackingHandle:
            def __init__(self, handle):
                self.handle = handle
                self.read_sizes = []

            @property
            def closed(self):
                return self.handle.closed

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                self.close()
                return False

            def __getattr__(self, name):
                return getattr(self.handle, name)

            def close(self):
                self.handle.close()

            def fileno(self):
                return self.handle.fileno()

            def read(self, size=-1):
                self.read_sizes.append(size)
                return self.handle.read(size)

        class ReparsePathStat:
            def __init__(self, path_stat, reparse_attribute):
                self.path_stat = path_stat
                self.reparse_attribute = reparse_attribute

            @property
            def st_file_attributes(self):
                return (
                    getattr(self.path_stat, "st_file_attributes", 0)
                    | self.reparse_attribute
                )

            def __getattr__(self, name):
                return getattr(self.path_stat, name)

        class ChangedContentPathStat:
            def __init__(self, path_stat):
                self.path_stat = path_stat

            @property
            def st_size(self):
                return self.path_stat.st_size + 1

            def __getattr__(self, name):
                return getattr(self.path_stat, name)

        reparse_attribute = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
        self.assertNotEqual(reparse_attribute, 0)
        real_open = skills_module.os.open
        real_fdopen = skills_module.os.fdopen
        real_fstat = skills_module.os.fstat
        real_path_stat = Path.stat

        def stat_identity(path_stat):
            return (
                path_stat.st_dev,
                path_stat.st_ino,
                stat.S_IFMT(path_stat.st_mode),
                getattr(path_stat, "st_file_attributes", 0)
                & reparse_attribute,
                path_stat.st_size,
                path_stat.st_mtime_ns,
                path_stat.st_ctime_ns,
            )

        def run_with_post_read_path_change(
            callback,
            expected_path,
            mutation,
            minimum_post_read_stats,
            expected_read_size=None,
        ):
            if mutation not in {"stable", "reparse", "identity"}:
                raise AssertionError(f"Unsupported path mutation: {mutation}")
            handles = []
            opened_paths = []
            fstat_identities = []
            post_read_target_stats = []

            def tracked_open(path, flags):
                opened_paths.append(Path(path))
                return real_open(path, flags)

            def tracked_fdopen(descriptor, *args, **kwargs):
                handle = TrackingHandle(real_fdopen(descriptor, *args, **kwargs))
                handles.append(handle)
                return handle

            def stable_fstat(descriptor):
                path_stat = real_fstat(descriptor)
                fstat_identities.append(stat_identity(path_stat))
                return path_stat

            def changed_path_after_read(path, *args, **kwargs):
                path_stat = real_path_stat(path, *args, **kwargs)
                if Path(path) != expected_path or not any(
                    handle.read_sizes for handle in handles
                ):
                    return path_stat
                post_read_target_stats.append(Path(path))
                if mutation == "stable":
                    return path_stat
                if mutation == "reparse":
                    return ReparsePathStat(path_stat, reparse_attribute)
                return ChangedContentPathStat(path_stat)

            with (
                mock.patch.object(
                    skills_module.os,
                    "open",
                    side_effect=tracked_open,
                ) as tracked_open_call,
                mock.patch.object(
                    skills_module.os,
                    "fdopen",
                    side_effect=tracked_fdopen,
                ),
                mock.patch.object(
                    skills_module.os,
                    "fstat",
                    side_effect=stable_fstat,
                ) as tracked_fstat,
                mock.patch.object(
                    Path,
                    "stat",
                    autospec=True,
                    side_effect=changed_path_after_read,
                ),
            ):
                try:
                    return callback()
                finally:
                    self.assertEqual(tracked_open_call.call_count, 1)
                    self.assertEqual(opened_paths, [expected_path])
                    self.assertGreaterEqual(tracked_fstat.call_count, 2)
                    self.assertEqual(len(fstat_identities), tracked_fstat.call_count)
                    self.assertEqual(
                        set(fstat_identities),
                        {fstat_identities[0]},
                    )
                    self.assertEqual(len(handles), 1)
                    self.assertTrue(handles[0].read_sizes)
                    if expected_read_size is not None:
                        self.assertEqual(
                            handles[0].read_sizes,
                            [expected_read_size],
                        )
                    self.assertGreaterEqual(
                        len(post_read_target_stats),
                        minimum_post_read_stats,
                    )
                    self.assertEqual(
                        set(post_read_target_stats),
                        {expected_path},
                    )
                    self.assertTrue(handles[0].closed)

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            directory = self._write_skill(
                root,
                name="path-recheck-directory",
                resources={"references/guide.txt": "directory path bytes\n"},
            )
            archive_path = root / "path-recheck-archive.skill"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr(
                    "path-recheck-archive/SKILL.md",
                    self._canonical_document("path-recheck-archive"),
                )
                archive.writestr(
                    "path-recheck-archive/references/guide.txt",
                    "archive path bytes\n",
                )

            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=3600,
            )
            directory_skill = service._registry.get("path-recheck-directory")
            archive_skill = service._registry.get("path-recheck-archive")
            resources = {resource.name: resource for resource in service._resources}
            directory_resource = resources[
                "path-recheck-directory/references/guide.txt"
            ]
            archive_resource = resources[
                "path-recheck-archive/references/guide.txt"
            ]
            management_tool = {
                tool.name: tool for tool in service._tools
            }["read_skill_resource"]
            server = McpJsonRpcServer(
                server_name="qcopilots-path-recheck-test",
                server_version="1.0.0",
                tools=lambda: list(service._tools),
                resources=lambda: list(service._resources),
            )

            def assert_four_layer_failure(
                skill,
                resource,
                slug,
                target,
                mutation,
                direct_message,
                rpc_id,
                minimum_post_read_stats,
                expected_read_size=None,
            ):
                with self.assertRaises(skills_module.SkillError) as direct_error:
                    run_with_post_read_path_change(
                        lambda: skill.open_bytes("references/guide.txt"),
                        target,
                        mutation,
                        minimum_post_read_stats,
                        expected_read_size,
                    )
                self.assertEqual(str(direct_error.exception), direct_message)

                with self.assertRaises(ToolError) as resource_error:
                    run_with_post_read_path_change(
                        resource.handler,
                        target,
                        mutation,
                        minimum_post_read_stats,
                        expected_read_size,
                    )
                self.assertEqual(
                    str(resource_error.exception),
                    "The requested skill resource is unavailable.",
                )

                with self.assertRaises(ToolError) as management_error:
                    run_with_post_read_path_change(
                        lambda: management_tool.handler(
                            {
                                "slug": slug,
                                "resource": "references/guide.txt",
                            }
                        ),
                        target,
                        mutation,
                        minimum_post_read_stats,
                        expected_read_size,
                    )
                self.assertEqual(
                    str(management_error.exception),
                    "The requested skill resource is unavailable.",
                )

                response = run_with_post_read_path_change(
                    lambda: server.handle_json_rpc(
                        {
                            "jsonrpc": "2.0",
                            "id": rpc_id,
                            "method": "tools/call",
                            "params": {
                                "name": "read_skill_resource",
                                "arguments": {
                                    "slug": slug,
                                    "resource": "references/guide.txt",
                                },
                            },
                        }
                    ),
                    target,
                    mutation,
                    minimum_post_read_stats,
                    expected_read_size,
                )
                self.assertNotIn("error", response)
                self.assertIs(response["result"]["isError"], True)
                self.assertEqual(
                    response["result"]["content"],
                    [
                        {
                            "type": "text",
                            "text": (
                                "The requested skill resource is unavailable."
                            ),
                        }
                    ],
                )

            directory_target = directory / "references" / "guide.txt"
            self.assertEqual(
                run_with_post_read_path_change(
                    lambda: directory_skill.open_bytes("references/guide.txt"),
                    directory_target,
                    "stable",
                    2,
                    skills_module.MAX_FILE_READ_BYTES + 1,
                ),
                b"directory path bytes\n",
            )
            assert_four_layer_failure(
                directory_skill,
                directory_resource,
                "path-recheck-directory",
                directory_target,
                "reparse",
                f"Bound skill source '{directory_target}' contains a symbolic "
                "link or junction",
                81,
                1,
                skills_module.MAX_FILE_READ_BYTES + 1,
            )

            # ZIP 的说明和 resource 共用 archive identity；这里保持句柄身份稳定，
            # 只让读取后的 archive 路径身份变化，所有入口都应统一失效。
            archive_identity_error = (
                f"Bound skill source '{archive_path}' changed since discovery"
            )
            self.assertEqual(
                run_with_post_read_path_change(
                    lambda: archive_skill.open_bytes("references/guide.txt"),
                    archive_path,
                    "stable",
                    2,
                ),
                b"archive path bytes\n",
            )
            assert_four_layer_failure(
                archive_skill,
                archive_resource,
                "path-recheck-archive",
                archive_path,
                "identity",
                archive_identity_error,
                82,
                2,
            )

    def test_loaded_sources_reject_directory_reparse_and_archive_replacement(self):
        import qcopilots_common.skills as skills_module
        from qcopilots_common.mcp_http import ToolError

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            directory = self._write_skill(
                root,
                name="bound-directory",
                body="# Bound directory\n",
                resources={"references/guide.txt": "trusted\n"},
            )
            archive_path = root / "bound-archive.skill"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr(
                    "bound-archive/SKILL.md",
                    self._canonical_document(
                        "bound-archive",
                        body="# Bound archive\n",
                    ),
                )
                archive.writestr(
                    "bound-archive/references/guide.txt",
                    "trusted archive data\n",
                )

            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=3600,
            )
            directory_skill = service._registry.get("bound-directory")
            archive_skill = service._registry.get("bound-archive")
            resources = {resource.name: resource for resource in service._resources}
            directory_resource = resources[
                "bound-directory/references/guide.txt"
            ]
            archive_resource = resources["bound-archive/references/guide.txt"]
            management_tool = {
                tool.name: tool for tool in service._tools
            }["read_skill_resource"]
            self.assertEqual(directory_resource.handler(), "trusted\n")
            self.assertEqual(archive_resource.handler(), "trusted archive data\n")

            original_directory = Path(tmp) / "original-bound-directory"
            directory.rename(original_directory)
            replacement_marker = "replacement directory data must not leak\n"
            self._write_skill(
                root,
                name="bound-directory",
                body="# Replacement directory\n",
                resources={
                    "references/guide.txt": replacement_marker * 4,
                },
            )
            directory_identity_error = (
                f"Bound skill source '{directory}' changed since discovery"
            )
            with (
                mock.patch.object(skills_module, "MAX_FILE_READ_BYTES", 16),
                mock.patch.object(
                    skills_module.os,
                    "open",
                    side_effect=AssertionError(
                        "Replacement resource must not be opened"
                    ),
                ) as bound_open,
            ):
                with self.assertRaises(skills_module.SkillError) as direct_error:
                    directory_skill.open_bytes("references/guide.txt")
            self.assertEqual(str(direct_error.exception), directory_identity_error)
            bound_open.assert_not_called()
            with self.assertRaises(skills_module.SkillError) as exists_error:
                directory_skill.exists("references/guide.txt")
            self.assertEqual(str(exists_error.exception), directory_identity_error)
            with self.assertRaises(ToolError) as resource_error:
                directory_resource.handler()
            self.assertEqual(
                str(resource_error.exception),
                "The requested skill resource is unavailable.",
            )
            with self.assertRaises(ToolError) as management_error:
                management_tool.handler(
                    {
                        "slug": "bound-directory",
                        "resource": "references/guide.txt",
                    }
                )
            self.assertEqual(
                str(management_error.exception),
                "The requested skill resource is unavailable.",
            )
            self.assertEqual(
                (directory / "references" / "guide.txt").read_text(
                    encoding="utf-8"
                ),
                replacement_marker * 4,
            )

            original_is_junction = getattr(Path, "is_junction", lambda path: False)

            def replacement_is_junction(path):
                if path == directory:
                    return True
                return original_is_junction(path)

            with mock.patch.object(
                Path,
                "is_junction",
                new=replacement_is_junction,
                create=True,
            ):
                with self.assertRaises(skills_module.SkillError) as reparse_error:
                    directory_skill.open_bytes("references/guide.txt")
            self.assertEqual(
                str(reparse_error.exception),
                f"Bound skill source '{directory}' contains a symbolic link "
                "or junction",
            )

            original_archive = Path(tmp) / "original-bound-archive.skill"
            archive_path.rename(original_archive)
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr(
                    "bound-archive/SKILL.md",
                    self._canonical_document(
                        "bound-archive",
                        body="# Replacement archive with a different size\n",
                    ),
                )
                archive.writestr(
                    "bound-archive/references/guide.txt",
                    "replacement archive data must not leak\n" * 4,
                )
            archive_identity_error = (
                f"Bound skill source '{archive_path}' changed since discovery"
            )
            with mock.patch.object(
                skills_module.os,
                "open",
                side_effect=AssertionError("Replacement archive must not be opened"),
            ) as archive_open:
                with self.assertRaises(
                    skills_module.SkillError
                ) as archive_direct_error:
                    archive_skill.open_bytes("references/guide.txt")
            self.assertEqual(
                str(archive_direct_error.exception),
                archive_identity_error,
            )
            archive_open.assert_not_called()
            with self.assertRaises(skills_module.SkillError) as archive_exists_error:
                archive_skill.exists("references/guide.txt")
            self.assertEqual(
                str(archive_exists_error.exception),
                archive_identity_error,
            )
            with self.assertRaises(ToolError) as archive_resource_error:
                archive_resource.handler()
            self.assertEqual(
                str(archive_resource_error.exception),
                "The requested skill resource is unavailable.",
            )

    def test_configured_skill_root_resolves_manifest_path_only(self):
        from qcopilots_common.skills import SkillError, configured_skill_roots

        with tempfile.TemporaryDirectory() as tmp:
            plugin_dir = Path(tmp) / "plugin"
            relative_root = plugin_dir / "Skills"
            absolute_root = Path(tmp) / "ExternalSkills"
            relative_root.mkdir(parents=True)
            absolute_root.mkdir()
            (plugin_dir / "qcopilots_service.json").write_text(
                json.dumps({"skillsRoot": str(absolute_root)}),
                encoding="utf-8",
            )

            self.assertEqual(
                configured_skill_roots(plugin_dir),
                [absolute_root.resolve()],
            )

            (plugin_dir / "qcopilots_service.json").write_text(
                json.dumps({"skillsRoot": "Skills"}),
                encoding="utf-8",
            )
            self.assertEqual(
                configured_skill_roots(plugin_dir),
                [relative_root.resolve()],
            )

            (plugin_dir / "qcopilots_service.json").write_text(
                json.dumps({"skills": {"roots": "Skills"}}),
                encoding="utf-8",
            )
            with self.assertRaises(SkillError) as nested_roots_error:
                configured_skill_roots(plugin_dir)
            self.assertIn("skills.roots", str(nested_roots_error.exception))
            self.assertIn("skillsRoot", str(nested_roots_error.exception))

            (plugin_dir / "qcopilots_service.json").write_text(
                "{not json",
                encoding="utf-8",
            )
            manifest_path = plugin_dir / "qcopilots_service.json"
            invalid_manifest = "{not json"
            with self.assertRaises(SkillError) as manifest_error:
                configured_skill_roots(plugin_dir)
            manifest_message = str(manifest_error.exception)
            self.assertIn(str(manifest_path), manifest_message)
            self.assertIn("json", manifest_message.lower())
            self.assertNotIn(invalid_manifest, manifest_message)
            manifest_path.unlink()

            env_root = Path(tmp) / "EnvSkills"
            codex_root = Path(tmp) / "CodexHome" / "skills"
            env_root.mkdir()
            codex_root.mkdir(parents=True)
            with mock.patch.dict(
                os.environ,
                {
                    "QCOPILOTS_SKILLS_ROOTS": str(env_root),
                    "CODEX_HOME": str(codex_root.parent),
                },
            ):
                self.assertEqual(
                    configured_skill_roots(plugin_dir),
                    [relative_root.resolve()],
                )

            cwd_root = Path(tmp) / "working-directory"
            self._write_skill(cwd_root / "Skills", name="cwd-only")
            with mock.patch.object(os, "getcwd", return_value=str(cwd_root)):
                self.assertEqual(configured_skill_roots(), [])

    def test_configured_root_is_not_augmented_by_external_sources(self):
        from qcopilots_common.skills import SkillRegistry, configured_skill_roots

        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            plugin_dir = base / "plugin"
            custom_root = plugin_dir / "CustomSkills"
            default_root = plugin_dir / "Skills"
            env_root = base / "EnvSkills"
            codex_home = base / "CodexHome"
            codex_root = codex_home / "skills"
            for root, label in (
                (custom_root, "manifest"),
                (env_root, "environment"),
                (codex_root, "codex"),
                (default_root, "default"),
            ):
                self._write_skill(root, name="shared-priority", body=f"# {label}\n")

            custom_duplicate = (
                str(custom_root).swapcase() if os.name == "nt" else str(custom_root)
            )
            (plugin_dir / "qcopilots_service.json").write_text(
                json.dumps({"skillsRoot": custom_duplicate}),
                encoding="utf-8",
            )
            with mock.patch.dict(
                os.environ,
                {
                    "QCOPILOTS_SKILLS_ROOTS": str(env_root),
                    "CODEX_HOME": str(codex_home),
                },
            ):
                roots = configured_skill_roots(plugin_dir)

            self.assertEqual(
                roots,
                [custom_root.resolve()],
            )
            registry = SkillRegistry(roots)
            registry.load()
            self.assertEqual(
                [skill.slug for skill in registry.skills],
                ["shared-priority"],
            )
            self.assertEqual(registry.get("shared-priority").body, "# manifest\n")
            collisions = [
                diagnostic
                for diagnostic in registry.diagnostics
                if diagnostic.code == "skills.skill.slug_collision"
            ]
            self.assertEqual(collisions, [])

    def test_cmake_installs_default_skills_directory(self):
        repo_root = Path(__file__).resolve().parents[3]
        cmake = (
            repo_root
            / "python"
            / "plugins"
            / "qcopilots_mcp_server_skills"
            / "CMakeLists.txt"
        ).read_text(encoding="utf-8")

        self.assertIn("file(GLOB_RECURSE SKILL_FILES", cmake)
        self.assertIn("CONFIGURE_DEPENDS", cmake)
        self.assertIn('RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "Skills/*"', cmake)
        self.assertIn(
            "PLUGIN_INSTALL(qcopilots_mcp_server_skills ${SKILL_SUBDIR} ${SKILL_FILE})",
            cmake,
        )

    def test_service_manifest_and_entrypoint_expose_prompts(self):
        import qcopilots_mcp_server_skills.server as server_module

        repo_root = Path(__file__).resolve().parents[3]
        plugin_dir = repo_root / "python" / "plugins" / "qcopilots_mcp_server_skills"
        manifest = json.loads(
            (plugin_dir / "qcopilots_service.json").read_text(encoding="utf-8")
        )
        schema = json.loads(
            (plugin_dir / "qcopilots_service.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertIn("prompts", manifest["capabilities"])
        self.assertEqual(manifest["skillsRoot"], "Skills")
        self.assertEqual(manifest["skills"]["validation_mode"], "compatible")
        self.assertIn("skillsRoot", schema["required"])
        self.assertIn(
            "prompts",
            schema["properties"]["capabilities"]["items"]["enum"],
        )
        self.assertEqual(schema["properties"]["skillsRoot"]["type"], "string")
        self.assertEqual(
            schema["properties"]["skills"]["properties"]["validation_mode"]["enum"],
            ["strict", "compatible"],
        )
        single_root_description = schema["properties"]["skillsRoot"]["description"]
        self.assertEqual(
            single_root_description,
            "字段作用是声明唯一的 Skills 根目录。候选值是一个非空路径字符串，相对路径按 "
            "qcopilots_service.json 所在插件目录解析，绝对路径按原路径使用。默认值是配置文件"
            "同目录下的 Skills 目录。该字段是服务选择技能目录的唯一配置入口，环境变量、"
            "CODEX_HOME/skills、skills_roots 和 skills.roots 都不会作为技能根来源。",
        )

        class FakeService:
            prompt_marker = object()

            def tools(self):
                return []

            def resources(self):
                return []

            def prompts(self):
                return [self.prompt_marker]

        calls = []
        original_build_skill_service = server_module.build_skill_service
        original_run_mcp_server = server_module.run_mcp_server
        try:
            server_module.build_skill_service = lambda plugin_path: FakeService()

            def fake_run_mcp_server(**kwargs):
                calls.append(kwargs)

            server_module.run_mcp_server = fake_run_mcp_server
            server_module.main()
        finally:
            server_module.build_skill_service = original_build_skill_service
            server_module.run_mcp_server = original_run_mcp_server

        self.assertEqual(len(calls), 1)
        self.assertIn("prompts", calls[0])
        self.assertTrue(callable(calls[0]["prompts"]))
        self.assertEqual(calls[0]["prompts"](), [FakeService.prompt_marker])

    def test_copied_external_testskills_are_served_from_configured_root(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService, configured_skill_roots

        configured_source = os.environ.get("QCOPILOTS_TESTSKILLS_DIR")
        if configured_source is None:
            self.skipTest(
                "Set QCOPILOTS_TESTSKILLS_DIR to run the external frozen TestSkills fixture check."
            )
        source = Path(configured_source)
        self.assertTrue(
            source.is_dir(),
            f"QCOPILOTS_TESTSKILLS_DIR is not an existing directory: {source}",
        )
        expected_file_hashes = {
            "openai-docs/agents/openai.yaml": (
                "E720E8DEF05CEE556A360D0E5A97A1BF191CE6D8EC65A7A51292566C18BB181D"
            ),
            "openai-docs/assets/openai.png": (
                "156CC84D7332BFE95B310350BD470B690D22AA33D65340CC6C2E06022946194C"
            ),
            "openai-docs/assets/openai-small.svg": (
                "85EF0A2CDE497872EDEC1525F1D538CCC42B33FF47ACDBB88499A1631A4BDB21"
            ),
            "openai-docs/LICENSE.txt": (
                "44E03F7E263EB3AD256440C1EEAB5E49168BAFF5A78ECEF9A5447261CF3A91BC"
            ),
            "openai-docs/references/latest-model.md": (
                "88A003C1BFC8F369C8A6F74997F1BBED8EB780411A7279B73C1A1E606320C884"
            ),
            "openai-docs/references/prompting-guide.md": (
                "2AF97956C3EDD3E285C68483733A45C7F83C85C495537F5AA0A7D66D957E7C73"
            ),
            "openai-docs/references/upgrade-guide.md": (
                "906DDABBC0E49CB41F1A718D144BDF0F49D9DFC855DE3FB4BC329FE47EFDF8E2"
            ),
            "openai-docs/scripts/fetch-codex-manual.mjs": (
                "1B9A36D3E59DDFF3317D1FAA182E5CD4967DB14FF993593A5F98D5682DBFFBE4"
            ),
            "openai-docs/scripts/resolve-latest-model-info.js": (
                "2E1D084E3B4DCF9F11910B1F1CA36CFCB056D4D3069F7FEBE2CED72B90AEE26E"
            ),
            "openai-docs/SKILL.md": (
                "117E696F80607076F63887F4F892418F38B6AF4C56CCF2B6E50E1DD1A4871DA4"
            ),
            "skill-creator/SKILL.md": (
                "2940E08D49E5197453E58A4387D14CB07101B97911C992EF75C8289E0A1734C0"
            ),
            "skill-creator/agents/openai.yaml": (
                "B750627CDD588FDBB5B46E854D3384D1A504FAD3B8ED73CA5D7EAD3804DF78B6"
            ),
            "skill-creator/assets/skill-creator-small.svg": (
                "EDB19A0C59F9A1B5253B9AF6D2F9F03CAC27B2FE6A0275EBCCBBDBFF07891A1A"
            ),
            "skill-creator/assets/skill-creator.png": (
                "A4024B0306DDB05847E1012879D37AAF1E658205199DA596F5145ED7A88D9162"
            ),
            "skill-creator/license.txt": (
                "3DDF9BE5C28FE27DAD143A5DC76EEA25222AD1DD68934A047064E56ED2FA40C5"
            ),
            "skill-creator/references/openai_yaml.md": (
                "7039D465B05342CBACF861F08D20192AB1EEEE112BC5D8FF0FB66234756887A9"
            ),
            "skill-creator/scripts/generate_openai_yaml.py": (
                "9EA002C57657E169B1BDDCAE227E55A5D87C43735DA6D87CF682B3E216ABB84B"
            ),
            "skill-creator/scripts/init_skill.py": (
                "50281D4B53C73F6FFA1E98ADF1EE9FCD55D4854970F24513E33DA65F073F74BE"
            ),
            "skill-creator/scripts/quick_validate.py": (
                "5347A0A09CFB546BBA1C0D1A30DAE0A233D9A05F57BD4E7877155C588BCDABF7"
            ),
        }
        actual_relative_files = {
            path.relative_to(source).as_posix()
            for path in source.rglob("*")
            if path.is_file()
        }
        self.assertEqual(actual_relative_files, set(expected_file_hashes))
        self.assertEqual(
            {path.name for path in source.iterdir() if path.is_dir()},
            {"openai-docs", "skill-creator"},
        )
        for relative_path, expected_hash in expected_file_hashes.items():
            fixture_file = source / Path(relative_path)
            self.assertTrue(fixture_file.is_file(), fixture_file)
            self.assertEqual(
                hashlib.sha256(fixture_file.read_bytes()).hexdigest().upper(),
                expected_hash,
                relative_path,
            )

        with tempfile.TemporaryDirectory() as tmp:
            plugin_dir = Path(tmp) / "plugin"
            skills_root = plugin_dir / "Skills"
            plugin_dir.mkdir()
            shutil.copytree(source, skills_root)
            (plugin_dir / "qcopilots_service.json").write_text(
                json.dumps(
                    {
                        "skillsRoot": "Skills",
                        "skills": {"validation_mode": "compatible"},
                    }
                ),
                encoding="utf-8",
            )

            service = SkillService(
                configured_skill_roots(plugin_dir),
                validation_mode="compatible",
            )
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
            )

            tool_names = self._tool_names(server)
            self.assertIn("skill-creator", tool_names)
            self.assertIn("openai-docs", tool_names)

            skills = self._call_tool(server, "list_skills", {})["skills"]
            self.assertEqual(
                [skill["slug"] for skill in skills],
                ["openai-docs", "skill-creator"],
            )
            summaries = {skill["slug"]: skill for skill in skills}
            self.assertEqual(summaries["openai-docs"]["conformance"], "strict")
            self.assertEqual(summaries["skill-creator"]["conformance"], "strict")
            diagnostics = self._call_tool(server, "list_skills", {})["diagnostics"]
            self.assertEqual(diagnostics, [])

            openai_docs = self._call_tool(
                server,
                "openai-docs",
                {"task": "读取 OpenAI 文档技能"},
            )
            self.assertIn("# OpenAI Docs", openai_docs["instructions"])
            self.assertEqual(openai_docs["metadata"]["allowed_tools"], [])
            self.assertEqual(
                {resource["path"] for resource in openai_docs["resources"]},
                {
                    "agents/openai.yaml",
                    "assets/openai.png",
                    "assets/openai-small.svg",
                    "LICENSE.txt",
                    "references/latest-model.md",
                    "references/prompting-guide.md",
                    "references/upgrade-guide.md",
                    "scripts/fetch-codex-manual.mjs",
                    "scripts/resolve-latest-model-info.js",
                },
            )

            skill_creator = self._call_tool(
                server,
                "skill-creator",
                {"task": "读取技能创建说明"},
            )
            self.assertIn("# Skill Creator", skill_creator["instructions"])
            self.assertEqual(skill_creator["conformance"], "strict")
            self.assertEqual(
                {resource["path"] for resource in skill_creator["resources"]},
                {
                    "agents/openai.yaml",
                    "assets/skill-creator.png",
                    "assets/skill-creator-small.svg",
                    "license.txt",
                    "references/openai_yaml.md",
                    "scripts/generate_openai_yaml.py",
                    "scripts/init_skill.py",
                    "scripts/quick_validate.py",
                },
            )

    def test_strict_portable_name_filename_and_frontmatter_boundaries(self):
        from qcopilots_common.skills import SkillRegistry

        valid_names = ("a", "a" * 64, "a-1-b")
        for name in valid_names:
            with self.subTest(valid_name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                self._write_raw_skill(root, name, self._canonical_document(name))
                registry = SkillRegistry(root)
                registry.load()
                self.assertEqual([skill.name for skill in registry.skills], [name])
                self.assertEqual(registry.skills[0].conformance, "strict")

        invalid_names = (
            "a" * 65,
            "-leading",
            "trailing-",
            "double--hyphen",
            "Uppercase",
            "under_score",
            "技能",
        )
        for name in invalid_names:
            with self.subTest(invalid_name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                skill_dir = self._write_raw_skill(
                    root,
                    name,
                    self._canonical_document(name),
                )
                registry = SkillRegistry(root)
                registry.load()
                self.assertEqual(registry.skills, ())
                self._assert_diagnostic(
                    registry,
                    code="skills.skill.invalid",
                    source=skill_dir / "SKILL.md",
                    message="must be 1-64 lowercase ASCII letters",
                )

        malformed_documents = {
            "bom": (
                "\ufeff" + self._canonical_document("bom"),
                "Skill {source} is missing YAML front matter.",
            ),
            "leading-space": (
                "\n" + self._canonical_document("leading-space"),
                "Skill {source} is missing YAML front matter.",
            ),
            "unclosed": (
                "---\nname: unclosed\ndescription: Missing close\n# Body\n",
                "Skill {source} is missing YAML front matter.",
            ),
            "container-mismatch": (
                self._canonical_document("other-name"),
                (
                    "Skill name 'other-name' in {source} must match container "
                    "'container-mismatch'."
                ),
            ),
            "padded-name": (
                self._canonical_document("' padded-name '"),
                (
                    "Skill name ' padded-name ' in {source} must be 1-64 lowercase "
                    "ASCII letters, digits, or single hyphen-separated segments."
                ),
            ),
        }
        for directory_name, (
            document,
            message_template,
        ) in malformed_documents.items():
            with (
                self.subTest(document=directory_name),
                tempfile.TemporaryDirectory() as tmp,
            ):
                root = Path(tmp) / "Skills"
                skill_dir = self._write_raw_skill(root, directory_name, document)
                registry = SkillRegistry(root)
                registry.load()
                self.assertEqual(registry.skills, ())
                source = str(skill_dir / "SKILL.md")
                self._assert_diagnostic(
                    registry,
                    code="skills.skill.invalid",
                    source=skill_dir / "SKILL.md",
                    message=message_template.format(source=source),
                    exact_message=True,
                )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            directory = root / "lowercase"
            directory.mkdir(parents=True)
            (directory / "skill.md").write_text(
                self._canonical_document("lowercase"), encoding="utf-8"
            )
            registry = SkillRegistry(root)
            registry.load()
            self.assertEqual(registry.skills, ())
            self.assertIn(
                "skills.instructions.noncanonical_name",
                self._diagnostic_codes(registry),
            )

    def test_strict_frontmatter_field_length_and_type_boundaries(self):
        from qcopilots_common.skills import SkillRegistry

        valid_cases = {
            "description-one": (
                self._canonical_document("description-one", "x"),
                {"description": "x"},
            ),
            "description-1024": (
                self._canonical_document("description-1024", "x" * 1024),
                {"description": "x" * 1024},
            ),
            "compatibility-500": (
                self._canonical_document(
                    "compatibility-500",
                    extra=("compatibility: " + ("x" * 500),),
                ),
                {"compatibility": "x" * 500},
            ),
            "typed-fields": (
                self._canonical_document(
                    "typed-fields",
                    extra=(
                        "license: Apache-2.0",
                        "compatibility: QGIS 4 compatible",
                        "allowed-tools: read_file write_file",
                        "metadata:",
                        "  owner: tests",
                        "  maturity: stable",
                    ),
                ),
                {
                    "license": "Apache-2.0",
                    "compatibility": "QGIS 4 compatible",
                    "allowed_tools": ("read_file", "write_file"),
                    "allowed_tools_declaration": "read_file write_file",
                    "metadata": {"owner": "tests", "maturity": "stable"},
                },
            ),
        }
        for name, (document, expected) in valid_cases.items():
            with self.subTest(valid=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                self._write_raw_skill(root, name, document)
                registry = SkillRegistry(root)
                registry.load()
                self.assertEqual([skill.name for skill in registry.skills], [name])
                metadata = registry.skills[0].metadata
                for field, value in expected.items():
                    self.assertEqual(getattr(metadata, field), value, field)
                self.assertEqual(registry.skills[0].conformance, "strict")
                self.assertEqual(registry.diagnostics, ())

        invalid_cases = (
            ("description-empty", "description: ''", "non-empty string 'description'"),
            ("description-int", "description: 123", "non-empty string 'description'"),
            ("description-null", "description: null", "non-empty string 'description'"),
            ("description-list", "description: [text]", "non-empty string 'description'"),
            ("description-map", "description: {text: value}", "non-empty string 'description'"),
            ("compatibility-int", "compatibility: 123", "field 'compatibility'"),
            ("compatibility-null", "compatibility: null", "field 'compatibility'"),
            ("compatibility-list", "compatibility: [QGIS]", "field 'compatibility'"),
            ("compatibility-map", "compatibility: {qgis: 4}", "field 'compatibility'"),
            ("license-int", "license: 123", "field 'license'"),
            ("license-null", "license: null", "field 'license'"),
            ("license-list", "license: [Apache-2.0]", "field 'license'"),
            ("license-map", "license: {name: Apache-2.0}", "field 'license'"),
            ("metadata-null", "metadata: null", "Metadata in"),
            ("metadata-list", "metadata: [owner]", "Metadata in"),
            ("metadata-string", "metadata: owner", "Metadata in"),
            (
                "metadata-value-list",
                "metadata:\n  owner: [tests]",
                "Metadata keys and values",
            ),
            ("allowed-tools-null", "allowed-tools: null", "field 'allowed-tools'"),
            ("allowed-tools-int", "allowed-tools: 123", "field 'allowed-tools'"),
            (
                "allowed-tools-list",
                "allowed-tools:\n  - read_file",
                "field 'allowed-tools'",
            ),
            (
                "allowed-tools-map",
                "allowed-tools: {read_file: true}",
                "field 'allowed-tools'",
            ),
            (
                "description-raw-1025",
                "description: ' " + ("x" * 1024) + "'",
                "1-1024 characters",
            ),
            (
                "compatibility-raw-501",
                "compatibility: ' " + ("x" * 500) + "'",
                "1-500 characters",
            ),
        )
        for name, field_yaml, message in invalid_cases:
            with self.subTest(invalid=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                document_lines = [
                    "---",
                    f"name: {name}",
                    "description: Valid description",
                ]
                if field_yaml.startswith("description:"):
                    document_lines[-1] = field_yaml
                else:
                    document_lines.extend(field_yaml.splitlines())
                document_lines.extend(["---", "# Body", ""])
                skill_dir = self._write_raw_skill(
                    root,
                    name,
                    "\n".join(document_lines),
                )
                registry = SkillRegistry(root)
                registry.load()
                self.assertEqual(registry.skills, ())
                self._assert_diagnostic(
                    registry,
                    code="skills.skill.invalid",
                    source=skill_dir / "SKILL.md",
                    message=message,
                )

    def test_metadata_nonstring_keys_are_converted_or_rejected_without_collisions(self):
        from qcopilots_common.skills import SkillRegistry

        metadata_yaml = "metadata:\n  1: 2"
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            strict_dir = self._write_raw_skill(
                root,
                "strict-metadata-key",
                self._canonical_document(
                    "strict-metadata-key",
                    extra=(metadata_yaml,),
                ),
            )
            strict_registry = SkillRegistry(root, validation_mode="strict")
            strict_registry.load()
            self.assertEqual(strict_registry.skills, ())
            self._assert_diagnostic(
                strict_registry,
                code="skills.skill.invalid",
                source=strict_dir / "SKILL.md",
                message=(
                    f"Metadata keys and values in {strict_dir / 'SKILL.md'} must be strings."
                ),
                exact_message=True,
            )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            compatible_dir = self._write_raw_skill(
                root,
                "compatible-metadata-key",
                self._canonical_document(
                    "compatible-metadata-key",
                    extra=(metadata_yaml,),
                ),
            )
            compatible_registry = SkillRegistry(root, validation_mode="compatible")
            compatible_registry.load()
            skill = compatible_registry.get("compatible-metadata-key")
            source = str(compatible_dir / "SKILL.md")
            self.assertEqual(skill.metadata.metadata, {"1": "2"})
            self.assertEqual(skill.conformance, "compatible")
            conversion_diagnostics = [
                diagnostic.as_dict()
                for diagnostic in compatible_registry.diagnostics
                if diagnostic.code.startswith("skills.compat.metadata_")
            ]
            self.assertEqual(
                conversion_diagnostics,
                [
                    {
                        "code": "skills.compat.metadata_key",
                        "severity": "warning",
                        "source": source,
                        "message": (
                            "Converted non-string metadata key to '1' in compatible mode."
                        ),
                    },
                    {
                        "code": "skills.compat.metadata_value",
                        "severity": "warning",
                        "source": source,
                        "message": (
                            "Converted a non-string metadata value to text in compatible mode."
                        ),
                    },
                ],
            )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            collision_dir = self._write_raw_skill(
                root,
                "metadata-key-collision",
                self._canonical_document(
                    "metadata-key-collision",
                    extra=("metadata:\n  1: integer\n  '1': string",),
                ),
            )
            collision_registry = SkillRegistry(root, validation_mode="compatible")
            collision_registry.load()
            self.assertEqual(collision_registry.skills, ())
            collision_message = (
                f"Metadata keys in {collision_dir / 'SKILL.md'} collide after compatible "
                "conversion: '1'."
            )
            self._assert_diagnostic(
                collision_registry,
                code="skills.metadata.key_collision",
                source=collision_dir / "SKILL.md",
                message=collision_message,
                exact_message=True,
            )
            self._assert_diagnostic(
                collision_registry,
                code="skills.skill.invalid",
                source=collision_dir / "SKILL.md",
                message=collision_message,
                exact_message=True,
            )

    def test_unknown_frontmatter_fields_are_preserved_with_diagnostics(self):
        from qcopilots_common.skills import SkillDiagnostic, SkillRegistry

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_raw_skill(
                root,
                "extra-field",
                self._canonical_document(
                    "extra-field",
                    extra=("vendor-extension: retained",),
                ),
            )
            registry = SkillRegistry(root)
            registry.load()

            skill = registry.get("extra-field")
            self.assertEqual(skill.metadata.extra, {"vendor-extension": "retained"})
            self.assertEqual(skill.conformance, "strict")
            diagnostic = registry.diagnostics[0]
            self.assertIsInstance(diagnostic, SkillDiagnostic)
            self.assertEqual(
                diagnostic.as_dict(),
                {
                    "code": "skills.metadata.unknown_field",
                    "severity": "warning",
                    "source": str(skill.instructions_path),
                    "message": (
                        "Unknown top-level front matter field 'vendor-extension' "
                        "was preserved."
                    ),
                },
            )
            with self.assertRaises(AttributeError):
                diagnostic.code = "changed"

    def test_unknown_yaml_values_are_stable_json_in_activation_and_tool_results(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_raw_skill(
                root,
                "json-safe-extra",
                "---\n"
                "name: json-safe-extra\n"
                "description: JSON safe unknown YAML values\n"
                "unknown-date: 2026-07-18\n"
                "unknown-datetime: 2026-07-18T12:34:56+00:00\n"
                "unknown-set: !!set\n"
                "  beta: null\n"
                "  alpha: null\n"
                "unknown-nested:\n"
                "  7: 2026-07-19\n"
                "  child:\n"
                "    when: 2026-07-20T01:02:03\n"
                "7: seven\n"
                "---\n"
                "# JSON safe\n",
            )
            service = SkillService([root])
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
                prompts=service.prompts,
            )

            response = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 401,
                    "method": "tools/call",
                    "params": {
                        "name": "json-safe-extra",
                        "arguments": {"task": "Serialize unknown YAML"},
                    },
                }
            )
            self.assertNotIn("error", response)
            serialized = json.dumps(
                response,
                ensure_ascii=False,
                sort_keys=True,
                allow_nan=False,
            )
            self.assertEqual(
                json.dumps(
                    json.loads(serialized),
                    ensure_ascii=False,
                    sort_keys=True,
                    allow_nan=False,
                ),
                serialized,
            )
            activation = response["result"]["structuredContent"]
            expected_extra = {
                "unknown-date": "2026-07-18",
                "unknown-datetime": "2026-07-18T12:34:56+00:00",
                "unknown-set": ["alpha", "beta"],
                "unknown-nested": {
                    "7": "2026-07-19",
                    "child": {"when": "2026-07-20T01:02:03"},
                },
                "7": "seven",
            }
            self.assertEqual(activation["metadata"]["extra"], expected_extra)
            self.assertEqual(activation["conformance"], "strict")
            self.assertEqual(
                json.loads(response["result"]["content"][0]["text"]),
                activation,
            )

            diagnostics = service.diagnostics
            unknown = [
                diagnostic
                for diagnostic in diagnostics
                if diagnostic.code == "skills.metadata.unknown_field"
            ]
            converted = [
                diagnostic
                for diagnostic in diagnostics
                if diagnostic.code == "skills.metadata.extra_converted"
            ]
            self.assertEqual(len(unknown), 5)
            self.assertEqual(len(converted), 5)
            self.assertEqual(
                {diagnostic.source for diagnostic in unknown + converted},
                {str(skill_dir / "SKILL.md")},
            )
            self.assertTrue(
                all(diagnostic.severity == "warning" for diagnostic in diagnostics)
            )

    def test_compatible_mode_recovers_legacy_forms_with_explicit_warnings(self):
        from qcopilots_common.skills import SkillRegistry

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_raw_skill(
                root,
                "canonical",
                self._canonical_document("canonical"),
            )
            self._write_raw_skill(
                root,
                "allowed-list",
                self._canonical_document(
                    "allowed-list", extra=("allowed-tools:", "  - read_file")
                ),
            )
            self._write_raw_skill(
                root,
                "allowed-alias",
                self._canonical_document(
                    "allowed-alias", extra=("allowed_tools: read_file write_file",)
                ),
            )
            self._write_raw_skill(
                root,
                "missing-name",
                "---\ndescription: Derived from container\n---\n# Derived\n",
            )
            self._write_raw_skill(
                root,
                "legacy-container",
                "---\n"
                "name: Legacy_Name\n"
                "description: Legacy name and metadata\n"
                "metadata:\n"
                "  retries: [3]\n"
                "---\n"
                "# Legacy\n",
            )
            lowercase = root / "lowercase"
            lowercase.mkdir(parents=True)
            (lowercase / "skill.md").write_text(
                self._canonical_document("lowercase"), encoding="utf-8"
            )
            self._write_raw_skill(
                root,
                "missing-description",
                "---\nname: missing-description\n---\n# Missing\n",
            )
            whitespace_dir = self._write_raw_skill(
                root,
                "whitespace-fields",
                "---\n"
                "name: ' whitespace-fields '\n"
                "description: ' Padded description '\n"
                "license: ' Apache-2.0 '\n"
                "compatibility: ' QGIS 4 '\n"
                "allowed-tools: ' read_file  write_file '\n"
                "---\n"
                "# Whitespace\n",
            )
            raw_boundaries_dir = self._write_raw_skill(
                root,
                "raw-boundaries",
                "---\n"
                "name: raw-boundaries\n"
                "description: ' "
                + ("x" * 1024)
                + "'\n"
                "compatibility: ' "
                + ("y" * 500)
                + "'\n"
                "---\n"
                "# Raw boundaries\n",
            )
            self._write_raw_skill(
                root,
                "invalid-yaml",
                "---\nname: invalid-yaml\ndescription: [unterminated\n---\n# Bad\n",
            )

            registry = SkillRegistry(root, validation_mode="compatible")
            registry.load()

            self.assertEqual(
                [skill.slug for skill in registry.skills],
                [
                    "allowed-alias",
                    "allowed-list",
                    "canonical",
                    "legacy-name",
                    "lowercase",
                    "missing-name",
                    "raw-boundaries",
                    "whitespace-fields",
                ],
            )
            self.assertEqual(registry.get("canonical").conformance, "strict")
            for slug in (
                "allowed-alias",
                "allowed-list",
                "legacy-name",
                "lowercase",
                "missing-name",
                "raw-boundaries",
                "whitespace-fields",
            ):
                self.assertEqual(registry.get(slug).conformance, "compatible", slug)
            self.assertEqual(
                registry.get("allowed-alias").metadata.allowed_tools,
                ("read_file", "write_file"),
            )
            whitespace = registry.get("whitespace-fields").metadata
            self.assertEqual(whitespace.name, "whitespace-fields")
            self.assertEqual(whitespace.description, "Padded description")
            self.assertEqual(whitespace.license, "Apache-2.0")
            self.assertEqual(whitespace.compatibility, "QGIS 4")
            self.assertEqual(whitespace.allowed_tools, ("read_file", "write_file"))
            self.assertEqual(
                whitespace.allowed_tools_declaration,
                " read_file  write_file ",
            )
            raw_boundaries = registry.get("raw-boundaries").metadata
            self.assertEqual(raw_boundaries.description, "x" * 1024)
            self.assertEqual(raw_boundaries.compatibility, "y" * 500)
            whitespace_diagnostics = {
                "skills.compat.name_whitespace": (
                    "Trimmed skill name padding in compatible mode."
                ),
                "skills.compat.description_whitespace": (
                    "Trimmed description padding in compatible mode."
                ),
                "skills.compat.license_whitespace": (
                    "Trimmed field 'license' padding in compatible mode."
                ),
                "skills.compat.compatibility_whitespace": (
                    "Trimmed field 'compatibility' padding in compatible mode."
                ),
                "skills.compat.allowed_tools_whitespace": (
                    "Normalized noncanonical allowed-tools whitespace in compatible mode."
                ),
            }
            for code, message in whitespace_diagnostics.items():
                self._assert_diagnostic(
                    registry,
                    code=code,
                    source=whitespace_dir / "SKILL.md",
                    message=message,
                    severity="warning",
                    exact_message=True,
                )
            for code, message in (
                (
                    "skills.compat.description_whitespace",
                    "Trimmed description padding in compatible mode.",
                ),
                (
                    "skills.compat.compatibility_whitespace",
                    "Trimmed field 'compatibility' padding in compatible mode.",
                ),
            ):
                self._assert_diagnostic(
                    registry,
                    code=code,
                    source=raw_boundaries_dir / "SKILL.md",
                    message=message,
                    severity="warning",
                    exact_message=True,
                )
            codes = self._diagnostic_codes(registry)
            self.assertTrue(
                {
                    "skills.compat.allowed_tools_alias",
                    "skills.compat.allowed_tools_list",
                    "skills.compat.name_missing",
                    "skills.compat.name_format",
                    "skills.compat.container_mismatch",
                    "skills.compat.metadata_value",
                    "skills.compat.instructions_lowercase",
                    "skills.compat.name_whitespace",
                    "skills.compat.description_whitespace",
                    "skills.compat.license_whitespace",
                    "skills.compat.compatibility_whitespace",
                    "skills.compat.allowed_tools_whitespace",
                    "skills.skill.invalid",
                }.issubset(codes)
            )

    def test_agentskills_fixed_commit_differential_vectors_are_explicit(self):
        from qcopilots_common.skills import (
            SkillRegistry,
            SkillValidationError,
            _parse_skill_document,
        )

        reference = (
            "agentskills/agentskills@"
            "38a2ff82958afee88dadf4831509e6f7e9d8ef4e"
        )
        reference_evidence = {
            "api": "skills_ref.validator.validate",
            "validator": f"{reference}/skills-ref/src/skills_ref/validator.py",
            "parser": f"{reference}/skills-ref/src/skills_ref/parser.py",
            "model": f"{reference}/skills-ref/src/skills_ref/models.py",
            "unexpected_fields_test": (
                f"{reference}/skills-ref/tests/test_validator.py::"
                "test_unexpected_fields"
            ),
        }
        classifications = {
            "unicode-name": "skills-ref-unicode-isalnum-vs-portable-ascii",
            "lowercase-filename": "skills-ref-case-fold-vs-exact-SKILL.md",
            "empty-compatibility": "skills-ref-empty-optional-vs-nonempty-spec-field",
            "metadata-nonstring": "skills-ref-string-coercion-vs-strict-string-map",
            "unknown-field": "skills-ref-unexpected-field-vs-qgis-preserved-extra",
        }
        self.assertEqual(
            reference,
            "agentskills/agentskills@38a2ff82958afee88dadf4831509e6f7e9d8ef4e",
        )
        self.assertEqual(reference_evidence["api"], "skills_ref.validator.validate")
        self.assertTrue(
            all(
                value.startswith(f"{reference}/skills-ref/")
                for key, value in reference_evidence.items()
                if key != "api"
            )
        )
        self.assertEqual(
            tuple(classifications),
            (
                "unicode-name",
                "lowercase-filename",
                "empty-compatibility",
                "metadata-nonstring",
                "unknown-field",
            ),
        )

        rejected = {
            "accepts": False,
            "normalized": None,
            "diagnostic_category": "SkillValidationError",
            "conformance": None,
        }
        oracle_by_classification = {
            "skills-ref-unicode-isalnum-vs-portable-ascii": {
                "reference": {
                    "accepts": True,
                    "normalized": "技能",
                    "diagnostic_category": (),
                },
                "strict": rejected,
                "compatible": {
                    "accepts": True,
                    "normalized": "技能",
                    "diagnostic_category": ("skills.compat.name_format",),
                    "conformance": "compatible",
                },
                "deviation": (
                    "The fixed skills-ref accepts Unicode isalnum names; the "
                    "portable Agent Skills grammar is ASCII and QGIS strict rejects it."
                ),
            },
            "skills-ref-case-fold-vs-exact-SKILL.md": {
                "reference": {
                    "accepts": True,
                    "normalized": ("lowercase-filename",),
                    "diagnostic_category": (),
                },
                "strict": {
                    "accepts": False,
                    "normalized": (),
                    "diagnostic_category": (
                        "skills.instructions.noncanonical_name",
                    ),
                    "conformance": None,
                },
                "compatible": {
                    "accepts": True,
                    "normalized": ("lowercase-filename",),
                    "diagnostic_category": (
                        "skills.compat.instructions_lowercase",
                    ),
                    "conformance": "compatible",
                },
                "deviation": (
                    "The fixed skills-ref case-folds skill.md; the normative filename "
                    "is exact SKILL.md and QGIS strict rejects the case variant."
                ),
            },
            "skills-ref-empty-optional-vs-nonempty-spec-field": {
                "reference": {
                    "accepts": True,
                    "normalized": "",
                    "diagnostic_category": (),
                },
                "strict": rejected,
                "compatible": {
                    "accepts": True,
                    "normalized": None,
                    "diagnostic_category": (
                        "skills.compat.compatibility_empty",
                    ),
                    "conformance": "compatible",
                },
                "deviation": (
                    "The fixed skills-ref retains an empty optional value; the "
                    "normative compatibility field is non-empty when present."
                ),
            },
            "skills-ref-string-coercion-vs-strict-string-map": {
                "reference": {
                    "accepts": True,
                    "normalized": {"1": "2"},
                    "diagnostic_category": (),
                },
                "strict": rejected,
                "compatible": {
                    "accepts": True,
                    "normalized": {"1": "2"},
                    "diagnostic_category": (
                        "skills.compat.metadata_key",
                        "skills.compat.metadata_value",
                    ),
                    "conformance": "compatible",
                },
                "deviation": (
                    "The fixed skills-ref coerces metadata keys and values to strings; "
                    "QGIS strict enforces the normative string map."
                ),
            },
            "skills-ref-unexpected-field-vs-qgis-preserved-extra": {
                "reference": {
                    "accepts": False,
                    "normalized": None,
                    "diagnostic_category": ("Unexpected fields",),
                },
                "strict": {
                    "accepts": True,
                    "normalized": {"vendor-extension": "retained"},
                    "diagnostic_category": ("skills.metadata.unknown_field",),
                    "conformance": "strict",
                },
                "compatible": {
                    "accepts": True,
                    "normalized": {"vendor-extension": "retained"},
                    "diagnostic_category": ("skills.metadata.unknown_field",),
                    "conformance": "strict",
                },
                "deviation": (
                    "The fixed skills_ref.validator.validate API rejects unexpected "
                    "fields; QGIS intentionally preserves the extra field and emits "
                    "an explicit product diagnostic."
                ),
            },
        }
        self.assertEqual(
            set(oracle_by_classification),
            set(classifications.values()),
        )
        expected_reference_relations = {
            "skills-ref-unicode-isalnum-vs-portable-ascii": (
                False,
                True,
                False,
                True,
            ),
            "skills-ref-case-fold-vs-exact-SKILL.md": (
                False,
                True,
                False,
                True,
            ),
            "skills-ref-empty-optional-vs-nonempty-spec-field": (
                False,
                True,
                False,
                False,
            ),
            "skills-ref-string-coercion-vs-strict-string-map": (
                False,
                True,
                False,
                True,
            ),
            "skills-ref-unexpected-field-vs-qgis-preserved-extra": (
                False,
                False,
                False,
                False,
            ),
        }
        self.assertEqual(
            set(expected_reference_relations),
            set(classifications.values()),
        )
        expected_reference_diagnostic_categories = {
            classification: () for classification in classifications.values()
        }
        expected_reference_diagnostic_categories[
            classifications["unknown-field"]
        ] = ("Unexpected fields",)

        parse_vectors = (
            {
                "name": "unicode-name",
                "document": self._canonical_document("技能"),
                "container": "技能",
                "normalize": lambda metadata: metadata.name,
            },
            {
                "name": "empty-compatibility",
                "document": self._canonical_document(
                    "empty-compatibility",
                    extra=("compatibility: ''",),
                ),
                "container": "empty-compatibility",
                "normalize": lambda metadata: metadata.compatibility,
            },
            {
                "name": "metadata-nonstring",
                "document": self._canonical_document(
                    "metadata-nonstring",
                    extra=("metadata:\n  1: 2",),
                ),
                "container": "metadata-nonstring",
                "normalize": lambda metadata: metadata.metadata,
            },
            {
                "name": "unknown-field",
                "document": self._canonical_document(
                    "unknown-field",
                    extra=("vendor-extension: retained",),
                ),
                "container": "unknown-field",
                "normalize": lambda metadata: metadata.extra,
            },
        )

        def parse_outcome(vector, mode):
            diagnostics = []
            try:
                metadata, _, conformance = _parse_skill_document(
                    vector["document"],
                    source=f"{reference}/{vector['name']}/SKILL.md",
                    container_name=vector["container"],
                    validation_mode=mode,
                    diagnostics=diagnostics,
                )
            except SkillValidationError:
                return dict(rejected)
            return {
                "accepts": True,
                "normalized": vector["normalize"](metadata),
                "diagnostic_category": tuple(
                    sorted(diagnostic.code for diagnostic in diagnostics)
                ),
                "conformance": conformance,
            }

        for vector in parse_vectors:
            classification = classifications[vector["name"]]
            oracle = oracle_by_classification[classification]
            self.assertTrue(oracle["deviation"])
            self.assertEqual(
                oracle["reference"]["diagnostic_category"],
                expected_reference_diagnostic_categories[classification],
            )
            outcomes = {
                mode: parse_outcome(vector, mode)
                for mode in ("strict", "compatible")
            }
            reference_outcome = oracle["reference"]
            actual_reference_relation = (
                outcomes["strict"]["accepts"]
                == reference_outcome["accepts"],
                outcomes["compatible"]["accepts"]
                == reference_outcome["accepts"],
                outcomes["strict"]["normalized"]
                == reference_outcome["normalized"],
                outcomes["compatible"]["normalized"]
                == reference_outcome["normalized"],
            )
            self.assertEqual(
                actual_reference_relation,
                expected_reference_relations[classification],
            )
            for mode in ("strict", "compatible"):
                with self.subTest(
                    reference=reference,
                    reference_api=reference_evidence["api"],
                    vector=vector["name"],
                    classification=classification,
                    mode=mode,
                ):
                    self.assertEqual(
                        outcomes[mode],
                        oracle[mode],
                    )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            directory = root / "lowercase-filename"
            directory.mkdir(parents=True)
            lowercase_path = directory / "skill.md"
            lowercase_path.write_text(
                self._canonical_document("lowercase-filename"),
                encoding="utf-8",
            )
            strict_registry = SkillRegistry(root, validation_mode="strict")
            strict_registry.load()
            classification = classifications["lowercase-filename"]
            oracle = oracle_by_classification[classification]
            self.assertTrue(oracle["deviation"])
            strict_outcome = {
                "accepts": bool(strict_registry.skills),
                "normalized": tuple(
                    skill.slug for skill in strict_registry.skills
                ),
                "diagnostic_category": tuple(
                    sorted(self._diagnostic_codes(strict_registry))
                ),
                "conformance": (
                    strict_registry.skills[0].conformance
                    if strict_registry.skills
                    else None
                ),
            }
            self.assertEqual(strict_outcome, oracle["strict"])

            compatible_registry = SkillRegistry(root, validation_mode="compatible")
            compatible_registry.load()
            compatible_outcome = {
                "accepts": bool(compatible_registry.skills),
                "normalized": tuple(
                    skill.slug for skill in compatible_registry.skills
                ),
                "diagnostic_category": tuple(
                    sorted(self._diagnostic_codes(compatible_registry))
                ),
                "conformance": compatible_registry.get(
                    "lowercase-filename"
                ).conformance,
            }
            self.assertEqual(compatible_outcome, oracle["compatible"])
            actual_reference_relation = (
                strict_outcome["accepts"] == oracle["reference"]["accepts"],
                compatible_outcome["accepts"]
                == oracle["reference"]["accepts"],
                strict_outcome["normalized"]
                == oracle["reference"]["normalized"],
                compatible_outcome["normalized"]
                == oracle["reference"]["normalized"],
            )
            self.assertEqual(
                actual_reference_relation,
                expected_reference_relations[classification],
            )
            self.assertEqual(
                oracle["reference"],
                {
                    "accepts": True,
                    "normalized": ("lowercase-filename",),
                    "diagnostic_category": (),
                },
            )

    def test_diagnostics_report_reserved_names_collisions_and_first_root_wins(self):
        from qcopilots_common.skills import SkillRegistry

        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            first = base / "first"
            second = base / "second"
            self._write_skill(first, name="shared", body="# First\n")
            self._write_skill(second, name="shared", body="# Second\n")
            self._write_skill(first, name="list-skills")

            registry = SkillRegistry([first, second])
            registry.load()

            self.assertEqual([skill.slug for skill in registry.skills], ["shared"])
            self.assertEqual(registry.get("shared").body.strip(), "# First")
            diagnostics = [diagnostic.as_dict() for diagnostic in registry.diagnostics]
            self.assertIn(
                "skills.skill.reserved_name",
                {diagnostic["code"] for diagnostic in diagnostics},
            )
            collision = next(
                diagnostic
                for diagnostic in diagnostics
                if diagnostic["code"] == "skills.skill.slug_collision"
            )
            self.assertEqual(collision["severity"], "warning")
            self.assertTrue(
                collision["source"].endswith("shared\\SKILL.md")
                or collision["source"].endswith("shared/SKILL.md")
            )
            self.assertIn("first configured root wins", collision["message"])

    def test_progressive_disclosure_activation_and_resource_metadata(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            root = base / "Skills"
            sentinel = base / "script-was-executed.txt"
            script_content = (
                "from pathlib import Path\n"
                f"Path({json.dumps(str(sentinel))}).write_text('executed', encoding='utf-8')\n"
            )
            skill_dir = self._write_skill(
                root,
                name="progressive-skill",
                description="中文 <技能> & metadata",
                body="# 中文技能\n\nSymbols <>& and spaces stay in the body.\n",
                allowed_tools="read_file exec_shell_command",
                resources={
                    "scripts/run tool.py": script_content,
                    "references/中文 guide.md": "参考资料\n",
                    "assets/pixel.bin": b"\x00\xff",
                    "notes.txt": "plain file\n",
                },
            )
            empty_dir = self._write_raw_skill(
                root,
                "empty-body",
                self._canonical_document("empty-body", body=""),
            )
            service = SkillService([root])
            server = McpJsonRpcServer(
                server_name="qcopilots-skills-test",
                server_version="1.0.0",
                tools=service.tools,
                resources=service.resources,
                prompts=service.prompts,
            )
            self.assertFalse(sentinel.exists(), "Discovery must not execute scripts")

            listing = self._call_tool(server, "list_skills", {})
            self.assertEqual(listing["count"], 2)
            self.assertEqual(listing["validation_mode"], "strict")
            self.assertEqual(listing["diagnostics"], [])
            for summary in listing["skills"]:
                self.assertEqual(
                    set(summary), {"slug", "name", "description", "conformance"}
                )
                self.assertNotIn("instructions", summary)
                self.assertNotIn("path", summary)
            self.assertFalse(
                sentinel.exists(),
                "Catalog listing must not execute scripts",
            )

            activation = self._call_tool(
                server,
                "progressive-skill",
                {"task": "处理 <图层> & 保留字符"},
            )
            self.assertEqual(activation["conformance"], "strict")
            self.assertEqual(activation["source_kind"], "directory")
            self.assertEqual(activation["skill_root"], str(skill_dir.resolve()))
            self.assertEqual(
                activation["instructions_path"], str((skill_dir / "SKILL.md").resolve())
            )
            self.assertIn("# 中文技能", activation["instructions"])
            self.assertEqual(
                activation["metadata"]["allowed_tools"],
                ["read_file", "exec_shell_command"],
            )
            self.assertEqual(
                activation["metadata"],
                {
                    "name": "progressive-skill",
                    "description": "中文 <技能> & metadata",
                    "license": None,
                    "compatibility": None,
                    "allowed_tools": ["read_file", "exec_shell_command"],
                    "allowed_tools_declaration": "read_file exec_shell_command",
                    "metadata": {"owner": "qcopilots-tests"},
                    "extra": {},
                    "conformance": "strict",
                },
            )
            self.assertIn("&lt;图层&gt; &amp; 保留字符", activation["skill_content"])
            resources = {entry["path"]: entry for entry in activation["resources"]}
            self.assertEqual(resources["scripts/run tool.py"]["kind"], "script")
            self.assertEqual(resources["references/中文 guide.md"]["kind"], "reference")
            self.assertEqual(resources["assets/pixel.bin"]["kind"], "asset")
            self.assertEqual(resources["notes.txt"]["kind"], "file")
            self.assertIn("run%20tool.py", resources["scripts/run tool.py"]["uri"])
            self.assertIn(
                "%E4%B8%AD%E6%96%87%20guide.md",
                resources["references/中文 guide.md"]["uri"],
            )
            expected_resources = {
                "assets/pixel.bin": {
                    "uri": "resource://skillz/progressive-skill/assets/pixel.bin",
                    "name": "progressive-skill/assets/pixel.bin",
                    "path": "assets/pixel.bin",
                    "mime_type": "application/octet-stream",
                    "mimeType": "application/octet-stream",
                    "kind": "asset",
                },
                "notes.txt": {
                    "uri": "resource://skillz/progressive-skill/notes.txt",
                    "name": "progressive-skill/notes.txt",
                    "path": "notes.txt",
                    "mime_type": "text/plain",
                    "mimeType": "text/plain",
                    "kind": "file",
                },
                "references/中文 guide.md": {
                    "uri": (
                        "resource://skillz/progressive-skill/references/"
                        "%E4%B8%AD%E6%96%87%20guide.md"
                    ),
                    "name": "progressive-skill/references/中文 guide.md",
                    "path": "references/中文 guide.md",
                    "mime_type": "text/markdown",
                    "mimeType": "text/markdown",
                    "kind": "reference",
                },
                "scripts/run tool.py": {
                    "uri": "resource://skillz/progressive-skill/scripts/run%20tool.py",
                    "name": "progressive-skill/scripts/run tool.py",
                    "path": "scripts/run tool.py",
                    "mime_type": "text/x-python",
                    "mimeType": "text/x-python",
                    "kind": "script",
                },
            }
            self.assertEqual(resources, expected_resources)
            self.assertFalse(sentinel.exists(), "Activation must not execute scripts")

            read_skill = self._call_tool(
                server,
                "read_skill",
                {"slug": "progressive-skill"},
            )
            self.assertEqual(
                set(read_skill),
                {
                    "name",
                    "slug",
                    "conformance",
                    "source_kind",
                    "skill_root",
                    "instructions_path",
                    "instructions_file",
                    "content",
                    "skill_content",
                    "metadata",
                    "resource_metadata",
                },
            )
            self.assertEqual(read_skill["name"], "progressive-skill")
            self.assertEqual(read_skill["slug"], "progressive-skill")
            self.assertEqual(read_skill["conformance"], activation["conformance"])
            self.assertEqual(read_skill["source_kind"], activation["source_kind"])
            self.assertEqual(read_skill["skill_root"], activation["skill_root"])
            self.assertEqual(
                read_skill["instructions_path"], activation["instructions_path"]
            )
            self.assertEqual(read_skill["content"], activation["instructions"])
            self.assertEqual(read_skill["skill_root"], str(skill_dir.resolve()))
            self.assertEqual(read_skill["instructions_file"], "SKILL.md")
            self.assertEqual(read_skill["metadata"], activation["metadata"])
            self.assertEqual(
                {entry["path"]: entry for entry in read_skill["resource_metadata"]},
                expected_resources,
            )
            self.assertIn("<skill_content", read_skill["skill_content"])
            self.assertIn("<skill_instructions>", read_skill["skill_content"])

            listed_resources = self._call_tool(
                server,
                "list_skill_resources",
                {"slug": "progressive-skill"},
            )
            self.assertEqual(
                set(listed_resources),
                {"name", "slug", "resources", "resource_metadata"},
            )
            self.assertEqual(listed_resources["name"], "progressive-skill")
            self.assertEqual(listed_resources["slug"], "progressive-skill")
            self.assertEqual(
                set(listed_resources["resources"]), set(expected_resources)
            )
            self.assertEqual(
                {
                    entry["path"]: entry
                    for entry in listed_resources["resource_metadata"]
                },
                expected_resources,
            )

            script_uri = expected_resources["scripts/run tool.py"]["uri"]
            script_resource = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": 301,
                    "method": "resources/read",
                    "params": {"uri": script_uri},
                }
            )
            self.assertNotIn("error", script_resource)
            self.assertEqual(
                script_resource["result"]["contents"][0]["text"],
                script_content,
            )
            script_tool = self._call_tool(
                server,
                "read_skill_resource",
                {"slug": "progressive-skill", "resource": "scripts/run tool.py"},
            )
            self.assertEqual(
                script_tool,
                {
                    "name": "progressive-skill",
                    "slug": "progressive-skill",
                    "resource": "scripts/run tool.py",
                    "content": script_content,
                    "encoding": "utf-8",
                },
            )
            self.assertFalse(
                sentinel.exists(),
                "Resource reads must not execute scripts",
            )

            empty = self._call_tool(
                server,
                "empty-body",
                {"task": "Read empty body"},
            )
            self.assertEqual(empty["skill_root"], str(empty_dir.resolve()))
            self.assertEqual(empty["instructions"], "")

            tool_names = self._tool_names(server)
            self.assertEqual(
                tool_names,
                {
                    "list_skills",
                    "read_skill",
                    "list_skill_resources",
                    "read_skill_resource",
                    "empty-body",
                    "progressive-skill",
                },
            )
            self.assertIn(
                "never bypasses host permissions",
                activation["skill_content"],
            )
            self.assertIn("not permission", activation["usage"])

    def test_symlink_and_windows_junction_branches_are_skipped_without_privileges(self):
        import qcopilots_common.skills as skills_module

        probe = Path("link-probe")
        with mock.patch.object(Path, "is_symlink", return_value=True) as is_symlink:
            with mock.patch.object(
                Path,
                "is_junction",
                create=True,
                side_effect=AssertionError(
                    "junction branch must not run for a symlink"
                ),
            ):
                self.assertTrue(skills_module._is_link_or_junction(probe))
        is_symlink.assert_called_once_with()

        with mock.patch.object(Path, "is_symlink", return_value=False) as is_symlink:
            with mock.patch.object(
                Path,
                "is_junction",
                create=True,
                return_value=True,
            ) as is_junction:
                self.assertTrue(skills_module._is_link_or_junction(probe))
        is_symlink.assert_called_once_with()
        is_junction.assert_called_once_with()

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="linked-resource",
                resources={
                    "assets/safe.txt": "safe",
                    "assets/symlink-escape/outside.txt": "must not be exposed",
                    "assets/junction-escape/outside.txt": "must not be exposed",
                },
            )
            symlink_escape = skill_dir / "assets" / "symlink-escape"
            junction_escape = skill_dir / "assets" / "junction-escape"
            original_is_symlink = Path.is_symlink
            original_is_junction = getattr(Path, "is_junction", lambda path: False)
            symlink_checks = []
            junction_checks = []

            def deterministic_is_symlink(path):
                symlink_checks.append(path)
                if path == symlink_escape:
                    return True
                return original_is_symlink(path)

            def deterministic_is_junction(path):
                junction_checks.append(path)
                if path == junction_escape:
                    return True
                return original_is_junction(path)

            registry = skills_module.SkillRegistry(root)
            with mock.patch.object(
                Path,
                "is_symlink",
                new=deterministic_is_symlink,
            ), mock.patch.object(
                Path,
                "is_junction",
                new=deterministic_is_junction,
                create=True,
            ):
                registry.load()
                self.assertIn(symlink_escape, symlink_checks)
                self.assertIn(junction_escape, junction_checks)

                symlink_checks.clear()
                junction_checks.clear()
                service = skills_module.SkillService([root])
                symlink_checks.clear()
                junction_checks.clear()
                service.ensure_current()
                self.assertIn(
                    symlink_escape,
                    symlink_checks,
                    "Fingerprint refresh did not invoke Path.is_symlink",
                )
                self.assertIn(
                    junction_escape,
                    junction_checks,
                    "Fingerprint refresh did not invoke Path.is_junction",
                )

            skill = registry.get("linked-resource")
            self.assertEqual(tuple(skill.iter_resource_paths()), ("assets/safe.txt",))
            with self.assertRaisesRegex(
                skills_module.SkillError,
                "Invalid resource path",
            ):
                skill.open_bytes("../outside.txt")
            link_diagnostics = [
                diagnostic
                for diagnostic in registry.diagnostics
                if diagnostic.code == "skills.scan.link_skipped"
            ]
            self.assertEqual(
                {diagnostic.source for diagnostic in link_diagnostics},
                {
                    str(skill_dir / "assets" / "symlink-escape"),
                    str(skill_dir / "assets" / "junction-escape"),
                },
            )
            self.assertTrue(
                all(
                    diagnostic.severity == "warning"
                    and diagnostic.message == "Symbolic link or junction was skipped."
                    for diagnostic in link_diagnostics
                )
            )

    def test_archive_layout_and_safety_limits_are_diagnostic(self):
        import qcopilots_common.skills as skills_module

        document = self._canonical_document("archive-skill")

        def assert_invalid_archive(
            builder,
            expected_message,
            patches=(),
            *,
            inventory_failure=True,
            exact_message=True,
        ):
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                root.mkdir(parents=True)
                archive_path = root / "archive-skill.skill"
                builder(archive_path)
                with ExitStack() as stack:
                    for name, value in patches:
                        stack.enter_context(
                            mock.patch.object(skills_module, name, value)
                        )
                    if inventory_failure:
                        with self.assertRaises(
                            skills_module.SkillValidationError
                        ) as error:
                            skills_module._validated_archive_inventory(archive_path)
                        if exact_message:
                            self.assertEqual(str(error.exception), expected_message)
                        else:
                            self.assertIn(expected_message, str(error.exception))
                    registry = skills_module.SkillRegistry(
                        root, validation_mode="compatible"
                    )
                    registry.load()
                self.assertEqual(registry.skills, ())
                self._assert_diagnostic(
                    registry,
                    code="skills.archive.invalid",
                    source=archive_path,
                    message=expected_message,
                    exact_message=exact_message,
                )

        def member_count(path):
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("SKILL.md", document)
                archive.writestr("notes.txt", "x")

        assert_invalid_archive(
            member_count,
            "Archive contains more than 1 members.",
            (("MAX_SKILL_ARCHIVE_MEMBERS", 1),),
        )

        def total_size(path):
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("SKILL.md", document)
                archive.writestr("notes.txt", "overflow")

        assert_invalid_archive(
            total_size,
            "Archive uncompressed content exceeds "
            f"{len(document.encode('utf-8')) + 1} bytes.",
            (
                (
                    "MAX_SKILL_ARCHIVE_UNCOMPRESSED_BYTES",
                    len(document.encode("utf-8")) + 1,
                ),
            ),
        )

        def compressed(path):
            with zipfile.ZipFile(
                path,
                "w",
                compression=zipfile.ZIP_DEFLATED,
            ) as archive:
                archive.writestr("SKILL.md", document + ("x" * 4096))

        assert_invalid_archive(
            compressed,
            "Archive member 'SKILL.md' exceeds compression ratio 1.",
            (("MAX_SKILL_ARCHIVE_COMPRESSION_RATIO", 1),),
        )

        def traversal(path):
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("SKILL.md", document)
                archive.writestr("../escape.txt", "escape")

        assert_invalid_archive(
            traversal,
            "Archive member has an unsafe path: ../escape.txt",
        )

        def multiple_roots(path):
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("a/SKILL.md", self._canonical_document("a"))
                archive.writestr("b/SKILL.md", self._canonical_document("b"))

        assert_invalid_archive(
            multiple_roots,
            "Archive contains multiple canonical skill roots.",
            inventory_failure=False,
        )

        def duplicate(path):
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", UserWarning)
                with zipfile.ZipFile(path, "w") as archive:
                    archive.writestr("SKILL.md", document)
                    archive.writestr("SKILL.md", document)

        assert_invalid_archive(
            duplicate,
            "Archive contains duplicate member: SKILL.md",
        )

        def missing_instructions(path):
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("notes.txt", "No instructions")

        assert_invalid_archive(
            missing_instructions,
            "Archive does not contain a canonical skill root.",
            inventory_failure=False,
        )

        def broken_archive(path):
            path.write_bytes(b"not a zip")

        assert_invalid_archive(
            broken_archive,
            "Archive cannot be read:",
            exact_message=False,
        )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir(parents=True)
            with zipfile.ZipFile(root / "archive-skill.skill", "w") as archive:
                archive.writestr("SKILL.md", document)
            strict_registry = skills_module.SkillRegistry(root)
            strict_registry.load()
            self.assertEqual(strict_registry.skills, ())
            self._assert_diagnostic(
                strict_registry,
                code="skills.archive.strict_unsupported",
                source=root / "archive-skill.skill",
                message=(
                    "Archive skills are a compatible-mode extension and were skipped."
                ),
                severity="warning",
                exact_message=True,
            )

    def test_archive_unsupported_features_are_controlled_and_lkg_recovers(self):
        import qcopilots_common.skills as skills_module
        from qcopilots_common.mcp_http import ToolError

        def write_archive(path, body, resource="resource data\n"):
            with zipfile.ZipFile(
                path,
                "w",
                compression=zipfile.ZIP_STORED,
            ) as archive:
                archive.writestr(
                    "archive-feature/SKILL.md",
                    self._canonical_document("archive-feature", body=body),
                )
                archive.writestr(
                    "archive-feature/references/state.txt",
                    resource,
                )

        def patch_member_headers(
            path,
            member_name,
            *,
            compression_method=None,
            extract_version=None,
        ):
            with zipfile.ZipFile(path) as archive:
                info = archive.getinfo(member_name)
            data = bytearray(Path(path).read_bytes())
            local_offset = info.header_offset
            self.assertEqual(
                data[local_offset : local_offset + 4],
                b"PK\x03\x04",
            )
            if extract_version is not None:
                data[local_offset + 4 : local_offset + 6] = int(
                    extract_version
                ).to_bytes(2, "little")
            if compression_method is not None:
                data[local_offset + 8 : local_offset + 10] = int(
                    compression_method
                ).to_bytes(2, "little")

            eocd_offset = data.rfind(b"PK\x05\x06")
            self.assertGreaterEqual(eocd_offset, 0)
            central_offset = int.from_bytes(
                data[eocd_offset + 16 : eocd_offset + 20],
                "little",
            )
            central_match = None
            offset = central_offset
            while data[offset : offset + 4] == b"PK\x01\x02":
                name_length = int.from_bytes(data[offset + 28 : offset + 30], "little")
                extra_length = int.from_bytes(
                    data[offset + 30 : offset + 32],
                    "little",
                )
                comment_length = int.from_bytes(
                    data[offset + 32 : offset + 34],
                    "little",
                )
                encoded_name = bytes(data[offset + 46 : offset + 46 + name_length])
                if encoded_name == member_name.encode("utf-8"):
                    central_match = offset
                    break
                offset += 46 + name_length + extra_length + comment_length
            self.assertIsNotNone(central_match, member_name)
            if extract_version is not None:
                data[central_match + 6 : central_match + 8] = int(
                    extract_version
                ).to_bytes(2, "little")
            if compression_method is not None:
                data[central_match + 10 : central_match + 12] = int(
                    compression_method
                ).to_bytes(2, "little")
            Path(path).write_bytes(data)

        unsupported_cases = (
            (
                "compression-method",
                {"compression_method": 99},
                r"compression|unsupported",
            ),
            (
                "extract-version",
                # Python 3.12 在构造 ZipFile 时会拒绝中央目录中高于
                # MAX_EXTRACT_VERSION (63) 的解压版本。
                {"extract_version": 255},
                r"version|unsupported",
            ),
        )
        instructions_member = "archive-feature/SKILL.md"
        resource_member = "archive-feature/references/state.txt"

        with (
            self.subTest(phase="initial-resource", case="compression-method"),
            tempfile.TemporaryDirectory() as tmp,
        ):
            root = Path(tmp) / "Skills"
            root.mkdir(parents=True)
            archive_path = root / "archive-feature.skill"
            private_resource = "PRIVATE-UNREADABLE-RESOURCE-MUST-NOT-LEAK\n"
            write_archive(
                archive_path,
                "# Supported instructions with unsupported resource\n",
                resource=private_resource,
            )
            patch_member_headers(
                archive_path,
                resource_member,
                compression_method=99,
            )

            registry = skills_module.SkillRegistry(
                root,
                validation_mode="compatible",
            )
            registry.load()
            self.assertEqual(registry.skills, ())
            self.assertNotIn(
                "skills.reload.failed",
                self._diagnostic_codes(registry),
            )
            diagnostic = self._assert_diagnostic(
                registry,
                code="skills.archive.invalid",
                source=archive_path,
                message="Archive",
            )
            self.assertRegex(
                diagnostic.message.lower(),
                r"compression.*unsupported|unsupported.*compression",
            )
            for leaked_detail in (
                private_resource.strip(),
                "NotImplementedError",
                "That compression method is not supported",
            ):
                self.assertNotIn(leaked_detail, diagnostic.message)

        for case_name, patch_kwargs, category_pattern in unsupported_cases:
            with (
                self.subTest(phase="initial", case=case_name),
                tempfile.TemporaryDirectory() as tmp,
            ):
                root = Path(tmp) / "Skills"
                root.mkdir(parents=True)
                archive_path = root / "archive-feature.skill"
                write_archive(archive_path, "# Initial unsupported feature\n")
                patch_member_headers(
                    archive_path,
                    instructions_member,
                    **patch_kwargs,
                )

                registry = skills_module.SkillRegistry(
                    root,
                    validation_mode="compatible",
                )
                registry.load()
                self.assertEqual(registry.skills, ())
                self.assertNotIn(
                    "skills.reload.failed",
                    self._diagnostic_codes(registry),
                )
                diagnostic = self._assert_diagnostic(
                    registry,
                    code="skills.archive.invalid",
                    source=archive_path,
                    message="Archive",
                )
                self.assertRegex(diagnostic.message.lower(), category_pattern)

            with (
                self.subTest(phase="runtime", case=case_name),
                tempfile.TemporaryDirectory() as tmp,
            ):
                root = Path(tmp) / "Skills"
                root.mkdir(parents=True)
                archive_path = root / "archive-feature.skill"
                write_archive(archive_path, "# Original archive LKG\n")
                service = skills_module.SkillService(
                    [root],
                    validation_mode="compatible",
                    refresh_interval_seconds=0,
                )
                patch_member_headers(
                    archive_path,
                    instructions_member,
                    **patch_kwargs,
                )

                lkg_tools = {tool.name: tool for tool in service.tools()}
                self.assertEqual(
                    lkg_tools["archive-feature"].handler(
                        {"task": "Use archive feature LKG"}
                    )["instructions"],
                    "# Original archive LKG\n",
                )
                lkg_listing = lkg_tools["list_skills"].handler({})
                self.assertEqual(
                    [entry["slug"] for entry in lkg_listing["skills"]],
                    ["archive-feature"],
                )
                self.assertIn(
                    "skills.reload.failed",
                    {
                        diagnostic["code"]
                        for diagnostic in lkg_listing["diagnostics"]
                    },
                )

                write_archive(archive_path, "# Repaired archive feature\n")
                repaired_tools = {tool.name: tool for tool in service.tools()}
                self.assertEqual(
                    repaired_tools["archive-feature"].handler(
                        {"task": "Use repaired archive feature"}
                    )["instructions"],
                    "# Repaired archive feature\n",
                )
                self.assertNotIn(
                    "skills.reload.failed",
                    {
                        diagnostic["code"]
                        for diagnostic in repaired_tools["list_skills"].handler({})[
                            "diagnostics"
                        ]
                    },
                )

            with (
                self.subTest(phase="resource", case=case_name),
                tempfile.TemporaryDirectory() as tmp,
            ):
                root = Path(tmp) / "Skills"
                root.mkdir(parents=True)
                archive_path = root / "archive-feature.skill"
                write_archive(archive_path, "# Resource feature\n")
                service = skills_module.SkillService(
                    [root],
                    validation_mode="compatible",
                    refresh_interval_seconds=0,
                )
                tools = {tool.name: tool for tool in service.tools()}
                published_skill = service._registry.get("archive-feature")
                published_archive_identity = published_skill._zip_identity
                self.assertIsNotNone(published_archive_identity)
                resource_tool = tools["read_skill_resource"]

                patch_member_headers(
                    archive_path,
                    resource_member,
                    **patch_kwargs,
                )
                real_identity_from_stat = skills_module._path_identity_from_stat
                actual_archive_identity = real_identity_from_stat(
                    archive_path.stat(follow_symlinks=False),
                    include_content=True,
                )
                stable_archive_fields = (
                    "device",
                    "inode",
                    "file_type",
                    "file_attributes",
                    "size",
                )
                self.assertEqual(
                    tuple(
                        getattr(actual_archive_identity, field)
                        for field in stable_archive_fields
                    ),
                    tuple(
                        getattr(published_archive_identity, field)
                        for field in stable_archive_fields
                    ),
                )

                def accept_published_archive_identity(
                    path_stat,
                    *,
                    include_content,
                ):
                    actual = real_identity_from_stat(
                        path_stat,
                        include_content=include_content,
                    )
                    if include_content and all(
                        getattr(actual, field)
                        == getattr(published_archive_identity, field)
                        for field in stable_archive_fields
                    ):
                        return published_archive_identity
                    return actual

                with mock.patch.object(
                    skills_module,
                    "_path_identity_from_stat",
                    side_effect=accept_published_archive_identity,
                ):
                    accepted_identity = skills_module._capture_path_identity(
                        archive_path,
                        require_file=True,
                        include_content=True,
                    )
                    self.assertEqual(
                        accepted_identity,
                        published_archive_identity,
                    )
                    with self.assertRaises(ToolError) as resource_error:
                        resource_tool.handler(
                            {
                                "slug": "archive-feature",
                                "resource": "references/state.txt",
                            }
                        )
                self.assertEqual(
                    str(resource_error.exception),
                    "The requested skill resource is unavailable.",
                )
                validation_cause = resource_error.exception.__cause__
                self.assertIsInstance(
                    validation_cause,
                    skills_module.SkillValidationError,
                )
                self.assertRegex(
                    str(validation_cause).lower(),
                    r"archive.*unsupported|unsupported.*(?:archive|compression|version)",
                )
                self.assertIsInstance(
                    validation_cause.__cause__,
                    NotImplementedError,
                )

    def test_archive_eocd_budgets_precede_zipfile_and_small_archive_remains_valid(self):
        import qcopilots_common.skills as skills_module

        def eocd_record(
            *,
            disk_number=0,
            central_directory_disk=0,
            entries_on_disk=0,
            entry_count=0,
            central_directory_bytes=0,
            central_directory_offset=0,
            comment=b"",
        ):
            return b"".join(
                (
                    b"PK\x05\x06",
                    int(disk_number).to_bytes(2, "little"),
                    int(central_directory_disk).to_bytes(2, "little"),
                    int(entries_on_disk).to_bytes(2, "little"),
                    int(entry_count).to_bytes(2, "little"),
                    int(central_directory_bytes).to_bytes(4, "little"),
                    int(central_directory_offset).to_bytes(4, "little"),
                    len(comment).to_bytes(2, "little"),
                    comment,
                )
            )

        def central_directory_entry(name=b"entry", *, declared_name_size=None):
            header = bytearray(46)
            header[0:4] = b"PK\x01\x02"
            name_size = len(name) if declared_name_size is None else declared_name_size
            header[28:30] = int(name_size).to_bytes(2, "little")
            return bytes(header) + name

        member_overflow = skills_module.MAX_SKILL_ARCHIVE_MEMBERS + 1
        central_byte_overflow = (
            skills_module.MAX_SKILL_ARCHIVE_CENTRAL_DIRECTORY_BYTES + 1
        )
        zip64_locator = b"PK\x06\x07" + (b"\x00" * 16)
        invalid_central_directory = b"\x00" * 46
        truncated_central_directory = central_directory_entry(
            b"x",
            declared_name_size=2,
        )
        one_central_entry = central_directory_entry(b"a")
        valid_comment_with_fake_signature = eocd_record(
            comment=b"commentPK\x05\x06"
        )
        trailing_invalid_end_record = (
            eocd_record() + b"trailingPK\x05\x06"
        )

        with tempfile.TemporaryDirectory() as tmp:
            archive_path = Path(tmp) / "valid-comment-fake-signature.skill"
            archive_path.write_bytes(valid_comment_with_fake_signature)
            with archive_path.open("rb") as handle:
                skills_module._preflight_skill_archive(handle)

        with tempfile.TemporaryDirectory() as tmp:
            archive_path = Path(tmp) / "valid-empty-comment.skill"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.comment = b"ordinary archive comment"
            real_zip_file = skills_module.zipfile.ZipFile
            with mock.patch.object(
                skills_module.zipfile,
                "ZipFile",
                wraps=real_zip_file,
            ) as zip_file:
                inventory = skills_module._validated_archive_inventory(archive_path)
            zip_file.assert_called_once()
            self.assertEqual(inventory, ())

        preflight_cases = (
            (
                "member-budget",
                eocd_record(
                    entries_on_disk=member_overflow,
                    entry_count=member_overflow,
                ),
                r"member|entr",
            ),
            (
                "central-directory-byte-budget",
                eocd_record(
                    entries_on_disk=1,
                    entry_count=1,
                    central_directory_bytes=central_byte_overflow,
                ),
                r"central directory.*exceeds",
            ),
            (
                "zip64-locator",
                zip64_locator + eocd_record(),
                r"zip64.*unsupported",
            ),
            (
                "zip64-sentinel",
                eocd_record(central_directory_offset=0xFFFFFFFF),
                r"zip64.*unsupported",
            ),
            (
                "multi-disk",
                eocd_record(disk_number=1),
                r"multi-disk.*unsupported",
            ),
            (
                "trailing-invalid-end-record",
                trailing_invalid_end_record,
                r"end record.*(?:missing|invalid)",
            ),
            (
                "bad-central-directory-signature",
                invalid_central_directory
                + eocd_record(
                    entries_on_disk=1,
                    entry_count=1,
                    central_directory_bytes=len(invalid_central_directory),
                ),
                r"central directory.*invalid",
            ),
            (
                "truncated-central-directory-entry",
                truncated_central_directory
                + eocd_record(
                    entries_on_disk=1,
                    entry_count=1,
                    central_directory_bytes=len(truncated_central_directory),
                ),
                r"central directory.*truncated",
            ),
            (
                "central-directory-entry-count-mismatch",
                one_central_entry
                + eocd_record(
                    entries_on_disk=2,
                    entry_count=2,
                    central_directory_bytes=len(one_central_entry),
                ),
                r"entry count.*invalid",
            ),
        )
        for case_name, payload, message_pattern in preflight_cases:
            with self.subTest(case=case_name), tempfile.TemporaryDirectory() as tmp:
                archive_path = Path(tmp) / "preflight.skill"
                archive_path.write_bytes(payload)
                with mock.patch.object(
                    skills_module.zipfile,
                    "ZipFile",
                    side_effect=AssertionError(
                        "ZipFile must not run before EOCD budgets pass"
                    ),
                ) as zip_file:
                    with self.assertRaises(
                        skills_module.SkillValidationError
                    ) as error_context:
                        skills_module._validated_archive_inventory(archive_path)
                zip_file.assert_not_called()
                self.assertRegex(
                    str(error_context.exception).lower(),
                    message_pattern,
                )

        with tempfile.TemporaryDirectory() as tmp:
            archive_path = Path(tmp) / "small-valid.skill"
            with zipfile.ZipFile(
                archive_path,
                "w",
                compression=zipfile.ZIP_STORED,
            ) as archive:
                archive.writestr(
                    "SKILL.md",
                    self._canonical_document("small-valid"),
                )
            inventory = skills_module._validated_archive_inventory(archive_path)
            self.assertEqual(
                [info.filename for info in inventory],
                ["SKILL.md"],
            )

    def test_scan_limits_are_bounded_and_report_specific_diagnostics(self):
        import qcopilots_common.skills as skills_module

        cases = (
            (
                "MAX_SKILL_SCAN_DEPTH",
                0,
                "skills.scan.depth_limit",
                lambda root: self._write_skill(root / "nested", name="deep-skill"),
            ),
            (
                "MAX_SKILL_SCAN_DIRECTORIES",
                1,
                "skills.scan.directory_limit",
                lambda root: self._write_skill(root / "nested", name="directory-skill"),
            ),
            (
                "MAX_SKILL_SCAN_FILES",
                0,
                "skills.scan.file_limit",
                lambda root: (
                    root.mkdir(parents=True),
                    (root / "one.txt").write_text("x", encoding="utf-8"),
                ),
            ),
        )
        for constant, limit, code, populate in cases:
            with self.subTest(limit=constant), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / "Skills"
                populate(root)
                registry = skills_module.SkillRegistry(root)
                with mock.patch.object(skills_module, constant, limit):
                    with self.assertRaises(skills_module.SkillError):
                        registry.load()
                self.assertIn(code, self._diagnostic_codes(registry))

    def test_root_and_scan_budgets_are_shared_across_all_roots_and_fingerprints(self):
        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            first = base / "first"
            second = base / "second"
            first.mkdir()
            second.mkdir()
            (first / "one.txt").write_text("one", encoding="utf-8")
            (second / "two.txt").write_text("two", encoding="utf-8")

            root_limited = skills_module.SkillRegistry([first, second])
            with mock.patch.object(skills_module, "MAX_SKILL_ROOTS", 1):
                with self.assertRaisesRegex(
                    skills_module.SkillError,
                    "Skill scan exceeds 1 configured roots",
                ):
                    root_limited.load()
            self.assertEqual(root_limited.skills, ())
            self._assert_diagnostic(
                root_limited,
                code="skills.scan.root_limit",
                source=first.resolve(),
                message="Skill scan exceeds 1 configured roots.",
                exact_message=True,
            )

            file_limited = skills_module.SkillRegistry([first, second])
            with mock.patch.object(skills_module, "MAX_SKILL_SCAN_FILES", 1):
                with self.assertRaisesRegex(
                    skills_module.SkillError,
                    "Skill scan exceeds 1 files",
                ):
                    file_limited.load()
                with self.assertRaisesRegex(
                    skills_module.SkillError,
                    "Skill scan exceeds 1 files",
                ):
                    skills_module._build_roots_fingerprint([first, second])
            self._assert_diagnostic(
                file_limited,
                code="skills.scan.file_limit",
                source=second / "two.txt",
                message="Skill scan exceeds 1 files.",
                exact_message=True,
            )

            first_resources = base / "first-resources"
            second_resources = base / "second-resources"
            self._write_skill(
                first_resources,
                name="first-resource-skill",
                resources={"assets/one.txt": "one"},
            )
            second_skill = self._write_skill(
                second_resources,
                name="second-resource-skill",
                resources={"assets/two.txt": "two"},
            )
            resource_limited = skills_module.SkillRegistry(
                [first_resources, second_resources]
            )
            with mock.patch.object(skills_module, "MAX_SKILL_SCAN_FILES", 3):
                with self.assertRaisesRegex(
                    skills_module.SkillError,
                    "Skill scan exceeds 3 files",
                ):
                    resource_limited.load()
                with self.assertRaisesRegex(
                    skills_module.SkillError,
                    "Skill scan exceeds 3 files",
                ):
                    skills_module._build_roots_fingerprint(
                        [first_resources, second_resources]
                    )
            self._assert_diagnostic(
                resource_limited,
                code="skills.scan.file_limit",
                source=second_skill / "assets" / "two.txt",
                message="Skill scan exceeds 3 files.",
                exact_message=True,
            )

            directory_limited = skills_module.SkillRegistry([first, second])
            with mock.patch.object(skills_module, "MAX_SKILL_SCAN_DIRECTORIES", 1):
                with self.assertRaisesRegex(
                    skills_module.SkillError,
                    "Skill scan exceeds 1 directories",
                ):
                    directory_limited.load()
                with self.assertRaisesRegex(
                    skills_module.SkillError,
                    "Skill scan exceeds 1 directories",
                ):
                    skills_module._build_roots_fingerprint([first, second])
            self._assert_diagnostic(
                directory_limited,
                code="skills.scan.directory_limit",
                source=second,
                message="Skill scan exceeds 1 directories.",
                exact_message=True,
            )

    def test_archive_fingerprint_aggregate_budgets_precede_archive_open_and_keep_lkg(
        self,
    ):
        import qcopilots_common.skills as skills_module

        budget_cases = (
            (
                "candidate-count",
                (b"not-an-archive-a", b"not-an-archive-b"),
                1,
                1024,
                r"candidate",
            ),
            (
                "cumulative-bytes",
                (b"01234", b"56789"),
                8,
                9,
                r"byte",
            ),
        )
        for (
            case_name,
            archive_payloads,
            candidate_limit,
            byte_limit,
            category_pattern,
        ) in budget_cases:
            with (
                self.subTest(case=case_name),
                tempfile.TemporaryDirectory() as tmp,
            ):
                base = Path(tmp)
                roots = (base / "SkillsA", base / "SkillsB")
                self._write_skill(
                    roots[0],
                    name="archive-budget-lkg",
                    body="# Archive fingerprint budget LKG\n",
                )
                roots[1].mkdir()
                service = skills_module.SkillService(
                    roots,
                    validation_mode="compatible",
                    refresh_interval_seconds=0,
                )
                initial_registry = service._registry
                candidate_paths = []
                for index, payload in enumerate(archive_payloads):
                    candidate_path = (
                        roots[index % len(roots)] / f"aggregate-{index}.skill"
                    )
                    candidate_path.write_bytes(payload)
                    candidate_paths.append(candidate_path)

                with (
                    mock.patch.object(
                        skills_module,
                        "MAX_SKILL_ARCHIVE_FINGERPRINT_CANDIDATES",
                        candidate_limit,
                    ),
                    mock.patch.object(
                        skills_module,
                        "MAX_SKILL_ARCHIVE_FINGERPRINT_BYTES",
                        byte_limit,
                    ),
                    mock.patch.object(
                        skills_module,
                        "_archive_snapshot_source_digest",
                        side_effect=AssertionError(
                            "聚合预算失败前不得打开归档构建摘要"
                        ),
                    ) as archive_digest,
                    mock.patch.object(
                        skills_module.zipfile,
                        "ZipFile",
                        side_effect=AssertionError(
                            "聚合预算失败前不得构造 ZipFile"
                        ),
                    ) as zip_file,
                ):
                    lkg_tools = {tool.name: tool for tool in service.tools()}

                archive_digest.assert_not_called()
                zip_file.assert_not_called()
                self.assertIs(service._registry, initial_registry)
                self.assertEqual(
                    lkg_tools["archive-budget-lkg"].handler(
                        {"task": "Use archive-budget LKG"}
                    )["instructions"],
                    "# Archive fingerprint budget LKG\n",
                )
                lkg_listing = lkg_tools["list_skills"].handler({})
                reload_failures = [
                    diagnostic
                    for diagnostic in lkg_listing["diagnostics"]
                    if diagnostic["code"] == "skills.reload.failed"
                ]
                self.assertEqual(len(reload_failures), 1)
                self.assertRegex(
                    reload_failures[0]["message"].lower(),
                    category_pattern,
                )

                for candidate_path in candidate_paths:
                    candidate_path.unlink()
                recovered_tools = {tool.name: tool for tool in service.tools()}
                self.assertEqual(
                    recovered_tools["archive-budget-lkg"].handler(
                        {"task": "Use recovered archive budget"}
                    )["instructions"],
                    "# Archive fingerprint budget LKG\n",
                )
                self.assertNotIn(
                    "skills.reload.failed",
                    {
                        diagnostic["code"]
                        for diagnostic in recovered_tools["list_skills"].handler({})[
                            "diagnostics"
                        ]
                    },
                )

    def test_strict_archive_fingerprints_ignore_budgets_without_archive_open(
        self,
    ):
        import qcopilots_common.skills as skills_module

        def assert_stat_only_entry(fingerprint, root, relative_path):
            entries = [entry for entry in fingerprint if entry[1] == relative_path]
            self.assertEqual(len(entries), 1, fingerprint)
            entry = entries[0]
            path_stat = (root / relative_path).stat()
            self.assertEqual(entry[2], "file")
            self.assertEqual(entry[3], path_stat.st_mtime_ns)
            self.assertEqual(entry[4], path_stat.st_size)
            self.assertIsNone(entry[-1])

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            instructions = "# Strict archive fingerprint directory skill\n"
            self._write_skill(
                root,
                name="strict-directory",
                body=instructions,
            )
            initial_archives = {
                "ignored-initial.skill": b"not-an-archive-a",
                "ignored-initial.zip": b"not-an-archive-b",
            }
            for relative_path, payload in initial_archives.items():
                (root / relative_path).write_bytes(payload)
            self.assertGreater(len(initial_archives), 1)
            self.assertGreater(sum(map(len, initial_archives.values())), 1)

            with (
                mock.patch.object(
                    skills_module,
                    "MAX_SKILL_ARCHIVE_FINGERPRINT_CANDIDATES",
                    1,
                ),
                mock.patch.object(
                    skills_module,
                    "MAX_SKILL_ARCHIVE_FINGERPRINT_BYTES",
                    1,
                ),
                mock.patch.object(
                    skills_module,
                    "_archive_snapshot_source_digest",
                    side_effect=AssertionError(
                        "严格模式指纹不得打开归档构建摘要"
                    ),
                ) as archive_digest,
                mock.patch.object(
                    skills_module.zipfile,
                    "ZipFile",
                    side_effect=AssertionError(
                        "严格模式首次加载和刷新均不得构造 ZipFile"
                    ),
                ) as zip_file,
            ):
                initial_fingerprint = skills_module._build_roots_fingerprint(
                    [root],
                    validation_mode="strict",
                )
                service = skills_module.SkillService(
                    [root],
                    validation_mode="strict",
                    refresh_interval_seconds=0,
                )
                initial_tools = {tool.name: tool for tool in service.tools()}
                initial_listing = initial_tools["list_skills"].handler({})
                initial_service_fingerprint = service._fingerprint

                runtime_relative_path = "ignored-runtime.skill"
                (root / runtime_relative_path).write_bytes(b"not-an-archive-runtime")
                runtime_fingerprint = skills_module._build_roots_fingerprint(
                    [root],
                    validation_mode="strict",
                )
                refreshed_tools = {tool.name: tool for tool in service.tools()}
                refreshed_listing = refreshed_tools["list_skills"].handler({})
                refreshed_service_fingerprint = service._fingerprint

            archive_digest.assert_not_called()
            zip_file.assert_not_called()
            self.assertEqual(initial_service_fingerprint, initial_fingerprint)
            self.assertEqual(refreshed_service_fingerprint, runtime_fingerprint)
            for relative_path in initial_archives:
                assert_stat_only_entry(initial_fingerprint, root, relative_path)
                assert_stat_only_entry(runtime_fingerprint, root, relative_path)
            assert_stat_only_entry(
                runtime_fingerprint,
                root,
                runtime_relative_path,
            )
            self.assertEqual(
                initial_tools["strict-directory"].handler(
                    {"task": "Use strict initial snapshot"}
                )["instructions"],
                instructions,
            )
            self.assertEqual(
                refreshed_tools["strict-directory"].handler(
                    {"task": "Use strict refreshed snapshot"}
                )["instructions"],
                instructions,
            )
            for listing in (initial_listing, refreshed_listing):
                self.assertEqual(
                    [skill["slug"] for skill in listing["skills"]],
                    ["strict-directory"],
                )
                self.assertNotIn(
                    "skills.reload.failed",
                    {
                        diagnostic["code"]
                        for diagnostic in listing["diagnostics"]
                    },
                )

    def test_archive_fingerprint_distinguishes_skill_resources_from_candidates(self):
        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            payload = b"not-a-skill-archive\x00\xff"
            skill_dir = self._write_skill(
                root,
                name="directory-archive-resource",
                resources={"assets/payload.zip": payload},
            )
            resource_path = skill_dir / "assets" / "payload.zip"

            with (
                mock.patch.object(
                    skills_module,
                    "_archive_snapshot_source_digest",
                    side_effect=AssertionError(
                        "目录技能内的普通 ZIP 资源不得构建归档技能摘要"
                    ),
                ) as archive_digest,
                mock.patch.object(
                    skills_module.zipfile,
                    "ZipFile",
                    side_effect=AssertionError(
                        "目录技能内的普通 ZIP 资源不得进入 ZipFile 预检"
                    ),
                ) as zip_file,
            ):
                fingerprint = skills_module._build_roots_fingerprint(
                    [root],
                    validation_mode="compatible",
                )
                service = skills_module.SkillService(
                    [root],
                    refresh_interval_seconds=0,
                )
                resource_result = {
                    tool.name: tool for tool in service.tools()
                }["read_skill_resource"].handler(
                    {
                        "slug": "directory-archive-resource",
                        "resource": "assets/payload.zip",
                    }
                )

            archive_digest.assert_not_called()
            zip_file.assert_not_called()
            resource_entries = [
                entry
                for entry in fingerprint
                if entry[1] == "directory-archive-resource/assets/payload.zip"
            ]
            self.assertEqual(len(resource_entries), 1, fingerprint)
            resource_entry = resource_entries[0]
            self.assertEqual(resource_entry[2], "file")
            self.assertEqual(resource_entry[4], resource_path.stat().st_size)
            self.assertIsNone(resource_entry[-1])
            self.assertEqual(resource_result["encoding"], "base64")
            self.assertEqual(
                resource_result["blob"],
                base64.b64encode(payload).decode("ascii"),
            )

            package_dir = root / "packages"
            package_dir.mkdir()
            standalone_archive = package_dir / "standalone.skill"
            with zipfile.ZipFile(
                standalone_archive,
                "w",
                compression=zipfile.ZIP_STORED,
            ) as archive:
                archive.writestr(
                    "standalone/SKILL.md",
                    self._canonical_document("standalone"),
                )
            candidate_fingerprint = skills_module._build_roots_fingerprint(
                [root],
                validation_mode="compatible",
            )
            candidate_entries = [
                entry
                for entry in candidate_fingerprint
                if entry[1] == "packages/standalone.skill"
            ]
            self.assertEqual(len(candidate_entries), 1, candidate_fingerprint)
            tag, digest = candidate_entries[0][-1]
            self.assertEqual(tag, "archive-snapshot-sha256")
            self.assertRegex(digest, r"^[0-9a-f]{64}$")

    def test_skill_root_and_validation_mode_are_configured(self):
        from qcopilots_common.skills import (
            SkillError,
            SkillRegistry,
            build_skill_service,
            configured_skill_roots,
            configured_skill_validation_mode,
        )

        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            plugin_dir = base / "plugin"
            root = plugin_dir / "Skills"
            self._write_skill(root, name="zulu")
            self._write_skill(root, name="alpha")
            manifest = plugin_dir / "qcopilots_service.json"
            manifest.write_text(
                json.dumps(
                    {
                        "skillsRoot": "Skills",
                        "skills": {"validation_mode": "compatible"},
                    }
                ),
                encoding="utf-8",
            )

            roots = configured_skill_roots(plugin_dir)
            self.assertEqual(roots, [root.resolve()])
            self.assertEqual(configured_skill_validation_mode(plugin_dir), "compatible")
            service = build_skill_service(plugin_dir)
            self.assertEqual(service.validation_mode, "compatible")
            self.assertEqual(
                [skill.slug for skill in service.registry.skills], ["alpha", "zulu"]
            )

            manifest.write_text(
                json.dumps({"skillsRoot": "Skills"}), encoding="utf-8"
            )
            self.assertEqual(configured_skill_validation_mode(plugin_dir), "strict")
            manifest.write_text(
                json.dumps(
                    {
                        "skillsRoot": "Skills",
                        "skills": {
                            "validation_mode": "PRIVATE-MODE-MUST-NOT-LEAK"
                        },
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaises(SkillError) as invalid_mode:
                configured_skill_validation_mode(plugin_dir)
            invalid_mode_message = str(invalid_mode.exception)
            self.assertIn(str(manifest), invalid_mode_message)
            self.assertIn("validation_mode", invalid_mode_message)
            self.assertNotIn("PRIVATE-MODE-MUST-NOT-LEAK", invalid_mode_message)

            registry = SkillRegistry([root, root])
            self.assertEqual(registry.roots, (root.resolve(),))

    def test_refresh_is_single_flight_and_readers_keep_old_snapshot_during_reload(
        self,
    ):
        import qcopilots_common.skills as skills_module

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="alpha",
                body="# Snapshot before\n",
                resources={"references/state.txt": "resource before\n"},
            )
            service = skills_module.SkillService(
                [root],
                refresh_interval_seconds=0,
            )
            before_tools = {tool.name: tool for tool in service._tools}
            before_prompts = {prompt.name: prompt for prompt in service._prompts}
            before_resources = {
                resource.name: resource for resource in service._resources
            }
            self.assertEqual(
                before_tools["alpha"].handler({"task": "Before reload"})[
                    "instructions"
                ],
                "# Snapshot before\n",
            )

            (skill_dir / "SKILL.md").write_text(
                self._canonical_document(
                    "alpha",
                    body="# Snapshot after with a different size\n",
                ),
                encoding="utf-8",
            )
            candidate_ready = threading.Event()
            release_candidate = threading.Event()
            candidate_bodies = []
            real_builder = skills_module._build_stable_skill_snapshot

            def build_and_pause(*args, **kwargs):
                snapshot = real_builder(*args, **kwargs)
                candidate_tools = {tool.name: tool for tool in snapshot.tools}
                candidate_bodies.append(
                    candidate_tools["alpha"].handler({"task": "Candidate"})[
                        "instructions"
                    ]
                )
                candidate_ready.set()
                if not release_candidate.wait(timeout=5):
                    raise AssertionError(
                        "Timed out waiting to publish candidate snapshot"
                    )
                return snapshot

            def read_during_reload():
                tools = {tool.name: tool for tool in service.tools()}
                prompts = {prompt.name: prompt for prompt in service.prompts()}
                resources = {
                    resource.name: resource for resource in service.resources()
                }
                dynamic_result = tools["alpha"].handler(
                    {"task": "Read the published snapshot"}
                )
                static_result = tools["read_skill"].handler({"slug": "alpha"})
                prompt_result = prompts["alpha"].handler(
                    {"task": "Prompt from the published snapshot"}
                )
                tool_resource_result = tools["read_skill_resource"].handler(
                    {
                        "slug": "alpha",
                        "resource": "references/state.txt",
                    }
                )
                return {
                    "tool_names": tuple(sorted(tools)),
                    "prompt_names": tuple(sorted(prompts)),
                    "resource_names": tuple(sorted(resources)),
                    "dynamic_instructions": dynamic_result["instructions"],
                    "static_instructions": static_result["content"],
                    "static_skill_content": static_result["skill_content"],
                    "prompt_text": prompt_result["messages"][0]["content"]["text"],
                    "tool_resource": tool_resource_result["content"],
                    "provider_resource": resources[
                        "alpha/references/state.txt"
                    ].handler(),
                }

            worker_threads = ()
            with mock.patch.object(
                skills_module,
                "_build_stable_skill_snapshot",
                side_effect=build_and_pause,
            ) as stable_builder:
                with ThreadPoolExecutor(
                    max_workers=3,
                    thread_name_prefix="skill-single-flight",
                ) as executor:
                    writer_future = executor.submit(service.ensure_current)
                    reader_future = None
                    follower_future = None
                    try:
                        self.assertTrue(
                            candidate_ready.wait(timeout=5),
                            "Writer did not finish building the candidate snapshot",
                        )
                        self.assertTrue(
                            service._refresh_in_progress,
                            "Writer did not hold the single-flight publication gate",
                        )
                        self.assertEqual(
                            candidate_bodies,
                            ["# Snapshot after with a different size\n"],
                        )
                        reader_future = executor.submit(read_during_reload)
                        follower_future = executor.submit(service.ensure_current)
                        reader_result = reader_future.result(timeout=5)
                        self.assertEqual(
                            reader_result["tool_names"],
                            tuple(sorted(before_tools)),
                        )
                        self.assertEqual(
                            reader_result["prompt_names"],
                            tuple(sorted(before_prompts)),
                        )
                        self.assertEqual(
                            reader_result["resource_names"],
                            tuple(sorted(before_resources)),
                        )
                        self.assertEqual(
                            reader_result["dynamic_instructions"],
                            "# Snapshot before\n",
                        )
                        self.assertEqual(
                            reader_result["static_instructions"],
                            "# Snapshot before\n",
                        )
                        self.assertIn(
                            "# Snapshot before",
                            reader_result["static_skill_content"],
                        )
                        self.assertNotIn(
                            "Snapshot after",
                            reader_result["static_skill_content"],
                        )
                        self.assertIn(
                            "# Snapshot before",
                            reader_result["prompt_text"],
                        )
                        self.assertNotIn(
                            "Snapshot after",
                            reader_result["prompt_text"],
                        )
                        self.assertEqual(
                            reader_result["tool_resource"],
                            "resource before\n",
                        )
                        self.assertEqual(
                            reader_result["provider_resource"],
                            "resource before\n",
                        )
                        self.assertIsNone(follower_future.result(timeout=5))
                        self.assertEqual(stable_builder.call_count, 1)
                        self.assertFalse(
                            writer_future.done(),
                            "Writer published before the deterministic release point",
                        )
                    finally:
                        release_candidate.set()
                    self.assertIsNone(writer_future.result(timeout=5))
                    worker_threads = tuple(executor._threads)

            self.assertGreaterEqual(len(worker_threads), 2)
            self.assertTrue(all(not thread.is_alive() for thread in worker_threads))
            self.assertEqual(stable_builder.call_count, 1)
            final_tools = {tool.name: tool for tool in service._tools}
            self.assertIsNot(final_tools["alpha"], before_tools["alpha"])
            self.assertEqual(
                final_tools["alpha"].handler({"task": "After commit"})[
                    "instructions"
                ],
                "# Snapshot after with a different size\n",
            )

            shutil.rmtree(skill_dir)
            deleted_tools = {tool.name: tool for tool in service.tools()}
            self.assertNotIn("alpha", deleted_tools)
            self.assertEqual(
                deleted_tools["list_skills"].handler({})["skills"],
                [],
            )
            self.assertNotIn(
                "alpha",
                {prompt.name for prompt in service.prompts()},
            )
            self.assertEqual(service.resources(), [])

            new_provider = skills_module.SkillService(
                [root],
                refresh_interval_seconds=0,
            )
            self.assertNotIn(
                "alpha",
                {tool.name for tool in new_provider.tools()},
            )
            self.assertEqual(new_provider.registry.skills, ())

    def test_path_identity_matching_ignores_only_windows_changed_time(self):
        import stat

        import qcopilots_common.skills as skills_module

        identity_values = {
            "device": 11,
            "inode": 22,
            "file_type": stat.S_IFREG,
            "file_attributes": 0,
            "size": 33,
            "modified_ns": 44,
            "changed_ns": 55,
        }

        def identity(**changes):
            values = dict(identity_values)
            values.update(changes)
            return skills_module._PathIdentity(**values)

        expected = identity()
        changed_time = identity(changed_ns=expected.changed_ns + 1)
        self.assertTrue(
            skills_module._path_identities_match(
                changed_time,
                expected,
                compare_changed_ns=False,
            )
        )
        self.assertFalse(
            skills_module._path_identities_match(
                changed_time,
                expected,
                compare_changed_ns=True,
            )
        )

        stable_field_changes = {
            "device": expected.device + 1,
            "inode": expected.inode + 1,
            "file_type": stat.S_IFDIR,
            "file_attributes": getattr(
                stat,
                "FILE_ATTRIBUTE_REPARSE_POINT",
                0x400,
            ),
            "size": expected.size + 1,
            "modified_ns": expected.modified_ns + 1,
        }
        for field, value in stable_field_changes.items():
            with self.subTest(field=field):
                self.assertFalse(
                    skills_module._path_identities_match(
                        identity(**{field: value}),
                        expected,
                        compare_changed_ns=False,
                    )
                )

        metadata_expected = identity(
            size=None,
            modified_ns=None,
            changed_ns=None,
        )
        metadata_equal_actual = identity(
            size=None,
            modified_ns=None,
            changed_ns=None,
        )
        self.assertIsNot(metadata_equal_actual, metadata_expected)
        self.assertEqual(metadata_equal_actual, metadata_expected)
        self.assertTrue(
            skills_module._path_identities_match(
                metadata_equal_actual,
                metadata_expected,
                compare_changed_ns=False,
            )
        )
        self.assertTrue(
            skills_module._path_identities_match(
                metadata_equal_actual,
                metadata_expected,
                compare_changed_ns=True,
            )
        )

        metadata_actual = identity(
            inode=metadata_expected.inode + 202,
            size=None,
            modified_ns=None,
            changed_ns=None,
        )
        self.assertNotEqual(metadata_actual, metadata_expected)
        self.assertFalse(
            skills_module._path_identities_match(
                metadata_actual,
                metadata_expected,
                compare_changed_ns=False,
            )
        )

        actual = identity(inode=expected.inode + 101)
        self.assertNotEqual(actual, expected)
        identity_path = Path("identity.skill")
        with (
            mock.patch.object(
                skills_module,
                "_path_identity_from_stat",
                return_value=actual,
            ) as identity_from_stat,
            mock.patch.object(
                skills_module,
                "_path_identities_match",
                return_value=True,
            ) as identities_match,
        ):
            skills_module._verify_stat_identity(
                mock.sentinel.path_stat,
                expected,
                identity_path,
            )
        identity_from_stat.assert_called_once_with(
            mock.sentinel.path_stat,
            include_content=True,
        )
        identities_match.assert_called_once_with(
            actual,
            expected,
            compare_changed_ns=os.name != "nt",
        )

        metadata_identity_path = Path("metadata-identity.skill")
        with (
            mock.patch.object(
                skills_module,
                "_path_identity_from_stat",
                return_value=metadata_actual,
            ) as metadata_identity_from_stat,
            mock.patch.object(
                skills_module,
                "_path_identities_match",
                return_value=True,
            ) as metadata_identities_match,
        ):
            skills_module._verify_stat_identity(
                mock.sentinel.metadata_path_stat,
                metadata_expected,
                metadata_identity_path,
            )
        metadata_identity_from_stat.assert_called_once_with(
            mock.sentinel.metadata_path_stat,
            include_content=False,
        )
        metadata_identities_match.assert_called_once_with(
            metadata_actual,
            metadata_expected,
            compare_changed_ns=os.name != "nt",
        )

    def test_same_size_mtime_edits_use_deterministic_resource_identity_branches(self):
        import qcopilots_common.skills as skills_module
        from qcopilots_common.mcp_http import ToolError

        def source_digest(fingerprint, rel_path, expected_tag):
            matches = [
                entry
                for entry in fingerprint
                if entry[1] == rel_path
            ]
            self.assertEqual(len(matches), 1, fingerprint)
            tag, digest = matches[0][-1]
            self.assertEqual(tag, expected_tag)
            self.assertRegex(digest, r"^[0-9a-f]{64}$")
            return digest

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            skill_dir = self._write_skill(
                root,
                name="same-size-hotload",
                body="# Initial A\n",
                resources={"references/state.txt": "resource-A\n"},
            )
            instructions_path = skill_dir / "SKILL.md"
            resource_path = skill_dir / "references" / "state.txt"
            initial_fingerprint = skills_module._build_roots_fingerprint([root])
            initial_source_digest = source_digest(
                initial_fingerprint,
                "same-size-hotload/SKILL.md",
                "instructions-sha256",
            )
            service = skills_module.SkillService([root], refresh_interval_seconds=0)

            initial_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                initial_tools["same-size-hotload"].handler(
                    {"task": "Read the initial body"}
                )["instructions"],
                "# Initial A\n",
            )
            self.assertEqual(
                initial_tools["read_skill_resource"].handler(
                    {
                        "slug": "same-size-hotload",
                        "resource": "references/state.txt",
                    }
                )["content"],
                "resource-A\n",
            )

            original_instructions = instructions_path.read_bytes()
            replacement_instructions = original_instructions.replace(
                b"# Initial A\n",
                b"# Updated B\n",
            )
            self.assertEqual(original_instructions.count(b"# Initial A\n"), 1)
            self.assertEqual(
                len(replacement_instructions),
                len(original_instructions),
            )
            instructions_stat = instructions_path.stat()
            instructions_path.write_bytes(replacement_instructions)
            os.utime(
                instructions_path,
                ns=(instructions_stat.st_atime_ns, instructions_stat.st_mtime_ns),
            )
            restored_instructions_stat = instructions_path.stat()
            self.assertEqual(
                restored_instructions_stat.st_size,
                instructions_stat.st_size,
            )
            self.assertEqual(
                restored_instructions_stat.st_mtime_ns,
                instructions_stat.st_mtime_ns,
            )
            changed_fingerprint = skills_module._build_roots_fingerprint([root])
            changed_source_digest = source_digest(
                changed_fingerprint,
                "same-size-hotload/SKILL.md",
                "instructions-sha256",
            )
            self.assertNotEqual(initial_source_digest, changed_source_digest)
            self.assertEqual(
                changed_source_digest,
                hashlib.sha256(replacement_instructions).hexdigest(),
            )

            refreshed_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                refreshed_tools["same-size-hotload"].handler(
                    {"task": "Read the refreshed body"}
                )["instructions"],
                "# Updated B\n",
            )
            published_skill = service._registry.get("same-size-hotload")
            resource_key = skills_module._resource_identity_key(
                "references/state.txt"
            )
            published_resource_identity = published_skill._resource_identities[
                resource_key
            ]
            published_resource_tool = refreshed_tools["read_skill_resource"]

            original_resource = resource_path.read_bytes()
            replacement_resource = original_resource.replace(
                b"resource-A\n",
                b"resource-B\n",
            )
            self.assertEqual(original_resource.count(b"resource-A\n"), 1)
            self.assertEqual(len(replacement_resource), len(original_resource))
            resource_stat = resource_path.stat()
            resource_path.write_bytes(replacement_resource)
            os.utime(
                resource_path,
                ns=(resource_stat.st_atime_ns, resource_stat.st_mtime_ns),
            )
            restored_resource_stat = resource_path.stat()
            self.assertEqual(restored_resource_stat.st_size, resource_stat.st_size)
            self.assertEqual(
                restored_resource_stat.st_mtime_ns,
                resource_stat.st_mtime_ns,
            )

            real_identity_from_stat = skills_module._path_identity_from_stat
            actual_resource_identity = real_identity_from_stat(
                resource_path.stat(follow_symlinks=False),
                include_content=True,
            )
            stable_fields = (
                "device",
                "inode",
                "file_type",
                "file_attributes",
                "size",
                "modified_ns",
            )
            self.assertEqual(
                tuple(
                    getattr(actual_resource_identity, field)
                    for field in stable_fields
                ),
                tuple(
                    getattr(published_resource_identity, field)
                    for field in stable_fields
                ),
            )

            def accept_stable_resource_identity(path_stat, *, include_content):
                actual = real_identity_from_stat(
                    path_stat,
                    include_content=include_content,
                )
                if include_content and all(
                    getattr(actual, field)
                    == getattr(published_resource_identity, field)
                    for field in stable_fields
                ):
                    return published_resource_identity
                return actual

            with mock.patch.object(
                skills_module,
                "_path_identity_from_stat",
                side_effect=accept_stable_resource_identity,
            ):
                accepted_identity = skills_module._capture_path_identity(
                    resource_path,
                    require_file=True,
                    include_content=True,
                )
                self.assertEqual(
                    accepted_identity,
                    published_resource_identity,
                )
                self.assertEqual(
                    published_resource_tool.handler(
                        {
                            "slug": "same-size-hotload",
                            "resource": "references/state.txt",
                        }
                    )["content"],
                    "resource-B\n",
                )

            changed_identity = skills_module._PathIdentity(
                device=published_resource_identity.device,
                inode=published_resource_identity.inode + 1,
                file_type=published_resource_identity.file_type,
                file_attributes=published_resource_identity.file_attributes,
                size=published_resource_identity.size,
                modified_ns=published_resource_identity.modified_ns,
                changed_ns=published_resource_identity.changed_ns,
            )
            self.assertNotEqual(changed_identity, published_resource_identity)
            real_capture_identity = skills_module._capture_path_identity
            trusted_resource_path = skills_module._absolute_lexical_path(resource_path)

            def report_changed_resource_identity(
                path,
                *,
                require_directory=False,
                require_file=False,
                include_content=False,
            ):
                if (
                    skills_module._absolute_lexical_path(Path(path))
                    == trusted_resource_path
                    and require_file
                    and include_content
                ):
                    return changed_identity
                return real_capture_identity(
                    path,
                    require_directory=require_directory,
                    require_file=require_file,
                    include_content=include_content,
                )

            with mock.patch.object(
                skills_module,
                "_capture_path_identity",
                side_effect=report_changed_resource_identity,
            ):
                observed_changed_identity = skills_module._capture_path_identity(
                    resource_path,
                    require_file=True,
                    include_content=True,
                )
                self.assertEqual(observed_changed_identity, changed_identity)
                self.assertNotEqual(
                    observed_changed_identity,
                    published_resource_identity,
                )
                with self.assertRaises(ToolError) as changed_resource_error:
                    published_resource_tool.handler(
                        {
                            "slug": "same-size-hotload",
                            "resource": "references/state.txt",
                        }
                    )
            self.assertEqual(
                str(changed_resource_error.exception),
                "The requested skill resource is unavailable.",
            )

    def test_stored_archive_source_digest_tracks_same_size_instruction_and_resource_edits(
        self,
    ):
        import qcopilots_common.skills as skills_module

        def source_digest(fingerprint, rel_path):
            matches = [entry for entry in fingerprint if entry[1] == rel_path]
            self.assertEqual(len(matches), 1, fingerprint)
            tag, digest = matches[0][-1]
            self.assertEqual(tag, "archive-snapshot-sha256")
            self.assertRegex(digest, r"^[0-9a-f]{64}$")
            return digest

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            root.mkdir(parents=True)
            archive_path = root / "stored-hotload.skill"

            def write_archive(body, resource):
                with zipfile.ZipFile(
                    archive_path,
                    "w",
                    compression=zipfile.ZIP_STORED,
                ) as archive:
                    archive.writestr(
                        "stored-hotload/SKILL.md",
                        self._canonical_document("stored-hotload", body=body),
                    )
                    archive.writestr(
                        "stored-hotload/references/state.txt",
                        resource,
                    )

            write_archive("# Initial A\n", "resource-A\n")
            initial_stat = archive_path.stat()
            initial_size = initial_stat.st_size
            initial_fingerprint = skills_module._build_roots_fingerprint(
                [root],
                validation_mode="compatible",
            )
            initial_digest = source_digest(
                initial_fingerprint,
                "stored-hotload.skill",
            )
            service = skills_module.SkillService(
                [root],
                validation_mode="compatible",
                refresh_interval_seconds=0,
            )
            initial_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                initial_tools["stored-hotload"].handler({"task": "Initial archive"})[
                    "instructions"
                ],
                "# Initial A\n",
            )

            write_archive("# Initial A\n", "resource-B\n")
            self.assertEqual(archive_path.stat().st_size, initial_size)
            os.utime(
                archive_path,
                ns=(initial_stat.st_atime_ns, initial_stat.st_mtime_ns),
            )
            resource_stat = archive_path.stat()
            self.assertEqual(resource_stat.st_size, initial_size)
            self.assertEqual(resource_stat.st_mtime_ns, initial_stat.st_mtime_ns)
            resource_fingerprint = skills_module._build_roots_fingerprint(
                [root],
                validation_mode="compatible",
            )
            resource_digest = source_digest(
                resource_fingerprint,
                "stored-hotload.skill",
            )
            self.assertNotEqual(resource_digest, initial_digest)
            resource_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                resource_tools["read_skill_resource"].handler(
                    {
                        "slug": "stored-hotload",
                        "resource": "references/state.txt",
                    }
                )["content"],
                "resource-B\n",
            )

            write_archive("# Updated B\n", "resource-B\n")
            self.assertEqual(archive_path.stat().st_size, initial_size)
            os.utime(
                archive_path,
                ns=(initial_stat.st_atime_ns, initial_stat.st_mtime_ns),
            )
            instructions_stat = archive_path.stat()
            self.assertEqual(instructions_stat.st_size, initial_size)
            self.assertEqual(instructions_stat.st_mtime_ns, initial_stat.st_mtime_ns)
            instructions_fingerprint = skills_module._build_roots_fingerprint(
                [root],
                validation_mode="compatible",
            )
            instructions_digest = source_digest(
                instructions_fingerprint,
                "stored-hotload.skill",
            )
            self.assertNotEqual(instructions_digest, resource_digest)
            refreshed_tools = {tool.name: tool for tool in service.tools()}
            self.assertEqual(
                refreshed_tools["stored-hotload"].handler(
                    {"task": "Refreshed archive"}
                )["instructions"],
                "# Updated B\n",
            )

    def test_manifest_byte_budget_boundary_and_runtime_lkg_recovery(self):
        import qcopilots_common.skills as skills_module

        readers = (
            ("configured-roots", skills_module.configured_skill_roots),
            ("configured-mode", skills_module.configured_skill_validation_mode),
            ("build-service", skills_module.build_skill_service),
        )

        def write_exact_size(manifest_path, data, size):
            encoded = json.dumps(data, separators=(",", ":")).encode("utf-8")
            self.assertLessEqual(len(encoded), size)
            manifest_path.write_bytes(encoded + (b" " * (size - len(encoded))))
            self.assertEqual(manifest_path.stat().st_size, size)

        with tempfile.TemporaryDirectory() as tmp:
            plugin_dir = Path(tmp) / "plugin"
            default_root = plugin_dir / "Skills"
            self._write_skill(default_root, name="manifest-boundary")
            manifest_path = plugin_dir / "qcopilots_service.json"
            exact_limit = skills_module.MAX_FILE_READ_BYTES
            write_exact_size(
                manifest_path,
                {
                    "skillsRoot": "Skills",
                    "skills_validation_mode": "compatible",
                },
                exact_limit,
            )

            with mock.patch.dict(os.environ, {}, clear=True):
                self.assertEqual(
                    skills_module.configured_skill_roots(plugin_dir),
                    [default_root.resolve()],
                )
                self.assertEqual(
                    skills_module.configured_skill_validation_mode(plugin_dir),
                    "compatible",
                )
                boundary_service = skills_module.build_skill_service(plugin_dir)
            self.assertEqual(boundary_service.validation_mode, "compatible")
            self.assertEqual(
                [skill.slug for skill in boundary_service.registry.skills],
                ["manifest-boundary"],
            )

            private_marker = "PRIVATE-OVERSIZE-MANIFEST-MUST-NOT-LEAK"
            write_exact_size(
                manifest_path,
                {
                    "skillsRoot": "Skills",
                    "private_marker": private_marker,
                },
                exact_limit + 1,
            )
            with mock.patch.dict(os.environ, {}, clear=True):
                for reader_name, reader in readers:
                    with self.subTest(phase="initial-overflow", reader=reader_name):
                        with self.assertRaises(
                            skills_module.SkillError
                        ) as error_context:
                            reader(plugin_dir)
                        message = str(error_context.exception)
                        self.assertIn(str(manifest_path), message)
                        self.assertRegex(message.lower(), r"byte|large|size|exceed")
                        self.assertNotIn(private_marker, message)

        with tempfile.TemporaryDirectory() as tmp:
            plugin_dir = Path(tmp) / "plugin"
            plugin_dir.mkdir()
            (plugin_dir / "Skills").mkdir()
            root_a = plugin_dir / "SkillsA"
            root_b = plugin_dir / "SkillsB"
            self._write_skill(
                root_a,
                name="manifest-budget-alpha",
                body="# Manifest budget alpha LKG\n",
            )
            self._write_skill(
                root_b,
                name="manifest-budget-bravo",
                body="# Manifest budget bravo repaired\n",
            )
            manifest_path = plugin_dir / "qcopilots_service.json"
            manifest_path.write_text(
                json.dumps({"skillsRoot": "SkillsA"}),
                encoding="utf-8",
            )

            with mock.patch.dict(os.environ, {}, clear=True):
                service = skills_module.build_skill_service(plugin_dir)
                service.refresh_interval_seconds = 0
                private_marker = "PRIVATE-RUNTIME-MANIFEST-MUST-NOT-LEAK"
                overlimit_payload = json.dumps(
                    {
                        "skillsRoot": "SkillsB",
                        "private_marker": private_marker,
                    },
                    separators=(",", ":"),
                ).encode("utf-8")
                runtime_limit = len(overlimit_payload) - 1
                manifest_path.write_bytes(overlimit_payload)
                self.assertEqual(
                    manifest_path.stat().st_size,
                    runtime_limit + 1,
                )

                with mock.patch.object(
                    skills_module,
                    "MAX_FILE_READ_BYTES",
                    runtime_limit,
                ):
                    lkg_tools = {tool.name: tool for tool in service.tools()}
                self.assertEqual(
                    lkg_tools["manifest-budget-alpha"].handler(
                        {"task": "Use byte-budget LKG"}
                    )["instructions"],
                    "# Manifest budget alpha LKG\n",
                )
                self.assertNotIn("manifest-budget-bravo", lkg_tools)
                lkg_listing = lkg_tools["list_skills"].handler({})
                reload_failures = [
                    diagnostic
                    for diagnostic in lkg_listing["diagnostics"]
                    if diagnostic["code"] == "skills.reload.failed"
                ]
                self.assertEqual(len(reload_failures), 1)
                self.assertIn(str(manifest_path), reload_failures[0]["message"])
                self.assertRegex(
                    reload_failures[0]["message"].lower(),
                    r"byte|large|size|exceed",
                )
                self.assertNotIn(
                    private_marker,
                    json.dumps(reload_failures, ensure_ascii=False),
                )

                manifest_path.write_text(
                    json.dumps({"skillsRoot": "SkillsB"}),
                    encoding="utf-8",
                )
                repaired_tools = {tool.name: tool for tool in service.tools()}
                self.assertNotIn("manifest-budget-alpha", repaired_tools)
                self.assertEqual(
                    repaired_tools["manifest-budget-bravo"].handler(
                        {"task": "Use repaired byte-budget manifest"}
                    )["instructions"],
                    "# Manifest budget bravo repaired\n",
                )
                self.assertNotIn(
                    "skills.reload.failed",
                    {
                        diagnostic["code"]
                        for diagnostic in repaired_tools["list_skills"].handler({})[
                            "diagnostics"
                        ]
                    },
                )

    def test_manifest_json_structure_budgets_cover_all_entrypoints(self):
        import qcopilots_common.skills as skills_module

        readers = (
            ("configured-roots", skills_module.configured_skill_roots),
            ("configured-mode", skills_module.configured_skill_validation_mode),
            ("build-service", skills_module.build_skill_service),
        )
        private_marker = "PRIVATE-MANIFEST-STRUCTURE-MUST-NOT-LEAK"
        deep_value = private_marker
        for _ in range(6):
            deep_value = {"nested": deep_value}
        structure_cases = (
            (
                "recursion-depth",
                "MAX_JSON_SAFE_RECURSION_DEPTH",
                2,
                {
                    "skillsRoot": "Skills",
                    "private_structure": deep_value,
                },
                r"recursion.*depth|depth.*budget",
            ),
            (
                "visited-nodes",
                "MAX_JSON_SAFE_VISITED_NODES",
                6,
                {
                    "skillsRoot": "Skills",
                    "private_structure": [private_marker] * 8,
                },
                r"visited.*node|node.*budget",
            ),
        )

        for (
            case_name,
            constant_name,
            limit,
            manifest_data,
            category_pattern,
        ) in structure_cases:
            with (
                self.subTest(case=case_name),
                tempfile.TemporaryDirectory() as tmp,
            ):
                plugin_dir = Path(tmp) / "plugin"
                self._write_skill(plugin_dir / "Skills", name="manifest-default")
                manifest_path = plugin_dir / "qcopilots_service.json"
                manifest_path.write_text(
                    json.dumps(manifest_data),
                    encoding="utf-8",
                )
                with (
                    mock.patch.dict(os.environ, {}, clear=True),
                    mock.patch.object(skills_module, constant_name, limit),
                ):
                    for reader_name, reader in readers:
                        with self.subTest(reader=reader_name):
                            with self.assertRaises(
                                skills_module.SkillError
                            ) as error_context:
                                reader(plugin_dir)
                            message = str(error_context.exception)
                            self.assertIn(str(manifest_path), message)
                            self.assertRegex(message.lower(), category_pattern)
                            self.assertNotIn(private_marker, message)

    def test_manifest_missing_defaults_and_invalid_initial_manifest_is_diagnostic(
        self,
    ):
        from qcopilots_common.skills import (
            SkillError,
            build_skill_service,
            configured_skill_roots,
            configured_skill_validation_mode,
        )

        with tempfile.TemporaryDirectory() as tmp:
            plugin_dir = Path(tmp) / "plugin"
            default_root = plugin_dir / "Skills"
            self._write_skill(default_root, name="manifest-default")
            manifest_path = plugin_dir / "qcopilots_service.json"

            with mock.patch.dict(os.environ, {}, clear=True):
                self.assertEqual(
                    configured_skill_roots(plugin_dir),
                    [default_root.resolve()],
                )
                self.assertEqual(
                    configured_skill_validation_mode(plugin_dir),
                    "strict",
                )
                default_service = build_skill_service(plugin_dir)
                self.assertEqual(
                    [skill.slug for skill in default_service.registry.skills],
                    ["manifest-default"],
                )

                sensitive_marker = "PRIVATE-MANIFEST-CONTENT-MUST-NOT-LEAK"
                invalid_cases = (
                    (
                        "invalid-utf8",
                        sensitive_marker.encode("ascii") + b"\xff",
                        r"utf-?8|unicode",
                    ),
                    (
                        "invalid-json",
                        (
                            '{"skillsRoot": "'
                            + sensitive_marker
                            + '",}'
                        ).encode("utf-8"),
                        r"json",
                    ),
                    (
                        "non-object-json",
                        json.dumps([sensitive_marker]).encode("utf-8"),
                        r"object|mapping",
                    ),
                )
                manifest_readers = (
                    ("configured-roots", configured_skill_roots),
                    ("configured-mode", configured_skill_validation_mode),
                    ("build-service", build_skill_service),
                )
                for case_name, payload, category_pattern in invalid_cases:
                    manifest_path.write_bytes(payload)
                    for reader_name, reader in manifest_readers:
                        with self.subTest(case=case_name, reader=reader_name):
                            with self.assertRaises(SkillError) as error_context:
                                reader(plugin_dir)
                            message = str(error_context.exception)
                            self.assertIn(str(manifest_path), message)
                            self.assertRegex(message.lower(), category_pattern)
                            self.assertNotIn(sensitive_marker, message)

    def test_manifest_runtime_failure_keeps_lkg_and_repair_recovers(self):
        from qcopilots_common.skills import build_skill_service

        with tempfile.TemporaryDirectory() as tmp:
            plugin_dir = Path(tmp) / "plugin"
            plugin_dir.mkdir()
            (plugin_dir / "Skills").mkdir()
            root_a = plugin_dir / "SkillsA"
            root_b = plugin_dir / "SkillsB"
            self._write_skill(
                root_a,
                name="manifest-alpha",
                body="# Manifest alpha LKG\n",
            )
            self._write_skill(
                root_b,
                name="manifest-bravo",
                body="# Manifest bravo repaired\n",
            )
            manifest_path = plugin_dir / "qcopilots_service.json"
            manifest_path.write_text(
                json.dumps({"skillsRoot": "SkillsA"}),
                encoding="utf-8",
            )

            with mock.patch.dict(os.environ, {}, clear=True):
                service = build_skill_service(plugin_dir)
                service.refresh_interval_seconds = 0
                initial_tools = {tool.name: tool for tool in service.tools()}
                self.assertIn("manifest-alpha", initial_tools)
                self.assertNotIn("manifest-bravo", initial_tools)

                sensitive_marker = "BROKEN-MANIFEST-CONTENT-MUST-NOT-LEAK"
                manifest_path.write_text(
                    '{"skillsRoot": "' + sensitive_marker + '",}',
                    encoding="utf-8",
                )
                lkg_tools = {tool.name: tool for tool in service.tools()}
                lkg_listing = lkg_tools["list_skills"].handler({})
                self.assertEqual(
                    [entry["slug"] for entry in lkg_listing["skills"]],
                    ["manifest-alpha"],
                )
                self.assertEqual(
                    lkg_tools["manifest-alpha"].handler(
                        {"task": "Use the last-known-good manifest root"}
                    )["instructions"],
                    "# Manifest alpha LKG\n",
                )
                reload_failures = [
                    diagnostic
                    for diagnostic in lkg_listing["diagnostics"]
                    if diagnostic["code"] == "skills.reload.failed"
                ]
                self.assertEqual(len(reload_failures), 1)
                self.assertIn(str(manifest_path), reload_failures[0]["message"])
                self.assertNotIn(
                    sensitive_marker,
                    json.dumps(reload_failures, ensure_ascii=False),
                )

                manifest_path.write_text(
                    json.dumps({"skillsRoot": "SkillsB"}),
                    encoding="utf-8",
                )
                repaired_tools = {tool.name: tool for tool in service.tools()}
                self.assertNotIn("manifest-alpha", repaired_tools)
                self.assertIn("manifest-bravo", repaired_tools)
                repaired_listing = repaired_tools["list_skills"].handler({})
                self.assertEqual(
                    [entry["slug"] for entry in repaired_listing["skills"]],
                    ["manifest-bravo"],
                )
                self.assertNotIn(
                    "skills.reload.failed",
                    {
                        diagnostic["code"]
                        for diagnostic in repaired_listing["diagnostics"]
                    },
                )
                self.assertEqual(
                    repaired_tools["manifest-bravo"].handler(
                        {"task": "Use the repaired manifest root"}
                    )["instructions"],
                    "# Manifest bravo repaired\n",
                )

    def test_manifest_schema_errors_cover_all_entrypoints_and_runtime_lkg(self):
        from qcopilots_common.skills import (
            SkillError,
            build_skill_service,
            configured_skill_roots,
            configured_skill_validation_mode,
        )

        sensitive_marker = "PRIVATE-MANIFEST-VALUE-MUST-NOT-LEAK"
        invalid_cases = (
            (
                "skill-root-not-string",
                {"skillsRoot": [sensitive_marker]},
                r"skillsroot.*string|string.*skillsroot",
            ),
            (
                "skill-root-empty",
                {"skillsRoot": ""},
                r"skillsroot.*non-empty|non-empty.*skillsroot",
            ),
            (
                "legacy-top-level-roots-unsupported",
                {"skills_roots": [sensitive_marker]},
                r"skills_roots.*skillsroot|skillsroot.*skills_roots",
            ),
            (
                "nested-roots-unsupported",
                {"skills": {"roots": sensitive_marker}},
                r"skills\.roots.*skillsroot|skillsroot.*skills\.roots",
            ),
            (
                "skills-not-object",
                {"skills": sensitive_marker},
                r"skills.*object|object.*skills",
            ),
            (
                "skills-unknown-key",
                {"skills": {"unexpected": sensitive_marker}},
                r"skills.*(?:unexpected|unknown|additional)|"
                r"(?:unexpected|unknown|additional).*skills",
            ),
            (
                "validation-mode-invalid",
                {"skills": {"validation_mode": sensitive_marker}},
                r"validation_mode.*(?:strict|compatible|enum)|"
                r"(?:strict|compatible|enum).*validation_mode",
            ),
            (
                "legacy-validation-mode-invalid",
                {"skills_validation_mode": sensitive_marker},
                r"skills_validation_mode.*(?:strict|compatible|enum)|"
                r"(?:strict|compatible|enum).*skills_validation_mode",
            ),
        )
        readers = (
            ("configured-roots", configured_skill_roots),
            ("configured-mode", configured_skill_validation_mode),
            ("build-service", build_skill_service),
        )

        for case_name, invalid_manifest, category_pattern in invalid_cases:
            with (
                self.subTest(phase="initial", case=case_name),
                tempfile.TemporaryDirectory() as tmp,
            ):
                plugin_dir = Path(tmp) / "plugin"
                self._write_skill(plugin_dir / "Skills", name="manifest-default")
                manifest_path = plugin_dir / "qcopilots_service.json"
                manifest_path.write_text(
                    json.dumps(invalid_manifest),
                    encoding="utf-8",
                )
                with mock.patch.dict(os.environ, {}, clear=True):
                    for reader_name, reader in readers:
                        with self.subTest(reader=reader_name):
                            with self.assertRaises(SkillError) as error_context:
                                reader(plugin_dir)
                            message = str(error_context.exception)
                            self.assertIn(str(manifest_path), message)
                            self.assertRegex(message.lower(), category_pattern)
                            self.assertNotIn(sensitive_marker, message)

            with (
                self.subTest(phase="runtime", case=case_name),
                tempfile.TemporaryDirectory() as tmp,
            ):
                plugin_dir = Path(tmp) / "plugin"
                plugin_dir.mkdir()
                (plugin_dir / "Skills").mkdir()
                root_a = plugin_dir / "SkillsA"
                root_b = plugin_dir / "SkillsB"
                self._write_skill(
                    root_a,
                    name="manifest-alpha",
                    body="# Schema alpha LKG\n",
                )
                self._write_skill(
                    root_b,
                    name="manifest-bravo",
                    body="# Schema bravo repaired\n",
                )
                manifest_path = plugin_dir / "qcopilots_service.json"
                manifest_path.write_text(
                    json.dumps({"skillsRoot": "SkillsA"}),
                    encoding="utf-8",
                )
                with mock.patch.dict(os.environ, {}, clear=True):
                    service = build_skill_service(plugin_dir)
                    service.refresh_interval_seconds = 0
                    manifest_path.write_text(
                        json.dumps(invalid_manifest),
                        encoding="utf-8",
                    )
                    lkg_tools = {tool.name: tool for tool in service.tools()}
                    self.assertEqual(
                        lkg_tools["manifest-alpha"].handler(
                            {"task": "Use schema-invalid LKG"}
                        )["instructions"],
                        "# Schema alpha LKG\n",
                    )
                    listing = lkg_tools["list_skills"].handler({})
                    self.assertEqual(
                        [entry["slug"] for entry in listing["skills"]],
                        ["manifest-alpha"],
                    )
                    reload_failures = [
                        diagnostic
                        for diagnostic in listing["diagnostics"]
                        if diagnostic["code"] == "skills.reload.failed"
                    ]
                    self.assertEqual(len(reload_failures), 1)
                    self.assertIn(str(manifest_path), reload_failures[0]["message"])
                    self.assertRegex(
                        reload_failures[0]["message"].lower(),
                        category_pattern,
                    )
                    self.assertNotIn(
                        sensitive_marker,
                        json.dumps(reload_failures, ensure_ascii=False),
                    )

                    manifest_path.write_text(
                        json.dumps({"skillsRoot": "SkillsB"}),
                        encoding="utf-8",
                    )
                    repaired_tools = {tool.name: tool for tool in service.tools()}
                    self.assertNotIn("manifest-alpha", repaired_tools)
                    self.assertEqual(
                        repaired_tools["manifest-bravo"].handler(
                            {"task": "Use repaired schema manifest"}
                        )["instructions"],
                        "# Schema bravo repaired\n",
                    )
                    self.assertNotIn(
                        "skills.reload.failed",
                        {
                            diagnostic["code"]
                            for diagnostic in repaired_tools["list_skills"].handler(
                                {}
                            )["diagnostics"]
                        },
                    )

    def test_skill_content_xml_round_trips_adversarial_markdown_exactly(self):
        import xml.etree.ElementTree as ElementTree

        from qcopilots_common.skills import SkillService

        body = (
            "# 结构化正文\n\n"
            "伪闭合 </skill_instructions> 不得结束正文。\n"
            "CDATA 终止符 ]]> 也必须保持原样。\n"
            "伪资源节点 <skill_resources> 不得成为直接子节点。\n\n"
            "```xml\n"
            "<layer name=\"道路 & 河流\">值</layer>\n"
            "```\n"
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(
                root,
                name="xml-roundtrip",
                description="Round-trip adversarial markdown",
                body=body,
            )
            service = SkillService([root], refresh_interval_seconds=0)
            tools = {tool.name: tool for tool in service.tools()}
            activation = tools["xml-roundtrip"].handler(
                {"task": "解析 <正文> & 保持结构"}
            )
            read_result = tools["read_skill"].handler({"slug": "xml-roundtrip"})

            self.assertEqual(activation["instructions"], body)
            self.assertEqual(read_result["content"], body)
            for source_name, skill_content in (
                ("activation", activation["skill_content"]),
                ("read-skill", read_result["skill_content"]),
            ):
                with self.subTest(source=source_name):
                    root_element = ElementTree.fromstring(skill_content)
                    self.assertEqual(root_element.tag, "skill_content")
                    self.assertEqual(root_element.attrib["name"], "xml-roundtrip")
                    self.assertEqual(root_element.attrib["slug"], "xml-roundtrip")
                    self.assertEqual(
                        [child.tag for child in root_element],
                        [
                            "activation_task",
                            "skill_metadata",
                            "skill_instructions",
                            "skill_resources",
                        ],
                    )
                    instructions_element = root_element.find(
                        "./skill_instructions"
                    )
                    self.assertIsNotNone(instructions_element)
                    self.assertEqual(list(instructions_element), [])
                    self.assertEqual(
                        "".join(instructions_element.itertext()),
                        body,
                    )

    def test_explicit_builtin_exec_runs_directory_skill_script_only_on_request(self):
        import sys

        from qcopilots_common.builtin_tools import build_builtin_tools
        from qcopilots_common.skills import SkillService

        base = None
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            root = base / "Skills"
            script_content = (
                "import sys\n"
                "from pathlib import Path\n"
                "Path('execution-sentinel.txt').write_text(\n"
                "    'created by explicit execution\\n', encoding='utf-8'\n"
                ")\n"
                "print(f'cwd={Path.cwd()}')\n"
                "print(f'arg={sys.argv[1]}')\n"
            )
            skill_dir = self._write_skill(
                root,
                name="explicit-script",
                description="Run a script only after an explicit authorized request",
                body="# Explicit script\n",
                allowed_tools="exec_shell_command",
                resources={"scripts/verify.py": script_content},
            )
            script_path = skill_dir / "scripts" / "verify.py"
            sentinel = skill_dir / "execution-sentinel.txt"
            service = SkillService([root], refresh_interval_seconds=0)
            tools = {tool.name: tool for tool in service.tools()}
            self.assertFalse(sentinel.exists(), "Discovery must not execute scripts")

            activation = tools["explicit-script"].handler(
                {"task": "Explicitly run the harmless verification script"}
            )
            self.assertEqual(activation["source_kind"], "directory")
            self.assertEqual(activation["skill_root"], str(skill_dir.resolve()))
            self.assertEqual(
                activation["metadata"]["allowed_tools"],
                ["exec_shell_command"],
            )
            self.assertEqual(
                activation["metadata"]["allowed_tools_declaration"],
                "exec_shell_command",
            )
            self.assertIn("user or host", activation["usage"].lower())
            self.assertIn("permission", activation["usage"].lower())
            self.assertFalse(sentinel.exists(), "Activation must not execute scripts")

            read_result = tools["read_skill"].handler({"slug": "explicit-script"})
            self.assertEqual(read_result["content"], "# Explicit script\n")
            script_resource = tools["read_skill_resource"].handler(
                {
                    "slug": "explicit-script",
                    "resource": "scripts/verify.py",
                }
            )
            self.assertEqual(script_resource["content"], script_content)
            self.assertFalse(
                sentinel.exists(),
                "Reading skill content or resources must not execute scripts",
            )

            builtin_tools = {
                tool.name: tool
                for tool in build_builtin_tools(activation["skill_root"])
            }
            self.assertIn("exec_shell_command", builtin_tools)
            command_result = builtin_tools["exec_shell_command"].handler(
                {
                    "command": [
                        sys.executable,
                        str(script_path.resolve()),
                        "explicit-argument",
                    ],
                    "cwd": activation["skill_root"],
                    "timeout_seconds": 5,
                }
            )
            self.assertEqual(
                set(command_result),
                {"exit_code", "timed_out", "stdout", "stderr"},
            )
            self.assertEqual(command_result["exit_code"], 0)
            self.assertIs(command_result["timed_out"], False)
            self.assertEqual(command_result["stderr"], "")
            stdout_lines = command_result["stdout"].splitlines()
            self.assertEqual(len(stdout_lines), 2)
            self.assertTrue(stdout_lines[0].startswith("cwd="))
            self.assertEqual(
                Path(stdout_lines[0].split("=", 1)[1]).resolve(),
                Path(activation["skill_root"]).resolve(),
            )
            self.assertEqual(stdout_lines[1], "arg=explicit-argument")
            self.assertEqual(
                sentinel.read_text(encoding="utf-8"),
                "created by explicit execution\n",
            )

        self.assertIsNotNone(base)
        self.assertFalse(
            base.exists(),
            "TemporaryDirectory did not clean test artifacts",
        )

    def test_tool_descriptions_define_refresh_workflow_and_script_permissions(self):
        from qcopilots_common.skills import SkillService

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "Skills"
            self._write_skill(
                root,
                name="description-contract",
                description="Neutral description for the tool contract",
                allowed_tools="exec_shell_command",
                resources={"scripts/verify.py": "print('resource only')\n"},
            )
            service = SkillService([root], refresh_interval_seconds=0)
            tools = {tool.name: tool for tool in service.tools()}
            self.assertTrue(
                {
                    "list_skills",
                    "read_skill",
                    "description-contract",
                }.issubset(tools)
            )

            list_description = tools["list_skills"].description.lower()
            read_description = tools["read_skill"].description.lower()
            dynamic_description = tools["description-contract"].description.lower()
            self.assertIn("read_skill", list_description)
            self.assertIn("refresh", list_description)
            self.assertIn("current", list_description)
            self.assertIn("list_skills", read_description)
            self.assertIn("script", read_description)
            self.assertIn("skill_root", read_description)
            self.assertRegex(read_description, r"allowed[-_]tools")
            self.assertRegex(read_description, r"\buser\b.*\bhost\b|\bhost\b.*\buser\b")
            self.assertIn("explicit", read_description)
            self.assertRegex(read_description, r"approv|authoriz")
            self.assertIn("permission", read_description)
            self.assertRegex(
                read_description,
                r"does not automatically|not automatically|never automatically",
            )

            self.assertIn("[skill]", dynamic_description)
            self.assertIn("specialized instructions", dynamic_description)
            self.assertIn("resources", dynamic_description)
            self.assertRegex(dynamic_description, r"activate|invoke")

    def test_source_qgis_skills_creator_is_strict_complete_and_permission_safe(self):
        import copy
        import re
        import runpy
        import subprocess
        import sys
        from urllib.parse import unquote, urlsplit

        from qcopilots_common.constants import MAX_FILE_READ_BYTES
        from qcopilots_common.skills import SkillError, SkillRegistry, SkillService

        repo_root = Path(__file__).resolve().parents[3]
        skills_root = (
            repo_root / "python" / "plugins" / "qcopilots_mcp_server_skills" / "Skills"
        )
        skill_slug = "qgis-skills-creator"
        missing_sibling = "qgis-skills-creator-missing"
        skill_root = skills_root / skill_slug
        required_resources = {
            "agents/openai.yaml",
            "scripts/plan_gate.py",
            "references/qgis-skill-creation-workflow.md",
            "references/geospatial-domain-checklists.md",
            "references/qgis-tools.catalog.json",
            "references/qgis-tools.catalog.schema.json",
            "references/qgis-skill-plan.schema.json",
            "assets/qgis-skill-plan.template.json",
            "assets/generated-skill-SKILL.template.md",
            "assets/generated-skill-openai.template.yaml",
        }
        activation_task = "Design a reviewed QGIS workflow skill"

        self.assertTrue(skill_root.is_dir(), skill_root)

        def package_snapshot():
            return {
                path.relative_to(skill_root).as_posix(): hashlib.sha256(
                    path.read_bytes()
                ).hexdigest()
                for path in skill_root.rglob("*")
                if path.is_file()
            }

        before_reads = package_snapshot()
        execution_guard = ExitStack()
        self.addCleanup(execution_guard.close)
        blocked_run = execution_guard.enter_context(
            mock.patch.object(subprocess, "run")
        )
        blocked_popen = execution_guard.enter_context(
            mock.patch.object(subprocess, "Popen")
        )
        blocked_system = execution_guard.enter_context(mock.patch.object(os, "system"))
        registry = SkillRegistry(skills_root, validation_mode="strict")
        registry.load()
        slugs = {skill.slug for skill in registry.skills}
        self.assertIn(skill_slug, slugs)
        self.assertNotIn(missing_sibling, slugs)
        with self.assertRaises(SkillError):
            registry.get(missing_sibling)

        skill = registry.get(skill_slug)
        self.assertEqual(skill.name, skill_slug)
        self.assertEqual(skill.slug, skill_slug)
        self.assertEqual(skill.conformance, "strict")
        self.assertEqual(skill.source_kind, "directory")
        self.assertEqual(skill.directory, skill_root.resolve())
        target_source = str(skill_root.resolve()).casefold()
        self.assertEqual(
            [
                diagnostic.as_dict()
                for diagnostic in registry.diagnostics
                if target_source in diagnostic.source.casefold()
            ],
            [],
        )
        self.assertEqual(skill.metadata.allowed_tools, ())
        self.assertIsNone(skill.metadata.allowed_tools_declaration)

        skill_document = (skill_root / "SKILL.md").read_text(encoding="utf-8")
        self.assertTrue(skill_document.startswith("---\n"))
        front_matter = skill_document.split("\n---", 1)[0].removeprefix("---\n")
        front_matter_keys = {
            line.split(":", 1)[0].strip().casefold()
            for line in front_matter.splitlines()
            if ":" in line and not line.startswith((" ", "\t"))
        }
        source_skill_body = skill_document.split("\n---\n", 1)[1]
        self.assertEqual(skill.read_body(), source_skill_body)
        self.assertNotIn("allowed-tools", front_matter_keys)
        self.assertNotIn("allowed_tools", front_matter_keys)

        resource_paths = tuple(skill.iter_resource_paths())
        self.assertSetEqual(set(resource_paths), required_resources)
        resource_contents = {}
        for resource_path in resource_paths:
            with self.subTest(resource=resource_path):
                parts = resource_path.split("/")
                self.assertFalse(resource_path.startswith("/"))
                self.assertNotIn("\\", resource_path)
                self.assertFalse(parts[0].endswith(":"))
                self.assertNotIn("", parts)
                self.assertNotIn(".", parts)
                self.assertNotIn("..", parts)
                package_file = skill_root.joinpath(*parts)
                self.assertTrue(package_file.is_file(), package_file)
                self.assertLessEqual(
                    package_file.stat().st_size,
                    MAX_FILE_READ_BYTES,
                )
                resource_contents[resource_path] = registry.read_resource(
                    skill_slug,
                    resource_path,
                )
                self.assertEqual(
                    resource_contents[resource_path],
                    package_file.read_text(encoding="utf-8"),
                )

        for package_file in skill_root.rglob("*"):
            if package_file.is_file():
                with self.subTest(package_file=package_file):
                    self.assertLessEqual(
                        package_file.stat().st_size,
                        MAX_FILE_READ_BYTES,
                    )

        for json_resource in (
            "references/qgis-tools.catalog.json",
            "references/qgis-tools.catalog.schema.json",
            "references/qgis-skill-plan.schema.json",
            "assets/qgis-skill-plan.template.json",
        ):
            with self.subTest(json_resource=json_resource):
                self.assertIsInstance(
                    json.loads(resource_contents[json_resource]),
                    (dict, list),
                )

        catalog_data = json.loads(
            resource_contents["references/qgis-tools.catalog.json"]
        )
        catalog_schema = json.loads(
            resource_contents["references/qgis-tools.catalog.schema.json"]
        )
        plan_schema = json.loads(
            resource_contents["references/qgis-skill-plan.schema.json"]
        )
        plan_template_data = json.loads(
            resource_contents["assets/qgis-skill-plan.template.json"]
        )
        self.assertEqual(plan_template_data["plan_schema_version"], "3.1.0")
        self.assertEqual(
            plan_schema["properties"]["plan_schema_version"]["const"],
            "3.1.0",
        )
        geospatial_schema = plan_schema["properties"]["geospatial_semantics"]
        domain_policy_fields = {
            "domains",
            "crs_policy",
            "vertical_crs_policy",
            "units_policy",
            "temporal_policy",
            "geometry_policy",
            "raster_policy",
            "point_cloud_policy",
            "mesh_policy",
            "three_d_policy",
            "service_policy",
            "tabular_policy",
            "quality_policy",
        }
        self.assertTrue(domain_policy_fields.issubset(geospatial_schema["required"]))
        self.assertSetEqual(
            set(
                geospatial_schema["properties"]["domains"]["items"]["enum"]
            ),
            {
                "vector",
                "raster",
                "point-cloud",
                "mesh",
                "three-dimensional",
                "geospatial-service",
                "spatiotemporal",
                "tabular",
                "pending-domain-selection",
            },
        )

        package_members = required_resources | {"SKILL.md"}

        def resolve_local_package_reference(document_path, raw_target):
            target = raw_target.strip()
            if target.startswith("#"):
                return None
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc:
                self.assertIn(
                    parsed.scheme.casefold(),
                    {"http", "https", "mailto"},
                )
                return None
            relative_target = unquote(parsed.path)
            if not relative_target:
                return None
            self.assertFalse(relative_target.startswith(("/", "\\")))
            self.assertNotIn("\\", relative_target)
            self.assertIsNone(re.match(r"^[A-Za-z]:", relative_target))
            source_path = skill_root / document_path
            resolved_target = (source_path.parent / relative_target).resolve()
            try:
                package_relative = resolved_target.relative_to(
                    skill_root.resolve()
                ).as_posix()
            except ValueError:
                self.fail(
                    f"Reference escapes skill root: {document_path} -> {raw_target}"
                )
            self.assertTrue(resolved_target.is_file(), resolved_target)
            self.assertIn(package_relative, package_members)
            return package_relative

        markdown_link_pattern = re.compile(
            r"!?\[[^\]]*\]\(\s*(?:<([^>]+)>|([^\s)]+))"
            r"(?:\s+[^)]*)?\)"
        )
        markdown_reference_pattern = re.compile(
            r"(?m)^[ \t]{0,3}\[[^\]]+\]:\s*"
            r"(?:<([^>]+)>|([^\s]+))"
        )
        markdown_documents = {
            "SKILL.md": skill_document,
            **{
                path: content
                for path, content in resource_contents.items()
                if path.endswith(".md")
            },
        }
        local_markdown_references = []
        for document_path, markdown in markdown_documents.items():
            matches = [
                *markdown_link_pattern.finditer(markdown),
                *markdown_reference_pattern.finditer(markdown),
            ]
            for match in matches:
                raw_target = match.group(1) or match.group(2) or ""
                resolved_reference = resolve_local_package_reference(
                    document_path,
                    raw_target,
                )
                if resolved_reference is not None:
                    local_markdown_references.append(
                        (document_path, resolved_reference)
                    )
        self.assertTrue(local_markdown_references)

        def iter_json_references(value):
            if isinstance(value, dict):
                for key, child in value.items():
                    if key in {"$schema", "$ref"} and isinstance(child, str):
                        yield key, child
                    yield from iter_json_references(child)
            elif isinstance(value, list):
                for child in value:
                    yield from iter_json_references(child)

        def resolve_json_pointer(document, pointer, reference_context):
            if pointer == "":
                return document
            self.assertTrue(
                pointer.startswith("/"),
                f"Invalid JSON Pointer in {reference_context}: {pointer!r}",
            )
            current = document
            for raw_token in pointer[1:].split("/"):
                self.assertIsNone(
                    re.search(r"~(?:[^01]|$)", raw_token),
                    f"Invalid JSON Pointer escape in {reference_context}",
                )
                token = raw_token.replace("~1", "/").replace("~0", "~")
                if isinstance(current, dict):
                    self.assertIn(
                        token,
                        current,
                        f"Missing JSON Pointer token in {reference_context}",
                    )
                    current = current[token]
                elif isinstance(current, list):
                    self.assertRegex(token, r"^(?:0|[1-9][0-9]*)$")
                    index = int(token)
                    self.assertLess(index, len(current), reference_context)
                    current = current[index]
                else:
                    self.fail(f"JSON Pointer traverses a scalar in {reference_context}")
            return current

        self.assertEqual(
            resolve_json_pointer(
                {"a/b": {"m~n": ["resolved"]}},
                "/a~1b/m~0n/0",
                "synthetic escaped JSON Pointer",
            ),
            "resolved",
        )

        json_documents = {
            "references/qgis-tools.catalog.json": catalog_data,
            "references/qgis-tools.catalog.schema.json": catalog_schema,
            "references/qgis-skill-plan.schema.json": plan_schema,
            "assets/qgis-skill-plan.template.json": plan_template_data,
        }
        local_schema_references = []
        local_json_pointer_references = []
        for document_path, document in json_documents.items():
            for reference_kind, raw_reference in iter_json_references(document):
                parsed_reference = urlsplit(raw_reference)
                if parsed_reference.scheme or parsed_reference.netloc:
                    self.assertIn(
                        parsed_reference.scheme.casefold(),
                        {"http", "https"},
                    )
                    continue
                if parsed_reference.path:
                    resolved_reference = resolve_local_package_reference(
                        document_path,
                        raw_reference,
                    )
                    self.assertIsNotNone(resolved_reference)
                    target_document = json_documents[resolved_reference]
                    if reference_kind == "$schema":
                        local_schema_references.append(
                            (document_path, resolved_reference)
                        )
                else:
                    resolved_reference = document_path
                    target_document = document
                fragment = unquote(parsed_reference.fragment)
                if reference_kind == "$ref":
                    resolved_value = resolve_json_pointer(
                        target_document,
                        fragment,
                        f"{document_path} -> {raw_reference}",
                    )
                    local_json_pointer_references.append(
                        (
                            document_path,
                            resolved_reference,
                            fragment,
                            resolved_value,
                        )
                    )
        self.assertTrue(local_json_pointer_references)
        self.assertSetEqual(
            set(local_schema_references),
            {
                (
                    "references/qgis-tools.catalog.json",
                    "references/qgis-tools.catalog.schema.json",
                ),
                (
                    "assets/qgis-skill-plan.template.json",
                    "references/qgis-skill-plan.schema.json",
                ),
            },
        )
        plan_schema_reference = plan_template_data["$schema"]
        self.assertEqual(
            plan_schema_reference,
            "../references/qgis-skill-plan.schema.json",
        )
        referenced_plan_schema = (
            skill_root / "assets" / plan_schema_reference
        ).resolve()
        self.assertEqual(
            referenced_plan_schema,
            (skill_root / "references" / "qgis-skill-plan.schema.json").resolve(),
        )
        self.assertEqual(
            referenced_plan_schema.relative_to(skill_root.resolve()).as_posix(),
            "references/qgis-skill-plan.schema.json",
        )
        self.assertTrue(referenced_plan_schema.is_file())
        host_artifact_fields = {
            "artifact_digest",
            "artifact_issuer",
            "artifact_nonce",
        }

        def nested_keys(value):
            keys = set()
            if isinstance(value, dict):
                keys.update(value)
                for child in value.values():
                    keys.update(nested_keys(child))
            elif isinstance(value, list):
                for child in value:
                    keys.update(nested_keys(child))
            return keys

        self.assertTrue(host_artifact_fields.isdisjoint(nested_keys(plan_schema)))
        self.assertTrue(
            host_artifact_fields.isdisjoint(nested_keys(plan_template_data))
        )
        approval_schema = plan_schema["properties"]["approval"]
        self.assertTrue(host_artifact_fields.isdisjoint(approval_schema["required"]))
        self.assertTrue(host_artifact_fields.isdisjoint(approval_schema["properties"]))
        self.assertTrue(host_artifact_fields.isdisjoint(plan_template_data["approval"]))
        approved_conditionals = [
            conditional["then"]["properties"]["approval"]
            for conditional in plan_schema.get("allOf", [])
            if "approval" in conditional.get("then", {}).get("properties", {})
        ]
        self.assertTrue(approved_conditionals)
        for conditional_approval in approved_conditionals:
            self.assertTrue(
                host_artifact_fields.isdisjoint(
                    conditional_approval.get("required", [])
                )
            )
            self.assertTrue(
                host_artifact_fields.isdisjoint(
                    conditional_approval.get("properties", {})
                )
            )

        service = SkillService(
            [skills_root],
            validation_mode="strict",
            refresh_interval_seconds=3600,
        )
        tools = {tool.name: tool for tool in service.tools()}
        listing = tools["list_skills"].handler({})
        summaries = {entry["slug"]: entry for entry in listing["skills"]}
        self.assertEqual(listing["validation_mode"], "strict")
        self.assertIn(skill_slug, summaries)
        self.assertNotIn(missing_sibling, summaries)
        self.assertEqual(summaries[skill_slug]["conformance"], "strict")
        self.assertFalse(
            any(
                diagnostic.get("code") == "skills.reload.failed"
                for diagnostic in listing["diagnostics"]
            )
        )
        self.assertFalse(
            any(
                diagnostic.get("severity") == "error"
                and target_source in str(diagnostic.get("source", "")).casefold()
                for diagnostic in listing["diagnostics"]
            )
        )

        read_result = tools["read_skill"].handler({"slug": skill_slug})
        activation = tools[skill_slug].handler({"task": activation_task})
        listed_resources = tools["list_skill_resources"].handler({"slug": skill_slug})
        service_resource_reads = {
            resource_path: tools["read_skill_resource"].handler(
                {
                    "slug": skill_slug,
                    "resource": resource_path,
                }
            )
            for resource_path in resource_paths
        }
        self.assertEqual(read_result["conformance"], "strict")
        self.assertEqual(read_result["skill_root"], str(skill_root.resolve()))
        self.assertEqual(read_result["content"], source_skill_body)
        self.assertEqual(activation["conformance"], "strict")
        self.assertEqual(activation["skill_root"], str(skill_root.resolve()))
        self.assertEqual(activation["instructions"], source_skill_body)
        self.assertEqual(activation["metadata"]["allowed_tools"], [])
        self.assertIsNone(activation["metadata"]["allowed_tools_declaration"])
        expected_instruction_block = (
            "<skill_instructions><![CDATA["
            f"{source_skill_body.replace(']]>', ']]]]><![CDATA[>')}"
            "]]></skill_instructions>"
        )
        self.assertEqual(
            read_result["skill_content"].count(expected_instruction_block),
            1,
        )
        self.assertEqual(
            activation["skill_content"].count(expected_instruction_block),
            1,
        )
        self.assertIn(
            f"<activation_task>\n{activation_task}\n</activation_task>",
            activation["skill_content"],
        )
        self.assertSetEqual(
            {resource["path"] for resource in activation["resources"]},
            required_resources,
        )
        self.assertSetEqual(
            set(listed_resources["resources"]),
            required_resources,
        )
        for resource_path, service_read in service_resource_reads.items():
            with self.subTest(service_resource=resource_path):
                self.assertEqual(service_read["encoding"], "utf-8")
                self.assertEqual(
                    service_read["content"],
                    resource_contents[resource_path],
                )
        self.assertEqual(
            package_snapshot(),
            before_reads,
            "Discovering and reading the source skill must not execute plan_gate.py",
        )
        execution_guard.close()
        blocked_run.assert_not_called()
        blocked_popen.assert_not_called()
        blocked_system.assert_not_called()

        with tempfile.TemporaryDirectory() as sentinel_tmp:
            sentinel_root = Path(sentinel_tmp)
            sentinel_skills_root = sentinel_root / "Skills"
            sentinel_skill_root = sentinel_skills_root / "sentinel-skill"
            sentinel_script_path = sentinel_skill_root / "scripts" / "sentinel.py"
            sentinel_file = sentinel_root / "script-executed.txt"
            sentinel_script_path.parent.mkdir(parents=True)
            (sentinel_skill_root / "SKILL.md").write_text(
                "---\n"
                "name: sentinel-skill\n"
                'description: "Detect automatic resource execution"\n'
                "---\n\n"
                "# Sentinel Skill\n\n"
                "Read the script resource without executing it.\n",
                encoding="utf-8",
                newline="\n",
            )
            sentinel_script = (
                "from pathlib import Path\n"
                "import os\n\n"
                'Path(os.environ["QCOPILOTS_SKILL_SENTINEL"]).write_text(\n'
                '    "executed", encoding="utf-8"\n'
                ")\n"
            )
            sentinel_script_path.write_text(
                sentinel_script,
                encoding="utf-8",
                newline="\n",
            )

            def assert_sentinel_absent(stage):
                self.assertFalse(
                    sentinel_file.exists(),
                    f"Skill script executed during {stage}",
                )

            with mock.patch.dict(
                os.environ,
                {"QCOPILOTS_SKILL_SENTINEL": str(sentinel_file)},
            ):
                sentinel_registry = SkillRegistry(
                    sentinel_skills_root,
                    validation_mode="strict",
                )
                sentinel_registry.load()
                assert_sentinel_absent("registry load")
                sentinel_service = SkillService(
                    [sentinel_skills_root],
                    validation_mode="strict",
                    refresh_interval_seconds=3600,
                )
                sentinel_tools = {tool.name: tool for tool in sentinel_service.tools()}
                sentinel_tools["list_skills"].handler({})
                assert_sentinel_absent("service list")
                sentinel_tools["read_skill"].handler({"slug": "sentinel-skill"})
                assert_sentinel_absent("service read")
                sentinel_tools["sentinel-skill"].handler(
                    {"task": "Read the sentinel resource"}
                )
                assert_sentinel_absent("activation")
                sentinel_tools["list_skill_resources"].handler(
                    {"slug": "sentinel-skill"}
                )
                assert_sentinel_absent("resource list")
                sentinel_resource = sentinel_tools["read_skill_resource"].handler(
                    {
                        "slug": "sentinel-skill",
                        "resource": "scripts/sentinel.py",
                    }
                )
                self.assertEqual(sentinel_resource["content"], sentinel_script)
                assert_sentinel_absent("resource read")

        plan_gate_path = skill_root / "scripts" / "plan_gate.py"
        plan_gate_helpers = runpy.run_path(
            str(plan_gate_path),
            run_name="qgis_skills_creator_plan_gate_test",
        )
        validate_schema_subset = plan_gate_helpers["validate_schema_subset"]
        validate_live_capability_contract = plan_gate_helpers[
            "validate_live_capability_contract"
        ]
        validate_common = plan_gate_helpers["validate_common"]
        validate_node_invocation = plan_gate_helpers["validate_node_invocation"]
        validate_skills_root_binding = plan_gate_helpers["validate_skills_root_binding"]
        validate_openai_yaml = plan_gate_helpers["validate_openai_yaml"]
        build_processing_invocation_fixture = plan_gate_helpers[
            "_build_processing_invocation_fixture"
        ]
        openai_errors = []
        validate_openai_yaml(
            resource_contents["agents/openai.yaml"],
            skill_slug,
            openai_errors,
        )
        self.assertEqual(openai_errors, [])
        self.assertFalse(plan_gate_helpers["is_relative_package_path"]("C:/x"))
        contains_obvious_machine_path = plan_gate_helpers[
            "contains_obvious_machine_path"
        ]
        machine_path_pattern = plan_gate_helpers["OBVIOUS_MACHINE_PATH_RE"]
        independent_machine_path_pattern = re.compile(
            r"(?<![A-Za-z0-9:/])(?:"
            r"[A-Za-z]:[\\/]|"
            r"(?:\\\\\?\\UNC\\|\\\\wsl\$\\|\\\\|//)"
            r"[\w.$-]+[\\/][\w.$-]+|"
            r"/(?:mnt|src|workspace|opt|usr|var|srv|home|Users|data)"
            r"(?:/|$))"
        )
        independent_remote_file_uri_pattern = re.compile(
            r"(?i)(?<![A-Za-z0-9])file://[\w.$-]+/[\w.$-]+"
        )
        independent_escaped_unc_pattern = re.compile(
            r"(?<!\\)\\{4}[\w.$-]+\\{2}[\w.$-]+"
        )

        def independently_contains_machine_path(value):
            return bool(
                independent_machine_path_pattern.search(value)
                or independent_remote_file_uri_pattern.search(value)
                or independent_escaped_unc_pattern.search(value)
            )

        machine_path_samples = (
            r"C:\Program Files\QGIS\bin",
            r"D:\custom\extensionless",
            "Z:/arbitrary/location",
            r"\\server\share\dataset",
            "//server/share/dataset",
            r"\\?\UNC\server\share\dataset",
            r"\\wsl$\Ubuntu\home\qgis",
            r"\\服务器\共享\数据",
            "file://server/share/dataset",
            "/mnt/volume/input",
            "/src/project/input",
            "/workspace/output",
            "/opt/qgis/bin",
            "/usr/local/bin/tool",
            "/var/lib/qgis",
            "/srv/data/item",
            "/home/qgis/input",
            "/Users/qgis/input",
            "/data/qgis/input",
        )
        for machine_path in machine_path_samples:
            with self.subTest(machine_path=machine_path):
                self.assertTrue(independently_contains_machine_path(machine_path))
                self.assertTrue(contains_obvious_machine_path(machine_path))
        for portable_path in (
            "references/tool-contract.json",
            "bin/qgis_process-qgis-qt6.bat",
            "https://example.invalid/qgis",
        ):
            with self.subTest(portable_path=portable_path):
                self.assertFalse(independently_contains_machine_path(portable_path))
                self.assertFalse(contains_obvious_machine_path(portable_path))

        reviewed_side_effect_names = (
            "reads_files",
            "writes_files",
            "modifies_qgis_state",
            "uses_network",
            "runs_shell",
            "destructive_possible",
        )
        risk_floor_cases = (
            ("qgis-state", "modifies_qgis_state", "R0", "R1"),
            ("file-write", "writes_files", "R1", "R2"),
            ("network", "uses_network", "R2", "R3"),
            ("shell", "runs_shell", "R2", "R3"),
            ("destructive", "destructive_possible", "R2", "R3"),
        )
        for label, side_effect, reported_risk, required_risk in risk_floor_cases:
            with self.subTest(
                uncatalogued_risk_floor=label,
                required_risk=required_risk,
            ):
                reviewed_side_effects = {
                    name: name == side_effect for name in reviewed_side_effect_names
                }
                self.assertEqual(
                    [
                        name
                        for name, enabled in reviewed_side_effects.items()
                        if enabled
                    ],
                    [side_effect],
                )
                capability = {
                    "source_type": "mcp",
                    "tool_name": "fixture_tool",
                    "risk_review_status": "reviewed",
                    "reviewed_risk_level": reported_risk,
                    "reviewed_side_effects": reviewed_side_effects,
                    "reviewed_invocation_policy": {"command_shape": "schema-defined"},
                    "reviewed_operation_id": "fixture_tool",
                    "reviewed_argument_target_kinds": {},
                    "risk_review_evidence": "Exact isolated side-effect review.",
                    "resolved_executable": None,
                    "resolved_executable_sha256": None,
                    "executable_evidence": None,
                    "reviewed_argv_template": None,
                    "reviewed_stdin_contract": None,
                }
                risk_errors = []
                validate_live_capability_contract(
                    f"CAP-{label}",
                    capability,
                    None,
                    risk_errors,
                )
                self.assertTrue(
                    any(
                        "uncatalogued risk understates reviewed effects" in error
                        for error in risk_errors
                    ),
                    risk_errors,
                )
                self.assertFalse(
                    any("uncatalogued CLI" in error for error in risk_errors),
                    risk_errors,
                )
                boundary_capability = copy.deepcopy(capability)
                boundary_capability["reviewed_risk_level"] = required_risk
                boundary_errors = []
                validate_live_capability_contract(
                    f"CAP-{label}-boundary",
                    boundary_capability,
                    None,
                    boundary_errors,
                )
                self.assertFalse(
                    any(
                        "uncatalogued risk understates reviewed effects" in error
                        for error in boundary_errors
                    ),
                    boundary_errors,
                )

        undisclosed_cli = copy.deepcopy(capability)
        undisclosed_cli.update(
            {
                "source_type": "cli",
                "tool_name": None,
                "reviewed_operation_id": "run-json-stdin",
                "reviewed_risk_level": "R3",
                "reviewed_side_effects": {
                    name: False for name in reviewed_side_effect_names
                },
                "reviewed_invocation_policy": {"command_shape": "argv-only"},
                "reviewed_argument_target_kinds": {},
            }
        )
        undisclosed_cli_errors = []
        validate_live_capability_contract(
            "CAP-undisclosed-cli",
            undisclosed_cli,
            None,
            undisclosed_cli_errors,
        )
        self.assertTrue(
            any(
                "uncatalogued CLI must disclose Shell effect" in error
                for error in undisclosed_cli_errors
            ),
            undisclosed_cli_errors,
        )
        self.assertFalse(
            any(
                "uncatalogued risk understates reviewed effects" in error
                for error in undisclosed_cli_errors
            ),
            undisclosed_cli_errors,
        )

        with tempfile.TemporaryDirectory(
            prefix="qgis-processing-invocation-"
        ) as processing_tmp:
            processing_fixture = build_processing_invocation_fixture(
                Path(processing_tmp)
            )
            processing_errors = []
            validate_node_invocation(
                "STEP-001",
                processing_fixture["node"],
                processing_fixture["tool"],
                processing_fixture["capability"],
                None,
                processing_errors,
            )
            self.assertEqual(processing_errors, [])

            mismatched_processing_node = copy.deepcopy(processing_fixture["node"])
            mismatched_processing_node["arguments"]["algorithm_id"] = "native:centroids"
            mismatched_algorithm_errors = []
            validate_node_invocation(
                "STEP-001",
                mismatched_processing_node,
                processing_fixture["tool"],
                processing_fixture["capability"],
                None,
                mismatched_algorithm_errors,
            )
            self.assertTrue(
                any(
                    "algorithm_id differs from selected tool" in error
                    for error in mismatched_algorithm_errors
                ),
                mismatched_algorithm_errors,
            )
        self.assertEqual(
            validate_schema_subset(catalog_data, catalog_schema),
            [],
        )
        self.assertEqual(
            validate_schema_subset(plan_template_data, plan_schema),
            [],
        )
        self.assertEqual(
            package_snapshot(),
            before_reads,
            "Loading plan_gate helpers must not modify the source skill package",
        )

        with tempfile.TemporaryDirectory() as tmp:
            self_test_root = Path(tmp)
            environment = os.environ.copy()
            environment.update(
                {
                    "PYTHONDONTWRITEBYTECODE": "1",
                    "TEMP": str(self_test_root),
                    "TMP": str(self_test_root),
                    "TMPDIR": str(self_test_root),
                }
            )
            actual_subprocess_run = subprocess.run
            with mock.patch.object(
                subprocess,
                "run",
                wraps=actual_subprocess_run,
            ) as explicit_self_test_run:
                self_test = subprocess.run(
                    [
                        sys.executable,
                        str(plan_gate_path),
                        "--self-test",
                    ],
                    cwd=self_test_root,
                    env=environment,
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    timeout=30,
                    check=False,
                )
            explicit_self_test_run.assert_called_once()
            self.assertEqual(
                self_test.returncode,
                0,
                f"stdout:\n{self_test.stdout}\nstderr:\n{self_test.stderr}",
            )
            self_test_result = json.loads(self_test.stdout)
            self.assertIs(self_test_result["ok"], True)
            self.assertEqual(self_test_result["mode"], "self-test")
            self.assertEqual(self_test_result["errors"], [])
            self.assertEqual(list(self_test_root.iterdir()), [])

            draft_verify = subprocess.run(
                [
                    sys.executable,
                    str(plan_gate_path),
                    "--mode",
                    "verify-staged",
                    "--plan",
                    str(skill_root / "assets" / "qgis-skill-plan.template.json"),
                    "--staged-package-root",
                    str(skill_root),
                ],
                cwd=self_test_root,
                env=environment,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=30,
                check=False,
            )
            self.assertEqual(
                draft_verify.returncode,
                2,
                f"stdout:\n{draft_verify.stdout}\nstderr:\n{draft_verify.stderr}",
            )
            draft_verify_result = json.loads(draft_verify.stdout)
            self.assertIs(draft_verify_result["ok"], False)
            self.assertEqual(draft_verify_result["mode"], "verify-staged")
            self.assertTrue(
                any(
                    "status approved or later" in error.casefold()
                    for error in draft_verify_result["errors"]
                )
            )
            self.assertIn(
                "status approved or later",
                (draft_verify.stdout + draft_verify.stderr).casefold(),
            )
            self.assertEqual(list(self_test_root.iterdir()), [])
        self.assertEqual(
            package_snapshot(),
            before_reads,
            "plan_gate.py --self-test must not modify the source skill package",
        )

        build_cli_fixture = plan_gate_helpers["_build_cli_fixture"]
        finalize_fixture_plan = plan_gate_helpers["_finalize_fixture_plan"]
        sha256_bytes = plan_gate_helpers["sha256_bytes"]
        sha256_value = plan_gate_helpers["sha256_value"]

        def write_cli_plan(fixture, plan, label):
            plan_path = Path(fixture["plan_path"]).with_name(f"{label}.json")
            plan_path.write_text(
                json.dumps(plan, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            return plan_path

        def run_gate_cli(fixture, mode, plan_path=None):
            fixture_root = Path(fixture["plan_path"]).parent
            environment = os.environ.copy()
            environment.update(
                {
                    "PYTHONDONTWRITEBYTECODE": "1",
                    "PYTHONIOENCODING": "utf-8",
                    "TEMP": str(fixture_root),
                    "TMP": str(fixture_root),
                    "TMPDIR": str(fixture_root),
                }
            )
            command = [
                sys.executable,
                str(plan_gate_path),
                "--mode",
                mode,
                "--plan",
                str(plan_path or fixture["plan_path"]),
                "--plan-schema",
                str(fixture["plan_schema_path"]),
                "--catalog",
                str(fixture["catalog_path"]),
                "--catalog-schema",
                str(fixture["catalog_schema_path"]),
            ]
            if mode == "verify-staged":
                command.extend(
                    [
                        "--staged-package-root",
                        str(fixture["staged_root"]),
                        "--skills-service-manifest",
                        str(fixture["manifest_path"]),
                        "--skills-root",
                        str(fixture["skills_root"]),
                    ]
                )
            return subprocess.run(
                command,
                cwd=fixture_root,
                env=environment,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=30,
                check=False,
            )

        def assert_cli_json_success(completed, mode):
            self.assertEqual(
                completed.returncode,
                0,
                f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}",
            )
            payload = json.loads(completed.stdout)
            self.assertIs(payload["ok"], True)
            self.assertEqual(payload["mode"], mode)
            self.assertEqual(payload["errors"], [])
            return payload

        def assert_cli_rejected(completed, mode, expected_error):
            self.assertEqual(
                completed.returncode,
                2,
                f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}",
            )
            payload = json.loads(completed.stdout)
            self.assertIs(payload["ok"], False)
            self.assertEqual(payload["mode"], mode)
            self.assertTrue(
                any(expected_error in error for error in payload["errors"]),
                f"Expected {expected_error!r} in {payload['errors']!r}",
            )
            return payload

        with tempfile.TemporaryDirectory(prefix="qgis-cli-positive-") as cli_tmp:
            positive_root = Path(cli_tmp)
            cli_fixture = build_cli_fixture(positive_root)
            staged_root = Path(cli_fixture["staged_root"])
            fixture_skills_root = Path(cli_fixture["skills_root"])
            creator_root = Path(cli_fixture["creator_root"])
            self.assertEqual(staged_root.parent, fixture_skills_root)
            self.assertEqual(creator_root.parent, fixture_skills_root)
            self.assertNotEqual(staged_root, creator_root)
            self.assertTrue(Path(cli_fixture["manifest_path"]).is_file())

            review = run_gate_cli(cli_fixture, "review")
            review_payload = assert_cli_json_success(review, "review")
            self.assertEqual(
                review_payload["computed"]["plan_hash"],
                cli_fixture["plan"]["plan_hash"],
            )

            rendered = run_gate_cli(cli_fixture, "render")
            self.assertEqual(
                rendered.returncode,
                0,
                f"stdout:\n{rendered.stdout}\nstderr:\n{rendered.stderr}",
            )
            approval_sentence = (
                f"批准计划 {cli_fixture['plan']['plan_id']} revision "
                f"{cli_fixture['plan']['revision']}，按该计划创建。"
            )
            review_section_headings = (
                "## A. 计划标识",
                "## B. 需求理解",
                "## C. 技能触发与边界",
                "## D. 数据契约和地理语义",
                "## E. 环境与工具发现快照",
                "## F. 选中工具",
                "## G. 工作流编排",
                "## H-I. 目标技能包与文件说明",
                "## J. 权限和副作用",
                "## K. 验证矩阵",
                "## L. 风险、替代和未解决决定",
                "## M. 修订日志",
                "## N. 明确审批请求",
            )
            actual_review_headings = tuple(
                line
                for line in rendered.stdout.splitlines()
                if line.startswith("## ") and line != "## 权威审查投影"
            )
            self.assertTupleEqual(actual_review_headings, review_section_headings)
            self.assertIn(approval_sentence, rendered.stdout)
            projection_heading = "## 权威审查投影"
            self.assertEqual(rendered.stdout.count(projection_heading), 1)
            projection_section = rendered.stdout.split(
                projection_heading,
                1,
            )[1]
            projection_fence = "~~~json\n"
            self.assertEqual(projection_section.count(projection_fence), 1)
            projection_json = projection_section.split(projection_fence, 1)[1].split(
                "\n~~~",
                1,
            )[0]
            authoritative_projection = json.loads(projection_json)
            core_review_keys = (
                "request_summary",
                "scope",
                "facts",
                "skill_identity",
                "usage_scenarios",
                "input_contracts",
                "output_contracts",
                "geospatial_semantics",
                "environment_contract",
                "capability_snapshot",
                "selected_tools",
                "known_issue_acceptances",
                "workflow_nodes",
                "target_skill_package",
                "permissions",
                "validation_matrix",
                "risks",
                "unresolved_decisions",
                "decision_log",
            )
            for key in core_review_keys:
                with self.subTest(authoritative_review_key=key):
                    self.assertIn(key, authoritative_projection)
                    self.assertEqual(
                        authoritative_projection[key],
                        cli_fixture["plan"][key],
                    )
            lifecycle_audit_keys = {
                "$schema",
                "status",
                "created_at",
                "updated_at",
                "approval",
            }
            self.assertSetEqual(
                set(authoritative_projection),
                set(cli_fixture["plan"]) - lifecycle_audit_keys,
            )
            for key, value in authoritative_projection.items():
                with self.subTest(authoritative_projection_value=key):
                    self.assertEqual(value, cli_fixture["plan"][key])

            verified = run_gate_cli(cli_fixture, "verify-staged")
            assert_cli_json_success(verified, "verify-staged")

            executable_leaf = Path(cli_fixture["executable_path"]).absolute()
            deterministic_reparse_cases = (
                ("lexical-leaf", executable_leaf),
                ("lexical-ancestor", executable_leaf.parent),
            )
            gate_globals = validate_common.__globals__
            for label, mocked_reparse_path in deterministic_reparse_cases:
                with self.subTest(cli_reparse=label):
                    mocked_key = os.path.normcase(str(mocked_reparse_path))

                    def deterministic_reparse_checker(path, expected=mocked_key):
                        return os.path.normcase(str(Path(path).absolute())) == expected

                    with mock.patch.dict(
                        gate_globals,
                        {"is_reparse_point": deterministic_reparse_checker},
                    ):
                        reparse_errors, _computed = validate_common(
                            cli_fixture["plan"],
                            cli_fixture["catalog"],
                            cli_fixture["plan_schema"],
                            cli_fixture["catalog_schema"],
                        )
                    self.assertTrue(
                        any(
                            "lexical path contains a reparse point" in error
                            and str(mocked_reparse_path) in error
                            for error in reparse_errors
                        ),
                        reparse_errors,
                    )

            manifest_path = Path(cli_fixture["manifest_path"]).absolute()
            supplied_skills_root = Path(cli_fixture["skills_root"]).absolute()
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            lexical_manifest_root = (
                manifest_path.parent / manifest["skillsRoot"]
            ).absolute()
            physical_manifest_root = lexical_manifest_root.resolve(strict=False)
            self.assertTrue(manifest_path.is_file())
            self.assertFalse(plan_gate_helpers["is_reparse_point"](manifest_path))
            self.assertEqual(
                supplied_skills_root.resolve(strict=False),
                physical_manifest_root,
            )
            manifest_root_reparse_cases = (
                ("lexical-leaf", lexical_manifest_root, 1),
                ("lexical-ancestor", lexical_manifest_root.parent, 2),
            )
            for (
                label,
                mocked_reparse_path,
                desired_match,
            ) in manifest_root_reparse_cases:
                with self.subTest(manifest_skills_root_reparse=label):
                    mocked_key = os.path.normcase(str(mocked_reparse_path))
                    matching_calls = []

                    def manifest_root_reparse_checker(
                        path,
                        expected=mocked_key,
                        desired=desired_match,
                    ):
                        actual = os.path.normcase(str(Path(path).absolute()))
                        if actual != expected:
                            return False
                        matching_calls.append(actual)
                        return len(matching_calls) == desired

                    manifest_errors = []
                    validated_root = validate_skills_root_binding(
                        cli_fixture["plan"],
                        manifest_path,
                        supplied_skills_root,
                        manifest_errors,
                        reparse_checker=manifest_root_reparse_checker,
                    )
                    self.assertEqual(validated_root, physical_manifest_root)
                    self.assertGreaterEqual(len(matching_calls), desired_match)
                    self.assertTrue(
                        any(
                            "manifest-derived Skills root path contains a reparse point"
                            in error
                            and str(mocked_reparse_path) in error
                            for error in manifest_errors
                        ),
                        manifest_errors,
                    )
                    self.assertFalse(
                        any(
                            error.startswith("Skills service manifest path")
                            or error.startswith("Skills root path")
                            for error in manifest_errors
                        ),
                        manifest_errors,
                    )
        self.assertFalse(positive_root.exists())

        def refresh_stdin_and_permission_effects(plan, node):
            node["effect_targets"] = [
                effect for effect in node["effect_targets"] if effect["kind"] != "stdin"
            ]
            stdin_hash = sha256_value(node["stdin_payload"])
            node["effect_targets"].append(
                {
                    "kind": "stdin",
                    "value": stdin_hash,
                    "permission_target": f"stdin:{stdin_hash}",
                    "rationale": "Exact deterministic invocation effect.",
                }
            )
            plan["permissions"][0]["targets"] = sorted(
                effect["permission_target"] for effect in node["effect_targets"]
            )

        def apply_coherent_uri_binding(
            plan,
            uri,
            target_kind,
            *,
            allow_network=False,
        ):
            pointer = "/stdin_payload/inputs/INPUT"
            canonical_endpoints = {
                "wss://example.invalid/socket": ("uri:wss://example.invalid/socket"),
                "mssql://example.invalid:1433/database": (
                    "uri:mssql://example.invalid/database"
                ),
                "custom+qgis://example.invalid/resource": (
                    "uri:custom+qgis://example.invalid/resource"
                ),
            }
            self.assertIn(uri, canonical_endpoints)
            endpoint = canonical_endpoints[uri]
            capability = plan["capability_snapshot"]["capabilities"][0]
            selected = plan["selected_tools"][0]
            reviewed = copy.deepcopy(capability["reviewed_nested_target_kinds"])
            reviewed[pointer] = target_kind
            capability["reviewed_nested_target_kinds"] = reviewed
            capability["reviewed_nested_target_kinds_hash"] = sha256_value(reviewed)
            selected_contract = copy.deepcopy(reviewed)
            selected["nested_target_kinds"] = selected_contract
            selected["nested_target_kinds_hash"] = sha256_value(selected_contract)

            uses_network = target_kind == "network-endpoint"
            capability["reviewed_side_effects"]["uses_network"] = uses_network
            selected["catalog_side_effects"]["uses_network"] = uses_network
            selected["network_policy"] = (
                "exact-endpoint-allowlist" if allow_network else "no-network"
            )

            node = plan["workflow_nodes"][0]
            node["stdin_payload"]["inputs"]["INPUT"] = uri
            input_binding = next(
                binding
                for binding in node["nested_target_bindings"]
                if binding["json_pointer"] == pointer
            )
            permission_target = f"{target_kind}:{endpoint}"
            input_binding["target_kind"] = target_kind
            input_binding["permission_target"] = permission_target
            node["effect_targets"] = [
                effect
                for effect in node["effect_targets"]
                if effect["kind"] != "input-path"
                and not (
                    effect["kind"] in {"database", "network-endpoint"}
                    and effect["value"] != "deny-all"
                )
                and not (
                    allow_network
                    and effect["kind"] == "network-endpoint"
                    and effect["value"] == "deny-all"
                )
            ]
            node["effect_targets"].append(
                {
                    "kind": target_kind,
                    "value": endpoint,
                    "permission_target": permission_target,
                    "rationale": "Exact reviewed URI effect.",
                }
            )
            refresh_stdin_and_permission_effects(plan, node)

        def apply_finalized_cli_mutation(label, plan, fixture, fixture_root):
            node = plan["workflow_nodes"][0]

            def replace_stdin_and_permission_effects():
                refresh_stdin_and_permission_effects(plan, node)

            if label == "algorithm-wildcard":
                plan["selected_tools"][0]["allowed_algorithm_ids"] = ["native:*"]
            elif label == "algorithm-wrong-id":
                plan["selected_tools"][0]["allowed_algorithm_ids"] = [
                    "native:centroids"
                ]
            elif label == "nested-stdin-escape":
                node["stdin_payload"]["inputs"]["OUTPUT"] = str(
                    fixture_root / "outside-output.gpkg"
                )
            elif label == "extra-effect":
                node["effect_targets"].append(
                    {
                        "kind": "qgis-state",
                        "value": "forged-extra-state",
                        "permission_target": "qgis-state:forged-extra-state",
                        "rationale": "Forged extra effect.",
                    }
                )
            elif label == "executable-escape":
                outside_executable = fixture_root / "outside-cli.bat"
                outside_executable.write_bytes(b"@echo off\r\n")
                node["executable"] = str(outside_executable)
            elif label == "executable-missing":
                node["executable"] = None
            elif label == "cwd-missing":
                node["cwd"] = None
            elif label == "executable-path-missing":
                node["executable"] = str(
                    Path(fixture["executable_path"]).parent / "missing-cli.bat"
                )
            elif label == "cwd-path-missing":
                node["cwd"] = str(Path(fixture["allowed_cwd_root"]) / "missing-cwd")
            elif label == "executable-is-directory":
                node["executable"] = str(Path(fixture["executable_path"]).parent)
            elif label == "cwd-is-file":
                cwd_file = Path(fixture["allowed_cwd_root"]) / "cwd-file.txt"
                cwd_file.write_bytes(b"not a directory")
                node["cwd"] = str(cwd_file)
            elif label == "output-role-forged-as-input":
                input_value = node["stdin_payload"]["inputs"]["INPUT"]
                node["stdin_payload"]["inputs"]["OUTPUT"] = input_value
                input_binding = next(
                    binding
                    for binding in node["nested_target_bindings"]
                    if binding["json_pointer"].endswith("/INPUT")
                )
                output_binding = next(
                    binding
                    for binding in node["nested_target_bindings"]
                    if binding["json_pointer"].endswith("/OUTPUT")
                )
                output_binding["target_kind"] = "input-path"
                output_binding["permission_target"] = input_binding["permission_target"]
                node["effect_targets"] = [
                    effect
                    for effect in node["effect_targets"]
                    if effect["kind"] != "output-path"
                ]
                replace_stdin_and_permission_effects()
            elif label.startswith("absolute-uri-none-"):
                uri_values = {
                    "absolute-uri-none-wss": "wss://example.invalid/socket",
                    "absolute-uri-none-mssql": (
                        "mssql://example.invalid:1433/database"
                    ),
                    "absolute-uri-none-unknown": (
                        "custom+qgis://example.invalid/resource"
                    ),
                }
                node["stdin_payload"]["inputs"]["INPUT"] = uri_values[label]
                input_binding = next(
                    binding
                    for binding in node["nested_target_bindings"]
                    if binding["json_pointer"].endswith("/INPUT")
                )
                input_binding["target_kind"] = "none"
                input_binding["permission_target"] = None
                node["effect_targets"] = [
                    effect
                    for effect in node["effect_targets"]
                    if effect["kind"] != "input-path"
                ]
                replace_stdin_and_permission_effects()
            elif label.startswith("uri-wrong-kind-"):
                uri_cases = {
                    "uri-wrong-kind-wss-database": (
                        "wss://example.invalid/socket",
                        "database",
                        False,
                    ),
                    "uri-wrong-kind-mssql-network": (
                        "mssql://example.invalid:1433/database",
                        "network-endpoint",
                        True,
                    ),
                    "uri-wrong-kind-unknown-database": (
                        "custom+qgis://example.invalid/resource",
                        "database",
                        False,
                    ),
                }
                uri, target_kind, allow_network = uri_cases[label]
                apply_coherent_uri_binding(
                    plan,
                    uri,
                    target_kind,
                    allow_network=allow_network,
                )
            elif label == "reviewed-nested-contract-drift":
                capability = plan["capability_snapshot"]["capabilities"][0]
                reviewed = copy.deepcopy(capability["reviewed_nested_target_kinds"])
                capability["reviewed_nested_target_kinds"] = reviewed
                reviewed["/stdin_payload/inputs/OUTPUT"] = "input-path"
                capability["reviewed_nested_target_kinds_hash"] = sha256_value(reviewed)
            elif label == "reviewed-nested-hash-drift":
                capability = plan["capability_snapshot"]["capabilities"][0]
                capability["reviewed_nested_target_kinds_hash"] = "sha256:" + "0" * 64
            elif label == "selected-nested-hash-drift":
                plan["selected_tools"][0]["nested_target_kinds_hash"] = (
                    "sha256:" + "0" * 64
                )
            elif label == "reviewed-parameter-contract-drift":
                capability = plan["capability_snapshot"]["capabilities"][0]
                reviewed = copy.deepcopy(capability["reviewed_parameter_constraints"])
                capability["reviewed_parameter_constraints"] = reviewed
                reviewed["properties"]["algorithm_id"]["minLength"] = 1
                capability["reviewed_parameter_constraints_hash"] = sha256_value(
                    reviewed
                )
            elif label == "reviewed-parameter-hash-drift":
                capability = plan["capability_snapshot"]["capabilities"][0]
                capability["reviewed_parameter_constraints_hash"] = "sha256:" + "0" * 64
            elif label == "selected-parameter-hash-drift":
                plan["selected_tools"][0]["parameter_constraints_hash"] = (
                    "sha256:" + "0" * 64
                )
            elif label == "argv-drift":
                node["materialized_argv"].append("--forged")
            elif label == "cwd-escape":
                node["cwd"] = str(fixture_root)
            elif label == "risk-underreport":
                capability = plan["capability_snapshot"]["capabilities"][0]
                capability["reviewed_risk_level"] = "R0"
                plan["selected_tools"][0]["risk_level"] = "R0"
                node["risk_level"] = "R0"
            elif label == "cli-undisclosed-shell":
                capability = plan["capability_snapshot"]["capabilities"][0]
                capability["reviewed_side_effects"]["runs_shell"] = False
                plan["selected_tools"][0]["catalog_side_effects"]["runs_shell"] = False
            elif label == "known-issue-missing":
                capability = plan["capability_snapshot"]["capabilities"][0]
                capability["known_issues"] = ["Fixture output can be incomplete."]
                plan["known_issue_acceptances"] = []
            else:
                self.fail(f"Unknown finalized CLI mutation: {label}")

        finalized_cli_negatives = (
            ("algorithm-wildcard", "algorithm wildcard is forbidden"),
            (
                "algorithm-wrong-id",
                "allowed_algorithm_ids must contain only exact algorithm_id",
            ),
            ("nested-stdin-escape", "outside selected allowed roots"),
            ("extra-effect", "un-derived extras"),
            ("executable-escape", "outside selected allowed roots"),
            ("executable-missing", "CLI executable is required"),
            ("cwd-missing", "CLI cwd is required"),
            (
                "executable-path-missing",
                "executable is not a live regular file",
            ),
            ("cwd-path-missing", "cwd is not a live regular directory"),
            (
                "executable-is-directory",
                "executable is not a live regular file",
            ),
            ("cwd-is-file", "cwd is not a live regular directory"),
            (
                "output-role-forged-as-input",
                "target_kind differs from authoritative reviewed contract",
            ),
            (
                "absolute-uri-none-wss",
                "target-like nested value cannot be none",
            ),
            (
                "absolute-uri-none-mssql",
                "target-like nested value cannot be none",
            ),
            (
                "absolute-uri-none-unknown",
                "target-like nested value cannot be none",
            ),
            (
                "uri-wrong-kind-wss-database",
                "network URI needs network-endpoint",
            ),
            (
                "uri-wrong-kind-mssql-network",
                "database URI needs database",
            ),
            (
                "uri-wrong-kind-unknown-database",
                "network URI needs network-endpoint",
            ),
            (
                "reviewed-nested-contract-drift",
                "nested_target_kinds differs from live capability",
            ),
            (
                "reviewed-nested-hash-drift",
                "reviewed_nested_target_kinds_hash differs from reviewed contract",
            ),
            (
                "selected-nested-hash-drift",
                "nested_target_kinds_hash differs from selected contract",
            ),
            (
                "reviewed-parameter-contract-drift",
                "parameter_constraints differs from live capability",
            ),
            (
                "reviewed-parameter-hash-drift",
                "reviewed_parameter_constraints_hash differs from reviewed contract",
            ),
            (
                "selected-parameter-hash-drift",
                "parameter_constraints_hash differs from selected contract",
            ),
            ("argv-drift", "materialized argv differs from template"),
            ("cwd-escape", "outside selected allowed roots"),
            (
                "risk-underreport",
                "uncatalogued risk understates reviewed effects",
            ),
            (
                "cli-undisclosed-shell",
                "uncatalogued CLI must disclose Shell effect",
            ),
            (
                "known-issue-missing",
                "known_issue_acceptances miss selected issues",
            ),
        )
        for label, expected_error in finalized_cli_negatives:
            with self.subTest(actual_cli_negative=label):
                with tempfile.TemporaryDirectory(
                    prefix=f"qgis-cli-{label}-"
                ) as cli_tmp:
                    fixture_root = Path(cli_tmp)
                    cli_fixture = build_cli_fixture(fixture_root)
                    candidate = copy.deepcopy(cli_fixture["plan"])
                    apply_finalized_cli_mutation(
                        label,
                        candidate,
                        cli_fixture,
                        fixture_root,
                    )
                    candidate = finalize_fixture_plan(candidate, approve=True)
                    candidate_path = write_cli_plan(
                        cli_fixture,
                        candidate,
                        label,
                    )
                    rejected = run_gate_cli(
                        cli_fixture,
                        "review",
                        candidate_path,
                    )
                    assert_cli_rejected(
                        rejected,
                        "review",
                        expected_error,
                    )

        correct_wss_network_cases = (
            (
                "no-network",
                False,
                "network target conflicts with no-network policy",
            ),
            ("exact-allowlist", True, None),
        )
        for label, allow_network, expected_error in correct_wss_network_cases:
            with self.subTest(actual_cli_wss_network=label):
                with tempfile.TemporaryDirectory(
                    prefix=f"qgis-cli-wss-network-{label}-"
                ) as cli_tmp:
                    fixture_root = Path(cli_tmp)
                    cli_fixture = build_cli_fixture(fixture_root)
                    candidate = copy.deepcopy(cli_fixture["plan"])
                    apply_coherent_uri_binding(
                        candidate,
                        "wss://example.invalid/socket",
                        "network-endpoint",
                        allow_network=allow_network,
                    )
                    candidate = finalize_fixture_plan(candidate, approve=True)
                    candidate_path = write_cli_plan(
                        cli_fixture,
                        candidate,
                        f"wss-network-{label}",
                    )
                    completed = run_gate_cli(
                        cli_fixture,
                        "review",
                        candidate_path,
                    )
                    if expected_error is None:
                        assert_cli_json_success(completed, "review")
                    else:
                        assert_cli_rejected(
                            completed,
                            "review",
                            expected_error,
                        )

        known_issue_text = "Fixture output can be incomplete."

        def add_exact_known_issue_acceptance(plan):
            capability = plan["capability_snapshot"]["capabilities"][0]
            capability["known_issues"] = [known_issue_text]
            acceptance = {
                "id": "ISSUE-ACCEPT-001",
                "plan_id": plan["plan_id"],
                "revision": plan["revision"],
                "capability_id": capability["id"],
                "capability_description_hash": capability["description_hash"],
                "issue": known_issue_text,
                "rationale": "The isolated validation detects incomplete output.",
                "acceptance_basis": "Explicitly reviewed in this revision.",
            }
            plan["known_issue_acceptances"] = [acceptance]
            return acceptance

        with tempfile.TemporaryDirectory(
            prefix="qgis-cli-known-issue-valid-"
        ) as cli_tmp:
            fixture_root = Path(cli_tmp)
            cli_fixture = build_cli_fixture(fixture_root)
            accepted_plan = copy.deepcopy(cli_fixture["plan"])
            add_exact_known_issue_acceptance(accepted_plan)
            accepted_plan = finalize_fixture_plan(accepted_plan, approve=True)
            accepted_path = write_cli_plan(
                cli_fixture,
                accepted_plan,
                "known-issue-valid",
            )
            accepted_result = run_gate_cli(
                cli_fixture,
                "review",
                accepted_path,
            )
            assert_cli_json_success(accepted_result, "review")

        known_issue_negative_cases = (
            (
                "stale-plan-id",
                "plan_id differs from current revision binding",
            ),
            (
                "stale-revision",
                "revision differs from current revision binding",
            ),
            (
                "stale-description-hash",
                "capability_description_hash differs from current revision binding",
            ),
            (
                "wrong-capability-id",
                "acceptance capability is not selected",
            ),
            ("blank-rationale", "rationale must be explicit"),
            (
                "blank-acceptance-basis",
                "acceptance_basis must be explicit",
            ),
            ("approximate-issue", "acceptance issue is not exact"),
            ("duplicate-issue", "duplicate known-issue acceptance"),
            (
                "unrelated-issue",
                "known_issue_acceptances contain unrelated issues",
            ),
            (
                "missing-issue",
                "known_issue_acceptances miss selected issues",
            ),
        )
        for label, expected_error in known_issue_negative_cases:
            with self.subTest(actual_cli_known_issue=label):
                with tempfile.TemporaryDirectory(
                    prefix=f"qgis-cli-known-issue-{label}-"
                ) as cli_tmp:
                    fixture_root = Path(cli_tmp)
                    cli_fixture = build_cli_fixture(fixture_root)
                    candidate = copy.deepcopy(cli_fixture["plan"])
                    acceptance = add_exact_known_issue_acceptance(candidate)
                    if label == "stale-plan-id":
                        acceptance["plan_id"] += "-stale"
                    elif label == "stale-revision":
                        acceptance["revision"] += 1
                    elif label == "stale-description-hash":
                        acceptance["capability_description_hash"] = "sha256:" + "0" * 64
                    elif label == "wrong-capability-id":
                        acceptance["capability_id"] = "CAP-NOT-SELECTED"
                    elif label == "blank-rationale":
                        acceptance["rationale"] = " "
                    elif label == "blank-acceptance-basis":
                        acceptance["acceptance_basis"] = " "
                    elif label == "approximate-issue":
                        acceptance["issue"] = known_issue_text.rstrip(".")
                    elif label == "duplicate-issue":
                        duplicate = copy.deepcopy(acceptance)
                        duplicate["id"] = "ISSUE-ACCEPT-002"
                        candidate["known_issue_acceptances"].append(duplicate)
                    elif label == "unrelated-issue":
                        unrelated = copy.deepcopy(acceptance)
                        unrelated["id"] = "ISSUE-ACCEPT-002"
                        unrelated["issue"] = "A different unreviewed issue."
                        candidate["known_issue_acceptances"].append(unrelated)
                    elif label == "missing-issue":
                        candidate["known_issue_acceptances"] = []
                    candidate = finalize_fixture_plan(candidate, approve=True)
                    candidate_path = write_cli_plan(
                        cli_fixture,
                        candidate,
                        f"known-issue-{label}",
                    )
                    rejected = run_gate_cli(
                        cli_fixture,
                        "review",
                        candidate_path,
                    )
                    assert_cli_rejected(
                        rejected,
                        "review",
                        expected_error,
                    )

        def apply_unfinalized_cli_drift(label, plan, fixture):
            if label == "stale-approval":
                plan["approval"]["approved_revision"] -= 1
            elif label == "plan-content-drift":
                plan["request_summary"]["interpreted_goal"] += " Drift."
            elif label == "capability-content-drift":
                capability = plan["capability_snapshot"]["capabilities"][0]
                capability["description"] += " Drift."
            elif label == "approval-hash-drift":
                plan["approval"]["approved_review_markdown_hash"] = "sha256:" + "0" * 64
            elif label == "executable-content-drift":
                executable_path = Path(fixture["executable_path"])
                executable_path.write_bytes(
                    executable_path.read_bytes() + b"rem drift\r\n"
                )
            else:
                self.fail(f"Unknown unfinalized CLI drift: {label}")

        unfinalized_cli_negatives = (
            (
                "stale-approval",
                "approval.approved_revision differs from the current approved plan",
            ),
            ("plan-content-drift", "plan_hash differs from current plan revision"),
            (
                "capability-content-drift",
                "capability_snapshot.snapshot_hash differs from content",
            ),
            (
                "approval-hash-drift",
                "approval.approved_review_markdown_hash differs",
            ),
            (
                "executable-content-drift",
                "resolved executable hash drifted",
            ),
        )
        for label, expected_error in unfinalized_cli_negatives:
            with self.subTest(actual_cli_drift=label):
                with tempfile.TemporaryDirectory(
                    prefix=f"qgis-cli-{label}-"
                ) as cli_tmp:
                    fixture_root = Path(cli_tmp)
                    cli_fixture = build_cli_fixture(fixture_root)
                    candidate = copy.deepcopy(cli_fixture["plan"])
                    apply_unfinalized_cli_drift(
                        label,
                        candidate,
                        cli_fixture,
                    )
                    candidate_path = write_cli_plan(
                        cli_fixture,
                        candidate,
                        label,
                    )
                    rejected = run_gate_cli(
                        cli_fixture,
                        "review",
                        candidate_path,
                    )
                    assert_cli_rejected(
                        rejected,
                        "review",
                        expected_error,
                    )

        staged_machine_values = (
            (
                "program-files",
                "Use "
                + "C:"
                + chr(92)
                + "Program Files"
                + chr(92)
                + "QGIS"
                + chr(92)
                + "bin\n",
            ),
            (
                "other-drive",
                "Use " + "D:" + chr(92) + "custom" + chr(92) + "data\n",
            ),
            ("unix-opt", "Use /" + "opt/qgis/bin\n"),
            ("unix-home", "Use /" + "home/qgis/data\n"),
            ("unix-users", "Use /" + "Users/qgis/data\n"),
            ("unix-data", "Use /" + "data/qgis/input\n"),
            ("remote-file-uri", "Use file://server/share/data\n"),
            ("forward-unc", "Use " + "/" + "/server/share/data\n"),
            (
                "normal-unc",
                "Use "
                + chr(92) * 2
                + "server"
                + chr(92)
                + "share"
                + chr(92)
                + "data\n",
            ),
            (
                "escaped-unc",
                '{"path":"'
                + chr(92) * 4
                + "server"
                + chr(92) * 2
                + "share"
                + chr(92) * 2
                + 'data"}\n',
            ),
            (
                "extended-unc",
                "Use "
                + chr(92) * 2
                + "?"
                + chr(92)
                + "UNC"
                + chr(92)
                + "server"
                + chr(92)
                + "share"
                + chr(92)
                + "data\n",
            ),
            (
                "wsl-unc",
                "Use "
                + chr(92) * 2
                + "wsl$"
                + chr(92)
                + "Ubuntu"
                + chr(92)
                + "home"
                + chr(92)
                + "qgis\n",
            ),
            (
                "unicode-unc",
                "Use " + chr(92) * 2 + "服务器" + chr(92) + "共享" + chr(92) + "数据\n",
            ),
        )
        for label, staged_text in staged_machine_values:
            with self.subTest(extensionless_staged_machine_path=label):
                self.assertTrue(independently_contains_machine_path(staged_text))
                with tempfile.TemporaryDirectory(
                    prefix=f"qgis-cli-staged-{label}-"
                ) as cli_tmp:
                    fixture_root = Path(cli_tmp)
                    cli_fixture = build_cli_fixture(fixture_root)
                    contract_path = (
                        Path(cli_fixture["staged_root"]) / "references" / "contract"
                    )
                    self.assertEqual(contract_path.suffix, "")
                    raw = staged_text.encode("utf-8")
                    contract_path.write_bytes(raw)
                    candidate = copy.deepcopy(cli_fixture["plan"])
                    contract_entry = next(
                        entry
                        for entry in candidate["target_skill_package"]["files"]
                        if entry["relative_path"] == "references/contract"
                    )
                    contract_entry["expected_sha256"] = sha256_bytes(raw)
                    contract_entry["expected_size_bytes"] = len(raw)
                    candidate = finalize_fixture_plan(candidate, approve=True)
                    candidate_path = write_cli_plan(
                        cli_fixture,
                        candidate,
                        f"staged-{label}",
                    )
                    rejected = run_gate_cli(
                        cli_fixture,
                        "verify-staged",
                        candidate_path,
                    )
                    assert_cli_rejected(
                        rejected,
                        "verify-staged",
                        "staged text contains an obvious machine path",
                    )

        portable_regex_text = '{"regex":"' + chr(92) * 4 + 'd+"}\n'
        self.assertFalse(independently_contains_machine_path(portable_regex_text))
        with tempfile.TemporaryDirectory(
            prefix="qgis-cli-staged-portable-regex-"
        ) as cli_tmp:
            fixture_root = Path(cli_tmp)
            cli_fixture = build_cli_fixture(fixture_root)
            contract_path = Path(cli_fixture["staged_root"]) / "references" / "contract"
            self.assertEqual(contract_path.suffix, "")
            raw = portable_regex_text.encode("utf-8")
            contract_path.write_bytes(raw)
            candidate = copy.deepcopy(cli_fixture["plan"])
            contract_entry = next(
                entry
                for entry in candidate["target_skill_package"]["files"]
                if entry["relative_path"] == "references/contract"
            )
            contract_entry["expected_sha256"] = sha256_bytes(raw)
            contract_entry["expected_size_bytes"] = len(raw)
            candidate = finalize_fixture_plan(candidate, approve=True)
            candidate_path = write_cli_plan(
                cli_fixture,
                candidate,
                "staged-portable-regex",
            )
            verified = run_gate_cli(
                cli_fixture,
                "verify-staged",
                candidate_path,
            )
            assert_cli_json_success(verified, "verify-staged")

        workflow = resource_contents["references/qgis-skill-creation-workflow.md"]
        domain_checklist = resource_contents[
            "references/geospatial-domain-checklists.md"
        ].casefold()
        generated_skill_template = resource_contents[
            "assets/generated-skill-SKILL.template.md"
        ].casefold()
        for domain_term in (
            "point-cloud",
            "mesh",
            "three-dimensional",
            "geospatial-service",
            "spatiotemporal",
        ):
            with self.subTest(domain_term=domain_term):
                self.assertIn(domain_term, domain_checklist)
        for generated_section in (
            "### point-cloud data",
            "### mesh and temporal simulation data",
            "### three-dimensional data",
            "### geospatial services",
        ):
            with self.subTest(generated_section=generated_section):
                self.assertIn(generated_section, generated_skill_template)
        skill_semantics = skill_document.casefold()
        expected_state_machine = (
            "DISCOVER → ANALYZE → DRAFT_PLAN → AWAIT_EXPLICIT_APPROVAL "
            "→ CREATE → VALIDATE → FORWARD_TEST → DELIVER"
        )
        state_machine_states = (
            "DISCOVER",
            "ANALYZE",
            "DRAFT_PLAN",
            "AWAIT_EXPLICIT_APPROVAL",
            "CREATE",
            "VALIDATE",
            "FORWARD_TEST",
            "DELIVER",
        )
        state_machine_documents = {
            "SKILL.md": skill_document,
            "references/qgis-skill-creation-workflow.md": workflow,
        }
        for document_path, document in state_machine_documents.items():
            state_blocks = [
                block
                for block in re.findall(
                    r"```text\s*\n(.*?)\n```",
                    document,
                    flags=re.DOTALL,
                )
                if "DISCOVER" in block
            ]
            with self.subTest(state_machine_document=document_path):
                self.assertEqual(len(state_blocks), 1)
                state_machine_text = state_blocks[0]
                self.assertEqual(
                    " ".join(state_machine_text.split()),
                    expected_state_machine,
                )
                state_positions = []
                for state in state_machine_states:
                    self.assertEqual(state_machine_text.count(state), 1)
                    state_positions.append(state_machine_text.index(state))
                self.assertEqual(state_positions, sorted(state_positions))
        semantic_terms = {
            "tool discovery": (
                "tool discovery",
                "discover available tools",
                "enumerate the current",
            ),
            "requirement decomposition": (
                "requirement decomposition",
                "decompose requirements",
                "decompose the request",
            ),
            "detailed plan": ("detailed plan", "implementation plan"),
            "revision loop": ("revision loop", "revise the plan", "revise"),
            "approval gate": ("explicit approval", "approval gate"),
            "containment": ("containment", "contained output"),
            "no overwrite": ("no-overwrite", "must not overwrite"),
            "collision": ("collision", "name conflict"),
            "resources first": ("resources-first", "resources first"),
            "SKILL last": ("skill.md last", "`skill.md` last", "skill-last"),
            "forward test": ("forward-test", "forward test"),
        }
        for requirement, alternatives in semantic_terms.items():
            with self.subTest(requirement=requirement):
                self.assertTrue(
                    any(term in skill_semantics for term in alternatives),
                    f"Missing stable {requirement} semantics",
                )

        for token in (
            "plan_id",
            "revision",
            "strict",
            "list_skills",
            "read_skill",
            "read_skill_resource",
            "references/qgis-tools.catalog.json",
            "references/qgis-skill-plan.schema.json",
            "assets/qgis-skill-plan.template.json",
            "scripts/plan_gate.py",
        ):
            with self.subTest(semantic_token=token):
                self.assertIn(token, skill_semantics)

        self.assertRegex(
            skill_semantics,
            r"(?:current|exact)[^\n]{0,200}plan_id[^\n]{0,200}revision|"
            r"plan_id[^\n]{0,200}revision[^\n]{0,200}(?:current|exact)",
        )
        self.assertRegex(
            skill_semantics,
            r"(?:before|until)[^\n]{0,200}approv[^\n]{0,200}"
            r"(?:must not|do not|never|forbid|no)[^\n]{0,120}"
            r"(?:create|write|modify|update)|"
            r"(?:must not|do not|never|forbid|no)[^\n]{0,120}"
            r"(?:create|write|modify|update)[^\n]{0,200}"
            r"(?:before|until)[^\n]{0,120}approv|"
            r"(?:批准|审批|核准)前[^\n]{0,200}(?:不得|不能|禁止)"
            r"[^\n]{0,120}(?:创建|写入|修改|更新)",
        )
        self.assertRegex(
            "\n".join((skill_document, workflow)).casefold(),
            r"(?:旧|old)[^\n]{0,80}(?:批准|approv)[^\n]{0,100}"
            r"(?:不可复用|失效|invalid|not reusable)",
        )

        permission_text = "\n".join((skill_document, workflow)).casefold()
        self.assertRegex(
            permission_text,
            r"(?:session|host)[^\n]{0,160}(?:available|provided|exposed)"
            r"[^\n]{0,80}tools|tools[^\n]{0,160}"
            r"(?:available|provided|exposed)[^\n]{0,80}(?:session|host)",
        )
        self.assertRegex(
            permission_text,
            r"(?:static|bundled)[^\n]{0,120}catalog[^\n]{0,160}"
            r"(?:does not|must not|cannot|never)[^\n]{0,80}(?:limit|restrict)|"
            r"catalog[^\n]{0,160}(?:does not|must not|cannot|never)"
            r"[^\n]{0,80}(?:limit|restrict)[^\n]{0,80}(?:discover|tool)",
        )
        self.assertRegex(
            permission_text,
            r"(?:does not|must not|cannot|never)[^\n]{0,120}"
            r"(?:bypass|override|circumvent)[^\n]{0,160}"
            r"(?:host|llama-ui)[^\n]{0,120}(?:permission|authorization|approval)|"
            r"(?:host|llama-ui)[^\n]{0,120}(?:permission|authorization|approval)"
            r"[^\n]{0,160}(?:does not|must not|cannot|never)[^\n]{0,120}"
            r"(?:bypass|override|circumvent)",
        )

        portable_documents = {"SKILL.md": skill_document, **resource_contents}
        drive_path_pattern = re.compile(r"(?<![A-Za-z0-9])[A-Za-z]:[\\/]")
        unc_path_pattern = re.compile(
            r"(?<![A-Za-z0-9:/])(?:\\\\|//)"
            r"[A-Za-z0-9][A-Za-z0-9._-]*[\\/]"
            r"[A-Za-z0-9$][A-Za-z0-9$._-]*"
        )
        allowed_drive_fixture_count = 0
        for relative_path, content in portable_documents.items():
            for line_number, line in enumerate(content.splitlines(), 1):
                drive_matches = list(drive_path_pattern.finditer(line))
                if drive_matches:
                    is_unsafe_fixture = (
                        relative_path == "scripts/plan_gate.py"
                        and "unsafe = [" in line
                        and line.count('"C:/x"') == 1
                    )
                    if not is_unsafe_fixture:
                        self.fail(
                            f"Nonportable drive path at "
                            f"{relative_path}:{line_number}: {line}"
                        )
                    allowed_drive_fixture_count += 1
                    line_without_fixture = line.replace('"C:/x"', "")
                    self.assertIsNone(drive_path_pattern.search(line_without_fixture))
                with self.subTest(
                    portable_document=relative_path,
                    line=line_number,
                ):
                    self.assertIsNone(unc_path_pattern.search(line))
                    self.assertIsNone(machine_path_pattern.search(line))
                    self.assertIsNone(independent_machine_path_pattern.search(line))
        self.assertEqual(allowed_drive_fixture_count, 0)
        self.assertIn(
            '        "C:" + "/x",',
            resource_contents["scripts/plan_gate.py"],
        )

    def _call_tool(self, server, name, arguments):
        response = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 100,
                "method": "tools/call",
                "params": {"name": name, "arguments": arguments},
            }
        )
        self.assertNotIn("error", response)
        return json.loads(response["result"]["content"][0]["text"])

    def _tool_names(self, server):
        return {
            tool["name"]
            for tool in server.handle_json_rpc(
                {"jsonrpc": "2.0", "id": 200, "method": "tools/list", "params": {}}
            )["result"]["tools"]
        }

    def _tools_by_name(self, server):
        return {
            tool["name"]: tool
            for tool in server.handle_json_rpc(
                {"jsonrpc": "2.0", "id": 203, "method": "tools/list", "params": {}}
            )["result"]["tools"]
        }

    def _prompt_names(self, server):
        return {
            prompt["name"]
            for prompt in server.handle_json_rpc(
                {"jsonrpc": "2.0", "id": 201, "method": "prompts/list", "params": {}}
            )["result"]["prompts"]
        }

    def _get_prompt_text(self, server, name, task="Read skill"):
        response = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 202,
                "method": "prompts/get",
                "params": {"name": name, "arguments": {"task": task}},
            }
        )
        self.assertNotIn("error", response)
        return response["result"]["messages"][0]["content"]["text"]


if __name__ == "__main__":
    unittest.main()
