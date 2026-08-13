"""Static tests for the QCopilots C++ browser container plugin.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import re
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


class TestQCopilotsContainer(unittest.TestCase):
    def setUp(self):
        self.repo_root = Path(__file__).resolve().parents[3]
        self.plugin_root = self.repo_root / "src" / "plugins" / "qcopilots"

    def source_block(self, text, signature):
        start = text.index(signature)
        brace_start = text.index("{", start)
        depth = 0
        for index in range(brace_start, len(text)):
            if text[index] == "{":
                depth += 1
            elif text[index] == "}":
                depth -= 1
                if depth == 0:
                    return text[brace_start : index + 1]
        self.fail(f"Could not find block for {signature}")

    def assert_web_setting(self, source, attribute, value):
        self.assertRegex(
            source,
            rf"setAttribute\(\s*QWebEngineSettings::{attribute},\s*{value}\s*\)",
        )

    def assert_single_web_setting(self, source, attribute, value):
        matches = re.findall(
            rf"setAttribute\(\s*QWebEngineSettings::{attribute},\s*(true|false)\s*\)",
            source,
        )
        self.assertEqual([value], matches)

    def test_container_uses_normalized_file_names(self):
        expected_files = (
            "qcopilots_container_plugin.cpp",
            "qcopilots_container_plugin.h",
            "qcopilots_container_dock.cpp",
            "qcopilots_container_dock.h",
            "qcopilots_container_utils.cpp",
            "qcopilots_container_utils.h",
            "qcopilots_mcp_bridge.cpp",
            "qcopilots_mcp_bridge.h",
            "qcopilots_mcp_catalog.cpp",
            "qcopilots_mcp_catalog.h",
            "ui/qcopilots_container_dock.ui",
        )
        legacy_files = (
            "qgsqcopilotsplugin.cpp",
            "qgsqcopilotsplugin.h",
            "qgsqcopilotsdock.cpp",
            "qgsqcopilotsdock.h",
            "qgsqcopilotsutils.cpp",
            "qgsqcopilotsutils.h",
            "ui/qgsqcopilotsdock.ui",
        )
        cmake = (self.plugin_root / "CMakeLists.txt").read_text(encoding="utf-8")

        for file_name in expected_files:
            self.assertTrue((self.plugin_root / file_name).is_file(), file_name)
            self.assertIn(file_name.replace("\\", "/"), cmake)

        for file_name in legacy_files:
            self.assertFalse((self.plugin_root / file_name).exists(), file_name)
            self.assertNotIn(file_name.replace("\\", "/"), cmake)

    def test_container_source_has_no_legacy_generated_file_references(self):
        legacy_tokens = (
            "qgsqcopilotsplugin.cpp",
            "qgsqcopilotsplugin.h",
            "qgsqcopilotsdock.cpp",
            "qgsqcopilotsdock.h",
            "qgsqcopilotsutils.cpp",
            "qgsqcopilotsutils.h",
            "qgsqcopilotsdock.ui",
            "ui_qgsqcopilotsdock",
            "moc_qgsqcopilots",
        )

        source_suffixes = {".cpp", ".h", ".ui", ".qrc", ".txt"}
        for path in self.plugin_root.rglob("*"):
            if not path.is_file():
                continue
            if path.name != "CMakeLists.txt" and path.suffix.lower() not in source_suffixes:
                continue

            text = path.read_text(encoding="utf-8")
            for token in legacy_tokens:
                self.assertNotIn(token, text, str(path))

    def test_container_registers_qcopilots_menu_and_actions(self):
        plugin_cpp = (self.plugin_root / "qcopilots_container_plugin.cpp").read_text(encoding="utf-8")

        self.assertIn('tr( "QCopilots" )', plugin_cpp)
        self.assertIn('QStringLiteral( "mQCopilotsMenu" )', plugin_cpp)
        self.assertIn('QStringLiteral( "mActionQCopilots" )', plugin_cpp)
        self.assertIn('QStringLiteral( "mActionConfigureQCopilotsUrl" )', plugin_cpp)
        self.assertIn('setProperty( "qcopilotsCreatedByPlugin", true )', plugin_cpp)
        self.assertIn("addDockWidget", plugin_cpp)
        self.assertIn("QWebEngineView", (self.plugin_root / "qcopilots_container_dock.h").read_text(encoding="utf-8"))
        ensure_menu_block = self.source_block(plugin_cpp, "void QgsQCopilotsPlugin::ensureMenu")
        self.assertLess(ensure_menu_block.index("mMenu = candidate"), ensure_menu_block.index("mOwnsMenu = false"))
        self.assertIn("mOwnsMenu = true", ensure_menu_block)
        unload_block = self.source_block(plugin_cpp, "void QgsQCopilotsPlugin::unload")
        self.assertIn("mOwnsMenu && mMenu->actions().isEmpty()", unload_block)

        common_menu = (
            self.repo_root / "python" / "plugins" / "qcopilots_common" / "menu.py"
        ).read_text(encoding="utf-8")
        self.assertIn('MENU_SHARED_CREATED_PROPERTY = "qcopilotsCreatedByPlugin"', common_menu)
        self.assertIn("menu.setProperty(MENU_SHARED_CREATED_PROPERTY, True)", common_menu)
        self.assertIn("menu.property(MENU_SHARED_CREATED_PROPERTY)", common_menu)

    def test_python_menu_removal_preserves_shared_menu_until_empty(self):
        from qcopilots_common.menu import (
            MENU_CREATED_PROPERTY,
            MENU_OBJECT_NAME,
            MENU_SHARED_CREATED_PROPERTY,
            remove_qcopilots_menu_action,
        )

        class FakeAction:
            def __init__(self, menu=None):
                self._menu = menu

            def menu(self):
                return self._menu

        class FakeMenu:
            def __init__(self, actions, properties):
                self._actions = list(actions)
                self._properties = dict(properties)
                self.deleted = False
                self._menu_action = FakeAction(self)

            def actions(self):
                return list(self._actions)

            def removeAction(self, action):
                if action in self._actions:
                    self._actions.remove(action)

            def property(self, name):
                return self._properties.get(name)

            def menuAction(self):
                return self._menu_action

            def deleteLater(self):
                self.deleted = True

            def objectName(self):
                return MENU_OBJECT_NAME

            def title(self):
                return "&QCopilots"

        class FakeMenuBar:
            def __init__(self, menu):
                self._menu = menu
                self.removed_actions = []

            def actions(self):
                return [self._menu.menuAction()]

            def removeAction(self, action):
                self.removed_actions.append(action)

        class FakeMainWindow:
            def __init__(self, menu_bar):
                self._menu_bar = menu_bar

            def menuBar(self):
                return self._menu_bar

        class FakeIface:
            def __init__(self, menu_bar):
                self._main_window = FakeMainWindow(menu_bar)

            def mainWindow(self):
                return self._main_window

        managed_action = object()
        external_action = object()
        shared_menu = FakeMenu(
            [managed_action, external_action],
            {MENU_SHARED_CREATED_PROPERTY: True},
        )
        shared_menu_bar = FakeMenuBar(shared_menu)
        remove_qcopilots_menu_action(FakeIface(shared_menu_bar), managed_action)
        self.assertFalse(shared_menu.deleted)
        self.assertEqual(shared_menu_bar.removed_actions, [])

        last_action_menu = FakeMenu(
            [managed_action],
            {MENU_SHARED_CREATED_PROPERTY: True},
        )
        last_action_menu_bar = FakeMenuBar(last_action_menu)
        remove_qcopilots_menu_action(FakeIface(last_action_menu_bar), managed_action)
        self.assertTrue(last_action_menu.deleted)
        self.assertEqual(last_action_menu_bar.removed_actions, [last_action_menu.menuAction()])

        legacy_menu = FakeMenu([managed_action], {MENU_CREATED_PROPERTY: True})
        legacy_menu_bar = FakeMenuBar(legacy_menu)
        remove_qcopilots_menu_action(FakeIface(legacy_menu_bar), managed_action)
        self.assertTrue(legacy_menu.deleted)

        foreign_menu = FakeMenu([managed_action], {})
        foreign_menu_bar = FakeMenuBar(foreign_menu)
        remove_qcopilots_menu_action(FakeIface(foreign_menu_bar), managed_action)
        self.assertFalse(foreign_menu.deleted)
        self.assertEqual(foreign_menu_bar.removed_actions, [])

    def test_container_uses_qt6_webengine_and_no_qt5_branch(self):
        cmake = (self.plugin_root / "CMakeLists.txt").read_text(encoding="utf-8")
        plugins_cmake = (self.repo_root / "src" / "plugins" / "CMakeLists.txt").read_text(encoding="utf-8")
        python_tests_cmake = (self.repo_root / "tests" / "src" / "python" / "CMakeLists.txt").read_text(encoding="utf-8")
        combined_cmake = "\n".join((plugins_cmake, python_tests_cmake, cmake))

        self.assertIn("if (WITH_QTWEBENGINE)", plugins_cmake)
        self.assertIn("add_subdirectory(qcopilots)", plugins_cmake)
        self.assertIn("if (WITH_QTWEBENGINE)", python_tests_cmake)
        self.assertIn("ADD_PYTHON_TEST(PyQCopilotsContainer test_qcopilots_container.py)", python_tests_cmake)
        self.assertEqual(
            1,
            len(re.findall(r"^\s*add_subdirectory\s*\(\s*qcopilots\s*\)\s*$", plugins_cmake, re.MULTILINE)),
        )
        self.assertEqual(
            1,
            len(
                re.findall(
                    r"^\s*ADD_PYTHON_TEST\s*\(\s*PyQCopilotsContainer\s+test_qcopilots_container\.py\s*\)\s*$",
                    python_tests_cmake,
                    re.MULTILINE,
                )
            ),
        )
        self.assertRegex(
            plugins_cmake,
            r"if\s*\(\s*WITH_QTWEBENGINE\s*\)\s*add_subdirectory\s*\(\s*qcopilots\s*\)\s*endif\s*\(\s*\)",
        )
        self.assertRegex(
            python_tests_cmake,
            r"if\s*\(\s*WITH_QTWEBENGINE\s*\)\s*ADD_PYTHON_TEST\s*\(\s*PyQCopilotsContainer\s+test_qcopilots_container\.py\s*\)\s*endif\s*\(\s*\)",
        )
        self.assertIn(
            "find_package(${QT_VERSION_BASE} COMPONENTS Network WebChannel WebEngineWidgets REQUIRED)",
            cmake,
        )
        self.assertIn("Qt::Network", cmake)
        self.assertIn("Qt::WebChannel", cmake)
        self.assertIn("WebEngineWidgets", cmake)
        self.assertIn("Qt::WebEngineWidgets", cmake)
        self.assertIsNone(
            re.search(
                r'(?:Qt6[A-Za-z_]*_VERSION|QT_VERSION|Qt\d+Core_VERSION)\s*VERSION_(?:GREATER_EQUAL|GREATER|LESS_EQUAL|LESS|EQUAL)\s*"?6\.8',
                combined_cmake,
            )
        )
        self.assertNotIn("6.8", combined_cmake)
        self.assertIsNone(
            re.search(r"\bQT_VERSION_(?:MAJOR|MINOR|PATCH)\b", combined_cmake)
        )
        self.assertIsNone(
            re.search(r"find_package\s*\(\s*\$\{QT_VERSION_BASE\}\s+\"?6\.8", cmake)
        )
        self.assertIsNone(re.search(r"\bQt5(?:Core_VERSION|[A-Za-z_]*_VERSION|::)?\b", combined_cmake))
        for path in self.plugin_root.rglob("*"):
            if path.suffix.lower() not in {".cpp", ".h", ".ui", ".txt"} and path.name != "CMakeLists.txt":
                continue

            text = path.read_text(encoding="utf-8")
            self.assertNotIn("QT_VERSION_CHECK", text, str(path))
            self.assertNotIn("QWebEnginePage::ClipboardReadWrite", text, str(path))
            self.assertNotIn("QWebEnginePermission", text, str(path))
            self.assertNotRegex(text, r"\bQt5(?:Core_VERSION|[A-Za-z_]*_VERSION|::)?\b", str(path))
            self.assertNotIn("setCodec", text, str(path))

        utils_cpp = (self.plugin_root / "qcopilots_container_utils.cpp").read_text(encoding="utf-8")
        self.assertIn("QStringConverter::Utf8", utils_cpp)

    def test_default_url_and_log_paths_are_stable(self):
        utils_cpp = (self.plugin_root / "qcopilots_container_utils.cpp").read_text(encoding="utf-8")
        dock_cpp = (self.plugin_root / "qcopilots_container_dock.cpp").read_text(encoding="utf-8")

        self.assertIn('http://127.0.0.1:8282', utils_cpp)
        self.assertIn('qcopilots-container.log', utils_cpp)
        self.assertIn('QStringLiteral( "storage" )', utils_cpp)
        self.assertIn('QStringLiteral( "cache" )', utils_cpp)
        plugin_cpp = (self.plugin_root / "qcopilots_container_plugin.cpp").read_text(encoding="utf-8")
        self.assertIn("Invalid QCopilots URL.", plugin_cpp)
        self.assertIn("displayUrl", dock_cpp)

    def test_connectivity_probe_cancellation_is_safe_for_synchronous_finished_signal(self):
        dock_cpp = (self.plugin_root / "qcopilots_container_dock.cpp").read_text(encoding="utf-8")
        dock_h = (self.plugin_root / "qcopilots_container_dock.h").read_text(encoding="utf-8")
        destructor_block = self.source_block(dock_cpp, "QgsQCopilotsDock::~QgsQCopilotsDock")
        cancel_block = self.source_block(dock_cpp, "void QgsQCopilotsDock::cancelConnectivityProbe")
        start_block = self.source_block(dock_cpp, "void QgsQCopilotsDock::startConnectivityProbe")

        self.assertIn("void cancelConnectivityProbe();", dock_h)
        self.assertIn("cancelConnectivityProbe();", destructor_block)
        self.assertIn("cancelConnectivityProbe();", start_block)
        self.assertLess(cancel_block.index("mProbeReply = nullptr"), cancel_block.index("reply->abort()"))
        self.assertLess(cancel_block.index("QObject::disconnect"), cancel_block.index("reply->abort()"))
        self.assertLess(cancel_block.index("reply->abort()"), cancel_block.index("reply->deleteLater()"))
        self.assertNotIn("mProbeReply->abort()", dock_cpp)

    def test_webengine_profile_uses_stable_profile_with_process_fallback(self):
        utils_h = (self.plugin_root / "qcopilots_container_utils.h").read_text(encoding="utf-8")
        utils_cpp = (self.plugin_root / "qcopilots_container_utils.cpp").read_text(encoding="utf-8")
        dock_cpp = (self.plugin_root / "qcopilots_container_dock.cpp").read_text(encoding="utf-8")
        profile_block = self.source_block(dock_cpp, "QWebEngineProfile *qcopilotsProfile")

        self.assertIn("QString webEngineProfileName();", utils_h)
        self.assertIn("#include <QCoreApplication>", utils_cpp)
        self.assertIn("#include <QLockFile>", utils_cpp)
        self.assertIn("QCoreApplication::applicationPid()", utils_cpp)
        self.assertIn("QLockFile", utils_cpp)
        self.assertIn("setStaleLockTime( 0 )", utils_cpp)
        self.assertIn("tryLock( 0 )", utils_cpp)
        self.assertIn('QStringLiteral( "profile.lock" )', utils_cpp)
        self.assertIn('QStringLiteral( "webengine" )', utils_cpp)
        self.assertIn('QStringLiteral( "qcopilots" )', utils_cpp)
        self.assertIn('QStringLiteral( "qcopilots-%1" )', utils_cpp)
        self.assertIn("processWebEngineProfileName()", utils_cpp)
        self.assertIn("QgsQCopilotsUtils::webEngineProfileName()", profile_block)
        self.assertIn("QgsQCopilotsUtils::webEngineStorageDirectory()", profile_block)
        self.assertIn("QgsQCopilotsUtils::webEngineCacheDirectory()", profile_block)
        self.assertNotIn('new QWebEngineProfile( QStringLiteral( "qcopilots" )', profile_block)
        self.assertNotIn('QStringLiteral( "webengine/storage" )', utils_cpp)
        self.assertNotIn('QStringLiteral( "webengine/cache" )', utils_cpp)

    def test_dock_contains_only_web_view_content(self):
        ui = (self.plugin_root / "ui" / "qcopilots_container_dock.ui").read_text(encoding="utf-8")
        dock_cpp = (self.plugin_root / "qcopilots_container_dock.cpp").read_text(encoding="utf-8")
        plugin_cpp = (self.plugin_root / "qcopilots_container_plugin.cpp").read_text(encoding="utf-8")

        self.assertIn('name="webEngineView"', ui)
        self.assertNotIn('name="navigationBar"', ui)
        self.assertNotIn('name="addressLineEdit"', ui)
        self.assertNotIn('name="openButton"', ui)
        self.assertNotIn("setAddressBarUrl", dock_cpp)
        self.assertIn("Configure QCopilots URL", plugin_cpp)
        self.assertNotIn("QCopilotsSetupURL", plugin_cpp)
        self.assertIn("mDock->loadUrl( url, true )", plugin_cpp)

    def test_dock_enables_chrome_like_keyboard_and_clipboard_behavior(self):
        dock_cpp = (self.plugin_root / "qcopilots_container_dock.cpp").read_text(encoding="utf-8")
        dock_h = (self.plugin_root / "qcopilots_container_dock.h").read_text(encoding="utf-8")
        settings_block = self.source_block(dock_cpp, "void configureQCopilotsWebSettings")

        self.assertIn("configureQCopilotsWebSettings", dock_cpp)
        self.assert_web_setting(settings_block, "JavascriptEnabled", "true")
        self.assert_web_setting(settings_block, "LocalStorageEnabled", "true")
        self.assert_web_setting(settings_block, "JavascriptCanOpenWindows", "false")
        self.assert_web_setting(settings_block, "JavascriptCanAccessClipboard", "true")
        self.assert_web_setting(settings_block, "JavascriptCanPaste", "false")
        self.assert_web_setting(settings_block, "WebGLEnabled", "true")
        self.assert_web_setting(settings_block, "Accelerated2dCanvasEnabled", "true")
        self.assert_single_web_setting(settings_block, "ScrollAnimatorEnabled", "false")
        self.assert_web_setting(settings_block, "ScreenCaptureEnabled", "false")
        self.assert_web_setting(settings_block, "PlaybackRequiresUserGesture", "true")
        self.assert_web_setting(settings_block, "DnsPrefetchEnabled", "false")
        self.assertIn("configureQCopilotsWebSettings( sProfile->settings() )", dock_cpp)
        self.assertIn("configureQCopilotsWebSettings( mWebView->settings() )", dock_cpp)
        self.assertIn("qcopilots-desktop-web-fixups", dock_cpp)
        self.assertIn("event.key !== 'Enter'", dock_cpp)
        self.assertIn("event.shiftKey", dock_cpp)
        self.assertIn("button[type=\"submit\"]", dock_cpp)
        self.assertIn("sendButton.click()", dock_cpp)
        self.assertIn("handleFeaturePermissionRequested", dock_h)
        self.assertIn("isTrustedWebUiOrigin", dock_h)
        self.assertIn("featurePermissionRequested", dock_cpp)
        self.assertIn("setFeaturePermission", dock_cpp)
        self.assertIn("QMetaEnum::fromType< QWebEnginePage::Feature >", dock_cpp)
        self.assertIn('keyToValue( "ClipboardReadWrite", &ok )', dock_cpp)
        self.assertIn("clipboardReadWriteFeature >= 0", dock_cpp)
        self.assertNotIn("QWebEnginePermission", dock_cpp)
        self.assertNotIn("QWebEnginePermission", dock_h)
        self.assertNotIn("permissionRequested", dock_cpp)
        self.assertNotIn("QWebEnginePage::ClipboardReadWrite", dock_cpp)
        self.assertIn("sameWebUiOrigin", dock_cpp)
        self.assertIn("effectivePort", dock_cpp)

        trusted_origin_block = self.source_block(dock_cpp, "bool QgsQCopilotsDock::isTrustedWebUiOrigin")
        self.assertIn("sameWebUiOrigin( origin, configuredUrl() )", trusted_origin_block)
        self.assertIn("sameWebUiOrigin( origin, mPendingUrl )", trusted_origin_block)
        self.assertNotIn("sameWebUiOrigin( origin, currentUrl() )", trusted_origin_block)
        self.assertNotIn("sameWebUiOrigin( origin, mLastSuccessfulUrl )", trusted_origin_block)
        self.assertNotIn("sameWebUiOrigin( origin, QgsQCopilotsUtils::defaultServerUrl() )", trusted_origin_block)
        self.assertNotIn("isSupportedWebUiUrl( origin )", trusted_origin_block)

        permission_block = self.source_block(dock_cpp, "void QgsQCopilotsDock::handleFeaturePermissionRequested")
        self.assertLess(permission_block.index("isTrustedWebUiOrigin( origin )"), permission_block.index("PermissionGrantedByUser"))
        self.assertLess(permission_block.index("feature == clipboardReadWriteFeature"), permission_block.index("PermissionGrantedByUser"))
        self.assertLess(permission_block.index("else"), permission_block.index("PermissionDeniedByUser"))

    def test_dock_accepts_download_and_file_access_requests(self):
        dock_cpp = (self.plugin_root / "qcopilots_container_dock.cpp").read_text(encoding="utf-8")
        dock_h = (self.plugin_root / "qcopilots_container_dock.h").read_text(encoding="utf-8")
        download_block = self.source_block(dock_cpp, "void QgsQCopilotsDock::handleDownloadRequested")
        file_access_block = self.source_block(dock_cpp, "void QgsQCopilotsDock::handleFileSystemAccessRequested")

        self.assertIn("QWebEngineDownloadRequest", dock_cpp)
        self.assertIn("downloadRequested", dock_cpp)
        self.assertIn("handleDownloadRequested", dock_h)
        self.assertIn("QFileDialog::getSaveFileName", dock_cpp)
        self.assertIn("setDownloadDirectory", dock_cpp)
        self.assertIn("setDownloadFileName", dock_cpp)
        self.assertLess(download_block.index("download->page() != mWebView->page()"), download_block.index("QFileDialog::getSaveFileName"))
        self.assertNotIn("download->page() &&", download_block)
        self.assertLess(download_block.index("filePath.isEmpty()"), download_block.index("download->cancel()"))
        cancel_index = download_block.index("download->cancel()")
        self.assertGreater(download_block.index("return;", cancel_index), cancel_index)
        self.assertLess(download_block.index("setDownloadDirectory"), download_block.index("download->accept()"))
        self.assertIn("downloadPointer->deleteLater()", download_block)
        self.assertIn("QWebEngineFileSystemAccessRequest", dock_cpp)
        self.assertIn("fileSystemAccessRequested", dock_cpp)
        self.assertIn("handleFileSystemAccessRequested", dock_h)
        self.assertLess(file_access_block.index("isTrustedWebUiOrigin( request.origin() )"), file_access_block.index("request.accept()"))
        self.assertLess(file_access_block.index("request.handleType()"), file_access_block.index("request.accept()"))
        self.assertLess(file_access_block.index("request.accessFlags()"), file_access_block.index("request.accept()"))
        self.assertIn("QWebEngineFileSystemAccessRequest::File", file_access_block)
        self.assertIn("QWebEngineFileSystemAccessRequest::Write", file_access_block)
        self.assertLess(file_access_block.index("else"), file_access_block.index("request.reject()"))

    def test_native_mcp_catalog_validation_is_strict_and_generation_is_monotonic(self):
        catalog_h = (self.plugin_root / "qcopilots_mcp_catalog.h").read_text(encoding="utf-8")
        catalog_cpp = (self.plugin_root / "qcopilots_mcp_catalog.cpp").read_text(encoding="utf-8")

        self.assertIn('"qcopilotsMcpRuntimeCatalog"', catalog_cpp)
        self.assertIn("QEvent::DynamicPropertyChange", catalog_cpp)
        self.assertIn("propertyEvent->propertyName() == sCatalogPropertyName", catalog_cpp)
        self.assertIn("property.metaType().id() != QMetaType::QString", catalog_cpp)
        self.assertIn("hasExactKeys( root", catalog_cpp)
        self.assertIn("hasExactKeys( object", catalog_cpp)
        self.assertIn('QStringLiteral( "starting" )', catalog_cpp)
        self.assertIn('QStringLiteral( "running" )', catalog_cpp)
        self.assertIn('QStringLiteral( "stopped" )', catalog_cpp)
        self.assertIn('QStringLiteral( "failed" )', catalog_cpp)
        self.assertIn('url.host() == QLatin1String( "qcopilots.localmachine" )', catalog_cpp)
        self.assertIn('url.host() == QLatin1String( "127.0.0.1" )', catalog_cpp)
        self.assertIn('url.path( QUrl::FullyEncoded ) == QLatin1String( "/mcp" )', catalog_cpp)
        self.assertIn("targetUrlText.isEmpty()", catalog_cpp)
        self.assertIn("authToken.isEmpty()", catalog_cpp)
        self.assertIn("serviceIds.contains", catalog_cpp)
        self.assertIn("virtualUrls.contains", catalog_cpp)
        self.assertIn("generation < mHighestGeneration", catalog_cpp)
        self.assertIn("generation == mHighestGeneration", catalog_cpp)
        self.assertIn("mHighestGeneration = generation", catalog_cpp)
        self.assertIn("qint64 mHighestGeneration = -1", catalog_h)
        self.assertRegex(
            catalog_cpp,
            r'if \( service\.state == QLatin1String\( "running" \) \)\s+servicesByVirtualUrl\.insert',
        )

    def test_mcp_config_merge_preserves_unknown_entries_and_rejects_damage(self):
        bootstrap = (
            self.plugin_root / "web" / "qcopilots_mcp_bootstrap.js"
        ).read_text(encoding="utf-8")

        self.assertIn("const CONFIG_KEY = 'LlamaUi.config'", bootstrap)
        self.assertIn("typeof config.mcpServers !== 'string'", bootstrap)
        self.assertIn("existingServers = JSON.parse(nested)", bootstrap)
        self.assertIn("if (!Array.isArray(existingServers))", bootstrap)
        self.assertGreaterEqual(
            bootstrap.count("return { ok: false, changed: false, value: serialized }"),
            4,
        )
        self.assertIn("const preserved = existingServers.filter", bootstrap)
        self.assertIn("currentManagedIds.has(entry.id)", bootstrap)
        self.assertIn("const mergedServers = preserved.concat(managed)", bootstrap)
        self.assertIn("const changed = oldServersJson !== mergedServersJson", bootstrap)
        self.assertIn("const result = changed ? JSON.stringify(config) : serialized", bootstrap)
        managed_block = self.source_block(bootstrap, "const managed = catalog.services.map")
        self.assertIn("Object.assign({}, previous)", managed_block)
        self.assertIn("delete merged.headers", managed_block)
        self.assertIn("merged.id = service.id", managed_block)
        self.assertIn("merged.enabled = service.state === 'running'", managed_block)
        self.assertIn("merged.url = service.virtualUrl", managed_block)
        self.assertIn("merged.useProxy = false", managed_block)
        self.assertIn("AUTO_REGISTRATION_DISABLED_KEY", bootstrap)
        self.assertIn("disableAutoRegistrationForSession()", bootstrap)
        self.assertIn("Automatic registration is disabled for this session", bootstrap)

    def test_mcp_first_visit_seeds_complete_catalog_and_coordinates_reload(self):
        dock_cpp = (self.plugin_root / "qcopilots_container_dock.cpp").read_text(encoding="utf-8")
        bootstrap = (
            self.plugin_root / "web" / "qcopilots_mcp_bootstrap.js"
        ).read_text(encoding="utf-8")

        install_start = dock_cpp.index("void installQCopilotsMcpBootstrapScript")
        install_end = dock_cpp.index("void installQCopilotsWebFixupScript", install_start)
        install_block = dock_cpp[install_start:install_end]
        self.assertIn("QWebEngineScript::DocumentCreation", dock_cpp)
        self.assertIn("QWebEngineScript::MainWorld", dock_cpp)
        self.assertIn("script.setRunsOnSubFrames( true )", install_block)
        self.assertNotIn("script.setRunsOnSubFrames( false )", install_block)
        self.assertIn(':/qtwebchannel/qwebchannel.js', dock_cpp)
        self.assertIn("webChannelSource + QStringLiteral", install_block)
        self.assertIn("Storage.prototype.setItem = function", bootstrap)
        self.assertIn("let firstVisitAtDocumentCreation", bootstrap)
        self.assertIn("firstConfigWriteObserved", bootstrap)
        self.assertIn("const creatingConfig = existing === null", bootstrap)
        self.assertIn(
            "!currentCatalog.startupComplete || currentCatalog.services.length === 0",
            bootstrap,
        )
        self.assertIn(
            "mergeConfigValue(creatingConfig ? '{}' : existing, currentCatalog)",
            bootstrap,
        )
        self.assertIn("firstVisitAtDocumentCreation = false", bootstrap)
        self.assertIn("const CATALOG_WAIT_MS = 30000", bootstrap)
        self.assertIn("beginInitialMcpCatalogWait()", dock_cpp)
        self.assertIn("completeInitialMcpCatalogWait( true )", dock_cpp)
        self.assertIn("snapshot.startupComplete", dock_cpp)
        self.assertIn("installQCopilotsMcpBootstrapScript", dock_cpp)
        self.assertIn("mMcpBridge->catalog()", dock_cpp)
        self.assertIn("window.__qcopilotsInitialMcpCatalogV1", dock_cpp)
        self.assertIn("acceptCatalog(window.__qcopilotsInitialMcpCatalogV1, true)", bootstrap)
        self.assertIn(
            "catalogActivated || synchronousInitialCatalog || parsed.startupComplete",
            bootstrap,
        )
        self.assertIn("sessionStorage.getItem(FIRST_VISIT_RELOAD_KEY)", bootstrap)
        self.assertIn("sessionStorage.setItem(FIRST_VISIT_RELOAD_KEY, '1')", bootstrap)
        first_reload_block = self.source_block(bootstrap, "function scheduleFirstVisitReload")
        generation_reload_block = self.source_block(
            bootstrap, "function scheduleCatalogGenerationReload"
        )
        self.assertEqual(1, first_reload_block.count("window.location.reload()"))
        self.assertEqual(1, generation_reload_block.count("window.location.reload()"))
        self.assertIn("CATALOG_RELOAD_GENERATION_KEY", generation_reload_block)
        self.assertLess(
            bootstrap.index("firstConfigWriteObserved = true"),
            bootstrap.index("mergeConfigValue(normalizedValue, currentCatalog)"),
        )

    def test_mcp_fetch_interceptor_is_main_frame_and_exact_virtual_origin_only(self):
        dock_cpp = (self.plugin_root / "qcopilots_container_dock.cpp").read_text(encoding="utf-8")
        bootstrap = (
            self.plugin_root / "web" / "qcopilots_mcp_bootstrap.js"
        ).read_text(encoding="utf-8")

        self.assertIn("window.top !== window", bootstrap)
        self.assertIn("window.__qcopilotsConfiguredOriginV1", bootstrap)
        self.assertIn("effectiveOriginPort(window.location)", bootstrap)
        self.assertIn("qcopilotsCurrentPort === qcopilotsExpectedPort", dock_cpp)
        self.assertIn("const VIRTUAL_ORIGIN = 'https://qcopilots.localmachine'", bootstrap)
        self.assertIn("['starting', 'running', 'stopped', 'failed'].includes(service.state)", bootstrap)
        self.assertIn("/^\\/mcp\\/[^/]+$/", bootstrap)
        self.assertIn("if (!targetsVirtualMcpHost(url))", bootstrap)
        self.assertIn("if (!isVirtualMcpUrl(url))", bootstrap)
        self.assertIn("if (!isCurrentRunningVirtualMcpUrl(url))", bootstrap)
        self.assertIn("function installBlockOnlyFetch()", bootstrap)
        self.assertIn("service.virtualUrl === url.href", bootstrap)
        self.assertIn("return rejectVirtualMcpRequest", bootstrap)
        self.assertIn("return originalFetch(input, init)", bootstrap)
        self.assertIn("method !== 'GET' && method !== 'POST' && method !== 'DELETE'", bootstrap)
        self.assertIn("bridge.request(requestId, method, request.url", bootstrap)
        self.assertIn("bridge.cancel(requestId)", bootstrap)
        self.assertIn("new Response", bootstrap)
        self.assertIn("new Headers(headers || {})", bootstrap)
        self.assertIn("base64Bytes(bodyBase64)", bootstrap)

    def test_native_mcp_bridge_enforces_origin_redirect_header_and_body_policies(self):
        bridge_h = (self.plugin_root / "qcopilots_mcp_bridge.h").read_text(encoding="utf-8")
        bridge_cpp = (self.plugin_root / "qcopilots_mcp_bridge.cpp").read_text(encoding="utf-8")
        bootstrap = (
            self.plugin_root / "web" / "qcopilots_mcp_bootstrap.js"
        ).read_text(encoding="utf-8")

        self.assertIn("Q_INVOKABLE void request", bridge_h)
        self.assertIn("Q_INVOKABLE void cancel", bridge_h)
        self.assertIn("mNetworkAccessManager->setProxy( QNetworkProxy::NoProxy )", bridge_cpp)
        self.assertIn("sameOrigin( mPage->url(), mConfiguredUrl )", bridge_cpp)
        self.assertIn("QWebEnginePage::loadStarted", bridge_cpp)
        self.assertIn('QStringLiteral( "navigation_started" )', bridge_cpp)
        self.assertIn("QWebEnginePage::urlChanged", bridge_cpp)
        self.assertGreaterEqual(bridge_cpp.count("!trustedTopLevelOrigin()"), 2)
        self.assertIn("QNetworkRequest::ManualRedirectPolicy", bridge_cpp)
        for status in (301, 302, 303, 305, 307, 308):
            self.assertIn(f"case {status}:", bridge_cpp)
        self.assertNotIn("status >= 300 && status < 400", bridge_cpp)
        self.assertIn("isRedirectStatus( status )", bridge_cpp)
        self.assertIn("redirect_not_allowed", bridge_cpp)
        self.assertIn("mNetworkAccessManager->get", bridge_cpp)
        self.assertIn("mNetworkAccessManager->post", bridge_cpp)
        self.assertIn("mNetworkAccessManager->deleteResource", bridge_cpp)
        self.assertIn("constexpr qsizetype sMaximumRequestBodyBytes = 1024 * 1024", bridge_cpp)
        self.assertIn("constexpr qsizetype sMaximumResponseBodyBytes = 16 * 1024 * 1024", bridge_cpp)
        self.assertIn("allowedRequestHeaders()", bridge_cpp)
        self.assertIn("allowedResponseHeaders()", bridge_cpp)
        self.assertIn('headerName == QByteArrayLiteral( "authorization" )', bridge_cpp)
        self.assertIn('QByteArrayLiteral( "Bearer " ) + service.authToken', bridge_cpp)
        self.assertIn("reply->error() != QNetworkReply::NoError && status <= 0", bridge_cpp)
        self.assertIn("mLastPublicGeneration = snapshot.generation", bridge_cpp)
        self.assertIn("!snapshot.valid && !mPublicCatalogJson.isEmpty()", bridge_cpp)
        self.assertIn("pending->reply->abort()", bridge_cpp)
        self.assertIn("QTimer::timeout", bridge_cpp)
        self.assertIn("constexpr int sMaximumTimeoutMs = 300000", bridge_cpp)
        self.assertIn("const REQUEST_TIMEOUT_MS = 300000", bootstrap)
        self.assertIn("function readRequestBody", bootstrap)
        self.assertIn("reader.read().then", bootstrap)
        self.assertIn("totalBytes > MAX_BODY_BYTES", bootstrap)
        self.assertIn("cancelBodyReader(reader)", bootstrap)
        self.assertIn("remainingRequestTimeout(deadline)", bootstrap)
        self.assertIn("request.signal.addEventListener('abort'", bootstrap)
        self.assertGreaterEqual(bootstrap.count("request.signal.aborted"), 3)
        wait_for_bridge_block = self.source_block(bootstrap, "function waitForBridge")
        self.assertIn("signal.addEventListener('abort'", wait_for_bridge_block)
        self.assertIn("bridgeReady.then", wait_for_bridge_block)
        self.assertNotIn("bridge.request", wait_for_bridge_block)
        self.assertNotIn("bridge.cancel", wait_for_bridge_block)
        native_fetch_block = self.source_block(bootstrap, "async function nativeMcpFetch")
        abort_block = self.source_block(native_fetch_block, "const abortHandler = function")
        self.assertIn("settlePending(requestId", abort_block)
        listener_index = native_fetch_block.index("request.signal.addEventListener('abort'")
        final_abort_index = native_fetch_block.index("if (request.signal.aborted)", listener_index)
        request_index = native_fetch_block.index("bridge.request(requestId", final_abort_index)
        self.assertLess(listener_index, final_abort_index)
        self.assertLess(final_abort_index, request_index)
        self.assertIn("pending.signal && pending.signal.aborted", bootstrap)
        self.assertIn("if (name === 'authorization')", bootstrap)
        self.assertIn("parsed.generation <= currentCatalog.generation", bootstrap)
        self.assertIn("bridgeReady.catch(function () {})", bootstrap)
        self.assertIn(
            "!currentCatalog.startupComplete && currentCatalog.services.length === 0",
            bootstrap,
        )

    def test_bootstrap_runtime_with_webchannel_mock(self):
        node = shutil.which("node")
        if node is None:
            self.skipTest("Node.js is unavailable in the configured test environment")
        bootstrap_path = self.plugin_root / "web" / "qcopilots_mcp_bootstrap.js"
        harness = textwrap.dedent(
            r"""
            'use strict';

            const assert = require('assert');
            const fs = require('fs');
            const vm = require('vm');
            const { webcrypto } = require('crypto');
            const bootstrap = fs.readFileSync(process.argv[2], 'utf8');

            class Signal {
              constructor() { this.handlers = []; }
              connect(handler) { this.handlers.push(handler); }
              emit(...args) { this.handlers.slice().forEach((handler) => handler(...args)); }
            }

            function makeEnvironment(initialConfig, catalog, options = {}) {
              class MockStorage {
                constructor() { this.values = new Map(); }
                getItem(key) {
                  const normalized = String(key);
                  return this.values.has(normalized) ? this.values.get(normalized) : null;
                }
                setItem(key, value) { this.values.set(String(key), String(value)); }
                removeItem(key) { this.values.delete(String(key)); }
              }
              class MockEvent {
                constructor(type, init = {}) { this.type = type; Object.assign(this, init); }
              }

              const localStorage = new MockStorage();
              const sessionStorage = new MockStorage();
              if (initialConfig !== null) {
                localStorage.setItem('LlamaUi.config', initialConfig);
              }
              const requests = [];
              const cancels = [];
              const originalFetchCalls = [];
              const reloads = [];
              const dispatchedEvents = [];
              const response = new Signal();
              const requestFailed = new Signal();
              const catalogChanged = new Signal();
              let deliverWebChannel = null;
              const bridge = {
                response,
                requestFailed,
                catalogChanged,
                catalog(callback) { callback(JSON.stringify(catalog)); },
                request(requestId, method, url, headers, body, timeoutMs) {
                  requests.push({ requestId, method, url, headers, body, timeoutMs });
                  if (method === 'GET') {
                    return;
                  }
                  const isHttpError = body === 'http-error';
                  const status = isHttpError ? 409 : 200;
                  const payload = isHttpError ? '{"error":"conflict"}' : '{"ok":true}';
                  setImmediate(() => response.emit(
                    requestId,
                    status,
                    isHttpError ? 'Conflict' : 'OK',
                    {'content-type': 'application/json', 'mcp-session-id': 'native-session'},
                    Buffer.from(payload).toString('base64')
                  ));
                },
                cancel(requestId) { cancels.push(requestId); }
              };

              const configuredOrigin = options.configuredOrigin || 'https://llama.example/';
              const locationUrl = new URL(options.locationOrigin || configuredOrigin);
              const location = {
                protocol: locationUrl.protocol,
                hostname: locationUrl.hostname,
                port: locationUrl.port,
                href: locationUrl.href,
                reload() { reloads.push('reload'); }
              };
              const window = {
                __qcopilotsConfiguredOriginV1: options.withoutConfiguredOrigin
                  ? undefined
                  : configuredOrigin,
                __qcopilotsInitialMcpCatalogV1: JSON.stringify(catalog),
                location,
                crypto: webcrypto,
                atob(value) { return Buffer.from(value, 'base64').toString('binary'); },
                fetch: async function (input, init) {
                  originalFetchCalls.push({input: String(input), init});
                  return new Response('remote', {status: 200});
                },
                dispatchEvent(event) { dispatchedEvents.push(event); return true; },
                setTimeout(callback, delay) {
                  if (delay === 0) { callback(); }
                  if (options.expireRequestTimeout && delay > 60000) {
                    return {immediate: setImmediate(callback)};
                  }
                  return 1;
                },
                clearTimeout(timerId) {
                  if (timerId && timerId.immediate) {
                    clearImmediate(timerId.immediate);
                  }
                },
                qt: options.withoutWebChannel ? undefined : {webChannelTransport: {}},
                QWebChannel: options.withoutWebChannel ? undefined : function (_transport, callback) {
                  const deliver = function () {
                    callback({objects: {qcopilotsMcpBridge: bridge}});
                  };
                  if (options.deferWebChannel) {
                    deliverWebChannel = deliver;
                  } else {
                    deliver();
                  }
                }
              };
              window.top = options.iframe ? {} : window;
              const document = {baseURI: location.href};
              const context = {
                window,
                document,
                localStorage,
                sessionStorage,
                Storage: MockStorage,
                StorageEvent: MockEvent,
                Event: MockEvent,
                URL,
                Request,
                Response,
                Headers,
                DOMException,
                TextEncoder,
                TextDecoder,
                Uint8Array,
                ReadableStream,
                Set,
                Map,
                Promise,
                JSON,
                Number,
                Object,
                Array,
                String,
                Date,
                TypeError,
                Error,
                console
              };
              vm.createContext(context);
              vm.runInContext(bootstrap, context, {filename: 'qcopilots_mcp_bootstrap.js'});
              return {
                window,
                localStorage,
                sessionStorage,
                bridge,
                requests,
                cancels,
                originalFetchCalls,
                reloads,
                dispatchedEvents,
                connectWebChannel() {
                  if (!deliverWebChannel) {
                    return;
                  }
                  const deliver = deliverWebChannel;
                  deliverWebChannel = null;
                  deliver();
                }
              };
            }

            function storedServers(environment) {
              const config = JSON.parse(environment.localStorage.getItem('LlamaUi.config'));
              return {config, servers: JSON.parse(config.mcpServers)};
            }

            async function main() {
              const serviceId = 'qcopilots.mcp_server_builtin_tools';
              const virtualUrl = 'https://qcopilots.localmachine/mcp/' + serviceId;
              const legacyVirtualUrl = 'https://legacy.example/mcp/' + serviceId;
              const catalog = {
                schemaVersion: 1,
                generation: 1,
                startupComplete: true,
                services: [{
                  id: serviceId,
                  displayName: 'QCopilots Builtin Tools',
                  state: 'running',
                  virtualUrl
                }]
              };
              function configWithManagedUrl(managedUrl) {
                return JSON.stringify({
                  theme: 'dark',
                  unknownOuter: {preserve: true},
                  mcpServers: JSON.stringify([
                    {id: 'external.server', url: 'https://external.example/mcp', enabled: true, custom: 7},
                    {id: serviceId, url: managedUrl, enabled: false, useProxy: true,
                     headers: {Authorization: 'must-not-survive'}, customManaged: 'preserve'}
                  ])
                });
              }
              const initialConfig = configWithManagedUrl(virtualUrl);
              const legacyInitialConfig = configWithManagedUrl(legacyVirtualUrl);
              const environment = makeEnvironment(legacyInitialConfig, catalog);
              let stored = storedServers(environment);
              assert.strictEqual(stored.config.theme, 'dark');
              assert.deepStrictEqual(stored.config.unknownOuter, {preserve: true});
              assert.strictEqual(stored.servers.length, 2);
              const external = stored.servers.find((entry) => entry.id === 'external.server');
              const managedEntries = stored.servers.filter((entry) => entry.id === serviceId);
              assert.strictEqual(managedEntries.length, 1);
              const managed = managedEntries[0];
              assert.strictEqual(external.custom, 7);
              assert.strictEqual(managed.enabled, true);
              assert.strictEqual(managed.url, virtualUrl);
              assert.strictEqual(managed.useProxy, false);
              assert.strictEqual(managed.customManaged, 'preserve');
              assert.strictEqual(Object.hasOwn(managed, 'headers'), false);
              assert.strictEqual(stored.servers.some((entry) => entry.url === legacyVirtualUrl), false);
              assert.strictEqual(environment.window.__qcopilotsMcpBlockOnlyInstalled, undefined);
              assert.strictEqual(environment.reloads.length, 0);

              const response = await environment.window.fetch(virtualUrl, {
                method: 'POST',
                headers: {
                  'Content-Type': 'application/json',
                  'Authorization': 'Bearer page-secret',
                  'MCP-Session-Id': 'browser-session'
                },
                body: '{"jsonrpc":"2.0"}'
              });
              assert.strictEqual(response.status, 200);
              assert.deepStrictEqual(await response.json(), {ok: true});
              assert.strictEqual(response.headers.get('mcp-session-id'), 'native-session');
              const nativeRequest = environment.requests[0];
              assert.strictEqual(nativeRequest.method, 'POST');
              assert.strictEqual(nativeRequest.url, virtualUrl);
              assert.strictEqual(nativeRequest.headers.authorization, undefined);
              assert.strictEqual(nativeRequest.headers['content-type'], 'application/json');
              assert.strictEqual(nativeRequest.headers['mcp-session-id'], 'browser-session');
              assert.strictEqual(nativeRequest.body, '{"jsonrpc":"2.0"}');
              assert.ok(nativeRequest.timeoutMs > 0 && nativeRequest.timeoutMs <= 300000);

              const bridgeRequestsBeforeInvalidUrls = environment.requests.length;
              const originalFetchesBeforeInvalidUrls = environment.originalFetchCalls.length;
              const rejectedVirtualUrls = [
                'https://qcopilots.localmachine/mcp/qcopilots.unknown',
                virtualUrl + '?unexpected=1',
                virtualUrl + '/extra',
                'https://qcopilots.localmachine/mcp/' + serviceId.replace('.', '%2E'),
                'http://qcopilots.localmachine/mcp/' + serviceId,
                'https://qcopilots.localmachine:444/mcp/' + serviceId,
                'https://qcopilots.localmachine./mcp/' + serviceId,
                'https://user@qcopilots.localmachine/mcp/' + serviceId
              ];
              for (const rejectedUrl of rejectedVirtualUrls) {
                await assert.rejects(
                  environment.window.fetch(rejectedUrl),
                  /local MCP virtual URL/
                );
              }
              assert.strictEqual(environment.requests.length, bridgeRequestsBeforeInvalidUrls);
              assert.strictEqual(
                environment.originalFetchCalls.length,
                originalFetchesBeforeInvalidUrls
              );

              const conflict = await environment.window.fetch(virtualUrl, {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: 'http-error'
              });
              assert.strictEqual(conflict.status, 409);
              assert.deepStrictEqual(await conflict.json(), {error: 'conflict'});
              await assert.rejects(
                environment.window.fetch(virtualUrl, {headers: {'X-Forbidden': '1'}}),
                /Unsupported local MCP request header/
              );
              await assert.rejects(
                environment.window.fetch(virtualUrl, {
                  method: 'POST',
                  headers: {'Content-Type': 'text/plain'},
                  body: 'x'.repeat(1024 * 1024 + 1)
                }),
                /exceeds the 1 MiB limit/
              );

              const abortController = new AbortController();
              const canceledFetch = environment.window.fetch(virtualUrl, {
                method: 'GET',
                signal: abortController.signal
              });
              while (!environment.requests.some((request) => request.method === 'GET')) {
                await new Promise((resolve) => setImmediate(resolve));
              }
              abortController.abort();
              await assert.rejects(canceledFetch, (error) => error.name === 'AbortError');
              const getRequest = environment.requests.find((request) => request.method === 'GET');
              assert.ok(environment.cancels.includes(getRequest.requestId));

              const delayedBridge = makeEnvironment(initialConfig, catalog, {deferWebChannel: true});
              const alreadyAbortedController = new AbortController();
              alreadyAbortedController.abort();
              await assert.rejects(
                delayedBridge.window.fetch(virtualUrl, {
                  method: 'GET',
                  signal: alreadyAbortedController.signal
                }),
                (error) => error.name === 'AbortError'
              );
              assert.strictEqual(delayedBridge.requests.length, 0);
              assert.strictEqual(delayedBridge.cancels.length, 0);

              const waitingController = new AbortController();
              const waitingForBridge = delayedBridge.window.fetch(virtualUrl, {
                method: 'GET',
                signal: waitingController.signal
              });
              await new Promise((resolve) => setImmediate(resolve));
              waitingController.abort();
              await assert.rejects(
                waitingForBridge,
                (error) => error.name === 'AbortError'
              );
              assert.strictEqual(delayedBridge.requests.length, 0);
              assert.strictEqual(delayedBridge.cancels.length, 0);
              delayedBridge.connectWebChannel();
              await new Promise((resolve) => setImmediate(resolve));
              assert.strictEqual(delayedBridge.requests.length, 0);
              assert.strictEqual(delayedBridge.cancels.length, 0);

              let abortedBodyCanceled = false;
              const abortBodyEnvironment = makeEnvironment(initialConfig, catalog);
              const hangingBody = new ReadableStream({
                start(controller) {
                  controller.enqueue(new TextEncoder().encode('{"jsonrpc":'));
                },
                cancel() { abortedBodyCanceled = true; }
              });
              const bodyAbortController = new AbortController();
              const readingBody = abortBodyEnvironment.window.fetch(virtualUrl, {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: hangingBody,
                duplex: 'half',
                signal: bodyAbortController.signal
              });
              await new Promise((resolve) => setImmediate(resolve));
              bodyAbortController.abort();
              await assert.rejects(readingBody, (error) => error.name === 'AbortError');
              await new Promise((resolve) => setImmediate(resolve));
              assert.strictEqual(abortedBodyCanceled, true);
              assert.strictEqual(abortBodyEnvironment.requests.length, 0);
              assert.strictEqual(abortBodyEnvironment.cancels.length, 0);

              let oversizedBodyCanceled = false;
              const oversizedBodyEnvironment = makeEnvironment(initialConfig, catalog);
              const oversizedBody = new ReadableStream({
                start(controller) {
                  controller.enqueue(new Uint8Array(1024 * 1024));
                  controller.enqueue(new Uint8Array(1));
                },
                cancel() { oversizedBodyCanceled = true; }
              });
              await assert.rejects(
                oversizedBodyEnvironment.window.fetch(virtualUrl, {
                  method: 'POST',
                  headers: {'Content-Type': 'application/json'},
                  body: oversizedBody,
                  duplex: 'half'
                }),
                /exceeds the 1 MiB limit/
              );
              await new Promise((resolve) => setImmediate(resolve));
              assert.strictEqual(oversizedBodyCanceled, true);
              assert.strictEqual(oversizedBodyEnvironment.requests.length, 0);
              assert.strictEqual(oversizedBodyEnvironment.cancels.length, 0);

              let timedOutBodyCanceled = false;
              const bodyTimeoutEnvironment = makeEnvironment(
                initialConfig,
                catalog,
                {expireRequestTimeout: true}
              );
              const timedOutBody = new ReadableStream({
                start(controller) {
                  controller.enqueue(new TextEncoder().encode('{"jsonrpc":'));
                },
                cancel() { timedOutBodyCanceled = true; }
              });
              await assert.rejects(
                bodyTimeoutEnvironment.window.fetch(virtualUrl, {
                  method: 'POST',
                  headers: {'Content-Type': 'application/json'},
                  body: timedOutBody,
                  duplex: 'half'
                }),
                /timed out/
              );
              await new Promise((resolve) => setImmediate(resolve));
              assert.strictEqual(timedOutBodyCanceled, true);
              assert.strictEqual(bodyTimeoutEnvironment.requests.length, 0);
              assert.strictEqual(bodyTimeoutEnvironment.cancels.length, 0);

              await environment.window.fetch('https://llama.example/api/models');
              assert.strictEqual(environment.originalFetchCalls.length, 1);

              const stoppedCatalog = {
                schemaVersion: 1,
                generation: 2,
                startupComplete: true,
                services: [{
                  id: serviceId,
                  displayName: 'QCopilots Builtin Tools',
                  state: 'stopped',
                  virtualUrl
                }]
              };
              environment.bridge.catalogChanged.emit(JSON.stringify(stoppedCatalog));
              stored = storedServers(environment);
              assert.strictEqual(stored.servers.find((entry) => entry.id === serviceId).enabled, false);
              assert.strictEqual(environment.reloads.length, 1);
              assert.strictEqual(
                environment.sessionStorage.getItem('QCopilots.mcpReloadedGenerationV1'),
                '2'
              );
              const bridgeRequestsBeforeStoppedFetch = environment.requests.length;
              const originalFetchesBeforeStoppedFetch = environment.originalFetchCalls.length;
              await assert.rejects(
                environment.window.fetch(virtualUrl),
                /not present in the current running catalog/
              );
              assert.strictEqual(environment.requests.length, bridgeRequestsBeforeStoppedFetch);
              assert.strictEqual(
                environment.originalFetchCalls.length,
                originalFetchesBeforeStoppedFetch
              );
              const conflictingSameGenerationCatalog = {
                ...stoppedCatalog,
                services: [{...stoppedCatalog.services[0], state: 'running'}]
              };
              environment.bridge.catalogChanged.emit(JSON.stringify(conflictingSameGenerationCatalog));
              assert.strictEqual(storedServers(environment).servers.find((entry) => entry.id === serviceId).enabled, false);
              assert.strictEqual(environment.reloads.length, 1);
              environment.bridge.catalogChanged.emit(JSON.stringify(catalog));
              assert.strictEqual(storedServers(environment).servers.find((entry) => entry.id === serviceId).enabled, false);
              assert.strictEqual(environment.reloads.length, 1);

              const firstVisit = makeEnvironment(null, catalog);
              const firstSeeded = storedServers(firstVisit);
              assert.strictEqual(firstSeeded.servers.length, 1);
              assert.strictEqual(firstSeeded.servers[0].id, serviceId);
              assert.strictEqual(firstSeeded.servers[0].enabled, true);
              assert.strictEqual(firstVisit.reloads.length, 0);
              assert.deepStrictEqual(
                JSON.parse(
                  firstVisit.localStorage.getItem('QCopilots.managedMcpServerIdsV1')
                ),
                [serviceId]
              );

              const routerDefaults = {
                theme: 'system',
                routerDefault: {temperature: 0.7}
              };
              firstVisit.localStorage.setItem(
                'LlamaUi.config',
                JSON.stringify(routerDefaults)
              );
              let firstStored = storedServers(firstVisit);
              assert.strictEqual(firstStored.config.theme, 'system');
              assert.deepStrictEqual(
                firstStored.config.routerDefault,
                {temperature: 0.7}
              );
              assert.strictEqual(firstStored.servers.length, 1);
              assert.strictEqual(firstStored.servers[0].id, serviceId);
              assert.strictEqual(firstStored.servers[0].enabled, true);
              assert.strictEqual(firstVisit.reloads.length, 0);
              firstVisit.localStorage.setItem(
                'LlamaUi.config',
                JSON.stringify(routerDefaults)
              );
              firstStored = storedServers(firstVisit);
              assert.strictEqual(firstStored.servers.length, 1);
              assert.strictEqual(firstVisit.reloads.length, 0);
              assert.strictEqual(
                firstVisit.sessionStorage.getItem('QCopilots.mcpFirstVisitReloadedV1'),
                null
              );

              const firstVisitStopped = makeEnvironment(null, stoppedCatalog);
              const firstStopped = storedServers(firstVisitStopped);
              assert.strictEqual(firstStopped.servers.length, 1);
              assert.strictEqual(firstStopped.servers[0].enabled, false);
              assert.strictEqual(firstVisitStopped.reloads.length, 0);

              const corrupt = makeEnvironment('{broken-json', catalog);
              assert.strictEqual(corrupt.localStorage.getItem('LlamaUi.config'), '{broken-json');
              assert.strictEqual(
                corrupt.sessionStorage.getItem('QCopilots.mcpAutoRegistrationDisabledV1'),
                '1'
              );
              corrupt.bridge.catalogChanged.emit(JSON.stringify(stoppedCatalog));
              assert.strictEqual(corrupt.localStorage.getItem('LlamaUi.config'), '{broken-json');
              assert.strictEqual(corrupt.reloads.length, 0);

              const incompleteCatalog = {
                schemaVersion: 1,
                generation: 1,
                startupComplete: false,
                services: []
              };
              const emptyCompleteCatalog = {
                ...incompleteCatalog,
                startupComplete: true
              };
              const emptyFirstVisit = makeEnvironment(null, emptyCompleteCatalog);
              assert.strictEqual(
                emptyFirstVisit.localStorage.getItem('LlamaUi.config'),
                null
              );
              assert.strictEqual(
                emptyFirstVisit.localStorage.getItem('QCopilots.managedMcpServerIdsV1'),
                null
              );
              assert.strictEqual(emptyFirstVisit.reloads.length, 0);

              const incomplete = makeEnvironment(initialConfig, incompleteCatalog);
              assert.strictEqual(incomplete.localStorage.getItem('LlamaUi.config'), initialConfig);
              assert.strictEqual(incomplete.reloads.length, 0);
              incomplete.bridge.catalogChanged.emit(JSON.stringify({
                ...incompleteCatalog,
                generation: 2,
                startupComplete: true
              }));
              const completedEmpty = storedServers(incomplete);
              assert.deepStrictEqual(
                completedEmpty.servers.map((entry) => entry.id),
                ['external.server']
              );
              assert.strictEqual(incomplete.reloads.length, 1);

              const incompleteFirstVisit = makeEnvironment(null, incompleteCatalog);
              assert.strictEqual(incompleteFirstVisit.localStorage.getItem('LlamaUi.config'), null);
              assert.strictEqual(incompleteFirstVisit.reloads.length, 0);
              incompleteFirstVisit.bridge.catalogChanged.emit(JSON.stringify({
                ...catalog,
                generation: 2
              }));
              const dynamicallySeeded = storedServers(incompleteFirstVisit);
              assert.strictEqual(dynamicallySeeded.servers.length, 1);
              assert.strictEqual(dynamicallySeeded.servers[0].enabled, true);
              assert.strictEqual(incompleteFirstVisit.reloads.length, 1);
              assert.strictEqual(
                incompleteFirstVisit.sessionStorage.getItem(
                  'QCopilots.mcpReloadedGenerationV1'
                ),
                '2'
              );
              incompleteFirstVisit.bridge.catalogChanged.emit(JSON.stringify({
                ...stoppedCatalog,
                generation: 2
              }));
              assert.strictEqual(
                storedServers(incompleteFirstVisit).servers[0].enabled,
                true
              );
              assert.strictEqual(incompleteFirstVisit.reloads.length, 1);

              const incompleteAfterPageWrite = makeEnvironment(null, incompleteCatalog);
              incompleteAfterPageWrite.localStorage.setItem(
                'LlamaUi.config',
                JSON.stringify({theme: 'system', routerDefault: {temperature: 0.5}})
              );
              assert.strictEqual(incompleteAfterPageWrite.reloads.length, 0);
              incompleteAfterPageWrite.bridge.catalogChanged.emit(JSON.stringify({
                ...catalog,
                generation: 2
              }));
              const mergedAfterPageWrite = storedServers(incompleteAfterPageWrite);
              assert.strictEqual(mergedAfterPageWrite.config.theme, 'system');
              assert.deepStrictEqual(
                mergedAfterPageWrite.config.routerDefault,
                {temperature: 0.5}
              );
              assert.strictEqual(mergedAfterPageWrite.servers.length, 1);
              assert.strictEqual(mergedAfterPageWrite.servers[0].enabled, true);
              assert.strictEqual(incompleteAfterPageWrite.reloads.length, 1);
              assert.strictEqual(
                incompleteAfterPageWrite.sessionStorage.getItem(
                  'QCopilots.mcpFirstVisitReloadedV1'
                ),
                '1'
              );
              assert.strictEqual(
                incompleteAfterPageWrite.sessionStorage.getItem(
                  'QCopilots.mcpReloadedGenerationV1'
                ),
                null
              );
              incompleteAfterPageWrite.bridge.catalogChanged.emit(JSON.stringify({
                ...stoppedCatalog,
                generation: 2
              }));
              assert.strictEqual(incompleteAfterPageWrite.reloads.length, 1);

              const unhandledRejections = [];
              const unhandledHandler = (reason) => unhandledRejections.push(reason);
              process.on('unhandledRejection', unhandledHandler);
              const noWebChannel = makeEnvironment(initialConfig, catalog, {withoutWebChannel: true});
              await assert.rejects(
                noWebChannel.window.fetch(virtualUrl),
                /Qt WebChannel is unavailable/
              );
              await new Promise((resolve) => setImmediate(resolve));
              process.off('unhandledRejection', unhandledHandler);
              assert.deepStrictEqual(unhandledRejections, []);

              const iframe = makeEnvironment(null, catalog, {iframe: true});
              assert.strictEqual(iframe.window.__qcopilotsNativeMcpInstalled, undefined);
              assert.strictEqual(iframe.window.__qcopilotsMcpBlockOnlyInstalled, true);
              assert.strictEqual(iframe.localStorage.getItem('LlamaUi.config'), null);
              await assert.rejects(
                iframe.window.fetch(virtualUrl),
                /virtual host is unavailable/
              );
              await assert.rejects(
                iframe.window.fetch('https://qcopilots.localmachine./mcp/' + serviceId),
                /virtual host is unavailable/
              );
              assert.strictEqual(iframe.originalFetchCalls.length, 0);
              await iframe.window.fetch('https://external.example/mcp');
              assert.strictEqual(iframe.originalFetchCalls.length, 1);

              const redirected = makeEnvironment(null, catalog, {
                configuredOrigin: 'https://llama.example/',
                locationOrigin: 'https://redirected.example/'
              });
              assert.strictEqual(redirected.window.__qcopilotsNativeMcpInstalled, undefined);
              assert.strictEqual(redirected.window.__qcopilotsMcpBlockOnlyInstalled, true);
              assert.strictEqual(redirected.localStorage.getItem('LlamaUi.config'), null);
              await assert.rejects(
                redirected.window.fetch(virtualUrl),
                /virtual host is unavailable/
              );
              assert.strictEqual(redirected.originalFetchCalls.length, 0);
              await redirected.window.fetch('https://external.example/mcp');
              assert.strictEqual(redirected.originalFetchCalls.length, 1);

              const unconfigured = makeEnvironment(null, catalog, {
                withoutConfiguredOrigin: true
              });
              assert.strictEqual(unconfigured.window.__qcopilotsNativeMcpInstalled, undefined);
              assert.strictEqual(unconfigured.window.__qcopilotsMcpBlockOnlyInstalled, true);
              assert.strictEqual(unconfigured.localStorage.getItem('LlamaUi.config'), null);
              await assert.rejects(
                unconfigured.window.fetch(virtualUrl),
                /virtual host is unavailable/
              );
              assert.strictEqual(unconfigured.originalFetchCalls.length, 0);
              await unconfigured.window.fetch('https://external.example/mcp');
              assert.strictEqual(unconfigured.originalFetchCalls.length, 1);
            }

            main().catch((error) => {
              console.error(error && error.stack ? error.stack : error);
              process.exitCode = 1;
            });
            """
        )
        temporary_path = None
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_path = Path(temporary_directory)
            harness_path = temporary_path / "bootstrap_runtime_test.js"
            harness_path.write_text(harness, encoding="utf-8")
            completed = subprocess.run(
                [node, str(harness_path), str(bootstrap_path)],
                capture_output=True,
                text=True,
                timeout=20,
                check=False,
            )
            self.assertEqual(
                completed.returncode,
                0,
                f"Node bootstrap runtime test failed:\n{completed.stdout}\n{completed.stderr}",
            )
        self.assertIsNotNone(temporary_path)
        self.assertFalse(temporary_path.exists(), "Bootstrap mock directory was not removed")


if __name__ == "__main__":
    unittest.main()
