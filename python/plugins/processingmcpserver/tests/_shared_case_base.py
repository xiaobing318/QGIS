from __future__ import annotations

import gc
import json
import shutil
import sys
import tempfile
import time
from pathlib import Path

from qgis.core import (
    QgsFeature,
    QgsGeometry,
    QgsProject,
    QgsRasterLayer,
    QgsSettings,
    QgsVectorLayer,
)
from qgis.testing import QgisTestCase

from processingmcpserver import dependency_manager as dependency_runtime
from processingmcpserver.config import (
    ProcessingMCPServerConfig,
    ProcessingMCPServerDependencies,
    processing_mcp_config_file_path,
)
from processingmcpserver.mcp_tools import ProcessingMCPTools

from ._shared_fixtures import DummyRunner, _ensure_qgis_test_app


class ProcessingMCPTestBase(QgisTestCase):
    """Shared base for processingmcpserver tests."""

    def setUp(self) -> None:
        _ensure_qgis_test_app()
        self.settings = QgsSettings()
        self._old_settings = self._backup_processing_mcp_settings()
        self.settings.remove("Processing/MCP")
        self._project_layer_ids_before = set(QgsProject.instance().mapLayers().keys())

        self.config_path = processing_mcp_config_file_path()
        self._old_config_text = None
        self._old_config_exists = self.config_path.exists()
        if self._old_config_exists:
            self._old_config_text = self.config_path.read_text(encoding="utf-8")
        if self.config_path.exists():
            self.config_path.unlink()

        self._tmp_dirs: list[tempfile.TemporaryDirectory] = []

    def tearDown(self) -> None:
        cleanup_errors: list[str] = []
        try:
            self._cleanup_new_project_layers()
            self._assert_no_new_project_layers()
            self._cleanup_temp_dirs_with_retry(cleanup_errors)
        finally:
            self.settings.remove("Processing/MCP")
            self._restore_processing_mcp_settings(self._old_settings)

            if self._old_config_exists and self._old_config_text is not None:
                self.config_path.parent.mkdir(parents=True, exist_ok=True)
                self.config_path.write_text(self._old_config_text, encoding="utf-8")
            elif self.config_path.exists():
                self.config_path.unlink()

        if cleanup_errors:
            raise AssertionError("临时目录清理失败: " + "; ".join(cleanup_errors))

    def make_temp_dir(self) -> Path:
        temp_dir = tempfile.TemporaryDirectory()
        self._tmp_dirs.append(temp_dir)
        return Path(temp_dir.name)

    def _backup_processing_mcp_settings(self) -> dict[str, object]:
        self.settings.beginGroup("Processing/MCP")
        old_settings = {key: self.settings.value(key) for key in self.settings.allKeys()}
        self.settings.endGroup()
        return old_settings

    def _restore_processing_mcp_settings(self, old_settings: dict[str, object]) -> None:
        for key, value in old_settings.items():
            self.settings.setValue(f"Processing/MCP/{key}", value)

    def _write_json_config(self, payload: dict) -> None:
        self.config_path.parent.mkdir(parents=True, exist_ok=True)
        self.config_path.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
        )

    def _write_raw_json_text(self, content: str) -> None:
        self.config_path.parent.mkdir(parents=True, exist_ok=True)
        self.config_path.write_text(content, encoding="utf-8")

    def _build_config(self, transport: str) -> ProcessingMCPServerConfig:
        return ProcessingMCPServerConfig(
            enabled=True,
            transport=transport,
            host="127.0.0.1",
            port=8000,
            mount_path="/",
            sse_path="/sse",
            message_path="/messages/",
            streamable_http_path="/mcp",
            stateless_http=True,
            json_response=True,
            log_level="INFO",
            cors_origins=None,
            cors_allow_headers=None,
            enable_execute_code=False,
            dependencies=ProcessingMCPServerDependencies(),
        )

    @staticmethod
    def build_env_snapshot() -> dependency_runtime.PythonEnvironmentSnapshot:
        return dependency_runtime.PythonEnvironmentSnapshot(
            platform_system="Windows",
            platform_release="10",
            platform_machine="x86_64",
            python_executable=sys.executable,
            python_version="3.12.0",
            python_prefix="C:/Python",
            python_base_prefix="C:/Python",
            python_exec_prefix="C:/Python",
            is_virtual_environment=False,
            site_packages=["C:/Python/Lib/site-packages"],
            user_site_packages="C:/Users/test/AppData/Roaming/Python/site-packages",
            environment_error="",
        )

    def build_tools(self) -> ProcessingMCPTools:
        return ProcessingMCPTools(iface=None, runner=DummyRunner())

    def _cleanup_new_project_layers(self) -> None:
        project = QgsProject.instance()
        current_ids = set(project.mapLayers().keys())
        for layer_id in current_ids - self._project_layer_ids_before:
            project.removeMapLayer(layer_id)
        gc.collect()

    def _assert_no_new_project_layers(self) -> None:
        current_ids = set(QgsProject.instance().mapLayers().keys())
        leaked = sorted(current_ids - self._project_layer_ids_before)
        self.assertEqual(leaked, [], msg=f"存在未清理的测试图层: {leaked}")

    def _cleanup_temp_dirs_with_retry(self, errors: list[str]) -> None:
        for temp_dir in self._tmp_dirs:
            last_error: Exception | None = None
            for attempt in range(1, 6):
                try:
                    temp_dir.cleanup()
                    last_error = None
                    break
                except PermissionError as exc:
                    last_error = exc
                    if attempt < 5:
                        time.sleep(0.1)
                    continue
                except Exception as exc:  # pragma: no cover
                    last_error = exc
                    break
            if last_error is not None:
                errors.append(f"{temp_dir.name}: {last_error}")
        self._tmp_dirs = []

    def data_dir(self) -> Path:
        candidates = [Path(__file__).resolve().parent / "data"]
        for candidate in candidates:
            if candidate.exists():
                return candidate
        raise AssertionError("未找到测试数据目录: tests/data")

    def sample_vector_path(self) -> Path:
        vector_path = self.data_dir() / "sample_vector.geojson"
        if vector_path.exists():
            return vector_path
        raise AssertionError("未找到测试数据文件: sample_vector.geojson")

    def add_sample_vector_layer(
        self, layer_name: str = "processingmcpserver_test_vector"
    ) -> QgsVectorLayer:
        layer = QgsVectorLayer(
            "Point?crs=EPSG:4326&field=name:string(32)&field=value:double&field=category:string(16)",
            layer_name,
            "memory",
        )
        self.assertTrue(layer.isValid())
        provider = layer.dataProvider()
        features: list[QgsFeature] = []
        samples = [
            ("alpha", 1.0, "g1", "POINT(108.9000 34.2000)"),
            ("beta", 2.0, "g1", "POINT(108.9100 34.2100)"),
            ("gamma", 3.0, "g2", "POINT(108.9200 34.2200)"),
        ]
        for name, value, category, wkt in samples:
            feature = QgsFeature(layer.fields())
            feature.setAttributes([name, value, category])
            feature.setGeometry(QgsGeometry.fromWkt(wkt))
            features.append(feature)

        add_result = provider.addFeatures(features)
        if isinstance(add_result, tuple):
            self.assertTrue(add_result[0])
        else:
            self.assertTrue(add_result)
        layer.updateExtents()
        QgsProject.instance().addMapLayer(layer)
        return layer

    def add_sample_polygon_layer(
        self, layer_name: str = "processingmcpserver_test_polygon"
    ) -> QgsVectorLayer:
        layer = QgsVectorLayer(
            "Polygon?crs=EPSG:4326&field=name:string(32)",
            layer_name,
            "memory",
        )
        self.assertTrue(layer.isValid())
        provider = layer.dataProvider()
        feature = QgsFeature(layer.fields())
        feature.setAttributes(["poly-1"])
        feature.setGeometry(
            QgsGeometry.fromWkt(
                "POLYGON((108.8 34.1,109.0 34.1,109.0 34.3,108.8 34.3,108.8 34.1))"
            )
        )
        add_result = provider.addFeatures([feature])
        if isinstance(add_result, tuple):
            self.assertTrue(add_result[0])
        else:
            self.assertTrue(add_result)
        layer.updateExtents()
        QgsProject.instance().addMapLayer(layer)
        return layer

    def sample_raster_path(self) -> Path:
        candidates = [
            self.data_dir() / "dem.tif",
            Path(__file__).resolve().parents[4] / "tests" / "testdata" / "analysis" / "dem.tif",
            Path(__file__).resolve().parents[4]
            / "python"
            / "plugins"
            / "processing"
            / "tests"
            / "testdata"
            / "dem.tif",
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate
        raise AssertionError("未找到可用的 DEM 测试数据（dem.tif）")

    def add_sample_raster_layer(
        self, layer_name: str = "processingmcpserver_test_dem"
    ) -> QgsRasterLayer:
        raster_path = self.sample_raster_path()
        layer = QgsRasterLayer(str(raster_path), layer_name, "gdal")
        self.assertTrue(layer.isValid())
        QgsProject.instance().addMapLayer(layer)
        return layer

    def create_vector_geojson_file(self, filename: str = "sample.geojson") -> Path:
        temp_root = self.make_temp_dir()
        target = temp_root / filename
        target.write_text(
            json.dumps(
                {
                    "type": "FeatureCollection",
                    "features": [
                        {
                            "type": "Feature",
                            "properties": {"name": "alpha", "value": 1},
                            "geometry": {
                                "type": "Point",
                                "coordinates": [108.9, 34.2],
                            },
                        }
                    ],
                },
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )
        return target

    def copy_test_data_file(
        self, source_name: str, target_dir: Path, target_name: str | None = None
    ) -> Path:
        source = self.data_dir() / source_name
        if not source.exists():
            raise AssertionError(f"未找到测试数据文件: {source_name}")
        target_dir.mkdir(parents=True, exist_ok=True)
        target = target_dir / (target_name or source_name)
        shutil.copy2(source, target)
        return target

    def create_text_file(self, path: Path, content: str) -> Path:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path


