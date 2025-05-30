/***************************************************************************
                    QgsPluginRegistry.cpp  -  Singleton class for
                    tracking registering plugins.
                             -------------------
    begin                : Fri Feb 7 2004
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

#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMessageBox>

#include "qgssettings.h"
#include "qgis.h"
#include "qgsapplication.h"
#include "qgisinterface.h"
#include "qgspluginregistry.h"
#include "qgspluginmetadata.h"
#include "qgisplugin.h"
#include "qgisapp.h"
#include "qgslogger.h"
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"
#include "qgsmessagebaritem.h"
#include "qgsruntimeprofiler.h"

#ifdef WITH_BINDINGS
#include "qgspythonutils.h"
#endif

/* typedefs for plugins */
typedef QgisPlugin *create_ui( QgisInterface *qI );
typedef const QString *name_t();
typedef const QString *description_t();
typedef const QString *category_t();
typedef int type_t();


QgsPluginRegistry *QgsPluginRegistry::sInstance = nullptr;

QgsPluginRegistry *QgsPluginRegistry::instance()
{
  if ( !sInstance )
  {
    sInstance = new QgsPluginRegistry();
  }
  return sInstance;
}

void QgsPluginRegistry::setQgisInterface( QgisInterface *iface )
{
  mQgisInterface = iface;
}

void QgsPluginRegistry::setPythonUtils( QgsPythonUtils *pythonUtils )
{
  mPythonUtils = pythonUtils;
}

bool QgsPluginRegistry::isLoaded( const QString &key ) const
{
  const QMap<QString, QgsPluginMetadata>::const_iterator it = mPlugins.find( key );
  if ( it != mPlugins.end() ) // found a c++ plugin?
    return true;

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    return mPythonUtils->isPluginLoaded( key );
  }
#endif

  return false;
}

QString QgsPluginRegistry::library( const QString &key )
{
  const QMap<QString, QgsPluginMetadata>::const_iterator it = mPlugins.constFind( key );
  if ( it != mPlugins.constEnd() )
    return it->library();

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    if ( mPythonUtils->isPluginLoaded( key ) )
      return key;
  }
#endif

  return QString();
}

QgisPlugin *QgsPluginRegistry::plugin( const QString &key )
{
  const QMap<QString, QgsPluginMetadata>::iterator it = mPlugins.find( key );
  if ( it == mPlugins.end() )
    return nullptr;

  // note: not used by python plugins

  return it->plugin();
}

bool QgsPluginRegistry::isPythonPlugin( const QString &key ) const
{
#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    if ( mPythonUtils->isPluginLoaded( key ) )
      return true;
  }
#else
  Q_UNUSED( key )
#endif
  return false;
}

void QgsPluginRegistry::addPlugin( const QString &key, const QgsPluginMetadata &metadata )
{
  mPlugins.insert( key, metadata );
}

void QgsPluginRegistry::dump()
{
  QgsDebugMsg( QStringLiteral( "PLUGINS IN REGISTRY: key -> (name, library)" ) );
  for ( QMap<QString, QgsPluginMetadata>::const_iterator it = mPlugins.constBegin();
        it != mPlugins.constEnd();
        ++it )
  {
    QgsDebugMsg( QStringLiteral( "PLUGIN: %1 -> (%2, %3)" )
                 .arg( it.key(),
                       it->name(),
                       it->library() ) );
  }

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    QgsDebugMsg( QStringLiteral( "PYTHON PLUGINS IN REGISTRY:" ) );
    const auto constListActivePlugins = mPythonUtils->listActivePlugins();
    for ( const QString &pluginName : constListActivePlugins )
    {
      Q_UNUSED( pluginName )
      QgsDebugMsg( pluginName );
    }
  }
#endif
}


void QgsPluginRegistry::removePlugin( const QString &key )
{
  QgsDebugMsg( "removing plugin: " + key );
  const QMap<QString, QgsPluginMetadata>::iterator it = mPlugins.find( key );
  if ( it != mPlugins.end() )
  {
    mPlugins.erase( it );
  }

  // python plugins are removed when unloaded
}

void QgsPluginRegistry::unloadAll()
{
  for ( QMap<QString, QgsPluginMetadata>::iterator it = mPlugins.begin();
        it != mPlugins.end();
        ++it )
  {
    if ( it->plugin() )
    {
      it->plugin()->unload();
    }
    else
    {
      QgsDebugMsg( "warning: plugin is NULL:" + it.key() );
    }
  }

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    const auto constListActivePlugins = mPythonUtils->listActivePlugins();
    for ( const QString &pluginName : constListActivePlugins )
    {
      mPythonUtils->unloadPlugin( pluginName );
    }
  }
#endif
}


