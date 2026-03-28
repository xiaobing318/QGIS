"""Shared base helpers for processingmcpserver integration-style test cases."""

from __future__ import annotations

import gc
import json
import shutil
import sys
import tempfile
import time
from pathlib import Path

from qgis.PyQt.QtCore import QDate, QDateTime, QTime, Qt, QVariant
from qgis.core import (
    QgsFeature,
    QgsField,
    QgsGeometry,
    QgsMapLayer,
    QgsProject,
    QgsRasterLayer,
    QgsSettings,
    QgsVectorLayer,
    QgsVectorFileWriter,
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
        """
        作用：准备当前测试用例运行所需的初始状态与测试资源。
        用途：准备当前测试用例运行所需的初始状态与测试资源。
        使用场景：在每个 unittest 测试用例开始或结束时由测试框架生命周期钩子调用。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
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
        """
        作用：清理当前测试用例创建的资源，避免对后续测试造成污染。
        用途：清理当前测试用例创建的资源，避免对后续测试造成污染。
        使用场景：在每个 unittest 测试用例开始或结束时由测试框架生命周期钩子调用。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        异常：可能显式抛出 `AssertionError`。
        """
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
        """
        作用：实现 `make_temp_dir` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `make_temp_dir` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        """
        temp_dir = tempfile.TemporaryDirectory()
        self._tmp_dirs.append(temp_dir)
        return Path(temp_dir.name)

    def _backup_processing_mcp_settings(self) -> dict[str, object]:
        """
        作用：封装内部辅助步骤 `_backup_processing_mcp_settings`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_backup_processing_mcp_settings`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `dict[str, object]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dict[str, object]` 类型结果，返回值语义遵循该函数实现约定。
        """
        self.settings.beginGroup("Processing/MCP")
        old_settings = {key: self.settings.value(key) for key in self.settings.allKeys()}
        self.settings.endGroup()
        return old_settings

    def _restore_processing_mcp_settings(self, old_settings: dict[str, object]) -> None:
        """
        作用：封装内部辅助步骤 `_restore_processing_mcp_settings`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_restore_processing_mcp_settings`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `old_settings`（`dict[str, object]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        for key, value in old_settings.items():
            self.settings.setValue(f"Processing/MCP/{key}", value)

    def _write_json_config(self, payload: dict) -> None:
        """
        作用：封装内部辅助步骤 `_write_json_config`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_write_json_config`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `payload`（`dict`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.config_path.parent.mkdir(parents=True, exist_ok=True)
        self.config_path.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
        )

    def _write_raw_json_text(self, content: str) -> None:
        """
        作用：封装内部辅助步骤 `_write_raw_json_text`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_write_raw_json_text`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `content`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        self.config_path.parent.mkdir(parents=True, exist_ok=True)
        self.config_path.write_text(content, encoding="utf-8")

    def _build_config(self, transport: str) -> ProcessingMCPServerConfig:
        """
        作用：构建 `_build_config` 相关对象或配置数据，供后续流程直接复用。
        用途：构建 `_build_config` 相关对象或配置数据，供后续流程直接复用。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `transport`（`str`）：数值控制参数，用于限制范围、数量或时限。
        - 返回：返回 `ProcessingMCPServerConfig` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `ProcessingMCPServerConfig` 类型结果，返回值语义遵循该函数实现约定。
        """
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
        """
        作用：实现 `build_env_snapshot` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `build_env_snapshot` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数：无。
        - 返回：返回 `dependency_runtime.PythonEnvironmentSnapshot` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `dependency_runtime.PythonEnvironmentSnapshot` 类型结果，返回值语义遵循该函数实现约定。
        """
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
        """
        作用：实现 `build_tools` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `build_tools` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `ProcessingMCPTools` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `ProcessingMCPTools` 类型结果，返回值语义遵循该函数实现约定。
        """
        return ProcessingMCPTools(
            iface=None,
            runner=DummyRunner(),
            config=self._build_config("streamable-http"),
        )

    def _cleanup_new_project_layers(self) -> None:
        """
        作用：封装内部辅助步骤 `_cleanup_new_project_layers`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_cleanup_new_project_layers`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        project = QgsProject.instance()
        current_ids = set(project.mapLayers().keys())
        for layer_id in current_ids - self._project_layer_ids_before:
            project.removeMapLayer(layer_id)
        gc.collect()

    def _assert_no_new_project_layers(self) -> None:
        """
        作用：封装内部辅助步骤 `_assert_no_new_project_layers`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_assert_no_new_project_layers`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        current_ids = set(QgsProject.instance().mapLayers().keys())
        leaked = sorted(current_ids - self._project_layer_ids_before)
        self.assertEqual(leaked, [], msg=f"存在未清理的测试图层: {leaked}")

    def _cleanup_temp_dirs_with_retry(self, errors: list[str]) -> None:
        """
        作用：封装内部辅助步骤 `_cleanup_temp_dirs_with_retry`，用于拆分并复用模块内重复处理逻辑。
        用途：封装内部辅助步骤 `_cleanup_temp_dirs_with_retry`，用于拆分并复用模块内重复处理逻辑。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `errors`（`list[str]`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：无返回值。
        返回结果：无返回值。
        """
        for temp_dir in self._tmp_dirs:
            last_error: Exception | None = None
            for attempt in range(1, 6):
                try:
                    temp_dir.cleanup()
                    last_error = None
                    break
                except PermissionError as exc:
                    last_error = exc
                    temp_root = Path(temp_dir.name)
                    if temp_root.exists():
                        for shp_path in temp_root.rglob("*.shp"):
                            try:
                                QgsVectorFileWriter.deleteShapeFile(str(shp_path))
                            except Exception:
                                pass
                    gc.collect()
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
        """
        作用：实现 `data_dir` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `data_dir` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        异常：可能显式抛出 `AssertionError`。
        """
        candidates = [Path(__file__).resolve().parent / "data"]
        for candidate in candidates:
            if candidate.exists():
                return candidate
        raise AssertionError("未找到测试数据目录: tests/data")

    def sample_vector_path(self) -> Path:
        """
        作用：实现 `sample_vector_path` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `sample_vector_path` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        异常：可能显式抛出 `AssertionError`。
        """
        vector_path = self.data_dir() / "sample_vector.geojson"
        if vector_path.exists():
            return vector_path
        raise AssertionError("未找到测试数据文件: sample_vector.geojson")

    def add_sample_vector_layer(
        self, layer_name: str = "processingmcpserver_test_vector"
    ) -> QgsVectorLayer:
        """
        作用：实现 `add_sample_vector_layer` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `add_sample_vector_layer` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `layer_name`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"processingmcpserver_test_vector"`。
        - 返回：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
        """
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
        """
        作用：实现 `add_sample_polygon_layer` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `add_sample_polygon_layer` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `layer_name`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"processingmcpserver_test_polygon"`。
        - 返回：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
        """
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

    def add_serialization_vector_layer(
        self, layer_name: str = "processingmcpserver_serialization_vector"
    ) -> QgsVectorLayer:
        """
        作用：实现 `add_serialization_vector_layer` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `add_serialization_vector_layer` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `layer_name`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"processingmcpserver_serialization_vector"`。
        - 返回：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `QgsVectorLayer` 类型结果，返回值语义遵循该函数实现约定。
        """
        layer = QgsVectorLayer(
            "Point?crs=EPSG:4326&field=name:string(32)",
            layer_name,
            "memory",
        )
        self.assertTrue(layer.isValid())
        provider = layer.dataProvider()
        add_fields_result = provider.addAttributes(
            [
                QgsField("event_date", QVariant.Date),
                QgsField("event_time", QVariant.Time),
                QgsField("event_ts", QVariant.DateTime),
                QgsField("enabled", QVariant.Bool),
                QgsField("notes", QVariant.String),
            ]
        )
        if isinstance(add_fields_result, tuple):
            self.assertTrue(add_fields_result[0])
        else:
            self.assertTrue(add_fields_result)
        layer.updateFields()

        feature = QgsFeature(layer.fields())
        feature.setAttributes(
            [
                "complex-row",
                QDate(2026, 3, 10),
                QTime(12, 34, 56, 789),
                QDateTime(QDate(2026, 3, 10), QTime(12, 34, 56, 789), Qt.TimeSpec.UTC),
                True,
                None,
            ]
        )
        feature.setGeometry(QgsGeometry.fromWkt("POINT(108.9300 34.2300)"))
        add_result = provider.addFeatures([feature])
        if isinstance(add_result, tuple):
            self.assertTrue(add_result[0])
        else:
            self.assertTrue(add_result)
        layer.updateExtents()
        QgsProject.instance().addMapLayer(layer)
        return layer

    def sample_raster_path(self) -> Path:
        """
        作用：实现 `sample_raster_path` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `sample_raster_path` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        异常：可能显式抛出 `AssertionError`。
        """
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
        """
        作用：实现 `add_sample_raster_layer` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `add_sample_raster_layer` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `layer_name`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。 默认值为 `"processingmcpserver_test_dem"`。
        - 返回：返回 `QgsRasterLayer` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `QgsRasterLayer` 类型结果，返回值语义遵循该函数实现约定。
        """
        raster_path = self.sample_raster_path()
        layer = QgsRasterLayer(str(raster_path), layer_name, "gdal")
        self.assertTrue(layer.isValid())
        QgsProject.instance().addMapLayer(layer)
        return layer

    def create_vector_geojson_file(self, filename: str = "sample.geojson") -> Path:
        """
        作用：实现 `create_vector_geojson_file` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `create_vector_geojson_file` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `filename`（`str`）：路径类参数，用于定位输入或输出文件系统位置。 默认值为 `"sample.geojson"`。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        """
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
        """
        作用：实现 `copy_test_data_file` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `copy_test_data_file` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `source_name`（`str`）：标识或模式参数，用于指定目标对象或流程分支。
        - 参数 `target_dir`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `target_name`（`str | None`）：标识或模式参数，用于指定目标对象或流程分支。 默认值为 `None`。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        异常：可能显式抛出 `AssertionError`。
        """
        source = self.data_dir() / source_name
        if not source.exists():
            raise AssertionError(f"未找到测试数据文件: {source_name}")
        target_dir.mkdir(parents=True, exist_ok=True)
        target = target_dir / (target_name or source_name)
        shutil.copy2(source, target)
        return target

    def create_text_file(self, path: Path, content: str) -> Path:
        """
        作用：实现 `create_text_file` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `create_text_file` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `content`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        """
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def export_layer_to_shapefile(
        self,
        layer: QgsVectorLayer,
        target_dir: Path,
        stem: str = "sample",
    ) -> Path:
        """
        作用：实现 `export_layer_to_shapefile` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `export_layer_to_shapefile` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
        - 参数 `target_dir`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `stem`（`str`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `"sample"`。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        """
        target_dir.mkdir(parents=True, exist_ok=True)
        target_path = target_dir / f"{stem}.shp"
        if target_path.exists():
            QgsVectorFileWriter.deleteShapeFile(str(target_path))

        options = QgsVectorFileWriter.SaveVectorOptions()
        options.driverName = "ESRI Shapefile"
        options.fileEncoding = "UTF-8"
        options.layerName = stem
        err, msg, new_file_name, _new_layer = QgsVectorFileWriter.writeAsVectorFormatV3(
            layer,
            str(target_path),
            QgsProject.instance().transformContext(),
            options,
        )
        self.assertEqual(err, QgsVectorFileWriter.WriterError.NoError, msg or new_file_name)
        self.assertTrue(target_path.exists())
        return target_path

    def copy_shapefile_bundle(
        self,
        source_path: Path,
        target_dir: Path,
        target_stem: str | None = None,
    ) -> Path:
        """
        作用：实现 `copy_shapefile_bundle` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `copy_shapefile_bundle` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `self`：实例或类上下文对象，用于访问当前方法所在对象状态。
        - 参数 `source_path`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `target_dir`（`Path`）：路径类参数，用于定位输入或输出文件系统位置。
        - 参数 `target_stem`（`str | None`）：业务输入参数，由调用方提供以驱动当前函数逻辑。 默认值为 `None`。
        - 返回：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `Path` 类型结果，返回值语义遵循该函数实现约定。
        异常：可能显式抛出 `AssertionError`。
        """
        if source_path.suffix.lower() != ".shp":
            raise AssertionError("source_path 必须是 .shp 文件")
        if not source_path.exists():
            raise AssertionError(f"未找到 shapefile 源文件: {source_path}")

        target_dir.mkdir(parents=True, exist_ok=True)
        stem = target_stem or source_path.stem
        copied_shp: Path | None = None
        for member in source_path.parent.glob(f"{source_path.stem}.*"):
            target = target_dir / f"{stem}{member.suffix}"
            shutil.copy2(member, target)
            if member.suffix.lower() == ".shp":
                copied_shp = target
        if copied_shp is None:
            raise AssertionError("复制 shapefile bundle 失败，缺少 .shp")
        return copied_shp

    @staticmethod
    def project_layer(layer_id: str) -> QgsMapLayer | None:
        """
        作用：实现 `project_layer` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `project_layer` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `layer_id`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
        - 返回：返回 `QgsMapLayer | None` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `QgsMapLayer | None` 类型结果，返回值语义遵循该函数实现约定。
        """
        return QgsProject.instance().mapLayer(layer_id)

    @staticmethod
    def vector_field_names(layer: QgsVectorLayer) -> list[str]:
        """
        作用：实现 `vector_field_names` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `vector_field_names` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
        - 返回：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `list[str]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return [field.name() for field in layer.fields()]

    @staticmethod
    def vector_attribute_values(layer: QgsVectorLayer, field_name: str) -> list[object]:
        """
        作用：实现 `vector_attribute_values` 方法，处理该类在当前职责中的一个流程步骤。
        用途：实现 `vector_attribute_values` 方法，处理该类在当前职责中的一个流程步骤。
        使用场景：在 processingmcpserver 测试辅助流程中被测试代码调用，用于构建、清理或断言测试上下文。
        参数与返回：
        - 参数 `layer`（`QgsVectorLayer`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
        - 参数 `field_name`（`str`）：QGIS 数据对象相关参数，用于定位图层、要素或空间参考上下文。
        - 返回：返回 `list[object]` 类型结果，返回值语义遵循该函数实现约定。
        返回结果：返回 `list[object]` 类型结果，返回值语义遵循该函数实现约定。
        """
        return [feature.attribute(field_name) for feature in layer.getFeatures()]
