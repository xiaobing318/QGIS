from __future__ import annotations

import json
from unittest.mock import patch

from processingmcpserver.mcp_resources import register_resources

from ._shared_case_base import ProcessingMCPTestBase
from ._shared_fixtures import DummyMcp, DummyTools


class _FakeProject:
    def __init__(self, file_name: str = "", title: str = "") -> None:
        """初始化 _FakeProject 实例状态。"""
        self._file_name = file_name
        self._title = title

    def fileName(self) -> str:
        """执行 fileName 相关逻辑。"""
        return self._file_name

    def title(self) -> str:
        """执行 title 相关逻辑。"""
        return self._title


class ResourceQgisProjectInfoTest(ProcessingMCPTestBase):
    def test_resource_registered(self):
        """验证 resource registered 场景。"""
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())
        self.assertIn("qgis://project/info", mcp.resource_uris)

    def test_resource_envelope_ok(self):
        """验证 resource envelope ok 场景。"""
        mcp = DummyMcp()
        register_resources(mcp, DummyTools())

        payload = json.loads(mcp.resource_funcs["qgis://project/info"]())
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["uri"], "qgis://project/info")
        self.assertEqual(payload["schema_version"], "1.0.0")
        self.assertIn("generated_at", payload)
        self.assertIn("project_id", payload)
        self.assertIn("data", payload)

    def test_project_id_prefers_snapshot_file_name_then_title(self):
        """验证 project ID prefers snapshot file name then title 场景。"""
        class FileNameTools(DummyTools):
            def get_project_snapshot(self):
                """返回 project snapshot。"""
                return {
                    "file_name": "C:/projects/demo.qgz",
                    "title": "snapshot-title",
                }

        mcp = DummyMcp()
        register_resources(mcp, FileNameTools())

        payload = json.loads(mcp.resource_funcs["qgis://project/info"]())
        self.assertEqual(payload["project_id"], "C:/projects/demo.qgz")

    @patch("processingmcpserver.mcp_resources.QgsProject.instance")
    def test_project_id_falls_back_to_snapshot_title_before_qgs_project(
        self, mock_project_instance
    ):
        """验证 project ID falls back to snapshot title before qgs project 场景。"""
        class TitleTools(DummyTools):
            def get_project_snapshot(self):
                """返回 project snapshot。"""
                return {"file_name": "", "title": "snapshot-title"}

        mock_project_instance.return_value = _FakeProject(
            file_name="C:/projects/fallback.qgz",
            title="fallback-title",
        )

        mcp = DummyMcp()
        register_resources(mcp, TitleTools())

        payload = json.loads(mcp.resource_funcs["qgis://project/info"]())
        self.assertEqual(payload["project_id"], "snapshot-title")

    def test_resource_envelope_error_when_supplier_fails(self):
        """验证 resource envelope error when supplier fails 场景。"""
        class BrokenTools(DummyTools):
            def get_project_snapshot(self):
                """返回 project snapshot。"""
                raise RuntimeError("project snapshot failed")

        with patch("processingmcpserver.mcp_resources.QgsProject.instance") as mock_project_instance:
            mock_project_instance.return_value = _FakeProject(
                file_name="",
                title="fallback-title",
            )

            mcp = DummyMcp()
            register_resources(mcp, BrokenTools())

            payload = json.loads(mcp.resource_funcs["qgis://project/info"]())
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["uri"], "qgis://project/info")
        self.assertEqual(payload["project_id"], "fallback-title")
        self.assertIn("error", payload)
        self.assertIn("project snapshot failed", payload["error"]["message"])