bool QgsPluginRegistry::checkQgisVersion( const QString &minVersion, const QString &maxVersion ) const
{
  // Parse qgisMinVersion. Must be in form x.y.z or just x.y
  const QStringList minVersionParts = minVersion.split( '.' );
  if ( minVersionParts.count() != 2 && minVersionParts.count() != 3 )
    return false;

  int minVerMajor, minVerMinor, minVerBugfix = 0;
  bool ok;
  minVerMajor = minVersionParts.at( 0 ).toInt( &ok );
  if ( !ok )
    return false;
  minVerMinor = minVersionParts.at( 1 ).toInt( &ok );
  if ( !ok )
    return false;
  if ( minVersionParts.count() == 3 )
  {
    minVerBugfix = minVersionParts.at( 2 ).toInt( &ok );
    if ( !ok )
      return false;
  }

  // Parse qgisMaxVersion. Must be in form x.y.z or just x.y
  int maxVerMajor, maxVerMinor, maxVerBugfix = 99;
  if ( maxVersion.isEmpty() || maxVersion == QLatin1String( "__error__" ) )
  {
    maxVerMajor = minVerMajor;
    maxVerMinor = 99;
  }
  else
  {
    const QStringList maxVersionParts = maxVersion.split( '.' );
    if ( maxVersionParts.count() != 2 && maxVersionParts.count() != 3 )
      return false;

    bool ok;
    maxVerMajor = maxVersionParts.at( 0 ).toInt( &ok );
    if ( !ok )
      return false;
    maxVerMinor = maxVersionParts.at( 1 ).toInt( &ok );
    if ( !ok )
      return false;
    if ( maxVersionParts.count() == 3 )
    {
      maxVerBugfix = maxVersionParts.at( 2 ).toInt( &ok );
      if ( !ok )
        return false;
    }
  }

  // our qgis version - cut release name after version number
  const QString qgisVersion = Qgis::version().section( '-', 0, 0 );

  const QStringList qgisVersionParts = qgisVersion.split( '.' );

  int qgisMajor = qgisVersionParts.at( 0 ).toInt();
  int qgisMinor = qgisVersionParts.at( 1 ).toInt();
  int qgisBugfix = qgisVersionParts.at( 2 ).toInt();

  if ( qgisMinor == 99 )
  {
    // we want the API version, so for x.99 bump it up to the next major release: e.g. 2.99 to 3.0.0
    qgisMajor ++;
    qgisMinor = 0;
    qgisBugfix = 0;
  };

  // build XxYyZz strings with trailing zeroes if needed
  const QString minVer = QStringLiteral( "%1%2%3" ).arg( minVerMajor, 2, 10, QChar( '0' ) )
                         .arg( minVerMinor, 2, 10, QChar( '0' ) )
                         .arg( minVerBugfix, 2, 10, QChar( '0' ) );
  const QString maxVer = QStringLiteral( "%1%2%3" ).arg( maxVerMajor, 2, 10, QChar( '0' ) )
                         .arg( maxVerMinor, 2, 10, QChar( '0' ) )
                         .arg( maxVerBugfix, 2, 10, QChar( '0' ) );
  const QString curVer = QStringLiteral( "%1%2%3" ).arg( qgisMajor, 2, 10, QChar( '0' ) )
                         .arg( qgisMinor, 2, 10, QChar( '0' ) )
                         .arg( qgisBugfix, 2, 10, QChar( '0' ) );

  // compare
  return ( minVer <= curVer && maxVer >= curVer );
}


void QgsPluginRegistry::loadPythonPlugin( const QString &packageName )
{
#ifdef WITH_BINDINGS
  if ( !mPythonUtils || !mPythonUtils->isEnabled() )
  {
    QgsMessageLog::logMessage( QObject::tr( "Python is not enabled in QGIS." ), QObject::tr( "Plugins" ) );
    return;
  }

  QgsSettings settings;

  // is loaded already?
  if ( ! isLoaded( packageName ) )
  {
    // if plugin is not compatible, disable it
    if ( ! isPythonPluginCompatible( packageName ) )
    {
      QgsMessageLog::logMessage( QObject::tr( "Plugin \"%1\" is not compatible with this version of QGIS.\nIt will be disabled." ).arg( packageName ),
                                 QObject::tr( "Plugins" ) );
      settings.setValue( "/PythonPlugins/" + packageName, false );
      return;
    }

    const QgsScopedRuntimeProfile profile( packageName );
    mPythonUtils->loadPlugin( packageName );
    mPythonUtils->startPlugin( packageName );

    // TODO: test success

    const QString pluginName = mPythonUtils->getPluginMetadata( packageName, QStringLiteral( "name" ) );

    // add to settings
    settings.setValue( "/PythonPlugins/" + packageName, true );
    QgsMessageLog::logMessage( QObject::tr( "Loaded %1 (package: %2)" ).arg( pluginName, packageName ), QObject::tr( "Plugins" ), Qgis::MessageLevel::Info );

    settings.remove( "/PythonPlugins/watchDog/" + packageName );
  }
#else
  Q_UNUSED( packageName )
#endif
}

