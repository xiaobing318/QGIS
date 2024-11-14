/***************************************************************************
                          qgspluginregistry.h
           Singleton class for keeping track of installed plugins.
                             -------------------
    begin                : Mon Jan 26 2004
    copyright            : (C) 2004 by Gary E.Sherman
    email                : sherman at mrcc.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*
* 杨小兵-2024-02-24
1. 代码分析
  这段代码定义了一个名为 `QgsPluginRegistry` 的类，它是 QGIS 项目中用于管理插件的单例类。这个类的主要职责是跟踪当前已加载的插件，并提供接
口来获取插件的指针、加载和卸载插件。它通过使用一个映射（`QMap<QString, QgsPluginMetadata>`）来存储插件的元数据，其中键是插件的唯一标识符
（对于C++插件是库的基本名称，对于Python插件是插件的模块名），值是插件的元数据（`QgsPluginMetadata`）。

主要功能包括：
- 管理插件的加载和卸载。
- 提供访问已加载插件的接口。
- 检查插件是否已加载。
- 存储和提供对Python工具和Qgis接口的访问。
- 从多个目录恢复会话插件。
- 检查插件与当前QGIS版本的兼容性。
  这个类使用了单例模式来确保整个应用程序中只有一个插件注册表实例（单例模式的作用就是为了确保整个应用程序中只含有一个插件注册表）。

### 2. 单例模式解释
  单例模式是一种设计模式，它确保一个类只有一个实例，并提供一个全局访问点来获取这个实例（1、确保一个类只有一个实例；2、提供全局的访问点来获
取这个类的唯一实例）。在单例模式中，类自己负责跟踪它的唯一实例，并确保没有其他实例可以被创建（通常是通过隐藏构造函数和提供一个静态方法来实现）。

单例模式的关键特点包括：
- **单一责任原则**：单例类专注于管理其唯一的实例。
- **全局访问点**：提供了一个全局访问该实例的接口，通常是通过一个静态方法实现，如 `instance()` 方法。
- **自我管理**：单例类负责创建、初始化和管理自己的唯一实例。

在 `QgsPluginRegistry` 类中，单例模式通过以下方式实现：
- 私有的构造函数防止外部直接创建实例。
- 一个私有的静态指针 `sInstance` 用来持有类的唯一实例。
- 一个公有的静态方法 `instance()` 用来提供全局访问点。如果实例不存在，`instance()` 方法会创建它；如果已存在，直接返回这个实例。
  单例模式在需要控制资源的访问或者需要全局访问点时非常有用，例如，在管理配置设置、线程池或注册表（如 `QgsPluginRegistry`）等场景中。然而，
使用单例模式也需要谨慎，因为它可能导致代码之间的高耦合以及在多线程环境中的同步问题。

qgspluginmetadata类的解释
  这个qgspluginmetadata类描述了一个被加载到QGIS中的插件属性信息
 1、已经被加载插件的名称
 2、已经被加载插件的路径
 3、已经被加载插件的实例化（通过指针的方式）
*/

#ifndef QGSPLUGINREGISTRY_H
#define QGSPLUGINREGISTRY_H

#include <QMap>
#include "qgspluginmetadata.h"
#include "qgis_app.h"

class QgsPythonUtils;
class QgisPlugin;
class QgisInterface;
class QString;

/**
* \class QgsPluginRegistry
* \brief This class tracks plugins that are currently loaded and provides
* a means to fetch a pointer to a plugin and unload it
*
* plugin key is:
*
* - C++ plugins: base name of plugin library, e.g. libgrassplugin
* - Python plugins: module name (directory) of plugin, e.g. db_manager
*/
class APP_EXPORT QgsPluginRegistry
{
  public:
    //! Returns the instance pointer, creating the object on the first call
    static QgsPluginRegistry *instance();

    //! Sets pointer to qgis interface passed to plugins (used by QgisApp)
    void setQgisInterface( QgisInterface *iface );

    //! Check whether this module is loaded
    bool isLoaded( const QString &key ) const;

    //! Retrieve library of the plugin
    QString library( const QString &key );

    //! Retrieve a pointer to a loaded plugin
    QgisPlugin *plugin( const QString &key );

    //! Returns whether the plugin is pythonic
    bool isPythonPlugin( const QString &key ) const;

    //! Add a plugin to the map of loaded plugins
    void addPlugin( const QString &key, const QgsPluginMetadata &metadata );

    //! Remove a plugin from the list of loaded plugins
    void removePlugin( const QString &key );

    //! Unload plugins
    void unloadAll();

    //! Save pointer for Python utils (needed for unloading Python plugins)
    void setPythonUtils( QgsPythonUtils *pythonUtils );

    //! Dump list of plugins
    void dump();

    //! C++ plugin loader
    void loadCppPlugin( const QString &mFullPath );
    //! Python plugin loader
    void loadPythonPlugin( const QString &packageName );

    //! C++ plugin unloader
    void unloadCppPlugin( const QString &fullPathName );
    //! Python plugin unloader
    void unloadPythonPlugin( const QString &packageName );

    //! Overloaded version of the next method that will load from multiple directories not just one
    void restoreSessionPlugins( const QStringList &pluginDirList );
    //! Load any plugins used in the last qgis session
    void restoreSessionPlugins( const QString &pluginDirString );

    //! Check whether plugin is compatible with current version of QGIS
    bool isPythonPluginCompatible( const QString &packageName ) const;

    //! Returns metadata of all loaded plugins
    QList<QgsPluginMetadata *> pluginData();

  protected:
    //! protected constructor
    QgsPluginRegistry() = default;

    //! Try to load and get metadata from c++ plugin, return true on success
    bool checkCppPlugin( const QString &pluginFullPath );
    //! Try to load and get metadata from Python plugin, return true on success
    bool checkPythonPlugin( const QString &packageName );

    /**
     * Check current QGIS version against requested minimal and optionally maximal QGIS version
     * if maxVersion not specified, the default value is assumed: std::floor(minVersion) + 0.99.99
     */
    bool checkQgisVersion( const QString &minVersion, const QString &maxVersion = QString() ) const;

  private:
    static QgsPluginRegistry *sInstance;
    QMap<QString, QgsPluginMetadata> mPlugins;
    QgsPythonUtils *mPythonUtils = nullptr;
    QgisInterface *mQgisInterface = nullptr;
};

// clazy:excludeall=qstring-allocations

#endif //QgsPluginRegistry_H
