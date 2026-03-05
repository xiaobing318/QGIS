from __future__ import annotations

from processingmcpserver.mcp_prompts import register_prompts

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class PromptQgisTaskPlannerTest(ProcessingMCPTestBase):
    def test_prompt_registered(self):
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())
        self.assertIn("qgis_task_planner", mcp.prompt_names)

    def test_prompt_template_structure(self):
        mcp = DummyMcp()
        register_prompts(mcp, DummyTools())

        prompt_fn = mcp.prompt_funcs["qgis_task_planner"]
        output = prompt_fn(task="缓冲道路图层", constraints="输出到 GeoPackage")

        self.assertIn(
            "Structure: Goal -> Inputs -> Tool Chain -> Key Params -> Validation -> Deliverables",
            output,
        )
        self.assertIn("Goal / 目标", output)
        self.assertIn("Tool Chain / 工具链", output)
        self.assertIn("Deliverables / 交付物", output)
        self.assertIn("Task: 缓冲道路图层", output)

