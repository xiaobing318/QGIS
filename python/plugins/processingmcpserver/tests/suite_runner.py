"""Aggregate and execute the processingmcpserver test modules as one suite."""

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


TEST_MODULES: list[str] = []

IGNORED_TEST_FILES = {
    "__init__.py",
    "_shared_case_base.py",
    "_shared_fixtures.py",
    "suite_runner.py",
}


def _discover_test_modules() -> list[str]:
    """
    作用：封装内部辅助步骤 `_discover_test_modules`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_discover_test_modules`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数：无。
    - 返回：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
    """
    tests_dir = Path(__file__).resolve().parent
    return sorted(
        f"processingmcpserver.tests.{path.stem}"
        for path in tests_dir.glob("*.py")
        if path.name not in IGNORED_TEST_FILES
    )


def _validate_test_modules() -> None:
    """
    作用：封装内部辅助步骤 `_validate_test_modules`，用于拆分并复用模块内重复处理逻辑。
    用途：封装内部辅助步骤 `_validate_test_modules`，用于拆分并复用模块内重复处理逻辑。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数：无。
    - 返回：无返回值。
    返回结果：无返回值。
    异常：可能显式抛出 `RuntimeError`。
    """
    # 使用动态发现，避免工具扩展时遗漏测试模块。
    return


def build_suite() -> unittest.TestSuite:
    """
    作用：构建 `suite`，完成当前函数负责的处理步骤并产出结果。
    用途：构建 `suite`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数：无。
    - 返回：返回 `unittest.TestSuite` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `unittest.TestSuite` 类型结果，返回值语义遵循该函数实现约定。
    """
    _validate_test_modules()
    loader = unittest.defaultTestLoader
    suite = unittest.TestSuite()
    modules = _discover_test_modules()
    for module_name in modules:
        module = importlib.import_module(module_name)
        suite.addTests(loader.loadTestsFromModule(module))
    return suite


def run_from_qgis_console(verbosity: int = 2) -> unittest.result.TestResult:
    """
    作用：运行 `from_qgis_console`，完成当前函数负责的处理步骤并产出结果。
    用途：运行 `from_qgis_console`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数 `verbosity`（`int`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `2`。
    - 返回：返回 `unittest.result.TestResult` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `unittest.result.TestResult` 类型结果，返回值语义遵循该函数实现约定。
    """
    _ensure_qgis_test_app()
    runner = unittest.TextTestRunner(stream=sys.stdout, verbosity=verbosity)
    return runner.run(build_suite())


def load_tests(
    _loader: unittest.TestLoader,
    _standard_tests: unittest.TestSuite,
    _pattern: str | None,
) -> unittest.TestSuite:
    """
    作用：加载 `tests`，完成当前函数负责的处理步骤并产出结果。
    用途：加载 `tests`，完成当前函数负责的处理步骤并产出结果。
    使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
    参数与返回：
    - 参数 `_loader`（`unittest.TestLoader`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `_standard_tests`（`unittest.TestSuite`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 参数 `_pattern`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
    - 返回：返回 `unittest.TestSuite` 类型结果，返回值语义遵循该函数实现约定。
    返回结果：返回 `unittest.TestSuite` 类型结果，返回值语义遵循该函数实现约定。
    """
    return build_suite()


if __name__ == "__main__":
    result = run_from_qgis_console()
    raise SystemExit(0 if result.wasSuccessful() else 1)
