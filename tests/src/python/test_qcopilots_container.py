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
        self.assertIn("find_package(${QT_VERSION_BASE} COMPONENTS WebEngineWidgets REQUIRED)", cmake)
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


if __name__ == "__main__":
    unittest.main()