/*
* 杨小兵-2024-02-23
  在QGIS（一个开源地理信息系统）中加载C++插件的函数实现。

### 1. 函数加载插件的逻辑
1. **初始化设置和插件基本信息**：通过`QgsSettings`初始化设置，使用`QFileInfo`获取插件文件的基本名称（不包括路径和扩展名）。
2. **检查插件是否已加载**：使用`isLoaded`函数检查基于插件基本名称的插件是否已经加载。如果已加载，函数将直接返回，不执行后续操作。
3. **插件库加载前的准备**：创建一个`QgsScopedRuntimeProfile`对象（可能用于性能分析），接着通过`QLibrary`尝试加载指定路径的插件库文件。
4. **检查库是否成功加载**：如果库文件没有成功加载，记录错误日志并返回。否则，继续执行。
5. **解析插件的导出函数**：尝试解析插件库中的`type`和`name`函数，以及基于插件类型（UI或Renderer）解析`classFactory`函数。
6. **根据插件类型创建并初始化插件实例**：
   - 如果`classFactory`函数解析成功，使用它来创建一个插件实例。
   - 调用插件的`initGui`方法进行初始化（对于插件类来说首先调用的是构造函数、然后调用initGui函数，这个函数必须override，插件类必须继承qgisplugin.h）
   - 将插件信息添加到插件注册表中，并更新设置以反映插件的加载状态。
7. **插件实例后续处理**：
   - 设置插件对象的名称和父对象。
   - 如果插件实例创建失败或`classFactory`函数解析失败，显示错误信息，并更新设置以反映插件未能加载。
8. **清理**：在成功加载插件后，移除与插件相关的特定设置。

### 2. QGIS插件的加载方式
  QGIS插件通常通过动态链接库（DLLs，在Windows上）或共享对象（.so文件，在Unix-like系统上）形式分发。这段代码通过以下方式加载这些插件：

- **动态解析**：使用`QLibrary`对象动态加载插件库文件。`QLibrary`提供跨平台支持，允许在运行时加载和解析库文件中的函数。

- **函数解析**：通过`QLibrary`的`resolve`方法动态解析库中的特定函数，例如`type`、`name`和`classFactory`。这允许QGIS在不直接链接到
插件库的情况下调用这些函数。

- **插件实例化**：通过解析出的`classFactory`函数动态创建插件实例。这个函数通常返回一个指向插件主类的指针，QGIS通过这个指针与插件进行交互。
(通过这个函数动态创建插件实例化)

- **接口绑定**：插件通过实现特定的接口（如UI或Renderer）与QGIS核心功能集成。这通常在`classFactory`函数中完成，其中QGIS提供了必要的接口
指针给插件，插件则通过这些接口与QGIS交互。
  这种方式允许QGIS在运行时加载和卸载插件，提供了高度的灵活性和扩展性。插件开发者可以独立于主程序开发和分发插件，而用户可以根据需要安装和使
用这些插件。

### 3.QT框架中QLibrary类的解释
  QLibrary 是 Qt 框架中用于动态加载共享库（动态链接库）的类（可以在多个不同平台中提供相同的加载动态库的函数接口）。这个类提供了一个便捷的
跨平台方式来加载库并访问其中的函数（1、加载动态链接库；2、访问其中的函数）。在不同的操作系统上，共享库可能被称为动态链接库（DLLs）在Windows
上，或者是共享对象（SO）在Unix-like系统上，QLibrary 封装了操作系统的细节，使得加载和使用这些库的过程变得简单而一致。

### QLibrary 的常用函数及其作用
1. **`QLibrary(const QString &fileName, QObject *parent = nullptr)`**
   - 构造函数，用于创建一个 QLibrary 对象，并指定要加载的库的名称。`fileName` 可以是库的完整路径名或仅仅是库名。如果只提供库名，QLibrary
   会在标准位置搜索该库。

2. **`bool load()`**
   - 加载由 `QLibrary` 对象指定的库。如果加载成功，返回 `true`；否则返回 `false`。这个函数实际上是在第一次需要库的时候调用，不过也可以显
   式调用来检测库是否可以被加载。

3. **`bool isLoaded() const`**
   - 检查库是否已经被加载。返回 `true` 表示库已加载，`false` 表示未加载。

4. **`bool unload()`**
   - 卸载库。如果库被成功卸载，返回 `true`；否则返回 `false`。卸载库后，库中的所有符号（函数、变量等）将不再可用。

5. **`void* resolve(const char *symbol)`**
   - 查找库中给定名称的符号（例如函数或变量的地址）。如果找到，返回指向该符号的指针；如果未找到，返回 `nullptr`。

6. **`static bool isLibrary(const QString &fileName)`**
   - 静态函数，用于检查指定的文件名是否为一个有效的库。这可以在尝试加载库之前用来验证文件。

7. **`QString errorString() const`**
   - 如果 `load()` 或 `resolve()` 失败，这个函数可以用来获取关于失败原因的详细信息。

8. **`QString fileName() const`**
   - 返回库的文件名。如果使用的是库的简称而非完整路径，这个函数返回的是完整的文件名。

*/
void QgsPluginRegistry::loadCppPlugin( const QString &fullPathName )
{
  QgsSettings settings;

  //  获取fullPathName（待加载插件的绝对路径）
  QString baseName = QFileInfo( fullPathName ).baseName();

  // first check to see if it's already loaded（如果已经加载了baseName的插件，那么直接返回就可以了，不需要在继续处理后续的代码）
  if ( isLoaded( baseName ) )
  {
    // plugin is loaded
    // QMessageBox::warning(this, "Loading Plugins", description + " is already loaded");
    return;
  }

  //  通过profile测试加载插件的性能问题
  const QgsScopedRuntimeProfile profile( baseName );

  
  QLibrary myLib( fullPathName );

  QString myError; //we will only show detailed diagnostics if something went wrong（仅展示细节诊断）
  myError += QObject::tr( "Library name is %1\n" ).arg( myLib.fileName() );

  const bool loaded = myLib.load();
  //  如果动态库myLib被加载失败，那么flag(loaded)将会是flase，输出对应的日志信息
  if ( !loaded )
  {
    QgsMessageLog::logMessage( QObject::tr( "Failed to load %1 (Reason: %2)" ).arg( myLib.fileName(), myLib.errorString() ), QObject::tr( "Plugins" ) );
    return;
  }

  myError += QObject::tr( "Attempting to resolve the classFactory function\n" );
  //  杨小兵-2024-02-23：QLibrary类可以解析一个动态库中暴露的函数或者变量，通过resolve函数来达到这个目的
  type_t *pType = ( type_t * ) cast_to_fptr( myLib.resolve( "type" ) );
  name_t *pName = ( name_t * ) cast_to_fptr( myLib.resolve( "name" ) );

  switch ( pType() )
  {
    case QgisPlugin::Renderer:
    case QgisPlugin::UI:
    {
      // UI only -- doesn't use mapcanvas
      create_ui *cf = ( create_ui * ) cast_to_fptr( myLib.resolve( "classFactory" ) );
      if ( cf )
      {
        QgisPlugin *pl = cf( mQgisInterface );
        if ( pl )
        {
          //  这就是为什么在插件类中首先执行构造函数，然后再调用initGui函数
          pl->initGui();
          // add it to the plugin registry
          addPlugin( baseName, QgsPluginMetadata( myLib.fileName(), *pName(), pl ) );
          //add it to the qsettings file [ts]
          settings.setValue( "/Plugins/" + baseName, true );
          QgsMessageLog::logMessage( QObject::tr( "Loaded %1 (Path: %2)" ).arg( *pName(), myLib.fileName() ), QObject::tr( "Plugins" ), Qgis::MessageLevel::Info );

          QObject *o = dynamic_cast<QObject *>( pl );
          if ( o )
          {
            QgsDebugMsgLevel( QStringLiteral( "plugin object name: %1" ).arg( o->objectName() ), 2 );
            if ( o->objectName().isEmpty() )
            {
#ifndef Q_OS_WIN
              baseName = baseName.mid( 3 );
#endif
              QgsDebugMsgLevel( QStringLiteral( "object name to %1" ).arg( baseName ), 2 );
              o->setObjectName( QStringLiteral( "qgis_plugin_%1" ).arg( baseName ) );
              QgsDebugMsgLevel( QStringLiteral( "plugin object name now: %1" ).arg( o->objectName() ), 2 );
            }

            if ( !o->parent() )
            {
              QgsDebugMsgLevel( QStringLiteral( "setting plugin parent" ), 2 );
              o->setParent( QgisApp::instance() );
            }
            else
            {
              QgsDebugMsgLevel( QStringLiteral( "plugin parent already set" ), 2 );
            }
          }

          settings.remove( "/Plugins/watchDog/" + baseName );
        }
        else
        {
          // something went wrong
          QMessageBox::warning( mQgisInterface->mainWindow(), QObject::tr( "Loading Plugins" ),
                                QObject::tr( "There was an error loading a plugin. "
                                             "The following diagnostic information may help the QGIS developers resolve the issue:\n%1." )
                                .arg( myError ) );
          //disable it to the qsettings file [ts]
          settings.setValue( "/Plugins/" + baseName, false );
        }
      }
      else
      {
        QgsMessageLog::logMessage( QObject::tr( "Unable to find the class factory for %1." ).arg( fullPathName ), QObject::tr( "Plugins" ) );
      }

    }
    break;
    default:
      // type is unknown
      QgsMessageLog::logMessage( QObject::tr( "Plugin %1 did not return a valid type and cannot be loaded" ).arg( fullPathName ), QObject::tr( "Plugins" ) );
      break;
  }
}


