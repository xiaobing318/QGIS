/***************************************************************************
                             main.cpp
                             --------
    begin                : 2019-02-25
    copyright            : (C) 2019 Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma region "包含头文件"

/***************************QT******************************/
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTimer>

/***************************STL******************************/
#include <cstdio>
#include <cstdlib>

/***************************QGIS******************************/
#include "qgsapplication.h"
#include "qgsprocess.h"
#include "qgsproviderregistry.h"

#ifdef Q_OS_WIN
#include <fcntl.h> /*  _O_BINARY */
#else
#include <getopt.h>
#endif

#ifdef Q_OS_MACX
#include <ApplicationServices/ApplicationServices.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
typedef SInt32 SRefCon;
#endif
#endif

#pragma endregion

#pragma region "取消和重定义一些宏"
#undef QgsDebugCall
#undef QgsDebugMsg
#undef QgsDebugMsgLevel
#define QgsDebugCall
#define QgsDebugMsg(str)
#define QgsDebugMsgLevel(str, level)
#pragma endregion

#pragma region "定义在仅本文件中可见的两个全局变量"
/*
* 杨小兵-2024-03-22
### 代码详解
- `static QString myProjectFileName;`
  这一行声明了一个静态的`QString`变量`myProjectFileName`。在C++中，`static`关键字在这里的用途是限制变量的作用域为本文件内，这意味着`myProjectFileName`
只在定义它的文件内可见，即使它是全局变量。`QString`是Qt框架中用于存储文本字符串的类。`myProjectFileName`用于存储一个将要自动加载的QGIS项目文件的名称。

- `static QStringList sFileList;`
  这行代码声明了另一个静态的变量`sFileList`，类型为`QStringList`，也是Qt中的一种类型，用于存储字符串的列表。这个变量用来收集命令行参数中的“剩余”参数，即
那些不直接作为选项处理的参数，通常这些是文件路径。

- 接着指出这两个变量是全局的，这样做的原因是它们可以被AppleEvent处理程序以及主程序（通过处理`argv`参数）设置。在macOS上，AppleEvent是操作系统用于应用程序
之间通信的机制，允许应用程序响应如打开文档这样的请求。

- 第一个变量`myProjectFileName`的作用是控制QGIS自动加载一个项目。这意味着，如果通过命令行或其他方式（如AppleEvent）指定了一个项目文件名，QGIS将在启动时自
动打开这个项目。

- 第二个变量`sFileList`用于收集那些不被直接识别为命令行选项的参数，这些通常是文件路径。这使得用户可以在命令行中指定一个或多个要在QGIS中打开的文件，这些文件
路径会被收集到`sFileList`中，然后由QGIS处理。
  总的来说，这段代码通过定义两个静态全局变量，为QGIS提供了一种机制，使其能够在启动时根据提供的命令行参数或其他输入自动加载项目文件和处理其他文件。

*/
/////////////////////////////////////////////////////////////////
// Command line options 'behavior' flag setup
////////////////////////////////////////////////////////////////

// These two are global so that they can be set by the OpenDocuments
// AppleEvent handler as well as by the main routine argv processing

// This behavior will cause QGIS to autoload a project
static QString myProjectFileName;

// This is the 'leftover' arguments collection
static QStringList sFileList;

#pragma endregion

