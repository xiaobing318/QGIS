"""HTTP bridge between out-of-process MCP services and the QGIS process.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import errno
import json
import math
import os
import threading
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, unquote, urlencode
from urllib.request import Request, urlopen

from qcopilots_common.constants import (
    DEFAULT_BRIDGE_PORT,
    DEFAULT_HOST,
    HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS,
    MAX_HTTP_ERROR_BODY_DRAIN_BYTES,
    MAX_HTTP_REQUEST_BODY_BYTES,
)


class BridgeClient:
    def __init__(self, base_url: str | None):
        self.base_url = (base_url or "").rstrip("/")

    def call(self, tool: str, arguments: dict[str, Any] | None = None) -> dict[str, Any]:
        if not self.base_url:
            raise RuntimeError("QCopilots QGIS bridge is not configured")
        payload = json.dumps({"tool": tool, "arguments": arguments or {}}).encode("utf-8")
        headers = {"Content-Type": "application/json"}
        request = Request(
            f"{self.base_url}/call",
            data=payload,
            headers=headers,
            method="POST",
        )
        with urlopen(request, timeout=60) as response:
            data = json.loads(response.read().decode("utf-8"))
        if not data.get("ok"):
            raise RuntimeError(data.get("error", "QGIS bridge call failed"))
        return data.get("result", {})


def _is_address_in_use(error: OSError) -> bool:
    return error.errno == errno.EADDRINUSE or getattr(error, "winerror", None) == 10048


class QgisBridgeHttpServer(ThreadingHTTPServer):
    allow_reuse_address = False


class QgisBridgeController:
    def __init__(
        self,
        iface: Any,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_BRIDGE_PORT,
    ):
        self.iface = iface
        self.host = host
        self.port = port
        self._httpd: ThreadingHTTPServer | None = None
        self._thread: threading.Thread | None = None
        self._dispatcher = _make_main_thread_dispatcher()

    @property
    def url(self) -> str:
        return f"http://{self.host}:{self.port}"

    def start(self) -> None:
        if self._httpd:
            return

        controller = self

        class Handler(BaseHTTPRequestHandler):
            server_version = "QCopilotsQgisBridge/0.1"

            def log_message(self, fmt: str, *args: Any) -> None:
                return

            def do_GET(self) -> None:
                if self.path.rstrip("/") != "/health":
                    self._send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not_found"})
                    return
                self._send_json(HTTPStatus.OK, {"ok": True, "status": "ok"})

            def do_POST(self) -> None:
                if self.path.rstrip("/") != "/call":
                    self._discard_request_body()
                    self._send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not_found"})
                    return
                if self.headers.get_content_type().lower() != "application/json":
                    self._discard_request_body()
                    self._send_json(HTTPStatus.UNSUPPORTED_MEDIA_TYPE, {"ok": False, "error": "unsupported_media_type"})
                    return
                try:
                    length = self._request_body_length()
                    if length < 0:
                        self._send_json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "bad_content_length"})
                        return
                    if length > MAX_HTTP_REQUEST_BODY_BYTES:
                        self.close_connection = True
                        self._send_json(
                            HTTPStatus.REQUEST_ENTITY_TOO_LARGE,
                            {"ok": False, "error": "request_too_large"},
                        )
                        return
                    payload = json.loads(self.rfile.read(length).decode("utf-8"))
                    result = controller.dispatch(payload.get("tool"), payload.get("arguments") or {})
                    self._send_json(HTTPStatus.OK, {"ok": True, "result": result})
                except Exception as err:
                    self._send_json(HTTPStatus.OK, {"ok": False, "error": str(err)})

            def _send_json(self, status: HTTPStatus, body: dict[str, Any]) -> None:
                data = json.dumps(body, ensure_ascii=False).encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                if self.close_connection:
                    self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(data)

            def _discard_request_body(self) -> None:
                length = self._request_body_length()
                if length <= 0:
                    return
                if length > MAX_HTTP_ERROR_BODY_DRAIN_BYTES:
                    self.close_connection = True
                    return
                old_timeout = self.connection.gettimeout()
                try:
                    self.connection.settimeout(HTTP_ERROR_BODY_DRAIN_TIMEOUT_SECONDS)
                    remaining = length
                    while remaining > 0:
                        chunk = self.rfile.read(min(remaining, 8192))
                        if not chunk:
                            self.close_connection = True
                            break
                        remaining -= len(chunk)
                except OSError:
                    self.close_connection = True
                finally:
                    try:
                        self.connection.settimeout(old_timeout)
                    except OSError:
                        self.close_connection = True

            def _request_body_length(self) -> int:
                try:
                    return max(0, int(self.headers.get("Content-Length", "0")))
                except ValueError:
                    self.close_connection = True
                    return -1

        try:
            self._httpd = QgisBridgeHttpServer((self.host, self.port), Handler)
        except OSError as err:
            if self.port == 0 or not _is_address_in_use(err):
                raise
            self._httpd = QgisBridgeHttpServer((self.host, 0), Handler)
        self.port = int(self._httpd.server_address[1])
        self._thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        httpd = self._httpd
        thread = self._thread
        if httpd:
            httpd.shutdown()
            httpd.server_close()
            self._httpd = None
        if thread and thread.is_alive():
            thread.join(timeout=5)
        self._thread = None

    def dispatch(self, tool: str, arguments: dict[str, Any]) -> dict[str, Any]:
        def invoke() -> dict[str, Any]:
            return QgisBridgeTools(self.iface).dispatch(tool, arguments)

        if self._dispatcher:
            return self._dispatcher.call(invoke)
        return invoke()


class _QtMainThreadDispatcher:
    def __init__(self):
        from qgis.PyQt.QtCore import QCoreApplication, QObject, QThread, Qt, pyqtSignal

        class DispatcherObject(QObject):
            request = pyqtSignal(object)

        self._app = QCoreApplication.instance()
        self._qthread = QThread
        self._object = DispatcherObject()
        self._object.request.connect(self._dispatch, Qt.ConnectionType.QueuedConnection)

    def call(self, function, timeout_seconds: float = 60):
        if not self._app or self._qthread.currentThread() == self._app.thread():
            return function()

        done = threading.Event()
        payload = {"function": function, "done": done, "result": None, "error": None}
        self._object.request.emit(payload)
        if not done.wait(timeout_seconds):
            raise TimeoutError("QCopilots QGIS bridge call timed out")
        if payload["error"]:
            raise payload["error"]
        return payload["result"]

    def _dispatch(self, payload: dict[str, Any]) -> None:
        try:
            payload["result"] = payload["function"]()
        except Exception as err:
            payload["error"] = err
        finally:
            payload["done"].set()


def _make_main_thread_dispatcher():
    try:
        return _QtMainThreadDispatcher()
    except Exception:
        return None


class QgisBridgeTools:
    def __init__(
        self,
        iface: Any,
    ):
        self.iface = iface

    def dispatch(self, tool: str, arguments: dict[str, Any]) -> dict[str, Any]:
        handlers = {
            "list_layers": self.list_layers,
            "add_vector_layer": self.add_vector_layer,
            "add_raster_layer": self.add_raster_layer,
            "remove_layer": self.remove_layer,
            "set_layer_visibility": self.set_layer_visibility,
            "zoom_to_layer": self.zoom_to_layer,
            "zoom_to_extent": self.zoom_to_extent,
            "zoom_in": self.zoom_in,
            "zoom_out": self.zoom_out,
            "zoom_full": self.zoom_full,
            "zoom_to_selection": self.zoom_to_selection,
            "zoom_to_native_resolution": self.zoom_to_native_resolution,
            "zoom_to_last_extent": self.zoom_to_last_extent,
            "zoom_to_next_extent": self.zoom_to_next_extent,
            "save_project": self.save_project,
            "refresh_canvas": self.refresh_canvas,
            "export_map_image": self.export_map_image,
            "interactive_layer_describe_sources": self.interactive_layer_describe_sources,
            "interactive_layer_list_layers": self.interactive_layer_list_layers,
            "interactive_layer_load_layer": self.interactive_layer_load_layer,
            "interactive_layer_remove_layers": self.interactive_layer_remove_layers,
            "create_vector_layer": self.create_vector_layer,
            "add_vector_features": self.add_vector_features,
            "update_vector_features": self.update_vector_features,
            "processing_list_algorithms": self.processing_list_algorithms,
            "processing_algorithm_details": self.processing_algorithm_details,
            "processing_run_algorithm": self.processing_run_algorithm,
        }
        if tool not in handlers:
            raise ValueError(f"Unknown QGIS bridge tool: {tool}")
        return handlers[tool](arguments)

    def list_layers(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layers = []
        for layer in QgsProject.instance().mapLayers().values():
            layers.append(_layer_summary(layer))
        return {"layers": layers}

    def add_vector_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject, QgsVectorLayer

        path = self._safe_workspace_path(arguments["path"])
        name = arguments.get("name") or Path(path).stem
        layer = QgsVectorLayer(path, name, arguments.get("provider", "ogr"))
        if not layer.isValid():
            raise RuntimeError(f"Invalid vector layer: {path}")
        QgsProject.instance().addMapLayer(layer)
        return {"layer_id": layer.id(), "name": layer.name()}

    def add_raster_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject, QgsRasterLayer

        path = self._safe_workspace_path(arguments["path"])
        name = arguments.get("name") or Path(path).stem
        layer = QgsRasterLayer(path, name)
        if not layer.isValid():
            raise RuntimeError(f"Invalid raster layer: {path}")
        QgsProject.instance().addMapLayer(layer)
        return {"layer_id": layer.id(), "name": layer.name()}

    def remove_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layer_id = arguments["layer_id"]
        QgsProject.instance().removeMapLayer(layer_id)
        return {"removed": layer_id}

    def set_layer_visibility(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        node = QgsProject.instance().layerTreeRoot().findLayer(arguments["layer_id"])
        if not node:
            raise RuntimeError("Layer tree node not found")
        node.setItemVisibilityChecked(bool(arguments.get("visible", True)))
        return {"layer_id": arguments["layer_id"], "visible": node.itemVisibilityChecked()}

    def zoom_to_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layer = QgsProject.instance().mapLayer(arguments["layer_id"])
        if not layer:
            raise RuntimeError("Layer not found")
        self.iface.mapCanvas().setExtent(layer.extent())
        self.iface.mapCanvas().refresh()
        return {"layer_id": layer.id()}

    def zoom_to_extent(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsRectangle

        extent = arguments["extent"]
        rectangle = QgsRectangle(extent[0], extent[1], extent[2], extent[3])
        self.iface.mapCanvas().setExtent(rectangle)
        self.iface.mapCanvas().refresh()
        return {"extent": extent}

    def zoom_in(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        self.iface.mapCanvas().zoomIn()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "in"}

    def zoom_out(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        self.iface.mapCanvas().zoomOut()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "out"}

    def zoom_full(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        if hasattr(self.iface, "zoomFull"):
            self.iface.zoomFull()
        else:
            self.iface.mapCanvas().zoomToFullExtent()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "full"}

    def zoom_to_selection(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layer = None
        if arguments.get("layer_id"):
            layer = QgsProject.instance().mapLayer(arguments["layer_id"])
            if not layer:
                raise RuntimeError("Layer not found")
        self.iface.mapCanvas().zoomToSelected(layer)
        self.iface.mapCanvas().refresh()
        return {"layer_id": layer.id() if layer else None}

    def zoom_to_last_extent(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        if hasattr(self.iface, "zoomToPrevious"):
            self.iface.zoomToPrevious()
        else:
            self.iface.mapCanvas().zoomToPreviousExtent()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "last"}

    def zoom_to_next_extent(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        if hasattr(self.iface, "zoomToNext"):
            self.iface.zoomToNext()
        else:
            self.iface.mapCanvas().zoomToNextExtent()
        self.iface.mapCanvas().refresh()
        return {"zoomed": "next"}

    def zoom_to_native_resolution(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject, QgsRasterLayer

        layer = QgsProject.instance().mapLayer(arguments.get("layer_id", ""))
        if layer is None and hasattr(self.iface, "activeLayer"):
            layer = self.iface.activeLayer()
        if not isinstance(layer, QgsRasterLayer):
            raise RuntimeError("A raster layer is required for native resolution zoom")

        canvas = self.iface.mapCanvas()
        factor = _native_resolution_zoom_factor(layer, canvas)
        canvas.zoomByFactor(factor)
        canvas.refresh()
        return {"layer_id": layer.id(), "zoom_factor": factor}

    def save_project(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        path = arguments.get("path")
        if path:
            path = self._safe_workspace_path(path)
        project = QgsProject.instance()
        if not path and project.fileName():
            self._safe_workspace_path(project.fileName())
        ok = project.write(path) if path else project.write()
        if not ok:
            raise RuntimeError("Project write failed")
        return {"saved": True, "path": path or project.fileName()}

    def refresh_canvas(self, arguments: dict[str, Any]) -> dict[str, Any]:
        self.iface.mapCanvas().refresh()
        return {"refreshed": True}

    def export_map_image(self, arguments: dict[str, Any]) -> dict[str, Any]:
        path = self._safe_workspace_path(arguments["path"])
        self.iface.mapCanvas().saveAsImage(path)
        image_path = Path(path)
        return {"path": path, "saved": image_path.is_file() and image_path.stat().st_size > 0}

    def interactive_layer_describe_sources(self, arguments: dict[str, Any]) -> dict[str, Any]:
        del arguments
        provider_keys = []
        try:
            from qgis.core import QgsProviderRegistry

            provider_keys = sorted(QgsProviderRegistry.instance().providerList())
        except Exception:
            provider_keys = []
        return {
            "supported_layer_types": _supported_layer_type_descriptions(),
            "available_providers": provider_keys,
        }

    def interactive_layer_list_layers(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self.list_layers(arguments)

    def interactive_layer_load_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        layer_type = _normalize_layer_type(arguments["layer_type"])
        source = str(arguments["source"])
        provider = str(arguments.get("provider") or _default_layer_provider(layer_type))
        name = str(arguments.get("name") or _layer_name_from_source(source, layer_type))
        uri_options = arguments.get("uri_options") or {}
        if not isinstance(uri_options, dict):
            raise RuntimeError("uri_options must be an object")

        layer, resolved_layer_type, resolved_provider, resolved_source = _create_qgis_layer(
            layer_type,
            source,
            name,
            provider,
            uri_options,
        )
        if not layer.isValid():
            raise RuntimeError(
                f"Invalid {resolved_layer_type} layer from provider {resolved_provider}: {resolved_source}"
            )

        added = bool(arguments.get("add_to_project", True))
        if added:
            QgsProject.instance().addMapLayer(layer)
        if bool(arguments.get("refresh_canvas", False)) and self.iface:
            self.iface.mapCanvas().refresh()
        return {
            "added": added,
            "layer": _layer_summary(layer),
            "layer_type": resolved_layer_type,
            "provider": resolved_provider,
            "source": resolved_source,
        }

    def interactive_layer_remove_layers(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject

        project = QgsProject.instance()
        selectors = _layer_remove_selectors(arguments)
        if not any(selectors.values()):
            raise RuntimeError("Specify at least one layer_id, layer_ids, name, names, source or sources value")

        removed = []
        layers = _layers_matching_selectors(
            list(project.mapLayers().values()),
            selectors,
            bool(arguments.get("allow_multiple", False)),
        )
        for layer in layers:
            removed.append(_layer_summary(layer))
            project.removeMapLayer(layer.id())
        return {"removed": removed, "removed_count": len(removed)}

    def create_vector_layer(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsProject, QgsVectorLayer

        name = str(arguments["name"])
        geometry_type = _vector_geometry_type(arguments.get("geometry_type", "Point"))
        crs = str(arguments.get("crs") or "EPSG:4326")
        layer = QgsVectorLayer(f"{geometry_type}?crs={crs}", name, "memory")
        if not layer.isValid():
            raise RuntimeError(f"Could not create vector layer: {name}")

        provider = layer.dataProvider()
        fields = [_qgs_field(field) for field in arguments.get("fields") or []]
        if fields:
            provider.addAttributes(fields)
            layer.updateFields()

        features = [
            _feature_from_payload(layer.fields(), feature)
            for feature in arguments.get("features") or []
        ]
        if features:
            ok, added = provider.addFeatures(features)
            if not ok:
                raise RuntimeError("Could not add initial vector features")
            layer.updateExtents()

        output_path = ""
        requested_path = str(arguments.get("path") or "").strip()
        if requested_path.lower() == "memory:":
            output_path = "memory:"
        elif requested_path:
            output_path = self._safe_workspace_path(requested_path)
            Path(output_path).parent.mkdir(parents=True, exist_ok=True)
            layer = _write_vector_layer(
                layer,
                output_path,
                arguments.get("driver_name"),
                name,
            )

        added_to_project = bool(arguments.get("add_to_project", True))
        if added_to_project:
            QgsProject.instance().addMapLayer(layer)
        return {
            "added": added_to_project,
            "layer": _layer_summary(layer),
            "path": output_path,
            "feature_count": layer.featureCount(),
        }

    def add_vector_features(self, arguments: dict[str, Any]) -> dict[str, Any]:
        layer = _vector_layer_from_id(arguments["layer_id"])
        features = [
            _feature_from_payload(layer.fields(), feature)
            for feature in arguments.get("features") or []
        ]
        if not features:
            raise RuntimeError("features must contain at least one feature")
        ok, added = layer.dataProvider().addFeatures(features)
        if not ok:
            raise RuntimeError("Could not add vector features")
        layer.updateExtents()
        if self.iface:
            self.iface.mapCanvas().refresh()
        return {
            "layer": _layer_summary(layer),
            "added_feature_ids": [feature.id() for feature in added],
            "added_count": len(added),
        }

    def update_vector_features(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsGeometry

        layer = _vector_layer_from_id(arguments["layer_id"])
        field_indexes = _field_index_by_name(layer.fields())
        attribute_updates = {}
        geometry_updates = {}
        updated_ids = []
        updates = arguments.get("updates") or []
        if not updates:
            raise RuntimeError("updates must contain at least one feature update")
        for update in updates:
            feature_id = _feature_id(update.get("feature_id"))
            has_attributes = bool(update.get("attributes"))
            has_geometry = bool(update.get("geometry_wkt"))
            if not has_attributes and not has_geometry:
                raise RuntimeError("Each update must include attributes or geometry_wkt")
            updated_ids.append(feature_id)
            if has_attributes:
                attribute_values = {}
                for name, value in (update.get("attributes") or {}).items():
                    if name not in field_indexes:
                        raise RuntimeError(f"Unknown vector field: {name}")
                    attribute_values[field_indexes[name]] = value
                attribute_updates[feature_id] = attribute_values
            if has_geometry:
                geometry = QgsGeometry.fromWkt(update["geometry_wkt"])
                if geometry.isNull():
                    raise RuntimeError(f"Invalid feature geometry WKT: {update['geometry_wkt']}")
                geometry_updates[feature_id] = geometry

        provider = layer.dataProvider()
        if attribute_updates and not provider.changeAttributeValues(attribute_updates):
            raise RuntimeError("Could not update vector feature attributes")
        if geometry_updates and not provider.changeGeometryValues(geometry_updates):
            raise RuntimeError("Could not update vector feature geometries")
        layer.updateExtents()
        if self.iface:
            self.iface.mapCanvas().refresh()
        return {
            "layer": _layer_summary(layer),
            "updated_feature_ids": updated_ids,
            "updated_count": len(updated_ids),
        }

    def processing_list_algorithms(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsApplication

        _ensure_processing_initialized()
        category = arguments.get("category")
        algorithms = []
        for provider in QgsApplication.processingRegistry().providers():
            for algorithm in provider.algorithms():
                if category and not _algorithm_matches_category(algorithm, category):
                    continue
                algorithms.append(
                    {
                        "id": algorithm.id(),
                        "name": algorithm.name(),
                        "display_name": algorithm.displayName(),
                        "group": algorithm.group(),
                        "provider": provider.id(),
                    }
                )
        return {"algorithms": algorithms}

    def processing_algorithm_details(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsApplication

        _ensure_processing_initialized()
        algorithm = QgsApplication.processingRegistry().createAlgorithmById(arguments["algorithm_id"])
        if not algorithm:
            raise RuntimeError("Processing algorithm not found")
        category = arguments.get("category")
        if category and not _algorithm_matches_category(algorithm, category):
            raise RuntimeError(f"Processing algorithm is not available for {category} data")
        return {
            "id": algorithm.id(),
            "display_name": algorithm.displayName(),
            "short_description": algorithm.shortDescription(),
            "group": algorithm.group(),
            "parameters": [_parameter_metadata(parameter) for parameter in algorithm.parameterDefinitions()],
        }

    def processing_run_algorithm(self, arguments: dict[str, Any]) -> dict[str, Any]:
        from qgis.core import QgsApplication

        _ensure_processing_initialized()
        import processing

        category = arguments.get("category")
        algorithm = QgsApplication.processingRegistry().createAlgorithmById(arguments["algorithm_id"])
        if not algorithm:
            raise RuntimeError("Processing algorithm not found")
        if category and not _algorithm_matches_category(algorithm, category):
            raise RuntimeError(f"Processing algorithm is not available for {category} data")
        parameters = _sanitize_processing_parameters(
            arguments.get("parameters") or {},
            algorithm.parameterDefinitions(),
        )
        result = processing.run(arguments["algorithm_id"], parameters)
        return {"result": result}

    def _safe_workspace_path(self, value: str | Path) -> str:
        return _safe_workspace_path(value)


def _ensure_processing_initialized() -> None:
    try:
        from processing.core.Processing import Processing
    except Exception as err:
        raise RuntimeError(f"QGIS Processing is not available: {err}") from err
    try:
        Processing.initialize()
    except Exception as err:
        raise RuntimeError(f"QGIS Processing could not be initialized: {err}") from err


def _supported_layer_type_descriptions() -> list[dict[str, Any]]:
    return [
        {
            "layer_type": "auto",
            "providers": ["ogr", "gdal"],
            "description": "Try common local vector and raster providers in order.",
        },
        {
            "layer_type": "vector",
            "providers": [
                "ogr",
                "memory",
                "delimitedtext",
                "WFS",
                "arcgisfeatureserver",
            ],
        },
        {
            "layer_type": "raster",
            "providers": [
                "gdal",
                "wms",
                "wcs",
                "arcgismapserver",
                "arcgisimageserver",
            ],
        },
        {"layer_type": "mesh", "providers": ["mdal", "mesh_memory"]},
        {"layer_type": "vector_tile", "providers": ["vectortile"]},
        {"layer_type": "point_cloud", "providers": ["pdal", "ept"]},
        {
            "layer_type": "tiled_scene",
            "providers": ["cesiumtiles", "esrii3s", "tiled_mesh"],
        },
        {"layer_type": "xyz", "providers": ["wms"]},
        {"layer_type": "wms", "providers": ["wms"]},
        {"layer_type": "wcs", "providers": ["wcs"]},
        {"layer_type": "arcgis_feature_server", "providers": ["arcgisfeatureserver"]},
        {"layer_type": "arcgis_map_server", "providers": ["arcgismapserver"]},
        {"layer_type": "arcgis_image_server", "providers": ["arcgisimageserver"]},
    ]


def _native_resolution_zoom_factor(layer: Any, canvas: Any) -> float:
    map_units_per_pixel = float(canvas.mapUnitsPerPixel())
    if map_units_per_pixel <= 0:
        raise RuntimeError("Map canvas resolution is not available")
    provider = layer.dataProvider()
    native_resolutions = provider.nativeResolutions() if provider else []
    if native_resolutions:
        factor = float(native_resolutions[0]) / map_units_per_pixel
    else:
        factor = math.sqrt(
            layer.rasterUnitsPerPixelX() * layer.rasterUnitsPerPixelX()
            + layer.rasterUnitsPerPixelY() * layer.rasterUnitsPerPixelY()
        ) / map_units_per_pixel
    if not math.isfinite(factor) or factor <= 0:
        raise RuntimeError("Raster native resolution is not available")
    return factor


def _normalize_layer_type(layer_type: str) -> str:
    normalized = str(layer_type).strip().lower().replace("-", "_")
    aliases = {
        "vectortile": "vector_tile",
        "vector_tiles": "vector_tile",
        "pointcloud": "point_cloud",
        "tiledscene": "tiled_scene",
        "arcgisfeatureserver": "arcgis_feature_server",
        "arcgismapserver": "arcgis_map_server",
        "arcgisimageserver": "arcgis_image_server",
    }
    normalized = aliases.get(normalized, normalized)
    valid_types = {
        item["layer_type"]
        for item in _supported_layer_type_descriptions()
    }
    if normalized not in valid_types:
        raise RuntimeError(f"Unsupported layer_type: {layer_type}")
    return normalized


def _default_layer_provider(layer_type: str) -> str:
    return {
        "auto": "",
        "vector": "ogr",
        "raster": "gdal",
        "mesh": "mdal",
        "vector_tile": "vectortile",
        "point_cloud": "pdal",
        "tiled_scene": "cesiumtiles",
        "xyz": "wms",
        "wms": "wms",
        "wcs": "wcs",
        "arcgis_feature_server": "arcgisfeatureserver",
        "arcgis_map_server": "arcgismapserver",
        "arcgis_image_server": "arcgisimageserver",
    }[layer_type]


def _resolve_layer_type_and_provider(
    layer_type: str,
    provider: Any,
    source: str,
    uri_options: dict[str, Any],
) -> tuple[str, str]:
    provider_token = _provider_token(provider)
    resolved_layer_type = _specialized_layer_type(
        layer_type,
        provider_token,
        source,
        uri_options,
    )
    return resolved_layer_type, _normalize_layer_provider(resolved_layer_type, provider)


def _specialized_layer_type(
    layer_type: str,
    provider_token: str,
    source: str,
    uri_options: dict[str, Any],
) -> str:
    if layer_type == "raster":
        if _looks_like_xyz_tile_template(source) or _uri_options_type(uri_options) == "xyz":
            return "xyz"
        if "layers" in uri_options:
            return "wms"
        if "identifier" in uri_options:
            return "wcs"
        encoded_layer_type = _layer_type_from_encoded_provider_uri(source)
        if encoded_layer_type in {
            "xyz",
            "wms",
            "wcs",
            "arcgis_map_server",
            "arcgis_image_server",
        }:
            return encoded_layer_type
    hinted_layer_type = _layer_type_from_provider_token(provider_token)
    if hinted_layer_type and _provider_hint_matches_layer_type(layer_type, hinted_layer_type):
        return hinted_layer_type
    if layer_type == "vector":
        encoded_layer_type = _layer_type_from_encoded_provider_uri(source)
        if encoded_layer_type == "arcgis_feature_server":
            return encoded_layer_type
    return layer_type


def _normalize_layer_provider(layer_type: str, provider: Any) -> str:
    provider_text = str(provider or "").strip()
    token = _provider_token(provider_text)
    if not token:
        return _default_layer_provider(layer_type)

    aliases = {
        "xyz": {"xyz": "wms", "wms": "wms"},
        "wms": {"wms": "wms"},
        "wcs": {"wcs": "wcs"},
        "vector_tile": {
            "vectortile": "vectortile",
            "vector_tile": "vectortile",
            "vector_tiles": "vectortile",
            "xyz": "vectortile",
        },
        "point_cloud": {
            "pointcloud": "pdal",
            "point_cloud": "pdal",
            "pdal": "pdal",
            "ept": "ept",
        },
        "tiled_scene": {
            "tiledscene": "cesiumtiles",
            "tiled_scene": "cesiumtiles",
            "cesiumtiles": "cesiumtiles",
            "cesium_tiles": "cesiumtiles",
            "esrii3s": "esrii3s",
            "tiled_mesh": "tiled_mesh",
        },
        "arcgis_feature_server": {
            "arcgisfeatureserver": "arcgisfeatureserver",
            "arcgis_feature_server": "arcgisfeatureserver",
            "arcgis_feature": "arcgisfeatureserver",
        },
        "arcgis_map_server": {
            "arcgismapserver": "arcgismapserver",
            "arcgis_map_server": "arcgismapserver",
            "arcgis_map": "arcgismapserver",
        },
        "arcgis_image_server": {
            "arcgisimageserver": "arcgisimageserver",
            "arcgis_image_server": "arcgisimageserver",
            "arcgis_image": "arcgisimageserver",
        },
        "mesh": {"mesh": "mdal", "mdal": "mdal", "mesh_memory": "mesh_memory"},
        "vector": {"wfs": "WFS"},
    }
    return aliases.get(layer_type, {}).get(token, provider_text)


def _provider_token(provider: Any) -> str:
    return str(provider or "").strip().lower().replace("-", "_").replace(" ", "_")


def _layer_type_from_provider_token(provider_token: str) -> str:
    return {
        "xyz": "xyz",
        "wms": "wms",
        "wcs": "wcs",
        "vectortile": "vector_tile",
        "vector_tile": "vector_tile",
        "vector_tiles": "vector_tile",
        "pointcloud": "point_cloud",
        "point_cloud": "point_cloud",
        "pdal": "point_cloud",
        "ept": "point_cloud",
        "tiledscene": "tiled_scene",
        "tiled_scene": "tiled_scene",
        "cesiumtiles": "tiled_scene",
        "cesium_tiles": "tiled_scene",
        "esrii3s": "tiled_scene",
        "arcgisfeatureserver": "arcgis_feature_server",
        "arcgis_feature_server": "arcgis_feature_server",
        "arcgismapserver": "arcgis_map_server",
        "arcgis_map_server": "arcgis_map_server",
        "arcgisimageserver": "arcgis_image_server",
        "arcgis_image_server": "arcgis_image_server",
        "mesh": "mesh",
        "mdal": "mesh",
    }.get(provider_token, "")


def _provider_hint_matches_layer_type(layer_type: str, hinted_layer_type: str) -> bool:
    if layer_type == "auto":
        return True
    if layer_type == hinted_layer_type:
        return True
    if layer_type == "raster":
        return hinted_layer_type in {
            "xyz",
            "wms",
            "wcs",
            "arcgis_map_server",
            "arcgis_image_server",
        }
    if layer_type == "vector":
        return hinted_layer_type in {"arcgis_feature_server", "vector_tile"}
    return False


def _uri_options_type(uri_options: dict[str, Any]) -> str:
    return str(uri_options.get("type") or "").strip().lower()


def _looks_like_xyz_tile_template(source: str) -> bool:
    decoded = unquote(str(source or "")).lower()
    return (
        decoded.startswith(("http://", "https://"))
        and "{x}" in decoded
        and "{y}" in decoded
        and ("{z}" in decoded or "{zoom}" in decoded)
    )


def _layer_type_from_encoded_provider_uri(source: str) -> str:
    if not _is_encoded_provider_uri(source):
        return ""
    source_type = _provider_uri_parts(source).get("type", [""])[0].strip().lower()
    if source_type == "xyz":
        return "xyz"
    return ""


def _create_qgis_layer(
    layer_type: str,
    source: str,
    name: str,
    provider: str,
    uri_options: dict[str, Any],
) -> tuple[Any, str, str, str]:
    layer_type = _normalize_layer_type(layer_type)
    layer_type, provider = _resolve_layer_type_and_provider(
        layer_type,
        provider,
        source,
        uri_options,
    )
    if layer_type == "auto":
        if _is_remote_or_provider_uri(source):
            raise RuntimeError(
                "auto layer_type is only supported for local files. "
                "Use an explicit layer_type for remote URLs or provider URIs."
            )
        attempts = [
            ("vector", "ogr"),
            ("raster", "gdal"),
            ("mesh", "mdal"),
            ("point_cloud", "pdal"),
            ("vector_tile", "vectortile"),
            ("tiled_scene", "cesiumtiles"),
        ]
        errors = []
        for candidate_type, candidate_provider in attempts:
            candidate_source = _layer_source_uri(
                candidate_type,
                source,
                candidate_provider,
                uri_options,
            )
            layer = _instantiate_qgis_layer(
                candidate_type,
                candidate_source,
                name,
                candidate_provider,
            )
            if layer.isValid():
                return layer, candidate_type, candidate_provider, candidate_source
            errors.append(f"{candidate_type}:{candidate_provider}")
        raise RuntimeError(
            f"Could not load source with common layer providers: {', '.join(errors)}"
        )

    resolved_provider = provider or _default_layer_provider(layer_type)
    resolved_source = _layer_source_uri(
        layer_type,
        source,
        resolved_provider,
        uri_options,
    )
    layer = _instantiate_qgis_layer(
        layer_type,
        resolved_source,
        name,
        resolved_provider,
    )
    return layer, layer_type, resolved_provider, resolved_source


def _instantiate_qgis_layer(
    layer_type: str,
    source: str,
    name: str,
    provider: str,
) -> Any:
    if layer_type in ("vector", "arcgis_feature_server"):
        from qgis.core import QgsVectorLayer

        return QgsVectorLayer(source, name, provider)
    if layer_type in (
        "raster",
        "xyz",
        "wms",
        "wcs",
        "arcgis_map_server",
        "arcgis_image_server",
    ):
        from qgis.core import QgsRasterLayer

        return QgsRasterLayer(source, name, provider)
    if layer_type == "mesh":
        from qgis.core import QgsMeshLayer

        return QgsMeshLayer(source, name, provider)
    if layer_type == "vector_tile":
        from qgis.core import QgsVectorTileLayer

        return QgsVectorTileLayer(source, name)
    if layer_type == "point_cloud":
        from qgis.core import QgsPointCloudLayer

        return QgsPointCloudLayer(source, name, provider)
    if layer_type == "tiled_scene":
        from qgis.core import QgsTiledSceneLayer

        return QgsTiledSceneLayer(source, name, provider)
    raise RuntimeError(f"Unsupported layer_type: {layer_type}")


def _vector_geometry_type(value: Any) -> str:
    geometry_type = str(value or "Point").strip()
    valid = {
        "Point",
        "LineString",
        "Polygon",
        "MultiPoint",
        "MultiLineString",
        "MultiPolygon",
        "NoGeometry",
    }
    if geometry_type not in valid:
        raise RuntimeError(f"Unsupported vector geometry_type: {geometry_type}")
    return geometry_type


def _qgs_field(field: dict[str, Any]) -> Any:
    from qgis.PyQt.QtCore import QVariant
    from qgis.core import QgsField

    field_type = str(field.get("type") or "string").strip().lower()
    qvariant_type = {
        "string": QVariant.String,
        "int": QVariant.Int,
        "integer": QVariant.Int,
        "long": QVariant.LongLong,
        "double": QVariant.Double,
        "float": QVariant.Double,
        "bool": QVariant.Bool,
        "boolean": QVariant.Bool,
        "date": QVariant.Date,
        "datetime": QVariant.DateTime,
    }.get(field_type)
    if qvariant_type is None:
        raise RuntimeError(f"Unsupported vector field type: {field_type}")
    return QgsField(str(field["name"]), qvariant_type)


def _feature_from_payload(fields: Any, payload: dict[str, Any]) -> Any:
    from qgis.core import QgsFeature, QgsGeometry

    feature = QgsFeature(fields)
    if payload.get("geometry_wkt"):
        geometry = QgsGeometry.fromWkt(payload["geometry_wkt"])
        if geometry.isNull():
            raise RuntimeError(f"Invalid feature geometry WKT: {payload['geometry_wkt']}")
        feature.setGeometry(geometry)
    attributes = payload.get("attributes") or {}
    field_indexes = _field_index_by_name(fields)
    for name, value in attributes.items():
        if name not in field_indexes:
            raise RuntimeError(f"Unknown vector field: {name}")
        feature.setAttribute(field_indexes[name], value)
    return feature


def _field_index_by_name(fields: Any) -> dict[str, int]:
    return {fields.at(index).name(): index for index in range(fields.count())}


def _vector_layer_from_id(layer_id: str) -> Any:
    from qgis.core import QgsProject, QgsVectorLayer

    layer = QgsProject.instance().mapLayer(layer_id)
    if not isinstance(layer, QgsVectorLayer):
        raise RuntimeError("Vector layer not found")
    return layer


def _feature_id(value: Any) -> int:
    try:
        return int(value)
    except (TypeError, ValueError) as err:
        raise RuntimeError(f"Invalid feature_id: {value}") from err


def _write_vector_layer(
    layer: Any,
    path: str,
    driver_name: str | None,
    layer_name: str,
) -> Any:
    from qgis.core import QgsProject, QgsVectorFileWriter, QgsVectorLayer

    options = QgsVectorFileWriter.SaveVectorOptions()
    options.driverName = driver_name or _driver_name_for_path(path)
    options.layerName = layer_name
    result, message, new_file_name, new_layer_name = QgsVectorFileWriter.writeAsVectorFormatV3(
        layer,
        path,
        QgsProject.instance().transformContext(),
        options,
    )
    if result != QgsVectorFileWriter.WriterError.NoError:
        raise RuntimeError(f"Could not write vector layer: {message}")
    source = new_file_name
    if options.driverName == "GPKG" and new_layer_name:
        source = f"{new_file_name}|layername={new_layer_name}"
    written_layer = QgsVectorLayer(source, layer.name(), "ogr")
    if not written_layer.isValid():
        raise RuntimeError(f"Could not load written vector layer: {source}")
    return written_layer


def _driver_name_for_path(path: str) -> str:
    suffix = Path(path).suffix.lower()
    if suffix == ".gpkg":
        return "GPKG"
    if suffix == ".shp":
        return "ESRI Shapefile"
    if suffix in (".geojson", ".json"):
        return "GeoJSON"
    return "GPKG"


def _layer_source_uri(
    layer_type: str,
    source: str,
    provider: str,
    uri_options: dict[str, Any],
) -> str:
    if _is_encoded_provider_uri(source):
        return source

    if uri_options or layer_type in _provider_uri_layer_types():
        parts = _normalized_uri_options(layer_type, source, dict(uri_options))
        _validate_uri_options(layer_type, parts, source)
        encoded = _encode_provider_uri(provider, parts)
        if encoded:
            return encoded
        return urlencode(parts, doseq=True)

    return _normalize_layer_source_path(source)


def _normalized_uri_options(
    layer_type: str,
    source: str,
    uri_options: dict[str, Any],
) -> dict[str, Any]:
    if layer_type == "xyz":
        uri_options.setdefault("type", "xyz")
        uri_options.setdefault("zmin", 0)
        uri_options.setdefault("zmax", 19)
        uri_options.setdefault("tilePixelRatio", 1)
    if layer_type == "vector_tile":
        uri_options.setdefault("type", _vector_tile_source_type(source))
        if uri_options["type"] == "xyz":
            uri_options.setdefault("zmin", 0)
            uri_options.setdefault("zmax", 19)
    source_key = _uri_source_key(layer_type, source)
    if source and source_key not in uri_options:
        uri_options[source_key] = source
    for key in ("path", "file", "filename", "url"):
        if key in uri_options and isinstance(uri_options[key], str):
            uri_options[key] = _normalize_layer_source_path(uri_options[key])
    if layer_type == "wms" and "layers" in uri_options and "styles" not in uri_options:
        layers = uri_options["layers"]
        if isinstance(layers, list):
            uri_options["styles"] = [""] * len(layers)
        else:
            uri_options["styles"] = ""
    return uri_options


def _uri_source_key(layer_type: str, source: str) -> str:
    if layer_type in (
        "xyz",
        "vector_tile",
        "wms",
        "wcs",
        "arcgis_feature_server",
        "arcgis_map_server",
        "arcgis_image_server",
    ):
        return "url"
    if _is_remote_or_provider_uri(source):
        return "url"
    return "path"


def _provider_uri_layer_types() -> set[str]:
    return {
        "xyz",
        "vector_tile",
        "wms",
        "wcs",
        "arcgis_feature_server",
        "arcgis_map_server",
        "arcgis_image_server",
    }


def _vector_tile_source_type(source: str) -> str:
    suffix = Path(source.split("|", 1)[0]).suffix.lower()
    if suffix == ".mbtiles":
        return "mbtiles"
    if suffix == ".vtpk":
        return "vtpk"
    return "xyz"


def _validate_uri_options(layer_type: str, parts: dict[str, Any], source: str) -> None:
    if _is_encoded_provider_uri(source):
        return
    if layer_type == "wms" and not parts.get("layers"):
        raise RuntimeError("WMS loading requires uri_options.layers.")
    if layer_type == "wcs" and not parts.get("identifier"):
        raise RuntimeError("WCS loading requires uri_options.identifier.")


def _encode_provider_uri(provider: str, parts: dict[str, Any]) -> str:
    if not provider:
        return ""
    try:
        from qgis.core import QgsProviderRegistry

        metadata = QgsProviderRegistry.instance().providerMetadata(provider)
        if not metadata:
            return ""
        encoded = metadata.encodeUri(parts)
        return _qgis_string(encoded)
    except Exception:
        return ""


def _qgis_string(value: Any) -> str:
    if hasattr(value, "data"):
        try:
            data = value.data()
            if isinstance(data, bytes):
                return data.decode("utf-8")
        except Exception:
            pass
    return str(value)


def _provider_uri_parts(source: str) -> dict[str, list[str]]:
    return parse_qs(str(source), keep_blank_values=True)


def _normalize_layer_source_path(source: str) -> str:
    if _is_remote_or_provider_uri(source):
        return source
    path_part, separator, suffix = source.partition("|")
    if not path_part or _is_remote_or_provider_uri(path_part):
        return source
    if _looks_like_path_value(path_part):
        return _safe_workspace_path(path_part) + separator + suffix
    return source


def _is_encoded_provider_uri(source: str) -> bool:
    lowered = source.strip().lower()
    if lowered.startswith(("http://", "https://", "file://", "ftp://")):
        return False
    if Path(source).drive:
        return False
    return "=" in lowered and any(
        token in lowered
        for token in ("type=", "url=", "dbname=", "service=", "host=")
    )


def _is_remote_or_provider_uri(source: str) -> bool:
    lowered = source.lower()
    if lowered.startswith(("http://", "https://", "file://", "ftp://")):
        return True
    if "://" in lowered:
        return True
    if lowered.startswith(("type=", "url=", "dbname=", "service=", "host=")):
        return True
    return "=" in lowered and not Path(source).drive


def _source_identity_values(source: str) -> set[str]:
    values = {source, _normalize_layer_source_path(source)}
    if _is_encoded_provider_uri(source):
        parts = _provider_uri_parts(source)
        for key in ("url", "path", "file", "filename"):
            for value in parts.get(key, []):
                if value:
                    values.add(value)
                    values.add(_normalize_layer_source_path(value))
    return {value for value in values if value}


def _layer_name_from_source(source: str, layer_type: str) -> str:
    path_part = source.split("|", 1)[0]
    if not _is_remote_or_provider_uri(path_part):
        stem = Path(path_part).stem
        if stem:
            return stem
    return layer_type.replace("_", " ").title()


def _layer_summary(layer: Any) -> dict[str, Any]:
    provider = ""
    try:
        provider = layer.providerType()
    except Exception:
        provider = ""
    try:
        layer_type_value = int(layer.type())
    except Exception:
        try:
            layer_type_value = str(layer.type())
        except Exception:
            layer_type_value = ""
    return {
        "id": layer.id(),
        "name": layer.name(),
        "source": layer.source(),
        "type": layer_type_value,
        "provider": provider,
        "valid": layer.isValid(),
    }


def _layer_remove_selectors(arguments: dict[str, Any]) -> dict[str, set[str]]:
    return {
        "ids": _selector_values(arguments, "layer_id", "layer_ids"),
        "names": _selector_values(arguments, "name", "names"),
        "sources": {
            identity
            for value in _selector_values(arguments, "source", "sources")
            for identity in _source_identity_values(value)
        },
    }


def _selector_values(arguments: dict[str, Any], single_key: str, many_key: str) -> set[str]:
    values = set()
    single = arguments.get(single_key)
    if isinstance(single, str) and single:
        values.add(single)
    many = arguments.get(many_key) or []
    if isinstance(many, list):
        values.update(str(value) for value in many if str(value))
    return values


def _layer_matches_selectors(layer: Any, selectors: dict[str, set[str]]) -> bool:
    if selectors["ids"] and layer.id() in selectors["ids"]:
        return True
    if selectors["ids"]:
        return False
    matched = False
    if selectors["names"]:
        matched = True
        if layer.name() not in selectors["names"]:
            return False
    if selectors["sources"]:
        matched = True
        sources = _source_identity_values(layer.source())
        if not sources & selectors["sources"]:
            return False
    return matched


def _layers_matching_selectors(
    layers: list[Any],
    selectors: dict[str, set[str]],
    allow_multiple: bool = False,
) -> list[Any]:
    matches = [
        layer
        for layer in layers
        if _layer_matches_selectors(layer, selectors)
    ]
    if selectors["ids"] or allow_multiple or len(matches) <= 1:
        return matches
    raise RuntimeError(
        "Layer selector matched multiple layers. Use layer_id/layer_ids "
        "or set allow_multiple to true."
    )


def _algorithm_matches_category(algorithm: Any, category: str) -> bool:
    text = " ".join(
        [
            algorithm.id(),
            algorithm.name(),
            algorithm.displayName(),
            algorithm.group(),
        ]
    ).lower()
    if category == "vector":
        return any(token in text for token in ("vector", "feature", "geometry", "attribute"))
    if category == "raster":
        return any(token in text for token in ("raster", "grid", "dem", "cell"))
    return True


def _parameter_metadata(parameter: Any) -> dict[str, Any]:
    optional = False
    try:
        optional_flag = getattr(parameter, "FlagOptional", None)
        optional = bool(optional_flag is not None and parameter.flags() & optional_flag)
    except Exception:
        optional = False
    return {
        "name": parameter.name(),
        "description": parameter.description(),
        "type": parameter.type(),
        "optional": optional,
    }


def _safe_workspace_path(value: str | Path) -> str:
    candidate = Path(value).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd().resolve() / candidate
    candidate = candidate.resolve(strict=False)
    return str(candidate)


def _sanitize_processing_parameters(
    value: Any,
    parameter_definitions: Any | None = None,
    key: str = "",
) -> Any:
    if parameter_definitions is not None and isinstance(value, dict):
        definitions_by_name = {
            str(parameter.name()).lower(): parameter
            for parameter in parameter_definitions
        }
        return {
            item_key: _sanitize_processing_value(
                item_value,
                str(item_key),
                _processing_parameter_accepts_workspace_path(
                    definitions_by_name.get(str(item_key).lower())
                ),
                str(item_key).lower() in definitions_by_name,
            )
            for item_key, item_value in value.items()
        }
    return _sanitize_processing_value(value, key, False, False)


def _sanitize_processing_value(
    value: Any,
    key: str,
    parameter_accepts_paths: bool,
    known_parameter: bool,
) -> Any:
    if isinstance(value, dict):
        return {
            item_key: _sanitize_processing_value(
                item_value,
                str(item_key),
                parameter_accepts_paths,
                False,
            )
            for item_key, item_value in value.items()
        }
    if isinstance(value, list):
        return [
            _sanitize_processing_value(
                item,
                key,
                parameter_accepts_paths,
                known_parameter,
            )
            for item in value
        ]
    if isinstance(value, str) and (
        _looks_like_processing_path_value(value)
        and (parameter_accepts_paths or (not known_parameter and _looks_like_file_parameter(key, value)))
    ):
        return _safe_workspace_path(value)
    return value


def _processing_parameter_accepts_workspace_path(parameter: Any | None) -> bool:
    if parameter is None:
        return False
    type_text = _processing_parameter_type_text(parameter)
    class_text = _processing_parameter_class_text(parameter)
    path_type_tokens = (
        "destination",
        "file",
        "folder",
        "maplayer",
        "mesh",
        "multilayer",
        "multiplelayers",
        "raster",
        "sink",
        "source",
        "vector",
    )
    path_class_tokens = (
        "qgsprocessingparameterfeature",
        "qgsprocessingparameterfile",
        "qgsprocessingparameterfolder",
        "qgsprocessingparametermaplayer",
        "qgsprocessingparametermeshlayer",
        "qgsprocessingparametermultiplelayers",
        "qgsprocessingparameterraster",
        "qgsprocessingparametersink",
        "qgsprocessingparametervector",
    )
    return any(token in type_text for token in path_type_tokens) or any(
        token in class_text for token in path_class_tokens
    )


def _processing_parameter_type_text(parameter: Any) -> str:
    try:
        return str(parameter.type()).lower()
    except Exception:
        return ""


def _processing_parameter_class_text(parameter: Any) -> str:
    for accessor in ("className", "typeName"):
        try:
            value = getattr(parameter, accessor)()
        except Exception:
            continue
        if value:
            return str(value).lower()
    return type(parameter).__name__.lower()


def _looks_like_file_parameter(key: str, value: str) -> bool:
    if not _looks_like_processing_path_value(value):
        return False
    lowered_key = key.lower()
    if any(token in lowered_key for token in ("path", "file", "folder", "output", "destination")):
        return True
    if lowered_key in ("input", "source", "uri", "database"):
        return _looks_like_path_value(value)
    return Path(value).is_absolute()


def _looks_like_processing_path_value(value: str) -> bool:
    if not value or value.startswith(("memory:", "TEMPORARY_OUTPUT", "qgis:")):
        return False
    return _looks_like_path_value(value)


def _looks_like_path_value(value: str) -> bool:
    candidate = Path(value)
    return candidate.is_absolute() or "/" in value or "\\" in value or bool(candidate.suffix)
