from __future__ import annotations

import io
import os
import sys
import traceback
from typing import Any

import processing
from qgis.core import (
    Qgis,
    QgsApplication,
    QgsCoordinateReferenceSystem,
    QgsMapLayer,
    QgsMapRendererParallelJob,
    QgsMapSettings,
    QgsProject,
    QgsProcessingParameterDefinition,
    QgsProcessingParameterEnum,
    QgsRasterLayer,
    QgsVectorLayer,
)
from qgis.PyQt.QtCore import QSize
from qgis.PyQt.QtGui import QColor
from qgis.utils import active_plugins

from processing.mcpserver.qtRunner import MainThreadRunner

class ProcessingMCPTools:
    def __init__(self, iface, runner: MainThreadRunner):
        self._iface = iface
        self._runner = runner

    def _run(self, func, *args, **kwargs):
        return self._runner.run(lambda: func(*args, **kwargs))

    def ping(self) -> dict[str, bool]:
        return self._run(self._ping_impl)

    def _ping_impl(self) -> dict[str, bool]:
        return {"pong": True}

    def get_qgis_info(self) -> dict[str, Any]:
        return self._run(self._get_qgis_info_impl)

    def _get_qgis_info_impl(self) -> dict[str, Any]:
        return {
            "qgis_version": Qgis.version(),
            "profile_folder": QgsApplication.qgisSettingsDirPath(),
            "plugins_count": len(active_plugins),
        }

    def load_project(self, path: str) -> dict[str, Any]:
        return self._run(self._load_project_impl, path)

    def _load_project_impl(self, path: str) -> dict[str, Any]:
        project = QgsProject.instance()
        if project.read(path):
            self._iface.mapCanvas().refresh()
            return {
                "loaded": path,
                "layer_count": len(project.mapLayers()),
            }
        raise Exception(f"Failed to load project from {path}")

    def create_new_project(self, path: str) -> dict[str, Any]:
        return self._run(self._create_new_project_impl, path)

    def _create_new_project_impl(self, path: str) -> dict[str, Any]:
        project = QgsProject.instance()

        if project.fileName():
            project.clear()

        project.setFileName(path)
        self._iface.mapCanvas().refresh()

        if project.write():
            return {
                "created": f"Project created and saved successfully at: {path}",
                "layer_count": len(project.mapLayers()),
            }
        raise Exception(f"Failed to save project to {path}")

    def get_project_info(self) -> dict[str, Any]:
        return self._run(self._get_project_info_impl)

    def _get_project_info_impl(self) -> dict[str, Any]:
        project = QgsProject.instance()

        info = {
            "filename": project.fileName(),
            "title": project.title(),
            "layer_count": len(project.mapLayers()),
            "crs": project.crs().authid(),
            "layers": [],
        }

        layers = list(project.mapLayers().values())
        for i, layer in enumerate(layers):
            if i >= 10:
                break

            layer_info = {
                "id": layer.id(),
                "name": layer.name(),
                "type": self._get_layer_type(layer),
                "visible": layer.isValid()
                and project.layerTreeRoot().findLayer(layer.id()).isVisible(),
            }
            info["layers"].append(layer_info)

        return info

    def add_vector_layer(
        self, path: str, provider: str = "ogr", name: str | None = None
    ) -> dict[str, Any]:
        return self._run(self._add_vector_layer_impl, path, provider, name)

    def _add_vector_layer_impl(
        self, path: str, provider: str, name: str | None
    ) -> dict[str, Any]:
        if not name:
            name = os.path.basename(path)

        layer = QgsVectorLayer(path, name, provider)
        if not layer.isValid():
            raise Exception(f"Layer is not valid: {path}")

        QgsProject.instance().addMapLayer(layer)

        return {
            "id": layer.id(),
            "name": layer.name(),
            "type": self._get_layer_type(layer),
            "feature_count": layer.featureCount(),
        }

    def add_raster_layer(
        self, path: str, provider: str = "gdal", name: str | None = None
    ) -> dict[str, Any]:
        return self._run(self._add_raster_layer_impl, path, provider, name)

    def _add_raster_layer_impl(
        self, path: str, provider: str, name: str | None
    ) -> dict[str, Any]:
        if not name:
            name = os.path.basename(path)

        layer = QgsRasterLayer(path, name, provider)
        if not layer.isValid():
            raise Exception(f"Layer is not valid: {path}")

        QgsProject.instance().addMapLayer(layer)

        return {
            "id": layer.id(),
            "name": layer.name(),
            "type": "raster",
            "width": layer.width(),
            "height": layer.height(),
        }

    def get_layers(self) -> list[dict[str, Any]]:
        return self._run(self._get_layers_impl)

    def _get_layers_impl(self) -> list[dict[str, Any]]:
        project = QgsProject.instance()
        layers = []

        for layer_id, layer in project.mapLayers().items():
            layer_info = {
                "id": layer_id,
                "name": layer.name(),
                "type": self._get_layer_type(layer),
                "visible": project.layerTreeRoot().findLayer(layer_id).isVisible(),
            }

            if layer.type() == QgsMapLayer.VectorLayer:
                layer_info.update(
                    {
                        "feature_count": layer.featureCount(),
                        "geometry_type": layer.geometryType(),
                    }
                )
            elif layer.type() == QgsMapLayer.RasterLayer:
                layer_info.update(
                    {
                        "width": layer.width(),
                        "height": layer.height(),
                    }
                )

            layers.append(layer_info)

        return layers

    def remove_layer(self, layer_id: str) -> dict[str, str]:
        return self._run(self._remove_layer_impl, layer_id)

    def _remove_layer_impl(self, layer_id: str) -> dict[str, str]:
        project = QgsProject.instance()

        if layer_id in project.mapLayers():
            project.removeMapLayer(layer_id)
            return {"removed": layer_id}
        raise Exception(f"Layer not found: {layer_id}")

    def zoom_to_layer(self, layer_id: str) -> dict[str, str]:
        return self._run(self._zoom_to_layer_impl, layer_id)

    def _zoom_to_layer_impl(self, layer_id: str) -> dict[str, str]:
        project = QgsProject.instance()

        if layer_id in project.mapLayers():
            layer = project.mapLayer(layer_id)
            self._iface.setActiveLayer(layer)
            self._iface.zoomToActiveLayer()
            return {"zoomed_to": layer_id}
        raise Exception(f"Layer not found: {layer_id}")

    def get_layer_features(self, layer_id: str, limit: int = 10) -> dict[str, Any]:
        return self._run(self._get_layer_features_impl, layer_id, limit)

    def _get_layer_features_impl(self, layer_id: str, limit: int) -> dict[str, Any]:
        project = QgsProject.instance()

        if layer_id not in project.mapLayers():
            raise Exception(f"Layer not found: {layer_id}")

        layer = project.mapLayer(layer_id)
        if layer.type() != QgsMapLayer.VectorLayer:
            raise Exception(f"Layer is not a vector layer: {layer_id}")

        features = []
        for i, feature in enumerate(layer.getFeatures()):
            if i >= limit:
                break

            attrs = {}
            for field in layer.fields():
                attrs[field.name()] = feature.attribute(field.name())

            geom = None
            if feature.hasGeometry():
                geom = {
                    "type": feature.geometry().type(),
                    "wkt": feature.geometry().asWkt(precision=4),
                }

            features.append(
                {
                    "id": feature.id(),
                    "attributes": attrs,
                    "geometry": geom,
                }
            )

        return {
            "layer_id": layer_id,
            "feature_count": layer.featureCount(),
            "features": features,
            "fields": [field.name() for field in layer.fields()],
        }

    def execute_processing(
        self, algorithm: str, parameters: dict, load_results: bool = True
    ) -> dict[str, Any]:
        return self._run(
            self._execute_processing_impl, algorithm, parameters, load_results
        )

    def _execute_processing_impl(
        self, algorithm: str, parameters: dict, load_results: bool
    ) -> dict[str, Any]:
        try:
            if load_results:
                result = processing.runAndLoadResults(algorithm, parameters)
            else:
                result = processing.run(algorithm, parameters)
            return {
                "algorithm": algorithm,
                "loaded": load_results,
                "result": {k: str(v) for k, v in result.items()},
            }
        except Exception as e:
            raise Exception(f"Processing error: {str(e)}")

    def get_processing_algorithms(
        self,
        algorithm_id: str | None = None,
        provider_id: str | None = None,
        include_parameters: bool = False,
        include_outputs: bool = False,
        limit: int | None = None,
    ) -> dict[str, Any]:
        return self._run(
            self._get_processing_algorithms_impl,
            algorithm_id,
            provider_id,
            include_parameters,
            include_outputs,
            limit,
        )

    def _get_processing_algorithms_impl(
        self,
        algorithm_id: str | None,
        provider_id: str | None,
        include_parameters: bool,
        include_outputs: bool,
        limit: int | None,
    ) -> dict[str, Any]:
        registry = QgsApplication.processingRegistry()

        if algorithm_id:
            algorithm = registry.algorithmById(algorithm_id)
            if algorithm is None:
                raise Exception(f"Algorithm not found: {algorithm_id}")
            return {
                "algorithm": self._serialize_algorithm(
                    algorithm, include_parameters=True, include_outputs=True
                )
            }

        algorithms = list(registry.algorithms())
        if provider_id:
            algorithms = [
                alg
                for alg in algorithms
                if alg.provider()
                and (
                    alg.provider().id() == provider_id
                    or alg.provider().name() == provider_id
                )
            ]

        total = len(algorithms)
        truncated = False
        if limit is not None and limit >= 0 and total > limit:
            algorithms = algorithms[:limit]
            truncated = True

        return {
            "count": total,
            "returned": len(algorithms),
            "truncated": truncated,
            "algorithms": [
                self._serialize_algorithm(
                    alg,
                    include_parameters=include_parameters,
                    include_outputs=include_outputs,
                )
                for alg in algorithms
            ],
        }

    def save_project(self, path: str | None = None) -> dict[str, str]:
        return self._run(self._save_project_impl, path)

    def _save_project_impl(self, path: str | None) -> dict[str, str]:
        project = QgsProject.instance()

        if not path and not project.fileName():
            raise Exception("No project path specified and no current project path")

        save_path = path if path else project.fileName()
        if project.write(save_path):
            return {"saved": save_path}
        raise Exception(f"Failed to save project to {save_path}")

    def render_map(self, path: str, width: int = 800, height: int = 600) -> dict[str, Any]:
        return self._run(self._render_map_impl, path, width, height)

    def _render_map_impl(self, path: str, width: int, height: int) -> dict[str, Any]:
        try:
            ms = QgsMapSettings()

            layers = list(QgsProject.instance().mapLayers().values())
            ms.setLayers(layers)

            rect = self._iface.mapCanvas().extent()
            ms.setExtent(rect)
            ms.setOutputSize(QSize(width, height))
            ms.setBackgroundColor(QColor(255, 255, 255))
            ms.setOutputDpi(96)

            render = QgsMapRendererParallelJob(ms)
            render.start()
            render.waitForFinished()

            img = render.renderedImage()
            if img.save(path):
                return {
                    "rendered": True,
                    "path": path,
                    "width": width,
                    "height": height,
                }
            raise Exception(f"Failed to save rendered image to {path}")
        except Exception as e:
            raise Exception(f"Render error: {str(e)}")

    def execute_code(self, code: str) -> dict[str, Any]:
        return self._run(self._execute_code_impl, code)

    def _execute_code_impl(self, code: str) -> dict[str, Any]:
        stdout_capture = io.StringIO()
        stderr_capture = io.StringIO()

        original_stdout = sys.stdout
        original_stderr = sys.stderr

        try:
            sys.stdout = stdout_capture
            sys.stderr = stderr_capture

            namespace = {
                "qgis": Qgis,
                "QgsProject": QgsProject,
                "iface": self._iface,
                "QgsApplication": QgsApplication,
                "QgsVectorLayer": QgsVectorLayer,
                "QgsRasterLayer": QgsRasterLayer,
                "QgsCoordinateReferenceSystem": QgsCoordinateReferenceSystem,
            }

            exec(code, namespace)

            sys.stdout = original_stdout
            sys.stderr = original_stderr

            return {
                "executed": True,
                "stdout": stdout_capture.getvalue(),
                "stderr": stderr_capture.getvalue(),
            }
        except Exception as e:
            error_traceback = traceback.format_exc()

            sys.stdout = original_stdout
            sys.stderr = original_stderr

            return {
                "executed": False,
                "error": str(e),
                "traceback": error_traceback,
                "stdout": stdout_capture.getvalue(),
                "stderr": stderr_capture.getvalue(),
            }

    @staticmethod
    def _get_layer_type(layer) -> str:
        if layer.type() == QgsMapLayer.VectorLayer:
            return f"vector_{layer.geometryType()}"
        if layer.type() == QgsMapLayer.RasterLayer:
            return "raster"
        return str(layer.type())

    @staticmethod
    def _serialize_value(value: Any) -> Any:
        if value is None:
            return None
        if isinstance(value, (bool, int, float, str)):
            return value
        if isinstance(value, list):
            return [ProcessingMCPTools._serialize_value(item) for item in value]
        if isinstance(value, dict):
            return {
                key: ProcessingMCPTools._serialize_value(val)
                for key, val in value.items()
            }
        if hasattr(value, "toString") and callable(value.toString):
            try:
                return value.toString()
            except Exception:
                pass
        return str(value)

    @staticmethod
    def _safe_call(obj: object, name: str, default: Any = None) -> Any:
        attr = getattr(obj, name, None)
        if callable(attr):
            try:
                return attr()
            except Exception:
                return default
        return attr if attr is not None else default

    def _serialize_parameter(self, param) -> dict[str, Any]:
        flags = self._safe_call(param, "flags", 0) or 0
        param_info = {
            "name": self._safe_call(param, "name"),
            "description": self._safe_call(param, "description"),
            "type": self._safe_call(param, "type"),
            "type_name": self._safe_call(param, "typeName"),
            "default": self._serialize_value(self._safe_call(param, "defaultValue")),
            "optional": bool(
                flags & QgsProcessingParameterDefinition.Flag.FlagOptional
            ),
            "hidden": bool(flags & QgsProcessingParameterDefinition.Flag.FlagHidden),
        }

        help_text = self._safe_call(param, "help")
        if help_text:
            param_info["help"] = help_text

        if isinstance(param, QgsProcessingParameterEnum):
            param_info["options"] = list(param.options())

        if hasattr(param, "allowMultiple"):
            param_info["allow_multiple"] = bool(
                self._safe_call(param, "allowMultiple", False)
            )

        minimum = self._safe_call(param, "minimum")
        if minimum is not None:
            param_info["min"] = minimum

        maximum = self._safe_call(param, "maximum")
        if maximum is not None:
            param_info["max"] = maximum

        return param_info

    def _serialize_output(self, output_def) -> dict[str, Any]:
        return {
            "name": self._safe_call(output_def, "name"),
            "description": self._safe_call(output_def, "description"),
            "type": self._safe_call(output_def, "type"),
            "type_name": self._safe_call(output_def, "typeName"),
        }

    def _serialize_algorithm(
        self, alg, include_parameters: bool, include_outputs: bool
    ) -> dict[str, Any]:
        provider = alg.provider() if hasattr(alg, "provider") else None
        info = {
            "id": alg.id(),
            "name": alg.displayName(),
            "group": alg.group(),
            "group_id": alg.groupId(),
            "provider": provider.name() if provider else None,
            "provider_id": provider.id() if provider else None,
            "parameters_count": len(alg.parameterDefinitions()),
            "outputs_count": len(alg.outputDefinitions()),
        }

        if include_parameters:
            info["parameters"] = [
                self._serialize_parameter(param)
                for param in alg.parameterDefinitions()
            ]

        if include_outputs:
            info["outputs"] = [
                self._serialize_output(output_def)
                for output_def in alg.outputDefinitions()
            ]

        return info