int main( int argc, char *argv[] )
{
#pragma region "设置文件读取模式：文本模式、二进制模式"
/*
* 杨小兵-2024-03-22
一、解释

### 条件编译指令解释
- `#ifdef Q_OS_WIN`：这个预处理器指令检查是否定义了`Q_OS_WIN`宏。这通常在Qt框架中用来判断当前编译环境是否为Windows平台。如果是，编译器会进入这个条件编译块
内部。
- `#ifdef _MSC_VER`：在Windows平台的条件编译块内，这个指令检查是否定义了`_MSC_VER`宏。`_MSC_VER`是Microsoft Visual C++编译器特有的宏，用于标识正在使用
Microsoft Visual C++编译器。如果代码是在Visual C++环境下编译的，编译器将执行`_set_fmode( _O_BINARY );`语句。
- `_set_fmode( _O_BINARY );`：这个函数调用设置全局文件模式为二进制模式。在二进制模式下，文件将原封不动地读写，不会对数据进行任何转换（例如，不会将换行符
`\n`转换为回车换行符`\r\n`）。
- `#else`：如果不是在Visual C++环境下编译，即使用的是MinGW等其他Windows平台的编译器，编译器将执行`_fmode = _O_BINARY;`语句。
- `_fmode = _O_BINARY;`：这行代码直接将全局变量`_fmode`设置为二进制模式，其效果与`_set_fmode( _O_BINARY );`相同。不同之处在于它是直接对全局变量进行赋值，
而不是通过函数调用实现。
- `#endif`：这些`#endif`指令结束了相应的条件编译块。

### `_set_fmode`函数
  `_set_fmode`函数是一个C运行时库函数，用于设置全局文件模式。函数原型通常如下：
```c
int _set_fmode(int mode);
```
  其中，`mode`参数指定了新的文件模式。在这个例子中，`_O_BINARY`是传给`_set_fmode`函数的参数，表示将文件模式设置为二进制模式。成功调用`_set_fmode`函数会影
响随后打开的所有文件的模式。在Windows平台上，默认的文件模式可能是文本模式，在文本模式下，当读写文件时，运行时会自动将`\n`转换为`\r\n`，并在读取时执行相反的转
换。将文件模式设置为二进制模式禁用了这种转换，这对处理非文本文件（如图像或二进制数据文件）至关重要，确保数据不会因为这种转换而损坏。

二、总结
1、采用二进制模式读取这种方式对处理非文本文件（如图像或二进制数据文件）至关重要，确保数据不会因为这种转换而损坏
2、`_set_fmode`函数是一个C运行时库函数，用于设置全局文件模式
*/
#ifdef Q_OS_WIN  // Windows
#ifdef _MSC_VER
  _set_fmode( _O_BINARY );
#else //MinGW
  _fmode = _O_BINARY;
#endif  // _MSC_VER
#endif  // Q_OS_WIN

#pragma endregion

/*
* 杨小兵-2024-03-22
### 初始化`QgsApplication`
```cpp
QgsApplication app(argc, argv, false, QString(), QStringLiteral("qgis_process"));
```
  这行代码创建了一个`QgsApplication`对象，它是QGIS应用程序的核心。这个对象管理应用程序的生命周期和设置。
- `argc`和`argv`是传递给`main`函数的参数，用于处理命令行输入。
- 第三个参数`false`指示应用程序不是GUI类型。这对于命令行工具或后台服务是必要的（命令行工具和后台服务）。
- 第四个参数（空的`QString()`）暂时未使用，可以指定GUI配置文件的路径。
- 最后一个参数`"qgis_process"`是应用程序的名称。

### 设置应用程序的前缀路径
```cpp
QString myPrefixPath;
if (myPrefixPath.isEmpty())
{
  QDir dir(QCoreApplication::applicationDirPath());
  dir.cdUp();
  myPrefixPath = dir.absolutePath();
}
QgsApplication::setPrefixPath(myPrefixPath, true);
```
  这段代码确定并设置QGIS的安装前缀路径。这个路径是QGIS查找其资源（如插件、配置等）的基础目录。如果`myPrefixPath`是空的，代码就计算出应用程序的目录，
然后向上一级，将这个路径设置为QGIS的前缀路径。

### 设置QSettings环境
```cpp
QgsApplication::setOrganizationName(QStringLiteral("QGIS"));
QgsApplication::setOrganizationDomain(QStringLiteral("qgis.org"));
QgsApplication::setApplicationName(QStringLiteral("QGIS3"));
```
  这些行配置了`QSettings`对象，用于存储和检索应用程序设置（比如配置选项和偏好）。它们定义了组织名称、组织域和应用程序名称，这对于确保应用程序设置的正确
存储和访问非常重要。

### 初始化QGIS应用程序
```cpp
QgsApplication::init();
QgsApplication::initQgis();
```
调用`init`和`initQgis`函数来完成QGIS应用程序的初始化工作。这包括加载QGIS资源、插件等。

### 插件路径和参数解析
```cpp
QgsProviderRegistry::instance(QgsApplication::pluginPath());
( void ) QgsApplication::resolvePkgPath();
const QStringList args = QCoreApplication::arguments();
```
- `QgsProviderRegistry::instance()`初始化插件注册表，这对于QGIS来说是必须的，因为许多功能都是通过插件提供的。
- `QgsApplication::resolvePkgPath()`确保应用程序路径被解析和存储，这对于后续寻找资源路径很重要。
- `QCoreApplication::arguments()`解析命令行参数。

### 执行QGIS处理
```cpp
QgsProcessingExec exec;
int res = 0;
QTimer::singleShot(0, &app, [&exec, args, &res]
{
  res = exec.run(args);
  QgsApplication::exitQgis();
  QCoreApplication::exit(res);
});
return QgsApplication::exec();
```
  这段代码使用`QgsProcessingExec`对象执行GIS处理任务。通过`QTimer::singleShot`延迟执行确保事件循环已启动。`exec.run(args)`根据命令行参数执行处理任务。
完成后，它调用`QgsApplication::exitQgis`来清理QGIS资源，并通过`QCoreApplication::exit`退出应用程序，返回处理结果。

*/
  QgsApplication app( argc, argv, false, QString(), QStringLiteral( "qgis_process" ) );
  QString myPrefixPath;
  if ( myPrefixPath.isEmpty() )
  {
    QDir dir( QCoreApplication::applicationDirPath() );
    dir.cdUp();
    myPrefixPath = dir.absolutePath();
  }
  QgsApplication::setPrefixPath( myPrefixPath, true );

  // Set up the QSettings environment must be done after qapp is created
  QgsApplication::setOrganizationName( QStringLiteral( "QGIS" ) );
  QgsApplication::setOrganizationDomain( QStringLiteral( "qgis.org" ) );
  QgsApplication::setApplicationName( QStringLiteral( "QGIS3" ) );

  QgsApplication::init();
  QgsApplication::initQgis();

  QgsProviderRegistry::instance( QgsApplication::pluginPath() );

  ( void ) QgsApplication::resolvePkgPath(); // trigger storing of application path in QgsApplication

  // Build a local QCoreApplication from arguments. This way, arguments are correctly parsed from their native locale
  // It will use QString::fromLocal8Bit( argv ) under Unix and GetCommandLine() under Windows.
  const QStringList args = QCoreApplication::arguments();

  QgsProcessingExec exec;
  int res = 0;
  QTimer::singleShot( 0, &app, [&exec, args, &res]
  {
    res = exec.run( args );
    QgsApplication::exitQgis();
    QCoreApplication::exit( res );
  } );
  return QgsApplication::exec();
}
