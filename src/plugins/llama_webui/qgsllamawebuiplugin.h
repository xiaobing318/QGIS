/***************************************************************************
 *  qgsllamawebuiplugin.h                                                  *
 *  ----------------------                                                 *
 *  Embeds the llama.cpp web UI inside a QGIS dock widget.                 *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#ifndef QGS_LLAMA_WEBUI_PLUGIN_H
#define QGS_LLAMA_WEBUI_PLUGIN_H

#include "qgis.h"
#include "qgisplugin.h"

#include <QAction>
#include <QApplication>

class QgisInterface;
class QgsLlamaWebuiDockWidget;

/**
 * Minimal plugin which adds a dockable web view pointing at a running llama-server UI.
 */
class QgsLlamaWebuiPlugin : public QObject, public QgisPlugin
{
    Q_OBJECT

  public:
    explicit QgsLlamaWebuiPlugin( QgisInterface *iface );
    void initGui() override;
    void unload() override;

  private:
    void ensureDock();

    QgisInterface *mIface = nullptr;
    QgsLlamaWebuiDockWidget *mDock = nullptr;
    QAction *mToggleAction = nullptr;
};

static const QString sName = QApplication::translate( "QgsLlamaWebuiPlugin", "LLM Web UI" );
static const QString sDescription = QApplication::translate( "QgsLlamaWebuiPlugin", "Embed llama.cpp web UI inside QGIS" );
static const QString sCategory = QApplication::translate( "QgsLlamaWebuiPlugin", "Web" );
static const QString sPluginVersion = QApplication::translate( "QgsLlamaWebuiPlugin", "Version 0.1" );
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
static const QString sPluginIcon = QStringLiteral( ":/images/icons/qgis-icon-60x60.png" );

#endif // QGS_LLAMA_WEBUI_PLUGIN_H
