from __future__ import annotations

import subprocess
from pathlib import Path
from unittest.mock import patch

from processingmcpserver import dependency_manager as dependency_runtime
from processingmcpserver.config import ProcessingMCPServerDependencies

from ._shared_case_base import ProcessingMCPTestBase


class DependenciesRuntimeTest(ProcessingMCPTestBase):
    def test_dependencies_support_requirement_syntax(self):
        """Verify dependencies support requirement syntax."""
        parsed, error = dependency_runtime._parse_requirement("mcp>=0.0.1")
        self.assertIsNotNone(parsed)
        self.assertEqual(error, "")
        self.assertEqual(parsed.name, "mcp")

    @patch("processingmcpserver.dependency_evaluator.importlib.metadata.version")
    def test_dependencies_detect_version_mismatch(self, mock_version):
        """Verify dependencies detect version mismatch."""
        mock_version.return_value = "0.1.0"
        result = dependency_runtime._evaluate_requirement("mcp>=1.0.0")
        self.assertFalse(result.satisfied)
        self.assertIn("does not satisfy", result.reason)

    def test_requirements_file_parser_skips_comments_and_blank_lines(self):
        """Verify requirements file parser skips comments and blank lines."""
        temp_root = self.make_temp_dir()
        req_path = temp_root / "requirements.txt"
        req_path.write_text(
            (
                "# comment line\n"
                "mcp>=1.13.0,<2.0.0\n"
                "  \n"
                "uvicorn>=0.31.1,<1.0.0 # inline comment\n"
                "MCP>=1.13.0,<2.0.0\n"
            ),
            encoding="utf-8",
        )
        result = dependency_runtime._load_requirements_from_file(req_path)

        self.assertTrue(result.read_success)
        self.assertEqual(
            result.requirements,
            ["mcp>=1.13.0,<2.0.0", "uvicorn>=0.31.1,<1.0.0"],
        )

    def test_requirements_file_missing_returns_error(self):
        """Verify requirements file missing returns error."""
        temp_root = self.make_temp_dir()
        req_path = temp_root / "missing.txt"
        result = dependency_runtime._load_requirements_from_file(req_path)

        self.assertFalse(result.read_success)
        self.assertEqual(result.requirements, [])
        self.assertIn("file not found", result.error)

    @patch("processingmcpserver.dependency_manager._log_result")
    @patch("processingmcpserver.dependency_manager._write_dependency_report")
    @patch("processingmcpserver.dependency_manager.subprocess.run")
    @patch("processingmcpserver.dependency_manager._evaluate_requirements")
    @patch("processingmcpserver.dependency_manager._detect_pip")
    @patch("processingmcpserver.dependency_manager._collect_python_environment")
    @patch("processingmcpserver.dependency_manager._load_requirements_from_file")
    @patch("processingmcpserver.dependency_manager._validate_mcp_runtime_contract")
    def test_dependencies_install_command_constructed_for_missing_requirements(
        self,
        mock_validate_contract,
        mock_load_requirements,
        mock_collect_env,
        mock_detect_pip,
        mock_evaluate_requirements,
        mock_subprocess_run,
        _mock_write_report,
        _mock_log_result,
    ):
        """Verify dependencies install command constructed for missing requirements."""
        mock_collect_env.return_value = self.build_env_snapshot()
        mock_detect_pip.return_value = dependency_runtime.PipDetectionResult(
            available=True,
            version="pip 24.0",
            python_executable="C:/Python/python.exe",
            python_source="sys.prefix",
            error="",
        )
        mock_load_requirements.return_value = dependency_runtime.RequirementsFileLoadResult(
            path="C:/fake/requirements.txt",
            read_success=True,
            requirements=["mcp", "uvicorn"],
            error="",
        )
        mock_validate_contract.return_value = (True, "")
        mock_evaluate_requirements.side_effect = [
            {
                "mcp": dependency_runtime.RequirementEvaluation(
                    requirement="mcp",
                    distribution_name="mcp",
                    installed_version="",
                    satisfied=False,
                    reason="Distribution 'mcp' is not installed.",
                ),
                "uvicorn": dependency_runtime.RequirementEvaluation(
                    requirement="uvicorn",
                    distribution_name="uvicorn",
                    installed_version="",
                    satisfied=False,
                    reason="Distribution 'uvicorn' is not installed.",
                ),
            },
            {
                "mcp": dependency_runtime.RequirementEvaluation(
                    requirement="mcp",
                    distribution_name="mcp",
                    installed_version="1.0.0",
                    satisfied=True,
                    reason="",
                ),
                "uvicorn": dependency_runtime.RequirementEvaluation(
                    requirement="uvicorn",
                    distribution_name="uvicorn",
                    installed_version="0.31.1",
                    satisfied=True,
                    reason="",
                ),
            },
        ]
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout="ok",
            stderr="",
        )

        result = dependency_runtime.ensure_processing_mcp_dependencies(
            ProcessingMCPServerDependencies(auto_install=True)
        )

        self.assertTrue(result.install_attempted)
        self.assertEqual(
            result.install_command,
            [
                "C:/Python/python.exe",
                "-m",
                "pip",
                "install",
                "--disable-pip-version-check",
                "--no-input",
                "mcp",
                "uvicorn",
            ],
        )
        self.assertEqual(result.unsatisfied_before, ["mcp", "uvicorn"])
        self.assertEqual(result.unsatisfied_after, [])
        self.assertTrue(result.success)
        self.assertEqual(result.requirements_file_path, "C:/fake/requirements.txt")
        self.assertTrue(result.requirements_file_read)
        self.assertTrue(result.mcp_runtime_contract_ok)

    @patch("processingmcpserver.dependency_manager._log_result")
    @patch("processingmcpserver.dependency_manager._write_dependency_report")
    @patch("processingmcpserver.dependency_manager._evaluate_requirements")
    @patch("processingmcpserver.dependency_manager._detect_pip")
    @patch("processingmcpserver.dependency_manager._collect_python_environment")
    @patch("processingmcpserver.dependency_manager._load_requirements_from_file")
    def test_dependencies_fail_when_pip_unavailable(
        self,
        mock_load_requirements,
        mock_collect_env,
        mock_detect_pip,
        mock_evaluate_requirements,
        _mock_write_report,
        _mock_log_result,
    ):
        """Verify dependencies fail when pip unavailable."""
        mock_collect_env.return_value = self.build_env_snapshot()
        mock_detect_pip.return_value = dependency_runtime.PipDetectionResult(
            available=False,
            version="",
            python_executable="C:/Python/python.exe",
            python_source="sys.prefix",
            error="pip probe failed",
        )
        mock_load_requirements.return_value = dependency_runtime.RequirementsFileLoadResult(
            path="C:/fake/requirements.txt",
            read_success=True,
            requirements=["mcp"],
            error="",
        )
        evaluations = {
            "mcp": dependency_runtime.RequirementEvaluation(
                requirement="mcp",
                distribution_name="mcp",
                installed_version="",
                satisfied=False,
                reason="Distribution 'mcp' is not installed.",
            )
        }
        mock_evaluate_requirements.side_effect = [evaluations, evaluations]

        result = dependency_runtime.ensure_processing_mcp_dependencies(
            ProcessingMCPServerDependencies(auto_install=True)
        )

        self.assertFalse(result.install_attempted)
        self.assertFalse(result.pip_available)
        self.assertFalse(result.success)
        self.assertIn("pip is unavailable", result.failure_reason)

    @patch("processingmcpserver.dependency_manager._log_result")
    @patch("processingmcpserver.dependency_manager._write_dependency_report")
    @patch("processingmcpserver.dependency_manager._evaluate_requirements")
    @patch("processingmcpserver.dependency_manager._detect_pip")
    @patch("processingmcpserver.dependency_manager._collect_python_environment")
    @patch("processingmcpserver.dependency_manager._load_requirements_from_file")
    @patch("processingmcpserver.dependency_manager._validate_mcp_runtime_contract")
    def test_dependencies_report_schema_v3(
        self,
        mock_validate_contract,
        mock_load_requirements,
        mock_collect_env,
        mock_detect_pip,
        mock_evaluate_requirements,
        _mock_write_report,
        _mock_log_result,
    ):
        """Verify dependencies report schema v3."""
        mock_collect_env.return_value = self.build_env_snapshot()
        mock_detect_pip.return_value = dependency_runtime.PipDetectionResult(
            available=True,
            version="pip 24.0",
            python_executable="C:/Python/python.exe",
            python_source="sys.prefix",
            command=["C:/Python/python.exe", "-m", "pip", "--version"],
            stdout="pip 24.0 from C:/Python/Lib/site-packages/pip (python 3.12)",
            stderr="",
            error="",
        )
        mock_load_requirements.return_value = dependency_runtime.RequirementsFileLoadResult(
            path="C:/fake/requirements.txt",
            read_success=True,
            requirements=["mcp"],
            error="",
        )
        mock_validate_contract.return_value = (True, "")
        evaluations = {
            "mcp": dependency_runtime.RequirementEvaluation(
                requirement="mcp",
                distribution_name="mcp",
                installed_version="1.0.0",
                satisfied=True,
                reason="",
            )
        }
        mock_evaluate_requirements.side_effect = [evaluations, evaluations]

        result = dependency_runtime.ensure_processing_mcp_dependencies(
            ProcessingMCPServerDependencies(auto_install=True)
        )

        self.assertEqual(result.report_schema_version, 3)
        self.assertEqual(result.requirements_file_path, "C:/fake/requirements.txt")
        self.assertTrue(result.requirements_file_read)
        self.assertEqual(result.requirements_file_error, "")
        self.assertTrue(result.mcp_runtime_contract_ok)
        self.assertEqual(result.mcp_runtime_contract_error, "")
        self.assertEqual(result.pip_python_executable, "C:/Python/python.exe")
        self.assertEqual(result.pip_python_source, "sys.prefix")
        self.assertEqual(
            result.pip_probe_command, ["C:/Python/python.exe", "-m", "pip", "--version"]
        )
        self.assertIn("pip 24.0", result.pip_probe_stdout)
        self.assertTrue(result.install_target_prefix)
        self.assertTrue(result.install_target_site_packages)

    @patch("processingmcpserver.dependency_manager._log_result")
    @patch("processingmcpserver.dependency_manager._write_dependency_report")
    @patch("processingmcpserver.dependency_manager._evaluate_requirements")
    @patch("processingmcpserver.dependency_manager._detect_pip")
    @patch("processingmcpserver.dependency_manager._collect_python_environment")
    @patch("processingmcpserver.dependency_manager._load_requirements_from_file")
    @patch("processingmcpserver.dependency_manager._validate_mcp_runtime_contract")
    def test_dependencies_fail_when_mcp_contract_invalid(
        self,
        mock_validate_contract,
        mock_load_requirements,
        mock_collect_env,
        mock_detect_pip,
        mock_evaluate_requirements,
        _mock_write_report,
        _mock_log_result,
    ):
        """Verify dependencies fail when MCP contract invalid."""
        mock_collect_env.return_value = self.build_env_snapshot()
        mock_detect_pip.return_value = dependency_runtime.PipDetectionResult(
            available=True,
            version="pip 24.0",
            python_executable="C:/Python/python.exe",
            python_source="sys.prefix",
            error="",
        )
        mock_load_requirements.return_value = dependency_runtime.RequirementsFileLoadResult(
            path="C:/fake/requirements.txt",
            read_success=True,
            requirements=["mcp"],
            error="",
        )
        evaluations = {
            "mcp": dependency_runtime.RequirementEvaluation(
                requirement="mcp",
                distribution_name="mcp",
                installed_version="1.20.0",
                satisfied=True,
                reason="",
            )
        }
        mock_evaluate_requirements.side_effect = [evaluations, evaluations]
        mock_validate_contract.return_value = (
            False,
            "FastMCP is missing required callable members: streamable_http_app",
        )

        result = dependency_runtime.ensure_processing_mcp_dependencies(
            ProcessingMCPServerDependencies(auto_install=True)
        )

        self.assertFalse(result.success)
        self.assertFalse(result.mcp_runtime_contract_ok)
        self.assertIn("MCP runtime contract validation failed", result.failure_reason)
        self.assertIn("streamable_http_app", result.failure_reason)

    @patch("processingmcpserver.dependency_manager._log_result")
    @patch("processingmcpserver.dependency_manager._write_dependency_report")
    @patch("processingmcpserver.dependency_manager._detect_pip")
    @patch("processingmcpserver.dependency_manager._collect_python_environment")
    @patch("processingmcpserver.dependency_manager._load_requirements_from_file")
    def test_dependencies_fail_when_requirements_file_missing(
        self,
        mock_load_requirements,
        mock_collect_env,
        mock_detect_pip,
        _mock_write_report,
        _mock_log_result,
    ):
        """Verify dependencies fail when requirements file missing."""
        mock_collect_env.return_value = self.build_env_snapshot()
        mock_detect_pip.return_value = dependency_runtime.PipDetectionResult(
            available=True,
            version="pip 24.0",
            python_executable="C:/Python/python.exe",
            python_source="sys.prefix",
            error="",
        )
        mock_load_requirements.return_value = dependency_runtime.RequirementsFileLoadResult(
            path="C:/fake/requirements.txt",
            read_success=False,
            requirements=[],
            error="file not found",
        )

        result = dependency_runtime.ensure_processing_mcp_dependencies(
            ProcessingMCPServerDependencies(auto_install=True)
        )

        self.assertFalse(result.success)
        self.assertIn("Failed to read", result.failure_reason)
        self.assertEqual(result.requested_requirements, [])
        self.assertFalse(result.install_attempted)

    @patch("processingmcpserver.dependency_manager._log_result")
    @patch("processingmcpserver.dependency_manager._write_dependency_report")
    @patch("processingmcpserver.dependency_manager._detect_pip")
    @patch("processingmcpserver.dependency_manager._collect_python_environment")
    @patch("processingmcpserver.dependency_manager._load_requirements_from_file")
    def test_dependencies_fail_when_requirements_file_empty(
        self,
        mock_load_requirements,
        mock_collect_env,
        mock_detect_pip,
        _mock_write_report,
        _mock_log_result,
    ):
        """Verify dependencies fail when requirements file empty."""
        mock_collect_env.return_value = self.build_env_snapshot()
        mock_detect_pip.return_value = dependency_runtime.PipDetectionResult(
            available=True,
            version="pip 24.0",
            python_executable="C:/Python/python.exe",
            python_source="sys.prefix",
            error="",
        )
        mock_load_requirements.return_value = dependency_runtime.RequirementsFileLoadResult(
            path="C:/fake/requirements.txt",
            read_success=True,
            requirements=[],
            error="",
        )

        result = dependency_runtime.ensure_processing_mcp_dependencies(
            ProcessingMCPServerDependencies(auto_install=True)
        )

        self.assertFalse(result.success)
        self.assertIn("does not contain any requirement entries", result.failure_reason)
        self.assertEqual(result.requested_requirements, [])
        self.assertFalse(result.install_attempted)

    @patch("processingmcpserver.dependency_probe.subprocess.run")
    def test_detect_pip_rejects_non_pip_output_even_on_success(self, mock_run):
        """Verify detect pip rejects non pip output even on success."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout="QGIS 3.44.7-Solothurn",
            stderr="",
        )

        result = dependency_runtime._detect_pip(
            "C:/Python/python.exe", "sys.prefix", ""
        )

        self.assertFalse(result.available)
        self.assertIn("does not contain 'pip '", result.error)

    @patch("processingmcpserver.dependency_manager._log_result")
    @patch("processingmcpserver.dependency_manager._write_dependency_report")
    @patch("processingmcpserver.dependency_manager.subprocess.run")
    @patch("processingmcpserver.dependency_manager._evaluate_requirements")
    @patch("processingmcpserver.dependency_manager._detect_pip")
    @patch("processingmcpserver.dependency_manager._collect_python_environment")
    @patch("processingmcpserver.dependency_manager._load_requirements_from_file")
    @patch("processingmcpserver.dependency_manager._validate_mcp_runtime_contract")
    def test_dependencies_fail_without_user_fallback_on_permission_denied(
        self,
        mock_validate_contract,
        mock_load_requirements,
        mock_collect_env,
        mock_detect_pip,
        mock_evaluate_requirements,
        mock_subprocess_run,
        _mock_write_report,
        _mock_log_result,
    ):
        """Verify dependencies fail without user fallback on permission denied."""
        mock_collect_env.return_value = self.build_env_snapshot()
        mock_detect_pip.return_value = dependency_runtime.PipDetectionResult(
            available=True,
            version="pip 24.0",
            python_executable="C:/Python/python.exe",
            python_source="sys.prefix",
            error="",
        )
        mock_load_requirements.return_value = dependency_runtime.RequirementsFileLoadResult(
            path="C:/fake/requirements.txt",
            read_success=True,
            requirements=["mcp"],
            error="",
        )
        evaluations = {
            "mcp": dependency_runtime.RequirementEvaluation(
                requirement="mcp",
                distribution_name="mcp",
                installed_version="",
                satisfied=False,
                reason="Distribution 'mcp' is not installed.",
            )
        }
        mock_evaluate_requirements.side_effect = [evaluations, evaluations]
        mock_validate_contract.return_value = (True, "")
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=1,
            stdout="",
            stderr="Access is denied",
        )

        result = dependency_runtime.ensure_processing_mcp_dependencies(
            ProcessingMCPServerDependencies(auto_install=True)
        )

        self.assertFalse(result.success)
        self.assertTrue(result.install_attempted)
        self.assertIn("Access is denied", result.failure_reason)
        self.assertNotIn("--user", result.install_command)

    @patch("processingmcpserver.dependency_probe.sys.executable", "C:/invalid/not-python.txt")
    def test_resolve_pip_python_executable_falls_back_to_prefix_python(self):
        """Verify resolve pip python executable falls back to prefix python."""
        temp_root = self.make_temp_dir()
        python_exe = temp_root / "python.exe"
        python_exe.write_text("", encoding="utf-8")
        snapshot = dependency_runtime.PythonEnvironmentSnapshot(
            platform_system="Windows",
            platform_release="10",
            platform_machine="x86_64",
            python_executable="C:/invalid/not-python.txt",
            python_version="3.12.0",
            python_prefix=str(temp_root),
            python_base_prefix="C:/base-prefix",
            python_exec_prefix="C:/exec-prefix",
            is_virtual_environment=False,
            site_packages=[],
            user_site_packages="",
            environment_error="",
        )

        python_path, source, error = dependency_runtime._resolve_pip_python_executable(
            snapshot
        )

        self.assertEqual(Path(python_path), python_exe.resolve())
        self.assertEqual(source, "sys.prefix")
        self.assertEqual(error, "")

    @patch("processingmcpserver.dependency_probe.subprocess.run")
    def test_resolve_install_target_details_falls_back_on_invalid_probe_output(
        self, mock_run
    ):
        """Verify resolve install target details falls back on invalid probe output."""
        environment = self.build_env_snapshot()
        mock_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout="not-json",
            stderr="",
        )

        prefix, site_packages = dependency_runtime._resolve_install_target_details(
            "C:/Python/python.exe",
            environment,
        )

        self.assertEqual(prefix, environment.python_prefix)
        self.assertEqual(site_packages, environment.site_packages[-1])
