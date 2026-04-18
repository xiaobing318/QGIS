from __future__ import annotations

import inspect

from processingmcpserver.mcp_prompts import register_prompts

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class PromptQgisShapefileWorkflowsTest(ProcessingMCPTestBase):
    def test_prompts_registered(self):
        """
        作用：执行测试用例 `prompts registered`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompts registered`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())
        self.assertEqual(
            set(mcp.prompt_names),
            {
                "qgis_shapefile_quality_repair_export_workflow",
                "qgis_shapefile_overlay_clip_stats_workflow",
                "qgis_shapefile_buffer_join_workflow",
            },
        )
        self.assertNotIn("qgis_shapefile_pipeline_planner", mcp.prompt_names)

    def test_prompt_signatures_match_expected_arguments(self):
        """
        作用：执行测试用例 `prompt signatures match expected arguments`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompt signatures match expected arguments`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        expected_signatures = {
            "qgis_shapefile_quality_repair_export_workflow": [
                "task_name",
                "input_dir",
                "output_dir",
                "target_crs",
                "geometry_type",
                "name_glob",
                "required_fields",
                "deliverables",
            ],
            "qgis_shapefile_overlay_clip_stats_workflow": [
                "task_name",
                "subject_shapefile",
                "overlay_shapefile",
                "output_shapefile",
                "target_crs",
                "area_field",
                "group_field",
                "deliverables",
            ],
            "qgis_shapefile_buffer_join_workflow": [
                "task_name",
                "line_shapefile",
                "point_shapefile",
                "output_shapefile",
                "buffer_distance",
                "target_crs",
                "predicate_codes",
                "point_category_field",
                "deliverables",
            ],
        }

        for prompt_name, expected_params in expected_signatures.items():
            prompt_fn = mcp.prompt_funcs[prompt_name]
            signature = inspect.signature(prompt_fn)
            self.assertEqual(list(signature.parameters.keys()), expected_params)

        self.assertEqual(
            inspect.signature(
                mcp.prompt_funcs["qgis_shapefile_quality_repair_export_workflow"]
            ).parameters["task_name"].default,
            inspect.Signature.empty,
        )
        self.assertEqual(
            inspect.signature(
                mcp.prompt_funcs["qgis_shapefile_overlay_clip_stats_workflow"]
            ).parameters["subject_shapefile"].default,
            inspect.Signature.empty,
        )
        self.assertEqual(
            inspect.signature(
                mcp.prompt_funcs["qgis_shapefile_buffer_join_workflow"]
            ).parameters["buffer_distance"].default,
            inspect.Signature.empty,
        )

    def test_prompt_templates_include_expected_toolchains(self):
        """
        作用：执行测试用例 `prompt templates include expected toolchains`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompt templates include expected toolchains`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        quality_output = mcp.prompt_funcs[
            "qgis_shapefile_quality_repair_export_workflow"
        ](
            task_name="roads-cleanup",
            input_dir="C:/input",
            output_dir="C:/output",
            required_fields="id,name",
            deliverables="final shapefile bundle",
        )
        self.assertIn("mcp_tools_dataset_inspect_vector_bundle", quality_output)
        self.assertIn("mcp_tools_vector_prepare_work_layer", quality_output)
        self.assertIn("normalize_field_names=true", quality_output)
        self.assertIn("multipart_policy=singleparts", quality_output)
        self.assertIn("mcp_tools_vector_export_dataset", quality_output)
        self.assertIn("mcp_tools_project_cleanup_work_layers", quality_output)

        overlay_output = mcp.prompt_funcs[
            "qgis_shapefile_overlay_clip_stats_workflow"
        ](
            task_name="parcel-overlay",
            subject_shapefile="C:/input/subject.shp",
            overlay_shapefile="C:/input/overlay.shp",
            output_shapefile="C:/output/result.shp",
            group_field="district",
        )
        self.assertIn("mcp_tools_vector_clip", overlay_output)
        self.assertIn("mcp_tools_vector_table_calculate_field", overlay_output)
        self.assertIn("expression=$area", overlay_output)
        self.assertIn("mcp_tools_vector_stats_by_categories", overlay_output)
        self.assertIn("mcp_tools_vector_export_dataset", overlay_output)

        buffer_output = mcp.prompt_funcs["qgis_shapefile_buffer_join_workflow"](
            task_name="road-service-area",
            line_shapefile="C:/input/roads.shp",
            point_shapefile="C:/input/poi.shp",
            output_shapefile="C:/output/service_area.shp",
            buffer_distance=100.0,
            point_category_field="poi_type",
        )
        self.assertIn("mcp_tools_vector_buffer", buffer_output)
        self.assertIn("mcp_tools_vector_join_attributes_by_location", buffer_output)
        self.assertIn("mcp_tools_vector_stats_by_categories", buffer_output)
        self.assertIn("mcp_tools_vector_table_query_records", buffer_output)
        self.assertIn("mcp_tools_vector_export_dataset", buffer_output)
        self.assertNotIn("qgis_shapefile_pipeline_planner", buffer_output)

    def test_prompt_defaults_for_optional_arguments(self):
        """
        作用：执行测试用例 `prompt defaults for optional arguments`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `prompt defaults for optional arguments`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        quality_output = mcp.prompt_funcs[
            "qgis_shapefile_quality_repair_export_workflow"
        ](
            task_name="roads-cleanup",
            input_dir="C:/input",
            output_dir="C:/output",
        )
        self.assertIn(
            "target_crs: EPSG:4490",
            quality_output,
        )
        self.assertIn(
            "geometry_type: any",
            quality_output,
        )

        overlay_output = mcp.prompt_funcs[
            "qgis_shapefile_overlay_clip_stats_workflow"
        ](
            task_name="parcel-overlay",
            subject_shapefile="C:/input/subject.shp",
            overlay_shapefile="C:/input/overlay.shp",
            output_shapefile="C:/output/result.shp",
        )
        self.assertIn("area_field: area_m2", overlay_output)

        buffer_output = mcp.prompt_funcs["qgis_shapefile_buffer_join_workflow"](
            task_name="road-service-area",
            line_shapefile="C:/input/roads.shp",
            point_shapefile="C:/input/poi.shp",
            output_shapefile="C:/output/service_area.shp",
            buffer_distance=60.5,
        )
        self.assertIn("predicate_codes: 0", buffer_output)
        self.assertIn(
            "point_category_field: (none)",
            buffer_output,
        )
