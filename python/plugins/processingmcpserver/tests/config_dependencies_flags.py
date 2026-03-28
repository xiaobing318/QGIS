from __future__ import annotations

from processingmcpserver.config import (
    default_processing_mcp_json_document,
    load_processing_mcp_server_config,
)

from ._shared_case_base import ProcessingMCPTestBase


class ConfigDependenciesFlagsTest(ProcessingMCPTestBase):
    def test_dependencies_default_loaded(self):
        """
        作用：执行测试用例 `dependencies default loaded`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `dependencies default loaded`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        config = load_processing_mcp_server_config()
        self.assertTrue(config.dependencies.auto_install)
        self.assertEqual(config.config_sources.get("dependencies.auto_install"), "JSON")

    def test_dependencies_json_overrides_settings(self):
        """
        作用：执行测试用例 `dependencies json overrides settings`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `dependencies json overrides settings`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.settings.setValue("Processing/MCP/dependencies/auto_install", False)
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["dependencies"] = {
            "auto_install": True,
        }
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertTrue(config.dependencies.auto_install)
        self.assertEqual(config.config_sources.get("dependencies.auto_install"), "JSON")

    def test_dependencies_invalid_json_fallback_to_settings(self):
        """
        作用：执行测试用例 `dependencies invalid json fallback to settings`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `dependencies invalid json fallback to settings`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.settings.setValue("Processing/MCP/dependencies/auto_install", False)
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["dependencies"] = {
            "auto_install": "invalid",
        }
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertFalse(config.dependencies.auto_install)
        self.assertEqual(config.config_sources.get("dependencies.auto_install"), "Settings")

    def test_enable_execute_code_default_and_json_override(self):
        """
        作用：执行测试用例 `enable execute code default and json override`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `enable execute code default and json override`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        config = load_processing_mcp_server_config()
        self.assertFalse(config.enable_execute_code)

        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["enable_execute_code"] = True
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()
        self.assertTrue(config.enable_execute_code)

    def test_enable_execute_code_invalid_json_fallback_to_settings(self):
        """
        作用：执行测试用例 `enable execute code invalid json fallback to settings`，验证目标行为在回归场景下是否符合预期。
        用途：执行测试用例 `enable execute code invalid json fallback to settings`，验证目标行为在回归场景下是否符合预期。
        使用场景：在 processingmcpserver 自动化测试套件执行阶段由 unittest 运行器调用，用于回归验证。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.settings.setValue("Processing/MCP/enable_execute_code", True)
        payload = default_processing_mcp_json_document()
        payload["processing_mcp"]["enable_execute_code"] = "invalid"
        self._write_json_config(payload)

        config = load_processing_mcp_server_config()

        self.assertTrue(config.enable_execute_code)
        self.assertEqual(config.config_sources.get("enable_execute_code"), "Settings")
