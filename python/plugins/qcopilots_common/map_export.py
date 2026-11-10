"""Private asynchronous rendering for the synchronous map image MCP tool.

Qt objects stay in the GUI runtime. The requesting HTTP thread owns publication;
a renderer which finishes after abandonment can only clean its private stage.

This program is free software under the GNU General Public License, version 2
or (at your option) any later version.
"""

from __future__ import annotations

import logging
import tempfile
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

EXPORT_TIMEOUT_SECONDS = 50.0
_LOGGER = logging.getLogger(__name__)


@dataclass
class MapImageExportPlan:
    path: Path
    world_path: Path
    encoder: str
    overwrite: bool
    expected_versions: list[dict[str, Any]]
    revalidate: Callable[[], None]


@dataclass(eq=False)
class _ExportOperation:
    deadline: float
    done: threading.Event = field(default_factory=threading.Event)
    plan: MapImageExportPlan | None = None
    staging_root: Path | None = None
    world_file_bytes: bytes = b""
    preview: dict[str, Any] | None = None
    error: Exception | None = None
    gui_started: bool = False
    writer_finished: bool = False
    abandoned: bool = False
    publishing: bool = False
    cleanup_claimed: bool = False


class MapImageExportCoordinator:
    """One active export per bridge, including abandoned writers and cleanup."""

    def __init__(
        self,
        dispatcher: Any,
        prepare: Callable[[dict[str, Any]], MapImageExportPlan | dict[str, Any]],
        *,
        iface: Any = None,
        timeout_seconds: float = EXPORT_TIMEOUT_SECONDS,
        clock: Callable[[], float] = time.monotonic,
        runtime_factory: Callable[[Any], Any] | None = None,
    ):
        self._dispatcher = dispatcher
        self.prepare = prepare
        self._iface = iface
        self._timeout = timeout_seconds
        self._clock = clock
        self._runtime_factory = runtime_factory or (
            lambda coordinator: _make_qt_runtime(coordinator, iface)
        )
        self._runtime = None
        self._condition = threading.Condition(threading.RLock())
        self._active: _ExportOperation | None = None
        self._stopping = False
        self.cleanup_residuals: list[str] = []

    def _remaining(self, operation: _ExportOperation) -> float:
        remaining = operation.deadline - self._clock()
        if remaining <= 0:
            raise TimeoutError("Map image export timed out before publication")
        return remaining

    def is_active(self, operation: _ExportOperation) -> bool:
        with self._condition:
            return (
                self._active is operation
                and not self._stopping
                and not operation.abandoned
                and self._clock() < operation.deadline
            )

    def export(self, arguments: dict[str, Any]) -> dict[str, Any]:
        if self._dispatcher is None:
            raise RuntimeError(
                "The QGIS GUI dispatcher is not available for map image export"
            )
        # The production dispatcher has this guard; fakes need no Qt imports.
        is_gui_thread = getattr(self._dispatcher, "is_gui_thread", None)
        if callable(is_gui_thread) and is_gui_thread():
            raise RuntimeError("Map image export must wait outside the QGIS GUI thread")
        operation = _ExportOperation(self._clock() + self._timeout)
        with self._condition:
            while self._active is not None and not self._stopping:
                self._condition.wait(self._remaining(operation))
            if self._stopping:
                raise RuntimeError("service_stopping: Map image exports are stopping")
            self._remaining(operation)
            self._active = operation

        primary_error = None
        try:
            self._dispatcher.call(
                lambda: self._begin(operation, arguments),
                timeout_seconds=self._remaining(operation),
            )
            if not operation.done.wait(self._remaining(operation)):
                raise TimeoutError("Map image export timed out before publication")
            with self._condition:
                self._remaining(operation)
                if operation.abandoned or self._stopping:
                    raise RuntimeError(
                        "service_stopping: Map image export was cancelled"
                    )
                if operation.error is not None:
                    raise operation.error
                if operation.preview is not None:
                    return operation.preview
                if not operation.writer_finished:
                    raise RuntimeError(
                        "Map image renderer did not reach a terminal state"
                    )
                # This is the publication ownership boundary. Shutdown cannot
                # hand this stage to another cleaner while publication runs.
                operation.publishing = True
            return self._publish(operation)
        except Exception as error:
            primary_error = error
            self._abandon(operation)
            raise
        finally:
            with self._condition:
                can_clean = operation.writer_finished and not operation.cleanup_claimed
                if can_clean:
                    operation.cleanup_claimed = True
            if can_clean:
                residuals = self._cleanup_and_release(operation)
                if residuals and primary_error is not None:
                    raise RuntimeError(
                        f"{primary_error}. Map image staging cleanup was incomplete: "
                        + ", ".join(residuals)
                    ) from primary_error

    def _begin(self, operation: _ExportOperation, arguments: dict[str, Any]) -> None:
        with self._condition:
            if self._active is not operation or operation.abandoned:
                return
            operation.gui_started = True
        try:
            if not self.is_active(operation):
                raise TimeoutError("Map image export timed out before rendering")
            if self._runtime is None:
                self._runtime = self._runtime_factory(self)
            self._runtime.begin(operation, arguments)
        except Exception as error:
            self.render_finished(operation, error)

    def preview_finished(
        self, operation: _ExportOperation, result: dict[str, Any]
    ) -> None:
        with self._condition:
            operation.preview = result
        self.render_finished(operation)

    def render_finished(
        self, operation: _ExportOperation, error: Exception | None = None
    ) -> None:
        with self._condition:
            if operation.writer_finished:
                return
            operation.writer_finished = True
            operation.error = error
            operation.done.set()
            clean_abandoned = operation.abandoned and not operation.cleanup_claimed
            if clean_abandoned:
                operation.cleanup_claimed = True
        if clean_abandoned:
            # Filesystem cleanup must not stall the GUI completion callback.
            threading.Thread(
                target=self._cleanup_and_release, args=(operation, True), daemon=True
            ).start()

    def _abandon(self, operation: _ExportOperation) -> None:
        with self._condition:
            if operation.publishing:
                return
            operation.abandoned = True
            operation.done.set()
            if not operation.gui_started:
                operation.writer_finished = True
            runtime = self._runtime
        if runtime is not None and not operation.writer_finished:
            runtime.cancel(operation)

    def shutdown(self) -> None:
        with self._condition:
            self._stopping = True
            operation = self._active
            self._condition.notify_all()
        if operation is not None:
            self._abandon(operation)
        with self._condition:
            runtime = self._runtime
        request_retire = getattr(runtime, "request_retire", None)
        if request_retire is not None:
            request_retire()

    def reopen(self, dispatcher: Any) -> None:
        with self._condition:
            if self._active is not None:
                raise RuntimeError("Map image export cleanup is still pending")
            self._dispatcher = dispatcher
            self._stopping = False

    def _publish(self, operation: _ExportOperation) -> dict[str, Any]:
        from qcopilots_common.bridge import (
            _cleanup_interactive_paths,
            _publish_staged_output_family,
        )

        plan = operation.plan
        staging_root = operation.staging_root
        if plan is None or staging_root is None:
            raise RuntimeError("Map image export has no staged output")
        staged_path = staging_root / plan.path.name
        if not staged_path.is_file() or staged_path.stat().st_size <= 0:
            raise RuntimeError("Map image export produced no output")
        if not operation.world_file_bytes:
            raise RuntimeError("Map image export produced no world file content")
        (staging_root / plan.world_path.name).write_bytes(operation.world_file_bytes)
        plan.revalidate()
        cleanup_residuals = _publish_staged_output_family(
            staging_root,
            plan.path,
            [plan.path, plan.world_path],
            overwrite=plan.overwrite,
            expected_versions=plan.expected_versions,
        )
        # Normal publication includes its cleanup report in the synchronous
        # response. The finally path remains responsible for failed exports.
        cleanup_residuals.extend(_cleanup_interactive_paths([staging_root]))
        result = {
            "path": str(plan.path),
            "world_file_path": str(plan.world_path),
            "encoder": plan.encoder,
            "saved": plan.path.is_file() and plan.path.stat().st_size > 0,
            "bytes_written": plan.path.stat().st_size if plan.path.is_file() else 0,
            "cleanup": {
                "complete": not cleanup_residuals,
                "residual_paths": sorted(set(cleanup_residuals)),
                "retry_recommended": bool(cleanup_residuals),
            },
        }
        with self._condition:
            operation.cleanup_claimed = True
            self.cleanup_residuals = (self.cleanup_residuals + cleanup_residuals)[-64:]
            if self._active is operation:
                self._active = None
                self._condition.notify_all()
        return result

    def _cleanup_and_release(
        self, operation: _ExportOperation, late: bool = False
    ) -> list[str]:
        from qcopilots_common.bridge import _cleanup_interactive_paths

        try:
            residuals = (
                _cleanup_interactive_paths([operation.staging_root])
                if operation.staging_root is not None
                else []
            )
        except Exception:
            residuals = [str(operation.staging_root)] if operation.staging_root else []
        with self._condition:
            self.cleanup_residuals = (self.cleanup_residuals + residuals)[-64:]
            if self._active is operation:
                self._active = None
                self._condition.notify_all()
        if late and residuals:
            _LOGGER.warning(
                "Map image export cleanup after cancellation left staged files: %s",
                ", ".join(residuals),
            )
        return residuals


