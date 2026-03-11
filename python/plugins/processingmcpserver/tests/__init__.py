"""Processing MCP server test package."""


def run_from_qgis_console(*args, **kwargs):
    """在 QGIS Python Console 中运行测试套件。"""
    from .suite_runner import run_from_qgis_console as _runner

    return _runner(*args, **kwargs)


__all__ = ["run_from_qgis_console"]
