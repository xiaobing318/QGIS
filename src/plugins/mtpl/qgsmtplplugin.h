/***************************************************************************
  qgsmtplplugin.h
  ---------------
  copyright            : (C) 2026 QGIS Project
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMTPLPLUGIN_H
#define QGSMTPLPLUGIN_H

#include "qgisplugin.h"

#include <QObject>

class QAction;
class QgisInterface;
class QgsMtplDockWidget;
class QMenu;

class QgsMtplPlugin final : public QObject, public QgisPlugin
{
    Q_OBJECT

  public:
    explicit QgsMtplPlugin( QgisInterface *iface );

    void initGui() override;
    void unload() override;

  private:
    void ensureDock();
    void ensureMenu();

    QgisInterface *mIface = nullptr;
    QgsMtplDockWidget *mDock = nullptr;
    QMenu *mMenu = nullptr;
    bool mOwnsMenu = false;
    QAction *mToggleAction = nullptr;
    bool mLayerTypeRegistered = false;
};

#endif // QGSMTPLPLUGIN_H
