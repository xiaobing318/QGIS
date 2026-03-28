"""Processing MCP server test package."""


def run_from_qgis_console(*args, **kwargs):
    """Run the test suite from the QGIS Python Console."""
    from .suite_runner import run_from_qgis_console as _runner

    return _runner(*args, **kwargs)


__all__ = ["run_from_qgis_console"]
