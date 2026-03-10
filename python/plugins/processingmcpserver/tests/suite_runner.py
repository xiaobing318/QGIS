from __future__ import annotations

import importlib
import sys
import unittest
from pathlib import Path

if __package__ in (None, ""):
    tests_dir = Path(__file__).resolve().parent
    plugins_root = tests_dir.parent.parent
    if str(plugins_root) not in sys.path:
        sys.path.insert(0, str(plugins_root))
    from processingmcpserver.tests._shared_fixtures import _ensure_qgis_test_app
else:
    from ._shared_fixtures import _ensure_qgis_test_app


TEST_MODULES = [
    "processingmcpserver.tests.config_loading_precedence",
    "processingmcpserver.tests.config_dependencies_flags",
    "processingmcpserver.tests.registrations_integrity",
    "processingmcpserver.tests.transports_contracts",
    "processingmcpserver.tests.log_handler_runtime",
    "processingmcpserver.tests.mcp_main_thread_runner_runtime",
    "processingmcpserver.tests.plugin_runtime",
    "processingmcpserver.tests.server_runtime",
    "processingmcpserver.tests.dependencies_runtime",
    "processingmcpserver.tests.mcp_tools_normalizers",
    "processingmcpserver.tests.mcp_tools_common_get_qgis_info",
    "processingmcpserver.tests.mcp_tools_vector_add_layer",
    "processingmcpserver.tests.mcp_tools_vector_add_layers",
    "processingmcpserver.tests.mcp_tools_raster_add_layer",
    "processingmcpserver.tests.mcp_tools_raster_add_layers",
    "processingmcpserver.tests.mcp_tools_layer_list",
    "processingmcpserver.tests.mcp_tools_layer_get_panel_tree",
    "processingmcpserver.tests.mcp_tools_layer_get_details",
    "processingmcpserver.tests.mcp_tools_layer_remove",
    "processingmcpserver.tests.mcp_tools_layer_remove_batch",
    "processingmcpserver.tests.mcp_tools_layer_resolve_references",
    "processingmcpserver.tests.mcp_tools_vector_get_layer_features",
    "processingmcpserver.tests.mcp_tools_vector_table_add_field",
    "processingmcpserver.tests.mcp_tools_vector_table_drop_fields",
    "processingmcpserver.tests.mcp_tools_vector_table_rename_field",
    "processingmcpserver.tests.mcp_tools_vector_table_calculate_field",
    "processingmcpserver.tests.mcp_tools_vector_table_query_records",
    "processingmcpserver.tests.mcp_tools_vector_table_insert_records",
    "processingmcpserver.tests.mcp_tools_vector_table_update_records",
    "processingmcpserver.tests.mcp_tools_vector_table_delete_records",
    "processingmcpserver.tests.mcp_tools_vector_table_truncate",
    "processingmcpserver.tests.mcp_tools_vector_stats_basic",
    "processingmcpserver.tests.mcp_tools_vector_stats_by_categories",
    "processingmcpserver.tests.mcp_tools_raster_stats_basic",
    "processingmcpserver.tests.mcp_tools_raster_stats_zonal",
    "processingmcpserver.tests.mcp_tools_raster_stats_cell",
    "processingmcpserver.tests.mcp_tools_dataset_list_files",
    "processingmcpserver.tests.mcp_tools_dataset_load_from_directory",
    "processingmcpserver.tests.mcp_tools_filesystem_query_list_entries",
    "processingmcpserver.tests.mcp_tools_filesystem_query_entry_info",
    "processingmcpserver.tests.mcp_tools_filesystem_query_read_text",
    "processingmcpserver.tests.mcp_tools_filesystem_edit_write_text",
    "processingmcpserver.tests.mcp_tools_filesystem_edit_append_text",
    "processingmcpserver.tests.mcp_tools_filesystem_edit_copy_entry",
    "processingmcpserver.tests.mcp_tools_filesystem_edit_move_entry",
    "processingmcpserver.tests.mcp_tools_filesystem_edit_delete_entry",
    "processingmcpserver.tests.mcp_tools_filesystem_stats_directory",
    "processingmcpserver.tests.mcp_tools_processing_list_providers",
    "processingmcpserver.tests.mcp_tools_processing_get_algorithms",
    "processingmcpserver.tests.mcp_tools_processing_get_parameter_template",
    "processingmcpserver.tests.mcp_tools_processing_execute_algorithm",
    "processingmcpserver.tests.mcp_tools_processing_execute_on_layers",
    "processingmcpserver.tests.mcp_prompts_qgis_task_planner",
    "processingmcpserver.tests.mcp_prompts_qgis_layer_health_check",
    "processingmcpserver.tests.mcp_resources_qgis_project_info",
    "processingmcpserver.tests.mcp_resources_qgis_project_layers_summary",
]

IGNORED_TEST_FILES = {
    "__init__.py",
    "_shared_case_base.py",
    "_shared_fixtures.py",
    "suite_runner.py",
}


def _discover_test_modules() -> list[str]:
    tests_dir = Path(__file__).resolve().parent
    return sorted(
        f"processingmcpserver.tests.{path.stem}"
        for path in tests_dir.glob("*.py")
        if path.name not in IGNORED_TEST_FILES
    )


def _validate_test_modules() -> None:
    discovered = _discover_test_modules()
    declared = sorted(TEST_MODULES)
    if discovered == declared:
        return

    missing = sorted(set(discovered) - set(TEST_MODULES))
    extra = sorted(set(TEST_MODULES) - set(discovered))
    details: list[str] = []
    if missing:
        details.append("missing=" + ", ".join(missing))
    if extra:
        details.append("extra=" + ", ".join(extra))
    raise RuntimeError(
        "TEST_MODULES is out of sync with tests directory: " + "; ".join(details)
    )


def build_suite() -> unittest.TestSuite:
    """Build full suite explicitly so filenames are unconstrained."""
    _validate_test_modules()
    loader = unittest.defaultTestLoader
    suite = unittest.TestSuite()
    for module_name in TEST_MODULES:
        module = importlib.import_module(module_name)
        suite.addTests(loader.loadTestsFromModule(module))
    return suite


def run_from_qgis_console(verbosity: int = 2) -> unittest.result.TestResult:
    """Run full processingmcpserver test suite from QGIS Python Console."""
    _ensure_qgis_test_app()
    runner = unittest.TextTestRunner(stream=sys.stdout, verbosity=verbosity)
    return runner.run(build_suite())


def load_tests(
    _loader: unittest.TestLoader,
    _standard_tests: unittest.TestSuite,
    _pattern: str | None,
) -> unittest.TestSuite:
    """Expose aggregated suite for `python -m unittest ...`."""
    return build_suite()


if __name__ == "__main__":
    result = run_from_qgis_console()
    raise SystemExit(0 if result.wasSuccessful() else 1)
