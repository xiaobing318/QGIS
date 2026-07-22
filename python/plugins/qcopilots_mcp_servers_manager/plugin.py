"""QGIS UI plugin for discovering and managing QCopilots MCP services.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import ipaddress
import json
import re
import socket
from dataclasses import replace
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

from qgis.PyQt.QtCore import QCoreApplication, QObject, QSize, QThread, QTimer, Qt, pyqtSignal
from qgis.PyQt.QtGui import QColor, QIcon, QPainter
from qgis.PyQt.QtWidgets import (
    QAction,
    QApplication,
    QCheckBox,
    QDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QScrollArea,
    QSizePolicy,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from qcopilots_common.bridge import QgisBridgeController
from qcopilots_common.constants import (
    BRIDGE_URL_ENV,
    DEFAULT_BRIDGE_PORT,
    DEFAULT_CORS_ORIGINS,
    DEFAULT_HOST,
)
from qcopilots_common.discovery import discover_service_manifests
from qcopilots_common.logging import configure_logger, qgis_log, service_log_file
from qcopilots_common.manifest import ServiceManifest
from qcopilots_common.menu import add_qcopilots_menu_action, remove_qcopilots_menu_action
from qcopilots_common.process_controller import ProcessController
from qcopilots_common.service_id import is_safe_service_id


TOGGLE_THREAD_WAIT_TIMEOUT_MS = 10000

DEFAULT_STARTUP_SERVICE_IDS = [
    "qcopilots.mcp_server_builtin_tools",
    "qcopilots.mcp_server_interactive_tools",
    "qcopilots.mcp_server_processing_vector",
    "qcopilots.mcp_server_processing_raster",
    "qcopilots.mcp_server_skills",
]
DEFAULT_SERVICE_NETWORK = {
    "enabled": True,
    "host": "0.0.0.0",
    "advertised_host": "",
    "cors_origins": ["*"],
}
FALLBACK_SERVICE_NETWORK = {
    "enabled": False,
    "host": DEFAULT_HOST,
    "advertised_host": "",
    "cors_origins": list(DEFAULT_CORS_ORIGINS),
}
DEFAULT_MANAGER_CONFIG = {
    "default_startup": {
        "enabled": True,
        "service_ids": DEFAULT_STARTUP_SERVICE_IDS,
    },
    "service_network": DEFAULT_SERVICE_NETWORK,
}


class QCopilotsMCPServersManagerPlugin:
    def __init__(self, iface):
        self.iface = iface
        self.action = None
        self.dialog = None
        self.icon_path = Path(__file__).with_name("icon.svg")
        self.plugins_root = Path(__file__).resolve().parents[1]
        self.controller = ProcessController(qgis_executable=QCoreApplication.applicationFilePath())
        self.bridge = QgisBridgeController(
            iface,
            port=DEFAULT_BRIDGE_PORT,
        )
        self._shutdown_started = False
        self._shutdown_completed = False
        self._about_to_quit_connected = False
        self._default_startup_applied = False
        self.logger = configure_logger(
            "qcopilots.manager",
            service_log_file("qcopilots.manager"),
        )
        self._manager_config = _load_manager_config(
            Path(__file__).with_name("qcopilots_manager_config.json"),
            self.logger,
        )

    def initGui(self):
        self._shutdown_started = False
        self._shutdown_completed = False
        self._connect_about_to_quit()
        self.bridge.start()
        qgis_log(f"QCopilots QGIS bridge listening at {self.bridge.url}")

        self.action = QAction(
            QIcon(str(self.icon_path)),
            self.tr("QCopilots MCP Servers Manager"),
            self.iface.mainWindow(),
        )
        self.action.setObjectName("qcopilots_mcp_servers_manager")
        self.action.setToolTip(self.tr("QCopilots MCP Servers Manager"))
        self.action.setIconVisibleInMenu(True)
        self.action.triggered.connect(self.run)
        add_qcopilots_menu_action(self.iface, self.action)
        self.iface.addToolBarIcon(self.action)

    def unload(self):
        self._disconnect_about_to_quit()
        self._close_dialog(wait=True)
        if self.action:
            remove_qcopilots_menu_action(self.iface, self.action)
            self.iface.removeToolBarIcon(self.action)
            self.action = None
        self._shutdown_services()

    def _connect_about_to_quit(self):
        if getattr(self, "_about_to_quit_connected", False):
            return
        app = QCoreApplication.instance()
        if not app:
            return
        try:
            app.aboutToQuit.connect(self._shutdown_services)
        except Exception as err:
            self.logger.warning("Could not register QCopilots MCP shutdown hook: %s", err)
            return
        self._about_to_quit_connected = True

    def _disconnect_about_to_quit(self):
        if not getattr(self, "_about_to_quit_connected", False):
            return
        app = QCoreApplication.instance()
        if app:
            try:
                app.aboutToQuit.disconnect(self._shutdown_services)
            except Exception:
                pass
        self._about_to_quit_connected = False

    def _close_dialog(self, wait: bool = False) -> bool:
        if not self.dialog:
            return True
        if self.dialog.prepare_close(wait=wait):
            self.dialog.close()
            self.dialog.deleteLater()
            self.dialog = None
            return True
        self.logger.warning("QCopilots MCP server action is still running during plugin unload")
        return False

    def _shutdown_services(self):
        if getattr(self, "_shutdown_completed", False):
            return
        self._shutdown_started = True
        if self.dialog and not self.dialog.prepare_close(wait=True):
            self.logger.warning("QCopilots MCP server action is still running during QGIS shutdown")
            self.logger.warning("Continuing QCopilots MCP shutdown cleanup with a running service action")

        for manifest in self._shutdown_manifests():
            try:
                self.controller.stop(manifest)
            except Exception as err:
                self.logger.warning("Failed to stop %s: %s", manifest.service_id, err)
        try:
            self.bridge.stop()
        except Exception as err:
            self.logger.warning("Failed to stop QCopilots QGIS bridge: %s", err)
            return
        self._shutdown_completed = True

    def is_shutting_down(self) -> bool:
        return getattr(self, "_shutdown_started", False)

    def _shutdown_manifests(self) -> list[ServiceManifest]:
        manifests: dict[str, ServiceManifest] = {}
        for manifest in self._discover_services(include_disabled=True):
            manifests[manifest.service_id] = manifest
        stored_manifests = getattr(self.controller, "stored_manifests", None)
        if stored_manifests:
            for manifest in stored_manifests():
                manifests.setdefault(manifest.service_id, manifest)
        return sorted(manifests.values(), key=lambda manifest: manifest.service_id)

    def run(self):
        if not self.dialog:
            self.dialog = ManagerDialog(self.iface.mainWindow(), self)
        self.dialog.populate_services()
        self.dialog.show()
        self.dialog.raise_()
        self.dialog.activateWindow()

    def start_service(self, manifest: ServiceManifest):
        extra_env = self._service_env(manifest)
        status = self.controller.start(
            manifest,
            bridge_url=extra_env.get(BRIDGE_URL_ENV),
            extra_env=extra_env,
        )
        qgis_log(f"Started {manifest.plugin_name}: {status.url}")
        return status

    def stop_service(self, manifest: ServiceManifest):
        status = self.controller.stop(manifest)
        qgis_log(f"Stopped {manifest.plugin_name}")
        return status

    def service_status(self, manifest: ServiceManifest, deep: bool = True):
        return self.controller.status(manifest, deep=deep)

    def _discover_services(self, include_disabled: bool = False) -> list[ServiceManifest]:
        manifests = discover_service_manifests(self.plugins_root, include_disabled=include_disabled)
        service_network = self._service_network_config()
        return [_network_overrides_manifest(manifest, service_network) for manifest in manifests]

    def manager_config(self) -> dict[str, Any]:
        return _copy_manager_config(self._manager_config)

    def _service_network_config(self) -> dict[str, Any]:
        return self._manager_config.get("service_network", _fallback_service_network_config())

    def _service_env(self, manifest: ServiceManifest) -> dict[str, str]:
        env = {}
        if _uses_qgis_bridge(manifest):
            env[BRIDGE_URL_ENV] = self.bridge.url
        return env

    def _bridge_url(self, manifest: ServiceManifest) -> str | None:
        return self.bridge.url if _uses_qgis_bridge(manifest) else None

    def tr(self, message):
        return QCoreApplication.translate("QCopilotsMCPServersManager", message)


def _uses_qgis_bridge(manifest: ServiceManifest) -> bool:
    return "qgis-bridge" in manifest.capabilities or "processing" in manifest.capabilities


def _load_manager_config(config_path: Path, logger) -> dict[str, Any]:
    try:
        data = json.loads(config_path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        _warn_manager_config(logger, "QCopilots manager config is missing; using loopback service network fallback")
        return _fallback_manager_config()
    except Exception as err:
        logger.warning("Could not read QCopilots manager config %s: %s", config_path, err)
        return _fallback_manager_config()
    try:
        return _normalize_manager_config(data, logger)
    except Exception as err:
        logger.warning("Could not use QCopilots manager config %s: %s", config_path, err)
        return _fallback_manager_config()


def _default_manager_config() -> dict[str, Any]:
    return {
        "default_startup": {
            "enabled": DEFAULT_MANAGER_CONFIG["default_startup"]["enabled"],
            "service_ids": list(DEFAULT_STARTUP_SERVICE_IDS),
        },
        "service_network": _default_service_network_config(),
    }


def _fallback_manager_config() -> dict[str, Any]:
    return {
        "default_startup": {
            "enabled": DEFAULT_MANAGER_CONFIG["default_startup"]["enabled"],
            "service_ids": list(DEFAULT_STARTUP_SERVICE_IDS),
        },
        "service_network": _fallback_service_network_config(),
    }


def _default_service_network_config() -> dict[str, Any]:
    return {
        "enabled": DEFAULT_SERVICE_NETWORK["enabled"],
        "host": DEFAULT_SERVICE_NETWORK["host"],
        "advertised_host": DEFAULT_SERVICE_NETWORK["advertised_host"],
        "cors_origins": list(DEFAULT_SERVICE_NETWORK["cors_origins"]),
    }


def _fallback_service_network_config() -> dict[str, Any]:
    return {
        "enabled": FALLBACK_SERVICE_NETWORK["enabled"],
        "host": FALLBACK_SERVICE_NETWORK["host"],
        "advertised_host": FALLBACK_SERVICE_NETWORK["advertised_host"],
        "cors_origins": list(FALLBACK_SERVICE_NETWORK["cors_origins"]),
    }


def _copy_manager_config(manager_config: dict[str, Any]) -> dict[str, Any]:
    default_startup = manager_config.get("default_startup", {})
    service_network = manager_config.get("service_network", _fallback_service_network_config())
    return {
        "default_startup": {
            "enabled": default_startup.get("enabled", DEFAULT_MANAGER_CONFIG["default_startup"]["enabled"]),
            "service_ids": list(default_startup.get("service_ids", DEFAULT_STARTUP_SERVICE_IDS)),
        },
        "service_network": {
            "enabled": service_network.get(
                "enabled",
                DEFAULT_SERVICE_NETWORK["enabled"],
            ),
            "host": service_network.get(
                "host",
                DEFAULT_SERVICE_NETWORK["host"],
            ),
            "advertised_host": service_network.get(
                "advertised_host",
                DEFAULT_SERVICE_NETWORK["advertised_host"],
            ),
            "cors_origins": list(
                service_network.get("cors_origins", DEFAULT_SERVICE_NETWORK["cors_origins"])
            ),
        },
    }


def _normalize_manager_config(data: dict[str, Any], logger=None) -> dict[str, Any]:
    if not isinstance(data, dict):
        raise ValueError("manager config must be a JSON object")
    if "default_startup" not in data:
        _warn_manager_config(logger, "QCopilots manager config is missing default_startup")
    default_startup = data.get("default_startup", {})
    if not isinstance(default_startup, dict):
        _warn_manager_config(logger, "Ignoring QCopilots manager default_startup because it is not an object")
        default_startup = {}
    if "enabled" not in default_startup:
        _warn_manager_config(logger, "QCopilots manager default_startup is missing enabled")
    if "service_ids" not in default_startup:
        _warn_manager_config(logger, "QCopilots manager default_startup is missing service_ids")
    return {
        "default_startup": {
            "enabled": _startup_enabled(default_startup.get("enabled", True), logger),
            "service_ids": _startup_service_ids({"default_startup": default_startup}, logger),
        },
        "service_network": _normalize_service_network_config(data.get("service_network"), logger),
    }


def _startup_enabled(value: Any, logger=None) -> bool:
    if isinstance(value, bool):
        return value
    _warn_manager_config(logger, "Ignoring QCopilots manager default_startup.enabled because it is not a boolean")
    return DEFAULT_MANAGER_CONFIG["default_startup"]["enabled"]


def _startup_service_ids(manager_config: dict[str, Any], logger=None) -> list[str]:
    default_startup = manager_config.get("default_startup", {})
    configured_ids = default_startup.get("service_ids", DEFAULT_STARTUP_SERVICE_IDS)
    if not isinstance(configured_ids, list):
        _warn_manager_config(logger, "Ignoring QCopilots manager default_startup.service_ids because it is not a list")
        return list(DEFAULT_STARTUP_SERVICE_IDS)
    ordered_ids = []
    for service_id in configured_ids:
        if not isinstance(service_id, str):
            _warn_manager_config(
                logger,
                "Ignoring QCopilots manager default startup service id because it is not a string",
            )
            continue
        service_id = service_id.strip()
        if not service_id:
            _warn_manager_config(logger, "Ignoring blank QCopilots manager default startup service id")
            continue
        if not is_safe_service_id(service_id):
            _warn_manager_config(logger, "Ignoring unsafe QCopilots manager default startup service id: %s", service_id)
            continue
        if service_id not in ordered_ids:
            ordered_ids.append(service_id)
        else:
            _warn_manager_config(
                logger,
                "Ignoring duplicate QCopilots manager default startup service id: %s",
                service_id,
            )
    return ordered_ids


def _warn_manager_config(logger, message: str, *args):
    if logger is not None:
        logger.warning(message, *args)


def _normalize_service_network_config(value: Any, logger=None) -> dict[str, Any]:
    if value is None:
        _warn_manager_config(logger, "QCopilots manager config is missing service_network; using loopback fallback")
        return _fallback_service_network_config()
    if not isinstance(value, dict):
        _warn_manager_config(logger, "Ignoring QCopilots manager service_network because it is not an object")
        return _fallback_service_network_config()

    host = _service_network_host(
        value.get("host", DEFAULT_SERVICE_NETWORK["host"]),
        FALLBACK_SERVICE_NETWORK["host"],
        logger,
    )
    advertised_host = _service_network_advertised_host(value.get("advertised_host"), logger)
    return {
        "enabled": _service_network_enabled(
            value.get("enabled", DEFAULT_SERVICE_NETWORK["enabled"]),
            FALLBACK_SERVICE_NETWORK["enabled"],
            logger,
        ),
        "host": host,
        "advertised_host": advertised_host,
        "cors_origins": _service_network_cors_origins(
            value.get("cors_origins"),
            FALLBACK_SERVICE_NETWORK["cors_origins"],
            logger,
        ),
    }


def _service_network_enabled(value: Any, fallback: bool, logger=None) -> bool:
    if isinstance(value, bool):
        return value
    _warn_manager_config(logger, "Ignoring QCopilots manager service_network.enabled because it is not a boolean")
    return fallback


def _service_network_host(value: Any, fallback: str, logger=None) -> str:
    if not isinstance(value, str):
        _warn_manager_config(
            logger,
            "Ignoring QCopilots manager service_network.host because it is not a string",
        )
        return fallback
    host = value.strip()
    if not host:
        _warn_manager_config(logger, "Ignoring blank QCopilots manager service_network.host")
        return fallback
    if host.lower() == "localhost":
        return host
    try:
        address = ipaddress.ip_address(host)
    except ValueError:
        _warn_manager_config(logger, "Ignoring invalid QCopilots manager service_network.host: %s", host)
        return fallback
    if address.version == 4 and (
        address.is_loopback
        or address.is_private
        or address.is_link_local
        or host == "0.0.0.0"
    ):
        return host
    _warn_manager_config(
        logger,
        "Ignoring QCopilots manager service_network.host outside loopback or LAN/private IPv4: %s",
        host,
    )
    return fallback


def _service_network_string(
    value: Any,
    fallback: str,
    logger=None,
    field: str = "host",
    allow_empty: bool = False,
) -> str:
    if isinstance(value, str):
        value = value.strip()
        if value or allow_empty:
            return value
    _warn_manager_config(
        logger,
        "Ignoring QCopilots manager service_network.%s because it is not a string",
        field,
    )
    return fallback


def _service_network_advertised_host(value: Any, logger=None) -> str:
    if value is None:
        return DEFAULT_SERVICE_NETWORK["advertised_host"]
    host = _service_network_string(
        value,
        FALLBACK_SERVICE_NETWORK["advertised_host"],
        logger,
        "advertised_host",
        allow_empty=True,
    )
    if not host:
        return ""
    if _service_network_advertised_host_is_valid(host):
        return host
    _warn_manager_config(logger, "Ignoring invalid QCopilots manager service_network.advertised_host: %s", host)
    return FALLBACK_SERVICE_NETWORK["advertised_host"]


def _service_network_advertised_host_is_valid(host: str) -> bool:
    if any(ord(character) < 32 or character.isspace() for character in host):
        return False
    if any(character in host for character in "/\\?#@:"):
        return False
    try:
        address = ipaddress.ip_address(host)
    except ValueError:
        return bool(
            re.fullmatch(
                r"(?!-)(?!.*\.\.)(?!.*-$)[A-Za-z0-9](?:[A-Za-z0-9_.-]*[A-Za-z0-9])?",
                host,
            )
        )
    return address.version == 4


def _service_network_cors_origins(value: Any, fallback: list[str] | tuple[str, ...], logger=None) -> list[str]:
    if value is None:
        return list(DEFAULT_SERVICE_NETWORK["cors_origins"])
    if not isinstance(value, list):
        _warn_manager_config(
            logger,
            "Ignoring QCopilots manager service_network.cors_origins because it is not a list",
        )
        return list(fallback)
    origins = []
    for origin in value:
        if not isinstance(origin, str):
            _warn_manager_config(
                logger,
                "Ignoring QCopilots manager service_network.cors_origins item because it is not a string",
            )
            continue
        origin = origin.strip()
        if not _service_network_origin_is_valid(origin):
            _warn_manager_config(
                logger,
                "Ignoring invalid QCopilots manager service_network.cors_origins item: %s",
                origin,
            )
            continue
        if origin and origin not in origins:
            origins.append(origin)
    return origins or list(fallback)


def _service_network_origin_is_valid(origin: str) -> bool:
    if origin == "*":
        return True
    parsed = urlparse(origin)
    return (
        parsed.scheme in ("http", "https")
        and bool(parsed.netloc)
        and parsed.path in ("", "/")
        and not parsed.params
        and not parsed.query
        and not parsed.fragment
    )


def _network_overrides_manifest(
    manifest: ServiceManifest,
    service_network: dict[str, Any],
) -> ServiceManifest:
    if not service_network.get("enabled", FALLBACK_SERVICE_NETWORK["enabled"]):
        return manifest

    host = service_network.get("host") or manifest.host
    advertised_host = service_network.get("advertised_host") or _default_advertised_host(host)
    cors_origins = service_network.get("cors_origins") or manifest.cors_origins
    return replace(
        manifest,
        transport=replace(
            manifest.transport,
            host=host,
            advertised_host=advertised_host,
        ),
        cors_origins=list(cors_origins),
    )


def _default_advertised_host(host: str) -> str:
    if host != "0.0.0.0":
        return host
    advertised_host = _local_lan_ipv4_host()
    if advertised_host:
        return advertised_host
    try:
        return socket.gethostname() or "127.0.0.1"
    except OSError:
        return "127.0.0.1"


def _local_lan_ipv4_host() -> str:
    candidates = []
    for probe_host in ("192.168.1.1", "10.0.0.1", "172.16.0.1"):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.connect((probe_host, 9))
                _append_lan_candidate(candidates, sock.getsockname()[0])
        except OSError:
            continue
    try:
        hostname = socket.gethostname()
        for address in socket.gethostbyname_ex(hostname)[2]:
            _append_lan_candidate(candidates, address)
        for address in (
            address[4][0]
            for address in socket.getaddrinfo(hostname, None, socket.AF_INET, socket.SOCK_DGRAM)
        ):
            _append_lan_candidate(candidates, address)
    except OSError:
        pass

    private_candidates = []
    link_local_candidates = []
    for candidate in candidates:
        try:
            address = ipaddress.ip_address(candidate)
        except ValueError:
            continue
        if address.version != 4 or address.is_loopback:
            continue
        if address.is_link_local:
            link_local_candidates.append(candidate)
        elif address.is_private:
            private_candidates.append(candidate)
    if private_candidates:
        return private_candidates[0]
    if link_local_candidates:
        return link_local_candidates[0]
    return ""


def _append_lan_candidate(candidates: list[str], candidate: str) -> None:
    if candidate and candidate not in candidates:
        candidates.append(candidate)


def _order_service_manifests(
    manifests: list[ServiceManifest],
    startup_service_ids: list[str],
) -> list[ServiceManifest]:
    ordered_service_ids = {service_id: index for index, service_id in enumerate(startup_service_ids)}
    discovered_indexes = {id(manifest): index for index, manifest in enumerate(manifests)}
    return sorted(
        manifests,
        key=lambda manifest: (
            ordered_service_ids.get(manifest.service_id, len(ordered_service_ids)),
            discovered_indexes[id(manifest)],
        ),
    )


class ManagerDialog(QDialog):
    def __init__(self, parent, plugin: QCopilotsMCPServersManagerPlugin):
        super().__init__(parent)
        self.plugin = plugin
        self.setWindowTitle(plugin.tr("QCopilots MCP Servers"))
        self.setWindowIcon(QIcon(str(plugin.icon_path)))
        self.resize(880, 560)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 14, 14, 12)
        layout.setSpacing(12)

        self.scroll_area = QScrollArea(self)
        self.scroll_area.setObjectName("qcopilots_mcp_servers_scroll_area")
        self.scroll_area.setWidgetResizable(True)
        self.content = QWidget(self.scroll_area)
        self.content_layout = QVBoxLayout(self.content)
        self.content_layout.setContentsMargins(2, 2, 2, 2)
        self.content_layout.setSpacing(12)
        self.content_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.scroll_area.setWidget(self.content)
        layout.addWidget(self.scroll_area)

        footer = QHBoxLayout()
        footer.setContentsMargins(4, 0, 4, 0)
        self.footer_label = QLabel(self)
        self.footer_label.setObjectName("qcopilots_mcp_servers_footer_label")
        footer.addWidget(self.footer_label)
        footer.addStretch(1)
        layout.addLayout(footer)

        self.setStyleSheet(
            "QDialog { background: #f5f7fb; }"
            "QScrollArea#qcopilots_mcp_servers_scroll_area { border: none; background: transparent; }"
            "QWidget#qcopilots_service_card { background: #ffffff; border: 1px solid #dce4ef;"
            " border-radius: 8px; }"
            "QFrame#qcopilots_service_details { background: #f8fafc; border: 1px solid #dce4ef;"
            " border-radius: 6px; }"
            "QLabel#qcopilots_service_icon { background: #eef4fb; border-radius: 6px; }"
            "QLabel#qcopilots_service_name { color: #202733; font-weight: 600; }"
            "QLabel#qcopilots_service_description { color: #4b5563; }"
            "QLabel#qcopilots_service_status { color: #5c6675; }"
            "QLabel#qcopilots_endpoint_url { color: #1368c4; }"
            "QLabel#qcopilots_mcp_servers_footer_label { color: #5c6675; }"
            "QToolButton { border: none; padding: 4px; }"
            "QToolButton:hover { background: #edf2f7; border-radius: 4px; }"
        )

    def populate_services(self):
        if not self.prepare_close():
            self.footer_label.setText(
                self.plugin.tr("Wait for the MCP server action to finish before updating the list.")
            )
            return

        while self.content_layout.count():
            item = self.content_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()

        manager_config = self.plugin.manager_config()
        manifests = _order_service_manifests(
            self.plugin._discover_services(),
            _startup_service_ids(manager_config),
        )
        if not manifests:
            self.content_layout.addWidget(QLabel(self.plugin.tr("No QCopilots MCP services were found.")))
            self.footer_label.setText(self.plugin.tr("0 of 0 MCP servers"))
            return

        cards = []
        for index, manifest in enumerate(manifests):
            card = ServiceCard(self.plugin, manifest, expanded=index == 0, parent=self)
            cards.append(card)
            self.content_layout.addWidget(card)
        self.content_layout.addStretch(1)
        self._apply_default_startup(cards, manager_config)
        self.footer_label.setText(
            self.plugin.tr("{0} of {1} MCP servers").format(len(manifests), len(manifests))
        )

    def _apply_default_startup(self, cards: list["ServiceCard"], manager_config: dict[str, Any]):
        if self.plugin._default_startup_applied:
            return
        self.plugin._default_startup_applied = True
        startup = manager_config.get("default_startup", {})
        if not startup.get("enabled", True):
            return
        startup_service_ids = set(_startup_service_ids(manager_config))
        if not startup_service_ids:
            return
        for card in cards:
            if card.manifest.service_id not in startup_service_ids:
                continue
            if card.status.running:
                try:
                    card.status = self.plugin.service_status(card.manifest, deep=True)
                    if hasattr(card, "update_status_widgets"):
                        card.update_status_widgets()
                except Exception as err:
                    self.plugin.logger.warning("Could not refresh %s status: %s", card.manifest.service_id, err)
            if card.status.running:
                continue
            card.toggle_service(True)

    def prepare_close(self, wait: bool = False) -> bool:
        for index in range(self.content_layout.count()):
            widget = self.content_layout.itemAt(index).widget()
            if hasattr(widget, "cleanup_toggle_thread") and not widget.cleanup_toggle_thread(wait=wait):
                return False
        return True

    def closeEvent(self, event):
        if not self.prepare_close():
            self.footer_label.setText(self.plugin.tr("Wait for the MCP server action to finish before closing."))
            event.ignore()
            return
        super().closeEvent(event)


class QCopilotsSwitch(QCheckBox):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setText("")
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setFixedSize(54, 30)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    def sizeHint(self):
        return QSize(54, 30)

    def hitButton(self, pos):
        return self.rect().contains(pos)

    def paintEvent(self, event):
        del event
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setPen(Qt.PenStyle.NoPen)

        checked = self.isChecked()
        track_color = QColor("#8bdc9a") if checked else QColor("#d4d8de")
        painter.setBrush(track_color)
        painter.drawRoundedRect(0, 0, 54, 30, 15, 15)

        knob_x = 26 if checked else 2
        painter.setBrush(QColor("#ffffff"))
        painter.drawEllipse(knob_x, 2, 26, 26)


class ServiceToggleWorker(QObject):
    finished = pyqtSignal(object)
    failed = pyqtSignal(str, object)

    def __init__(
        self,
        controller: ProcessController,
        logger,
        manifest: ServiceManifest,
        enabled: bool,
        was_running: bool,
        bridge_url: str | None,
        extra_env: dict[str, str] | None,
        unhealthy_message: str,
        unknown_state_message: str,
        shutdown_requested=None,
    ):
        super().__init__()
        self.controller = controller
        self.logger = logger
        self.manifest = manifest
        self.enabled = enabled
        self.was_running = was_running
        self.bridge_url = bridge_url
        self.extra_env = extra_env or {}
        self.unhealthy_message = unhealthy_message
        self.unknown_state_message = unknown_state_message
        self.shutdown_requested = shutdown_requested or (lambda: False)

    def run(self):
        try:
            status = self._toggle_service()
        except Exception as err:
            self.failed.emit(str(err), self._safe_service_status())
            return

        if self.enabled and status and status.running and self.shutdown_requested():
            self.finished.emit(self._safe_stop_service(status))
            return

        if self._reached_target(status):
            self.finished.emit(status)
            return

        detail = status.health if status else self.unknown_state_message
        if self.enabled and status and status.running:
            status = self._safe_stop_service(status)
            detail = self.unhealthy_message
        self.failed.emit(detail, status)

    def _toggle_service(self):
        if self.enabled and not self.was_running:
            return self.controller.start(
                self.manifest,
                bridge_url=self.bridge_url,
                extra_env=self.extra_env,
            )
        if not self.enabled and self.was_running:
            return self.controller.stop(self.manifest)
        return self.controller.status(self.manifest)

    def _reached_target(self, status) -> bool:
        if not status:
            return False
        if self.enabled:
            return status.running and status.health == "ok"
        return not status.running

    def _safe_service_status(self):
        try:
            return self.controller.status(self.manifest)
        except Exception:
            return None

    def _safe_stop_service(self, fallback_status):
        try:
            return self.controller.stop(self.manifest)
        except Exception as err:
            self.logger.warning("Failed to stop unhealthy %s: %s", self.manifest.service_id, err)
            return fallback_status


class ServiceCard(QWidget):
    def __init__(
        self,
        plugin: QCopilotsMCPServersManagerPlugin,
        manifest: ServiceManifest,
        expanded: bool = False,
        parent=None,
    ):
        super().__init__(parent)
        self.plugin = plugin
        self.manifest = manifest
        self.status = plugin.service_status(manifest, deep=False)
        self.expanded = expanded
        self.copy_button = None
        self._copy_restore_text = ""
        self._copy_restore_tooltip = ""
        self._copy_feedback_button = None
        self.copy_feedback_timer = QTimer(self)
        self.copy_feedback_timer.setSingleShot(True)
        self.copy_feedback_timer.timeout.connect(self._restore_copy_button)
        self.status_feedback_timer = QTimer(self)
        self.status_feedback_timer.setSingleShot(True)
        self.status_feedback_timer.timeout.connect(self.update_status_widgets)
        self.toggle_thread = None
        self.toggle_worker = None
        self.toggle_expected_enabled = False
        self.toggle_failure_text = ""
        self.setObjectName("qcopilots_service_card")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 12, 14, 12)
        layout.setSpacing(10)

        header = QHBoxLayout()
        header.setSpacing(12)
        layout.addLayout(header)

        self.expand_button = QToolButton(self)
        self.expand_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.expand_button.setFixedSize(24, 24)
        self.expand_button.setToolTip(self.plugin.tr("Show service details"))
        self.expand_button.clicked.connect(self.toggle_details)
        header.addWidget(self.expand_button)

        self.icon_label = QLabel(self)
        self.icon_label.setObjectName("qcopilots_service_icon")
        self.icon_label.setFixedSize(40, 40)
        self.icon_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._set_icon()
        header.addWidget(self.icon_label)

        text_layout = QVBoxLayout()
        text_layout.setSpacing(3)
        self.name_label = QLabel(manifest.plugin_name, self)
        self.name_label.setObjectName("qcopilots_service_name")
        self.name_label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        text_layout.addWidget(self.name_label)

        self.description_label = QLabel(manifest.description, self)
        self.description_label.setObjectName("qcopilots_service_description")
        self.description_label.setWordWrap(True)
        self.description_label.setSizePolicy(
            QSizePolicy.Policy.Expanding,
            QSizePolicy.Policy.Preferred,
        )
        text_layout.addWidget(self.description_label)
        header.addLayout(text_layout, 1)

        self.inline_endpoint_label = QLabel(self.status.url, self)
        self.inline_endpoint_label.setObjectName("qcopilots_endpoint_url")
        self.inline_endpoint_label.setMinimumWidth(190)
        self.inline_endpoint_label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        header.addWidget(self.inline_endpoint_label)

        self.running_label = QLabel(self._running_text(), self)
        self.running_label.setObjectName("qcopilots_service_status")
        self.running_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.running_label.setMinimumWidth(70)
        header.addWidget(self.running_label)

        self.switch = QCopilotsSwitch(self)
        self.switch.setToolTip(self.plugin.tr("Enable or stop this MCP service"))
        self.switch.setChecked(self.status.running)
        self.switch.clicked.connect(self.toggle_service)
        header.addWidget(self.switch)

        self.settings_button = QToolButton(self)
        self.settings_button.setObjectName("qcopilots_service_settings_placeholder")
        self.settings_button.setText("⋮")
        self.settings_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.settings_button.setToolTip(self.plugin.tr("Service settings placeholder"))
        header.addWidget(self.settings_button)

        self.details_frame = QFrame(self)
        self.details_frame.setObjectName("qcopilots_service_details")
        details_layout = QVBoxLayout(self.details_frame)
        details_layout.setContentsMargins(12, 10, 12, 10)
        details_layout.setSpacing(8)
        self.endpoint_label = self._details_row(
            details_layout,
            self.plugin.tr("Endpoint URL"),
            self.status.url,
            add_copy=True,
        )
        self.port_label = self._details_row(details_layout, self.plugin.tr("Port"), str(self.status.port))
        layout.addWidget(self.details_frame)

        self._update_details_visibility()
        self._update_card_style()

    def toggle_service(self, enabled: bool):
        if self.toggle_thread:
            return
        if self._plugin_is_shutting_down():
            self.update_status_widgets()
            return
        previous_status = self.status
        action_text = self.plugin.tr("Starting...") if enabled else self.plugin.tr("Stopping...")
        failure_text = self.plugin.tr("Start failed") if enabled else self.plugin.tr("Stop failed")
        self.switch.setEnabled(False)
        self.running_label.setText(action_text)
        self.running_label.setToolTip(action_text)
        self._start_toggle_worker(enabled, previous_status.running, failure_text)

    def _start_toggle_worker(self, enabled: bool, was_running: bool, failure_text: str):
        extra_env = {}
        bridge_url = None
        if enabled and not was_running:
            try:
                extra_env = self.plugin._service_env(self.manifest)
                bridge_url = extra_env.get(BRIDGE_URL_ENV)
            except Exception as err:
                self._fail_toggle_service(
                    failure_text,
                    str(err),
                    self.plugin.service_status(self.manifest, deep=False),
                )
                return
        self.toggle_expected_enabled = enabled
        self.toggle_failure_text = failure_text
        thread = QThread(self)
        worker = ServiceToggleWorker(
            self.plugin.controller,
            self.plugin.logger,
            self.manifest,
            enabled,
            was_running,
            bridge_url,
            extra_env,
            self.plugin.tr("Service did not become healthy"),
            self.plugin.tr("Unknown service state"),
            self._plugin_is_shutting_down,
        )
        self.toggle_thread = thread
        self.toggle_worker = worker
        worker.moveToThread(thread)
        thread.started.connect(worker.run)
        worker.finished.connect(self._finish_toggle_service_from_worker)
        worker.failed.connect(self._fail_toggle_service_from_worker)
        worker.finished.connect(thread.quit)
        worker.failed.connect(thread.quit)
        worker.finished.connect(worker.deleteLater)
        worker.failed.connect(worker.deleteLater)
        thread.finished.connect(thread.deleteLater)
        thread.finished.connect(self._clear_toggle_worker)
        thread.start()

    def cleanup_toggle_thread(self, wait: bool = False) -> bool:
        thread = self.toggle_thread
        if not thread:
            return True
        if hasattr(thread, "isRunning") and thread.isRunning():
            if not wait:
                return False
            thread.quit()
            if not thread.wait(TOGGLE_THREAD_WAIT_TIMEOUT_MS):
                self.plugin.logger.warning(
                    "Timed out waiting for %s MCP server action to finish",
                    self.manifest.service_id,
                )
                return False
            if hasattr(QApplication, "processEvents"):
                QApplication.processEvents()
        self.toggle_thread = None
        self.toggle_worker = None
        return True

    def _finish_toggle_service_from_worker(self, status):
        self._finish_toggle_service(self.toggle_expected_enabled, self.toggle_failure_text, status)

    def _fail_toggle_service_from_worker(self, detail: str, status):
        self._fail_toggle_service(self.toggle_failure_text, detail, status)

    def _finish_toggle_service(self, enabled: bool, failure_text: str, status):
        self.status = status
        if enabled and self._stop_started_service_during_shutdown():
            return
        self.switch.setEnabled(True)
        if not self._status_reached_target(enabled):
            self.update_status_widgets()
            self._show_temporary_status(failure_text, self.status.health)
            return
        self.update_status_widgets()

    def _fail_toggle_service(self, failure_text: str, detail: str, status):
        self.plugin.logger.warning("Failed to toggle %s: %s", self.manifest.service_id, detail)
        if status:
            self.status = status
        else:
            self.status = self.plugin.service_status(self.manifest, deep=False)
        if self._stop_started_service_during_shutdown():
            return
        self.switch.setEnabled(True)
        self.update_status_widgets()
        self._show_temporary_status(failure_text, detail)

    def _status_reached_target(self, enabled: bool) -> bool:
        if enabled:
            return self.status.running and self.status.health == "ok"
        return not self.status.running

    def _plugin_is_shutting_down(self) -> bool:
        shutdown_check = getattr(self.plugin, "is_shutting_down", None)
        return bool(shutdown_check and shutdown_check())

    def _stop_started_service_during_shutdown(self) -> bool:
        if not self._plugin_is_shutting_down() or not self.status or not self.status.running:
            return False
        try:
            self.status = self.plugin.stop_service(self.manifest)
        except Exception as err:
            self.plugin.logger.warning(
                "Failed to stop %s after shutdown was requested: %s",
                self.manifest.service_id,
                err,
            )
            self.switch.setEnabled(True)
            self.update_status_widgets()
            self._show_temporary_status(self.plugin.tr("Stop failed"), str(err))
            return True
        self.switch.setEnabled(True)
        self.update_status_widgets()
        return True

    def _clear_toggle_worker(self):
        self.toggle_thread = None
        self.toggle_worker = None

    def toggle_details(self):
        self.expanded = not self.expanded
        self._update_details_visibility()

    def copy_endpoint_url(self):
        self._copy_value(
            self.status.url,
            self.copy_button,
            self.plugin.tr("Endpoint URL copied"),
        )

    def _copy_value(self, value: str, button, copied_tooltip: str):
        QApplication.clipboard().setText(value)
        if not button:
            return
        if self._copy_feedback_button and self._copy_feedback_button is not button:
            self._restore_copy_button()
        self._copy_feedback_button = button
        self._copy_restore_text = button.text()
        self._copy_restore_tooltip = button.toolTip()
        button.setText("✓")
        button.setToolTip(copied_tooltip)
        button.setEnabled(False)
        self.copy_feedback_timer.start(1200)

    def _restore_copy_button(self):
        button = self._copy_feedback_button or self.copy_button
        if button:
            button.setText(self._copy_restore_text)
            button.setToolTip(self._copy_restore_tooltip)
            button.setEnabled(True)
        self._copy_feedback_button = None

    def update_status_widgets(self):
        self.port_label.setText(str(self.status.port))
        self.running_label.setText(self._running_text())
        self.running_label.setToolTip("")
        if self.switch.isChecked() != self.status.running:
            self.switch.blockSignals(True)
            self.switch.setChecked(self.status.running)
            self.switch.blockSignals(False)
        self.inline_endpoint_label.setText(self.status.url)
        self.endpoint_label.setText(self.status.url)
        self._update_card_style()

    def _show_temporary_status(self, message: str, tooltip: str = ""):
        self.running_label.setText(message)
        self.running_label.setToolTip(tooltip)
        self.status_feedback_timer.start(1600)

    def _running_text(self) -> str:
        return self.plugin.tr("Running") if self.status.running else self.plugin.tr("Stopped")

    def _set_icon(self):
        icon_path = self.manifest.icon_path
        if icon_path and icon_path.is_file():
            pixmap = QIcon(str(icon_path)).pixmap(QSize(28, 28))
            self.icon_label.setPixmap(pixmap)
            return
        self.icon_label.setText("Q")

    def _details_row(
        self,
        layout: QVBoxLayout,
        label_text: str,
        value: str,
        add_copy: bool = False,
        copy_handler=None,
        copy_button_attr: str = "copy_button",
        copy_object_name: str = "qcopilots_endpoint_copy_button",
        copy_tooltip: str | None = None,
    ) -> QLabel:
        row = QHBoxLayout()
        label = QLabel(label_text, self.details_frame)
        label.setMinimumWidth(95)
        row.addWidget(label)
        value_label = QLabel(value, self.details_frame)
        value_label.setObjectName("qcopilots_endpoint_url" if add_copy else "qcopilots_detail_value")
        value_label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        row.addWidget(value_label, 1)
        if add_copy:
            copy_button = QToolButton(self.details_frame)
            copy_button.setObjectName(copy_object_name)
            copy_button.setText("⧉")
            copy_button.setToolTip(copy_tooltip or self.plugin.tr("Copy endpoint URL"))
            copy_button.clicked.connect(copy_handler or self.copy_endpoint_url)
            setattr(self, copy_button_attr, copy_button)
            row.addWidget(copy_button)
        layout.addLayout(row)
        return value_label

    def _update_details_visibility(self):
        self.details_frame.setVisible(self.expanded)
        self.inline_endpoint_label.setVisible(not self.expanded)
        self.expand_button.setText("⌄" if self.expanded else "›")

    def _update_card_style(self):
        background = "#e9f9f4" if self.status.running else "#ffffff"
        accent = _service_accent_color(self.manifest)
        self.setStyleSheet(
            "QWidget#qcopilots_service_card {"
            f" background-color: {background};"
            f" border-left: 4px solid {accent};"
            " border-top: 1px solid #dce4ef;"
            " border-right: 1px solid #dce4ef;"
            " border-bottom: 1px solid #dce4ef;"
            " border-radius: 8px;"
            "}"
        )


def _service_accent_color(manifest: ServiceManifest) -> str:
    if "builtin" in manifest.service_id:
        return "#30b36b"
    if "interactive" in manifest.service_id:
        return "#2f80ed"
    if "raster" in manifest.service_id:
        return "#2eb67d"
    if "vector" in manifest.service_id:
        return "#8758ff"
    if "skills" in manifest.service_id:
        return "#f2a541"
    return "#7a8ca3"
