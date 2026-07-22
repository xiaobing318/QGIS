"""Shared top-level QCopilots menu helpers for Python plugins.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations


MENU_OBJECT_NAME = "mQCopilotsMenu"
MENU_CREATED_PROPERTY = "qcopilotsCreatedByPython"
MENU_SHARED_CREATED_PROPERTY = "qcopilotsCreatedByPlugin"


def add_qcopilots_menu_action(iface, action) -> None:
    menu = qcopilots_menu(iface)
    menu.addAction(action)


def remove_qcopilots_menu_action(iface, action) -> None:
    menu = _find_qcopilots_menu(iface)
    if not menu:
        return
    menu.removeAction(action)
    if menu.actions() or not _qcopilots_created_menu(menu):
        return
    menu_bar = iface.mainWindow().menuBar()
    menu_bar.removeAction(menu.menuAction())
    menu.deleteLater()


def qcopilots_menu(iface):
    menu = _find_qcopilots_menu(iface)
    if menu:
        return menu

    from qgis.PyQt.QtWidgets import QMenu

    main_window = iface.mainWindow()
    menu_bar = main_window.menuBar()
    menu = QMenu("&QCopilots", main_window)
    menu.setObjectName(MENU_OBJECT_NAME)
    menu.setProperty(MENU_CREATED_PROPERTY, True)
    menu.setProperty(MENU_SHARED_CREATED_PROPERTY, True)

    before = None
    if hasattr(iface, "firstRightStandardMenu"):
        right_menu = iface.firstRightStandardMenu()
        if right_menu:
            before = right_menu.menuAction()

    if before:
        menu_bar.insertMenu(before, menu)
    else:
        menu_bar.addMenu(menu)
    return menu


def _qcopilots_created_menu(menu) -> bool:
    return bool(menu.property(MENU_SHARED_CREATED_PROPERTY) or menu.property(MENU_CREATED_PROPERTY))


def _find_qcopilots_menu(iface):
    menu_bar = iface.mainWindow().menuBar()
    for action in menu_bar.actions():
        menu = action.menu()
        if not menu:
            continue
        title = menu.title().replace("&", "")
        if menu.objectName() == MENU_OBJECT_NAME or title == "QCopilots":
            return menu
    return None