void QgsPluginRegistry::unloadPythonPlugin( const QString &packageName )
{
#ifdef WITH_BINDINGS
  if ( !mPythonUtils || !mPythonUtils->isEnabled() )
  {
    QgsMessageLog::logMessage( QObject::tr( "Python is not enabled in QGIS." ), QObject::tr( "Plugins" ) );
    return;
  }

  if ( isLoaded( packageName ) )
  {
    mPythonUtils->unloadPlugin( packageName );
    QgsDebugMsg( "Python plugin successfully unloaded: " + packageName );
  }

  // disable the plugin no matter if successfully loaded or not
  QgsSettings settings;
  settings.setValue( "/PythonPlugins/" + packageName, false );
#else
  Q_UNUSED( packageName )
#endif
}


void QgsPluginRegistry::unloadCppPlugin( const QString &fullPathName )
{
  QgsSettings settings;
  const QString baseName = QFileInfo( fullPathName ).baseName();
  settings.setValue( "/Plugins/" + baseName, false );
  if ( isLoaded( baseName ) )
  {
    QgisPlugin *pluginInstance = plugin( baseName );
    if ( pluginInstance )
    {
      pluginInstance->unload();
    }
    // remove the plugin from the registry
    removePlugin( baseName );
    QgsDebugMsg( "Cpp plugin successfully unloaded: " + baseName );
  }
}