def _export_settings(canvas: Any) -> Any:
    """Snapshot the current map using the native Save as Image conventions."""
    from qgis.core import (
        Qgis,
        QgsAnnotationLayer,
        QgsExpressionContext,
        QgsExpressionContextScope,
        QgsExpressionContextUtils,
        QgsMapSettings,
        QgsProject,
        QgsSettingsTree,
        QgsTemporalNavigationObject,
    )

    project = QgsProject.instance()
    canvas_settings = canvas.mapSettings()
    # QgsMapSaveDialog obtains the unrotated visible extent before applying
    # rotation to its fresh export settings. Reusing a rotated visible extent
    # would enlarge the view a second time.
    unrotated = QgsMapSettings(canvas_settings)
    unrotated.setRotation(0)
    settings = QgsMapSettings()
    antialias = bool(
        QgsSettingsTree.node("qgis")
        .childSetting("enable-anti-aliasing")
        .valueAsVariant()
    )
    settings.setFlag(Qgis.MapSettingsFlag.Antialiasing, antialias)
    settings.setFlag(Qgis.MapSettingsFlag.HighQualityImageTransforms, antialias)
    settings.setFlag(Qgis.MapSettingsFlag.DrawEditingInfo, False)
    settings.setFlag(Qgis.MapSettingsFlag.DrawSelection, True)
    settings.setRasterizedRenderingPolicy(Qgis.RasterizedRenderingPolicy.PreferVector)
    settings.setRendererUsage(Qgis.RendererUsage.Export)
    settings.setSelectionColor(canvas_settings.selectionColor())
    settings.setDestinationCrs(canvas_settings.destinationCrs())
    settings.setExtent(unrotated.visibleExtent())
    settings.setOutputSize(canvas_settings.outputSize())
    settings.setOutputDpi(canvas_settings.outputDpi())
    settings.setDevicePixelRatio(canvas_settings.devicePixelRatio())
    settings.setBackgroundColor(canvas.canvasColor())
    settings.setRotation(canvas.rotation())
    settings.setEllipsoid(project.ellipsoid())
    settings.setTransformContext(project.transformContext())
    settings.setPathResolver(project.pathResolver())
    layers = list(canvas.layers())
    annotations = project.mainAnnotationLayer()
    if not annotations.isEmpty() and annotations not in layers:
        layers.insert(0, annotations)
    # Match QgsMapCanvas::filterLayersForRender, including annotation visibility
    # links. Test links against the original set before removing invalid layers.
    settings.setLayers(
        [
            layer
            for layer in layers
            if layer is not None
            and layer.isValid()
            and not (
                isinstance(layer, QgsAnnotationLayer)
                and layer.linkedVisibilityLayer() is not None
                and layer.linkedVisibilityLayer() not in layers
            )
        ]
    )
    settings.setLayerStyleOverrides(canvas_settings.layerStyleOverrides())
    settings.setLabelingEngineSettings(canvas_settings.labelingEngineSettings())
    settings.setTemporalRange(canvas_settings.temporalRange())
    settings.setIsTemporal(canvas_settings.isTemporal())
    settings.setZRange(canvas_settings.zRange())
    settings.setElevationShadingRenderer(canvas_settings.elevationShadingRenderer())
    settings.setFrameRate(canvas_settings.frameRate())
    settings.setCurrentFrame(canvas_settings.currentFrame())
    temporal_getter = getattr(canvas, "temporalController", None)
    temporal_controller = temporal_getter() if temporal_getter is not None else None
    if isinstance(
        temporal_controller, QgsTemporalNavigationObject
    ) and temporal_controller.navigationMode() in (
        Qgis.TemporalNavigationMode.Animated,
        Qgis.TemporalNavigationMode.Movie,
    ):
        # The canvas updates these only when its delayed refresh starts. Read
        # the controller now so animated symbol expressions use this frame.
        settings.setFrameRate(temporal_controller.framesPerSecond())
        settings.setCurrentFrame(temporal_controller.currentFrameNumber())
    if canvas.theme():
        settings.setLayerStyleOverrides(
            project.mapThemeCollection().mapThemeStyleOverrides(canvas.theme())
        )
    # Preserve the canvas's native atlas, temporal-controller, default and
    # custom scopes, including their precedence. Only the map-settings scope
    # must be refreshed from this export snapshot, not the previous render.
    canvas_context = canvas.createExpressionContext()
    map_scope = QgsExpressionContextUtils.mapSettingsScope(settings)
    map_scope_index = canvas_context.indexOfScope(map_scope.name())
    if map_scope_index < 0:
        raise RuntimeError("The canvas expression context has no map settings scope")
    context = QgsExpressionContext()
    for index, scope in enumerate(canvas_context.scopes()):
        context.appendScope(
            map_scope if index == map_scope_index else QgsExpressionContextScope(scope)
        )
    settings.setExpressionContext(context)
    if not settings.hasValidSettings():
        raise RuntimeError("The current map has no valid export settings")
    return settings


