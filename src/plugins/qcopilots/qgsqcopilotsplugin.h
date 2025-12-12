/***************************************************************************
 *  qgsqcopilotsplugin.h                                                  *
 *  ----------------------                                                 *
 *  在 QGIS 中使用 LLM 辅助用户进行开发、数据处理、制图等等                    *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#ifndef QGS_QCOPILOTS_PLUGIN_H
#define QGS_QCOPILOTS_PLUGIN_H

#include "qgis.h"
#include "qgisplugin.h"

#include <QAction>
#include <QApplication>
#include <QUrl>

class QgisInterface;
class QgsQCopilotsDock;

/*
1. Minimal plugin which adds a dockable web view pointing at a running llama-server UI.
*/
class QgsQCopilotsPlugin : public QObject, public QgisPlugin
{
    Q_OBJECT

  public:
    explicit QgsQCopilotsPlugin( QgisInterface *iface );
    void initGui() override;
    void unload() override;

  private:
    void configureServerUrl();
    void handleDockLoadFailed( const QUrl &url );
    void ensureDock();

    QgisInterface *mIface = nullptr;
    QgsQCopilotsDock *mQgsQCopilotsDock = nullptr;
    QAction *mToggleAction = nullptr;
    QAction *mReloadAction = nullptr;
    QAction *mConfigureUrlAction = nullptr;
};

static const QString sName = QApplication::translate( "QgsQCopilotsPlugin", "QCopilots" );
static const QString sDescription = QApplication::translate( "QgsQCopilotsPlugin", "在 QGIS 中使用 LLM 辅助用户进行开发、数据处理、制图等等。" );
static const QString sCategory = QApplication::translate( "QgsQCopilotsPlugin", "Web" );
static const QString sPluginVersion = QApplication::translate( "QgsQCopilotsPlugin", "Version 1.0" );
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
static const QString sPluginIcon = QStringLiteral( ":/qcopilots/icons/qcopilots-plugin.svg" );

#endif // QGS_QCOPILOTS_PLUGIN_H
