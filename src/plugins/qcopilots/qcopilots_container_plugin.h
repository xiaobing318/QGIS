/***************************************************************************
    qcopilots_container_plugin.h
    ----------------------------
    begin                : July 2026
    copyright            : (C) 2026
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QCOPILOTS_CONTAINER_PLUGIN_H
#define QCOPILOTS_CONTAINER_PLUGIN_H

#include "qgis.h"
#include "qgisplugin.h"

#include <QObject>
#include <QUrl>

class QAction;
class QgisInterface;
class QgsQCopilotsDock;
class QMenu;

class QgsQCopilotsPlugin : public QObject, public QgisPlugin
{
    Q_OBJECT

  public:
    explicit QgsQCopilotsPlugin( QgisInterface *iface );

    void initGui() override;
    void unload() override;

  private:
    void ensureDock();
    void ensureMenu();
    void toggleDock( bool visible );
    void configureServerUrl();
    void handleDockLoadFailed( const QUrl &url );

    QgisInterface *mIface = nullptr;
    QgsQCopilotsDock *mDock = nullptr;
    QMenu *mMenu = nullptr;
    bool mOwnsMenu = false;

    QAction *mToggleAction = nullptr;
    QAction *mConfigureUrlAction = nullptr;
    QAction *mMenuSeparator = nullptr;
};

#endif // QCOPILOTS_CONTAINER_PLUGIN_H
