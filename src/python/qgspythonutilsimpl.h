/***************************************************************************
    qgspythonutilsimpl.h - routines for interfacing Python
    ---------------------
    begin                : May 2008
    copyright            : (C) 2008 by Martin Dobias
    email                : wonder.sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef QGSPYTHONUTILSIMPL_H
#define QGSPYTHONUTILSIMPL_H

#pragma region "包含头文件"
/*
* 杨小兵-2024-03-22
一、解释

  `"qgspythonutilsimpl.h - routines for interfacing Python"` 这段话描述的是一个头文件（`qgspythonutilsimpl.h`），该文件包含了一系列用于与 Python
进行交互的例程（即函数或方法）。在 QGIS 的上下文中，这种交互通常涉及调用 Python 脚本和模块以扩展 QGIS 的功能，执行自动化任务，或集成 Python 开发的第三方库。

### 分析提到的概念
- **C++ 和 Python 的集成**：`qgspythonutilsimpl.h` 实现的例程使得 C++（QGIS 的主要开发语言之一）能够调用 Python 代码。这种集成使 QGIS 可以利用 Python
的广泛库和框架，为 QGIS 用户提供脚本和自动化的能力。
- **自动化和扩展**：通过这种方式，QGIS 可以扩展其功能，包括自动化任务、数据处理和分析，以及集成新的工具和算法，这些可能是用 Python 开发的。
- **插件开发**：Python 是 QGIS 插件开发的主要语言。这个接口允许插件与 QGIS 的核心功能进行交互，使得开发人员可以创建功能丰富的插件。

二、总结
1、qgspythonutilsimpl.h当前这个头文件中包含了一个类QgsPythonUtilsImpl这个类中有一些函数或者是方法，这些函数或者方法的作用就是用来同python进行交互。
2、- **qgspythonutilsimpl**：名字中的“qgs”通常是 QGIS 项目特有的前缀，表示这是 QGIS 相关的一部分。“pythonutilsimpl”可能表示这是 Python 实用程序
（utilities）的实现（implementation），“impl”通常是“implementation”的缩写。

*/
#include "qgspythonutils.h"
#pragma endregion

#pragma region "前向声明"
// forward declaration for PyObject
#ifndef PyObject_HEAD
struct _object;
typedef _object PyObject;
#endif
#pragma endregion

/*
* 杨小兵-2024-03-22

一、QgsPythonUtilsImpl中提供了哪些函数接口（目前总结了部分函数接口）
1、QgsPythonUtilsImpl          构造函数
2、~QgsPythonUtilsImpl()       重写的析构函数
3、initPython                  重写初始化python函数
4、exitPython                  重写退出python函数
5、isEnabled                   重写是否可用函数
6、runString                   重写运行string函数
7、runStringUnsafe             重写不安全运行string函数
8、evalString                  重写evaluate字符串函数
9、getError                    重写获得error函数

*/
class QgsPythonUtilsImpl : public QgsPythonUtils
{
  public:
    QgsPythonUtilsImpl();

    ~QgsPythonUtilsImpl() override;

#pragma region "通用目的函数"
    /* general purpose functions */

    void initPython( QgisInterface *interface, bool installErrorHook, const QString &faultHandlerLogPath = QString() ) override;
#ifdef HAVE_SERVER_PYTHON_PLUGINS
    void initServerPython( QgsServerInterface *interface ) override;
    bool startServerPlugin( QString packageName ) override;
#endif
    void exitPython() override;
    bool isEnabled() override;
    bool runString( const QString &command, QString msgOnError = QString(), bool single = true ) override;
    QString runStringUnsafe( const QString &command, bool single = true ) override; // returns error traceback on failure, empty QString on success
    bool evalString( const QString &command, QString &result ) override;
    bool getError( QString &errorClassName, QString &errorText ) override;

#pragma endregion

#pragma region "辅助函数"
    /**
     * Returns the path where QGIS Python related files are located.
     */
    QString pythonPath() const;

    /**
     * Returns an object's type name as a string
     */
    QString getTypeAsString( PyObject *obj );

#pragma endregion

#pragma region "插件相关的函数"
    /* plugins related functions */

    /**
     * Returns the current path for Python plugins
     */
    QString pluginsPath() const;

    /**
     * Returns the current path for Python in home directory.
     */
    QString homePythonPath() const;

    /**
     * Returns the current path for home directory Python plugins.
     */
    QString homePluginsPath() const;

    /**
     * Returns a list of extra plugins paths passed with QGIS_PLUGINPATH environment variable.
     */
    QStringList extraPluginsPaths() const;

    QStringList pluginList() override;
    bool isPluginLoaded( const QString &packageName ) override;
    QStringList listActivePlugins() override;
    bool loadPlugin( const QString &packageName ) override;
    bool startPlugin( const QString &packageName ) override;
    bool startProcessingPlugin( const QString &packageName ) override;
    QString getPluginMetadata( const QString &pluginName, const QString &function ) override;
    bool pluginHasProcessingProvider( const QString &pluginName ) override;
    bool canUninstallPlugin( const QString &packageName ) override;
    bool unloadPlugin( const QString &packageName ) override;
    bool isPluginEnabled( const QString &packageName ) const override;
#pragma endregion

  protected:
#pragma region "进行初始化工作的函数"
    /* functions that do the initialization work */

    //! initialize Python context
    void init();

    //! check qgis imports and plugins
    //\returns true if all imports worked
    bool checkSystemImports();

    //\returns true if qgis.user could be imported
    bool checkQgisUser();

    //! import custom user and global Python code (startup scripts)
    void doCustomImports();

    //! cleanup Python context
    void finish();

    void installErrorHook();

    void uninstallErrorHook();

    QString getTraceback();

    //! convert Python object to QString. If the object isn't unicode/str, it will be converted
    QString PyObjectToQString( PyObject *obj );

#pragma endregion

#pragma region "受保护的成员变量"
    //! reference to module __main__
    PyObject *mMainModule = nullptr;

    //! dictionary of module __main__
    PyObject *mMainDict = nullptr;

    //! flag determining that Python support is enabled
    bool mPythonEnabled = false;

#pragma endregion

  private:
#pragma region "私有成员变量"
    bool mErrorHookInstalled = false;
#pragma endregion
};

#endif
