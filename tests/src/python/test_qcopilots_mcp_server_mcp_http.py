"""QGIS unit tests for QCopilots MCP JSON-RPC handling.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import base64
import copy
import json
import logging
import os
import socket
import sys
import tempfile
import threading
import types
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen


class TestQCopilotsMcpServerMcpHttp(unittest.TestCase):
    HTTP_AUTH_TOKEN = "qcopilots-unit-test-service-token"

    def _http_headers(self, headers=None):
        return {
            "Authorization": f"Bearer {self.HTTP_AUTH_TOKEN}",
            **(headers or {}),
        }

    def _raw_post_response(
        self,
        port,
        path,
        content_type,
        content_length,
        extra_headers=None,
    ):
        additional_headers = "".join(
            f"{name}: {value}\r\n" for name, value in (extra_headers or {}).items()
        )
        request = (
            f"POST {path} HTTP/1.1\r\n"
            f"Host: 127.0.0.1:{port}\r\n"
            f"Content-Type: {content_type}\r\n"
            f"Content-Length: {content_length}\r\n"
            f"{additional_headers}"
            "Connection: keep-alive\r\n"
            "\r\n"
        ).encode("ascii")
        with socket.create_connection(("127.0.0.1", port), timeout=2) as connection:
            connection.settimeout(2)
            connection.sendall(request)
            received = bytearray()
            expected_size = None
            while True:
                try:
                    chunk = connection.recv(4096)
                except socket.timeout as exc:
                    raise AssertionError(
                        "Timed out before receiving a complete HTTP response"
                    ) from exc
                if not chunk:
                    break
                received.extend(chunk)
                header_end = received.find(b"\r\n\r\n")
                if header_end >= 0 and expected_size is None:
                    headers = received[:header_end].decode("iso-8859-1")
                    content_lengths = [
                        line.split(":", 1)[1].strip()
                        for line in headers.split("\r\n")[1:]
                        if line.casefold().startswith("content-length:")
                    ]
                    self.assertLessEqual(len(content_lengths), 1)
                    if content_lengths:
                        expected_size = header_end + 4 + int(content_lengths[0])
                if expected_size is not None and len(received) >= expected_size:
                    del received[expected_size:]
                    break
            self.assertIn(b"\r\n\r\n", received)
            if expected_size is not None:
                self.assertEqual(len(received), expected_size)
            return received.decode("iso-8859-1")

    def _stop_http_server(self, httpd, thread):
        httpd.shutdown()
        httpd.server_close()
        thread.join(timeout=5)
        self.assertFalse(thread.is_alive(), "HTTP server thread did not terminate")

    def test_initialize_tools_list_and_tools_call(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer, McpTool

        calls = []

        def echo(arguments, context):
            calls.append((arguments, context.client_name))
            return {
                "content": [{"type": "text", "text": arguments["text"]}],
                "structuredContent": {"echoed": arguments["text"]},
            }

        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[
                McpTool(
                    name="echo",
                    description="Echo text for tests",
                    input_schema={
                        "type": "object",
                        "properties": {"text": {"type": "string"}},
                        "required": ["text"],
                    },
                    handler=echo,
                )
            ],
        )

        initialize = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {},
                    "clientInfo": {"name": "unit-test", "version": "1.0"},
                },
            },
            session_id="unit-session",
        )
        self.assertEqual(initialize["jsonrpc"], "2.0")
        self.assertEqual(initialize["id"], 1)
        self.assertIn("protocolVersion", initialize["result"])
        self.assertEqual(
            initialize["result"]["serverInfo"],
            {"name": "qcopilots-test", "version": "1.0.0"},
        )
        self.assertIn("tools", initialize["result"]["capabilities"])

        tools = server.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}}
        )
        self.assertEqual(
            tools["result"]["tools"],
            [
                {
                    "name": "echo",
                    "description": "Echo text for tests",
                    "inputSchema": {
                        "type": "object",
                        "properties": {"text": {"type": "string"}},
                        "required": ["text"],
                    },
                }
            ],
        )

        call = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {"name": "echo", "arguments": {"text": "hello"}},
            },
            session_id="unit-session",
        )
        self.assertEqual(call["result"]["content"], [{"type": "text", "text": "hello"}])
        self.assertEqual(call["result"]["structuredContent"], {"echoed": "hello"})
        self.assertEqual(calls, [({"text": "hello"}, "unit-test")])

    def test_initialize_prompts_list_and_prompts_get(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer, McpPrompt, McpTool

        calls = []

        def summarize(arguments, context):
            calls.append((arguments, context.client_name))
            return {
                "description": "Summary prompt",
                "messages": [
                    {
                        "role": "user",
                        "content": {
                            "type": "text",
                            "text": f"Summarize {arguments['topic']}",
                        },
                    }
                ],
            }

        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[
                McpTool(
                    name="noop",
                    description="No operation",
                    input_schema={"type": "object"},
                    handler=lambda arguments: arguments,
                )
            ],
            prompts=[
                McpPrompt(
                    name="summarize",
                    description="Summarize a topic",
                    arguments=[
                        {
                            "name": "topic",
                            "description": "Topic to summarize",
                            "required": True,
                        }
                    ],
                    handler=summarize,
                    title="Summarize",
                )
            ],
        )

        initialize = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {"clientInfo": {"name": "unit-test", "version": "1.0"}},
            },
            session_id="unit-session",
        )
        self.assertIn("prompts", initialize["result"]["capabilities"])

        prompts = server.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 2, "method": "prompts/list", "params": {}}
        )
        self.assertEqual(
            prompts["result"]["prompts"],
            [
                {
                    "name": "summarize",
                    "description": "Summarize a topic",
                    "arguments": [
                        {
                            "name": "topic",
                            "description": "Topic to summarize",
                            "required": True,
                        }
                    ],
                    "title": "Summarize",
                }
            ],
        )

        prompt = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "prompts/get",
                "params": {
                    "name": "summarize",
                    "arguments": {"topic": "skills"},
                },
            },
            session_id="unit-session",
        )
        self.assertEqual(prompt["result"]["description"], "Summary prompt")
        self.assertEqual(
            prompt["result"]["messages"][0]["content"]["text"],
            "Summarize skills",
        )
        self.assertEqual(calls, [({"topic": "skills"}, "unit-test")])

    def test_list_methods_support_opaque_cursor_pagination(self):
        from qcopilots_common.mcp_http import (
            McpJsonRpcServer,
            McpPrompt,
            McpResource,
            McpTool,
        )

        tools_source = [
            McpTool(
                name=f"tool-{index}",
                description=f"Tool {index}",
                input_schema={"type": "object"},
                handler=lambda arguments: arguments,
            )
            for index in range(5)
        ]
        resources = [
            McpResource(
                uri=f"resource://test/{index}",
                name=f"resource-{index}",
                mime_type="text/plain",
                handler=lambda value=str(index): value,
            )
            for index in range(3)
        ]
        prompts = [
            McpPrompt(
                name=f"prompt-{index}",
                description=f"Prompt {index}",
                arguments=[],
                handler=lambda arguments: arguments,
            )
            for index in range(3)
        ]
        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=lambda: list(tools_source),
            resources=resources,
            prompts=prompts,
            list_page_size=2,
        )

        first_tools = server.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}}
        )
        self.assertEqual(
            [tool["name"] for tool in first_tools["result"]["tools"]],
            ["tool-0", "tool-1"],
        )
        self.assertIn("nextCursor", first_tools["result"])

        second_tools = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "tools/list",
                "params": {"cursor": first_tools["result"]["nextCursor"]},
            }
        )
        self.assertEqual(
            [tool["name"] for tool in second_tools["result"]["tools"]],
            ["tool-2", "tool-3"],
        )
        self.assertIn("nextCursor", second_tools["result"])

        third_tools = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/list",
                "params": {"cursor": second_tools["result"]["nextCursor"]},
            }
        )
        self.assertEqual(
            [tool["name"] for tool in third_tools["result"]["tools"]],
            ["tool-4"],
        )
        self.assertNotIn("nextCursor", third_tools["result"])

        resources_page = server.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 4, "method": "resources/list", "params": {}}
        )
        self.assertEqual(
            [resource["uri"] for resource in resources_page["result"]["resources"]],
            ["resource://test/0", "resource://test/1"],
        )
        self.assertIn("nextCursor", resources_page["result"])
        resources_last_page = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 5,
                "method": "resources/list",
                "params": {"cursor": resources_page["result"]["nextCursor"]},
            }
        )
        self.assertEqual(
            [
                resource["uri"]
                for resource in resources_last_page["result"]["resources"]
            ],
            ["resource://test/2"],
        )
        self.assertNotIn("nextCursor", resources_last_page["result"])

        prompts_page = server.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 6, "method": "prompts/list", "params": {}}
        )
        self.assertEqual(
            [prompt["name"] for prompt in prompts_page["result"]["prompts"]],
            ["prompt-0", "prompt-1"],
        )
        self.assertIn("nextCursor", prompts_page["result"])
        prompts_last_page = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 7,
                "method": "prompts/list",
                "params": {"cursor": prompts_page["result"]["nextCursor"]},
            }
        )
        self.assertEqual(
            [prompt["name"] for prompt in prompts_last_page["result"]["prompts"]],
            ["prompt-2"],
        )
        self.assertNotIn("nextCursor", prompts_last_page["result"])

        cross_method_cursor = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 8,
                "method": "prompts/list",
                "params": {"cursor": first_tools["result"]["nextCursor"]},
            }
        )
        self.assertEqual(cross_method_cursor["error"]["code"], -32602)
        self.assertIn(
            "Invalid pagination cursor",
            cross_method_cursor["error"]["message"],
        )

        tampered_payload = json.loads(
            base64.urlsafe_b64decode(
                first_tools["result"]["nextCursor"].encode("ascii")
                + b"=" * (-len(first_tools["result"]["nextCursor"]) % 4)
            ).decode("utf-8")
        )
        tampered_payload["start"] = 4
        tampered_cursor = base64.urlsafe_b64encode(
            json.dumps(
                tampered_payload,
                separators=(",", ":"),
                sort_keys=True,
            ).encode("utf-8")
        ).decode("ascii").rstrip("=")
        tampered_response = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 9,
                "method": "tools/list",
                "params": {"cursor": tampered_cursor},
            }
        )
        self.assertEqual(tampered_response["error"]["code"], -32602)
        self.assertIn(
            "Invalid pagination cursor",
            tampered_response["error"]["message"],
        )

        resource_templates = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 10,
                "method": "resources/templates/list",
                "params": {},
            }
        )
        self.assertEqual(
            resource_templates["result"],
            {"resourceTemplates": []},
        )

        invalid_cursor = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 11,
                "method": "tools/list",
                "params": {"cursor": "not-a-valid-cursor"},
            }
        )
        self.assertEqual(invalid_cursor["error"]["code"], -32602)
        self.assertIn("Invalid pagination cursor", invalid_cursor["error"]["message"])

        tools_source.append(
            McpTool(
                name="tool-5",
                description="Tool 5",
                input_schema={"type": "object"},
                handler=lambda arguments: arguments,
            )
        )
        stale_cursor = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 12,
                "method": "tools/list",
                "params": {"cursor": first_tools["result"]["nextCursor"]},
            }
        )
        self.assertEqual(stale_cursor["error"]["code"], -32602)
        self.assertIn("Invalid pagination cursor", stale_cursor["error"]["message"])

        default_server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=lambda: list(tools_source),
        )
        default_tools = default_server.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 13, "method": "tools/list", "params": {}}
        )
        self.assertEqual(
            [tool["name"] for tool in default_tools["result"]["tools"]],
            [f"tool-{index}" for index in range(6)],
        )
        self.assertNotIn("nextCursor", default_tools["result"])

        default_cursor = default_server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 14,
                "method": "tools/list",
                "params": {"cursor": first_tools["result"]["nextCursor"]},
            }
        )
        self.assertEqual(default_cursor["error"]["code"], -32602)
        self.assertIn("Invalid pagination cursor", default_cursor["error"]["message"])

        with self.assertRaisesRegex(ValueError, "list_page_size"):
            McpJsonRpcServer(
                server_name="qcopilots-test",
                server_version="1.0.0",
                tools=[],
                list_page_size=-1,
            )

    def test_json_rpc_sessions_isolate_context(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer, McpTool

        def client_name(arguments, context):
            del arguments
            return context.client_name

        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[
                McpTool(
                    name="client_name",
                    description="Return client name",
                    input_schema={"type": "object"},
                    handler=client_name,
                )
            ],
        )

        for session_id, name in (("session-a", "client-a"), ("session-b", "client-b")):
            server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": session_id,
                    "method": "initialize",
                    "params": {"clientInfo": {"name": name, "version": "1.0"}},
                },
                session_id=session_id,
            )

        call_a = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "tools/call",
                "params": {"name": "client_name", "arguments": {}},
            },
            session_id="session-a",
        )
        call_b = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "tools/call",
                "params": {"name": "client_name", "arguments": {}},
            },
            session_id="session-b",
        )
        call_without_session = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {"name": "client_name", "arguments": {}},
            }
        )

        self.assertEqual(call_a["result"]["content"][0]["text"], "client-a")
        self.assertEqual(call_b["result"]["content"][0]["text"], "client-b")
        self.assertEqual(call_without_session["result"]["content"][0]["text"], "")

    def test_session_contexts_use_lru_ttl_and_validate_client_info(self):
        from qcopilots_common.mcp_http import (
            MAX_MCP_CLIENT_INFO_FIELD_LENGTH,
            McpJsonRpcServer,
            McpTool,
        )

        class FakeClock:
            def __init__(self):
                self.value = 1000.0

            def __call__(self):
                return self.value

            def advance(self, seconds):
                self.value += seconds

        def client_identity(arguments, context):
            del arguments
            return {
                "name": context.client_name,
                "version": context.client_version,
            }

        def make_server(clock, max_session_contexts=2, session_ttl_seconds=10.0):
            return McpJsonRpcServer(
                server_name="qcopilots-context-test",
                server_version="1.0.0",
                tools=[
                    McpTool(
                        name="client_identity",
                        description="Return the active client identity",
                        input_schema={"type": "object"},
                        handler=client_identity,
                    )
                ],
                max_session_contexts=max_session_contexts,
                session_ttl_seconds=session_ttl_seconds,
                clock=clock,
            )

        def initialize(server, session_id, name, version="1.0"):
            response = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": f"initialize-{session_id}",
                    "method": "initialize",
                    "params": {
                        "clientInfo": {
                            "name": name,
                            "version": version,
                        }
                    },
                },
                session_id=session_id,
            )
            self.assertNotIn("error", response)
            return response

        def identity(server, session_id, request_id):
            response = server.handle_json_rpc(
                {
                    "jsonrpc": "2.0",
                    "id": request_id,
                    "method": "tools/call",
                    "params": {
                        "name": "client_identity",
                        "arguments": {},
                    },
                },
                session_id=session_id,
            )
            self.assertNotIn("error", response)
            return response["result"]["structuredContent"]

        clock = FakeClock()
        server = make_server(clock)
        initialize(server, "session-a", "client-a")
        initialize(server, "session-b", "client-b")
        self.assertEqual(tuple(server._contexts), ("session-a", "session-b"))

        self.assertEqual(
            identity(server, "session-a", 1),
            {"name": "client-a", "version": "1.0"},
        )
        self.assertEqual(tuple(server._contexts), ("session-b", "session-a"))

        initialize(server, "session-c", "client-c")
        self.assertEqual(tuple(server._contexts), ("session-a", "session-c"))
        self.assertEqual(
            identity(server, "session-b", 2),
            {"name": "", "version": ""},
        )

        clock.advance(9.0)
        self.assertEqual(
            identity(server, "session-a", 3),
            {"name": "client-a", "version": "1.0"},
        )
        clock.advance(2.0)
        self.assertEqual(
            identity(server, "session-c", 4),
            {"name": "", "version": ""},
        )
        self.assertEqual(tuple(server._contexts), ("session-a",))
        clock.advance(8.0)
        self.assertEqual(
            identity(server, "session-a", 5),
            {"name": "", "version": ""},
        )
        self.assertEqual(tuple(server._contexts), ())

        for index in range(5):
            initialize(server, f"reconnect-{index}", f"client-{index}")
            self.assertLessEqual(len(server._contexts), 2)
        self.assertEqual(
            tuple(server._contexts),
            ("reconnect-3", "reconnect-4"),
        )
        server.clear_session("reconnect-3")
        self.assertEqual(tuple(server._contexts), ("reconnect-4",))
        server.clear_all_sessions()
        self.assertEqual(tuple(server._contexts), ())

        zero_capacity = make_server(
            clock,
            max_session_contexts=0,
            session_ttl_seconds=10.0,
        )
        initialize(zero_capacity, "disabled-session", "disabled-client")
        self.assertEqual(tuple(zero_capacity._contexts), ())
        self.assertEqual(
            identity(zero_capacity, "disabled-session", 6),
            {"name": "", "version": ""},
        )

        zero_ttl = make_server(
            clock,
            max_session_contexts=2,
            session_ttl_seconds=0.0,
        )
        initialize(zero_ttl, "ttl-zero", "ttl-zero-client")
        self.assertEqual(tuple(zero_ttl._contexts), ("ttl-zero",))
        self.assertEqual(
            identity(zero_ttl, "ttl-zero", 7),
            {"name": "", "version": ""},
        )
        self.assertEqual(tuple(zero_ttl._contexts), ())

        validation_server = make_server(clock, max_session_contexts=8)
        valid_client_info = (
            ("omitted", {}),
            ("empty-object", {"clientInfo": {}}),
            (
                "empty-fields",
                {"clientInfo": {"name": "", "version": ""}},
            ),
            (
                "boundary-fields",
                {
                    "clientInfo": {
                        "name": "n" * MAX_MCP_CLIENT_INFO_FIELD_LENGTH,
                        "version": "v" * MAX_MCP_CLIENT_INFO_FIELD_LENGTH,
                    }
                },
            ),
        )
        for case_name, params in valid_client_info:
            with self.subTest(valid_client_info=case_name):
                response = validation_server.handle_json_rpc(
                    {
                        "jsonrpc": "2.0",
                        "id": case_name,
                        "method": "initialize",
                        "params": params,
                    },
                    session_id=f"valid-{case_name}",
                )
                self.assertNotIn("error", response)
                self.assertIn("result", response)

        invalid_client_info = (
            ("client-info-list", [], "clientInfo must be an object"),
            ("name-type", {"name": 1}, "clientInfo.name must be a string"),
            (
                "version-type",
                {"version": []},
                "clientInfo.version must be a string",
            ),
            (
                "name-too-long",
                {"name": "n" * (MAX_MCP_CLIENT_INFO_FIELD_LENGTH + 1)},
                "clientInfo.name must not exceed 256 characters",
            ),
            (
                "version-too-long",
                {"version": "v" * (MAX_MCP_CLIENT_INFO_FIELD_LENGTH + 1)},
                "clientInfo.version must not exceed 256 characters",
            ),
        )
        for case_name, client_info, expected_message in invalid_client_info:
            with self.subTest(invalid_client_info=case_name):
                response = validation_server.handle_json_rpc(
                    {
                        "jsonrpc": "2.0",
                        "id": case_name,
                        "method": "initialize",
                        "params": {"clientInfo": client_info},
                    },
                    session_id=f"invalid-{case_name}",
                )
                self.assertEqual(
                    response,
                    {
                        "jsonrpc": "2.0",
                        "id": case_name,
                        "error": {
                            "code": -32602,
                            "message": expected_message,
                        },
                    },
                )
                self.assertNotIn("-32000", json.dumps(response, sort_keys=True))
                self.assertNotIn(
                    f"invalid-{case_name}",
                    validation_server._contexts,
                )

        for invalid_capacity in (-1, 1.5, True):
            with self.subTest(invalid_capacity=invalid_capacity):
                with self.assertRaisesRegex(
                    ValueError,
                    "^max_session_contexts must be a non-negative integer$",
                ):
                    make_server(clock, max_session_contexts=invalid_capacity)
        for invalid_ttl in (-1, float("inf"), float("nan"), True):
            with self.subTest(invalid_ttl=invalid_ttl):
                with self.assertRaisesRegex(
                    ValueError,
                    "^session_ttl_seconds must be a non-negative finite number$",
                ):
                    make_server(clock, session_ttl_seconds=invalid_ttl)
        with self.assertRaisesRegex(ValueError, "^clock must be callable$"):
            make_server(None)
        with self.assertRaisesRegex(
            ValueError,
            "^clock must return a finite number$",
        ):
            make_server(lambda: float("nan"))

    def test_http_server_close_clears_all_session_contexts(self):
        from qcopilots_common.mcp_http import McpHttpServer

        class FakeHttpServer:
            server_address = ("127.0.0.1", 43210)

            def __init__(self):
                self.served = False
                self.closed = False

            def serve_forever(self):
                self.served = True

            def server_close(self):
                self.closed = True

        controller = McpHttpServer(
            name="qcopilots-http-context-cleanup-test",
            version="1.0.0",
            tools=[],
            host="127.0.0.1",
            port=0,
            max_session_contexts=2,
            session_ttl_seconds=60.0,
        )
        response = controller.rpc_server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "clientInfo": {
                        "name": "cleanup-client",
                        "version": "1.0",
                    }
                },
            },
            session_id="cleanup-session",
        )
        self.assertNotIn("error", response)
        self.assertEqual(
            tuple(controller.rpc_server._contexts),
            ("cleanup-session",),
        )

        httpd = FakeHttpServer()
        controller.create_http_server = lambda: httpd
        controller.serve_forever()

        self.assertTrue(httpd.served)
        self.assertTrue(httpd.closed)
        self.assertEqual(tuple(controller.rpc_server._contexts), ())

        response = controller.rpc_server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "initialize",
                "params": {"clientInfo": {"name": "again", "version": "2.0"}},
            },
            session_id="cleanup-after-error",
        )
        self.assertNotIn("error", response)
        failing_httpd = FakeHttpServer()

        def fail_serve_forever():
            failing_httpd.served = True
            raise RuntimeError("transport stopped")

        failing_httpd.serve_forever = fail_serve_forever
        controller.create_http_server = lambda: failing_httpd
        with self.assertRaisesRegex(RuntimeError, "^transport stopped$"):
            controller.serve_forever()
        self.assertTrue(failing_httpd.served)
        self.assertTrue(failing_httpd.closed)
        self.assertEqual(tuple(controller.rpc_server._contexts), ())

    def test_tools_call_uses_one_dynamic_tool_snapshot(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer, McpTool

        phase = {"after_init": False, "post_init_calls": 0}

        def echo(arguments):
            return arguments["text"]

        def changed(arguments):
            return arguments["text"]

        def tools_source():
            if not phase["after_init"]:
                return [
                    McpTool(
                        "echo",
                        "Echo text",
                        {"type": "object"},
                        echo,
                    )
                ]
            phase["post_init_calls"] += 1
            return [
                McpTool(
                    "echo" if phase["post_init_calls"] == 1 else "changed",
                    "Dynamic text",
                    {"type": "object"},
                    echo if phase["post_init_calls"] == 1 else changed,
                )
            ]

        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=tools_source,
        )
        phase["after_init"] = True

        call = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "tools/call",
                "params": {"name": "echo", "arguments": {"text": "hello"}},
            }
        )

        self.assertEqual(call["result"]["content"][0]["text"], "hello")
        self.assertEqual(phase["post_init_calls"], 1)

    def test_json_rpc_errors_are_structured(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer, McpPrompt, ToolError

        def needs_task(arguments):
            if not arguments.get("task"):
                raise ToolError("Task is required")
            return f"Task: {arguments['task']}"

        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[],
            prompts=[
                McpPrompt(
                    name="needs_task",
                    description="Needs a task",
                    arguments=[{"name": "task", "required": True}],
                    handler=needs_task,
                )
            ],
        )

        invalid = server.handle_json_rpc({"id": 1, "method": "tools/list"})
        self.assertEqual(invalid["jsonrpc"], "2.0")
        self.assertEqual(invalid["id"], 1)
        self.assertEqual(invalid["error"]["code"], -32600)

        unknown_method = server.handle_json_rpc(
            {"jsonrpc": "2.0", "id": 2, "method": "unknown", "params": {}}
        )
        self.assertEqual(unknown_method["error"]["code"], -32601)
        self.assertIn("unknown", unknown_method["error"]["message"])

        unknown_tool = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {"name": "missing", "arguments": {}},
            }
        )
        self.assertEqual(unknown_tool["error"]["code"], -32602)
        self.assertIn("missing", unknown_tool["error"]["message"])

        unknown_prompt = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 4,
                "method": "prompts/get",
                "params": {"name": "missing", "arguments": {}},
            }
        )
        self.assertEqual(unknown_prompt["error"]["code"], -32602)
        self.assertIn("Unknown prompt: missing", unknown_prompt["error"]["message"])

        prompt_tool_error = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 5,
                "method": "prompts/get",
                "params": {"name": "needs_task", "arguments": {}},
            }
        )
        self.assertEqual(prompt_tool_error["error"]["code"], -32602)
        self.assertIn("Task is required", prompt_tool_error["error"]["message"])

    def test_initialize_negotiates_2025_06_18_and_only_actual_capabilities(self):
        from qcopilots_common.mcp_http import (
            McpJsonRpcServer,
            McpPrompt,
            McpResource,
        )

        tools_only = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[],
        )
        tools_initialize = tools_only.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {"protocolVersion": "2024-11-05"},
            }
        )
        self.assertEqual(tools_initialize["result"]["protocolVersion"], "2025-06-18")
        self.assertEqual(tools_initialize["result"]["capabilities"], {"tools": {}})

        complete = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[],
            resources=[
                McpResource(
                    uri="resource://test/value",
                    name="value",
                    mime_type="text/plain",
                    handler=lambda: "value",
                )
            ],
            prompts=[
                McpPrompt(
                    name="prompt",
                    description="Prompt",
                    arguments=[],
                    handler=lambda arguments: arguments,
                )
            ],
        )
        complete_initialize = complete.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "initialize",
                "params": {"protocolVersion": "2025-06-18"},
            }
        )
        capabilities = complete_initialize["result"]["capabilities"]
        self.assertEqual(
            capabilities,
            {"tools": {}, "resources": {}, "prompts": {}},
        )
        for capability in capabilities.values():
            self.assertNotIn("listChanged", capability)
        self.assertNotIn("sse", capabilities)

    def test_tool_arguments_validate_supported_json_schema_subset_before_handler(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer, McpTool

        calls = []

        def handler(arguments):
            calls.append(arguments)
            return {"accepted": True}

        schema = {
            "type": "object",
            "required": [
                "object",
                "array",
                "string",
                "number",
                "integer",
                "boolean",
                "null",
                "enum",
                "const",
                "choice",
                "either",
                "labels",
                "overlap",
                "unique_nested",
                "bounded_object",
            ],
            "properties": {
                "object": {
                    "type": "object",
                    "required": ["value"],
                    "properties": {"value": {"type": "string"}},
                    "additionalProperties": False,
                    "minProperties": 1,
                    "maxProperties": 1,
                },
                "array": {
                    "type": "array",
                    "items": {
                        "oneOf": [
                            {"type": "integer"},
                            {"type": "boolean"},
                        ]
                    },
                    "minItems": 1,
                    "maxItems": 3,
                    "uniqueItems": True,
                },
                "string": {
                    "type": "string",
                    "minLength": 2,
                    "maxLength": 4,
                    "pattern": "^[a-z]+$",
                },
                "number": {"type": "number", "minimum": 0, "maximum": 10},
                "integer": {"type": "integer", "minimum": 1, "maximum": 3},
                "boolean": {"type": "boolean"},
                "null": {"type": "null"},
                "enum": {"enum": ["x", 2, True]},
                "const": {"const": {"key": "value"}},
                "choice": {
                    "oneOf": [
                        {"type": "string", "const": "s"},
                        {"type": "integer", "const": 1},
                    ]
                },
                "either": {
                    "anyOf": [
                        {"type": "null"},
                        {"type": "array", "maxItems": 0},
                    ]
                },
                "labels": {
                    "type": "object",
                    "additionalProperties": {"type": "string"},
                },
                "overlap": {
                    "oneOf": [
                        {"type": "number"},
                        {"type": "integer"},
                    ]
                },
                "unique_nested": {
                    "type": "array",
                    "uniqueItems": True,
                },
                "bounded_object": {
                    "type": "object",
                    "properties": {
                        "first": {"type": "integer"},
                        "second": {"type": "integer"},
                    },
                    "additionalProperties": False,
                    "minProperties": 1,
                    "maxProperties": 1,
                },
            },
            "additionalProperties": False,
            "minProperties": 15,
            "maxProperties": 15,
        }
        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[McpTool("validate", "Validate arguments", schema, handler)],
        )
        valid = {
            "object": {"value": "ok"},
            "array": [1, 2],
            "string": "abc",
            "number": 2.5,
            "integer": 2,
            "boolean": True,
            "null": None,
            "enum": "x",
            "const": {"key": "value"},
            "choice": "s",
            "either": [],
            "labels": {"owner": "tests"},
            "overlap": 1.5,
            "unique_nested": [{"items": [1]}, {"items": [2]}],
            "bounded_object": {"first": 1},
        }

        valid_cases = (
            valid,
            {**valid, "enum": True, "choice": 1, "either": None},
            {**valid, "number": 0, "integer": 3, "array": [True, 1]},
        )
        for request_id, arguments in enumerate(valid_cases, start=1):
            with self.subTest(valid=request_id):
                response = server.handle_json_rpc(
                    {
                        "jsonrpc": "2.0",
                        "id": request_id,
                        "method": "tools/call",
                        "params": {"name": "validate", "arguments": arguments},
                    }
                )
                self.assertNotIn("error", response)
                self.assertEqual(
                    response["result"]["structuredContent"], {"accepted": True}
                )

        invalid_cases = {}

        def invalid(name, field, value):
            arguments = copy.deepcopy(valid)
            if field is None:
                value(arguments)
            else:
                arguments[field] = value
            invalid_cases[name] = arguments

        invalid("root-type", None, lambda arguments: arguments.clear())
        invalid("required", None, lambda arguments: arguments.pop("object"))
        invalid("object-type", "object", [])
        invalid("object-required-min-properties", "object", {})
        invalid(
            "object-additional-max-properties",
            "object",
            {"value": "ok", "extra": "x"},
        )
        invalid("object-value-path", "object", {"value": 1})
        invalid("additional-properties-schema", "labels", {"owner": 1})
        invalid("array-type", "array", {})
        invalid("array-items", "array", [1, "2"])
        invalid("array-min-items", "array", [])
        invalid("array-max-items", "array", [1, 2, 3, 4])
        invalid("array-unique-items", "array", [1, 1])
        invalid(
            "array-unique-nested-items",
            "unique_nested",
            [{"items": [1, {"key": "value"}]}, {"items": [1, {"key": "value"}]}],
        )
        invalid("string-type", "string", 1)
        invalid("string-min-length", "string", "a")
        invalid("string-max-length", "string", "abcde")
        invalid("string-pattern", "string", "A1")
        invalid("number-type", "number", "2")
        invalid("number-minimum", "number", -1)
        invalid("number-maximum", "number", 11)
        invalid("number-bool", "number", True)
        invalid("integer-type", "integer", 1.5)
        invalid("integer-minimum", "integer", 0)
        invalid("integer-maximum", "integer", 4)
        invalid("integer-bool", "integer", False)
        invalid("boolean-type", "boolean", 1)
        invalid("null-type", "null", False)
        invalid("enum", "enum", "missing")
        invalid("const", "const", {"key": "different"})
        invalid("one-of", "choice", "missing")
        invalid("one-of-overlap", "overlap", 1)
        invalid("any-of", "either", [1])
        invalid("bounded-object-min-properties", "bounded_object", {})
        invalid(
            "bounded-object-max-properties",
            "bounded_object",
            {"first": 1, "second": 2},
        )
        invalid(
            "root-additional-property",
            None,
            lambda arguments: arguments.update(extra=True),
        )

        calls_before_invalid = len(calls)
        expected_paths = {
            "object-value-path": "$.object.value",
            "additional-properties-schema": "$.labels.owner",
            "array-items": "$.array[1]",
            "array-unique-nested-items": "$.unique_nested",
            "one-of-overlap": "$.overlap",
            "bounded-object-min-properties": "$.bounded_object",
            "bounded-object-max-properties": "$.bounded_object",
        }
        for offset, (name, arguments) in enumerate(invalid_cases.items(), start=100):
            with self.subTest(invalid=name):
                response = server.handle_json_rpc(
                    {
                        "jsonrpc": "2.0",
                        "id": offset,
                        "method": "tools/call",
                        "params": {"name": "validate", "arguments": arguments},
                    }
                )
                self.assertEqual(response["error"]["code"], -32602)
                self.assertIn("Invalid tool arguments", response["error"]["message"])
                if name in expected_paths:
                    self.assertIn(expected_paths[name], response["error"]["message"])
        self.assertEqual(len(calls), calls_before_invalid)

        non_object = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 999,
                "method": "tools/call",
                "params": {"name": "validate", "arguments": []},
            }
        )
        self.assertEqual(non_object["error"]["code"], -32602)
        self.assertEqual(len(calls), calls_before_invalid)

    def test_http_invalid_tool_arguments_return_jsonrpc_error_before_handler(self):
        from qcopilots_common.mcp_http import McpHttpServer, McpTool

        calls = []

        def handler(arguments):
            calls.append(arguments)
            return {"unexpected": True}

        server = McpHttpServer(
            name="qcopilots-http-schema-test",
            version="1.0.0",
            tools=[
                McpTool(
                    "validate-http",
                    "Validate HTTP arguments",
                    {
                        "type": "object",
                        "properties": {
                            "outer": {
                                "type": "object",
                                "properties": {"value": {"type": "integer"}},
                                "required": ["value"],
                                "additionalProperties": False,
                            }
                        },
                        "required": ["outer"],
                        "additionalProperties": False,
                    },
                    handler,
                )
            ],
            port=0,
            auth_token=self.HTTP_AUTH_TOKEN,
        )
        httpd = server.create_http_server()
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        try:
            payload = json.dumps(
                {
                    "jsonrpc": "2.0",
                    "id": "invalid-http-arguments",
                    "method": "tools/call",
                    "params": {
                        "name": "validate-http",
                        "arguments": {"outer": {"value": "not-an-integer"}},
                    },
                }
            ).encode("utf-8")
            request = Request(
                f"http://127.0.0.1:{port}/mcp",
                data=payload,
                headers=self._http_headers({"Content-Type": "application/json"}),
                method="POST",
            )
            with urlopen(request, timeout=5) as response:
                self.assertEqual(response.status, 200)
                body = json.loads(response.read().decode("utf-8"))
            self.assertEqual(body["jsonrpc"], "2.0")
            self.assertEqual(body["id"], "invalid-http-arguments")
            self.assertEqual(body["error"]["code"], -32602)
            self.assertIn("Invalid tool arguments", body["error"]["message"])
            self.assertIn("$.outer.value", body["error"]["message"])
            self.assertEqual(calls, [])
        finally:
            self._stop_http_server(httpd, thread)

    def test_http_json_rpc_batch_limits_are_atomic_and_deterministic(self):
        from qcopilots_common.mcp_http import (
            MAX_JSON_RPC_BATCH_ITEMS,
            McpHttpServer,
            McpTool,
        )

        calls = []

        def record(arguments):
            calls.append(arguments["value"])
            return {"value": arguments["value"]}

        controller = McpHttpServer(
            name="qcopilots-http-batch-test",
            version="1.0.0",
            tools=[
                McpTool(
                    name="record",
                    description="Record a batch value",
                    input_schema={
                        "type": "object",
                        "properties": {"value": {"type": "integer"}},
                        "required": ["value"],
                        "additionalProperties": False,
                    },
                    handler=record,
                )
            ],
            host="127.0.0.1",
            port=0,
            auth_token=self.HTTP_AUTH_TOKEN,
        )
        httpd = controller.create_http_server()
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()

        def post_batch(batch):
            payload = json.dumps(batch).encode("utf-8")
            request = Request(
                f"http://127.0.0.1:{port}/mcp",
                data=payload,
                headers=self._http_headers({"Content-Type": "application/json"}),
                method="POST",
            )
            with urlopen(request, timeout=5) as response:
                self.assertEqual(response.status, 200)
                return json.loads(response.read().decode("utf-8"))

        expected_batch_error = {
            "jsonrpc": "2.0",
            "id": None,
            "error": {
                "code": -32600,
                "message": (
                    "Invalid JSON-RPC batch: expected "
                    f"1-{MAX_JSON_RPC_BATCH_ITEMS} requests"
                ),
            },
        }

        try:
            self.assertEqual(post_batch([]), expected_batch_error)
            self.assertEqual(calls, [])

            over_limit = [
                {
                    "jsonrpc": "2.0",
                    "id": index,
                    "method": "tools/call",
                    "params": {
                        "name": "record",
                        "arguments": {"value": index},
                    },
                }
                for index in range(MAX_JSON_RPC_BATCH_ITEMS + 1)
            ]
            self.assertEqual(post_batch(over_limit), expected_batch_error)
            self.assertEqual(calls, [])

            at_limit = over_limit[:MAX_JSON_RPC_BATCH_ITEMS]
            responses = post_batch(at_limit)
            self.assertIsInstance(responses, list)
            self.assertEqual(len(responses), MAX_JSON_RPC_BATCH_ITEMS)
            self.assertEqual(
                [response["id"] for response in responses],
                list(range(MAX_JSON_RPC_BATCH_ITEMS)),
            )
            self.assertEqual(
                [
                    response["result"]["structuredContent"]["value"]
                    for response in responses
                ],
                list(range(MAX_JSON_RPC_BATCH_ITEMS)),
            )
            self.assertEqual(calls, list(range(MAX_JSON_RPC_BATCH_ITEMS)))
        finally:
            self._stop_http_server(httpd, thread)

    def test_tool_result_and_error_categories_follow_mcp_contract(self):
        from qcopilots_common.mcp_http import McpJsonRpcServer, McpTool, ToolError

        def tool_error(arguments):
            del arguments
            raise ToolError("expected tool failure")

        def internal_error(arguments):
            del arguments
            raise RuntimeError("unexpected handler failure")

        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[
                McpTool(
                    "plain-dict",
                    "Return a plain dictionary",
                    {
                        "type": "object",
                        "properties": {"value": {"type": "string"}},
                        "required": ["value"],
                        "additionalProperties": False,
                    },
                    lambda arguments: {
                        "value": arguments["value"],
                        "nested": {"ok": True},
                    },
                ),
                McpTool("tool-error", "Tool error", {"type": "object"}, tool_error),
                McpTool(
                    "internal-error",
                    "Internal error",
                    {"type": "object"},
                    internal_error,
                ),
            ],
        )

        plain = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "tools/call",
                "params": {
                    "name": "plain-dict",
                    "arguments": {"value": "中文"},
                },
            }
        )
        self.assertEqual(
            plain["result"]["structuredContent"],
            {"value": "中文", "nested": {"ok": True}},
        )
        self.assertEqual(
            json.loads(plain["result"]["content"][0]["text"]),
            plain["result"]["structuredContent"],
        )

        invalid = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "tools/call",
                "params": {"name": "plain-dict", "arguments": {}},
            }
        )
        self.assertEqual(invalid["error"]["code"], -32602)

        unknown = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {"name": "missing", "arguments": {}},
            }
        )
        self.assertEqual(unknown["error"]["code"], -32602)
        self.assertIn("Unknown tool", unknown["error"]["message"])

        expected = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 4,
                "method": "tools/call",
                "params": {"name": "tool-error", "arguments": {}},
            }
        )
        self.assertTrue(expected["result"]["isError"])
        self.assertIn(
            "expected tool failure", expected["result"]["content"][0]["text"]
        )

        unexpected = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 5,
                "method": "tools/call",
                "params": {"name": "internal-error", "arguments": {}},
            }
        )
        self.assertEqual(unexpected["error"]["code"], -32000)
        self.assertIn("unexpected handler failure", unexpected["error"]["message"])

    def test_initialize_exposes_service_metadata_icons(self):
        from qcopilots_common.constants import SERVICE_ICON_ENV
        from qcopilots_common.mcp_http import McpJsonRpcServer, _server_icons_from_env

        icon = {
            "src": "data:image/svg+xml;base64,PHN2Zy8+",
            "mimeType": "image/svg+xml",
            "sizes": ["any"],
        }
        old_icons = os.environ.get(SERVICE_ICON_ENV)
        try:
            os.environ[SERVICE_ICON_ENV] = json.dumps([icon])
            self.assertEqual(_server_icons_from_env(), [icon])
        finally:
            if old_icons is None:
                os.environ.pop(SERVICE_ICON_ENV, None)
            else:
                os.environ[SERVICE_ICON_ENV] = old_icons

        server = McpJsonRpcServer(
            server_name="qcopilots-test",
            server_version="1.0.0",
            tools=[],
            server_title="QCopilots MCP Server Test",
            server_description="测试服务描述",
            server_icons=[icon],
        )

        initialize = server.handle_json_rpc(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2025-06-18",
                    "capabilities": {},
                    "clientInfo": {"name": "unit-test", "version": "1.0"},
                },
            }
        )

        self.assertEqual(
            initialize["result"]["serverInfo"],
            {
                "name": "qcopilots-test",
                "version": "1.0.0",
                "title": "QCopilots MCP Server Test",
                "description": "测试服务描述",
                "icons": [icon],
            },
        )

    def test_configure_logger_uses_writable_stderr_without_log_file(self):
        from qcopilots_common.logging import configure_logger

        logger_name = f"qcopilots-stderr-log-test-{os.getpid()}"
        logger = logging.getLogger(logger_name)
        for handler in list(logger.handlers):
            logger.removeHandler(handler)
            handler.close()
        logger.propagate = False

        stderr = StringIO()
        try:
            with redirect_stderr(stderr):
                configured_logger = configure_logger(logger_name)
                configured_logger.info("stream handler is available")
                for handler in configured_logger.handlers:
                    handler.flush()

            self.assertEqual(len(configured_logger.handlers), 1)
            self.assertIs(type(configured_logger.handlers[0]), logging.StreamHandler)
            self.assertIn("stream handler is available", stderr.getvalue())
        finally:
            for handler in list(logger.handlers):
                logger.removeHandler(handler)
                handler.close()

    def test_configure_logger_skips_unavailable_stderr(self):
        from qcopilots_common.logging import configure_logger

        unavailable_streams = (
            ("missing", None),
            ("not writable", types.SimpleNamespace(write=None)),
        )
        for description, stderr in unavailable_streams:
            with self.subTest(stderr=description):
                logger_name = (
                    f"qcopilots-unavailable-stderr-test-{description}-{os.getpid()}"
                )
                logger = logging.getLogger(logger_name)
                for handler in list(logger.handlers):
                    logger.removeHandler(handler)
                    handler.close()
                logger.propagate = False

                try:
                    with redirect_stderr(stderr):
                        configured_logger = configure_logger(logger_name)
                        configured_logger.info("discarded without stderr")
                    self.assertEqual(len(configured_logger.handlers), 1)
                    self.assertIs(
                        type(configured_logger.handlers[0]),
                        logging.NullHandler,
                    )
                finally:
                    for handler in list(logger.handlers):
                        logger.removeHandler(handler)
                        handler.close()

    def test_configure_logger_log_file_replaces_managed_stream_handler(self):
        from logging.handlers import RotatingFileHandler

        from qcopilots_common.logging import configure_logger

        logger_name = f"qcopilots-single-log-output-test-{os.getpid()}"
        logger = logging.getLogger(logger_name)
        for handler in list(logger.handlers):
            logger.removeHandler(handler)
            handler.close()
        logger.propagate = False

        stderr = StringIO()
        temp_dir = tempfile.TemporaryDirectory()
        try:
            log_file = Path(temp_dir.name) / "service.log"
            with redirect_stderr(stderr):
                configured_logger = configure_logger(logger_name)
                configured_logger = configure_logger(logger_name, log_file)
                configured_logger.info("written once to the service log")
                for handler in configured_logger.handlers:
                    handler.flush()

            self.assertEqual(len(configured_logger.handlers), 1)
            self.assertIsInstance(
                configured_logger.handlers[0],
                RotatingFileHandler,
            )
            self.assertNotIn("written once to the service log", stderr.getvalue())
            self.assertEqual(
                log_file.read_text(encoding="utf-8").count(
                    "written once to the service log"
                ),
                1,
            )
        finally:
            for handler in list(logger.handlers):
                logger.removeHandler(handler)
                handler.close()
            temp_dir.cleanup()

    def test_configure_logger_replaces_previous_log_file_handler(self):
        from qcopilots_common.logging import configure_logger

        logger_name = f"qcopilots-reconfigure-log-test-{os.getpid()}"
        logger = logging.getLogger(logger_name)
        for handler in list(logger.handlers):
            logger.removeHandler(handler)
            handler.close()
        logger.propagate = False

        temp_dir = tempfile.TemporaryDirectory()
        try:
            first_log_file = Path(temp_dir.name) / "first.log"
            second_log_file = Path(temp_dir.name) / "second.log"

            configured_logger = configure_logger(logger_name, first_log_file)
            configured_logger.info("first log file only")
            configured_logger = configure_logger(logger_name, second_log_file)
            configured_logger.info("second log file only")
            for handler in configured_logger.handlers:
                handler.flush()

            self.assertIn(
                "first log file only",
                first_log_file.read_text(encoding="utf-8"),
            )
            self.assertNotIn(
                "second log file only",
                first_log_file.read_text(encoding="utf-8"),
            )
            self.assertIn(
                "second log file only",
                second_log_file.read_text(encoding="utf-8"),
            )
            self.assertEqual(
                [
                    Path(handler.baseFilename).resolve()
                    for handler in configured_logger.handlers
                    if getattr(handler, "baseFilename", None)
                ],
                [second_log_file.resolve()],
            )
        finally:
            for handler in list(logger.handlers):
                logger.removeHandler(handler)
                handler.close()
            temp_dir.cleanup()

    def test_run_mcp_server_applies_log_file_argument(self):
        import qcopilots_common.mcp_http as mcp_http
        from qcopilots_common.constants import MCP_AUTH_TOKEN_ENV

        class FakeMcpHttpServer:
            instances = []

            def __init__(self, **kwargs):
                self.kwargs = kwargs
                type(self).instances.append(self)

            def serve_forever(self):
                self.kwargs["logger"].info("log file argument applied")

        logger_name = f"qcopilots-log-file-test-{os.getpid()}"
        logger = logging.getLogger(logger_name)
        for handler in list(logger.handlers):
            logger.removeHandler(handler)
            handler.close()
        logger.propagate = False

        old_argv = sys.argv[:]
        original_server = mcp_http.McpHttpServer
        temp_dir = tempfile.TemporaryDirectory()
        old_auth_token = os.environ.get(MCP_AUTH_TOKEN_ENV)
        try:
            os.environ[MCP_AUTH_TOKEN_ENV] = "unit-test-service-token"
            log_file = Path(temp_dir.name) / "service.log"
            sys.argv = ["server.py", "--port", "0", "--log-file", str(log_file)]
            mcp_http.McpHttpServer = FakeMcpHttpServer

            mcp_http.run_mcp_server(
                name="qcopilots-log-test",
                version="1.0.0",
                tools=[],
                default_port=48211,
                description="Log file test",
                logger=logger,
            )

            self.assertEqual(FakeMcpHttpServer.instances[0].kwargs["port"], 0)
            self.assertEqual(
                FakeMcpHttpServer.instances[0].kwargs["auth_token"],
                "unit-test-service-token",
            )
            self.assertEqual(
                FakeMcpHttpServer.instances[0].kwargs["logger"].name,
                logger_name,
            )
            for handler in FakeMcpHttpServer.instances[0].kwargs["logger"].handlers:
                handler.flush()
            self.assertTrue(log_file.is_file())
            self.assertIn(
                "log file argument applied",
                log_file.read_text(encoding="utf-8"),
            )
        finally:
            sys.argv = old_argv
            mcp_http.McpHttpServer = original_server
            if old_auth_token is None:
                os.environ.pop(MCP_AUTH_TOKEN_ENV, None)
            else:
                os.environ[MCP_AUTH_TOKEN_ENV] = old_auth_token
            for handler in list(logger.handlers):
                logger.removeHandler(handler)
                handler.close()
            temp_dir.cleanup()

    def test_run_mcp_server_requires_auth_token_environment(self):
        import qcopilots_common.mcp_http as mcp_http
        from qcopilots_common.constants import MCP_AUTH_TOKEN_ENV

        old_argv = sys.argv[:]
        old_auth_token = os.environ.get(MCP_AUTH_TOKEN_ENV)
        try:
            sys.argv = ["server.py", "--port", "0"]
            for value in (None, "   "):
                if value is None:
                    os.environ.pop(MCP_AUTH_TOKEN_ENV, None)
                else:
                    os.environ[MCP_AUTH_TOKEN_ENV] = value
                with self.subTest(value=value):
                    with self.assertRaisesRegex(RuntimeError, MCP_AUTH_TOKEN_ENV):
                        mcp_http.run_mcp_server(
                            name="qcopilots-auth-required-test",
                            version="1.0.0",
                            tools=[],
                            default_port=48211,
                            description="Auth required test",
                            logger=logging.getLogger("qcopilots-auth-required-test"),
                        )
        finally:
            sys.argv = old_argv
            if old_auth_token is None:
                os.environ.pop(MCP_AUTH_TOKEN_ENV, None)
            else:
                os.environ[MCP_AUTH_TOKEN_ENV] = old_auth_token

    def test_http_transport_refuses_to_start_without_auth_token(self):
        from qcopilots_common.mcp_http import McpHttpServer

        for auth_token in (None, "", "   "):
            server = McpHttpServer(
                name="qcopilots-http-test",
                version="1.0.0",
                tools=[],
                port=0,
                auth_token=auth_token,
            )
            with self.subTest(auth_token=auth_token):
                with self.assertRaisesRegex(ValueError, "auth token"):
                    server.create_http_server()

    def test_http_transport_requires_bearer_and_strict_host(self):
        from qcopilots_common.mcp_http import McpHttpServer

        auth_token = "unit-test-service-secret"
        log_stream = StringIO()
        logger = logging.getLogger(f"qcopilots-http-auth-test-{os.getpid()}")
        logger.handlers.clear()
        logger.propagate = False
        logger.setLevel(logging.INFO)
        handler = logging.StreamHandler(log_stream)
        logger.addHandler(handler)
        server = McpHttpServer(
            name="qcopilots-http-auth-test",
            version="1.0.0",
            tools=[],
            port=0,
            auth_token=auth_token,
            logger=logger,
        )
        httpd = server.create_http_server()
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        payload = json.dumps(
            {"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}}
        ).encode("utf-8")
        try:
            for headers in ({}, {"Authorization": "Bearer wrong"}):
                request = Request(
                    f"http://127.0.0.1:{port}/health",
                    headers=headers,
                )
                with self.subTest(endpoint="health", headers=headers):
                    with self.assertRaises(HTTPError) as error_context:
                        urlopen(request, timeout=5)
                    self.assertEqual(error_context.exception.code, 401)
                    self.assertNotIn(
                        auth_token,
                        error_context.exception.read().decode("utf-8"),
                    )

            with urlopen(
                Request(
                    f"http://127.0.0.1:{port}/health",
                    headers={"Authorization": f"Bearer {auth_token}"},
                ),
                timeout=5,
            ) as response:
                self.assertEqual(response.status, 200)

            for authorization in (None, "Bearer wrong"):
                headers = {"Content-Type": "application/json"}
                if authorization is not None:
                    headers["Authorization"] = authorization
                request = Request(
                    f"http://127.0.0.1:{port}/mcp",
                    data=payload,
                    headers=headers,
                    method="POST",
                )
                with self.subTest(endpoint="mcp", authorization=authorization):
                    with self.assertRaises(HTTPError) as error_context:
                        urlopen(request, timeout=5)
                    self.assertEqual(error_context.exception.code, 401)

            authorized_headers = {
                "Authorization": f"Bearer {auth_token}",
                "Content-Type": "application/json",
            }
            with urlopen(
                Request(
                    f"http://127.0.0.1:{port}/mcp",
                    data=payload,
                    headers=authorized_headers,
                    method="POST",
                ),
                timeout=5,
            ) as response:
                self.assertEqual(
                    json.loads(response.read().decode("utf-8"))["result"],
                    {"tools": []},
                )

            with self.assertRaises(HTTPError) as origin_context:
                urlopen(
                    Request(
                        f"http://127.0.0.1:{port}/mcp",
                        data=payload,
                        headers={
                            **authorized_headers,
                            "Origin": "http://127.0.0.1:8282",
                        },
                        method="POST",
                    ),
                    timeout=5,
                )
            self.assertEqual(origin_context.exception.code, 403)

            with self.assertRaises(HTTPError) as options_context:
                urlopen(
                    Request(
                        f"http://127.0.0.1:{port}/mcp",
                        headers={"Authorization": f"Bearer {auth_token}"},
                        method="OPTIONS",
                    ),
                    timeout=5,
                )
            self.assertEqual(options_context.exception.code, 405)

            raw_request = (
                "GET /health HTTP/1.1\r\n"
                f"Host: localhost:{port}\r\n"
                f"Authorization: Bearer {auth_token}\r\n"
                "Connection: close\r\n"
                "\r\n"
            ).encode("ascii")
            with socket.create_connection(("127.0.0.1", port), timeout=2) as connection:
                connection.sendall(raw_request)
                raw_response = connection.recv(4096).decode("iso-8859-1")
            self.assertIn(" 403 ", raw_response.splitlines()[0])

            handler.flush()
            self.assertNotIn(auth_token, log_stream.getvalue())
        finally:
            self._stop_http_server(httpd, thread)
            logger.removeHandler(handler)
            handler.close()

    def test_http_health_uses_cached_tool_names(self):
        from qcopilots_common.mcp_http import McpHttpServer, McpTool

        phase = {"after_init": False, "post_init_calls": 0}

        def tools_source():
            if phase["after_init"]:
                phase["post_init_calls"] += 1
            return [
                McpTool(
                    "echo",
                    "Echo text",
                    {"type": "object"},
                    lambda arguments: arguments,
                )
            ]

        server = McpHttpServer(
            name="qcopilots-http-test",
            version="1.0.0",
            tools=tools_source,
            port=0,
            auth_token=self.HTTP_AUTH_TOKEN,
        )
        phase["after_init"] = True
        httpd = server.create_http_server()
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        try:
            with urlopen(
                Request(
                    f"http://127.0.0.1:{port}/health",
                    headers=self._http_headers(),
                ),
                timeout=5,
            ) as response:
                self.assertEqual(response.status, 200)
                body = json.loads(response.read().decode("utf-8"))
                self.assertEqual(body["tools"], ["echo"])
            self.assertEqual(phase["post_init_calls"], 0)
        finally:
            self._stop_http_server(httpd, thread)

    def test_http_initialize_notification_and_tools_list_sequence(self):
        from qcopilots_common.mcp_http import McpHttpServer

        server = McpHttpServer(
            name="qcopilots-http-test",
            version="1.0.0",
            tools=[],
            port=0,
            auth_token=self.HTTP_AUTH_TOKEN,
        )
        httpd = server.create_http_server()
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()

        def post(payload, session_id=None):
            headers = {
                "Accept": "application/json, text/event-stream",
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self.HTTP_AUTH_TOKEN}",
            }
            if session_id:
                headers.update(
                    {
                        "MCP-Protocol-Version": "2025-06-18",
                        "MCP-Session-Id": session_id,
                    }
                )
            return urlopen(
                Request(
                    f"http://127.0.0.1:{port}/mcp",
                    data=json.dumps(payload).encode("utf-8"),
                    headers=headers,
                    method="POST",
                ),
                timeout=5,
            )

        initialized_notification = {
            "jsonrpc": "2.0",
            "method": "notifications/initialized",
        }
        try:
            with post(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-06-18",
                        "capabilities": {},
                        "clientInfo": {
                            "name": "http-sequence-test",
                            "version": "1.0",
                        },
                    },
                }
            ) as response:
                self.assertEqual(response.status, 200)
                session_id = response.headers["mcp-session-id"]
                self.assertTrue(session_id)
                self.assertEqual(
                    json.loads(response.read().decode("utf-8"))["result"][
                        "protocolVersion"
                    ],
                    "2025-06-18",
                )

            with post(initialized_notification, session_id) as response:
                self.assertEqual(response.status, 202)
                self.assertEqual(response.headers["content-length"], "0")
                self.assertEqual(response.headers["mcp-session-id"], session_id)
                self.assertEqual(response.read(), b"")

            stream_request = Request(
                f"http://127.0.0.1:{port}/mcp",
                headers={
                    "Accept": "text/event-stream",
                    "MCP-Protocol-Version": "2025-06-18",
                    "MCP-Session-Id": session_id,
                    "Authorization": f"Bearer {self.HTTP_AUTH_TOKEN}",
                },
                method="GET",
            )
            with self.assertRaises(HTTPError) as raised:
                urlopen(stream_request, timeout=5)
            stream_response = raised.exception
            self.assertEqual(stream_response.code, 405)
            self.assertEqual(stream_response.headers["content-length"], "0")
            self.assertIn("POST", stream_response.headers["Allow"])
            self.assertIn("DELETE", stream_response.headers["Allow"])
            self.assertNotIn("OPTIONS", stream_response.headers["Allow"])
            self.assertEqual(stream_response.read(), b"")

            with post(
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "method": "tools/list",
                    "params": {},
                },
                session_id,
            ) as response:
                self.assertEqual(response.status, 200)
                self.assertEqual(
                    json.loads(response.read().decode("utf-8"))["result"],
                    {"tools": []},
                )

            with post([initialized_notification], session_id) as response:
                self.assertEqual(response.status, 202)
                self.assertEqual(response.headers["content-length"], "0")
                self.assertEqual(response.headers["mcp-session-id"], session_id)
                self.assertEqual(response.read(), b"")
        finally:
            self._stop_http_server(httpd, thread)

    def test_http_initialize_generates_session_header_and_delete_clears_context(self):
        from qcopilots_common.mcp_http import McpHttpServer, McpTool

        def client_name(arguments, context):
            del arguments
            return context.client_name

        server = McpHttpServer(
            name="qcopilots-http-test",
            version="1.0.0",
            tools=[
                McpTool(
                    "client_name",
                    "Return client name",
                    {"type": "object"},
                    client_name,
                )
            ],
            port=0,
            auth_token=self.HTTP_AUTH_TOKEN,
        )
        httpd = server.create_http_server()
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        try:
            initialize_payload = json.dumps(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {"clientInfo": {"name": "http-client", "version": "1.0"}},
                }
            ).encode("utf-8")
            initialize_request = Request(
                f"http://127.0.0.1:{port}/mcp",
                data=initialize_payload,
                headers=self._http_headers({"Content-Type": "application/json"}),
                method="POST",
            )
            with urlopen(initialize_request, timeout=5) as response:
                self.assertEqual(response.status, 200)
                session_id = response.headers["mcp-session-id"]
                self.assertTrue(session_id)

            call_payload = json.dumps(
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "method": "tools/call",
                    "params": {"name": "client_name", "arguments": {}},
                }
            ).encode("utf-8")
            call_request = Request(
                f"http://127.0.0.1:{port}/mcp",
                data=call_payload,
                headers={
                    "Content-Type": "application/json",
                    "mcp-session-id": session_id,
                    "Authorization": f"Bearer {self.HTTP_AUTH_TOKEN}",
                },
                method="POST",
            )
            with urlopen(call_request, timeout=5) as response:
                self.assertEqual(
                    json.loads(response.read().decode("utf-8"))["result"]["content"][0][
                        "text"
                    ],
                    "http-client",
                )

            delete_request = Request(
                f"http://127.0.0.1:{port}/mcp",
                headers=self._http_headers({"mcp-session-id": session_id}),
                method="DELETE",
            )
            with urlopen(delete_request, timeout=5) as response:
                self.assertEqual(response.status, 204)

            with urlopen(call_request, timeout=5) as response:
                self.assertEqual(
                    json.loads(response.read().decode("utf-8"))["result"]["content"][0][
                        "text"
                    ],
                    "",
                )
        finally:
            self._stop_http_server(httpd, thread)

    def test_empty_cors_environment_still_rejects_origin(self):
        from qcopilots_common.constants import CORS_ORIGINS_ENV
        from qcopilots_common.mcp_http import McpHttpServer

        old_origins = os.environ.get(CORS_ORIGINS_ENV)
        try:
            os.environ[CORS_ORIGINS_ENV] = os.pathsep
            server = McpHttpServer(
                name="qcopilots-http-test",
                version="1.0.0",
                tools=[],
                port=0,
                auth_token=self.HTTP_AUTH_TOKEN,
            )
            httpd = server.create_http_server()
            port = httpd.server_address[1]
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()
            try:
                request = Request(
                    f"http://127.0.0.1:{port}/mcp",
                    headers={
                        "Origin": "http://127.0.0.1:8282",
                        "Access-Control-Request-Method": "POST",
                    },
                    method="OPTIONS",
                )
                with self.assertRaises(HTTPError) as error_context:
                    urlopen(request, timeout=5)
                self.assertEqual(error_context.exception.code, 403)
            finally:
                self._stop_http_server(httpd, thread)
        finally:
            if old_origins is None:
                os.environ.pop(CORS_ORIGINS_ENV, None)
            else:
                os.environ[CORS_ORIGINS_ENV] = old_origins

    def test_service_bind_host_only_accepts_literal_loopback(self):
        from qcopilots_common.constants import DEFAULT_HOST
        from qcopilots_common.mcp_http import McpHttpServer, parse_server_args

        loopback_server = McpHttpServer(
            name="qcopilots-http-loopback-test",
            version="1.0.0",
            tools=[],
            host=DEFAULT_HOST,
        )
        self.assertEqual(loopback_server.host, DEFAULT_HOST)
        for host in ("localhost", "0.0.0.0", "192.168.1.10", "::1", "8.8.8.8"):
            with self.subTest(host=host):
                with self.assertRaises(ValueError):
                    McpHttpServer(
                        name="qcopilots-http-rejected-host-test",
                        version="1.0.0",
                        tools=[],
                        host=host,
                    )

        old_argv = sys.argv[:]
        try:
            for host in ("localhost", "0.0.0.0", "192.168.1.10", "::1", "8.8.8.8"):
                sys.argv = ["server.py", "--host", host]
                with self.subTest(argument_host=host):
                    with redirect_stderr(StringIO()):
                        with self.assertRaises(SystemExit):
                            parse_server_args(48211, "QCopilots test server")

            sys.argv = ["server.py", "--host", DEFAULT_HOST]
            self.assertEqual(
                parse_server_args(48211, "QCopilots test server").host,
                DEFAULT_HOST,
            )
        finally:
            sys.argv = old_argv

    def test_http_transport_oversized_error_body_does_not_wait_for_body(self):
        from qcopilots_common.constants import MAX_HTTP_REQUEST_BODY_BYTES
        from qcopilots_common.mcp_http import McpHttpServer

        server = McpHttpServer(
            name="qcopilots-http-test",
            version="1.0.0",
            tools=[],
            port=0,
            auth_token=self.HTTP_AUTH_TOKEN,
        )
        httpd = server.create_http_server()
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        try:
            oversized_length = MAX_HTTP_REQUEST_BODY_BYTES + 1
            wrong_path_response = self._raw_post_response(
                port,
                "/missing",
                "text/plain",
                oversized_length,
            )
            self.assertIn(" 404 ", wrong_path_response.splitlines()[0])
            self.assertIn("Connection: close", wrong_path_response)

            valid_path_response = self._raw_post_response(
                port,
                "/mcp",
                "application/json",
                oversized_length,
                {"Authorization": f"Bearer {self.HTTP_AUTH_TOKEN}"},
            )
            self.assertIn(" 413 ", valid_path_response.splitlines()[0])
            self.assertIn("Connection: close", valid_path_response)
        finally:
            self._stop_http_server(httpd, thread)

    def test_http_transport_rejects_requests_without_bearer(self):
        from qcopilots_common.mcp_http import McpHttpServer

        server = McpHttpServer(
            name="qcopilots-http-test",
            version="1.0.0",
            tools=[],
            port=0,
            auth_token=self.HTTP_AUTH_TOKEN,
        )
        httpd = server.create_http_server()
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        try:
            payload = json.dumps(
                {"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}}
            ).encode("utf-8")
            request = Request(
                f"http://127.0.0.1:{port}/mcp",
                data=payload,
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with self.assertRaises(HTTPError) as error_context:
                urlopen(request, timeout=5)
            self.assertEqual(error_context.exception.code, 401)
        finally:
            self._stop_http_server(httpd, thread)

    def test_http_transport_rejects_wildcard_cors_configuration(self):
        from qcopilots_common.mcp_http import McpHttpServer

        with self.assertRaisesRegex(ValueError, "wildcard CORS"):
            McpHttpServer(
                name="qcopilots-http-wildcard-cors-test",
                version="1.0.0",
                tools=[],
                host="127.0.0.1",
                port=0,
                cors_origins=["*"],
            )

    def test_http_transport_runs_qcopilots_tools_with_bearer(self):
        import qcopilots_common.interactive_layer_tools as interactive_layer_tools
        import qcopilots_common.processing_tools as processing_tools
        from qcopilots_common.mcp_http import McpHttpServer, McpTool

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {
                    "content": [{"type": "text", "text": tool}],
                    "structuredContent": {"tool": tool},
                }

        original_bridge_client = processing_tools.BridgeClient
        original_interactive_layer_bridge_client = interactive_layer_tools.BridgeClient
        processing_tools.BridgeClient = FakeBridgeClient
        interactive_layer_tools.BridgeClient = FakeBridgeClient
        try:
            server = McpHttpServer(
                name="qcopilots-http-sensitive-test",
                version="1.0.0",
                tools=[
                    McpTool(
                        "read_file",
                        "safe read",
                        {"type": "object"},
                        lambda arguments: {
                            "content": [{"type": "text", "text": "ok"}],
                            "structuredContent": {"ok": True},
                        },
                    ),
                    McpTool(
                        "write_fixture",
                        "Fixture write tool",
                        {"type": "object"},
                        lambda arguments: {
                            "content": [{"type": "text", "text": "written"}],
                            "structuredContent": {"written": arguments.get("path")},
                        },
                    ),
                ]
                + processing_tools.build_interactive_tools()
                + processing_tools.build_processing_tools("vector")
                + processing_tools.build_processing_tools("raster"),
                host="127.0.0.1",
                port=0,
                auth_token=self.HTTP_AUTH_TOKEN,
            )
            httpd = server.create_http_server()
            port = httpd.server_address[1]
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()
            try:
                def post_rpc(request_id, method, params):
                    payload = json.dumps(
                        {
                            "jsonrpc": "2.0",
                            "id": request_id,
                            "method": method,
                            "params": params,
                        }
                    ).encode("utf-8")
                    request = Request(
                        f"http://127.0.0.1:{port}/mcp",
                        data=payload,
                        headers=self._http_headers(
                            {"Content-Type": "application/json"}
                        ),
                        method="POST",
                    )
                    with urlopen(request, timeout=5) as response:
                        return json.loads(response.read().decode("utf-8"))

                tools_list = post_rpc(0, "tools/list", {})
                listed_tools = {
                    tool["name"]: tool
                    for tool in tools_list["result"]["tools"]
                }
                self.assertEqual(
                    [tool["name"] for tool in tools_list["result"]["tools"]],
                    [
                        "read_file",
                        "write_fixture",
                        "list_layers",
                        "set_layer_visibility",
                        "zoom_to_layer",
                        "zoom_to_extent",
                        "zoom_in",
                        "zoom_out",
                        "zoom_full",
                        "zoom_to_selection",
                        "zoom_to_native_resolution",
                        "zoom_to_last_extent",
                        "zoom_to_next_extent",
                        "save_project",
                        "refresh_canvas",
                        "export_map_image",
                        "describe_layer_sources",
                        "list_map_layers",
                        "load_map_layer",
                        "remove_map_layers",
                        "create_vector_layer",
                        "add_vector_features",
                        "update_vector_features",
                        "list_vector_processing_algorithms",
                        "get_vector_processing_algorithm_details",
                        "start_vector_processing_algorithm",
                        "get_vector_processing_job",
                        "list_vector_processing_jobs",
                        "cancel_vector_processing_job",
                        "list_raster_processing_algorithms",
                        "get_raster_processing_algorithm_details",
                        "start_raster_processing_algorithm",
                        "get_raster_processing_job",
                        "list_raster_processing_jobs",
                        "cancel_raster_processing_job",
                    ],
                )
                for legacy_tool_name in (
                    "add_vector_layer",
                    "add_raster_layer",
                    "remove_layer",
                    "interactive_layer_load_layer",
                    "interactive_layer_remove_layers",
                ):
                    self.assertNotIn(legacy_tool_name, listed_tools)
                self.assertEqual(
                    listed_tools["create_vector_layer"]["inputSchema"]["required"],
                    ["name"],
                )
                self.assertEqual(
                    listed_tools["add_vector_features"]["inputSchema"]["properties"]["features"]["minItems"],
                    1,
                )
                self.assertEqual(
                    listed_tools["update_vector_features"]["inputSchema"]["properties"]["updates"]["minItems"],
                    1,
                )
                self.assertEqual(
                    listed_tools["start_vector_processing_algorithm"]["inputSchema"]["required"],
                    ["algorithm_id"],
                )
                self.assertEqual(
                    listed_tools["start_vector_processing_algorithm"]["inputSchema"]["properties"]["parameters"]["default"],
                    {},
                )
                self.assertTrue(
                    listed_tools["start_vector_processing_algorithm"]["inputSchema"]["properties"]["add_outputs_to_project"]["default"]
                )
                self.assertEqual(
                    listed_tools["list_vector_processing_jobs"]["inputSchema"]["properties"]["states"]["items"]["enum"],
                    [
                        "queued",
                        "running",
                        "cancelling",
                        "succeeded",
                        "failed",
                        "cancelled",
                    ],
                )
                self.assertNotIn("run_vector_processing_algorithm", listed_tools)
                self.assertNotIn("run_raster_processing_algorithm", listed_tools)

                for index, (tool_name, arguments, expected_message) in enumerate(
                    (
                        ("create_vector_layer", {}, "$.name is required"),
                        (
                            "create_vector_layer",
                            {"name": "scratch", "unexpected": True},
                            "$.unexpected is not allowed",
                        ),
                        (
                            "add_vector_features",
                            {"layer_id": "scratch"},
                            "$.features is required",
                        ),
                        (
                            "add_vector_features",
                            {"layer_id": "scratch", "features": []},
                            "$.features contains fewer than minItems 1",
                        ),
                        (
                            "update_vector_features",
                            {"layer_id": "scratch"},
                            "$.updates is required",
                        ),
                        (
                            "update_vector_features",
                            {"layer_id": "scratch", "updates": []},
                            "$.updates contains fewer than minItems 1",
                        ),
                        (
                            "update_vector_features",
                            {"layer_id": "scratch", "updates": [{"feature_id": 1}]},
                            "$.updates[0] must match at least one anyOf alternative",
                        ),
                        (
                            "update_vector_features",
                            {"layer_id": "scratch", "updates": [{"feature_id": 1, "attributes": {}}]},
                            "$.updates[0] must match at least one anyOf alternative",
                        ),
                        (
                            "start_vector_processing_algorithm",
                            {},
                            "$.algorithm_id is required",
                        ),
                        (
                            "start_vector_processing_algorithm",
                            {"algorithm_id": ""},
                            "$.algorithm_id is shorter than minLength 1",
                        ),
                        (
                            "start_vector_processing_algorithm",
                            {
                                "algorithm_id": "native:buffer",
                                "client_request_id": "r" * 129,
                            },
                            "$.client_request_id is longer than maxLength 128",
                        ),
                        (
                            "get_vector_processing_job",
                            {},
                            "$.job_id is required",
                        ),
                        (
                            "list_vector_processing_jobs",
                            {"states": []},
                            "$.states contains fewer than minItems 1",
                        ),
                        (
                            "list_vector_processing_jobs",
                            {"states": ["unknown"]},
                            "$.states[0] is not one of the allowed values",
                        ),
                        (
                            "list_vector_processing_jobs",
                            {"limit": 201},
                            "$.limit must be <= 200",
                        ),
                    )
                ):
                    call_count = len(calls)
                    error_body = post_rpc(
                        50 + index,
                        "tools/call",
                        {"name": tool_name, "arguments": arguments},
                    )
                    self.assertEqual(error_body["error"]["code"], -32602)
                    self.assertIn(expected_message, error_body["error"]["message"])
                    self.assertEqual(len(calls), call_count)

                safe_payload = json.dumps(
                    {
                        "jsonrpc": "2.0",
                        "id": 1,
                        "method": "tools/call",
                        "params": {"name": "read_file", "arguments": {}},
                    }
                ).encode("utf-8")
                safe_request = Request(
                    f"http://127.0.0.1:{port}/mcp",
                    data=safe_payload,
                    headers=self._http_headers(
                        {"Content-Type": "application/json"}
                    ),
                    method="POST",
                )
                with urlopen(safe_request, timeout=5) as response:
                    body = json.loads(response.read().decode("utf-8"))
                self.assertEqual(body["result"]["structuredContent"], {"ok": True})

                list_layers_payload = json.dumps(
                    {
                        "jsonrpc": "2.0",
                        "id": 2,
                        "method": "tools/call",
                        "params": {"name": "list_layers", "arguments": {}},
                    }
                ).encode("utf-8")
                list_layers_request = Request(
                    f"http://127.0.0.1:{port}/mcp",
                    data=list_layers_payload,
                    headers=self._http_headers(
                        {"Content-Type": "application/json"}
                    ),
                    method="POST",
                )
                with urlopen(list_layers_request, timeout=5) as response:
                    body = json.loads(response.read().decode("utf-8"))
                self.assertEqual(
                    body["result"]["structuredContent"],
                    {"tool": "list_layers"},
                )

                write_payload = json.dumps(
                    {
                        "jsonrpc": "2.0",
                        "id": 3,
                        "method": "tools/call",
                        "params": {
                            "name": "write_fixture",
                            "arguments": {"path": "notes.txt"},
                        },
                    }
                ).encode("utf-8")
                write_request = Request(
                    f"http://127.0.0.1:{port}/mcp",
                    data=write_payload,
                    headers=self._http_headers(
                        {"Content-Type": "application/json"}
                    ),
                    method="POST",
                )
                with urlopen(write_request, timeout=5) as response:
                    body = json.loads(response.read().decode("utf-8"))
                self.assertEqual(
                    body["result"]["structuredContent"],
                    {"written": "notes.txt"},
                )

                for tool_name, arguments, bridge_tool_name in (
                    ("describe_layer_sources", {}, "interactive_layer_describe_sources"),
                    ("list_map_layers", {}, "interactive_layer_list_layers"),
                    (
                        "load_map_layer",
                        {
                            "layer_type": "xyz",
                            "source": "http://127.0.0.1/tiles/{z}/{x}/{y}.png",
                        },
                        "interactive_layer_load_layer",
                    ),
                    (
                        "list_vector_processing_algorithms",
                        {},
                        "processing_list_algorithms",
                    ),
                    (
                        "remove_map_layers",
                        {"layer_id": "layer-1"},
                        "interactive_layer_remove_layers",
                    ),
                    (
                        "create_vector_layer",
                        {"name": "scratch", "path": "memory:"},
                        "create_vector_layer",
                    ),
                    (
                        "add_vector_features",
                        {
                            "layer_id": "scratch",
                            "features": [{"attributes": {"name": "alpha"}}],
                        },
                        "add_vector_features",
                    ),
                    (
                        "update_vector_features",
                        {
                            "layer_id": "scratch",
                            "updates": [{"feature_id": 1, "attributes": {"name": "beta"}}],
                        },
                        "update_vector_features",
                    ),
                    (
                        "start_vector_processing_algorithm",
                        {
                            "algorithm_id": "native:buffer",
                            "parameters": {"OUTPUT": "memory:"},
                            "add_outputs_to_project": False,
                            "client_request_id": "vector-request",
                        },
                        "processing_start_algorithm",
                    ),
                    (
                        "get_vector_processing_job",
                        {"job_id": "vector-job"},
                        "processing_get_job",
                    ),
                    (
                        "list_vector_processing_jobs",
                        {"states": ["running"], "limit": 10},
                        "processing_list_jobs",
                    ),
                    (
                        "cancel_vector_processing_job",
                        {"job_id": "vector-job"},
                        "processing_cancel_job",
                    ),
                    (
                        "list_raster_processing_algorithms",
                        {},
                        "processing_list_algorithms",
                    ),
                    (
                        "start_raster_processing_algorithm",
                        {"algorithm_id": "gdal:warpreproject"},
                        "processing_start_algorithm",
                    ),
                    (
                        "get_raster_processing_job",
                        {"job_id": "raster-job"},
                        "processing_get_job",
                    ),
                    (
                        "list_raster_processing_jobs",
                        {},
                        "processing_list_jobs",
                    ),
                    (
                        "cancel_raster_processing_job",
                        {"job_id": "raster-job"},
                        "processing_cancel_job",
                    ),
                ):
                    payload = json.dumps(
                        {
                            "jsonrpc": "2.0",
                            "id": 4,
                            "method": "tools/call",
                            "params": {"name": tool_name, "arguments": arguments},
                        }
                    ).encode("utf-8")
                    request = Request(
                        f"http://127.0.0.1:{port}/mcp",
                        data=payload,
                        headers=self._http_headers(
                            {"Content-Type": "application/json"}
                        ),
                        method="POST",
                    )
                    with urlopen(request, timeout=5) as response:
                        body = json.loads(response.read().decode("utf-8"))
                    self.assertEqual(
                        body["result"]["structuredContent"],
                        {"tool": bridge_tool_name},
                    )
                self.assertIn(
                    ("processing_list_algorithms", {"category": "vector"}),
                    calls,
                )
                self.assertIn(
                    ("processing_list_algorithms", {"category": "raster"}),
                    calls,
                )
                self.assertIn(
                    (
                        "processing_start_algorithm",
                        {
                            "algorithm_id": "native:buffer",
                            "parameters": {"OUTPUT": "memory:"},
                            "add_outputs_to_project": False,
                            "client_request_id": "vector-request",
                            "category": "vector",
                        },
                    ),
                    calls,
                )
                self.assertIn(
                    (
                        "processing_start_algorithm",
                        {
                            "algorithm_id": "gdal:warpreproject",
                            "parameters": {},
                            "add_outputs_to_project": True,
                            "category": "raster",
                        },
                    ),
                    calls,
                )
                self.assertIn(
                    (
                        "processing_list_jobs",
                        {
                            "states": ["running"],
                            "limit": 10,
                            "category": "vector",
                        },
                    ),
                    calls,
                )
            finally:
                self._stop_http_server(httpd, thread)
        finally:
            processing_tools.BridgeClient = original_bridge_client
            interactive_layer_tools.BridgeClient = original_interactive_layer_bridge_client

    def test_qgis_bridge_dispatcher_close_cancels_queued_calls(self):
        from qcopilots_common.bridge import _QtMainThreadDispatcher

        emitted = threading.Event()
        queued_payloads = []
        invoked = []
        errors = []

        class FakeSignal:
            def emit(self, payload):
                queued_payloads.append(payload)
                emitted.set()

        class FakeThread:
            @staticmethod
            def currentThread():
                return object()

        dispatcher = object.__new__(_QtMainThreadDispatcher)
        dispatcher._app = types.SimpleNamespace(thread=lambda: "main-thread")
        dispatcher._qthread = FakeThread
        dispatcher._object = types.SimpleNamespace(request=FakeSignal())
        dispatcher._closed = threading.Event()
        dispatcher._pending_lock = threading.Lock()
        dispatcher._pending = {}

        def wait_for_dispatch():
            try:
                dispatcher.call(lambda: invoked.append(True), timeout_seconds=5)
            except Exception as err:
                errors.append(err)

        thread = threading.Thread(target=wait_for_dispatch)
        thread.start()
        self.assertTrue(emitted.wait(2), "Dispatcher call was not queued")
        dispatcher.close()
        thread.join(timeout=2)
        self.assertFalse(thread.is_alive(), "Dispatcher close did not unblock the call")
        self.assertEqual(len(errors), 1)
        self.assertIn("stopping", str(errors[0]))

        dispatcher._dispatch(queued_payloads[0])
        self.assertEqual(invoked, [])
        with self.assertRaisesRegex(RuntimeError, "stopping"):
            dispatcher.call(lambda: None)

    def test_bridge_client_reads_bearer_token_from_environment(self):
        import qcopilots_common.bridge as bridge_module
        from qcopilots_common.bridge import BridgeClient
        from qcopilots_common.constants import QGIS_BRIDGE_AUTH_TOKEN_ENV

        class FakeResponse:
            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                del exc_type, exc_value, traceback

            def read(self):
                return b'{"ok": true, "result": {"value": 1}}'

        captured_requests = []
        original_urlopen = bridge_module.urlopen
        old_auth_token = os.environ.get(QGIS_BRIDGE_AUTH_TOKEN_ENV)
        try:
            os.environ[QGIS_BRIDGE_AUTH_TOKEN_ENV] = "unit-test-bridge-secret"

            def fake_urlopen(request, timeout):
                captured_requests.append((request, timeout))
                return FakeResponse()

            bridge_module.urlopen = fake_urlopen
            result = BridgeClient("http://127.0.0.1:48200").call("echo", {})

            self.assertEqual(result, {"value": 1})
            request, timeout = captured_requests[0]
            self.assertEqual(timeout, 60)
            self.assertEqual(
                request.get_header("Authorization"),
                "Bearer unit-test-bridge-secret",
            )
        finally:
            bridge_module.urlopen = original_urlopen
            if old_auth_token is None:
                os.environ.pop(QGIS_BRIDGE_AUTH_TOKEN_ENV, None)
            else:
                os.environ[QGIS_BRIDGE_AUTH_TOKEN_ENV] = old_auth_token

    def test_qgis_bridge_requires_bearer_and_rejects_origin(self):
        from qcopilots_common.bridge import QgisBridgeController

        auth_token = "unit-test-bridge-secret"
        controller = QgisBridgeController(None, port=0, auth_token=auth_token)
        controller.dispatch = lambda tool, arguments: {
            "tool": tool,
            "arguments": arguments,
        }
        controller_thread = None
        try:
            controller.start()
            controller_thread = controller._thread
            self.assertIsNotNone(controller_thread)
            payload = json.dumps(
                {"tool": "echo", "arguments": {"value": 1}}
            ).encode("utf-8")
            for path, method, data in (
                ("/health", "GET", None),
                ("/call", "POST", payload),
            ):
                for authorization in (None, "Bearer wrong"):
                    headers = {"Content-Type": "application/json"}
                    if authorization is not None:
                        headers["Authorization"] = authorization
                    request = Request(
                        f"{controller.url}{path}",
                        data=data,
                        headers=headers,
                        method=method,
                    )
                    with self.subTest(path=path, authorization=authorization):
                        with self.assertRaises(HTTPError) as error_context:
                            urlopen(request, timeout=5)
                        self.assertEqual(error_context.exception.code, 401)

            with urlopen(
                Request(
                    f"{controller.url}/health",
                    headers={"Authorization": f"Bearer {auth_token}"},
                ),
                timeout=5,
            ) as response:
                self.assertEqual(response.status, 200)

            authorized_headers = {
                "Content-Type": "application/json",
                "Authorization": f"Bearer {auth_token}",
            }
            with urlopen(
                Request(
                    f"{controller.url}/call",
                    data=payload,
                    headers=authorized_headers,
                    method="POST",
                ),
                timeout=5,
            ) as response:
                self.assertEqual(
                    json.loads(response.read().decode("utf-8"))["result"],
                    {"tool": "echo", "arguments": {"value": 1}},
                )

            for origin in ("http://127.0.0.1:8282", "https://example.invalid"):
                request = Request(
                    f"{controller.url}/call",
                    data=payload,
                    headers={**authorized_headers, "Origin": origin},
                    method="POST",
                )
                with self.subTest(origin=origin):
                    with self.assertRaises(HTTPError) as error_context:
                        urlopen(request, timeout=5)
                    self.assertEqual(error_context.exception.code, 403)

            with self.assertRaises(HTTPError) as options_context:
                urlopen(
                    Request(
                        f"{controller.url}/call",
                        headers={"Authorization": f"Bearer {auth_token}"},
                        method="OPTIONS",
                    ),
                    timeout=5,
                )
            self.assertEqual(options_context.exception.code, 405)

            raw_request = (
                "GET /health HTTP/1.1\r\n"
                f"Host: localhost:{controller.port}\r\n"
                f"Authorization: Bearer {auth_token}\r\n"
                "Connection: close\r\n"
                "\r\n"
            ).encode("ascii")
            with socket.create_connection(
                ("127.0.0.1", controller.port), timeout=2
            ) as connection:
                connection.sendall(raw_request)
                raw_response = connection.recv(4096).decode("iso-8859-1")
            self.assertIn(" 403 ", raw_response.splitlines()[0])

            controller.clear_auth_token()
            with self.assertRaises(HTTPError) as revoked_context:
                urlopen(
                    Request(
                        f"{controller.url}/health",
                        headers={"Authorization": f"Bearer {auth_token}"},
                    ),
                    timeout=5,
                )
            self.assertEqual(revoked_context.exception.code, 401)
        finally:
            controller.stop()
            if controller_thread is not None:
                self.assertFalse(
                    controller_thread.is_alive(),
                    "QGIS bridge HTTP thread did not terminate",
                )

    def test_qgis_bridge_requires_token_and_literal_loopback_to_start(self):
        from qcopilots_common.bridge import QgisBridgeController

        with self.assertRaisesRegex(ValueError, "auth token"):
            QgisBridgeController(None, port=0).start()
        for host in ("localhost", "0.0.0.0", "192.168.1.20", "::1"):
            with self.subTest(host=host):
                with self.assertRaises(ValueError):
                    QgisBridgeController(
                        None,
                        host=host,
                        port=0,
                        auth_token="bridge-token",
                    )

    def test_qgis_bridge_stop_retains_live_thread_for_retry(self):
        from qcopilots_common.bridge import QgisBridgeController

        class FakeDispatcher:
            def __init__(self):
                self.closed = False

            def close(self):
                self.closed = True

        class FakeServer:
            def __init__(self):
                self.shutdown_count = 0
                self.close_count = 0

            def shutdown(self):
                self.shutdown_count += 1

            def server_close(self):
                self.close_count += 1

        class FakeThread:
            def __init__(self):
                self.alive = True
                self.join_timeouts = []

            def is_alive(self):
                return self.alive

            def join(self, timeout=None):
                self.join_timeouts.append(timeout)

        controller = QgisBridgeController(
            None,
            port=0,
            auth_token="bridge-token",
        )
        dispatcher = FakeDispatcher()
        server = FakeServer()
        thread = FakeThread()
        controller._dispatcher = dispatcher
        controller._httpd = server
        controller._thread = thread

        with self.assertRaisesRegex(RuntimeError, "did not stop"):
            controller.stop(timeout_seconds=0.01)

        self.assertTrue(dispatcher.closed)
        self.assertIs(controller._httpd, server)
        self.assertIs(controller._thread, thread)
        self.assertTrue(controller._shutdown_requested)
        self.assertEqual(len(thread.join_timeouts), 1)
        self.assertAlmostEqual(thread.join_timeouts[0], 0.01)

        thread.alive = False
        controller.stop(timeout_seconds=0.01)

        self.assertIsNone(controller._httpd)
        self.assertIsNone(controller._thread)
        self.assertFalse(controller._shutdown_requested)
        self.assertEqual(server.shutdown_count, 1)
        self.assertEqual(server.close_count, 2)

    def test_qgis_bridge_falls_back_when_preferred_port_is_busy(self):
        from qcopilots_common.bridge import QgisBridgeController

        auth_token = "unit-test-bridge-token"
        first_controller = QgisBridgeController(
            None,
            port=0,
            auth_token=auth_token,
        )
        second_controller = None
        first_thread = None
        second_thread = None
        try:
            first_controller.start()
            first_thread = first_controller._thread
            self.assertIsNotNone(first_thread)
            second_controller = QgisBridgeController(
                None,
                port=first_controller.port,
                auth_token=auth_token,
            )
            second_controller.dispatch = lambda tool, arguments: {
                "tool": tool,
                "arguments": arguments,
            }
            second_controller.start()
            second_thread = second_controller._thread
            self.assertIsNotNone(second_thread)

            self.assertNotEqual(second_controller.port, first_controller.port)
            self.assertNotEqual(second_controller.url, first_controller.url)

            health_headers = {"Authorization": f"Bearer {auth_token}"}
            with urlopen(
                Request(f"{first_controller.url}/health", headers=health_headers),
                timeout=5,
            ) as response:
                self.assertEqual(response.status, 200)
            with urlopen(
                Request(f"{second_controller.url}/health", headers=health_headers),
                timeout=5,
            ) as response:
                self.assertEqual(response.status, 200)

            payload = json.dumps(
                {"tool": "echo", "arguments": {"value": 2}}
            ).encode("utf-8")
            request = Request(
                f"{second_controller.url}/call",
                data=payload,
                headers={
                    "Content-Type": "application/json",
                    "Authorization": f"Bearer {auth_token}",
                },
                method="POST",
            )
            with urlopen(request, timeout=5) as response:
                self.assertEqual(response.status, 200)
                self.assertEqual(
                    json.loads(response.read().decode("utf-8"))["result"],
                    {"tool": "echo", "arguments": {"value": 2}},
                )
        finally:
            if second_controller:
                second_controller.stop()
                if second_thread is not None:
                    self.assertFalse(
                        second_thread.is_alive(),
                        "Fallback QGIS bridge HTTP thread did not terminate",
                    )
            first_controller.stop()
            if first_thread is not None:
                self.assertFalse(
                    first_thread.is_alive(),
                    "Preferred QGIS bridge HTTP thread did not terminate",
                )

    def test_qgis_bridge_oversized_error_body_does_not_wait_for_body(self):
        from qcopilots_common.bridge import QgisBridgeController
        from qcopilots_common.constants import MAX_HTTP_REQUEST_BODY_BYTES

        auth_token = "unit-test-bridge-token"
        controller = QgisBridgeController(
            None,
            port=0,
            auth_token=auth_token,
        )
        controller_thread = None
        try:
            controller.start()
            controller_thread = controller._thread
            self.assertIsNotNone(controller_thread)
            oversized_length = MAX_HTTP_REQUEST_BODY_BYTES + 1
            wrong_path_response = self._raw_post_response(
                controller.port,
                "/missing",
                "text/plain",
                oversized_length,
            )
            self.assertIn(" 404 ", wrong_path_response.splitlines()[0])
            self.assertIn("Connection: close", wrong_path_response)

            valid_path_response = self._raw_post_response(
                controller.port,
                "/call",
                "application/json",
                oversized_length,
                {"Authorization": f"Bearer {auth_token}"},
            )
            self.assertIn(" 413 ", valid_path_response.splitlines()[0])
            self.assertIn("Connection: close", valid_path_response)
        finally:
            controller.stop()
            if controller_thread is not None:
                self.assertFalse(
                    controller_thread.is_alive(),
                    "QGIS bridge HTTP thread did not terminate",
                )

    def test_qgis_bridge_file_paths_allow_external_paths(self):
        from qcopilots_common.bridge import (
            _safe_workspace_path,
            _sanitize_processing_parameters,
        )

        class FakeParameter:
            def __init__(self, name, type_name, class_name):
                self._name = name
                self._type_name = type_name
                self._class_name = class_name

            def name(self):
                return self._name

            def type(self):
                return self._type_name

            def className(self):
                return self._class_name

        cwd = Path.cwd().resolve()
        with tempfile.TemporaryDirectory() as tmp:
            root = (Path(tmp) / "铁路数据").resolve()
            root.mkdir()
            self.assertEqual(
                _safe_workspace_path(str(root)),
                str(root.resolve()),
            )
            self.assertEqual(
                _safe_workspace_path("outputs/map.png"),
                str((cwd / "outputs" / "map.png").resolve()),
            )
            self.assertEqual(
                _safe_workspace_path(str(root / "railway.shp")),
                str((root / "railway.shp").resolve()),
            )
            self.assertEqual(
                _safe_workspace_path(str(root.parent / "未授权" / "escape.shp")),
                str((root.parent / "未授权" / "escape.shp").resolve()),
            )
            self.assertEqual(
                _safe_workspace_path(str(root.parent / f"{root.name}2" / "escape.shp")),
                str((root.parent / f"{root.name}2" / "escape.shp").resolve()),
            )
            self.assertEqual(
                _sanitize_processing_parameters(
                    {
                        "INPUT": str(root / "input.gpkg"),
                        "SOURCE": "data/source.gpkg",
                        "OUTPUT": "outputs/result.gpkg",
                        "TEMPORARY": "memory:",
                        "LAYER": "existing-layer-id",
                        "FIELD": "a.b",
                        "EXPRESSION": "population / area",
                    }
                ),
                {
                    "INPUT": str((root / "input.gpkg").resolve()),
                    "SOURCE": str((cwd / "data" / "source.gpkg").resolve()),
                    "OUTPUT": str((cwd / "outputs" / "result.gpkg").resolve()),
                    "TEMPORARY": "memory:",
                    "LAYER": "existing-layer-id",
                    "FIELD": "a.b",
                    "EXPRESSION": "population / area",
                },
            )
            with tempfile.TemporaryDirectory() as outside_tmp:
                self.assertEqual(
                    _safe_workspace_path(str(Path(outside_tmp) / "escape.png")),
                    str((Path(outside_tmp) / "escape.png").resolve()),
                )
                self.assertEqual(
                    _sanitize_processing_parameters(
                        {"OUTPUT": str(Path(outside_tmp) / "escape.gpkg")}
                    ),
                    {"OUTPUT": str((Path(outside_tmp) / "escape.gpkg").resolve())},
                )

            parameter_definitions = [
                FakeParameter("MASK", "source", "QgsProcessingParameterFeatureSource"),
                FakeParameter("HUBS", "vector", "QgsProcessingParameterVectorLayer"),
                FakeParameter(
                    "INPUT_A",
                    "source",
                    "QgsProcessingParameterFeatureSource",
                ),
                FakeParameter(
                    "LAYERS",
                    "multilayer",
                    "QgsProcessingParameterMultipleLayers",
                ),
                FakeParameter("OUTPUT", "sink", "QgsProcessingParameterFeatureSink"),
                FakeParameter("FIELD", "field", "QgsProcessingParameterField"),
            ]
            self.assertEqual(
                _sanitize_processing_parameters(
                    {
                        "MASK": "data/mask.gpkg",
                        "HUBS": "existing-layer-id",
                        "INPUT_A": "data/input-a.gpkg",
                        "LAYERS": ["data/a.gpkg", "existing-layer-id"],
                        "OUTPUT": "TEMPORARY_OUTPUT",
                        "FIELD": "a.b",
                    },
                    parameter_definitions,
                ),
                {
                    "MASK": str((cwd / "data" / "mask.gpkg").resolve()),
                    "HUBS": "existing-layer-id",
                    "INPUT_A": str((cwd / "data" / "input-a.gpkg").resolve()),
                    "LAYERS": [
                        str((cwd / "data" / "a.gpkg").resolve()),
                        "existing-layer-id",
                    ],
                    "OUTPUT": "TEMPORARY_OUTPUT",
                    "FIELD": "a.b",
                },
            )
            self.assertEqual(
                _sanitize_processing_parameters(
                    {"MASK": "../outside.gpkg"},
                    parameter_definitions,
                ),
                {"MASK": str((cwd.parent / "outside.gpkg").resolve())},
            )

    def test_qgis_bridge_absolute_paths_are_allowed_by_default(self):
        from qcopilots_common.bridge import (
            _safe_workspace_path,
            _sanitize_processing_parameters,
        )

        with tempfile.TemporaryDirectory() as outside_tmp:
            outside = Path(outside_tmp).resolve()

            self.assertEqual(
                _safe_workspace_path(str(outside / "escape.png")),
                str((outside / "escape.png").resolve()),
            )
            self.assertEqual(
                _sanitize_processing_parameters(
                    {"OUTPUT": str(outside / "escape.gpkg")}
                ),
                {"OUTPUT": str((outside / "escape.gpkg").resolve())},
            )

    def test_qgis_bridge_save_project_without_path_allows_existing_project_path(self):
        from qcopilots_common.bridge import QgisBridgeTools

        module_names = ("qgis", "qgis.core")
        original_modules = {name: sys.modules.get(name) for name in module_names}
        qgis_module = types.ModuleType("qgis")
        core_module = types.ModuleType("qgis.core")
        sys.modules["qgis"] = qgis_module
        sys.modules["qgis.core"] = core_module
        try:
            with tempfile.TemporaryDirectory() as outside_tmp:
                outside = Path(outside_tmp).resolve()

                class FakeProject:
                    def __init__(self):
                        self.path = str(outside / "project.qgz")
                        self.writes = []

                    def fileName(self):
                        return self.path

                    def write(self, path=None):
                        self.writes.append(path)
                        return True

                fake_project = FakeProject()

                class FakeQgsProject:
                    @staticmethod
                    def instance():
                        return fake_project

                core_module.QgsProject = FakeQgsProject
                qgis_module.core = core_module
                tools = QgisBridgeTools(None)

                self.assertEqual(
                    tools.save_project({}),
                    {"saved": True, "path": str(outside / "project.qgz")},
                )
                self.assertEqual(fake_project.writes, [None])
        finally:
            for name, module in original_modules.items():
                if module is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = module

    def test_qgis_bridge_initializes_processing_before_algorithm_access(self):
        from qcopilots_common.bridge import _ensure_processing_initialized

        calls = []
        module_names = (
            "processing",
            "processing.core",
            "processing.core.Processing",
        )
        original_modules = {name: sys.modules.get(name) for name in module_names}
        processing_module = types.ModuleType("processing")
        processing_core_module = types.ModuleType("processing.core")
        processing_class_module = types.ModuleType("processing.core.Processing")

        class FakeProcessing:
            @staticmethod
            def initialize():
                calls.append("initialize")

        processing_class_module.Processing = FakeProcessing
        processing_module.core = processing_core_module
        processing_core_module.Processing = processing_class_module
        try:
            sys.modules["processing"] = processing_module
            sys.modules["processing.core"] = processing_core_module
            sys.modules["processing.core.Processing"] = processing_class_module

            _ensure_processing_initialized()
        finally:
            for name, module in original_modules.items():
                if module is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = module

        self.assertEqual(calls, ["initialize"])

    def test_qgis_bridge_reports_processing_import_failure(self):
        from qcopilots_common.bridge import _ensure_processing_initialized

        module_names = (
            "processing",
            "processing.core",
            "processing.core.Processing",
        )
        original_modules = {name: sys.modules.get(name) for name in module_names}
        processing_module = types.ModuleType("processing")
        processing_core_module = types.ModuleType("processing.core")
        processing_class_module = types.ModuleType("processing.core.Processing")
        processing_module.core = processing_core_module
        processing_core_module.Processing = processing_class_module
        try:
            sys.modules["processing"] = processing_module
            sys.modules["processing.core"] = processing_core_module
            sys.modules["processing.core.Processing"] = processing_class_module

            with self.assertRaisesRegex(
                RuntimeError,
                "QGIS Processing is not available",
            ):
                _ensure_processing_initialized()
        finally:
            for name, module in original_modules.items():
                if module is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = module

    def test_qgis_bridge_reports_processing_initialize_failure(self):
        from qcopilots_common.bridge import _ensure_processing_initialized

        module_names = (
            "processing",
            "processing.core",
            "processing.core.Processing",
        )
        original_modules = {name: sys.modules.get(name) for name in module_names}
        processing_module = types.ModuleType("processing")
        processing_core_module = types.ModuleType("processing.core")
        processing_class_module = types.ModuleType("processing.core.Processing")

        class FailingProcessing:
            @staticmethod
            def initialize():
                raise RuntimeError("provider setup failed")

        processing_class_module.Processing = FailingProcessing
        processing_module.core = processing_core_module
        processing_core_module.Processing = processing_class_module
        try:
            sys.modules["processing"] = processing_module
            sys.modules["processing.core"] = processing_core_module
            sys.modules["processing.core.Processing"] = processing_class_module

            with self.assertRaisesRegex(
                RuntimeError,
                "QGIS Processing could not be initialized: provider setup failed",
            ):
                _ensure_processing_initialized()
        finally:
            for name, module in original_modules.items():
                if module is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = module


if __name__ == "__main__":
    unittest.main()
