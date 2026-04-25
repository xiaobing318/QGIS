from __future__ import annotations

from processingmcpserver.mcp_tools import register_tools

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp


class ToolsLayerGetPanelTreeTest(ProcessingMCPTestBase):
    def test_removed_layer_get_panel_tree_not_registered(self):
        mcp = DummyMcp()
        register_tools(mcp, self.build_tools(), enable_execute_code=False)
        self.assertNotIn("layer_get_panel_tree", set(mcp.tool_names))
