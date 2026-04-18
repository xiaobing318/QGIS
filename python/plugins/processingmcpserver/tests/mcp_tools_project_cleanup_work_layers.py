from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class ToolsProjectCleanupWorkLayersTest(ProcessingMCPTestBase):
    def test_registered(self):
        """
        作用：执行测试用例 `registered`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `registered`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        assert_tool_registered(self, "mcp_tools_project_cleanup_work_layers")

    def test_cleanup_removes_work_layers_and_temp_files(self):
        """
        作用：执行测试用例 `cleanup removes work layers and temp files`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `cleanup removes work layers and temp files`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        tools = self.build_tools()
        layer = self.add_sample_vector_layer("cleanup_source")
        prepared = tools.mcp_tools_vector_prepare_work_layer(
            layer_ref=layer.id(),
            task_name="cleanup-task",
        )
        work_layer_id = prepared["summary"]["output_layer_id"]
        output_dir = self.make_temp_dir()
        shapefile_path = self.export_layer_to_shapefile(layer, output_dir, "cleanup_temp")

        result = tools.mcp_tools_project_cleanup_work_layers(
            task_name="cleanup-task",
            temp_paths=[str(shapefile_path)],
            delete_temp_files=True,
            confirm_write=True,
            confirm_destructive=True,
        )

        self.assertIn(work_layer_id, result["outputs"]["removed_layer_ids"])
        self.assertIsNone(self.project_layer(work_layer_id))
        self.assertFalse(shapefile_path.exists())