def register_tools(mcp, tools: ProcessingMCPTools) -> None:
    @mcp.tool()
    def ping() -> dict[str, bool]:
        """Simple ping command to check server connectivity."""
        return tools.ping()

    @mcp.tool()
    def get_qgis_info() -> dict[str, Any]:
        """Get QGIS information."""
        return tools.get_qgis_info()

    @mcp.tool()
    def load_project(path: str) -> dict[str, Any]:
        """Load a QGIS project from the specified path."""
        return tools.load_project(path)

    @mcp.tool()
    def create_new_project(path: str) -> dict[str, Any]:
        """Create a new project and save it."""
        return tools.create_new_project(path)

    @mcp.tool()
    def get_project_info() -> dict[str, Any]:
        """Get current project information."""
        return tools.get_project_info()

    @mcp.tool()
    def add_vector_layer(
        path: str, provider: str = "ogr", name: str | None = None
    ) -> dict[str, Any]:
        """Add a vector layer to the project."""
        return tools.add_vector_layer(path, provider=provider, name=name)

    @mcp.tool()
    def add_raster_layer(
        path: str, provider: str = "gdal", name: str | None = None
    ) -> dict[str, Any]:
        """Add a raster layer to the project."""
        return tools.add_raster_layer(path, provider=provider, name=name)

    @mcp.tool()
    def get_layers() -> list[dict[str, Any]]:
        """Retrieve all layers in the current project."""
        return tools.get_layers()

    @mcp.tool()
    def remove_layer(layer_id: str) -> dict[str, str]:
        """Remove a layer from the project by its ID."""
        return tools.remove_layer(layer_id)

    @mcp.tool()
    def zoom_to_layer(layer_id: str) -> dict[str, str]:
        """Zoom to the extent of a specified layer."""
        return tools.zoom_to_layer(layer_id)

    @mcp.tool()
    def get_layer_features(layer_id: str, limit: int = 10) -> dict[str, Any]:
        """Retrieve features from a vector layer with an optional limit."""
        return tools.get_layer_features(layer_id, limit=limit)

    @mcp.tool()
    def execute_processing(
        algorithm: str, parameters: dict, load_results: bool = True
    ) -> dict[str, Any]:
        """Execute a processing algorithm with the given parameters."""
        return tools.execute_processing(
            algorithm, parameters, load_results=load_results
        )

    @mcp.tool()
    def get_processing_algorithms(
        algorithm_id: str | None = None,
        provider_id: str | None = None,
        include_parameters: bool = False,
        include_outputs: bool = False,
        limit: int | None = None,
    ) -> dict[str, Any]:
        """List processing algorithms and optionally return parameter details."""
        return tools.get_processing_algorithms(
            algorithm_id=algorithm_id,
            provider_id=provider_id,
            include_parameters=include_parameters,
            include_outputs=include_outputs,
            limit=limit,
        )

    @mcp.tool()
    def save_project(path: str | None = None) -> dict[str, str]:
        """Save the current project to the given path."""
        return tools.save_project(path)

    @mcp.tool()
    def render_map(path: str, width: int = 800, height: int = 600) -> dict[str, Any]:
        """Render the current map view to an image file."""
        return tools.render_map(path, width=width, height=height)

    @mcp.tool()
    def execute_code(code: str) -> dict[str, Any]:
        """Execute arbitrary PyQGIS code provided as a string."""
        return tools.execute_code(code)
