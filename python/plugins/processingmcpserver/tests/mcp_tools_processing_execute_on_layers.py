from __future__ import annotations

from processingmcpserver.mcp_tools import register_tools

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp


class ToolsProcessingExecuteOnLayersTest(ProcessingMCPTestBase):
    def test_removed_processing_execute_on_layers_not_registered(self):
        mcp = DummyMcp()
        register_tools(mcp, self.build_tools(), enable_execute_code=False)
        self.assertNotIn("processing_execute_on_layers", set(mcp.tool_names))
