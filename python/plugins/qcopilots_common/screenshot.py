"""Screenshots of QGIS windows for the QCopilots capture_qgis_screenshot tool.

The capture helpers run inside QGIS on the GUI thread. The result helpers at
the end of this module run in the stdlib-only MCP server processes, so Qt is
only imported inside the functions that need it.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import base64
import math
import time
from collections.abc import Callable, Mapping
from typing import Any


TOOL_NAME = "capture_qgis_screenshot"
BRIDGE_OPERATION = "interactive_capture_screenshot"

TARGETS = ("map_canvas", "main_window", "active_window", "layout_designer")
CAPTURE_METHODS = ("widget", "screen")
IMAGE_FORMATS = ("auto", "png", "jpeg")
DEFAULT_TARGET = "map_canvas"
DEFAULT_CAPTURE_METHOD = "widget"
DEFAULT_IMAGE_FORMAT = "auto"
DEFAULT_INCLUDE_QCOPILOTS_PANEL = True
MIN_MAX_DIMENSION = 256
MAX_MAX_DIMENSION = 3840
DEFAULT_MAX_DIMENSION = 2560
MIN_JPEG_QUALITY = 40
MAX_JPEG_QUALITY = 95
DEFAULT_JPEG_QUALITY = 95
FALLBACK_JPEG_QUALITIES = (85, 70)
MAX_WAIT_FOR_RENDER_MS = 10000
DEFAULT_WAIT_FOR_RENDER_MS = 10000
RENDER_POLL_INTERVAL_SECONDS = 0.2

# base64 编码后仍低于 Claude API 的 10 MB 单图上限和 QgsQCopilotsMcpBridge 的 16 MiB 响应上限。
MAX_IMAGE_BYTES = 7 * 1024 * 1024
SHRINK_FACTOR = 0.8
MIN_SHRINK_EDGE = 64

QCOPILOTS_DOCK_OBJECT_NAME = "QCopilotsDock"
QCOPILOTS_MASK_COLOR = "#4a5160"
QCOPILOTS_MASK_LABEL = "QCopilots panel hidden"
MAX_VISIBLE_WINDOWS = 20
MAX_SUMMARY_TITLE_CHARS = 120

CANVAS_STILL_RENDERING = "canvas_still_rendering"

_ARGUMENT_NAMES = frozenset(
    {
        "target",
        "layout_name",
        "region",
        "include_qcopilots_panel",
        "capture_method",
        "max_dimension",
        "format",
        "jpeg_quality",
        "wait_for_render_ms",
    }
)


# Argument handling (stdlib only)


def normalize_screenshot_arguments(arguments: Mapping[str, Any] | None) -> dict[str, Any]:
    """Validate bridge arguments and fill in the documented defaults."""

    arguments = dict(arguments or {})
    unknown = sorted(set(arguments) - _ARGUMENT_NAMES)
    if unknown:
        raise ValueError("Unknown screenshot arguments: " + ", ".join(unknown))

    target = arguments.get("target", DEFAULT_TARGET)
    if target not in TARGETS:
        raise ValueError("target must be one of " + ", ".join(TARGETS))

    layout_name = arguments.get("layout_name")
    if layout_name is not None and (
        not isinstance(layout_name, str) or not layout_name.strip()
    ):
        raise ValueError("layout_name must be a non-empty string")
    if target == "layout_designer" and layout_name is None:
        raise ValueError(
            "layout_name is required when target is layout_designer. "
            "Use list_print_layouts to find the layout names."
        )

    region = arguments.get("region")
    if region is not None:
        if (
            not isinstance(region, (list, tuple))
            or len(region) != 4
            or not all(_is_integer(value) for value in region)
        ):
            raise ValueError(
                "region must be [x, y, width, height] with 4 integers in logical pixels"
            )
        region = [int(value) for value in region]
        if region[0] < 0 or region[1] < 0 or region[2] < 1 or region[3] < 1:
            raise ValueError(
                "region x and y must be 0 or more and width and height must be 1 or more"
            )

    include_panel = arguments.get(
        "include_qcopilots_panel", DEFAULT_INCLUDE_QCOPILOTS_PANEL
    )
    if not isinstance(include_panel, bool):
        raise ValueError("include_qcopilots_panel must be a boolean")

    capture_method = arguments.get("capture_method", DEFAULT_CAPTURE_METHOD)
    if capture_method not in CAPTURE_METHODS:
        raise ValueError("capture_method must be one of " + ", ".join(CAPTURE_METHODS))

    image_format = arguments.get("format", DEFAULT_IMAGE_FORMAT)
    if image_format not in IMAGE_FORMATS:
        raise ValueError("format must be one of " + ", ".join(IMAGE_FORMATS))

    return {
        "target": target,
        "layout_name": layout_name,
        "region": region,
        "include_qcopilots_panel": include_panel,
        "capture_method": capture_method,
        "max_dimension": _bounded_integer(
            arguments,
            "max_dimension",
            DEFAULT_MAX_DIMENSION,
            MIN_MAX_DIMENSION,
            MAX_MAX_DIMENSION,
        ),
        "format": image_format,
        "jpeg_quality": _bounded_integer(
            arguments,
            "jpeg_quality",
            DEFAULT_JPEG_QUALITY,
            MIN_JPEG_QUALITY,
            MAX_JPEG_QUALITY,
        ),
        "wait_for_render_ms": _bounded_integer(
            arguments,
            "wait_for_render_ms",
            DEFAULT_WAIT_FOR_RENDER_MS,
            0,
            MAX_WAIT_FOR_RENDER_MS,
        ),
    }


def _is_integer(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _bounded_integer(
    arguments: Mapping[str, Any],
    name: str,
    default: int,
    minimum: int,
    maximum: int,
) -> int:
    value = arguments.get(name, default)
    if not _is_integer(value) or value < minimum or value > maximum:
        raise ValueError(f"{name} must be an integer from {minimum} to {maximum}")
    return int(value)


# Capture inside QGIS (Qt, GUI thread only)


def capture_qgis_screenshot(
    iface: Any,
    arguments: Mapping[str, Any] | None,
    application: Any = None,
) -> dict[str, Any]:
    """Capture a QGIS window and return the base64 image with its metadata.

    While the map canvas shown in the target is still rendering and
    wait_for_render_ms is positive, return a pending_render result at once so
    the caller can poll without blocking the GUI thread.
    """

    options = normalize_screenshot_arguments(arguments)
    assert_gui_thread()
    if iface is None:
        raise RuntimeError("QGIS interface is not available, so no screenshot can be taken")
    if application is None:
        from qgis.PyQt.QtWidgets import QApplication

        application = QApplication

    warnings: list[str] = []
    widget = resolve_capture_target(
        iface,
        options["target"],
        options["layout_name"],
        application,
        warnings,
    )
    canvas = _map_canvas(iface)
    shows_canvas = canvas is not None and (
        widget is canvas or bool(widget.isAncestorOf(canvas))
    )
    canvas_meta = canvas_metadata(canvas) if shows_canvas else None
    if canvas_meta is not None and canvas_meta["is_drawing"]:
        if options["wait_for_render_ms"] > 0:
            return {
                "pending_render": True,
                "target": options["target"],
                "wait_for_render_ms": options["wait_for_render_ms"],
                "canvas": canvas_meta,
            }
        warnings.append(CANVAS_STILL_RENDERING)

    geometry = capture_geometry(
        widget,
        options["capture_method"],
        include_frame=options["target"] != "map_canvas",
    )
    logical_width, logical_height = geometry["logical_size"]
    region = options["region"]
    if region is not None:
        validate_region(region, geometry["logical_size"])
    capture_rect = region or [0, 0, logical_width, logical_height]

    image, device_pixel_ratio = grab_image(widget, geometry)
    panel_rect = qcopilots_panel_rect(iface, widget, geometry)
    panel_status = "not_visible"
    if panel_rect is not None and _rects_intersect(panel_rect, capture_rect):
        if options["include_qcopilots_panel"]:
            panel_status = "included"
        else:
            mask_image_rect(image, panel_rect, device_pixel_ratio)
            panel_status = "masked"
    if options["capture_method"] == "screen":
        warnings.append("screen_capture_may_show_other_applications")
    warnings.extend(geometry["warnings"])

    image = crop_image(image, device_pixel_ratio, region)
    if max(image.width(), image.height()) > options["max_dimension"]:
        warnings.append("downscaled_to_max_dimension")
    image = scale_to_max_dimension(image, options["max_dimension"])
    data, image_format, image = encode_image(
        image,
        options["format"],
        options["jpeg_quality"],
        warnings,
    )

    result = {
        "image_base64": base64.b64encode(data).decode("ascii"),
        "mime_type": "image/png" if image_format == "png" else "image/jpeg",
        "target": options["target"],
        "capture_method": options["capture_method"],
        "window_title": _window_title(widget),
        "logical_size": [logical_width, logical_height],
        "device_pixel_ratio": _finite(device_pixel_ratio, 4),
        "image_size": [image.width(), image.height()],
        "scale": _finite(image.width() / capture_rect[2], 4),
        "format": image_format,
        "bytes": len(data),
        "qcopilots_panel": panel_status,
        "region": region,
        "warnings": warnings,
        "visible_windows": visible_windows(application),
    }
    if options["target"] == "layout_designer":
        result["layout_name"] = options["layout_name"]
    if canvas_meta is not None:
        result["canvas"] = canvas_meta
    return result


def assert_gui_thread() -> None:
    from qgis.PyQt.QtCore import QCoreApplication, QThread

    app = QCoreApplication.instance()
    if app is None:
        raise RuntimeError("No Qt application is running, so no screenshot can be taken")
    if QThread.currentThread() != app.thread():
        raise RuntimeError("QGIS screenshots must be taken on the QGIS GUI thread")


def resolve_capture_target(
    iface: Any,
    target: str,
    layout_name: str | None,
    application: Any,
    warnings: list[str],
) -> Any:
    if target == "map_canvas":
        canvas = _map_canvas(iface)
        if canvas is None:
            raise RuntimeError("The QGIS map canvas is not available")
        return canvas
    if target == "main_window":
        return _require_main_window(iface)
    if target == "active_window":
        return _active_window(iface, application, warnings)
    if target == "layout_designer":
        return _layout_designer_window(iface, layout_name or "")
    raise ValueError("target must be one of " + ", ".join(TARGETS))


def _map_canvas(iface: Any) -> Any:
    map_canvas = getattr(iface, "mapCanvas", None)
    return map_canvas() if callable(map_canvas) else None


def _main_window(iface: Any) -> Any:
    main_window = getattr(iface, "mainWindow", None)
    return main_window() if callable(main_window) else None


def _require_main_window(iface: Any) -> Any:
    window = _main_window(iface)
    if window is None:
        raise RuntimeError("The QGIS main window is not available")
    return window


def _active_window(iface: Any, application: Any, warnings: list[str]) -> Any:
    skipped_panel = False
    for getter in ("activeModalWidget", "activePopupWidget", "activeWindow"):
        widget = getattr(application, getter)()
        if widget is None:
            continue
        if is_qcopilots_panel(widget):
            skipped_panel = True
            continue
        return widget if widget.isWindow() else widget.window()
    warnings.append(
        "active_window_is_qcopilots_panel_used_main_window"
        if skipped_panel
        else "no_active_window_used_main_window"
    )
    return _require_main_window(iface)


def is_qcopilots_panel(widget: Any) -> bool:
    current = widget
    while current is not None:
        if current.objectName() == QCOPILOTS_DOCK_OBJECT_NAME:
            return True
        current = current.parentWidget()
    return False


def _layout_designer_window(iface: Any, layout_name: str) -> Any:
    open_designers = getattr(iface, "openLayoutDesigners", None)
    designers = list(open_designers() or []) if callable(open_designers) else []
    open_names = []
    for designer in designers:
        name = _designer_layout_name(designer)
        if name == layout_name:
            window = designer.window()
            if window is None:
                break
            return window
        if name:
            open_names.append(name)
    listed = ", ".join(f'"{name}"' for name in open_names) or "none"
    message = (
        f'No open layout designer shows the layout "{layout_name}". '
        f"Open layout designers: {listed}. "
    )
    if _project_has_layout(layout_name):
        message += (
            "The layout exists in the project. Ask the user to open it in the "
            "layout designer, or use export_print_layout for a rendered file."
        )
    else:
        message += (
            "The project has no layout with this name. Use list_print_layouts "
            "to see the layout names."
        )
    raise RuntimeError(message)


def _designer_layout_name(designer: Any) -> str:
    for getter in ("masterLayout", "layout"):
        method = getattr(designer, getter, None)
        layout = method() if callable(method) else None
        name = getattr(layout, "name", None)
        if callable(name):
            return str(name())
    return ""


def _project_has_layout(layout_name: str) -> bool:
    try:
        from qgis.core import QgsProject

        return QgsProject.instance().layoutManager().layoutByName(layout_name) is not None
    except Exception:
        return False


def capture_geometry(
    widget: Any,
    capture_method: str,
    include_frame: bool = True,
) -> dict[str, Any]:
    """Return the logical capture size and, for screen capture, its screen area.

    Screen captures of windows include the title bar and frame when
    include_frame is true.
    """

    if capture_method == "widget":
        return {
            "method": "widget",
            "logical_size": [max(1, widget.width()), max(1, widget.height())],
            "origin": None,
            "screen": None,
            "warnings": [],
        }

    from qgis.PyQt.QtCore import QPoint, QRect
    from qgis.PyQt.QtGui import QGuiApplication

    window = widget.window()
    if not widget.isVisible() or window.isMinimized():
        raise RuntimeError(
            "The target is not visible on screen. Restore the QGIS window or use "
            "capture_method widget."
        )
    if include_frame and widget.isWindow():
        global_rect = QRect(widget.frameGeometry())
    else:
        global_rect = QRect(widget.mapToGlobal(QPoint(0, 0)), widget.size())
    screen = widget.screen() or QGuiApplication.screenAt(global_rect.center())
    if screen is None:
        screen = QGuiApplication.primaryScreen()
    if screen is None:
        raise RuntimeError("No screen is available for capture_method screen")
    screen_rect = screen.geometry()
    visible = global_rect.intersected(screen_rect)
    if visible.isEmpty():
        raise RuntimeError(
            "The target is outside the screen. Use capture_method widget instead."
        )
    warnings = []
    full_area = max(1, global_rect.width() * global_rect.height())
    if visible.width() * visible.height() < 0.95 * full_area:
        warnings.append("target_partly_off_screen")
    return {
        "method": "screen",
        "logical_size": [visible.width(), visible.height()],
        "origin": visible.topLeft(),
        "screen": screen,
        "screen_rect": screen_rect,
        "warnings": warnings,
    }


def validate_region(region: list[int], logical_size: list[int]) -> None:
    x, y, width, height = region
    logical_width, logical_height = logical_size
    if x + width > logical_width or y + height > logical_height:
        raise ValueError(
            f"region [{x}, {y}, {width}, {height}] is outside the captured target, "
            f"which is {logical_width} x {logical_height} logical pixels"
        )


def grab_image(widget: Any, geometry: Mapping[str, Any]) -> tuple[Any, float]:
    """Grab the target and return a QImage with a device pixel ratio of 1."""

    if geometry["method"] == "widget":
        pixmap = widget.grab()
    else:
        origin = geometry["origin"]
        screen_rect = geometry["screen_rect"]
        width, height = geometry["logical_size"]
        pixmap = geometry["screen"].grabWindow(
            0,
            origin.x() - screen_rect.x(),
            origin.y() - screen_rect.y(),
            width,
            height,
        )
    if pixmap is None or pixmap.isNull() or pixmap.width() <= 0 or pixmap.height() <= 0:
        raise RuntimeError("Qt returned an empty screenshot for the target")
    logical_width = geometry["logical_size"][0]
    device_pixel_ratio = float(pixmap.devicePixelRatio() or 1.0)
    if abs(pixmap.width() - logical_width * device_pixel_ratio) > 2:
        device_pixel_ratio = pixmap.width() / float(logical_width)
    image = _paintable_image(pixmap.toImage())
    image.setDevicePixelRatio(1.0)
    return image, device_pixel_ratio


def _paintable_image(image: Any) -> Any:
    from qgis.PyQt.QtGui import QImage

    paintable = (
        QImage.Format.Format_RGB32,
        QImage.Format.Format_ARGB32,
        QImage.Format.Format_ARGB32_Premultiplied,
    )
    if image.format() in paintable:
        return image
    return image.convertToFormat(QImage.Format.Format_ARGB32_Premultiplied)


def qcopilots_panel_rect(
    iface: Any,
    widget: Any,
    geometry: Mapping[str, Any],
) -> list[int] | None:
    """Return the visible QCopilots panel rectangle in logical capture pixels."""

    from qgis.PyQt.QtCore import QPoint, QRect

    dock = find_qcopilots_panel(iface, widget)
    if dock is None or dock is widget or not dock.isVisible():
        return None
    if geometry["method"] == "widget":
        if dock.isWindow() or not widget.isAncestorOf(dock):
            return None
        rect = QRect(dock.mapTo(widget, QPoint(0, 0)), dock.size())
    else:
        if dock.isWindow():
            rect = QRect(dock.frameGeometry())
        else:
            rect = QRect(dock.mapToGlobal(QPoint(0, 0)), dock.size())
        origin = geometry["origin"]
        rect.translate(-origin.x(), -origin.y())
    width, height = geometry["logical_size"]
    rect = rect.intersected(QRect(0, 0, width, height))
    if rect.isEmpty():
        return None
    return [rect.x(), rect.y(), rect.width(), rect.height()]


def find_qcopilots_panel(iface: Any, widget: Any = None) -> Any:
    from qgis.PyQt.QtWidgets import QDockWidget

    for owner in (_main_window(iface), widget.window() if widget is not None else None):
        if owner is None:
            continue
        if owner.objectName() == QCOPILOTS_DOCK_OBJECT_NAME:
            return owner
        dock = owner.findChild(QDockWidget, QCOPILOTS_DOCK_OBJECT_NAME)
        if dock is not None:
            return dock
    return None


def _rects_intersect(first: list[int], second: list[int]) -> bool:
    return (
        first[0] < second[0] + second[2]
        and second[0] < first[0] + first[2]
        and first[1] < second[1] + second[3]
        and second[1] < first[1] + first[3]
    )


def _device_rect(rect: list[int], device_pixel_ratio: float, image: Any) -> Any:
    from qgis.PyQt.QtCore import QRect

    left = int(round(rect[0] * device_pixel_ratio))
    top = int(round(rect[1] * device_pixel_ratio))
    right = int(round((rect[0] + rect[2]) * device_pixel_ratio))
    bottom = int(round((rect[1] + rect[3]) * device_pixel_ratio))
    device = QRect(left, top, max(1, right - left), max(1, bottom - top))
    return device.intersected(image.rect())


def mask_image_rect(image: Any, rect: list[int], device_pixel_ratio: float) -> None:
    """Cover a logical rectangle of the image with a solid block and a label."""

    from qgis.PyQt.QtCore import QRectF, Qt
    from qgis.PyQt.QtGui import QColor, QPainter, QTextOption

    device = _device_rect(rect, device_pixel_ratio, image)
    if device.isEmpty():
        return
    painter = QPainter(image)
    try:
        painter.fillRect(device, QColor(QCOPILOTS_MASK_COLOR))
        if device.width() >= 48 and device.height() >= 16:
            font = painter.font()
            font.setPixelSize(
                max(10, min(int(round(14 * device_pixel_ratio)), device.height() // 3))
            )
            painter.setFont(font)
            painter.setPen(QColor("#ffffff"))
            option = QTextOption(Qt.AlignmentFlag.AlignCenter)
            option.setWrapMode(QTextOption.WrapMode.WordWrap)
            painter.drawText(
                QRectF(device.adjusted(4, 4, -4, -4)),
                QCOPILOTS_MASK_LABEL,
                option,
            )
    finally:
        painter.end()


def crop_image(image: Any, device_pixel_ratio: float, region: list[int] | None) -> Any:
    if region is None:
        return image
    device = _device_rect(region, device_pixel_ratio, image)
    if device.isEmpty():
        raise ValueError("region does not overlap the captured image")
    return image.copy(device)


def scale_to_max_dimension(image: Any, max_dimension: int) -> Any:
    from qgis.PyQt.QtCore import Qt

    width = image.width()
    height = image.height()
    long_edge = max(width, height)
    if long_edge <= max_dimension:
        return image
    if width >= height:
        new_width = max_dimension
        new_height = max(1, int(round(height * max_dimension / width)))
    else:
        new_height = max_dimension
        new_width = max(1, int(round(width * max_dimension / height)))
    scaled = image.scaled(
        new_width,
        new_height,
        Qt.AspectRatioMode.IgnoreAspectRatio,
        Qt.TransformationMode.SmoothTransformation,
    )
    scaled.setDevicePixelRatio(1.0)
    return scaled


def encode_image(
    image: Any,
    image_format: str,
    jpeg_quality: int,
    warnings: list[str],
    max_bytes: int = MAX_IMAGE_BYTES,
) -> tuple[bytes, str, Any]:
    """Encode within the size budget and return (data, format, final image).

    auto keeps lossless PNG while it fits, then tries JPEG at jpeg_quality and
    at each lower fallback quality. Every format then shrinks the image in 0.8
    steps until it fits max_bytes.
    """

    attempts = []
    if image_format != "jpeg":
        attempts.append(("png", None))
    if image_format != "png":
        attempts.append(("jpeg", jpeg_quality))
        attempts.extend(
            ("jpeg", quality)
            for quality in FALLBACK_JPEG_QUALITIES
            if quality < jpeg_quality
        )

    for encoded_format, quality in attempts:
        data = _encode(image, encoded_format, quality)
        if len(data) <= max_bytes:
            warnings.extend(
                _encoding_warnings(image_format, encoded_format, quality, jpeg_quality)
            )
            return data, encoded_format, image

    encoded_format, quality = attempts[-1]
    current = image
    while True:
        width = int(current.width() * SHRINK_FACTOR)
        height = int(current.height() * SHRINK_FACTOR)
        if min(width, height) < 1 or max(width, height) < MIN_SHRINK_EDGE:
            raise RuntimeError(
                f"The screenshot could not be encoded within {max_bytes} bytes. "
                "Use a region or a smaller max_dimension."
            )
        current = scale_to_max_dimension(current, max(width, height))
        data = _encode(current, encoded_format, quality)
        if len(data) <= max_bytes:
            warnings.extend(
                _encoding_warnings(image_format, encoded_format, quality, jpeg_quality)
            )
            warnings.append("downscaled_to_fit_size_budget")
            return data, encoded_format, current


def _encoding_warnings(
    requested_format: str,
    encoded_format: str,
    quality: int | None,
    jpeg_quality: int,
) -> list[str]:
    warnings = []
    if requested_format == "auto" and encoded_format == "jpeg":
        warnings.append("converted_to_jpeg_to_fit_size_budget")
    if quality is not None and quality != jpeg_quality:
        warnings.append(f"jpeg_quality_lowered_to_{quality}")
    return warnings


def _encode(image: Any, image_format: str, quality: int | None) -> bytes:
    from qgis.PyQt.QtCore import QBuffer, QByteArray, QIODevice

    if image_format == "jpeg":
        image = _rgb_on_white(image)
    buffer_data = QByteArray()
    buffer = QBuffer(buffer_data)
    if not buffer.open(QIODevice.OpenModeFlag.WriteOnly):
        raise RuntimeError("Could not open an in-memory buffer for the screenshot")
    try:
        if image_format == "jpeg":
            saved = image.save(buffer, "JPEG", int(quality))
        else:
            saved = image.save(buffer, "PNG")
    finally:
        buffer.close()
    if not saved:
        raise RuntimeError(f"Qt could not encode the screenshot as {image_format.upper()}")
    return bytes(buffer_data)


def _rgb_on_white(image: Any) -> Any:
    from qgis.PyQt.QtCore import Qt
    from qgis.PyQt.QtGui import QImage, QPainter

    background = QImage(image.size(), QImage.Format.Format_RGB32)
    background.setDevicePixelRatio(1.0)
    background.fill(Qt.GlobalColor.white)
    painter = QPainter(background)
    try:
        painter.drawImage(0, 0, image)
    finally:
        painter.end()
    return background.convertToFormat(QImage.Format.Format_RGB888)


def canvas_metadata(canvas: Any) -> dict[str, Any]:
    crs = canvas.mapSettings().destinationCrs()
    crs_id = crs.authid() if crs.isValid() else ""
    if not crs_id and crs.isValid():
        crs_id = crs.userFriendlyIdentifier()
    extent = canvas.extent()
    return {
        "crs": crs_id or None,
        "extent": [
            _finite(extent.xMinimum(), 6),
            _finite(extent.yMinimum(), 6),
            _finite(extent.xMaximum(), 6),
            _finite(extent.yMaximum(), 6),
        ],
        "scale": _finite(canvas.scale(), 2),
        "rotation": _finite(canvas.rotation(), 6),
        "is_drawing": bool(canvas.isDrawing()),
    }


def visible_windows(application: Any) -> list[dict[str, Any]]:
    """Describe visible top-level QGIS windows so a later call can pick a target."""

    from qgis.PyQt.QtCore import Qt

    skipped_types = {
        Qt.WindowType.Popup,
        Qt.WindowType.ToolTip,
        Qt.WindowType.SplashScreen,
        Qt.WindowType.Desktop,
    }
    top_level_widgets = getattr(application, "topLevelWidgets", None)
    if not callable(top_level_widgets):
        return []
    active = application.activeWindow()
    windows = []
    for widget in top_level_widgets():
        try:
            if not widget.isWindow() or not widget.isVisible():
                continue
            if widget.windowType() in skipped_types:
                continue
            title = str(widget.windowTitle() or "")
            modal = bool(widget.isModal())
            if not title and not modal:
                continue
            windows.append(
                {
                    "title": title,
                    "class": str(widget.metaObject().className()),
                    "modal": modal,
                    "active": widget is active or bool(widget.isActiveWindow()),
                }
            )
        except RuntimeError:
            # The C++ widget was deleted while the list was collected.
            continue
    windows.sort(key=lambda item: (not item["active"], not item["modal"], item["title"].lower()))
    return windows[:MAX_VISIBLE_WINDOWS]


def _window_title(widget: Any) -> str:
    window = widget.window() if hasattr(widget, "window") else widget
    return str(window.windowTitle() or "")


def _finite(value: Any, digits: int) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(number):
        return None
    return round(number, digits)


# MCP result (stdlib only, runs in the MCP server process)


def capture_screenshot_via_bridge(
    call: Callable[[str, dict[str, Any]], Any],
    arguments: Mapping[str, Any],
    clock: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> Any:
    """Call the bridge, poll while the canvas renders, then build the MCP result.

    The first bridge call receives the arguments unchanged. Results without
    image_base64 are returned unchanged.
    """

    started = clock()
    result = call(BRIDGE_OPERATION, arguments)
    timed_out = False
    if _is_pending_render(result):
        wait_ms = arguments.get("wait_for_render_ms", DEFAULT_WAIT_FOR_RENDER_MS)
        if not _is_integer(wait_ms) or wait_ms < 0:
            wait_ms = DEFAULT_WAIT_FOR_RENDER_MS
        deadline = started + wait_ms / 1000.0
        while _is_pending_render(result):
            remaining = deadline - clock()
            if remaining <= 0:
                break
            sleep(min(RENDER_POLL_INTERVAL_SECONDS, remaining))
            result = call(BRIDGE_OPERATION, arguments)
        if _is_pending_render(result):
            final_arguments = dict(arguments)
            final_arguments["wait_for_render_ms"] = 0
            result = call(BRIDGE_OPERATION, final_arguments)
            timed_out = True
            if _is_pending_render(result):
                from qcopilots_common.mcp_http import ToolError

                raise ToolError("The QGIS map canvas is still rendering. Try again later.")
    if not isinstance(result, Mapping) or "image_base64" not in result:
        return result
    return screenshot_tool_result(
        result,
        [CANVAS_STILL_RENDERING] if timed_out else [],
    )


def _is_pending_render(result: Any) -> bool:
    return isinstance(result, Mapping) and result.get("pending_render") is True


def screenshot_tool_result(
    result: Mapping[str, Any],
    extra_warnings: list[str] | tuple[str, ...] = (),
) -> dict[str, Any]:
    meta = {name: value for name, value in result.items() if name != "image_base64"}
    warnings = [str(warning) for warning in meta.get("warnings") or []]
    for warning in extra_warnings:
        if warning not in warnings:
            warnings.append(warning)
    meta["warnings"] = warnings
    mime_type = str(result.get("mime_type") or "image/png")
    return {
        "content": [
            {"type": "text", "text": screenshot_summary_text(meta)},
            {"type": "image", "data": result["image_base64"], "mimeType": mime_type},
        ],
        "structuredContent": meta,
    }


_TARGET_LABELS = {
    "map_canvas": "the QGIS map canvas",
    "main_window": "the QGIS main window",
    "active_window": "the active QGIS window",
    "layout_designer": "the QGIS layout designer",
}

_PANEL_TEXT = {
    "included": "is included in the image",
    "masked": "is covered by a grey block in the image",
    "not_visible": "is not visible in the image",
}


def screenshot_summary_text(meta: Mapping[str, Any]) -> str:
    """Plain text without Markdown markers, so chat clients show the image."""

    target = str(meta.get("target") or "")
    lines = [
        f"Screenshot of {_TARGET_LABELS.get(target, plain_text(target))}, "
        f'window title "{plain_text(meta.get("window_title"))}".'
    ]
    if meta.get("layout_name"):
        lines.append(f'Print layout "{plain_text(meta.get("layout_name"))}".')

    image_width, image_height = _pair(meta.get("image_size"))
    logical_width, logical_height = _pair(meta.get("logical_size"))
    image_format = str(meta.get("format") or "").upper() or "unknown format"
    size_kib = max(1, int(round(float(meta.get("bytes") or 0) / 1024.0)))
    lines.append(
        f"Image {image_width} x {image_height} pixels, {image_format}, {size_kib} KB. "
        f"Scale {_number(meta.get('scale'))} image pixels per logical pixel. "
        f"Target logical size {logical_width} x {logical_height}, device pixel "
        f"ratio {_number(meta.get('device_pixel_ratio'))}."
    )
    region = meta.get("region")
    if isinstance(region, (list, tuple)) and len(region) == 4:
        lines.append(
            f"Region x {region[0]}, y {region[1]}, width {region[2]}, height "
            f"{region[3]} in logical pixels of the target. Image pixel "
            "coordinates divided by the scale plus the region x and y give "
            "target logical pixels."
        )
    else:
        lines.append(
            "Region: full target. Image pixel coordinates divided by the scale "
            "give target logical pixels for a later region crop."
        )
    panel = _PANEL_TEXT.get(str(meta.get("qcopilots_panel")), "status is unknown")
    lines.append(
        f"Capture method {plain_text(meta.get('capture_method'))}. "
        f"The QCopilots chat panel {panel}."
    )

    canvas = meta.get("canvas")
    if isinstance(canvas, Mapping):
        xmin, ymin, xmax, ymax = (list(canvas.get("extent") or []) + [None] * 4)[:4]
        scale = canvas.get("scale")
        scale_text = (
            f"1:{int(round(scale))}" if isinstance(scale, (int, float)) else "unknown"
        )
        lines.append(
            f"Map canvas CRS {plain_text(canvas.get('crs') or 'unknown')}, extent "
            f"xmin {_number(xmin)}, ymin {_number(ymin)}, xmax {_number(xmax)}, "
            f"ymax {_number(ymax)}, scale {scale_text}, rotation "
            f"{_number(canvas.get('rotation'))} degrees"
            + (", still rendering." if canvas.get("is_drawing") else ".")
        )

    warnings = [plain_text(warning) for warning in meta.get("warnings") or []]
    lines.append("Warnings: " + (", ".join(warnings) if warnings else "none") + ".")

    windows = []
    for window in meta.get("visible_windows") or []:
        if not isinstance(window, Mapping):
            continue
        flags = [plain_text(window.get("class") or "window")]
        if window.get("active"):
            flags.append("active")
        if window.get("modal"):
            flags.append("modal")
        windows.append(f'"{plain_text(window.get("title"))}" ({", ".join(flags)})')
    if windows:
        lines.append("Visible QGIS windows: " + ", ".join(windows) + ".")

    lines.append(
        "The screenshot image is attached to this result. Look at it and "
        "describe what you see. Call capture_qgis_screenshot again after "
        "changes to see a fresh view."
    )
    return "\n".join(lines)


def plain_text(value: Any, max_chars: int = MAX_SUMMARY_TITLE_CHARS) -> str:
    """Make a dynamic string safe for a plain-text summary line."""

    text = " ".join(str(value if value is not None else "").split())
    replacements = {
        "`": "'",
        "|": "/",
        "[": "(",
        "]": ")",
        "<": "(",
        ">": ")",
        "#": "No.",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    while "**" in text:
        text = text.replace("**", "*")
    while "__" in text:
        text = text.replace("__", "_")
    if len(text) > max_chars:
        text = text[: max_chars - 3].rstrip() + "..."
    return text


def _pair(value: Any) -> tuple[Any, Any]:
    if isinstance(value, (list, tuple)) and len(value) == 2:
        return value[0], value[1]
    return "unknown", "unknown"


def _number(value: Any) -> str:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return "unknown"
    if not math.isfinite(value):
        return "unknown"
    if abs(value) >= 1000:
        text = f"{value:.2f}"
    else:
        text = f"{value:.6f}"
    text = text.rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text
