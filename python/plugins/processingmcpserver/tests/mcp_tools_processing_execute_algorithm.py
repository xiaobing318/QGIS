from __future__ import annotations

from unittest.mock import patch

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProcessingExecuteAlgorithmTest(ProcessingMCPTestBase):
    def test_registered(self):
        """验证目标能力已完成注册。"""
        assert_tool_registered(self, "processing_execute_algorithm")

    @patch("processingmcpserver.mcp_tools.mcp_tools_processing_execute_algorithm.processing.run")
    def test_success_default_rewrites_disk_output_and_blocks_in_place(self, mock_run):
        """验证 default rewrites disk output and blocks in place 的成功场景。"""
        tools = self.build_tools()
        mock_run.return_value = {"OUTPUT": "TEMPORARY_OUTPUT"}

        result = tools.processing_execute_algorithm(
            algorithm="fake:buffer",
            parameters={
                "INPUT": "layer-1",
                "OUTPUT": "C:/tmp/result.gpkg",
                "IN_PLACE": True,
            },
            load_results=False,
        )

        called_parameters = mock_run.call_args.args[1]
        self.assertEqual(called_parameters.get("OUTPUT"), "TEMPORARY_OUTPUT")
        self.assertFalse(bool(called_parameters.get("IN_PLACE")))
        self.assertFalse(result["safety_policy"]["allow_disk_write"])
        self.assertFalse(result["safety_policy"]["allow_in_place_edit"])
        self.assertTrue(result["warnings"])

    @patch("processingmcpserver.mcp_tools.mcp_tools_processing_execute_algorithm.processing.run")
    def test_success_allows_disk_write_when_explicit(self, mock_run):
        """验证 allows disk write when explicit 的成功场景。"""
        tools = self.build_tools()
        mock_run.return_value = {"OUTPUT": "C:/tmp/result.gpkg"}

        result = tools.processing_execute_algorithm(
            algorithm="fake:buffer",
            parameters={
                "INPUT": "layer-1",
                "OUTPUT": "C:/tmp/result.gpkg",
                "IN_PLACE": True,
            },
            load_results=False,
            allow_disk_write=True,
            allow_in_place_edit=True,
        )

        called_parameters = mock_run.call_args.args[1]
        self.assertEqual(called_parameters.get("OUTPUT"), "C:/tmp/result.gpkg")
        self.assertTrue(bool(called_parameters.get("IN_PLACE")))
        self.assertTrue(result["safety_policy"]["allow_disk_write"])
        self.assertTrue(result["safety_policy"]["allow_in_place_edit"])

    def test_failure_parameters_must_be_object(self):
        """验证 parameters must be object 的失败场景。"""
        tools = self.build_tools()
        with self.assertRaises(Exception) as ctx:
            tools.processing_execute_algorithm(
                algorithm="native:buffer",
                parameters="not-an-object",
            )
        self.assertIn("parameters must be an object", str(ctx.exception))

    @patch("processingmcpserver.mcp_tools.mcp_tools_processing_execute_algorithm.processing.run")
    def test_failure_propagates_processing_runtime_error(self, mock_run):
        """验证 propagates processing runtime error 的失败场景。"""
        tools = self.build_tools()
        mock_run.side_effect = RuntimeError("processing boom")

        with self.assertRaises(RuntimeError) as ctx:
            tools.processing_execute_algorithm(
                algorithm="fake:buffer",
                parameters={"INPUT": "layer-1"},
                load_results=False,
            )
        self.assertIn("processing boom", str(ctx.exception))