#pragma region "restoreSessionPlugins(恢复会话插件:多个目录恢复插件、单个目录恢复插件)"
/*
* 杨小兵-2024-02-23
一、解释
  上述函数`restoreSessionPlugins`属于`QgsPluginRegistry`类，是一个用于恢复QGIS插件会话的函数。它的主要处理逻辑可以分为以下几个部分：

- **初始化设置**：使用`QgsSettings`创建一个配置设置对象`mySettings`，用于读取和存储插件的配置信息。
- **确定插件扩展名**：根据不同的操作系统，设置插件文件的扩展名（如Windows下为`*.dll`，Linux下为`*.so`等）。
- **遍历插件目录**：构造一个`QDir`对象`myPluginDir`，指向包含插件的目录，该目录下的文件以先前确定的插件扩展名进行过滤。然后遍历这些插件文件。
- **检查并恢复插件**：对于每个插件文件，先检查该插件是否是C++插件（通过调用`checkCppPlugin`）。如果是，再检查该插件在上次会话中是否被激活。如果插件之前导致QGIS崩溃而被禁用，则显示一条消息条目，提供“启用插件”和“忽略”的选项。对于被激活的插件，重新加载插件并更新设置，以反映插件的当前状态。
- **处理Python插件**（如果编译时包含了Python绑定）：检查系统范围内的Python插件，并尝试加载它们。与C++插件类似，如果某个Python插件之前导致QGIS崩溃，则为用户提供重新启用或忽略该插件的选项。
- **临时修复**：对于从构建目录运行的QGIS，还包括了一些临时修复措施，以处理特定的问题。

const QgsScopedRuntimeProfile profile( QObject::tr( "Load plugins" ) );
- 这行代码创建了一个`QgsScopedRuntimeProfile`实例`profile`，用于测量和记录代码块的运行时间。`QgsScopedRuntimeProfile`是一个作用域性能
分析工具，它在构造时开始计时，在析构时结束计时，自动记录该作用域内代码的执行时间。这对于性能调优和诊断潜在的性能瓶颈非常有用。
- `QObject::tr("Load plugins")`是一个国际化函数调用，用于翻译给定的字符串（在这里是"Load plugins"）。这意味着`profile`的目的是记录"加载
插件"这一操作的执行时间，这对于分析和优化插件加载过程的性能至关重要。
- `const`关键字表明`profile`对象在其生命周期内不会被修改。由于`QgsScopedRuntimeProfile`的工作机制，这里的`const`主要是用于强调对象的只读
性质，实际上`profile`的构造和析构会影响其内部状态（如计时器的开始和结束）

  QLibrary 主要用于需要动态加载库的场合。这样的需求常见于插件系统，或当应用需要扩展功能但不希望增加初始加载时间或增大可执行文件大小时。动态
加载还允许应用仅在需要某个库的功能时才加载它，从而减少资源消耗。从计算机实现的角度看，使用 QLibrary 可以使应用更加模块化。开发者可以将应用的
不同部分编译成独立的库，然后在运行时根据需要动态加载。这种方式提高了应用的灵活性和可维护性，也方便了用户按需安装额外功能。该函数中首先创建了
一个 QLibrary 对象，并尝试加载指定的库。如果加载失败，则记录错误信息并返回。这种处理方式对于开发具有动态插件架构的应用尤为重要，因为它允许应
用在缺少某些非关键组件的情况下继续运行，同时通知用户哪些功能不可用及其原因。


**`profile`的作用**：
- `profile`实例的主要作用是自动测量和记录从它被创建到被销毁（即从这一代码点到离开作用域的整个代码块）的时间跨度。在这个上下文中，它被用来监控
和记录插件加载过程的性能。这样的性能分析对于开发者在优化代码、改进加载时间等方面提供了重要的信息。通过这种方式，如果加载插件的过程异常缓慢，开
发者可以轻松地识别出潜在的性能问题并着手解决。


二、总结
  `restoreSessionPlugins`函数的主要作用是在QGIS启动时恢复上一次会话中的插件状态。具体来说，它会检查每个插件是否在上一次会话中被用户启用或因崩溃而被禁用，
并据此决定是否自动重新加载该插件。这个过程不仅适用于C++编写的插件，如果QGIS编译时包含了Python支持，同样的逻辑也会应用于Python插件。此外，该函数还提供了用
户界面元素（如消息条目），允许用户对之前因崩溃而被禁用的插件进行管理，例如重新启用或忽略这些插件，以提高用户体验和软件稳定性。

*/

//overloaded version of the next method that will load from multiple directories not just one
void QgsPluginRegistry::restoreSessionPlugins( const QStringList &pluginDirList )
{
  QStringListIterator myIterator( pluginDirList );
  while ( myIterator.hasNext() )
  {
    restoreSessionPlugins( myIterator.next() );
  }
}



