from __future__ import annotations

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import assert_tool_registered


class Test(ProcessingMCPTestBase):
    def test_registered(self):
        assert_tool_registered(self, "mcp_tools_dataset_load_layers")