def _world_file_content(settings: Any) -> bytes:
    """Georeference physical image pixels using the unchanged map snapshot."""
    from qgis.core import QgsMapSettingsUtils

    content = QgsMapSettingsUtils.worldFileContent(settings)
    pixel_ratio = settings.devicePixelRatio()
    if pixel_ratio != 1:
        # Native worldFileContent scales the pixel steps by DPR, but uses a
        # logical half-pixel for the origin. The image task renders physical
        # pixels: their first center is half a physical pixel from the corner.
        # Preserve the native step coefficients, extent, rotation and aspect.
        center = settings.mapToPixel().toMapCoordinatesF(
            0.5 / pixel_ratio, 0.5 / pixel_ratio
        )
        lines = content.splitlines()
        lines[4] = format(center.x(), ".17g")
        lines[5] = format(center.y(), ".17g")
        content = "\r\n".join(lines) + "\r\n"
    return content.encode("utf-8")


def _make_qt_runtime(coordinator: MapImageExportCoordinator, iface: Any) -> Any:
    from qgis.core import QgsApplication, QgsMapRendererTask
    from qgis.PyQt.QtCore import QCoreApplication, QObject, Qt, QThread, pyqtSignal

    from qcopilots_common.canvas import LayerTreeUpdateBarrier
    from qcopilots_common.map_export_overlays import snapshot_map_overlays

    def assert_gui() -> None:
        app = QCoreApplication.instance()
        if app is None or QThread.currentThread() != app.thread():
            raise RuntimeError("Map image renderer objects require the QGIS GUI thread")

    assert_gui()

    class Runtime(QObject):
        cancel_requested = pyqtSignal(object)
        snapshot_requested = pyqtSignal(object)
        retire_requested = pyqtSignal()

        def __init__(self, owner, target_iface):
            super().__init__(QCoreApplication.instance())
            self.coordinator = owner
            self.iface = target_iface
            self.entries: dict[_ExportOperation, dict[str, Any]] = {}
            self.cancel_requested.connect(
                self._cancel, Qt.ConnectionType.QueuedConnection
            )
            self.snapshot_requested.connect(
                self._start_when_ready, Qt.ConnectionType.QueuedConnection
            )
            self.retire_requested.connect(
                self._retire, Qt.ConnectionType.QueuedConnection
            )

        def begin(self, operation, arguments):
            assert_gui()
            # Register ownership before authorization or renderer construction,
            # either of which can invoke provider code with nested event loops.
            self.entries[operation] = {
                "task": None,
                "barrier": None,
                "overlay": None,
                "preparing": True,
            }
            try:
                if not self.coordinator.is_active(operation):
                    raise TimeoutError(
                        "Map image export timed out before authorization"
                    )
                plan = self.coordinator.prepare(arguments)
                if isinstance(plan, dict):
                    self.entries.pop(operation, None)
                    self.coordinator.preview_finished(operation, plan)
                    self._retire()
                    return
                operation.plan = plan
                self.entries[operation]["barrier"] = LayerTreeUpdateBarrier(self.iface)
                self.entries[operation]["preparing"] = False
                self._start_when_ready(operation)
            except Exception as error:
                self._finish(operation, error)

        def _start_when_ready(self, operation):
            assert_gui()
            entry = self.entries.get(operation)
            if entry is None:
                return
            if not self.coordinator.is_active(operation):
                self._finish(
                    operation,
                    TimeoutError("Map image export was cancelled or timed out"),
                )
                return
            try:
                # Each observed tree mutation advances the barrier generation.
                # A new queued update behind the previous callback requires
                # another turn before snapshotting.
                if not entry["barrier"].is_settled():
                    self.snapshot_requested.emit(operation)
                    return
                entry["preparing"] = True
                plan = operation.plan
                settings = _export_settings(self.iface.mapCanvas())
                operation.world_file_bytes = _world_file_content(settings)
                entry["overlay"] = snapshot_map_overlays(self.iface, settings)
                if not self.coordinator.is_active(operation):
                    raise TimeoutError("Map image export timed out before rendering")
                plan.revalidate()
                plan.path.parent.mkdir(parents=True, exist_ok=True)
                operation.staging_root = Path(
                    tempfile.mkdtemp(
                        prefix=f".{plan.path.stem}.qcopilots-stage-",
                        dir=str(plan.path.parent),
                    )
                )
                task = QgsMapRendererTask(
                    settings, str(operation.staging_root / plan.path.name), plan.encoder
                )
                entry["task"] = task
                # Keep the explicit sidecar contract for every supported format,
                # including TIFF, where the native task otherwise embeds it.
                task.setSaveWorldFile(False)
                task.setDependentLayers(settings.layers())
                if entry["overlay"] is not None:
                    task.addDecorations([entry["overlay"]])
                task.renderingComplete.connect(lambda: self._finish(operation))
                task.errorOccurred.connect(
                    lambda code: self._finish(
                        operation,
                        RuntimeError(f"Map image rendering failed (error {code})"),
                    )
                )
                task.taskTerminated.connect(
                    lambda: self._finish(
                        operation, RuntimeError("Map image rendering was cancelled")
                    )
                )
                task.destroyed.connect(
                    lambda: self._finish(
                        operation,
                        RuntimeError("Map image rendering task was destroyed"),
                    )
                )
                if not self.coordinator.is_active(operation):
                    task.cancel()
                    task.deleteLater()
                    raise TimeoutError(
                        "Map image export timed out before task submission"
                    )
                entry["preparing"] = False
                if not QgsApplication.taskManager().addTask(task):
                    task.cancel()
                    task.deleteLater()
                    raise RuntimeError("Map image renderer task was not accepted")
            except Exception as error:
                self._finish(operation, error)

        def cancel(self, operation):
            # Emitting a signal is the only cross-thread Qt operation here.
            if self.coordinator is None:
                return
            if QThread.currentThread() == self.thread():
                self._cancel(operation)
            else:
                self.cancel_requested.emit(operation)

        def _cancel(self, operation):
            assert_gui()
            entry = self.entries.get(operation)
            if entry is None or entry["preparing"]:
                return
            if entry["task"] is not None:
                entry["task"].cancel()
            else:
                self._finish(operation, RuntimeError("Map image export was cancelled"))

        def _finish(self, operation, error=None):
            assert_gui()
            entry = self.entries.pop(operation, None)
            if entry is None:
                return
            if entry["barrier"] is not None:
                try:
                    entry["barrier"].close()
                except Exception as close_error:
                    error = error or close_error
            self.coordinator.render_finished(operation, error)
            self._retire()

        def request_retire(self):
            if self.coordinator is None:
                return
            if QThread.currentThread() == self.thread():
                self._retire()
            else:
                self.retire_requested.emit()

        def _retire(self):
            assert_gui()
            if self.entries or self.coordinator is None:
                return
            owner = self.coordinator
            with owner._condition:
                if not owner._stopping:
                    return
                if owner._runtime is self:
                    owner._runtime = None
            # Do not leave app-parented QObjects retaining a stopped bridge via
            # method closures. Delete on their GUI thread after the last writer.
            self.coordinator = None
            self.iface = None
            self.deleteLater()

    return Runtime(coordinator, iface)