void QgsPluginRegistry::restoreSessionPlugins( const QString &pluginDirString )
{
  QgsSettings mySettings;

  const QgsScopedRuntimeProfile profile( QObject::tr( "Load plugins" ) );

/*
* 杨小兵-2024-02-23
1. `#if defined(Q_OS_WIN) || defined(__CYGWIN__)`这段代码是预处理指令，用于条件编译。它的作用是检查是否定义了`Q_OS_WIN`或`__CYGWIN__`宏。
如果其中任一宏被定义，说明代码正在Windows平台或使用Cygwin环境下编译，此时将执行这个条件编译块内的代码。这样的机制允许开发者为不同的操作系统编
写特定的代码逻辑。

2. 在Windows平台中，动态链接库（Dynamic Link Libraries）的扩展名通常是`.dll`。这是Windows操作系统识别和管理动态库的标准扩展名。因此，上述
代码中`QString pluginExt = "*.dll";`指定了在Windows平台下，插件文件应该具有`.dll`扩展名。

3. 在Android平台中，并没有一个统一的标准来规定动态库（也称作共享对象）的扩展名必须是`plugin.so`。一般来说，Android平台上的动态库扩展名是
`.so`（表示共享对象）。然而，上述代码中使用`*plugin.so`可能是为了区分特定类型的插件动态库，或者是该项目自定义的命名约定。这意味着并非所有
Android平台上的动态库扩展名都是`plugin.so`，而是该特定应用或框架中用于识别插件的一种约定。

4. 在Linux平台中，动态库的扩展名通常是`.so`，代表共享对象（Shared Object）。这是Linux和类Unix系统中动态库的标准扩展名。因此，上述代码中
`const QString pluginExt = QStringLiteral( "*.so" );`正确指出了在非Windows和非Android平台（通常指的是Linux或其他Unix-like系统）中，
插件文件应该具有`.so`扩展名。这一标准适用于大多数基于Linux的系统和应用程序。
*/
#if defined(Q_OS_WIN) || defined(__CYGWIN__)
  QString pluginExt = "*.dll";
#elif ANDROID
  QString pluginExt = "*plugin.so";
#else
  const QString pluginExt = QStringLiteral( "*.so" );
#endif

/*
* 杨小兵-2024-02-23
  这段代码创建了一个`QDir`对象`myPluginDir`，用于表示一个目录，这里特指插件所在的目录。`QDir`类是Qt框架中用于操作目录和文件系统的类，
提供了访问目录内容、创建、删除目录等功能。在这个场景中，`QDir`对象用于检索特定扩展名的插件文件，并对这些文件进行进一步操作。下面详细解
释这行代码的各个部分：

- **`pluginDirString`**: 这是`QDir`构造函数的第一个参数，代表插件目录的路径，是一个`QString`类型的变量。它指定了要搜索插件的目录位置。

- **`pluginExt`**: 这是构造函数的第二个参数，代表要搜索的文件的扩展名模式。根据前面的条件编译逻辑，这个变量的值会根据不同的操作系统而有
所不同（如Windows下是`*.dll`，Android下是`*plugin.so`，Linux下是`*.so`）。

- **`QDir::Name | QDir::IgnoreCase`**: 这是构造函数的第三个参数，用于设置目录搜索时的排序和比较方式。`QDir::Name`表示按名称排序文件
和目录。`QDir::IgnoreCase`表示在比较文件名时忽略大小写。这两个选项通过位或操作组合起来，用于指定如何列出目录中的文件。

- **`QDir::Files | QDir::NoSymLinks`**: 这是构造函数的第四个参数，用于指定过滤条件。`QDir::Files`表示只列出文件，不包括目录。
`QDir::NoSymLinks`表示不包括符号链接（即快捷方式或软链接）。这同样是通过位或操作组合起来的，用于精确指定要列出的目录内容类型。

  综上所述，这行代码的作用是初始化一个`QDir`对象，用于访问和检索指定目录（`pluginDirString`）中符合特定扩展名（`pluginExt`）的文件，
同时按照文件名进行排序，并忽略大小写和符号链接。这对于实现插件的动态加载和管理非常有用，使得可以自动发现和处理目录中的所有合适的插件文件。
*/
  // check all libs in the current plugin directory and get name and descriptions（检查当前插件目录中的所有库并获取名称和描述）
  const QDir myPluginDir( pluginDirString, pluginExt, QDir::Name | QDir::IgnoreCase, QDir::Files | QDir::NoSymLinks );

  for ( uint i = 0; i < myPluginDir.count(); i++ )
  {
    //  针对不同的操作系统平台下，构建插件目录下所有的动态库绝对路径
    const QString myFullPath = pluginDirString + '/' + myPluginDir[i];
    if ( checkCppPlugin( myFullPath ) )
    {
      // check if the plugin was active on last session
      /* 杨小兵-2024-02-23
        这段代码的主要作用是检查一个插件在上一次QGIS会话中是否被激活，并且根据该插件的状态提供相应的用户界面（UI）选项，让用户决定是否重新
      启用之前崩溃的插件。具体的执行逻辑如下：
      
      1. **获取插件基础名称**：首先，通过`QFileInfo`和插件的完整路径`myFullPath`获取插件的基础名称（即不包含路径和扩展名的文件名）并存储
      在`baseName`变量中。
      
      2. **检查插件状态**：接着，检查QGIS的设置（通过`mySettings`对象），查看对应插件的状态是否被记录在`Plugins/watchDog/%1`路径下
      （`%1`会被替换为插件的基础名称）。如果该设置存在，说明插件在上一次会话中被禁用（通常是因为崩溃）。
      
      3. **创建UI元素**：如果插件之前被禁用，代码会创建两个按钮（`QToolButton`）：`btnEnablePlugin`用于启用插件，`btnIgnore`用于忽略
      （即不启用）插件。这两个按钮分别设置了文本和大小策略。
      
      4. **创建消息条目**：使用`QgsMessageBarItem`创建一个消息条目`watchdogMsg`，通知用户该插件在之前崩溃过QGIS。这个消息条目包含了之
      前创建的两个按钮，并将其添加到QGIS的消息栏中。
      
      5. **连接按钮的点击信号**：为两个按钮设置点击事件的槽函数。当`btnEnablePlugin`被点击时，会重新启用插件，并从`watchDog`设置中移除
      该插件的记录，然后从消息栏中移除这个消息条目。当`btnIgnore`被点击时，会在设置中标记该插件为不启用，并同样从`watchDog`设置和消息栏
      中移除相关记录和消息条目。
      
      6. **更新设置**：最后，无论插件是否之前被禁用，都会在设置中更新插件的状态。如果插件之前被检测到崩溃过，则会被标记为不启用，直到用户
      显式地选择重新启用它。
      
      */
      const QString baseName = QFileInfo( myFullPath ).baseName();
      //  如果在“watchDog”中找到了想要的插件，那么说明QGIS在加载这个插件的时候发生了问题，然后需要经过一些处理
      if ( mySettings.value( QStringLiteral( "Plugins/watchDog/%1" ).arg( baseName ) ).isValid() )
      {
        QToolButton *btnEnablePlugin = new QToolButton();
        btnEnablePlugin ->setText( QObject::tr( "Enable Plugin" ) );
        btnEnablePlugin ->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        QToolButton *btnIgnore = new QToolButton();
        btnIgnore->setText( QObject::tr( "Ignore" ) );
        btnIgnore->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        QgsMessageBarItem *watchdogMsg = new QgsMessageBarItem(
          QObject::tr( "Plugin %1" ).arg( baseName ),
          QObject::tr( "This plugin is disabled because it previously crashed QGIS." ),
          btnEnablePlugin,
          Qgis::MessageLevel::Warning,
          0,
          mQgisInterface->messageBar() );
        watchdogMsg->layout()->addWidget( btnIgnore );

        QObject::connect( btnEnablePlugin, &QToolButton::clicked, mQgisInterface->messageBar(), [ = ]()
        {
          QgsSettings settings;
          settings.setValue( "/Plugins/" + baseName, true );
          loadCppPlugin( myFullPath );
          settings.remove( QStringLiteral( "/Plugins/watchDog/%1" ).arg( baseName ) );
          mQgisInterface->messageBar()->popWidget( watchdogMsg );
        } );
        QObject::connect( btnIgnore, &QToolButton::clicked, mQgisInterface->messageBar(), [ = ]()
        {
          QgsSettings settings;
          settings.setValue( "/Plugins/" + baseName, false );
          settings.remove( "/Plugins/watchDog/" + baseName );
          mQgisInterface->messageBar()->popWidget( watchdogMsg );
        } );

        mQgisInterface->messageBar()->pushItem( watchdogMsg );
        mySettings.setValue( "/Plugins/" + baseName, false );
      }
      //  如果在mySettings中找到了想要的插件，那么将这个对应的插件加载到QGIS中
      if ( mySettings.value( "/Plugins/" + baseName ).toBool() )
      {
        mySettings.setValue( QStringLiteral( "Plugins/watchDog/%1" ).arg( baseName ), true );
        loadCppPlugin( myFullPath );
        mySettings.remove( QStringLiteral( "/Plugins/watchDog/%1" ).arg( baseName ) );
      }
    }
  }

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    // check for python plugins system-wide
    const QStringList pluginList = mPythonUtils->pluginList();
    QgsDebugMsgLevel( QStringLiteral( "Loading python plugins" ), 2 );
    QgsDebugMsgLevel( QStringLiteral( "Python plugins will be loaded in the following order: " ) + pluginList.join( "," ), 2 );

    QStringList corePlugins = QStringList();
    corePlugins << QStringLiteral( "GdalTools" );
    corePlugins << QStringLiteral( "db_manager" );
    corePlugins << QStringLiteral( "processing" );
    corePlugins << QStringLiteral( "MetaSearch" );
    corePlugins << QStringLiteral( "sagaprovider" );
    corePlugins << QStringLiteral( "grassprovider" );

    // make the required core plugins enabled by default:
    const auto constCorePlugins = corePlugins;
    for ( const QString &corePlugin : constCorePlugins )
    {
      if ( !mySettings.contains( "/PythonPlugins/" + corePlugin ) )
      {
        mySettings.setValue( "/PythonPlugins/" + corePlugin, true );
      }
    }

    const auto constPluginList = pluginList;
    for ( const QString &packageName : constPluginList )
    {
      // TODO: apply better solution for #5879
      // start - temporary fix for issue #5879
      if ( QgsApplication::isRunningFromBuildDir() )
      {
        if ( corePlugins.contains( packageName ) )
        {
          QgsApplication::setPkgDataPath( QString() );
        }
        else
        {
          QgsApplication::setPkgDataPath( QgsApplication::buildSourcePath() );
        }
      }
      // end - temporary fix for issue #5879, more below


      if ( mySettings.value( "/PythonPlugins/watchDog/" + packageName ).isValid() )
      {
        QToolButton *btnEnablePlugin = new QToolButton();
        btnEnablePlugin->setText( QObject::tr( "Enable Plugin" ) );
        btnEnablePlugin->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        QToolButton *btnIgnore = new QToolButton();
        btnIgnore->setText( QObject::tr( "Ignore" ) );
        btnIgnore->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        QgsMessageBarItem *watchdogMsg = new QgsMessageBarItem(
          QObject::tr( "Plugin %1" ).arg( packageName ),
          QObject::tr( "This plugin is disabled because it previously crashed QGIS." ),
          btnEnablePlugin,
          Qgis::MessageLevel::Warning,
          0,
          mQgisInterface->messageBar() );
        watchdogMsg->layout()->addWidget( btnIgnore );

        QObject::connect( btnEnablePlugin, &QToolButton::clicked, mQgisInterface->messageBar(), [ = ]()
        {
          QgsSettings settings;
          settings.setValue( "/PythonPlugins/" + packageName, true );
          if ( checkPythonPlugin( packageName ) )
          {
            loadPythonPlugin( packageName );
          }
          settings.remove( "/PythonPlugins/watchDog/" + packageName );

          mQgisInterface->messageBar()->popWidget( watchdogMsg );
        } );

        QObject::connect( btnIgnore, &QToolButton::clicked, mQgisInterface->messageBar(), [ = ]()
        {
          QgsSettings settings;
          settings.setValue( "/PythonPlugins/" + packageName, false );
          settings.remove( "/PythonPlugins/watchDog/" + packageName );
          mQgisInterface->messageBar()->popWidget( watchdogMsg );
        } );

        mQgisInterface->messageBar()->pushItem( watchdogMsg );

        mySettings.setValue( "/PythonPlugins/" + packageName, false );
      }
      // check if the plugin was active on last session
      if ( mySettings.value( "/PythonPlugins/" + packageName ).toBool() )
      {
        mySettings.setValue( "/PythonPlugins/watchDog/" + packageName, true );
        if ( checkPythonPlugin( packageName ) )
        {
          loadPythonPlugin( packageName );
        }
        mySettings.remove( "/PythonPlugins/watchDog/" + packageName );

      }
    }
    // start - temporary fix for issue #5879, more above
    if ( QgsApplication::isRunningFromBuildDir() )
    {
      QgsApplication::setPkgDataPath( QgsApplication::buildOutputPath() + QStringLiteral( "/data" ) );
    }
    // end - temporary fix for issue #5879
  }
