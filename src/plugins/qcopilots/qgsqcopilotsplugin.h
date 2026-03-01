/***************************************************************************
 *  qgsqcopilotsplugin.h                                                  *
 *  ----------------------                                                 *
 *  在 QGIS 中使用 LLM 辅助用户进行开发、数据处理、制图等等，该插件中将会包含多个*
 *  功能模块，目前只包含了一个基于 llama-server 的 Web 视图模块。              *
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
class QMenu;

/*
注：实现一个简单的插件，添加了一个可停靠的网页视图，指向正在运行的 llama-server UI 界面，用户可以在 QGIS 内部直接访问该界面进行相关设置。插件的核心功能是提供一个界面来显示和配置与 LLM 相关的服务器 URL，并处理加载失败的情况以提供用户反馈。
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
    QMenu *mMenu = nullptr;
    QAction *mToggleAction = nullptr;
    QAction *mConfigureUrlAction = nullptr;
};

// QCopilots 插件元数据
static const QString sName = QApplication::translate( "QgsQCopilotsPlugin", "QCopilots" );
static const QString sDescription = QApplication::translate( "QgsQCopilotsPlugin", "LLM is used in QGIS to assist users in development, data processing, cartography, and more. The current module provides a docked llama-server Web UI container, and MCP servers can be configured from the llama-server Web UI settings." );
static const QString sCategory = QApplication::translate( "QgsQCopilotsPlugin", "QCopilots" );
static const QString sPluginVersion = QApplication::translate( "QgsQCopilotsPlugin", "Version 1.0" );
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
static const QString sPluginIcon = QStringLiteral( ":/qcopilots/icons/qcopilots-plugin.svg" );

#endif // QGS_QCOPILOTS_PLUGIN_H
