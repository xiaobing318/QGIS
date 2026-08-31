"""QGIS UI plugin for discovering and managing QCopilots MCP services.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import concurrent.futures
import inspect
import ipaddress
import json
import re
import secrets
import threading
from dataclasses import replace
from pathlib import Path
from typing import Any
from urllib.parse import quote, urlsplit

from qgis.PyQt.QtCore import QCoreApplication, QObject, QSize, QThread, QTimer, Qt, pyqtSignal, pyqtSlot
from qgis.PyQt.QtGui import QColor, QIcon, QPainter
from qgis.PyQt.QtWidgets import (
    QAction,
    QApplication,
    QCheckBox,
    QDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QScrollArea,
    QSizePolicy,
    QToolButton,
    QVBoxLayout,
    QWidget,
)
from qgis.core import QgsSettings

from qcopilots_common.bridge import QgisBridgeController
from qcopilots_common.constants import (
    BRIDGE_URL_ENV,
    CORS_ORIGINS_ENV,
    DEFAULT_BRIDGE_PORT,
    DEFAULT_HOST,
    QGIS_BRIDGE_AUTH_TOKEN_ENV,
    encode_cors_origins,
)
from qcopilots_common.discovery import discover_service_manifests
from qcopilots_common.logging import configure_logger, qgis_log, service_log_file
from qcopilots_common.manifest import ServiceManifest
from qcopilots_common.menu import add_qcopilots_menu_action, remove_qcopilots_menu_action
from qcopilots_common.process_controller import ProcessController
from qcopilots_common.security_policy import (
    FORMAL_RESTRICTED_MODE,
    FilesystemPolicy,
    filesystem_policy_from_config,
)
from .config_store import (
    ConfigSaveResult,
    MANAGER_CONFIG_VERSION,
    ManagerConfigStore,
    is_valid_browser_auth_token,
    validate_manager_config,
)


TOGGLE_THREAD_WAIT_TIMEOUT_MS = 35000
TOGGLE_THREAD_FINAL_WAIT_SLICE_MS = 1000
STARTUP_THREAD_WAIT_TIMEOUT_MS = 35000
STARTUP_THREAD_FINAL_WAIT_SLICE_MS = 1000
SERVICE_STARTUP_TIMEOUT_SECONDS = 30.0
RUNTIME_CATALOG_PROPERTY = "qcopilotsMcpRuntimeCatalog"
RUNTIME_CATALOG_SCHEMA_VERSION = 1
RUNTIME_CATALOG_MAX_SERVICES = 128
RUNTIME_CATALOG_SERVICE_ID_PATTERN = re.compile(
    r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$"
)
RUNTIME_CATALOG_STATES = frozenset(("failed", "running", "starting", "stopped"))

DEFAULT_STARTUP_SERVICE_IDS = [
    "qcopilots.mcp_server_builtin_tools",
    "qcopilots.mcp_server_interactive_tools",
    "qcopilots.mcp_server_processing_vector",
    "qcopilots.mcp_server_processing_raster",
    "qcopilots.mcp_server_processing_general",
    "qcopilots.mcp_server_skills",
    "qcopilots.mcp_server_qgis_binary",
]
DEFAULT_QCOPILOTS_URL = "http://127.0.0.1:8282"


class _ManagerEventRelay(QObject):
    service_starting = pyqtSignal(object)
    service_start_finished = pyqtSignal(object, object, str)
    service_start_failed = pyqtSignal(object, str)
    service_stop_finished = pyqtSignal(object, object)
    service_stop_failed = pyqtSignal(object, str)

    def __init__(self, manager: "QCopilotsMCPServersManagerPlugin"):
        super().__init__()
        self.manager = manager
        self.service_starting.connect(self._handle_service_starting)
        self.service_start_finished.connect(self._handle_service_start_finished)
        self.service_start_failed.connect(self._handle_service_start_failed)
        self.service_stop_finished.connect(self._handle_service_stop_finished)
        self.service_stop_failed.connect(self._handle_service_stop_failed)

    @pyqtSlot(object)
    def _handle_service_starting(self, manifest):
        self.manager._handle_service_starting(manifest)

    @pyqtSlot(object, object, str)
    def _handle_service_start_finished(self, manifest, status, auth_token):
        self.manager._handle_service_start_finished(manifest, status, auth_token)

    @pyqtSlot(object, str)
    def _handle_service_start_failed(self, manifest, detail):
        self.manager._handle_service_start_failed(manifest, detail)

    @pyqtSlot(object, object)
    def _handle_service_stop_finished(self, manifest, status):
        self.manager._handle_service_stop_finished(manifest, status)

    @pyqtSlot(object, str)
    def _handle_service_stop_failed(self, manifest, detail):
        self.manager._handle_service_stop_failed(manifest, detail)

    @pyqtSlot(object)
    def handle_default_start_result(self, result):
        self.manager._handle_default_start_result(result)

    @pyqtSlot(bool)
    def handle_default_start_finished(self, cancelled):
        self.manager._handle_default_start_finished(cancelled)


class _DefaultStartupWorker(QObject):
    service_finished = pyqtSignal(object)
    finished = pyqtSignal(bool)

    def __init__(self, controller: ProcessController, jobs: list[dict[str, Any]]):
        super().__init__()
        self.controller = controller
        self.jobs = list(jobs)
        self._cancelled = threading.Event()

    def cancel(self):
        self._cancelled.set()

    def run(self):
        if not self.jobs:
            self.finished.emit(self._cancelled.is_set())
            return

        with concurrent.futures.ThreadPoolExecutor(
            max_workers=len(self.jobs),
            thread_name_prefix="qcopilots-startup",
        ) as executor:
            futures = {
                executor.submit(self._start_one, job): job
                for job in self.jobs
                if not self._cancelled.is_set()
            }
            for future in concurrent.futures.as_completed(futures):
                job = futures[future]
                try:
                    result = future.result()
                except Exception as err:
                    status, _stop_error = _stop_after_failed_start(
                        self.controller,
                        job["manifest"],
                        cancel_event=self._cancelled,
                    )
                    result = {
                        "manifest": job["manifest"],
                        "status": status,
                        "auth_token": job["auth_token"],
                        "succeeded": False,
                        "detail": str(err),
                    }
                self.service_finished.emit(result)

        if self._cancelled.is_set():
            self._stop_cancelled_jobs()
        self.finished.emit(self._cancelled.is_set())

    def _start_one(self, job: dict[str, Any]) -> dict[str, Any]:
        manifest = job["manifest"]
        if self._cancelled.is_set():
            return {
                "manifest": manifest,
                "status": None,
                "auth_token": job["auth_token"],
                "succeeded": False,
                "detail": "Startup cancelled",
            }
        try:
            status = _controller_start(
                self.controller,
                manifest,
                bridge_url=job["bridge_url"],
                extra_env=job["extra_env"],
                startup_timeout_seconds=SERVICE_STARTUP_TIMEOUT_SECONDS,
                auth_token=job["auth_token"],
                port_conflict_policy=job.get("port_conflict_policy", "fail"),
                cancel_event=self._cancelled,
            )
        except Exception as err:
            status, _stop_error = _stop_after_failed_start(
                self.controller,
                manifest,
                cancel_event=self._cancelled,
            )
            return {
                "manifest": manifest,
                "status": status,
                "auth_token": job["auth_token"],
                "succeeded": False,
                "detail": str(err),
            }

        current_token = _controller_auth_token(self.controller, manifest.service_id)
        if current_token is None and not hasattr(self.controller, "current_auth_token"):
            current_token = job["auth_token"]
        succeeded = _service_start_succeeded(status) and bool(current_token)
        detail = "" if succeeded else _service_start_failure_detail(status)
        if (
            (self._cancelled.is_set() or not succeeded)
            and _status_is_owned_and_running(status)
        ):
            stopped_status, _stop_error = _stop_after_failed_start(
                self.controller,
                manifest,
                cancel_event=self._cancelled,
            )
            if stopped_status is not None:
                status = stopped_status
        return {
            "manifest": manifest,
            "status": status,
            "auth_token": current_token or "",
            "succeeded": succeeded and not self._cancelled.is_set(),
            "detail": detail,
        }

    def _stop_cancelled_jobs(self) -> None:
        manifests = {}
        for job in self.jobs:
            manifest = job["manifest"]
            manifests.setdefault(manifest.service_id, manifest)
        for manifest in manifests.values():
            status = _safe_controller_status(self.controller, manifest)
            if not _status_is_owned_and_running(status):
                continue
            _stop_after_failed_start(
                self.controller,
                manifest,
                cancel_event=self._cancelled,
            )


class QCopilotsMCPServersManagerPlugin:
    def __init__(self, iface):
        self.iface = iface
        self.action = None
        self.dialog = None
        self.icon_path = Path(__file__).with_name("icon.svg")
        self.plugins_root = Path(__file__).resolve().parents[1]
        self.controller = ProcessController(qgis_executable=QCoreApplication.applicationFilePath())
        self._bridge_auth_token = secrets.token_urlsafe(32)
        self._filesystem_policy = FilesystemPolicy()
        self.bridge = QgisBridgeController(
            iface,
            port=DEFAULT_BRIDGE_PORT,
            auth_token=self._bridge_auth_token,
        )
        self._initialized = False
        self._shutdown_started = False
        self._shutdown_completed = False
        self._shutdown_event = threading.Event()
        self._shutdown_lock = threading.RLock()
        self._about_to_quit_connected = False
        self._startup_thread = None
        self._startup_worker = None
        self._state_lock = threading.RLock()
        self._starting_service_ids: set[str] = set()
        self._owned_manifests: dict[str, ServiceManifest] = {}
        self._service_auth_tokens: dict[str, str] = {}
        self._catalog_manifests: dict[str, ServiceManifest] = {}
        self._catalog_states: dict[str, str] = {}
        self._catalog_statuses: dict[str, Any] = {}
        self._catalog_generation = 0
        self._catalog_startup_complete = True
        self.logger = configure_logger(
            "qcopilots.manager",
            service_log_file("qcopilots.manager"),
        )
        self._config_blocked_notice_shown = False
        self._config_store = ManagerConfigStore(
            Path(__file__).with_name("qcopilots_manager_config.json"),
            self.logger,
        )
        self._manager_config: dict[str, Any] = {}
        self._browser_auth_token = ""
        self._browser_access_persisted = False
        self._browser_access_error = ""
        self._browser_origin = ""
        self._initialize_browser_access()
        if bool(getattr(self._config_store, "blocked", False)):
            self._filesystem_policy = FilesystemPolicy(
                mode=FORMAL_RESTRICTED_MODE,
                shell_enabled=False,
                network_enabled=False,
            )
        else:
            self._filesystem_policy = filesystem_policy_from_config(
                self._manager_config.get("security_policy")
            )
        set_filesystem_policy = getattr(
            self.bridge, "set_filesystem_policy", None
        )
        if callable(set_filesystem_policy):
            set_filesystem_policy(self._filesystem_policy)
        self._event_relay = _ManagerEventRelay(self)

    def initGui(self):
        if self._initialized:
            return
        config_store = getattr(self, "_config_store", None)
        if bool(getattr(config_store, "blocked", False)):
            self.logger.warning(
                "QCopilots MCP services are disabled because manager configuration "
                "is blocked: %s",
                getattr(config_store, "blocked_error", "invalid configuration"),
            )
            self._show_config_blocked_message(config_store)
            return
        self._shutdown_started = False
        self._shutdown_completed = False
        self._shutdown_event.clear()
        if not self._bridge_auth_token:
            self._bridge_auth_token = secrets.token_urlsafe(32)
            set_auth_token = getattr(self.bridge, "set_auth_token", None)
            if set_auth_token:
                set_auth_token(self._bridge_auth_token)
            else:
                self.bridge = QgisBridgeController(
                    self.iface,
                    port=DEFAULT_BRIDGE_PORT,
                    auth_token=self._bridge_auth_token,
                )
                set_filesystem_policy = getattr(
                    self.bridge, "set_filesystem_policy", None
                )
                if callable(set_filesystem_policy):
                    set_filesystem_policy(self._filesystem_policy)
        self._connect_about_to_quit()
        self.bridge.start()
        qgis_log(f"QCopilots QGIS bridge listening at {self.bridge.url}")
        self._initialized = True

        if not self.action:
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
        self._discover_and_start_default_services()

    def _show_config_blocked_message(self, config_store) -> None:
        """Report a blocked configuration once without weakening fail-closed startup."""

        if getattr(self, "_config_blocked_notice_shown", False):
            return
        self._config_blocked_notice_shown = True

        user_path = getattr(config_store, "user_path", "unknown")
        message = self.tr(
            "The manager configuration could not be loaded safely. "
            "QCopilots MCP services remain disabled. Configuration file: {path}"
        ).format(path=user_path)
        try:
            message_bar_getter = getattr(self.iface, "messageBar", None)
            if not callable(message_bar_getter):
                raise RuntimeError("QGIS message bar is unavailable")
            message_bar = message_bar_getter()
            push_critical = getattr(message_bar, "pushCritical", None)
            if not callable(push_critical):
                raise RuntimeError("QGIS critical message API is unavailable")
            push_critical(self.tr("QCopilots configuration error"), message)
        except Exception as err:
            logger = getattr(self, "logger", None)
            warning = getattr(logger, "warning", None)
            if callable(warning):
                warning(
                    "Could not display the blocked QCopilots manager configuration "
                    "message: %s",
                    err,
                )

    def unload(self):
        if not self._shutdown_services():
            message = "QCopilots MCP shutdown remains incomplete after plugin unload"
            self.logger.warning(message)
            raise RuntimeError(message)
        if not self._close_dialog(wait=True):
            message = "QCopilots MCP dialog cleanup remains incomplete after plugin unload"
            self.logger.warning(message)
            raise RuntimeError(message)
        self._disconnect_about_to_quit()
        if self.action:
            remove_qcopilots_menu_action(self.iface, self.action)
            self.iface.removeToolBarIcon(self.action)
            self.action = None
        self._initialized = False

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
        shutdown_lock = getattr(self, "_shutdown_lock", None)
        if shutdown_lock is None:
            shutdown_lock = threading.RLock()
            self._shutdown_lock = shutdown_lock
        with shutdown_lock:
            return self._shutdown_services_locked()

    def _shutdown_services_locked(self):
        if getattr(self, "_shutdown_completed", False):
            return True
        self._shutdown_started = True
        action = getattr(self, "action", None)
        if action and hasattr(action, "setEnabled"):
            action.setEnabled(False)
        shutdown_event = getattr(self, "_shutdown_event", None)
        if shutdown_event is not None:
            shutdown_event.set()

        self._revoke_runtime_access()
        self._request_shutdown_cancellation()

        # Stop everything already owned before waiting for workers. This also
        # releases health checks which are waiting on a child process.
        self._stop_shutdown_services(final_sweep=False)

        startup_stopped = self._cleanup_startup_thread(wait=True)
        dialog = getattr(self, "dialog", None)
        dialog_stopped = not dialog or dialog.prepare_close(wait=True)
        if not dialog_stopped:
            self.logger.warning("QCopilots MCP server action is still running during QGIS shutdown")
            self.logger.warning("Continuing QCopilots MCP shutdown cleanup with a running service action")

        # A start may have completed while cancellation was being delivered.
        # The final ownership sweep is authoritative for shutdown success.
        services_stopped = self._stop_shutdown_services(final_sweep=True)

        bridge_stopped = True
        try:
            bridge_result = self.bridge.stop()
            if bridge_result is False:
                bridge_stopped = False
                self.logger.warning("QCopilots QGIS bridge thread did not stop")
        except Exception as err:
            self.logger.warning("Failed to stop QCopilots QGIS bridge: %s", err)
            bridge_stopped = False

        cleanup_complete = (
            startup_stopped
            and dialog_stopped
            and services_stopped
            and bridge_stopped
        )

        if cleanup_complete:
            with self._state_lock:
                self._starting_service_ids.clear()
                self._owned_manifests.clear()
                self._service_auth_tokens.clear()
                self._catalog_manifests.clear()
                self._catalog_states.clear()
                self._catalog_statuses.clear()
                self._catalog_startup_complete = True
            self._shutdown_completed = True
        else:
            self._shutdown_completed = False
        self._publish_runtime_catalog()
        return cleanup_complete

    def _revoke_runtime_access(self):
        self._bridge_auth_token = ""
        with self._state_lock:
            self._service_auth_tokens.clear()
            self._catalog_statuses.clear()
            for service_id in self._catalog_manifests:
                self._catalog_states[service_id] = "stopped"
            self._catalog_startup_complete = True
        self._publish_runtime_catalog()

        clear_auth_token = getattr(self.bridge, "clear_auth_token", None)
        if clear_auth_token:
            try:
                clear_auth_token()
            except Exception as err:
                self.logger.warning("Failed to revoke QCopilots QGIS bridge token: %s", err)

    def _request_shutdown_cancellation(self):
        worker = getattr(self, "_startup_worker", None)
        if worker and hasattr(worker, "cancel"):
            worker.cancel()
        dialog = getattr(self, "dialog", None)
        cancel_actions = getattr(dialog, "cancel_service_actions", None) if dialog else None
        if cancel_actions:
            cancel_actions()

    def _stop_shutdown_services(self, *, final_sweep: bool) -> bool:
        services_stopped = True
        shutdown_event = getattr(self, "_shutdown_event", None)
        for manifest in self._shutdown_manifest_snapshot():
            with self._state_lock:
                explicitly_owned = manifest.service_id in self._owned_manifests
            if not explicitly_owned:
                current = _safe_controller_status(self.controller, manifest)
                if not _status_is_owned_and_running(current):
                    continue
                with self._state_lock:
                    self._owned_manifests[manifest.service_id] = manifest
                    self._shutdown_completed = False
            status = None
            try:
                status = _controller_stop(
                    self.controller,
                    manifest,
                    cancel_event=shutdown_event,
                )
            except Exception as err:
                if final_sweep:
                    self.logger.warning(
                        "Failed to stop %s during final shutdown sweep: %s",
                        manifest.service_id,
                        err,
                    )
                else:
                    self.logger.warning("Failed to stop %s: %s", manifest.service_id, err)
                status = _safe_controller_status(self.controller, manifest)
            if status is None or _status_is_owned_and_running(status):
                services_stopped = False
                continue
            with self._state_lock:
                self._starting_service_ids.discard(manifest.service_id)
                self._owned_manifests.pop(manifest.service_id, None)
                self._service_auth_tokens.pop(manifest.service_id, None)
                if manifest.service_id in self._catalog_manifests:
                    self._catalog_states[manifest.service_id] = "stopped"
                    self._catalog_statuses.pop(manifest.service_id, None)
        return services_stopped

    def is_shutting_down(self) -> bool:
        return getattr(self, "_shutdown_started", False)

    def _owned_manifest_snapshot(self) -> list[ServiceManifest]:
        with self._state_lock:
            return sorted(self._owned_manifests.values(), key=lambda manifest: manifest.service_id)

    def _shutdown_manifest_snapshot(self) -> list[ServiceManifest]:
        with self._state_lock:
            manifests = dict(self._catalog_manifests)
            manifests.update(self._owned_manifests)
            return sorted(manifests.values(), key=lambda manifest: manifest.service_id)

    def run(self):
        if self.is_shutting_down():
            return
        if not self.dialog:
            self.dialog = ManagerDialog(self.iface.mainWindow(), self)
        self.dialog.populate_services()
        self.dialog.show()
        self.dialog.raise_()
        self.dialog.activateWindow()

    def prepare_service_start(self, manifest: ServiceManifest) -> str | None:
        if self.is_shutting_down() or bool(
            getattr(getattr(self, "_config_store", None), "blocked", False)
        ):
            return None
        with self._state_lock:
            if manifest.service_id in self._starting_service_ids:
                return None
            auth_token = self.browser_auth_token()
            self._starting_service_ids.add(manifest.service_id)
            self._catalog_states[manifest.service_id] = "starting"
            self._catalog_statuses.pop(manifest.service_id, None)
        self._publish_runtime_catalog()
        return auth_token

    def start_service(
        self,
        manifest: ServiceManifest,
        auth_token: str | None = None,
        cancel_event: threading.Event | None = None,
    ):
        if bool(
            getattr(getattr(self, "_config_store", None), "blocked", False)
        ):
            raise RuntimeError(
                "QCopilots MCP service startup is blocked by invalid manager configuration"
            )
        if self.is_shutting_down():
            return self.service_status(manifest, deep=False)
        operation_cancel_event = (
            cancel_event
            if cancel_event is not None
            else getattr(self, "_shutdown_event", None)
        )
        if operation_cancel_event is not None and operation_cancel_event.is_set():
            return self.service_status(manifest, deep=False)
        try:
            current = self.service_status(manifest, deep=False)
            current_token = _controller_auth_token(self.controller, manifest.service_id)
        except Exception as err:
            self._emit_manager_event("service_start_failed", manifest, str(err))
            raise
        if current.running:
            current = self.service_status(manifest, deep=True)
            if current_token:
                self._emit_manager_event(
                    "service_start_finished",
                    manifest,
                    current,
                    current_token,
                )
            else:
                self._emit_manager_event(
                    "service_start_failed",
                    manifest,
                    "Running service is not owned by this manager session",
                )
            return current

        if auth_token is None:
            auth_token = self.browser_auth_token()
            self._emit_manager_event("service_starting", manifest)

        extra_env = self._service_env(manifest)
        try:
            status = _controller_start(
                self.controller,
                manifest,
                bridge_url=extra_env.get(BRIDGE_URL_ENV),
                extra_env=extra_env,
                startup_timeout_seconds=SERVICE_STARTUP_TIMEOUT_SECONDS,
                auth_token=auth_token,
                port_conflict_policy=self._browser_access_config().get(
                    "port_conflict_policy",
                    "fail",
                ),
                cancel_event=operation_cancel_event,
            )
        except Exception as err:
            cleanup_status, cleanup_error = _stop_after_failed_start(
                self.controller,
                manifest,
                cancel_event=operation_cancel_event,
            )
            if _status_is_owned_and_running(cleanup_status):
                with self._state_lock:
                    self._owned_manifests[manifest.service_id] = manifest
                    self._shutdown_completed = False
            if cleanup_error is not None and (
                cleanup_status is None or getattr(cleanup_status, "running", False)
            ):
                self.logger.warning(
                    "Failed to clean up %s after its start raised an exception: %s",
                    manifest.service_id,
                    cleanup_error,
                )
            self._emit_manager_event("service_start_failed", manifest, str(err))
            raise
        actual_token = _controller_auth_token(self.controller, manifest.service_id)
        if actual_token is None and not hasattr(self.controller, "current_auth_token"):
            actual_token = auth_token
        self._emit_manager_event("service_start_finished", manifest, status, actual_token or "")
        if _service_start_succeeded(status) and actual_token:
            qgis_log(f"Started {manifest.plugin_name}: {status.url}")
        return status

    def stop_service(
        self,
        manifest: ServiceManifest,
        cancel_event: threading.Event | None = None,
    ):
        operation_cancel_event = (
            cancel_event
            if cancel_event is not None
            else getattr(self, "_shutdown_event", None)
        )
        try:
            status = _controller_stop(
                self.controller,
                manifest,
                cancel_event=operation_cancel_event,
            )
        except Exception as err:
            self._emit_manager_event("service_stop_failed", manifest, str(err))
            raise
        self._emit_manager_event("service_stop_finished", manifest, status)
        if status and not status.running:
            qgis_log(f"Stopped {manifest.plugin_name}")
        return status

    def service_status(self, manifest: ServiceManifest, deep: bool = True):
        return self.controller.status(manifest, deep=deep)

    def service_status_snapshot(self, manifest: ServiceManifest):
        snapshot = getattr(self.controller, "status_snapshot", None)
        if snapshot:
            return snapshot(manifest)
        return self.controller.status(manifest, deep=False)

    def _discover_services(self, include_disabled: bool = False) -> list[ServiceManifest]:
        manifests = discover_service_manifests(self.plugins_root, include_disabled=include_disabled)
        service_network = self._service_network_config()
        cors_origins = self._browser_cors_origins()
        return [
            _network_overrides_manifest(
                manifest,
                service_network,
                cors_origins=cors_origins,
            )
            for manifest in manifests
        ]

    def manager_config(self) -> dict[str, Any]:
        return _copy_manager_config(self._manager_config)

    def remember_service_startup(
        self,
        service_id: str,
        enabled: bool,
    ) -> ConfigSaveResult | None:
        """Persist one successful explicit UI service state transition."""

        if self.is_shutting_down():
            return None
        result = self._config_store.set_startup_service_enabled(service_id, enabled)
        self._manager_config = self._config_store.snapshot()
        if not result.saved:
            self.logger.warning(
                "QCopilots %s changed state, but its startup preference was not saved: %s",
                service_id,
                result.error,
            )
        return result

    def _service_network_config(self) -> dict[str, Any]:
        manager_config = getattr(self, "_manager_config", None)
        if manager_config is None:
            return _default_service_network_config()
        return dict(manager_config["service_network"])

    def _browser_access_config(self) -> dict[str, Any]:
        manager_config = getattr(self, "_manager_config", None)
        if manager_config is None:
            return _default_browser_access_config()
        return dict(manager_config["browser_access"])

    def _initialize_browser_access(self) -> None:
        ensure_token = getattr(self._config_store, "ensure_browser_auth_token", None)
        result = ensure_token() if ensure_token else None
        self._manager_config = self._config_store.snapshot()
        configured = self._browser_access_config()
        configured_token = configured.get("auth_token", "")
        if result is None:
            persisted = is_valid_browser_auth_token(configured_token)
            error = ""
        else:
            persisted = bool(result.saved and is_valid_browser_auth_token(configured_token))
            error = result.error

        self._browser_access_persisted = persisted
        self._browser_access_error = error
        self._browser_auth_token = (
            configured_token if persisted else secrets.token_urlsafe(32)
        )
        self._browser_origin = _configured_qcopilots_origin()
        if not persisted:
            self.logger.warning(
                "QCopilots browser access is disabled because its shared token could not be persisted"
            )
        elif configured.get("enabled", True) and not self._browser_origin:
            self.logger.warning(
                "QCopilots browser access is disabled because the configured QCopilots URL has no valid HTTP origin"
            )

    def browser_auth_token(self) -> str:
        token = getattr(self, "_browser_auth_token", "")
        if is_valid_browser_auth_token(token):
            return token
        token = secrets.token_urlsafe(32)
        self._browser_auth_token = token
        self._browser_access_persisted = False
        return token

    def browser_access_snapshot(self) -> dict[str, Any]:
        configured = self._browser_access_config()
        enabled = bool(configured.get("enabled", True))
        auth_token = self.browser_auth_token()
        available = bool(
            enabled
            and getattr(self, "_browser_access_persisted", False)
            and getattr(self, "_browser_origin", "")
            and is_valid_browser_auth_token(auth_token)
        )
        return {
            "enabled": enabled,
            "available": available,
            "origin": getattr(self, "_browser_origin", ""),
            "auth_token": auth_token if available else "",
            "error": getattr(self, "_browser_access_error", ""),
            "port_conflict_policy": configured.get("port_conflict_policy", "fail"),
        }

    def _browser_cors_origins(self) -> list[str]:
        snapshot = self.browser_access_snapshot()
        return [snapshot["origin"]] if snapshot["available"] else []

    def regenerate_browser_auth_token(self) -> ConfigSaveResult:
        """Persist a new shared token before any services are restarted."""

        state_lock = getattr(self, "_state_lock", threading.RLock())
        with state_lock:
            if getattr(self, "_starting_service_ids", set()):
                return ConfigSaveResult(
                    changed=False,
                    saved=False,
                    dirty=bool(getattr(self._config_store, "dirty", False)),
                    error=self.tr(
                        "Wait for all MCP services to finish starting before regenerating the Token."
                    ),
                )
        result = self._config_store.regenerate_browser_auth_token()
        self._manager_config = self._config_store.snapshot()
        configured_token = self._browser_access_config().get("auth_token", "")
        if result.saved and is_valid_browser_auth_token(configured_token):
            self._browser_auth_token = configured_token
            self._browser_access_persisted = True
            self._browser_access_error = ""
        else:
            self._browser_access_error = result.error
        return result

    def running_service_manifests(self) -> list[ServiceManifest]:
        """Return manifests for services owned and running by this manager."""

        running = []
        for manifest in self._owned_manifest_snapshot():
            status = _safe_controller_status(self.controller, manifest)
            if _status_is_owned_and_running(status):
                running.append(manifest)
        return running

    def _service_env(self, manifest: ServiceManifest) -> dict[str, str]:
        env = {
            CORS_ORIGINS_ENV: encode_cors_origins(manifest.cors_origins),
        }
        policy = getattr(self, "_filesystem_policy", FilesystemPolicy())
        env.update(policy.to_environment())
        if _uses_qgis_bridge(manifest):
            env[BRIDGE_URL_ENV] = self.bridge.url
            env[QGIS_BRIDGE_AUTH_TOKEN_ENV] = self._bridge_auth_token
        return env

    def _bridge_url(self, manifest: ServiceManifest) -> str | None:
        return self.bridge.url if _uses_qgis_bridge(manifest) else None

    def _discover_and_start_default_services(self):
        try:
            manifests = _order_service_manifests(
                self._discover_services(),
                _startup_service_ids(self._manager_config),
            )
            manifests = _runtime_catalog_compatible_manifests(
                manifests,
                self.logger,
            )
        except Exception as err:
            self.logger.warning("Could not discover QCopilots MCP services: %s", err)
            manifests = []

        with self._state_lock:
            self._catalog_manifests = {manifest.service_id: manifest for manifest in manifests}
            self._catalog_states = {manifest.service_id: "stopped" for manifest in manifests}
            self._catalog_statuses.clear()
            self._catalog_startup_complete = False

        startup = self._manager_config["default_startup"]
        startup_ids = set(_startup_service_ids(self._manager_config)) if startup["enabled"] else set()
        jobs = []
        for manifest in manifests:
            if manifest.service_id not in startup_ids:
                continue
            auth_token = self.browser_auth_token()
            try:
                extra_env = self._service_env(manifest)
            except Exception as err:
                self._apply_service_start_failure(manifest, str(err))
                continue
            with self._state_lock:
                self._starting_service_ids.add(manifest.service_id)
                self._catalog_states[manifest.service_id] = "starting"
            jobs.append(
                {
                    "manifest": manifest,
                    "auth_token": auth_token,
                    "bridge_url": extra_env.get(BRIDGE_URL_ENV),
                    "extra_env": extra_env,
                    "port_conflict_policy": self._browser_access_config().get(
                        "port_conflict_policy",
                        "fail",
                    ),
                }
            )

        self._publish_runtime_catalog()
        if not jobs:
            with self._state_lock:
                self._catalog_startup_complete = True
            self._publish_runtime_catalog()
            return

        thread = QThread()
        worker = _DefaultStartupWorker(self.controller, jobs)
        self._startup_thread = thread
        self._startup_worker = worker
        worker.moveToThread(thread)
        thread.started.connect(worker.run)
        worker.service_finished.connect(self._event_relay.handle_default_start_result)
        worker.finished.connect(self._event_relay.handle_default_start_finished)
        worker.finished.connect(thread.quit)
        worker.finished.connect(worker.deleteLater)
        thread.finished.connect(thread.deleteLater)
        thread.finished.connect(lambda: self._clear_startup_thread(thread, worker))
        thread.start()

    def _handle_default_start_result(self, result: dict[str, Any]):
        if self.is_shutting_down():
            status = result.get("status")
            if _status_is_owned_and_running(status):
                manifest = result["manifest"]
                with self._state_lock:
                    self._owned_manifests[manifest.service_id] = manifest
                    self._shutdown_completed = False
                    auth_token = result.get("auth_token")
                    if auth_token:
                        self._service_auth_tokens[manifest.service_id] = auth_token
            return
        manifest = result["manifest"]
        if result["succeeded"]:
            self._apply_service_start_result(
                manifest,
                result["status"],
                result["auth_token"],
            )
            return
        self._apply_service_start_failure(manifest, result["detail"], result["status"])

    def _handle_default_start_finished(self, cancelled: bool):
        if self.is_shutting_down() or cancelled:
            return
        with self._state_lock:
            self._catalog_startup_complete = True
        self._publish_runtime_catalog()

    def _clear_startup_thread(self, thread, worker):
        if self._startup_thread is thread:
            self._startup_thread = None
        if self._startup_worker is worker:
            self._startup_worker = None

    def _cleanup_startup_thread(self, wait: bool = False) -> bool:
        worker = getattr(self, "_startup_worker", None)
        thread = getattr(self, "_startup_thread", None)
        if not thread:
            return True
        if worker and hasattr(worker, "cancel"):
            worker.cancel()
        if not hasattr(thread, "isRunning") or not thread.isRunning():
            self._startup_thread = None
            self._startup_worker = None
            return True
        if not wait:
            return False
        thread.quit()
        if not thread.wait(STARTUP_THREAD_WAIT_TIMEOUT_MS):
            self.logger.warning(
                "QCopilots default startup cancellation exceeded the normal wait; "
                "performing one final bounded wait"
            )
            if worker and hasattr(worker, "cancel"):
                worker.cancel()
            thread.quit()
            if not thread.wait(STARTUP_THREAD_FINAL_WAIT_SLICE_MS):
                self.logger.warning(
                    "QCopilots default startup worker is still running after cancellation"
                )
                return False
        if hasattr(QApplication, "processEvents"):
            QApplication.processEvents()
        self._startup_thread = None
        self._startup_worker = None
        return True

    def _emit_manager_event(self, event_name: str, *args):
        relay = getattr(self, "_event_relay", None)
        signal = getattr(relay, event_name, None) if relay else None
        if signal:
            signal.emit(*args)
            return
        handler = getattr(self, f"_handle_{event_name}")
        handler(*args)

    def _handle_service_starting(self, manifest: ServiceManifest):
        if self.is_shutting_down():
            return
        with self._state_lock:
            self._starting_service_ids.add(manifest.service_id)
            self._catalog_states[manifest.service_id] = "starting"
            self._catalog_statuses.pop(manifest.service_id, None)
        self._publish_runtime_catalog()

    def _handle_service_start_finished(self, manifest: ServiceManifest, status, auth_token: str):
        if self.is_shutting_down():
            if _status_is_owned_and_running(status):
                stopped_status, stop_error = _stop_after_failed_start(
                    self.controller,
                    manifest,
                    cancel_event=getattr(self, "_shutdown_event", None),
                )
                if _status_is_owned_and_running(stopped_status):
                    with self._state_lock:
                        self._owned_manifests[manifest.service_id] = manifest
                        self._shutdown_completed = False
                        if auth_token:
                            self._service_auth_tokens[manifest.service_id] = auth_token
                if stop_error is not None:
                    self.logger.warning(
                        "Failed to stop %s after shutdown was requested: %s",
                        manifest.service_id,
                        stop_error,
                    )
            return
        if _service_start_succeeded(status) and auth_token:
            self._apply_service_start_result(manifest, status, auth_token)
        else:
            self._apply_service_start_failure(manifest, _service_start_failure_detail(status), status)

    def _handle_service_start_failed(self, manifest: ServiceManifest, detail: str):
        if not self.is_shutting_down():
            self._apply_service_start_failure(manifest, detail)

    def _handle_service_stop_finished(self, manifest: ServiceManifest, status):
        if self.is_shutting_down():
            return
        with self._state_lock:
            self._starting_service_ids.discard(manifest.service_id)
            if status and status.running:
                auth_token = self._service_auth_tokens.get(manifest.service_id)
                if auth_token:
                    self._catalog_states[manifest.service_id] = "running"
                    self._catalog_statuses[manifest.service_id] = status
                else:
                    self._catalog_states[manifest.service_id] = "failed"
                    self._catalog_statuses.pop(manifest.service_id, None)
            else:
                self._owned_manifests.pop(manifest.service_id, None)
                self._service_auth_tokens.pop(manifest.service_id, None)
                self._catalog_states[manifest.service_id] = "stopped"
                self._catalog_statuses.pop(manifest.service_id, None)
        self._publish_runtime_catalog()
        self._refresh_dialog_service(manifest, status)

    def _handle_service_stop_failed(self, manifest: ServiceManifest, detail: str):
        if self.is_shutting_down():
            return
        self.logger.warning("Failed to stop %s: %s", manifest.service_id, detail)
        try:
            status = self.service_status(manifest, deep=False)
        except Exception:
            status = None
        self._handle_service_stop_finished(manifest, status)

    def _apply_service_start_result(self, manifest: ServiceManifest, status, auth_token: str):
        with self._state_lock:
            self._starting_service_ids.discard(manifest.service_id)
            self._owned_manifests[manifest.service_id] = manifest
            self._service_auth_tokens[manifest.service_id] = auth_token
            self._catalog_states[manifest.service_id] = "running"
            self._catalog_statuses[manifest.service_id] = status
        self._publish_runtime_catalog()
        self._refresh_dialog_service(manifest, status)

    def _apply_service_start_failure(self, manifest: ServiceManifest, detail: str, status=None):
        self.logger.warning("Failed to start %s: %s", manifest.service_id, detail)
        with self._state_lock:
            self._starting_service_ids.discard(manifest.service_id)
            if status and status.running and getattr(status, "owner_match", True):
                self._owned_manifests[manifest.service_id] = manifest
            self._service_auth_tokens.pop(manifest.service_id, None)
            self._catalog_states[manifest.service_id] = "failed"
            self._catalog_statuses.pop(manifest.service_id, None)
        self._publish_runtime_catalog()
        self._refresh_dialog_service(manifest, status)

    def _refresh_dialog_service(self, manifest: ServiceManifest, status):
        dialog = getattr(self, "dialog", None)
        if dialog and status and hasattr(dialog, "update_service_status"):
            dialog.update_service_status(manifest.service_id, status)

    def _publish_runtime_catalog(self):
        instance = getattr(QCoreApplication, "instance", None)
        app = instance() if instance else None
        with self._state_lock:
            existing_generation = _runtime_catalog_generation(app)
            self._catalog_generation = max(self._catalog_generation, existing_generation) + 1
            services = []
            for service_id, manifest in self._catalog_manifests.items():
                if len(services) >= RUNTIME_CATALOG_MAX_SERVICES:
                    break
                if service_id != getattr(manifest, "service_id", None):
                    continue
                if _runtime_catalog_manifest_error(manifest):
                    continue
                state = self._catalog_states.get(service_id, "stopped")
                if state not in RUNTIME_CATALOG_STATES:
                    state = "failed"
                status = self._catalog_statuses.get(service_id)
                auth_token = self._service_auth_tokens.get(service_id)
                if state == "running" and not _runtime_catalog_credentials_are_valid(
                    status,
                    auth_token,
                ):
                    state = "failed"
                item = {
                    "id": service_id,
                    "displayName": manifest.display_name,
                    "state": state,
                    "virtualUrl": f"https://qcopilots.localmachine/mcp/{quote(service_id, safe='')}",
                    "targetUrl": "",
                    "authToken": "",
                }
                if state == "running":
                    item["targetUrl"] = _runtime_target_url(manifest, status)
                    item["authToken"] = auth_token
                services.append(item)
            payload = {
                "schemaVersion": RUNTIME_CATALOG_SCHEMA_VERSION,
                "generation": self._catalog_generation,
                "startupComplete": self._catalog_startup_complete,
                "services": services,
            }
        if app and hasattr(app, "setProperty"):
            app.setProperty(
                RUNTIME_CATALOG_PROPERTY,
                json.dumps(payload, ensure_ascii=False, separators=(",", ":")),
            )

    def tr(self, message):
        return QCoreApplication.translate("QCopilotsMCPServersManager", message)


def _uses_qgis_bridge(manifest: ServiceManifest) -> bool:
    return "qgis-bridge" in manifest.capabilities or "processing" in manifest.capabilities


def _service_start_succeeded(status) -> bool:
    return bool(
        status
        and status.running
        and status.health == "ok"
        and getattr(status, "owner_match", True)
    )


def _status_is_owned_and_running(status) -> bool:
    return bool(
        status
        and getattr(status, "running", False)
        and getattr(status, "owner_match", False)
    )


def _safe_controller_status(controller: ProcessController, manifest: ServiceManifest):
    try:
        return controller.status(manifest, deep=False)
    except Exception:
        return None


def _callable_accepts_keyword(callback, keyword: str) -> bool:
    try:
        parameters = inspect.signature(callback).parameters.values()
    except (TypeError, ValueError):
        return False
    return any(
        parameter.kind == inspect.Parameter.VAR_KEYWORD
        or (
            parameter.name == keyword
            and parameter.kind
            in (inspect.Parameter.POSITIONAL_OR_KEYWORD, inspect.Parameter.KEYWORD_ONLY)
        )
        for parameter in parameters
    )


def _controller_start(
    controller: ProcessController,
    manifest: ServiceManifest,
    *,
    cancel_event: threading.Event | None = None,
    **kwargs,
):
    if "port_conflict_policy" in kwargs and not _callable_accepts_keyword(
        controller.start,
        "port_conflict_policy",
    ):
        kwargs.pop("port_conflict_policy")
    if cancel_event is not None and _callable_accepts_keyword(
        controller.start,
        "cancel_event",
    ):
        kwargs["cancel_event"] = cancel_event
    return controller.start(manifest, **kwargs)


def _controller_stop(
    controller: ProcessController,
    manifest: ServiceManifest,
    *,
    cancel_event: threading.Event | None = None,
):
    if cancel_event is not None and _callable_accepts_keyword(
        controller.stop,
        "cancel_event",
    ):
        return controller.stop(manifest, cancel_event=cancel_event)
    return controller.stop(manifest)


def _stop_after_failed_start(
    controller: ProcessController,
    manifest: ServiceManifest,
    *,
    cancel_event: threading.Event | None = None,
):
    try:
        return (
            _controller_stop(
                controller,
                manifest,
                cancel_event=cancel_event,
            ),
            None,
        )
    except Exception as err:
        return _safe_controller_status(controller, manifest), err


def _manager_status_snapshot(manager, manifest: ServiceManifest):
    snapshot = getattr(manager, "service_status_snapshot", None)
    if snapshot:
        return snapshot(manifest)
    return manager.service_status(manifest, deep=False)


def _manager_start_service(
    manager,
    manifest: ServiceManifest,
    auth_token: str | None,
    cancel_event: threading.Event,
):
    kwargs = {"auth_token": auth_token}
    if _callable_accepts_keyword(manager.start_service, "cancel_event"):
        kwargs["cancel_event"] = cancel_event
    return manager.start_service(manifest, **kwargs)


def _manager_stop_service(
    manager,
    manifest: ServiceManifest,
    cancel_event: threading.Event,
):
    if _callable_accepts_keyword(manager.stop_service, "cancel_event"):
        return manager.stop_service(manifest, cancel_event=cancel_event)
    return manager.stop_service(manifest)


def _runtime_catalog_compatible_manifests(
    manifests: list[ServiceManifest],
    logger,
) -> list[ServiceManifest]:
    compatible = []
    service_ids = set()
    for manifest in manifests:
        reason = _runtime_catalog_manifest_error(manifest)
        service_id = getattr(manifest, "service_id", "")
        if reason:
            logger.warning(
                "Ignoring MCP service %r in the native runtime catalog: %s",
                service_id,
                reason,
            )
            continue
        if service_id in service_ids:
            logger.warning(
                "Ignoring duplicate MCP service %r in the native runtime catalog",
                service_id,
            )
            continue
        if len(compatible) >= RUNTIME_CATALOG_MAX_SERVICES:
            logger.warning(
                "Ignoring MCP service %r because the native runtime catalog is full",
                service_id,
            )
            continue
        compatible.append(manifest)
        service_ids.add(service_id)
    return compatible


def _runtime_catalog_manifest_error(manifest: ServiceManifest) -> str:
    service_id = getattr(manifest, "service_id", None)
    if not isinstance(service_id, str) or not RUNTIME_CATALOG_SERVICE_ID_PATTERN.fullmatch(
        service_id
    ):
        return "service id does not match the native catalog contract"

    display_name = getattr(manifest, "display_name", None)
    if (
        not isinstance(display_name, str)
        or not display_name
        or display_name != display_name.strip()
        or _qt_utf16_length(display_name) > 256
        or any(ord(character) < 0x20 or ord(character) == 0x7F for character in display_name)
    ):
        return "display name does not match the native catalog contract"

    if getattr(manifest, "mcp_path", None) != "/mcp":
        return "MCP path must be exactly /mcp for the native catalog"
    return ""


def _runtime_catalog_credentials_are_valid(status, auth_token) -> bool:
    port = getattr(status, "port", None)
    if isinstance(port, bool) or not isinstance(port, int) or not 1 <= port <= 65535:
        return False
    if not isinstance(auth_token, str):
        return False
    if not 16 <= _qt_utf16_length(auth_token) <= 4096:
        return False
    return not any(
        character.isspace() or ord(character) < 0x21 or ord(character) == 0x7F
        for character in auth_token
    )


def _qt_utf16_length(value: str) -> int:
    return len(value.encode("utf-16-le", errors="surrogatepass")) // 2


def _service_start_failure_detail(status) -> str:
    if not status:
        return "Unknown service state"
    diagnostic = str(getattr(status, "diagnostic", "") or "").strip()
    if diagnostic:
        return diagnostic
    health = str(getattr(status, "health", "") or "").strip()
    return health or "Service did not become healthy"


def _controller_auth_token(controller: ProcessController, service_id: str) -> str | None:
    getter = getattr(controller, "current_auth_token", None)
    if not getter:
        return None
    try:
        token = getter(service_id)
    except Exception:
        return None
    return token if isinstance(token, str) and token else None


def _runtime_target_url(manifest: ServiceManifest, status) -> str:
    del manifest
    return f"http://127.0.0.1:{int(status.port)}/mcp"


def _runtime_catalog_generation(app) -> int:
    if not app or not hasattr(app, "property"):
        return 0
    try:
        value = app.property(RUNTIME_CATALOG_PROPERTY)
        if not value:
            return 0
        payload = json.loads(str(value))
        generation = payload.get("generation", 0)
        return generation if isinstance(generation, int) and generation >= 0 else 0
    except (AttributeError, TypeError, ValueError, json.JSONDecodeError):
        return 0


def _load_manager_config(config_path: Path, logger) -> dict[str, Any]:
    """Load one complete latest-format manager configuration.

    Missing, malformed, obsolete, and partial configurations are rejected. The
    caller decides how to surface the blocked state and no fallback is applied.
    """

    del logger
    try:
        data = json.loads(config_path.read_text(encoding="utf-8"))
    except Exception as err:
        raise ValueError(
            f"Could not read QCopilots manager config {config_path}: {err}"
        ) from err
    try:
        return validate_manager_config(data, str(config_path))
    except Exception as err:
        raise ValueError(
            f"Could not use QCopilots manager config {config_path}: {err}"
        ) from err


def _copy_manager_config(manager_config: dict[str, Any]) -> dict[str, Any]:
    return validate_manager_config(manager_config)


def _default_service_network_config() -> dict[str, Any]:
    return {
        "enabled": True,
        "host": DEFAULT_HOST,
        "advertised_host": DEFAULT_HOST,
        "cors_origins": [],
    }


def _default_browser_access_config() -> dict[str, Any]:
    return {
        "enabled": True,
        "origin_source": "configured_qcopilots_url",
        "auth_token": "",
        "port_conflict_policy": "fail",
    }


def _default_manager_config() -> dict[str, Any]:
    return {
        "config_version": MANAGER_CONFIG_VERSION,
        "default_startup": {
            "enabled": True,
            "service_ids": list(DEFAULT_STARTUP_SERVICE_IDS),
        },
        "service_network": _default_service_network_config(),
        "browser_access": _default_browser_access_config(),
        "security_policy": FilesystemPolicy().to_config(),
    }


def _normalize_manager_config(data: dict[str, Any], logger=None) -> dict[str, Any]:
    del logger
    return validate_manager_config(data)


def _startup_service_ids(manager_config: dict[str, Any]) -> list[str]:
    validated = validate_manager_config(manager_config)
    return list(validated["default_startup"]["service_ids"])


def _browser_origin_from_url(value: Any) -> str:
    """Serialize an HTTP URL to the Origin form used by browsers."""

    if not isinstance(value, str):
        return ""
    text = value.strip()
    if not text or any(ord(character) <= 0x20 for character in text):
        return ""
    try:
        parsed = urlsplit(text)
        scheme = parsed.scheme.lower()
        hostname = parsed.hostname
        port = parsed.port
    except (UnicodeError, ValueError):
        return ""
    if scheme not in ("http", "https") or not hostname:
        return ""
    if parsed.username is not None or parsed.password is not None:
        return ""
    try:
        ascii_hostname = hostname.encode("idna").decode("ascii").lower()
    except (UnicodeError, ValueError):
        return ""
    if not ascii_hostname or any(
        character in ascii_hostname for character in (" ", "/", "\\", "#", "?", "@", "%")
    ):
        return ""
    if ":" in ascii_hostname:
        try:
            ipaddress.IPv6Address(ascii_hostname)
        except ValueError:
            return ""
        serialized_host = f"[{ascii_hostname}]"
    else:
        serialized_host = ascii_hostname
    default_port = 80 if scheme == "http" else 443
    if port is not None and port != default_port:
        serialized_host = f"{serialized_host}:{port}"
    return f"{scheme}://{serialized_host}"


def _configured_qcopilots_origin(settings=None) -> str:
    settings = settings or QgsSettings()
    try:
        try:
            configured_url = settings.value(
                "QCopilots/serverUrl",
                DEFAULT_QCOPILOTS_URL,
                section=QgsSettings.Section.Plugins,
            )
        except TypeError:
            configured_url = settings.value(
                "QCopilots/serverUrl",
                DEFAULT_QCOPILOTS_URL,
                QgsSettings.Section.Plugins,
            )
    except Exception:
        return ""
    return _browser_origin_from_url(configured_url)


def _network_overrides_manifest(
    manifest: ServiceManifest,
    service_network: dict[str, Any],
    *,
    cors_origins: list[str] | tuple[str, ...] = (),
) -> ServiceManifest:
    host = DEFAULT_HOST
    advertised_host = DEFAULT_HOST
    safe_cors_origins = [
        origin
        for origin in cors_origins
        if isinstance(origin, str) and _browser_origin_from_url(origin) == origin
    ]
    return replace(
        manifest,
        transport=replace(
            manifest.transport,
            host=host,
            advertised_host=advertised_host,
        ),
        cors_origins=safe_cors_origins[:1],
    )


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


class BrowserAccessRestartWorker(QObject):
    """Restart the services which were running when the token was rotated."""

    finished = pyqtSignal(object)

    def __init__(
        self,
        plugin: QCopilotsMCPServersManagerPlugin,
        manifests: list[ServiceManifest],
    ):
        super().__init__()
        self.plugin = plugin
        self.manifests = list(manifests)
        self._cancelled = threading.Event()

    def cancel(self):
        self._cancelled.set()

    def run(self):
        failures = []
        restartable = []
        for manifest in self.manifests:
            if self._cancelled.is_set():
                break
            try:
                status = self.plugin.stop_service(
                    manifest,
                    cancel_event=self._cancelled,
                )
            except Exception as err:
                failures.append((manifest.service_id, str(err)))
                continue
            if status is not None and not getattr(status, "running", False):
                restartable.append(manifest)

        for manifest in restartable:
            if self._cancelled.is_set():
                break
            try:
                self.plugin._emit_manager_event("service_starting", manifest)
                status = self.plugin.start_service(
                    manifest,
                    auth_token=self.plugin.browser_auth_token(),
                    cancel_event=self._cancelled,
                )
                if not _service_start_succeeded(status):
                    failures.append(
                        (manifest.service_id, _service_start_failure_detail(status))
                    )
            except Exception as err:
                failures.append((manifest.service_id, str(err)))
        self.finished.emit(failures)


class BrowserAccessPanel(QFrame):
    def __init__(
        self,
        plugin: QCopilotsMCPServersManagerPlugin,
        parent=None,
    ):
        super().__init__(parent)
        self.plugin = plugin
        self.restart_thread = None
        self.restart_worker = None
        self.setObjectName("qcopilots_browser_access")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(7)

        title = QLabel(self.plugin.tr("Chrome manual connection"), self)
        title.setObjectName("qcopilots_browser_access_title")
        layout.addWidget(title)

        origin_row = QHBoxLayout()
        origin_row.addWidget(QLabel(self.plugin.tr("Allowed Origin"), self))
        self.origin_label = QLabel(self)
        self.origin_label.setObjectName("qcopilots_browser_origin")
        self.origin_label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        origin_row.addWidget(self.origin_label, 1)
        layout.addLayout(origin_row)

        token_row = QHBoxLayout()
        token_row.addWidget(QLabel(self.plugin.tr("Shared Bearer Token"), self))
        self.token_edit = QLineEdit(self)
        self.token_edit.setObjectName("qcopilots_browser_auth_token")
        self.token_edit.setReadOnly(True)
        self.token_edit.setEchoMode(QLineEdit.EchoMode.Password)
        token_row.addWidget(self.token_edit, 1)

        self.copy_button = QToolButton(self)
        self.copy_button.setObjectName("qcopilots_browser_token_copy_button")
        self.copy_button.setText(self.plugin.tr("Copy Token"))
        self.copy_button.clicked.connect(self.copy_token)
        token_row.addWidget(self.copy_button)

        self.regenerate_button = QToolButton(self)
        self.regenerate_button.setObjectName("qcopilots_browser_token_regenerate_button")
        self.regenerate_button.setText(self.plugin.tr("Regenerate Token"))
        self.regenerate_button.clicked.connect(self.regenerate_token)
        token_row.addWidget(self.regenerate_button)
        layout.addLayout(token_row)

        self.status_label = QLabel(self)
        self.status_label.setObjectName("qcopilots_browser_access_status")
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)
        self.update_display()

    def update_display(self):
        snapshot = self.plugin.browser_access_snapshot()
        origin = snapshot["origin"] or self.plugin.tr("Invalid configured QCopilots URL")
        self.origin_label.setText(origin)
        self.token_edit.setText(snapshot["auth_token"])
        self.copy_button.setEnabled(snapshot["available"])
        self.regenerate_button.setEnabled(self.restart_thread is None)
        if snapshot["available"]:
            self.status_label.setText(
                self.plugin.tr(
                    "Use this Token as Authorization for all six 127.0.0.1 MCP endpoints. "
                    "Keep Use llama-server proxy turned off. The Token is stored as plaintext "
                    "in the current user configuration. Chrome may ask for Local Network Access permission."
                )
            )
        else:
            detail = snapshot["error"] or self.plugin.tr(
                "Browser access is disabled until the Token can be saved and the configured URL is valid."
            )
            self.status_label.setText(detail)

    def copy_token(self):
        snapshot = self.plugin.browser_access_snapshot()
        if not snapshot["available"]:
            return
        QApplication.clipboard().setText(snapshot["auth_token"])
        self.status_label.setText(self.plugin.tr("Shared Bearer Token copied."))

    def regenerate_token(self):
        answer = QMessageBox.question(
            self,
            self.plugin.tr("Regenerate shared Token"),
            self.plugin.tr(
                "Regenerating the Token invalidates the current Chrome configuration and restarts all running MCP services. Continue?"
            ),
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if answer != QMessageBox.StandardButton.Yes:
            return
        result = self.plugin.regenerate_browser_auth_token()
        self.update_display()
        if not result.saved:
            self.status_label.setText(
                result.error or self.plugin.tr("The new Token could not be saved.")
            )
            return

        manifests = self.plugin.running_service_manifests()
        if not manifests:
            self.status_label.setText(
                self.plugin.tr("Shared Bearer Token regenerated. No running services needed a restart.")
            )
            return
        self._start_restart(manifests)

    def _start_restart(self, manifests: list[ServiceManifest]):
        self.regenerate_button.setEnabled(False)
        self.copy_button.setEnabled(False)
        self.status_label.setText(self.plugin.tr("Restarting running MCP services..."))
        thread = QThread(self)
        worker = BrowserAccessRestartWorker(self.plugin, manifests)
        self.restart_thread = thread
        self.restart_worker = worker
        worker.moveToThread(thread)
        thread.started.connect(worker.run)
        worker.finished.connect(self._finish_restart)
        worker.finished.connect(thread.quit)
        worker.finished.connect(worker.deleteLater)
        thread.finished.connect(thread.deleteLater)
        thread.finished.connect(self._clear_restart)
        thread.start()

    def _finish_restart(self, failures):
        if failures:
            service_ids = ", ".join(service_id for service_id, _detail in failures)
            self.status_label.setText(
                self.plugin.tr("Token saved, but these services failed to restart: {0}").format(
                    service_ids
                )
            )
        else:
            self.status_label.setText(
                self.plugin.tr("Shared Bearer Token regenerated and running services restarted.")
            )

    def _clear_restart(self):
        self.restart_thread = None
        self.restart_worker = None
        snapshot = self.plugin.browser_access_snapshot()
        self.token_edit.setText(snapshot["auth_token"])
        self.copy_button.setEnabled(snapshot["available"])
        self.regenerate_button.setEnabled(True)

    def cleanup_restart_thread(self, wait: bool = False) -> bool:
        thread = self.restart_thread
        worker = self.restart_worker
        if not thread:
            return True
        if hasattr(thread, "isRunning") and thread.isRunning():
            if not wait:
                return False
            if worker and hasattr(worker, "cancel"):
                worker.cancel()
            thread.quit()
            if not thread.wait(TOGGLE_THREAD_WAIT_TIMEOUT_MS):
                return False
        self.restart_thread = None
        self.restart_worker = None
        return True


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

        self.browser_access_panel = BrowserAccessPanel(self.plugin, self)
        layout.addWidget(self.browser_access_panel)

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
            "QFrame#qcopilots_browser_access { background: #ffffff; border: 1px solid #dce4ef;"
            " border-radius: 8px; }"
            "QLabel#qcopilots_browser_access_title { color: #202733; font-weight: 600; }"
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
        browser_access_panel = getattr(self, "browser_access_panel", None)
        if browser_access_panel:
            browser_access_panel.update_display()

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
        self.footer_label.setText(
            self.plugin.tr("{0} of {1} MCP servers").format(len(manifests), len(manifests))
        )

    def update_service_status(self, service_id: str, status):
        for index in range(self.content_layout.count()):
            card = self.content_layout.itemAt(index).widget()
            manifest = getattr(card, "manifest", None)
            if not manifest or manifest.service_id != service_id:
                continue
            card.status = status
            card.update_status_widgets()
            return

    def prepare_close(self, wait: bool = False) -> bool:
        browser_access_panel = getattr(self, "browser_access_panel", None)
        if browser_access_panel and not browser_access_panel.cleanup_restart_thread(wait=wait):
            return False
        for index in range(self.content_layout.count()):
            widget = self.content_layout.itemAt(index).widget()
            if hasattr(widget, "cleanup_toggle_thread") and not widget.cleanup_toggle_thread(wait=wait):
                return False
        return True

    def cancel_service_actions(self):
        for index in range(self.content_layout.count()):
            widget = self.content_layout.itemAt(index).widget()
            worker = getattr(widget, "toggle_worker", None)
            if worker and hasattr(worker, "cancel"):
                worker.cancel()

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
        manager: QCopilotsMCPServersManagerPlugin,
        manifest: ServiceManifest,
        enabled: bool,
        was_running: bool,
        auth_token: str | None,
        unhealthy_message: str,
        unknown_state_message: str,
        shutdown_requested=None,
    ):
        super().__init__()
        self.manager = manager
        self.controller = manager.controller
        self.logger = manager.logger
        self.manifest = manifest
        self.enabled = enabled
        self.was_running = was_running
        self.auth_token = auth_token
        self.unhealthy_message = unhealthy_message
        self.unknown_state_message = unknown_state_message
        self.shutdown_requested = shutdown_requested or (lambda: False)
        self._cancelled = threading.Event()

    def cancel(self):
        self._cancelled.set()

    def run(self):
        try:
            status = self._toggle_service()
        except Exception as err:
            self.failed.emit(str(err), self._safe_service_status())
            return

        if (
            self.enabled
            and status
            and status.running
            and (self.shutdown_requested() or self._cancelled.is_set())
        ):
            self.finished.emit(self._safe_stop_service(status))
            return

        if self._reached_target(status):
            self.finished.emit(status)
            return

        detail = self._failure_detail(status)
        if self.enabled and status and status.running:
            startup_detail = detail
            status = self._safe_stop_service(status)
            detail = startup_detail
        self.failed.emit(detail, status)

    def _toggle_service(self):
        if self.enabled and not self.was_running:
            if self._cancelled.is_set():
                return self._safe_service_status()
            return _manager_start_service(
                self.manager,
                self.manifest,
                self.auth_token,
                self._cancelled,
            )
        if not self.enabled and self.was_running:
            return _manager_stop_service(
                self.manager,
                self.manifest,
                self._cancelled,
            )
        return self.manager.service_status(self.manifest)

    def _reached_target(self, status) -> bool:
        if not status:
            return False
        if self.enabled:
            return _service_start_succeeded(status)
        return not status.running

    def _safe_service_status(self):
        try:
            return self.manager.service_status(self.manifest)
        except Exception:
            return None

    def _failure_detail(self, status) -> str:
        if not status:
            return self.unknown_state_message
        diagnostic = str(getattr(status, "diagnostic", "") or "").strip()
        if diagnostic:
            return diagnostic
        if self.enabled and getattr(status, "running", False):
            return self.unhealthy_message
        health = str(getattr(status, "health", "") or "").strip()
        return health or self.unhealthy_message

    def _safe_stop_service(self, fallback_status):
        try:
            return _manager_stop_service(
                self.manager,
                self.manifest,
                self._cancelled,
            )
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
        self.status = _manager_status_snapshot(plugin, manifest)
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
        self.log_file_label = self._details_row(
            details_layout,
            self.plugin.tr("Log file"),
            str(getattr(self.status, "log_file", "") or ""),
        )
        self.diagnostic_label = self._details_row(
            details_layout,
            self.plugin.tr("Startup diagnostic"),
            str(getattr(self.status, "diagnostic", "") or ""),
        )
        self.diagnostic_label.setWordWrap(True)
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
        auth_token = None
        if enabled and not was_running:
            try:
                auth_token = self.plugin.prepare_service_start(self.manifest)
                if not auth_token:
                    raise RuntimeError(self.plugin.tr("Service is already starting"))
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
            self.plugin,
            self.manifest,
            enabled,
            was_running,
            auth_token,
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
        worker = self.toggle_worker
        if not thread:
            return True
        if wait and worker and hasattr(worker, "cancel"):
            worker.cancel()
        if hasattr(thread, "isRunning") and thread.isRunning():
            if not wait:
                return False
            thread.quit()
            if not thread.wait(TOGGLE_THREAD_WAIT_TIMEOUT_MS):
                self.plugin.logger.warning(
                    "QCopilots %s MCP server action cancellation exceeded the normal wait; "
                    "performing one final bounded wait",
                    self.manifest.service_id,
                )
                if worker and hasattr(worker, "cancel"):
                    worker.cancel()
                thread.quit()
                if not thread.wait(TOGGLE_THREAD_FINAL_WAIT_SLICE_MS):
                    self.plugin.logger.warning(
                        "QCopilots %s MCP server action is still running after cancellation",
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
        if self._plugin_is_shutting_down():
            if enabled and self._stop_started_service_during_shutdown():
                return
            self.switch.setEnabled(True)
            self.update_status_widgets()
            return
        self.switch.setEnabled(True)
        if not self._status_reached_target(enabled):
            self.update_status_widgets()
            self._show_temporary_status(failure_text, self.status.health)
            return
        self.update_status_widgets()
        remember = getattr(self.plugin, "remember_service_startup", None)
        if not remember:
            return
        try:
            result = remember(self.manifest.service_id, enabled)
        except Exception as err:
            self.plugin.logger.warning(
                "Could not update the startup preference for %s: %s",
                self.manifest.service_id,
                err,
            )
            self._show_temporary_status(
                self.plugin.tr("Preference not saved"),
                str(err),
            )
            return
        if result is not None and not result.saved:
            self._show_temporary_status(
                self.plugin.tr("Preference not saved"),
                result.error
                or self.plugin.tr("The service changed state, but its startup preference was not saved."),
            )

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
        diagnostic = str(getattr(self.status, "diagnostic", "") or "")
        self.running_label.setToolTip(diagnostic)
        if self.switch.isChecked() != self.status.running:
            self.switch.blockSignals(True)
            self.switch.setChecked(self.status.running)
            self.switch.blockSignals(False)
        self.inline_endpoint_label.setText(self.status.url)
        self.endpoint_label.setText(self.status.url)
        if getattr(self, "log_file_label", None):
            self.log_file_label.setText(str(getattr(self.status, "log_file", "") or ""))
        if getattr(self, "diagnostic_label", None):
            self.diagnostic_label.setText(diagnostic)
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
    if "qgis_binary" in manifest.service_id:
        return "#16697a"
    if "raster" in manifest.service_id:
        return "#2eb67d"
    if "vector" in manifest.service_id:
        return "#8758ff"
    if "skills" in manifest.service_id:
        return "#f2a541"
    return "#7a8ca3"