#endif

  QgsDebugMsgLevel( QStringLiteral( "Plugin loading completed" ), 2 );
}

#pragma endregion

bool QgsPluginRegistry::checkCppPlugin( const QString &pluginFullPath )
{
  QLibrary myLib( pluginFullPath );
  const bool loaded = myLib.load();
  if ( ! loaded )
  {
    QgsMessageLog::logMessage( QObject::tr( "Failed to load %1 (Reason: %2)" ).arg( myLib.fileName(), myLib.errorString() ), QObject::tr( "Plugins" ) );
    return false;
  }

  name_t *myName = ( name_t * ) cast_to_fptr( myLib.resolve( "name" ) );
  description_t   *myDescription = ( description_t * )  cast_to_fptr( myLib.resolve( "description" ) );
  category_t   *myCategory = ( category_t * )  cast_to_fptr( myLib.resolve( "category" ) );
  version_t   *myVersion = ( version_t * ) cast_to_fptr( myLib.resolve( "version" ) );

  if ( myName && myDescription && myVersion  && myCategory )
    return true;

  QgsDebugMsgLevel( "Failed to get name, description, category or type for " + myLib.fileName(), 2 );
  return false;
}


bool QgsPluginRegistry::checkPythonPlugin( const QString &packageName )
{
#ifdef WITH_BINDINGS
  QString pluginName, description, /*category,*/ version;

  // get information from the plugin
  // if there are some problems, don't continue with metadata retrieval
  pluginName  = mPythonUtils->getPluginMetadata( packageName, QStringLiteral( "name" ) );
  description = mPythonUtils->getPluginMetadata( packageName, QStringLiteral( "description" ) );
  version     = mPythonUtils->getPluginMetadata( packageName, QStringLiteral( "version" ) );
  // for Python plugins category still optional, by default used "Plugins" category
  //category = mPythonUtils->getPluginMetadata( packageName, "category" );

  if ( pluginName == QLatin1String( "__error__" ) || description == QLatin1String( "__error__" ) || version == QLatin1String( "__error__" ) )
  {
    QgsMessageLog::logMessage( QObject::tr( "Error when reading metadata of plugin %1" ).arg( packageName ),
                               QObject::tr( "Plugins" ) );
    return false;
  }

  return true;
#else
  Q_UNUSED( packageName )
  return false;
#endif
}

bool QgsPluginRegistry::isPythonPluginCompatible( const QString &packageName ) const
{
#ifdef WITH_BINDINGS
  const QString minVersion = mPythonUtils->getPluginMetadata( packageName, QStringLiteral( "qgisMinimumVersion" ) );
  // try to read qgisMaximumVersion. Note checkQgisVersion can cope with "__error__" value.
  const QString maxVersion = mPythonUtils->getPluginMetadata( packageName, QStringLiteral( "qgisMaximumVersion" ) );
  return minVersion != QLatin1String( "__error__" ) && checkQgisVersion( minVersion, maxVersion );
#else
  Q_UNUSED( packageName )
  return false;
#endif
}

QList<QgsPluginMetadata *> QgsPluginRegistry::pluginData()
{
  QList<QgsPluginMetadata *> resultList;
  QMap<QString, QgsPluginMetadata>::iterator it = mPlugins.begin();
  for ( ; it != mPlugins.end(); ++it )
  {
    resultList.push_back( &( it.value() ) );
  }
  return resultList;
}
