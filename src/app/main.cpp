/***************************************************************************
                            main.cpp  -  description
                              -------------------
              begin                : Fri Jun 21 10:48:28 AKDT 2002
              copyright            : (C) 2002 by Gary E.Sherman
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
#pragma region "包含一些需要用到的头文件"

#pragma region "一、包含QT框架中的一些头文件"
/*
* 杨小兵-2024-02-20
  这些头文件属于Qt框架，一个跨平台的C++图形用户界面应用程序开发框架，广泛用于开发具有图形界面的应用程序，也可以用于开发非图形界面的程序，
如控制台工具和服务器。
总结：
1、	这些头文件都是属于Qt框架的
2、	Qt框架是一个开发框架
3、	Qt框架是用C++进行开发的框架
4、	Qt框架是跨平台的

- `QBitmap`: 用于处理位图图像，是`QPixmap`的子类，专门用于单色图像处理。
- `QDir`: 提供了操作目录结构和内容的方法。
- `QFile`: 用于文件的读写操作。
- `QFileInfo`: 提供了系统无关的文件信息。
- `QFont`: 用于字体的指定和使用。
- `QFontDatabase`: 提供了关于系统字体的信息。
- `QPixmap`: 用于处理图像数据，常用于在窗口部件上绘制图像。
- `QLocale`: 提供了国际化支持，包括货币、日期、时间的格式化。
- `QSplashScreen`: 用于应用程序启动时显示启动画面。
- `QString`: 用于字符串的处理。
- `QStringList`: `QString`对象的列表，用于处理字符串集合。
- `QStyle`: 用于定制应用程序或窗口部件的外观。
- `QStyleFactory`: 用于创建`QStyle`对象。
- `QImageReader`: 提供了读取图像文件的功能。
- `QMessageBox`: 显示一个模态对话框，用于显示信息、警告或错误等。
- `QStandardPaths`: 提供了访问标准路径（如文档、配置文件等）的方法。
- `QScreen`: 提供了关于计算机屏幕的信息和控制功能。

*/
//qt includes
#include <QBitmap>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QPixmap>
#include <QLocale>
#include <QSplashScreen>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>
#include <QImageReader>
#include <QMessageBox>
#include <QStandardPaths>
#include <QScreen>
#pragma endregion

#pragma region "二、包含系统头文件和QGIS配置文件"
/*
* 杨小兵-2024-02-20
  `<cstdio>`、`<cstdlib>`、`<cstdarg>` 属于C++标准库，用于输入输出、标准库函数和变长参数列表处理
总结：
1、	cstdio是C++标准库中用来输入输出的头文件
2、 cstdlib是C++标准库中用来使用一些函数的头文件
3、 cstdarg是C++标准库中用来实现变长参数列表的头文件
4、`qgsconfig.h`: 这是特定于项目的配置文件，通常包含了预编译的宏定义，用于控制软件的编译选项和功能特性
*/
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include "qgsconfig.h"
#pragma endregion

#pragma region "三、通过预编译指令包含不同的头文件以适应不同操作系统平台的要求"
/*
* 杨小兵-2024-02-20
  这段代码通过预处理指令来控制编译时包含的头文件，以适应不同操作系统平台的需求。预处理指令`#if`、`#ifdef`、
`#ifndef`和`#endif`用于条件编译，它们根据条件是否满足来决定是否编译包含在它们之间的代码

总结：
1、`#if`、`#ifdef`、`#ifndef`和`#endif`这些都是预处理指令，在不同的操作系统平台下编译系统都是认识的
2、如果没有定义宏Q_OS_WIN（意味着当前不是Windows操作系统），则包含"sigwatch.h"
3、如果定义了宏WIN32（意味着当前是Windows操作系统），则包含相应的头文件


头文件说明：
  - `<fcntl.h>`: 包含文件控制选项，比如`_O_BINARY`，用于设置文件的打开模式为二进制模式。
  - `<windows.h>`: Windows应用程序的核心头文件，提供了大量的Windows API函数声明、宏定义等。
  - `<dbghelp.h>`: 提供了用于Windows程序调试的函数和结构体，如堆栈回溯、符号处理等。
  - `<time.h>`: 提供了日期和时间处理的函数。
  - `<getopt.h>`: 提供了解析命令行选项的函数，这在Unix-like系统中很常用。
*/
#if !defined(Q_OS_WIN)
#include "sigwatch.h"
#endif

#ifdef WIN32
#include <fcntl.h> /*  _O_BINARY */
#include <windows.h>
#include <dbghelp.h>
#include <time.h>
#else
#include <getopt.h>
#endif

#ifdef Q_OS_MACX
#include <ApplicationServices/ApplicationServices.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
typedef SInt32 SRefCon;
#endif
#endif

#ifdef Q_OS_UNIX
// For getrlimit() / setrlimit()
#include <sys/resource.h>
#include <sys/time.h>
#endif

/*
* 杨小兵-2024-02-20
  `#if defined(__GLIBC__) || defined(__FreeBSD__)`: 在确认需要崩溃处理器后，这一行进一步检查编译环境是否为使用GNU C Library（glibc）的系统
（如大多数Linux发行版）或FreeBSD。这是因为接下来要使用的功能或头文件可能只在这些环境中可用或兼容。

头文件说明：
   - `<unistd.h>`: 提供了对POSIX操作系统API的访问，如进程控制、文件操作等。
   - `<execinfo.h>`: 提供了执行栈回溯工具，可以用于崩溃时获取函数调用栈。
   - `<csignal>`: 提供了信号处理机制，用于处理例如段错误等崩溃信号。
   - `<sys/wait.h>`: 提供了进程等待的函数，用于同步进程状态。
   - `<cerrno>`: 提供了通过错误码报告系统调用错误的机制。
*/
#ifdef HAVE_CRASH_HANDLER
#if defined(__GLIBC__) || defined(__FreeBSD__)
#define QGIS_CRASH
#include <unistd.h>
#include <execinfo.h>
#include <csignal>
#include <sys/wait.h>
#include <cerrno>
#endif
#endif

/*
* 杨小兵-2024-02-20
  每个头文件通常声明了特定的类或函数，用于QGIS应用程序的不同方面。这些头文件涵盖了QGIS软件的多个方面，包括用户界面定制、设置管理、
地图渲染、项目管理、数据导入导出、

头文件说明：
1. **`qgscustomization.h`**: 定义了QGIS中的定制化功能，允许用户或开发者定制UI元素，如菜单、工具栏等。

2. **`qgssettings.h`**: 提供了一个接口来存取应用程序设置，如用户偏好、项目参数等。

3. **`qgsfontutils.h`**: 包含字体相关的实用函数，如加载字体、管理字体库等。

4. **`qgsmessagelog.h`**: 为应用程序提供了一个消息日志系统，用于记录和显示警告、错误消息等。

5. **`qgspythonrunner.h`**: 提供了执行Python脚本的功能，允许在QGIS中运行Python代码。

6. **`qgslocalec.h`**: 与本地化和国际化相关的功能，帮助管理语言设置和转换。

7. **`qgisapp.h`**: 定义了QGIS主应用程序的类，是QGIS用户界面的核心。

8. **`qgsapplication.h`**: 扩展了Qt的QApplication类，添加了QGIS特定的初始化和清理功能。

9. **`qgsconfig.h`**: 包含了编译时定义的宏，通常用于控制编译选项和条件编译。

10. **`qgsversion.h`**: 提供了QGIS版本信息。

11. **`qgsproject.h`**: 管理QGIS项目文件，包括加载、保存项目等功能。

12. **`qgsrectangle.h`**: 定义了一个矩形类，通常用于地图画布中的区域表示。

13. **`qgslogger.h`**: 提供了日志记录功能，用于调试和记录程序运行时的信息。

14. **`qgsdxfexport.h`**: 提供了将地图数据导出为DXF格式文件的功能。

15. **`qgsvectorlayer.h`**: 定义了矢量图层的类，是处理地图上矢量数据的基础。

16. **`qgis_app.h`**: 通常是QGIS应用程序的启动点，包含main函数。

17. **`qgscrashhandler.h`**: 如果定义了HAVE_CRASH_HANDLER，这个文件提供了崩溃处理功能，用于捕获和报告崩溃。

18. **`qgsziputils.h`**: 提供了处理ZIP文件的工具函数。

19. **`qgsversionmigration.h`**: 管理QGIS版本升级过程中的数据迁移。

20. **`qgsfirstrundialog.h`**: 第一次运行QGIS时显示的初始化对话框。

21. **`qgsproxystyle.h`**: 定义了一个代理样式类，用于自定义QWidget的外观。

22. **`qgsmessagebar.h`**: 提供了一个消息条，用于在QGIS界面中显示临时消息和警告。

23. **`qgsuserprofilemanager.h`** 和 **`qgsuserprofile.h`**: 管理用户配置文件，包括保存用户的UI布局、设置等。

24. **`layers/qgsapplayerhandling.h`**: 与图层操作有关的高级API函数，可能涉及图层添加、移除和管理。

25. **`qgsopenclutils.h`**: 如果定义了HAVE_OPENCL，这个文件提供了OpenCL（一个用于在各种GPU和CPU上执行并行编程的框架）工具函数和类。
*/
#include "qgscustomization.h"
#include "qgssettings.h"
#include "qgsfontutils.h"
#include "qgsmessagelog.h"
#include "qgspythonrunner.h"
#include "qgslocalec.h"
#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsconfig.h"
#include "qgsversion.h"
#include "qgsproject.h"
#include "qgsrectangle.h"
#include "qgslogger.h"
#include "qgsdxfexport.h"
#include "qgsvectorlayer.h"
#include "qgis_app.h"
#ifdef HAVE_CRASH_HANDLER
#include "qgscrashhandler.h"
#endif
#include "qgsziputils.h"
#include "qgsversionmigration.h"
#include "qgsfirstrundialog.h"
#include "qgsproxystyle.h"
#include "qgsmessagebar.h"

#include "qgsuserprofilemanager.h"
#include "qgsuserprofile.h"
#include "layers/qgsapplayerhandling.h"

#ifdef HAVE_OPENCL
#include "qgsopenclutils.h"
#endif
#pragma endregion

#pragma endregion

#pragma region "输出版本信息函数、输出命令行使用帮助函数、检测是否通过在Mac OS X上双击应用程序包来启动的，而不是通过命令行函数、dumpBacktrace函数"
/*
解释：
  `QStringLiteral`是一个宏，用于在编译时创建一个`QString`对象，从而避免运行时的动态分配和字符编码转换的开销。
这种机制通过直接在编译时将字符串字面量转换为`QString`的UTF-16表示，能够提高字符串处理的效率，特别是在字符串常量频繁使用的场合。
`QStringLiteral`不是一个函数，而是一个宏。它在预处理阶段展开，用于创建`QString`对象。与函数调用不同，宏在编译时展开，没有运行时开销。

总结：
1、QStringLiteral是一个宏
2、在编译时创建一个QString的对象，而不是在运行时动态的分配
3、`QStringLiteral`虽然用于生成`QString`对象，但它本身不属于`QString`类。它是一个宏，作为Qt框架提供的一个工具来在编译时优化`QString`对象的创建。
4、这部分内容类似于帮助文档的说明
*/
/**
 * Print QGIS version
 */
void version( )
{
  const QString msg = QStringLiteral( "QGIS %1 '%2' (%3)\n" ).arg( VERSION ).arg( RELEASE_NAME ).arg( QGSVERSION );
  std::cout << msg.toStdString();
}

/**
 * Print usage text
 */
void usage( const QString &appName )
{
  QStringList msg;

  msg
      << QStringLiteral( "QGIS is a user friendly Open Source Geographic Information System.\n" )
      << QStringLiteral( "Usage: " ) << appName <<  QStringLiteral( " [OPTION] [FILE]\n" )
      << QStringLiteral( "  OPTION:\n" )
      << QStringLiteral( "\t[-v, --version]\tdisplay version information and exit\n" )
      << QStringLiteral( "\t[-s, --snapshot filename]\temit snapshot of loaded datasets to given file\n" )
      << QStringLiteral( "\t[-w, --width width]\twidth of snapshot to emit\n" )
      << QStringLiteral( "\t[-h, --height height]\theight of snapshot to emit\n" )
      << QStringLiteral( "\t[-l, --lang language]\tuse language for interface text (changes existing override)\n" )
      << QStringLiteral( "\t[-p, --project projectfile]\tload the given QGIS project\n" )
      << QStringLiteral( "\t[-e, --extent xmin,ymin,xmax,ymax]\tset initial map extent\n" )
      << QStringLiteral( "\t[-n, --nologo]\thide splash screen\n" )
      << QStringLiteral( "\t[-V, --noversioncheck]\tdon't check for new version of QGIS at startup\n" )
      << QStringLiteral( "\t[-P, --noplugins]\tdon't restore plugins on startup\n" )
      << QStringLiteral( "\t[-B, --skipbadlayers]\tdon't prompt for missing layers\n" )
      << QStringLiteral( "\t[-C, --nocustomization]\tdon't apply GUI customization\n" )
      << QStringLiteral( "\t[-z, --customizationfile path]\tuse the given ini file as GUI customization\n" )
      << QStringLiteral( "\t[-g, --globalsettingsfile path]\tuse the given ini file as Global Settings (defaults)\n" )
      << QStringLiteral( "\t[-a, --authdbdirectory path] use the given directory for authentication database\n" )
      << QStringLiteral( "\t[-f, --code path]\trun the given python file on load\n" )
      << QStringLiteral( "\t[-F, --py-args arguments]\targuments for python. This arguments will be available for each python execution via 'sys.argv' included the file specified by '--code'. All arguments till '--' are passed to python and ignored by QGIS\n" )
      << QStringLiteral( "\t[-d, --defaultui]\tstart by resetting user ui settings to default\n" )
      << QStringLiteral( "\t[--hide-browser]\thide the browser widget\n" )
      << QStringLiteral( "\t[--dxf-export filename.dxf]\temit dxf output of loaded datasets to given file\n" )
      << QStringLiteral( "\t[--dxf-extent xmin,ymin,xmax,ymax]\tset extent to export to dxf\n" )
      << QStringLiteral( "\t[--dxf-symbology-mode none|symbollayer|feature]\tsymbology mode for dxf output\n" )
      << QStringLiteral( "\t[--dxf-scale-denom scale]\tscale for dxf output\n" )
      << QStringLiteral( "\t[--dxf-encoding encoding]\tencoding to use for dxf output\n" )
      << QStringLiteral( "\t[--dxf-map-theme maptheme]\tmap theme to use for dxf output\n" )
      << QStringLiteral( "\t[--take-screenshots output_path]\ttake screen shots for the user documentation\n" )
      << QStringLiteral( "\t[--screenshots-categories categories]\tspecify the categories of screenshot to be used (see QgsAppScreenShots::Categories).\n" )
      << QStringLiteral( "\t[--profile name]\tload a named profile from the users profiles folder.\n" )
      << QStringLiteral( "\t[-S, --profiles-path path]\tpath to store user profile folders. Will create profiles inside a {path}\\profiles folder \n" )
      << QStringLiteral( "\t[--version-migration]\tforce the settings migration from older version if found\n" )
#ifdef HAVE_OPENCL
      << QStringLiteral( "\t[--openclprogramfolder]\t\tpath to the folder containing the sources for OpenCL programs.\n" )
#endif
      << QStringLiteral( "\t[--help]\t\tthis text\n" )
      << QStringLiteral( "\t[--]\t\ttreat all following arguments as FILEs\n\n" )
      << QStringLiteral( "  FILE:\n" )
      << QStringLiteral( "    Files specified on the command line can include rasters, vectors,\n" )
      << QStringLiteral( "    QGIS layer definition files (.qlr) and QGIS project files (.qgs and .qgz): \n" )
      << QStringLiteral( "     1. Rasters - supported formats include GeoTiff, DEM \n" )
      << QStringLiteral( "        and others supported by GDAL\n" )
      << QStringLiteral( "     2. Vectors - supported formats include ESRI Shapefiles\n" )
      << QStringLiteral( "        and others supported by OGR and PostgreSQL layers using\n" )
      << QStringLiteral( "        the PostGIS extension\n" )  ; // OK

/*
* 杨小兵-2024-02-20
解释：
  在Windows平台上，使用`MessageBox`函数来显示一个消息框。`MessageBox`是Windows API的一部分，用于弹出一个包含指定文本和标题的对话框
这个表达式将`msg`字符串列表中的所有字符串合并为一个单独的字符串，并转换为本地8位编码（通常是系统的默认编码）。`constData()`返回指向编码后数据的指针
总结：
1、MessageBox是Windows API的一个函数
2、这个函数的作用就是用来显示一个对话框
3、`msg.join( QString() )`使用空的`QString`作为分隔符来合并`msg`列表中的字符串，实际上就是将它们直接拼接在一起
4、`toLocal8Bit()`将`QString`对象转换为本地8位编码的字节数组，这个编码通常依赖于系统的区域设置或语言设置
5、`constData()`返回一个指向字节数组内部数据的常量指针，适用于C风格的字符串操作
*/
#ifdef Q_OS_WIN
  MessageBox( nullptr,
              msg.join( QString() ).toLocal8Bit().constData(),
              "QGIS command line options",
              MB_OK );
#else
  std::cout << msg.join( QString() ).toLocal8Bit().constData();
#endif

} // usage()

#pragma region "设置两个全局变量"
/*
* 杨小兵-2024-02-20
一、解释：
  这段代码是设置命令行选项的一部分，主要用于一个软件项目（如QGIS，一个开源地理信息系统软件）中。从这段代码可以看出，
它设计了两个全局变量：`sProjectFileName`和`sFileList`。这两个变量用于存储通过命令行或其他方式（如AppleEvent）传递给程序的参数。

二、全局变量解释：
1. **`sProjectFileName`**：这是一个静态的`QString`类型的变量，用于存储项目文件名。这个变量的存在意味着程序可以通过命令行参数自动
加载一个指定的项目文件。在GIS软件中，项目文件通常包含了地图视图、图层设置、符号系统等一系列配置信息，使得用户能够保存和恢复工作环境。

2. **`sFileList`**：这是一个静态的`QStringList`类型的变量，用于收集不直接关联到特定命令行选项的参数。这些“剩余”的参数可能是文件名
或其他数据，它们不直接触发程序的特定行为，但可能会在程序运行过程中被使用。例如，在QGIS中，这些参数可能用于打开额外的数据文件或图层。

*/
/////////////////////////////////////////////////////////////////
// Command line options 'behavior' flag setup
////////////////////////////////////////////////////////////////

// These two are global so that they can be set by the OpenDocuments
// AppleEvent handler as well as by the main routine argv processing

// This behavior will cause QGIS to autoload a project
static QString sProjectFileName;

// This is the 'leftover' arguments collection
static QStringList sFileList;
#pragma endregion

/*
* 杨小兵-2024-02-20
一、解释
  这段代码和注释是用来检测一个程序是否是通过在Mac OS X上双击应用程序包来启动的，而不是通过命令行。这种检测对于应用程序的启动行为处理非常有用，
特别是在处理命令行参数时。这里的关键点是理解Mac OS X特有的启动参数，以及如何区分命令行启动和图形界面启动
二、总结
1、用来在Mac OS上检测是否通过双击打开QGIS
2、这部分内容不用关注
*/
/* Test to determine if this program was started on Mac OS X by double-clicking
 * the application bundle rather then from a command line. If clicked, argv[1]
 * contains a process serial number in the form -psn_0_1234567. Don't process
 * the command line arguments in this case because argv[1] confuses the processing.
 */
bool bundleclicked( int argc, char *argv[] )
{
  return ( argc > 1 && memcmp( argv[1], "-psn_", 5 ) == 0 );
}

void myPrint( const char *fmt, ... )
{
  va_list ap;
  va_start( ap, fmt );
#if defined(Q_OS_WIN)
  char buffer[1024];
  vsnprintf( buffer, sizeof buffer, fmt, ap );
  OutputDebugString( buffer );
#else
  vfprintf( stderr, fmt, ap );
#endif
  va_end( ap );
}

static void dumpBacktrace( unsigned int depth )
{
  if ( depth == 0 )
    depth = 20;

#ifdef QGIS_CRASH
  // Below there is a bunch of operations that are not safe in multi-threaded
  // environment (dup()+close() combo, wait(), juggling with file descriptors).
  // Maybe some problems could be resolved with dup2() and waitpid(), but it seems
  // that if the operations on descriptors are not serialized, things will get nasty.
  // That's why there's this lovely mutex here...
  static QMutex sMutex;
  QMutexLocker locker( &sMutex );

  int stderr_fd = -1;
  if ( access( "/usr/bin/c++filt", X_OK ) < 0 )
  {
    myPrint( "Stacktrace (c++filt NOT FOUND):\n" );
  }
  else
  {
    int fd[2];

    if ( pipe( fd ) == 0 && fork() == 0 )
    {
      close( STDIN_FILENO ); // close stdin

      // stdin from pipe
      if ( dup( fd[0] ) != STDIN_FILENO )
      {
        QgsDebugMsg( QStringLiteral( "dup to stdin failed" ) );
      }

      close( fd[1] );        // close writing end
      execl( "/usr/bin/c++filt", "c++filt", static_cast< char * >( nullptr ) );
      perror( "could not start c++filt" );
      exit( 1 );
    }

    myPrint( "Stacktrace (piped through c++filt):\n" );
    stderr_fd = dup( STDERR_FILENO );
    close( fd[0] );          // close reading end
    close( STDERR_FILENO );  // close stderr

    // stderr to pipe
    int stderr_new = dup( fd[1] );
    if ( stderr_new != STDERR_FILENO )
    {
      if ( stderr_new >= 0 )
        close( stderr_new );
      QgsDebugMsg( QStringLiteral( "dup to stderr failed" ) );
    }

    close( fd[1] );  // close duped pipe
  }

  void **buffer = new void *[ depth ];
  int nptrs = backtrace( buffer, depth );
  backtrace_symbols_fd( buffer, nptrs, STDERR_FILENO );
  delete [] buffer;
  if ( stderr_fd >= 0 )
  {
    int status;
    close( STDERR_FILENO );
    int dup_stderr = dup( stderr_fd );
    if ( dup_stderr != STDERR_FILENO )
    {
      close( dup_stderr );
      QgsDebugMsg( QStringLiteral( "dup to stderr failed" ) );
    }
    close( stderr_fd );
    wait( &status );
  }
#elif defined(Q_OS_WIN)
  // TODO Replace with incoming QgsStackTrace
#else
  Q_UNUSED( depth )
#endif
}

#pragma endregion

#pragma region "如果定义了QGIS_CRASH这个宏，那么将会定义名为qgisCrash的函数"
/*
* 杨小兵-2024-02-20
一、解释
  下列这段代码是预处理命令检查是否定义了QGIS_CRASH宏，如果定义了QGIS_CRASH这个宏，那么将会定义名为qgisCrash的函数，这个预处理命令应该
是为了在非windows环境中处理QGIS崩溃时堆栈的调用情况，这里先不用进行处理
二、总结

*/
#ifdef QGIS_CRASH
void qgisCrash( int signal )
{
  fprintf( stderr, "QGIS died on signal %d", signal );

  QgsCrashHandler::handle( 0 );

  if ( access( "/usr/bin/gdb", X_OK ) == 0 )
  {
    // take full stacktrace using gdb
    // http://stackoverflow.com/questions/3151779/how-its-better-to-invoke-gdb-from-program-to-print-its-stacktrace
    // unfortunately, this is not so simple. the proper method is way more OS-specific
    // than this code would suggest, see http://stackoverflow.com/a/1024937

    char exename[512];
#if defined(__FreeBSD__)
    int len = readlink( "/proc/curproc/file", exename, sizeof( exename ) - 1 );
#else
    int len = readlink( "/proc/self/exe", exename, sizeof( exename ) - 1 );
#endif
    if ( len < 0 )
    {
      myPrint( "Could not read link (%d: %s)\n", errno, strerror( errno ) );
    }
    else
    {
      exename[ len ] = 0;

      char pidstr[32];
      snprintf( pidstr, sizeof pidstr, "--pid=%d", getpid() );

      int gdbpid = fork();
      if ( gdbpid == 0 )
      {
        // attach, backtrace and continue
        execl( "/usr/bin/gdb", "gdb", "-q", "-batch", "-n", pidstr, "-ex", "thread", "-ex", "bt full", exename, NULL );
        perror( "cannot exec gdb" );
        exit( 1 );
      }
      else if ( gdbpid >= 0 )
      {
        int status;
        waitpid( gdbpid, &status, 0 );
        myPrint( "gdb returned %d\n", status );
      }
      else
      {
        myPrint( "Cannot fork (%d: %s)\n", errno, strerror( errno ) );
        dumpBacktrace( 256 );
      }
    }
  }

  abort();
}
#endif

#pragma endregion

#pragma region "拦截和处理来自`libpng`库的警告和错误消息"
/*
* 杨小兵-2024-02-20
一、解释
  主要目的是拦截和处理来自`libpng`库的警告和错误消息。`libpng`是一个广泛使用的库，用于处理PNG（便携式网络图形）格式的图像文件。
这些消息通常涉及图像处理过程中的问题，比如文件损坏、不支持的格式特性等。特别提到了使用JPL WMS（Jet Propulsion Laboratory Web Map Service）
图像时遇到的问题。JPL WMS是一种网络服务，提供从卫星获取的地球图像。注释中提到，某些JPL WMS图像在使用libpng版本1.2.2处理时会导致问题，
尤其是在图像放大（缩放）的情况下，这可能导致最终的图像显示为空白。
  提到了基于Qt文档中的`qInstallMsgHandler`示例代码来实现这一功能。`qInstallMsgHandler`是一个Qt框架的功能，允许开发者自定义消息处理函数，
以拦截和处理Qt应用程序运行时产生的各种消息，包括警告、错误等。这表明开发者通过设置自定义的消息处理器，将`libpng`的消息引导至用户界面或日志系统，
而不是让它们在控制台或日志文件中默默丢失。
二、总结
1、libpng是一个第三方库
2、libpng这个库的作用就是用来处理PNG格式的图像文件数据
3、PNG：便携式网络图形
4、使用JPL WMS图像是将会用到的问题：某些JPL WMS图像在使用libpng版本1.2.2处理时会出现问题（这种问题现在处理不了，退一步的解决方式是给出提示信息）
5、JPL WMS是一种网络服务，提供从卫星获取的地球图像

*/
/*
 * Hook into the qWarning/qFatal mechanism so that we can channel messages
 * from libpng to the user.
 *
 * Some JPL WMS images tend to overload the libpng 1.2.2 implementation
 * somehow (especially when zoomed in)
 * and it would be useful for the user to know why their picture turned up blank
 *
 * Based on qInstallMsgHandler example code in the Qt documentation.
 *
 */
void myMessageOutput( QtMsgType type, const QMessageLogContext &, const QString &msg )
{
  switch ( type )
  {
    case QtDebugMsg:
      myPrint( "%s\n", msg.toLocal8Bit().constData() );
      if ( msg.startsWith( QLatin1String( "Backtrace" ) ) )
      {
        const QString trace = msg.mid( 9 );
        dumpBacktrace( atoi( trace.toLocal8Bit().constData() ) );
      }
      break;
    case QtCriticalMsg:
      myPrint( "Critical: %s\n", msg.toLocal8Bit().constData() );

#ifdef QGISDEBUG
      dumpBacktrace( 20 );
#endif

      break;
    case QtWarningMsg:
    {
      /* Ignore:
       * - libpng iCPP known incorrect SRGB profile errors (which are thrown by 3rd party components
       *  we have no control over and have low value anyway);
       * - QtSVG warnings with regards to lack of implementation beyond Tiny SVG 1.2
       */
      if ( msg.contains( QLatin1String( "QXcbClipboard" ), Qt::CaseInsensitive ) ||
           msg.startsWith( QLatin1String( "libpng warning: iCCP: known incorrect sRGB profile" ), Qt::CaseInsensitive ) ||
           msg.contains( QLatin1String( "Could not add child element to parent element because the types are incorrect" ), Qt::CaseInsensitive ) ||
           msg.contains( QLatin1String( "OpenType support missing for" ), Qt::CaseInsensitive ) )
        break;

      myPrint( "Warning: %s\n", msg.toLocal8Bit().constData() );

#ifdef QGISDEBUG
      // Print all warnings except setNamedColor.
      // Only seems to happen on windows
      if ( !msg.startsWith( QLatin1String( "QColor::setNamedColor: Unknown color name 'param" ), Qt::CaseInsensitive )
           && !msg.startsWith( QLatin1String( "Trying to create a QVariant instance of QMetaType::Void type, an invalid QVariant will be constructed instead" ), Qt::CaseInsensitive )
           && !msg.startsWith( QLatin1String( "Logged warning" ), Qt::CaseInsensitive ) )
      {
        // TODO: Verify this code in action.
        dumpBacktrace( 20 );

        // also be super obnoxious -- we DON'T want to allow these errors to be ignored!!
        if ( QgisApp::instance() && QgisApp::instance()->messageBar() && QgisApp::instance()->thread() == QThread::currentThread() )
        {
          QgisApp::instance()->messageBar()->pushCritical( QStringLiteral( "Qt" ), msg );
        }
        else
        {
          QgsMessageLog::logMessage( msg, QStringLiteral( "Qt" ) );
        }
      }
#endif

      // TODO: Verify this code in action.
      if ( msg.startsWith( QLatin1String( "libpng error:" ), Qt::CaseInsensitive ) )
      {
        // Let the user know
        QgsMessageLog::logMessage( msg, QStringLiteral( "libpng" ) );
      }

      break;
    }

    case QtFatalMsg:
    {
      myPrint( "Fatal: %s\n", msg.toLocal8Bit().constData() );
#ifdef QGIS_CRASH
      qgisCrash( -1 );
#else
      dumpBacktrace( 256 );
      abort();                    // deliberately dump core
#endif
      break; // silence warnings
    }

    case QtInfoMsg:
      myPrint( "Info: %s\n", msg.toLocal8Bit().constData() );
      break;
  }
}
#pragma endregion

#pragma region "根据不同的编译器设置宏（导出函数的宏定义）"
/*
* 杨小兵-2024-02-20
一、解释
1、#ifdef _MSC_VER检查是否定义了`_MSC_VER`宏，这个宏是Microsoft Visual C++编译器特有的，用于标识正在使用Visual C++编译器
2、__declspec(dllexport)这是Visual C++特有的语法，用于声明导出到DLL（动态链接库）的函数、类、变量等。这样，使用`APP_EXPORT`标记的符号会被导出，
使得其他模块或程序可以链接和使用这些符号
3、`#if defined(ANDROID) || defined(Q_OS_WIN)`：检查是否定义了`ANDROID`或`Q_OS_WIN`宏。这两个宏分别用于标识编译目标平台为Android或Windows
二、总结
1、注释解释了为什么需要导出某些符号？
2、Android---->在Android上，使用`libqgis.so`动态库而不是`qgis`可执行文件。因此，需要导出库的`main`方法符号，以便Java代码可以调用它
3、Windows---->在Windows上，`main`方法包含在`qgis_app`中，并从`mainwin.cpp`调用
*/
#ifdef _MSC_VER
#undef APP_EXPORT
#define APP_EXPORT __declspec(dllexport)
#endif

#if defined(ANDROID) || defined(Q_OS_WIN)
// On Android, there there is a libqgis.so instead of a qgis executable.
// The main method symbol of this library needs to be exported so it can be called by java
// On Windows this main is included in qgis_app and called from mainwin.cpp
APP_EXPORT
#endif
#pragma endregion

int main( int argc, char *argv[] )
{
  //log messages written before creating QgsApplication
  QStringList preApplicationLogMessages;

#pragma region "如果是unix操作系统，将会修改当前进程的文件资源信息以提升可以打开的文件资源"
/*
* 杨小兵-2024-02-20
一、解释
  这段代码的目的是在Unix系统上增加文件资源限制，即允许打开的文件数量的上限。这通常是针对那些需要同时处理大量文件的应用程序，如大型数据库系统或文件服务器。

  `#ifdef Q_OS_UNIX`: 这行代码是一个预处理指令，用于检查是否定义了`Q_OS_UNIX`宏。如果定义了，说明代码正在Unix系统上编译，接下来的代码块将被执行。
这是确保只在Unix系统上尝试修改文件打开限制的方法。这段代码通过修改进程的资源限制来提高其可打开文件的数量，这对于需要处理大量文件的应用程序是非常有用的。
通过提高软限制，应用程序可以打开更多的文件而不会遇到系统限制错误。然而，这种修改是有上限的，即不能超过系统规定的硬限制，确保系统的稳定性不会因单个进程
的贪婪而受到威胁

二、总结
1、如果定义了Q_OS_UNIX宏，那么说明代码将会再unix操作系统上进行编译，预处理指令代码块中的代码将会被执行
2、这段代码将会在unix操作系统上生效（被编译系统编译），目的是为了增大文件资源
*/
#ifdef Q_OS_UNIX
  // Increase file resource limits (i.e., number of allowed open files)
  // (from code provided by Larry Biehl, Purdue University, USA, from 'MultiSpec' project)
  // This is generally 256 for the soft limit on Mac
  // NOTE: setrlimit() must come *before* initialization of stdio strings,
  //       e.g. before any debug messages, or setrlimit() gets ignored
  // see: http://stackoverflow.com/a/17726104/2865523
  struct rlimit rescLimit;
  if ( getrlimit( RLIMIT_NOFILE, &rescLimit ) == 0 )
  {
    const rlim_t oldSoft( rescLimit.rlim_cur );
#ifdef OPEN_MAX
    rlim_t newSoft( OPEN_MAX );
#else
    rlim_t newSoft( 4096 );
#endif
    const char *qgisMaxFileCount = getenv( "QGIS_MAX_FILE_COUNT" );
    if ( qgisMaxFileCount )
      newSoft = static_cast<rlim_t>( atoi( qgisMaxFileCount ) );
    if ( rescLimit.rlim_cur < newSoft || qgisMaxFileCount )
    {
      rescLimit.rlim_cur = std::min( newSoft, rescLimit.rlim_max );

      if ( setrlimit( RLIMIT_NOFILE, &rescLimit ) == 0 )
      {
        QgsDebugMsg( QStringLiteral( "RLIMIT_NOFILE Soft NEW: %1 / %2" )
                     .arg( rescLimit.rlim_cur ).arg( rescLimit.rlim_max ) );
      }
    }
    Q_UNUSED( oldSoft ) //avoid warnings
    QgsDebugMsg( QStringLiteral( "RLIMIT_NOFILE Soft/Hard ORIG: %1 / %2" )
                 .arg( oldSoft ).arg( rescLimit.rlim_max ) );
  }
#endif
#pragma endregion

/*
* 杨小兵-2024-02-20
一、解释
  `QgsDebugMsg`是一个在QGIS项目中用于输出调试信息的函数，`QgsDebugMsg`函数通常用于在开发过程中输出调试信息。其中QgsDebugMsg(str) do {} while(false)
在C++中是一个常见的技巧，用于在宏定义中安全地执行单个语句。结合起来理解，`QgsDebugMsg(str) do {} while(false)`这种形式实际上是为了确保在宏定义中调用
`QgsDebugMsg`函数时，即使后面没有分号，也能保证代码的正确性和完整性。这种写法可以避免在宏展开后产生的语法问题，比如在`if`语句中使用宏时可能会遇到的问题。
*/
  QgsDebugMsg( QStringLiteral( "Starting qgis main" ) );

#pragma region "为了能够在windows平台上对如图像或可执行文件的读写设置文件的读写模式"
/*
* 杨小兵-2024-02-20
一、解释
  如果定义了宏WIN32（意味着当前是Windows操作系统），并且编译器是_MSC_VER类型的则执行代码块中的内容。这段代码主要用于Windows平台，
用于设置文件模式为二进制模式。在Windows操作系统中，文件可以以文本模式或二进制模式打开。文本模式和二进制模式的主要区别在于如何处理文件中的换行符。
  `_set_fmode( _O_BINARY );`: 当使用Microsoft Visual C++编译器时，这行代码会被执行。`_set_fmode`函数用于设置全局文件模式。`_O_BINARY`标
志指示所有文件默认以二进制模式打开，这意味着文件将按字节精确读写，不会对换行符进行特殊处理。
  如果没有定义`_MSC_VER`宏，代码会跳到这个分支，这通常意味着代码正在使用MinGW（Minimalist GNU for Windows）编译器。在使用MinGW编译器时，通过
直接设置`_fmode`全局变量为`_O_BINARY`来改变文件的默认打开模式为二进制模式。和`_set_fmode`函数相比，这是一种更直接的方法，但效果相同。

二、函数解释
  是一个Microsoft Visual C++提供的函数，用于设置进程级别的默认文件打开模式。这个函数接受一个参数来指定文件模式，例如`_O_BINARY`，使得所有文件
默认以二进制模式打开。在二进制模式下，文件数据不会进行任何转换，这对于非文本文件（如图像或可执行文件）的读写是必要的。

三、总结
1、_set_fmode这个函数是用来设置[进程]级别的默认文件打开方式
2、这样设置的原因是为了能够对如图像或可执行文件的读写
*/
#ifdef WIN32  // Windows
#ifdef _MSC_VER
  _set_fmode( _O_BINARY );
#else //MinGW
  _fmode = _O_BINARY;
#endif  // _MSC_VER
#endif  // WIN32
#pragma endregion

#pragma region "根据不同的平台、编译器设置错误处理函数、异常捕获函数、QT的一些设置"
/*
* 杨小兵-2024-02-20
一、解释
  - `#ifndef ANDROID`：这是一个预处理指令，用于检查`ANDROID`宏是否未定义。如果代码不是在Android平台上编译，那么条件编译的代码块将会执行。
这意味着自定义消息处理器只在非Android平台上安装。这个函数调用安装了一个自定义的消息处理器`myMessageOutput`。`myMessageOutput`函数是一个
用户定义的函数，它能够接收应用程序运行时产生的各种消息，包括警告、调试信息等，并对这些消息进行自定义处理。

1、- `signal(SIGQUIT, qgisCrash);`等：这些调用设置了一个信号处理函数`qgisCrash`来处理不同类型的信号。信号是操作系统发送给程序的通知，
表明发生了某些特定的系统事件。这些事件通常包括错误情况，如非法指令（`SIGILL`）、浮点异常（`SIGFPE`）、无效内存访问（`SIGSEGV`）等，
它们通常会导致程序崩溃。
2、- `qgisCrash`函数是一个用户定义的函数，它在接收到上述信号时被调用，允许程序在崩溃前执行特定的操作，如清理资源、保存状态、记录错误信息等。

二、总结
1、信号是操作系统发送给程序的通知，表明发生了某些特定的系统事件
2、信号的类型有SIGQUIT、SIGILL等等
3、qgisCrash是一个用户自定义的函数，在这个文件的前面已经实现过了
*/
  // Set up the custom qWarning/qDebug custom handler
#ifndef ANDROID
  qInstallMessageHandler( myMessageOutput );
#endif

#ifdef QGIS_CRASH
  signal( SIGQUIT, qgisCrash );
  signal( SIGILL, qgisCrash );
  signal( SIGFPE, qgisCrash );
  signal( SIGSEGV, qgisCrash );
  signal( SIGBUS, qgisCrash );
  signal( SIGSYS, qgisCrash );
  signal( SIGTRAP, qgisCrash );
  signal( SIGXCPU, qgisCrash );
  signal( SIGXFSZ, qgisCrash );
#endif

/*
* 杨小兵-2024-02-20
一、解释
  上述代码段是用于在Windows平台上配置异常处理策略的一部分，特别是在使用Microsoft Visual C++ (MSVC) 编译器时。
这是通过预处理指令和`SetUnhandledExceptionFilter`函数实现的。`SetUnhandledExceptionFilter(QgsCrashHandler::handle);`
这个函数调用安装了一个未处理异常的自定义处理函数，即`QgsCrashHandler::handle`。当程序遇到未被捕获的异常时，这个处理函数将被调用。

二、总结
1、当程序遇到未被捕获的异常时，这个处理函数将被调用，通过Windows.h中的函数SetUnhandledExceptionFilter来实现
2、`SetUnhandledExceptionFilter`是Windows API的一部分，用于设置一个应用程序的顶级异常过滤函数，该函数会在异常
未被任何异常处理代码（如`try-catch`块）捕获时调用。

参数
- **LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter**：指向异常处理函数的指针。这个函数必须遵循
`TOP_LEVEL_EXCEPTION_FILTER`函数类型的签名，即接受一个指向`EXCEPTION_POINTERS`结构体的指针作为参数，并返回一个`LONG`值。

返回值
- **LPTOP_LEVEL_EXCEPTION_FILTER**：函数返回一个指向之前设置的异常过滤函数的指针。如果之前没有设置过异常处理函数，
则返回`NULL`。如果函数调用失败，也返回`NULL`。

*/
#ifdef _MSC_VER
  SetUnhandledExceptionFilter( QgsCrashHandler::handle );
#endif

/*
* 杨小兵-2024-02-20
1、QT版本低于6.0.0的时候，需要初始化随机种子，而QT版本6.0.0则是不需要的
2、qsrand是QT框架中的一个函数，用来初始化随机种子
*/
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  // initialize random number seed - not required for Qt 6
  qsrand( time( nullptr ) );
#endif
#pragma endregion

#pragma region "注释解释:解析命令行参数以确定用户是否请求了特殊行为，以及处理这些请求的方法"
/*
* 杨小兵-2024-02-20
一、解释
  这段注释描述了QGIS的特定功能，即解析命令行参数以确定用户是否请求了特殊行为，以及处理这些请求的方法。
这一部分介绍了代码的主要目的，即设置命令行选项以定义应用程序的行为。这些"behavior"标志是根据用户通过命令行输入的参数来设置的，
它们指示应用程序以某种特定方式运行。
  应用程序开始通过分析命令行参数来查找用户是否请求了任何特殊行为。这通常涉及检查每个命令行参数，以判断它们是否匹配预定义的选项或标志，
从而决定应用程序的行为模式。
  用户可以通过命令行参数请求应用程序执行特定的操作或以特定的模式运行。例如，用户可能想要应用程序在启动时加载一个特定的地图层、
执行一个快照操作、将地图保存为图像文件，然后退出。
  在解析命令行参数并识别出特殊行为请求之后，剩余的非命令行参数（即那些不对应任何特定行为标志的参数）将被保留。这些参数可能代表
需要被加载的图层或项目文件。应用程序将这些参数作为一个列表处理，用于后续操作，例如加载相应的地图层或项目。
  注释中提到的一个具体的行为示例是，应用程序根据命令行参数启动，自动加载指定的地图或项目，然后捕获（快照）当前地图的状态，将这个
状态保存为图像文件到磁盘上，完成这些操作后立即退出。这种模式对于需要自动化地图生成和导出的场景非常有用，比如在批处理操作或自动化测试中。
*/
  /////////////////////////////////////////////////////////////////
  // Command line options 'behavior' flag setup
  ////////////////////////////////////////////////////////////////

  //
  // Parse the command line arguments, looking to see if the user has asked for any
  // special behaviors. Any remaining non command arguments will be kept aside to
  // be passed as a list of layers and / or a project that should be loaded.
  //

  // This behavior is used to load the app, snapshot the map,
  // save the image to disk and then exit
#pragma endregion

#pragma region "定义各种类型的QGIS程序中需要用到的变量（其中包括对命令行参数的解析）"
  QString mySnapshotFileName;
  QString configLocalStorageLocation;
  QString profileName;
  int mySnapshotWidth = 800;
  int mySnapshotHeight = 600;

  bool myHideSplash = false;
  bool settingsMigrationForce = false;
  bool mySkipVersionCheck = false;
  bool hideBrowser = false;
#if defined(ANDROID)
  QgsDebugMsg( QStringLiteral( "Android: Splash hidden" ) );
  myHideSplash = true;
#endif

  bool myRestoreDefaultWindowState = false;
  bool myRestorePlugins = true;
  bool mySkipBadLayers = false;
  bool myCustomization = true;

  QString dxfOutputFile;
  QgsDxfExport::SymbologyExport dxfSymbologyMode = QgsDxfExport::SymbolLayerSymbology;
  double dxfScale = 50000.0;
  QString dxfEncoding = QStringLiteral( "CP1252" );
  QString dxfMapTheme;
  QgsRectangle dxfExtent;

  bool takeScreenShots = false;
  QString screenShotsPath;
  int screenShotsCategories = 0;

  /*
  * 杨小兵-2024-02-20
  一、解释
    讨论了QGIS应用程序在启动时如何根据不同条件设置地图画布的初始显示范围（即地图的初始视图或范围）。
  注释指出，该行为（或功能）的目的是在QGIS启动时设置地图画布的初始范围。地图画布的初始范围是用户首
  次打开QGIS时看到的地图区域的大小和位置。这一功能仅在没有命令行参数提供给QGIS时触发。命令行参数通
  常用于启动应用程序时指定一些启动选项，比如直接加载特定的项目或图层。这意味着，如果用户通过命令行
  启动QGIS并指定了特定的行为（如加载特定图层），该功能不会调整地图画布的初始范围。当QGIS启动时没有
  加载任何图层（即用户没有通过命令行参数指定加载图层），这个功能会设置一个“可用的”地图范围。这样做
  是为了确保即使在没有加载任何地图数据的情况下，用户也能得到一个有意义的、默认的地图视图。这个默认
  范围可能基于某个预设的地理范围，以便用户看到一个初始的地图画面，而不是一个空白画布。注释还指出，
  当有图层被加载时，QGIS将不使用这个默认的初始范围，而是让加载的图层定义地图画布的初始范围。这意
  味着，如果用户启动QGIS时通过命令行参数或其他方式加载了一个或多个图层，QGIS会自动调整地图画布的
  范围，以便完整显示所有加载的图层。这种行为确保了用户在启动应用程序时立即看到的是他们期望加载的数据。

  二、总结
  1、如果命令行参数没有提供给QGIS，那么将会设置地图画布的初始显示范围
  2、如果命令行参数提供给了QGIS，那么将不会设置地图画布的初始显示范围
  3、如果没有加载任何地图数据，那么将会设置地图画布的初始显示范围
  4、如果加载一个或者多个图层，那么QGIS会自动调整地图画布的范围
  */
  // This behavior will set initial extent of map canvas, but only if
  // there are no command line arguments. This gives a usable map
  // extent when qgis starts with no layers loaded. When layers are
  // loaded, we let the layers define the initial extent.
  QString myInitialExtent;
  if ( argc == 1 )
    myInitialExtent = QStringLiteral( "-1,-1,1,1" );

  // This behavior will allow you to force the use of a translation file
  // which is useful for testing
  QString translationCode;

  // The user can specify a path which will override the default path of custom
  // user settings (~/.qgis) and it will be used for QgsSettings INI file
  QString authdbdirectory;

  QString pythonfile;
  QStringList pythonArgs;

  QString customizationfile;
  QString globalsettingsfile;

#ifdef HAVE_OPENCL
  QString openClProgramFolder;
#endif

// TODO Fix android
#if defined(ANDROID)
  QgsDebugMsg( QStringLiteral( "Android: All params stripped" ) );// Param %1" ).arg( argv[0] ) );
  //put all QGIS settings in the same place
  QString configpath = QgsApplication::qgisSettingsDirPath();
  QgsDebugMsg( QStringLiteral( "Android: configpath set to %1" ).arg( configpath ) );
#endif

#pragma region "如果是命令行启动的QGIS，那么需要对命令行参数进行解析处理"
  /*
  * 杨小兵-2024-02-20
  一、解释
    主要目的是解析启动应用程序时传入的命令行参数，根据这些参数执行不同的操作或设置不同的配置。`QCoreApplication`是Qt框架中的一个类，用于管理应用程序级别的资源和设置。它提供了事件循
  环的管理，使得应用程序可以处理事件（如键盘输入、鼠标事件等），并且它是非GUI应用程序的基础。在跨平台的Qt应用程序中如何处理命令行参数，以确保这些参数能够根据操作系统的本地化设置正确
  解析。在不同的操作系统中，命令行参数的编码方式可能会有所不同，因此需要特别注意以确保应用程序可以正确理解这些参数。注释指出，通过构建一个`QCoreApplication`实例并传入`argc`和`argv`
  ，Qt框架能够自动地、正确地根据操作系统的本地化设置解析命令行参数。这意味着无论参数是使用何种本地字符集编码，应用程序都能正确理解它们。

  - **在Unix系统下**，Qt使用`QString::fromLocal8Bit(argv)`来转换命令行参数。这个函数将字节字符串（`argv`中的内容）转换为`QString`对象，
  这是Qt用于处理Unicode文本的类。这样做的目的是确保在Unix系统（通常使用UTF-8或其他本地化编码）下，命令行参数能够正确地转换为内部使用的Unicode表示。
  - **在Windows系统下**，Qt内部使用`GetCommandLine()`函数来获取命令行参数。`GetCommandLine()`是Windows API的一部分，返回一个表示命令行
  输入的Unicode字符串。这种方法确保了在Windows系统下，即使命令行参数包含非ASCII字符（如中文、日文等），也能被应用程序正确解析。

    - **为什么使用这个函数**：通过使用`QCoreApplication::arguments()`而不是直接操作`argv`数组，开发者可以避免直接处理字符编码问题，让Qt框架
  自动根据不同操作系统的特性来正确解析和处理这些参数。这样，无论应用程序运行在何种语言环境的操作系统上，都能保证命令行参数的正确解析和使用。

  二、总结
  1、传入的不同命令行参数将会有不同的配置信息
  2、QCoreApplication是QT框架中的一个类，是非GUI应用程序的基础
  3、构建一个QCoreApplication，然后传入`argc`和`argv`两个参数，这两个参数可以被QT框架中QCoreApplication正确处理
  */
  QStringList args;

  {
    /*
    * 杨小兵-2024-03-26
    一、解释
    1、- **在Unix/Linux系统下**：命令行参数通常是按照系统的本地编码（比如UTF-8）传递的。`QCoreApplication::arguments()`内部会使用`QString::fromLocal8Bit()`
    方法将这些参数转换成`QString`列表，`QString`是Qt中用于存储文本的类，它内部使用Unicode（UTF-16）编码。这一转换确保了无论参数使用何种本地编码，应用程序都能正
    确地读取和处理这些参数。
    2、- **在Windows系统下**：命令行参数可以通过Windows API `GetCommandLine()`函数获取，该函数返回一个包含整个命令行的字符串。`QCoreApplication`内部会处理这
    个字符串，并分割它为单独的参数，同时考虑到参数可能包含空格的情况（这通常通过将参数包含在引号中来解决）。然后，它将这些参数转换为`QString`列表，同样保证了参数的
    正确解析和使用。

    二、总结
      通过这种方式，`QCoreApplication`提供了一个跨平台且便捷的方法来处理命令行参数，确保了参数在不同操作系统下的一致解析和表示，使得开发跨平台应用程序时，能够更加
    专注于应用逻辑而非底层的编码和解析差异。考虑到跨平台的因素，这里QT提供了内部的方式可以更好的处理参数信息，而不需要进行分情况处理，这样有利于使用者更加关注上层逻辑
    而不是底层转化原理。
    */  
    QCoreApplication coreApp( argc, argv );
    // trigger storing of application path in QgsApplication（这个函数在出错的情况也不会退出程序）
    ( void ) QgsApplication::resolvePkgPath(); 

    if ( !bundleclicked( argc, argv ) )
    {
      // Build a local QCoreApplication from arguments. This way, arguments are correctly parsed from their native locale
      // It will use QString::fromLocal8Bit( argv ) under Unix and GetCommandLine() under Windows.
      args = QCoreApplication::arguments();

      for ( int i = 1; i < args.size(); ++i )
      {
        const QString &arg = args[i];

        if ( arg == QLatin1String( "--help" ) || arg == QLatin1String( "-?" ) )
        {
          usage( args[0] );
          return EXIT_SUCCESS;
        }
        else if ( arg == QLatin1String( "--version" ) || arg == QLatin1String( "-v" ) )
        {
          version();
          return EXIT_SUCCESS;
        }
        else if ( arg == QLatin1String( "--nologo" ) || arg == QLatin1String( "-n" ) )
        {
          myHideSplash = true;
        }
        else if ( arg == QLatin1String( "--version-migration" ) )
        {
          settingsMigrationForce = true;
        }
        else if ( arg == QLatin1String( "--noversioncheck" ) || arg == QLatin1String( "-V" ) )
        {
          mySkipVersionCheck = true;
        }
        else if ( arg == QLatin1String( "--noplugins" ) || arg == QLatin1String( "-P" ) )
        {
          myRestorePlugins = false;
        }
        else if ( arg == QLatin1String( "--skipbadlayers" ) || arg == QLatin1String( "-B" ) )
        {
          QgsDebugMsg( QStringLiteral( "Skipping bad layers" ) );
          mySkipBadLayers = true;
        }
        else if ( arg == QLatin1String( "--nocustomization" ) || arg == QLatin1String( "-C" ) )
        {
          myCustomization = false;
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--profile" ) ) )
        {
          profileName = args[++i];
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--profiles-path" ) || arg == QLatin1String( "-S" ) ) )
        {
          //  对于文件路径参数，代码使用`QDir::toNativeSeparators`来确保路径字符串符合当前操作系统的格式
          configLocalStorageLocation = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--snapshot" ) || arg == QLatin1String( "-s" ) ) )
        {
          mySnapshotFileName = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--width" ) || arg == QLatin1String( "-w" ) ) )
        {
          mySnapshotWidth = QString( args[++i] ).toInt();
        }
        else if ( arg == QLatin1String( "--hide-browser" ) )
        {
          hideBrowser = true;
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--height" ) || arg == QLatin1String( "-h" ) ) )
        {
          mySnapshotHeight = QString( args[++i] ).toInt();
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--lang" ) || arg == QLatin1String( "-l" ) ) )
        {
          translationCode = args[++i];
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--project" ) || arg == QLatin1String( "-p" ) ) )
        {
          const QString projectUri { args[++i] };
          const QFileInfo projectFileInfo { projectUri };
          if ( projectFileInfo.isFile() )
          {
            sProjectFileName = QDir::toNativeSeparators( projectFileInfo.absoluteFilePath() );
          }
          else
          {
            sProjectFileName = projectUri;
          }
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--extent" ) || arg == QLatin1String( "-e" ) ) )
        {
          myInitialExtent = args[++i];
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--authdbdirectory" ) || arg == QLatin1String( "-a" ) ) )
        {
          authdbdirectory = QDir::toNativeSeparators( QDir( args[++i] ).absolutePath() );
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--code" ) || arg == QLatin1String( "-f" ) ) )
        {
          pythonfile = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--py-args" ) || arg == QLatin1String( "-F" ) ) )
        {
          // Handle all parameters till '--' as code args
          for ( i++; i < args.size(); ++i )
          {
            if ( args[i] == QLatin1String( "--" ) )
            {
              i--;
              break;
            }
            pythonArgs << args[i];
          }
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--customizationfile" ) || arg == QLatin1String( "-z" ) ) )
        {
          customizationfile = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
        }
        else if ( i + 1 < argc && ( arg == QLatin1String( "--globalsettingsfile" ) || arg == QLatin1String( "-g" ) ) )
        {
          globalsettingsfile = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
        }
        else if ( arg == QLatin1String( "--defaultui" ) || arg == QLatin1String( "-d" ) )
        {
          myRestoreDefaultWindowState = true;
        }
        else if ( arg == QLatin1String( "--dxf-export" ) )
        {
          dxfOutputFile = args[++i];
        }
        else if ( arg == QLatin1String( "--dxf-extent" ) )
        {
          QgsLocaleNumC l;
          QString ext( args[++i] );
          QStringList coords( ext.split( ',' ) );

          if ( coords.size() != 4 )
          {
            std::cerr << "invalid dxf extent " << ext.toStdString() << std::endl;
            return 2;
          }

          for ( int i = 0; i < 4; i++ )
          {
            bool ok;
            double d;

            d = coords[i].toDouble( &ok );
            if ( !ok )
            {
              std::cerr << "invalid dxf coordinate " << coords[i].toStdString() << " in extent " << ext.toStdString() << std::endl;
              return 2;
            }

            switch ( i )
            {
              case 0:
                dxfExtent.setXMinimum( d );
                break;
              case 1:
                dxfExtent.setYMinimum( d );
                break;
              case 2:
                dxfExtent.setXMaximum( d );
                break;
              case 3:
                dxfExtent.setYMaximum( d );
                break;
            }
          }
        }
        else if ( arg == QLatin1String( "--dxf-symbology-mode" ) )
        {
          QString mode( args[++i] );
          if ( mode == QLatin1String( "none" ) )
          {
            dxfSymbologyMode = QgsDxfExport::NoSymbology;
          }
          else if ( mode == QLatin1String( "symbollayer" ) )
          {
            dxfSymbologyMode = QgsDxfExport::SymbolLayerSymbology;
          }
          else if ( mode == QLatin1String( "feature" ) )
          {
            dxfSymbologyMode = QgsDxfExport::FeatureSymbology;
          }
          else
          {
            std::cerr << "invalid dxf symbology mode " << mode.toStdString() << std::endl;
            return 2;
          }
        }
        else if ( arg == QLatin1String( "--dxf-scale-denom" ) )
        {
          bool ok;
          QString scale( args[++i] );
          dxfScale = scale.toDouble( &ok );
          if ( !ok )
          {
            std::cerr << "invalid dxf scale " << scale.toStdString() << std::endl;
            return 2;
          }
        }
        else if ( arg == QLatin1String( "--dxf-encoding" ) )
        {
          dxfEncoding = args[++i];
        }
        else if ( arg == QLatin1String( "--dxf-map-theme" ) )
        {
          dxfMapTheme = args[++i];
        }
        else if ( arg == QLatin1String( "--take-screenshots" ) )
        {
          takeScreenShots = true;
          screenShotsPath = args[++i];
        }
        else if ( arg == QLatin1String( "--screenshots-categories" ) )
        {
          screenShotsCategories = args[++i].toInt();
        }
#ifdef HAVE_OPENCL
        else if ( arg == QLatin1String( "--openclprogramfolder" ) )
        {
          openClProgramFolder = args[++i];
        }
#endif
        else if ( arg == QLatin1String( "--" ) )
        {
          for ( i++; i < args.size(); ++i )
            sFileList.append( QDir::toNativeSeparators( QFileInfo( args[i] ).absoluteFilePath() ) );
        }
        else
        {
          sFileList.append( QDir::toNativeSeparators( QFileInfo( args[i] ).absoluteFilePath() ) );
        }
      }
    }
  }

#pragma endregion

  //  如果QGIS的项目文件是空的，那么从命令行中寻找是否传入的参数存在*.qgs或者*.qgz
  /////////////////////////////////////////////////////////////////////
  // If no --project was specified, parse the args to look for a     //
  // .qgs file and set myProjectFileName to it. This allows loading  //
  // of a project file by clicking on it in various desktop managers //
  // where an appropriate mime-type has been set up.                 //
  /////////////////////////////////////////////////////////////////////
  if ( sProjectFileName.isEmpty() )
  {
    // check for a .qgs/z
    for ( int i = 0; i < args.size(); i++ )
    {
      QString arg = QDir::toNativeSeparators( QFileInfo( args[i] ).absoluteFilePath() );
      if ( arg.endsWith( QLatin1String( ".qgs" ), Qt::CaseInsensitive ) ||
           arg.endsWith( QLatin1String( ".qgz" ), Qt::CaseInsensitive ) )
      {
        sProjectFileName = arg;
        break;
      }
    }
  }

#pragma endregion


  /////////////////////////////////////////////////////////////////////
  // Now we have the handlers for the different behaviors...
  /////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////
  // Initialize the application and the translation stuff
  /////////////////////////////////////////////////////////////////////

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC) && !defined(ANDROID)
  bool myUseGuiFlag = nullptr != getenv( "DISPLAY" );
#else
  bool myUseGuiFlag = true;
#endif

  /*
  * 杨小兵-2024-02-20
  一、解释
    在Qt框架中，`QObject`类是许多Qt类的基类，提供了信号和槽机制以及Qt的对象模型，包括对象间的通信、事件处理等功能。
  然而，另一个重要特性是国际化（i18n）和本地化（l10n）支持。通过`QObject::tr()`方法，Qt支持在运行时根据用户的语言
  环境动态翻译文本字符串。这是为什么在Qt中处理用户可见的字符串时经常使用`QObject`而不是`QString`的原因。这表明，即
  使在非GUI的环境或模块中，也可以利用Qt的国际化支持。此外，即使这段代码看起来是在处理错误消息，也考虑到了将来可能对
  该消息进行本地化的需求。
  */
  if ( !myUseGuiFlag )
  {
    std::cerr << QObject::tr(
                "QGIS starting in non-interactive mode not supported.\n"
                "You are seeing this message most likely because you "
                "have no DISPLAY environment variable set.\n"
              ).toUtf8().constData();
    exit( 1 ); //exit for now until a version of qgis is capable of running non interactive
  }

  // GUI customization is enabled according to settings (loaded when instance is created)
  // we force disabled here if --nocustomization argument is used
  if ( !myCustomization )
  {
    QgsCustomization::instance()->setEnabled( false );
  }

  /*
  * 杨小兵-2024-02-20
  一、解释
    在Qt应用程序中进行一些基本的应用程序和组织设置的示例，以及根据Qt版本设置特定的应用程序属性。
  1、`QgsApplication::QGIS_ORGANIZATION_NAME`是一个静态成员，提供了QGIS应用程序所属组织的名称。这个设置在多个地方有用，
  比如在配置文件或注册表（Windows系统）中创建和管理设置时，可以将这些设置与特定的组织关联起来。
  2、`QCoreApplication::setOrganizationDomain( QgsApplication::QGIS_ORGANIZATION_DOMAIN );`
  - 设置应用程序的组织域名。与组织名称相似，这个信息用于唯一标识应用程序的开发者或发布者。在网络相关的功能中尤为重要，例如自动更新检查。
  3、- `QCoreApplication::setApplicationName( QgsApplication::QGIS_APPLICATION_NAME );`
  - 设置应用程序的名称。这个名称用于用户界面显示，并且也用于配置文件或注册表中的路径，以便于识别不同应用程序的设置。
  4、- `QCoreApplication::setAttribute( Qt::AA_DontShowIconsInMenus, false );`
  - 设置应用程序属性，以决定是否在菜单项旁边显示图标。这里设置为`false`，意味着允许在菜单项旁显示图标。这个设置可以根据应用程序的视觉设计需求来调整。
  5、- `QCoreApplication::setAttribute( Qt::AA_DisableWindowContextHelpButton, true );`
  - 这行代码在Qt版本低于6.0.0时被执行。它禁用了Windows系统窗口标题栏中的上下文帮助按钮（那个通常带有一个问号的按钮）。在Qt 6中，这个属性已经默认设置为`true`，所以这个条件编译指令确保了在旧版本的Qt中也不会显示上下文帮助按钮。

  */
  QCoreApplication::setOrganizationName( QgsApplication::QGIS_ORGANIZATION_NAME );
  QCoreApplication::setOrganizationDomain( QgsApplication::QGIS_ORGANIZATION_DOMAIN );
  QCoreApplication::setApplicationName( QgsApplication::QGIS_APPLICATION_NAME );
  QCoreApplication::setAttribute( Qt::AA_DontShowIconsInMenus, false);

  // this is implicit in Qt 6 now
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QCoreApplication::setAttribute( Qt::AA_DisableWindowContextHelpButton, true);
#endif
  /*
  * 杨小兵-2024-02-20
  一、解释
    在多线程环境中，一些插件可能依赖于Qt WebEngine模块进行图形渲染或者网页内容的展示。Qt WebEngine是一个基于Chromium的模块，
  用于在Qt应用中嵌入网页。为了提高渲染效率和资源共享，可能需要设置共享的OpenGL上下文。OpenGL是一个跨平台的图形API，用于2D和3D
  图形渲染。共享OpenGL上下文允许在不同的线程或插件间共享图形资源，如纹理和缓冲区，这对于性能优化尤为重要。这是因为，如注释中提
  到的，启用共享OpenGL上下文会破坏Qt 3D的正常工作。这种做法是一种临时的解决方案，直到相关的问题（如QTBUG-60614和QTBUG-60605）
  得到解决。

    OpenGL上下文是一个存储了所有OpenGL状态的对象，包括纹理、缓冲区、视口设置等。当我们说“共享OpenGL上下文”时，指的是允许多个上
  下文共享一部分OpenGL资源，如纹理、缓冲对象（Vertex Buffer Objects, VBOs）、帧缓冲对象（Frame Buffer Objects, FBOs）等。这
  意味着，一个上下文中创建的资源可以在其他上下文中被访问和修改，而无需重新创建或复制资源，从而提高了资源的重用性和应用程序的效率。

    OpenGL（Open Graphics Library）不是一个软件，而是一个规范（标准），由Khronos Group维护。它定义了一个跨平台的编程接口（API）
  用于开发2D和3D图形应用程序。虽然OpenGL自身是一个API规范，但存在多种实现，这些实现由不同的硬件制造商提供，以支持其图形硬件。
  因此，OpenGL可以看作是一个允许开发者在各种系统和设备上创建图形和视觉效果的平台和语言。

  二、总结
  1、Qt WebEngine模块：是基于Chromium的模块中的内容，这个模块的作用就是为了在QT应用中嵌入网页
  2、OpenGL是一个跨平台的图形API，用于2D和3D图形渲染(这个API的作用就是为了对2D和3D图形进行渲染)
  3、OpenGL上下文：一个存储了所有OpenGL状态的对象
  4、OpenGL是一个规范，存在多种不同的实现，可以在各种系统和设备上创建图形和视觉效果的平台和语言
  */
  // Set up an OpenGL Context to be shared between threads beforehand
  // for plugins that depend on Qt WebEngine module.
  // As suggested by Qt documentation at:
  //   - https://doc.qt.io/qt-5/qtwebengine.html
  //   - https://code.qt.io/cgit/qt/qtwebengine.git/plain/src/webenginewidgets/api/qtwebenginewidgetsglobal.cpp
#if 0
  // this is disabled, because it breaks Qt 3D. See
  // https://interest.qt-project.narkive.com/GYwuMDac/qwebengineview-qsurfaceformat-errors-in-console
  // https://bugreports.qt.io/browse/QTBUG-60614
  // https://bugreports.qt.io/browse/QTBUG-60605
#if !defined(QT_NO_OPENGL)
  QCoreApplication::setAttribute( Qt::AA_ShareOpenGLContexts, true );
#endif

#endif

#pragma region "QGIS应用程序的相关设置的存储位置：确定全局设置文件的位置、确定自定义设置存储文件位置，然后处理自定义用户配置文件"
  /*
  * 杨小兵-2024-02-20
  一、解释
    这段注释说明了设置`QgsSettings`全局设置的顺序和逻辑。`QgsSettings`是用于存储和管理QGIS（一个开源地理信息系统软件）
  应用设置的机制。注释描述了确定全局设置文件位置的步骤。
  1、- **首选项**：使用命令行参数`--globalsettingsfile`指定的路径。这允许用户或管理员指定一个自定义的配置文件路径，通常用于
  指向特定的配置文件，以便启动时加载。
  2、- **环境变量**：如果命令行参数未指定或指定的路径不存在，接下来会检查环境变量。这提供了一种灵活的方式，通过设置环境变量来
  指定配置文件的位置，适用于需要在不同环境中快速切换配置的情况。
  3、- **应用数据位置**：如果上述两种方法都未能确定配置文件的位置，将使用AppDataLocation，这是一个平台依赖的标准位置。
  在Linux上，这通常是`$HOME/.local/share/QGIS/QGIS3`，而在Windows上，则是一个漫游路径（roaming path），这意味着配置
  可以在用户的不同设备之间同步。
  4、- **默认位置**：作为最后的备选，如果以上所有方法都未找到配置文件，将使用一个预设的默认位置。这确保了无论如何QGIS都能够以
  某种预定义的设置启动，即使没有明确指定配置文件的位置。

  // Set up the QgsSettings Global Settings:
  // - use the path specified with --globalsettingsfile path,
  // - use the environment if not found
  // - use the AppDataLocation ($HOME/.local/share/QGIS/QGIS3 on Linux, roaming path on Windows)
  // - use a default location as a fallback
  */

  if ( globalsettingsfile.isEmpty() )
  {
    globalsettingsfile = getenv( "QGIS_GLOBAL_SETTINGS_FILE" );
  }

  if ( globalsettingsfile.isEmpty() )
  {
    QStringList startupPaths = QStandardPaths::locateAll( QStandardPaths::AppDataLocation, QStringLiteral( "qgis_global_settings.ini" ) );
    if ( !startupPaths.isEmpty() )
    {
      globalsettingsfile = startupPaths.at( 0 );
    }
  }

  if ( globalsettingsfile.isEmpty() )
  {
    /*
    * 杨小兵-2024-02-20
      `QgsApplication::resolvePkgPath()`是QGIS应用程序框架中的一个函数，其主要作用是定位和返回QGIS安装的包（package）
    目录的路径。这个函数通过分析QGIS应用程序的运行环境，自动确定QGIS主程序所在的文件系统路径。返回的路径是QGIS安装根目录
    的绝对路径，该目录包含了QGIS应用程序的所有资源、库、插件和配置文件等。
    */
    QString default_globalsettingsfile = QgsApplication::resolvePkgPath() + "/resources/qgis_global_settings.ini";
    if ( QFile::exists( default_globalsettingsfile ) )
    {
      globalsettingsfile = default_globalsettingsfile;
    }
  }

  if ( !globalsettingsfile.isEmpty() )
  {
    if ( !QgsSettings::setGlobalSettingsPath( globalsettingsfile ) )
    {
      preApplicationLogMessages << QObject::tr( "Invalid globalsettingsfile path: %1" ).arg( globalsettingsfile ), QStringLiteral( "QGIS" );
    }
    else
    {
      preApplicationLogMessages << QObject::tr( "Successfully loaded globalsettingsfile path: %1" ).arg( globalsettingsfile ), QStringLiteral( "QGIS" );
    }
  }

  if ( configLocalStorageLocation.isEmpty() )
  {
    QSettings globalSettings( globalsettingsfile, QSettings::IniFormat );
    if ( getenv( "QGIS_CUSTOM_CONFIG_PATH" ) )
    {
      configLocalStorageLocation = getenv( "QGIS_CUSTOM_CONFIG_PATH" );
    }
    else if ( globalSettings.contains( QStringLiteral( "core/profilesPath" ) ) )
    {
      configLocalStorageLocation = globalSettings.value( QStringLiteral( "core/profilesPath" ), "" ).toString();
      QgsDebugMsg( QStringLiteral( "Loading profiles path from global config at %1" ).arg( configLocalStorageLocation ) );
    }

    // If it is still empty at this point we get it from the standard location.
    if ( configLocalStorageLocation.isEmpty() )
    {
      configLocalStorageLocation = QStandardPaths::standardLocations( QStandardPaths::AppDataLocation ).value( 0 );
    }
  }

  /*
  * 杨小兵-2024-02-20
  一、解释
    上述代码段是在QGIS（一个开源地理信息系统软件）应用程序中，用于处理用户配置文件的一个例子。它通过一系列步骤，
  确定用户配置文件的存储位置，创建或获取一个用户配置文件，并最终获取该配置文件的名称和文件夹路径。这一行调用
  `QgsUserProfileManager::resolveProfilesFolder`方法，传入`configLocalStorageLocation`（一个表示配置存
  储位置的字符串）。该方法解析并返回存储用户配置文件的根文件夹路径。这个路径是所有用户配置文件存储的基础位置。

    如何在QGIS中管理用户配置文件，包括确定配置文件存储位置、获取或创建特定的用户配置文件，以及访问该配置文件的
  基本信息（如其名称和文件夹路径）。这对于在QGIS应用程序中实现用户特定的设置和偏好非常重要。
  */
  QString rootProfileFolder = QgsUserProfileManager::resolveProfilesFolder( configLocalStorageLocation );
  QgsUserProfileManager manager( rootProfileFolder );
  QgsUserProfile *profile = manager.getProfile( profileName, true );
  QString profileFolder = profile->folder();
  profileName = profile->name();
  delete profile;
#pragma endregion

#pragma region "确定和应用QGIS应用程序的语言翻译、数字格式化和区域设置、配置应用程序的本地化设置（包括语言和数值格式：将会根据本地情况自动变化）"
  /*
  * 杨小兵-2024-02-20
  一、解释
    这段代码是QGIS（一个开源地理信息系统软件）中用于处理应用程序的本地化和国际化设置的。代码的核心目标
  是确定和应用QGIS应用程序的语言翻译、数字格式化和区域设置。

  1. **获取当前的用户和全局区域设置**：代码首先从`QgsApplication`的设置中获取当前用户的区域设置（`myUserTranslation`）、
  全局区域设置（`myGlobalLocale`）、是否覆盖区域设置的标志（`myLocaleOverrideFlag`）以及是否显示数字分组分隔符的标志
  （`myShowGroupSeparatorFlag`）。
  
  2. **处理区域设置覆盖逻辑**：如果设置了区域设置覆盖标志（`myLocaleOverrideFlag`），代码会根据`QgsApplication`的设置
  来更新显示数字分组分隔符的标志（`myShowGroupSeparatorFlag`）。
  
  3. **确定最终的翻译代码**：代码通过一系列逻辑判断来决定使用哪个翻译代码。这个逻辑考虑了命令行参数、用户在选项对话框中指
  定的设置以及系统默认区域设置。命令行参数具有最高优先级，其次是用户指定的设置，最后是系统区域设置。
  
  4. **应用全局区域设置**：如果设置了区域设置覆盖标志并且指定了全局区域设置，代码将创建一个新的`QLocale`对象，并将其设置
  为默认的区域设置。
  
  5. **配置数字格式化**：根据是否显示数字分组分隔符的标志，代码调整当前区域设置的数字格式化选项，然后将其设置为应用程序的
  默认区域设置。
  
  6. **设置应用程序翻译**：最后，代码使用确定的翻译代码通过`QgsApplication::setTranslation`方法设置QGIS应用程序的翻译，
  以确保应用程序界面使用相应的语言显示。

  二、总结
    总的来说，这段代码负责根据用户的偏好、命令行参数或系统默认设置，来配置QGIS应用程序的语言和数字格式化设置，确保应用程序
  能够在不同地区设置下正确显示，并保持与用户期望的一致性。这是提升用户体验、支持多语言环境的重要步骤。
  */
  {
    /* Translation file for QGIS.
    */
    QString myUserTranslation = QgsApplication::settingsLocaleUserLocale.value();
    QString myGlobalLocale = QgsApplication::settingsLocaleGlobalLocale.value();
    bool myShowGroupSeparatorFlag = false; // Default to false
    bool myLocaleOverrideFlag = QgsApplication::settingsLocaleOverrideFlag.value();

    // Override Show Group Separator if the global override flag is set
    if ( myLocaleOverrideFlag )
    {
      // Default to false again
      myShowGroupSeparatorFlag = QgsApplication::settingsLocaleShowGroupSeparator.value();
    }

    //
    // Priority of translation is:
    //  - command line
    //  - user specified in options dialog (with group checked on)
    //  - system locale
    //
    //  When specifying from the command line it will change the user
    //  specified user locale
    //
    if ( !translationCode.isNull() && !translationCode.isEmpty() )
    {
      QgsApplication::settingsLocaleUserLocale.setValue( translationCode );
    }
    else
    {
      if ( !myLocaleOverrideFlag || myUserTranslation.isEmpty() )
      {
        translationCode = QLocale().name();
        //setting the locale/userLocale when the --lang= option is not set will allow third party
        //plugins to always use the same locale as the QGIS, otherwise they can be out of sync
        QgsApplication::settingsLocaleUserLocale.setValue( translationCode );
      }
      else
      {
        translationCode = myUserTranslation;
      }
    }

    // Global locale settings
    if ( myLocaleOverrideFlag && ! myGlobalLocale.isEmpty( ) )
    {
      QLocale currentLocale( myGlobalLocale );
      QLocale::setDefault( currentLocale );
    }

    // Number settings
    QLocale currentLocale;
    if ( myShowGroupSeparatorFlag )
    {
      currentLocale.setNumberOptions( currentLocale.numberOptions() &= ~QLocale::NumberOption::OmitGroupSeparator );
    }
    else
    {
      currentLocale.setNumberOptions( currentLocale.numberOptions() |= QLocale::NumberOption::OmitGroupSeparator );
    }
    QLocale::setDefault( currentLocale );

    QgsApplication::setTranslation( translationCode );
  }

  /*
  * 杨小兵-2024-02-20
  一、解释
    这段代码摘自QGIS（一个开源的地理信息系统软件）的初始化过程，它负责配置应用程序的本地化设置（包括语言和数值格式）并记录启动过程中的日志消息。

  1. **初始化QgsApplication实例**：
     ```cpp
     QgsApplication myApp(argc, argv, myUseGuiFlag, QString(), QStringLiteral("desktop"));
     ```
     这行代码创建了一个`QgsApplication`的实例，它是QGIS应用程序的主入口点。`argc`和`argv`参数用于接收命令行参数，
     `myUseGuiFlag`决定了应用程序是否以图形界面模式运行，最后的参数指定了应用程序的类型（在这个例子中是“desktop”桌面应用）。
  
  2. **设置应用程序的本地化（Locale）**：
     ```cpp
     QgsApplication::setLocale(QLocale());
     ```
     这行代码通过调用`setLocale`方法设置QGIS应用程序的本地化设置，`QLocale()`构造函数创建了一个代表系统当前区域设置的`QLocale`实例。这意味着QGIS会尝试根据系统的区域设置来调整其界面语言和数值格式。
  
  3. **记录应用程序启动前的日志消息**：
     ```cpp
     for (const QString &preApplicationLogMessage : std::as_const(preApplicationLogMessages))
       QgsMessageLog::logMessage(preApplicationLogMessage);
     ```
     在`QgsApplication`实例创建之前，可能会有一些日志消息需要记录（例如，错误信息或者警告）。这段代码遍历`preApplicationLogMessages`容器中的所有消息，并通过`QgsMessageLog::logMessage`方法将它们记录下来。这确保了应用程序启动过程中的所有重要信息都被捕获并可供后续分析。
  
  */
  QgsApplication myApp( argc, argv, myUseGuiFlag, QString(), QStringLiteral( "desktop" ) );

  // Set locale to emit QgsApplication's localeChanged signal
  QgsApplication::setLocale( QLocale() );

  //write the log messages written before creating QgsApplication
  for ( const QString &preApplicationLogMessage : std::as_const( preApplicationLogMessages ) )
    QgsMessageLog::logMessage( preApplicationLogMessage );

#pragma endregion

#pragma region "目前仅默认配置文件支持设置迁移"
  // Settings migration is only supported on the default profile for now.
  if ( profileName == QLatin1String( "default" ) )
  {
    // Note: this flag is ka version number so that we can reset it once we change the version.
    // Note2: Is this a good idea can we do it better.
    // Note3: Updated to only show if we have a migration from QGIS 2 - see https://github.com/qgis/QGIS/pull/38616
    QString path = QSettings( "QGIS", "QGIS2" ).fileName() ;
    if ( QFile::exists( path ) )
    {
      QgsSettings migSettings;
      int firstRunVersion = migSettings.value( QStringLiteral( "migration/firstRunVersionFlag" ), 0 ).toInt();
      bool showWelcome = ( firstRunVersion == 0  || Qgis::versionInt() > firstRunVersion );
      std::unique_ptr< QgsVersionMigration > migration( QgsVersionMigration::canMigrate( 20000, Qgis::versionInt() ) );
      if ( migration && ( settingsMigrationForce || migration->requiresMigration() ) )
      {
        bool runMigration = true;
        if ( !settingsMigrationForce && showWelcome )
        {
          QgsFirstRunDialog dlg;
          dlg.exec();
          runMigration = dlg.migrateSettings();
          migSettings.setValue( QStringLiteral( "migration/firstRunVersionFlag" ), Qgis::versionInt() );
        }

        if ( runMigration )
        {
          QgsDebugMsg( QStringLiteral( "RUNNING MIGRATION" ) );
          migration->runMigration();
        }
      }
    }
  }

#pragma endregion

#pragma region "使用QgsSettings配置QGIS相关的设置"
  /*
    直到此时我们才能使用 QgsSettings，因为在获取配置文件之前不会设置格式和文件夹。 以后应该清理一下，让这里更干净。
  */
  // We can't use QgsSettings until this point because the format and
  // folder isn't set until profile is fetch.
  // Should be cleaned up in future to make this cleaner.
  QgsSettings settings;

  QgsDebugMsgLevel( QStringLiteral( "User profile details:" ), 2 );
  QgsDebugMsgLevel( QStringLiteral( "\t - %1" ).arg( profileName ), 2 );
  QgsDebugMsgLevel( QStringLiteral( "\t - %1" ).arg( profileFolder ), 2 );
  QgsDebugMsgLevel( QStringLiteral( "\t - %1" ).arg( rootProfileFolder ), 2 );

  QgsApplication::init( profileFolder );
#pragma endregion

#pragma region "设置库路径、设置任务栏中应用程序的窗口图标（这里需要进行替换成自定义图标）"
  /*
  * 杨小兵-2024-02-20
  一、解释
    注释中的内容强调了在QGIS应用程序初始化过程中设置库路径（`QgsApplication::libraryPaths`）的重要性。简而言之，注释强调了
  正确设置库路径对于应用程序能够正常加载和使用Qt插件的重要性，并指出了设置库路径的正确时机。

  - **设置时机**：必须在`QgsApplication`实例化之后且在Qt使用任何插件之前重新定义库路径。这是因为库路径的设置会影响Qt插件的加载，
  如图像处理插件、数据库驱动等。
  - **重要性**：在加载启动画面、设置窗口图标等操作之前确保库路径正确设置，是确保这些操作能够正确访问所需插件的关键。
  - **环境变量**：强调始终要尊重`QT_PLUGIN_PATH`环境变量或`qt.conf`文件中的配置，这些配置在`QgsApplication`创建后会自动成为
  库路径的一部分。

    代码段的目的是确保在Windows平台上非静态构建的QGIS应用程序能够正确找到Qt图像插件。由于静态构建不支持，所以特别指出了这个设置
  对于非静态构建的重要性。

  1、- **`QApplication::applicationDirPath()`**：返回应用程序可执行文件所在的目录路径。这是确定Qt插件目录位置的基点。
  2、- **`QDir::separator()`**：返回用于分隔文件路径中目录的字符，通常是`/`或`\`，根据操作系统的不同而不同。在Windows系统中
  通常是`\`。
  3、- **`"qtplugins"`**：这是Qt插件目录的名称。此代码行的作用是构建一个指向应用程序目录下`qtplugins`子目录的完整路径，并将
  该路径添加到库搜索路径中。

  二、总结
  1、在QGIS应用程序初始化过程中设置库的路径是非常重要的


  // Redefine QgsApplication::libraryPaths as necessary.
  // IMPORTANT: Do *after* QgsApplication myApp(...), but *before* Qt uses any plugins,
  //            e.g. loading splash screen, setting window icon, etc.
  //            Always honor QT_PLUGIN_PATH env var or qt.conf, which will
  //            be part of libraryPaths just after QgsApplication creation.

  */

#ifdef Q_OS_WIN
  // For non static builds on win (static builds are not supported)
  // we need to be sure we can find the qt image plugins.
  //  杨小兵-2024-02-20：debug
  //QString qstr1 = QApplication::applicationDirPath();
  //QString qstr2 = QDir::separator();
  //QString qstr3 = qstr1 + qstr2 + "qtplugins";
  //std::string str1 = qstr1.toStdString();
  //std::string str2 = qstr2.toStdString();
  //std::string str3 = qstr3.toStdString();
  //QString debug_library_path = QApplication::applicationDirPath() + QDir::separator() + "qtplugins";
  QCoreApplication::addLibraryPath( QApplication::applicationDirPath()
                                    + QDir::separator() + "qtplugins" );
#endif
#if defined(Q_OS_UNIX)
  // Resulting libraryPaths has critical QGIS plugin paths first, then any Qt plugin paths, then
  // any dev-defined paths (in app's qt.conf) and/or user-defined paths (QT_PLUGIN_PATH env var).
  //
  // NOTE: Minimizes, though does not fully protect against, crashes due to dev/user-defined libs
  //       built against a different Qt/QGIS, while still allowing custom C++ plugins to load.
  QStringList libPaths( QCoreApplication::libraryPaths() );

  QgsDebugMsgLevel( QStringLiteral( "Initial macOS/UNIX QCoreApplication::libraryPaths: %1" )
                    .arg( libPaths.join( " " ) ), 4 );

  // Strip all critical paths that should always be prepended
  if ( libPaths.removeAll( QDir::cleanPath( QgsApplication::pluginPath() ) ) )
  {
    QgsDebugMsgLevel( QStringLiteral( "QgsApplication::pluginPath removed from initial libraryPaths" ), 4 );
  }
  if ( libPaths.removeAll( QCoreApplication::applicationDirPath() ) )
  {
    QgsDebugMsgLevel( QStringLiteral( "QCoreApplication::applicationDirPath removed from initial libraryPaths" ), 4 );
  }
  // Prepend path, so a standard Qt bundle directory is parsed
  QgsDebugMsgLevel( QStringLiteral( "Prepending QCoreApplication::applicationDirPath to libraryPaths" ), 4 );
  libPaths.prepend( QCoreApplication::applicationDirPath() );

  // Check if we are running in a 'release' app bundle, i.e. contains copied-in
  // standard Qt-specific plugin subdirectories (ones never created by QGIS, e.g. 'sqldrivers' is).
  // Note: bundleclicked(...) is inadequate to determine which *type* of bundle was opened, e.g. release or build dir.
  // An app bundled with QGIS_MACAPP_BUNDLE > 0 is considered a release bundle.
  QString  relLibPath( QDir::cleanPath( QCoreApplication::applicationDirPath().append( "/../PlugIns" ) ) );
  // Note: relLibPath becomes the defacto QT_PLUGINS_DIR of a release app bundle
  if ( QFile::exists( relLibPath + QStringLiteral( "/imageformats" ) ) )
  {
    // We are in a release app bundle.
    // Strip QT_PLUGINS_DIR because it will crash a launched release app bundle, since
    // the appropriate Qt frameworks and plugins have been copied into the bundle.
    if ( libPaths.removeAll( QT_PLUGINS_DIR ) )
    {
      QgsDebugMsgLevel( QStringLiteral( "QT_PLUGINS_DIR removed from initial libraryPaths" ), 4 );
    }
    // Prepend the Plugins path, so copied-in Qt plugin bundle directories are parsed.
    QgsDebugMsgLevel( QStringLiteral( "Prepending <bundle>/Plugins to libraryPaths" ), 4 );
    libPaths.prepend( relLibPath );

    // TODO: see if this or another method can be used to avoid QCA's install prefix plugins
    //       from being parsed and loaded (causes multi-Qt-loaded errors when bundled Qt should
    //       be the only one loaded). QCA core (> v2.1.3) needs an update first.
    //setenv( "QCA_PLUGIN_PATH", relLibPath.toUtf8().constData(), 1 );
  }
  else
  {
    // We are either running from build dir bundle, or launching Mach-O binary directly.  //#spellok
    // Add system Qt plugins, since they are not bundled, and not always referenced by default.
    // An app bundled with QGIS_MACAPP_BUNDLE = 0 will still have Plugins/qgis in it.
    // Note: Don't always prepend.
    //       User may have already defined it in QT_PLUGIN_PATH in a specific order.
    if ( !libPaths.contains( QT_PLUGINS_DIR ) )
    {
      QgsDebugMsgLevel( QStringLiteral( "Prepending QT_PLUGINS_DIR to libraryPaths" ), 4 );
      libPaths.prepend( QT_PLUGINS_DIR );
    }
  }

  QgsDebugMsgLevel( QStringLiteral( "Prepending QgsApplication::pluginPath to libraryPaths" ), 4 );
  libPaths.prepend( QDir::cleanPath( QgsApplication::pluginPath() ) );

  // Redefine library search paths.
  QCoreApplication::setLibraryPaths( libPaths );

  QgsDebugMsgLevel( QStringLiteral( "Rewritten macOS QCoreApplication::libraryPaths: %1" )
                    .arg( QCoreApplication::libraryPaths().join( " " ) ), 4 );
#endif

#ifdef Q_OS_MAC
  // Set hidpi icons; use SVG icons, as PNGs will be relatively too small
  QCoreApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );

  // Set 1024x1024 icon for dock, app switcher, etc., rendering
  myApp.setWindowIcon( QIcon( QgsApplication::iconsPath() + QStringLiteral( "qgis-icon-macos.png" ) ) );
#else
  //  设置任务栏中QGIS的窗口图标
  const QString debug_appIconPath = QgsApplication::appIconPath();
  QgsApplication::setWindowIcon( QIcon( QgsApplication::appIconPath() ) );
#endif

#pragma endregion

#pragma region "从指定文件中加载自定义设置"
  /*
  * 杨小兵-2024-02-21
  一、解释
    这段代码是用于在QGIS（一个开源的地理信息系统软件）中设置和加载自定义配置的例子。在QGIS中，用户可能需要根据自己的需求来定制软件的行为，
  比如界面布局、工具栏的显示/隐藏状态、插件的加载情况等。这种定制化设置可以通过修改配置文件来实现，而QGIS提供了一种机制来加载和应用这些自定义配置。

  1. **QSettings 类**: `QSettings` 是Qt框架中用于管理应用程序的配置设置的类。它可以用来保存和恢复应用程序的设置，比如窗口大小、位置或者最近打开
  文件的列表。在这段代码中，`QSettings`被用来从一个INI格式的文件中读取配置信息。
  2. **QgsCustomization 类**: `QgsCustomization` 是QGIS中用于管理软件定制设置的类。它允许开发者或用户指定软件在运行时应该如何表现，比如哪些工具
  栏或菜单项应该被隐藏或显示。

    这段代码的作用是将用户通过INI文件定义的定制化设置应用到QGIS软件中，从而改变软件的行为或外观以适应用户的特定需求。这使得QGIS更加灵活和适应不同用户的需求

  二、总结
  1、在QGIS中设置和加载自定义配置
  2、通过配置文件实现的这种功能
  3、QSettings是QT框架中的一个类（这个类的作用就是用来管理应用程序的配置设置）
  4、QSettings类是同一个ini格式的文件中读取配置信息
  */
  // TODO: use QgsSettings
  QSettings *customizationsettings = nullptr;

  if ( !customizationfile.isEmpty() )
  {
    // Using the customizationfile option always overrides the option and config path options.
    QgsCustomization::instance()->setEnabled( true );
  }
  else
  {
    // Use the default file location
    customizationfile = profileFolder + QDir::separator() + QStringLiteral( "QGIS" ) + QDir::separator() + QStringLiteral( "QGISCUSTOMIZATION3.ini" ) ;
    //  杨小兵-2024-02-20：debug
    //std::string str_customizationfile = customizationfile.toStdString();
  }

  //  指定了配置文件(`customizationfile`)和配置文件的格式(`QSettings::IniFormat`)。这表明程序将从一个INI格式的配置文件中读取设置。
  customizationsettings = new QSettings( customizationfile, QSettings::IniFormat );
  //  调用`QgsCustomization::instance()->loadDefault()`方法，程序加载并应用这些定制化设置
  // Load and set possible default customization, must be done after QgsApplication init and QgsSettings ( QCoreApplication ) init
  QgsCustomization::instance()->setSettings( customizationsettings );
  QgsCustomization::instance()->loadDefault();
#pragma endregion

#pragma region "如果定义了Q_OS_MACX宏，Q_OS_MACX宏代表当前的编译环境将会在MAC OS上，执行相关的代码块"
//  这部分内容是关于MAC OS操作系统平台上的内容，先不进行分析
#ifdef Q_OS_MACX
  if ( !getenv( "GDAL_DRIVER_PATH" ) )
  {
    // If the GDAL plugins are bundled with the application and GDAL_DRIVER_PATH
    // is not already defined, use the GDAL plugins in the application bundle.
    QString gdalPlugins( QCoreApplication::applicationDirPath().append( "/lib/gdalplugins" ) );
    if ( QFile::exists( gdalPlugins ) )
    {
      setenv( "GDAL_DRIVER_PATH", gdalPlugins.toUtf8(), 1 );
    }
  }

  // Point GDAL_DATA at any GDAL share directory embedded in the app bundle
  if ( !getenv( "GDAL_DATA" ) )
  {
    QStringList gdalShares;
    gdalShares << QCoreApplication::applicationDirPath().append( "/share/gdal" )
               << QDir::cleanPath( QgsApplication::pkgDataPath() ).append( "/share/gdal" )
               << QDir::cleanPath( QgsApplication::pkgDataPath() ).append( "/gdal" );
    const auto constGdalShares = gdalShares;
    for ( const QString &gdalShare : constGdalShares )
    {
      if ( QFile::exists( gdalShare ) )
      {
        setenv( "GDAL_DATA", gdalShare.toUtf8().constData(), 1 );
        break;
      }
    }
  }

  // Point PYTHONHOME to embedded interpreter if present in the bundle
  if ( !getenv( "PYTHONHOME" ) )
  {
    if ( QFile::exists( QCoreApplication::applicationDirPath().append( "/bin/python3" ) ) )
    {
      setenv( "PYTHONHOME", QCoreApplication::applicationDirPath().toUtf8().constData(), 1 );
    }
  }
#endif

#pragma endregion

#pragma region "设置自定义环境变量"
  /*
  * 杨小兵-2024-02-21
  一、解释
    - `QMap<QString, QString> systemEnvVars = QgsApplication::systemEnvVars();` 这行代码创建了一个名为`systemEnvVars`的`QMap`对象，用于存储环境变量
  `QgsApplication::systemEnvVars()`是一个静态方法，调用此方法会返回当前系统环境变量的一个快照，这个快照以键值对的形式存储在`QMap`对象中。这里`settings`
  对象预计是`QSettings`类型的实例，它用于访问应用程序的配置设置。`settings.value()`方法尝试从配置中读取指定的设置项（此处为`"qgis/customEnvVarsUse"`），
  如果找不到该设置项，则返回第二个参数提供的默认值（此处为`QVariant(false)`）。`toBool()`方法将设置值转换为布尔值，以便判断是否启用自定义环境变量。

    通过`settings.value()`读取的配置项允许用户在QGIS中启用或禁用自定义环境变量的使用。这提供了一种机制，使用户能够通过修改配置文件来影响QGIS的行为，例如，
  指定特定的插件路径、日志级别或其他软件行为。这种灵活性对于高级用户和开发人员尤其重要，他们可能需要根据特定的项目需求调整软件环境。
  二、总结
  1、`QMap`是Qt框架中的一个容器类，用于存储键值对，其中键和值都是`QString`类型
  2、systemEnvVars函数用来返回当前系统环境变量的一个快照
  3、使用户能够通过修改配置文件来影响QGIS的行为
  */
  // custom environment variables
  QMap<QString, QString> systemEnvVars = QgsApplication::systemEnvVars();
  bool useCustomVars = settings.value( QStringLiteral( "qgis/customEnvVarsUse" ), QVariant( false ) ).toBool();
  if ( useCustomVars )
  {
    QStringList customVarsList = settings.value( QStringLiteral( "qgis/customEnvVars" ), "" ).toStringList();
    if ( !customVarsList.isEmpty() )
    {
      const auto constCustomVarsList = customVarsList;
      for ( const QString &varStr : constCustomVarsList )
      {
        int pos = varStr.indexOf( QLatin1Char( '|' ) );
        if ( pos == -1 )
          continue;
        QString envVarApply = varStr.left( pos );
        if ( envVarApply == QLatin1String( "skip" ) )
          continue;
        QString varStrNameValue = varStr.mid( pos + 1 );
        pos = varStrNameValue.indexOf( QLatin1Char( '=' ) );
        if ( pos == -1 )
          continue;
        QString envVarName = varStrNameValue.left( pos );
        QString envVarValue = varStrNameValue.mid( pos + 1 );

        if ( systemEnvVars.contains( envVarName ) )
        {
          if ( envVarApply == QLatin1String( "prepend" ) )
          {
            envVarValue += systemEnvVars.value( envVarName );
          }
          else if ( envVarApply == QLatin1String( "append" ) )
          {
            envVarValue = systemEnvVars.value( envVarName ) + envVarValue;
          }
        }

        if ( systemEnvVars.contains( envVarName ) && envVarApply == QLatin1String( "unset" ) )
        {
#ifdef Q_OS_WIN
          putenv( QString( "%1=" ).arg( envVarName ).toUtf8().constData() );
#else
          unsetenv( envVarName.toUtf8().constData() );
#endif
        }
        else
        {
#ifdef Q_OS_WIN
          if ( envVarApply != "undefined" || !getenv( envVarName.toUtf8().constData() ) )
            putenv( QString( "%1=%2" ).arg( envVarName ).arg( envVarValue ).toUtf8().constData() );
#else
          setenv( envVarName.toUtf8().constData(), envVarValue.toUtf8().constData(), envVarApply == QLatin1String( "undefined" ) ? 0 : 1 );
#endif
        }
      }
    }
  }

#pragma endregion


#ifdef QGISDEBUG
  QgsFontUtils::loadStandardTestFonts( QStringList() << QStringLiteral( "Roman" ) << QStringLiteral( "Bold" ) );
#endif


#pragma region "设置应用程序的样式，如果没有设置将会采用平台的默认样式"
  /*
  * 杨小兵-2024-02-21
  一、解释
    注释解释了这段代码的目的是设置应用程序的样式。如果没有明确设置样式，Qt将使用平台默认的样式，但在Windows上默认样式通常看起来不尽如人意（也就是很丑陋），
  因此代码选择使用了`QPlastiqueStyle`样式。但实际代码片段中没有直接使用`QPlastiqueStyle`，而是提到了如果用户界面主题（UI Theme）不是默认值，且可用样式
  中包含“fusion”时，会选择使用“fusion”样式。

  1. **读取样式设置：** 首先，代码通过`settings.value()`方法读取名为`"qgis/style"`的设置项的值，这个值代表用户或系统设定的应用程序样式。然后，它读取名
  为`"UI/UITheme"`的设置项的值，这个值表示当前的界面主题。

  2. **判断并应用样式：** 如果界面主题不是`"default"`，代码将检查可用的样式中是否包含不区分大小写的`"fusion"`样式。如果包含，那么将`desiredStyle`变量
  设置为`"fusion"`。这一步骤表明，如果用户选择了非默认主题，并且系统支持“fusion”样式，应用程序将使用“fusion”样式来提升界面的外观。

  3. **样式应用的条件判断：** 通过`QStyleFactory::keys().contains()`方法检查当前Qt环境是否支持`"fusion"`样式。这是因为不同的Qt安装可能支持不同的样式
  集，所以需要先进行检查。

  二、总结
  1. **QString：** 用于处理Unicode字符序列的Qt类。在这段代码中，`QString`用于存储和比较样式名称和主题名称。

  2. **QSettings：** Qt的配置设置类，用于读取和写入应用程序的配置信息。这里使用`QSettings`来获取应用程序当前的样式和界面主题设置。

  3. **QStyleFactory：** 一个工厂类，用于创建`QStyle`对象。`QStyle`对象定义了Qt应用程序界面的外观。`QStyleFactory::keys()`方法返回所有可用样式的名称列
  表，这样可以检查是否存在某个特定的样式。

  */
  // Set the application style.  If it's not set QT will use the platform style except on Windows
  // as it looks really ugly so we use QPlastiqueStyle.
  QString desiredStyle = settings.value( QStringLiteral( "qgis/style" ) ).toString();
  const QString theme = settings.value( QStringLiteral( "UI/UITheme" ) ).toString();
  if ( theme != QLatin1String( "default" ) )
  {
    if ( QStyleFactory::keys().contains( QStringLiteral( "fusion" ), Qt::CaseInsensitive ) )
    {
      desiredStyle = QStringLiteral( "fusion" );
    }
  }

  /*
  * 杨小兵-2024-02-21
  一、解释
  1. **判断并避免使用Adwaita样式：** 如果`desiredStyle`包含“adwaita”（不区分大小写），或者如果`desiredStyle`为空但当前激活的样式名称包含“adwaita”，
  那么程序会尝试更改样式。原因是Adwaita样式的Qt变种对于一些应用程序（如QGIS）来说存在问题，比如控件尺寸过大，可能导致界面元素显示不全，从而影响用户体验。

  2. **选择替代样式：** 如果存在问题（如上述Adwaita样式问题），并且系统支持“fusion”样式（通过`QStyleFactory::keys().contains()`方法检查），那么代码
  将把`desiredStyle`设置为“fusion”。这是因为“fusion”样式通常被视为外观现代且兼容性好的样式，适用于多种平台，可以作为替代选择来避免Adwaita样式带来的问题。

  二、总结
  - **QApplication：** 管理GUI应用程序的控制流和主要设置。`QApplication::style()`返回当前应用程序正在使用的样式。

  - **QStyleFactory：** 用于创建`QStyle`对象的工厂类，`QStyleFactory::keys()`方法返回所有可用样式的名称列表。

  */
  const QString activeStyleName = QApplication::style()->metaObject()->className();
  if ( desiredStyle.contains( QLatin1String( "adwaita" ), Qt::CaseInsensitive )
       || ( desiredStyle.isEmpty() && activeStyleName.contains( QLatin1String( "adwaita" ), Qt::CaseInsensitive ) ) )
  {
    //never allow Adwaita themes - the Qt variants of these are VERY broken
    //for apps like QGIS. E.g. oversized controls like spinbox widgets prevent actually showing
    //any content in these widgets, leaving a very bad impression of QGIS

    //note... we only do this if there's a known good style available (fusion), as SOME
    //style choices can cause Qt apps to crash...
    if ( QStyleFactory::keys().contains( QStringLiteral( "fusion" ), Qt::CaseInsensitive ) )
    {
      desiredStyle = QStringLiteral( "fusion" );
    }
  }

  if ( !desiredStyle.isEmpty() )
  {
    QApplication::setStyle( new QgsAppStyle( desiredStyle ) );

    if ( activeStyleName != desiredStyle )
      settings.setValue( QStringLiteral( "qgis/style" ), desiredStyle );
  }
  else
  {
    // even if user has not set a style, we need to override the application style with the QgsAppStyle proxy
    // based on the default style (or we miss custom style tweaks)
    QApplication::setStyle( new QgsAppStyle( activeStyleName ) );
  }

#pragma endregion

#pragma region "设置认证数据库目录"
  //  如果认证数据库目录不是空的，那么则调用QGIS中的QgsApplication类中的方法设置[认证数据库目录]
  // set authentication database directory
  if ( !authdbdirectory.isEmpty() )
  {
    QgsApplication::setAuthDatabaseDirPath( authdbdirectory );
  }

#pragma endregion

#pragma region "设置启动画面"
  /*
  * 杨小兵-2024-02-21
  一、解释
  1. **加载启动画面图像**：首先，通过`QgsCustomization::instance()->splashPath()`获取启动画面图像的路径，并使用这个路径加上文件名`"splash.png"`来
  创建一个`QPixmap`对象`myPixmap`。这个对象装载了实际的图像数据。

  2. **计算屏幕DPI和启动画面尺寸**：代码计算当前屏幕的DPI（每英寸点数），默认值为96。如果能够获取到当前屏幕的实际DPI
  （通过`QGuiApplication::primaryScreen()->physicalDotsPerInch()`），则使用该值。然后，根据屏幕DPI调整启动画面图
  像的宽度和高度，以保持图像的清晰度和适当的显示尺寸。

  3. **创建并调整启动画面位置**：使用调整后的图像尺寸创建一个`QSplashScreen`对象`mypSplash`，并通过`myPixmap.scaled()`方法调整图像的尺寸，使之适应不
  同DPI的屏幕。如果能够获取到当前屏幕对象，代码会将启动画面移动到屏幕中心位置。

  4. **显示启动画面**：最后，根据条件判断（例如用户是否设置了隐藏启动画面的配置），决定是否显示启动画面。如果决定显示，对于Windows和Linux系统，可以直接
  使用PNG图像的透明区域作为启动画面的mask，从而实现透明效果，并调用`show()`方法显示启动画面。

  二、总结
  1、myPixmap对象中持有的是splash.png图像的大小
  */
  //set up splash screen（设置启动画面，这个启动画面就是QGIS应用程序启动的时候出现的画面）。
  QString mySplashPath( QgsCustomization::instance()->splashPath() );
  QPixmap myPixmap( mySplashPath + QStringLiteral( "LSL.png" ) );

  double screenDpi = 96;
  if ( QScreen *screen = QGuiApplication::primaryScreen() )
  {
    screenDpi = screen->physicalDotsPerInch();
  }

  //  杨小兵-2024-02-21：这两个参数用来按比例放大启动画面图像
  int w = 600 * screenDpi / 96;
  int h = 300 * screenDpi / 96;

  QSplashScreen *mypSplash = new QSplashScreen( myPixmap.scaled( w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );

  // Force splash screen to start on primary screen
  if ( QScreen *screen = QGuiApplication::primaryScreen() )
  {
    const QPoint currentDesktopsCenter = screen->availableGeometry().center();
    mypSplash->move( currentDesktopsCenter - mypSplash->rect().center() );
  }

  if ( !takeScreenShots && !myHideSplash && !settings.value( QStringLiteral( "qgis/hideSplash" ) ).toBool() )
  {
    //for win and linux we can just automask and png transparency areas will be used
    mypSplash->setMask( myPixmap.mask() );
    mypSplash->show();
  }

#pragma endregion

#pragma region "加载默认窗口设置、设置最大线程数"
  /*
  * 杨小兵-2024-02-21
  一、解释
    `value`函数是Qt设置管理类（如`QSettings`）的一个成员函数，用于从应用程序的设置（通常是配置文件或注册表）中读取值。函数的原型是：
  QVariant QgsSettings::value( const QString &key, const QVariant &defaultValue, const QgsSettings::Section section ) const

    `QgsSettings::value`函数是一个非常核心的功能，在Qt框架的配置管理中广泛使用，用于从应用程序的配置存储中检索设置值。函数的签名表明它是
  设计来读取任何类型的设置值，返回一个`QVariant`类型，这使得它能够返回不同类型的数据，如整数、浮点数、字符串等。

  1. **指定键名**：通过参数`const QString &key`指定要检索的设置的键名。这个键通常是一个字符串，标识了配置项的唯一名称。
  2. **提供默认值**：通过参数`const QVariant &defaultValue = QVariant()`提供一个默认值。如果指定的键不存在，则返回这
  个默认值。这个参数是可选的，默认为一个空的`QVariant`对象。
  3. **指定配置部分**（可选）：通过`const QgsSettings::Section section`参数可以指定配置项所属的部分（Section）。这是
  `QgsSettings`特有的功能，用于将设置分组管理，提高配置的组织性。这个参数也是可选的，默认值是`NoSection`，意味着不指定
  特定部分（如下列代码所示内容）。

  //! Sections for namespaced settings
    enum Section
    {
      NoSection,
      Core,
      Gui,
      Server,
      Plugins,
      Auth,
      App,
      Providers,
      Expressions,
      Misc,
      Gps, //!< GPS section, since QGIS 3.22
    };

  二、总结
  1、QgsSettings::value在QGIS中是比较常使用的一个函数
  2、QgsSettings::value这个函数从一个地方检查是否存在想要查询的值（用于从应用程序的配置存储中检索设置值）
  3、QgsSettings::value这个函数可以返回不同类型的数据
  */
  // optionally restore default window state
  // use restoreDefaultWindowState setting only if NOT using command line (then it is set already)
  if ( myRestoreDefaultWindowState || settings.value( QStringLiteral( "qgis/restoreDefaultWindowState" ), false ).toBool() )
  {
    QgsDebugMsg( QStringLiteral( "Resetting /UI/state settings!" ) );
    settings.remove( QStringLiteral( "/UI/state" ) );
    settings.remove( QStringLiteral( "/qgis/restoreDefaultWindowState" ) );
  }

  if ( hideBrowser )
  {
    if ( settings.value( QStringLiteral( "/Windows/Data Source Manager/tab" ) ).toInt() == 0 )
      settings.setValue( QStringLiteral( "/Windows/Data Source Manager/tab" ), 1 );
    settings.setValue( QStringLiteral( "/UI/hidebrowser" ), true );
  }

  // set max. thread count
  // this should be done in QgsApplication::init() but it doesn't know the settings dir.
  int debug_number_max_threads = settings.value(QStringLiteral("qgis/max_threads"), -1).toInt();
  QgsApplication::setMaxThreads(settings.value(QStringLiteral("qgis/max_threads"), -1).toInt());

#pragma endregion

#pragma region "通过前面的一些列设置，现在创建一个QgisApp类的实例"
  /*
  * 杨小兵-2024-02-21
  一、解释
    注释`"QgisApp" used to find canonical instance`指的是，通过使用`"QgisApp"`作为对象名称，这使得可以在QGIS应用程序的不同部分中
  找到这个特定的`QgisApp`实例。在复杂的应用程序中，可能需要在不同的组件或插件之间访问主应用程序实例。给这个实例设置一个易于识别的名称
  （在这里是`"QgisApp"`）允许其他代码通过这个名称来查找并与之交互，确保他们引用的是同一个应用程序实例。这是一种常见的模式，用于在大型
  或模块化的应用程序中管理对核心组件的访问。

    `setObjectName`函数是Qt框架中`QObject`类的一个成员函数，它允许给对象设置一个名称。这个名称是一个字符串，可以用于在运行时识别对象。
  尽管这个名称对于程序的逻辑执行没有直接影响，但它在调试、自动化测试以及通过名称查找对象时非常有用。Qt提供了通过名称查找对象的功能。在
  QGIS中，如果需要从插件或其他组件中访问主应用程序实例，可以使用这个名称来查找对象，而不是依赖于全局变量或者复杂的依赖注入机制。

    使用Qt的信号与槽机制来建立两个对象之间的连接，具体来说，是将`QgsApplication`的`preNotify`信号与`QgsCustomization::instance()`
  的`preNotify`槽（或方法）连接起来。信号和槽是Qt框架中实现对象间通信的一种机制，允许一个对象在发生特定事件时通知另一个对象。
  1、- **信号 (`SIGNAL(preNotify(QObject *, QEvent *, bool *))`)**：当`QgsApplication`即将处理一个事件时，它发出`preNotify`信号。
  这个信号携带有关即将被处理的事件的信息，包括事件的目标对象(`QObject *`)、事件对象(`QEvent *`)以及一个布尔指针(`bool *`)，后者可以用
  于修改事件处理的行为（例如，阻止事件的进一步传递）。
  - **槽 (`SLOT(preNotify(QObject *, QEvent *, bool *))`)**：`QgsCustomization::instance()`的`preNotify`槽是一个方法，设计用来响
  应`preNotify`信号。当接收到信号时，这个方法会被调用，允许`QgsCustomization`的实例根据即将处理的事件执行自定义的逻辑。

    `QgsApplication`代表了应用程序的核心，负责管理事件循环和其他全局性的任务。`QgsCustomization`是一个设计用来提供定制QGIS界面和行为的
  组件。将这两者连接起来，意味着可以利用`QgsApplication`的全局性事件处理能力，来实现`QgsCustomization`的定制需求。

  二、总结
  1、使用QgisApp可以在QGIS不同的组件或者插件之间访问QGIS程序实例
  2、setObjectName是QT框架中`QObject`类的一个成员函数，它允许给对象设置一个名称（一个实例）
  */
  QgisApp *qgis = new QgisApp( mypSplash, myRestorePlugins, mySkipBadLayers, mySkipVersionCheck, rootProfileFolder, profileName ); // "QgisApp" used to find canonical instance
  qgis->setObjectName( QStringLiteral( "QgisApp" ) );

  QgsApplication::connect(
    &myApp, SIGNAL( preNotify( QObject *, QEvent *, bool * ) ),
    //qgis, SLOT( preNotify( QObject *, QEvent *))
    QgsCustomization::instance(), SLOT( preNotify( QObject *, QEvent *, bool * ) )
  );

#pragma endregion

#pragma region "是否启动qgis项目文件、是否初始化布局、python参数等等"
  /////////////////////////////////////////////////////////////////////
  // Load a project file if one was specified
  /////////////////////////////////////////////////////////////////////
  if ( ! sProjectFileName.isEmpty() )
  {
    // in case the project contains broken layers, interactive
    // "Handle Bad Layers" is displayed that could be blocked by splash screen
    mypSplash->hide();
    qgis->openProject( sProjectFileName );
  }

  /////////////////////////////////////////////////////////////////////
  // autoload any file names that were passed in on the command line
  /////////////////////////////////////////////////////////////////////
  for ( const QString &layerName : std::as_const( sFileList ) )
  {
    QgsDebugMsg( QStringLiteral( "Trying to load file : %1" ).arg( layerName ) );
    // don't load anything with a .qgs extension - these are project files
    if ( layerName.endsWith( QLatin1String( ".qgs" ), Qt::CaseInsensitive ) ||
         layerName.endsWith( QLatin1String( ".qgz" ), Qt::CaseInsensitive ) ||
         QgsZipUtils::isZipFile( layerName ) )
    {
      continue;
    }
    else if ( layerName.endsWith( QLatin1String( ".qlr" ), Qt::CaseInsensitive ) )
    {
      QgsAppLayerHandling::openLayerDefinition( layerName );
    }
    else
    {
      bool ok = false;
      QgsAppLayerHandling::openLayer( layerName, ok );
    }
  }


  /////////////////////////////////////////////////////////////////////
  // Set initial extent if requested
  /////////////////////////////////////////////////////////////////////
  if ( ! myInitialExtent.isEmpty() )
  {
    QgsLocaleNumC l;
    double coords[4];
    int pos, posOld = 0;
    bool ok = true;

    // parse values from string
    // extent is defined by string "xmin,ymin,xmax,ymax"
    for ( int i = 0; i < 3; i++ )
    {
      // find comma and get coordinate
      pos = myInitialExtent.indexOf( ',', posOld );
      if ( pos == -1 )
      {
        ok = false;
        break;
      }

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
      coords[i] = myInitialExtent.midRef( posOld, pos - posOld ).toDouble( &ok );
#else
      coords[i] = QStringView {myInitialExtent}.mid( posOld, pos - posOld ).toDouble( &ok );
#endif
      if ( !ok )
        break;

      posOld = pos + 1;
    }

    // parse last coordinate
    if ( ok )
    {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
      coords[3] = myInitialExtent.midRef( posOld ).toDouble( &ok );
#else
      coords[3] = QStringView {myInitialExtent}.mid( posOld ).toDouble( &ok );
#endif
    }

    if ( !ok )
    {
      QgsDebugMsg( QStringLiteral( "Error while parsing initial extent!" ) );
    }
    else
    {
      // set extent from parsed values
      QgsRectangle rect( coords[0], coords[1], coords[2], coords[3] );
      qgis->setExtent( rect );
    }
  }

  if ( !pythonArgs.isEmpty() )
  {
    if ( !pythonfile.isEmpty() )
    {
#ifdef Q_OS_WIN
      //replace backslashes with forward slashes
      pythonfile.replace( '\\', '/' );
#endif
      pythonArgs.prepend( pythonfile );
    }

    QgsPythonRunner::run( QStringLiteral( "sys.argv = ['%1']" ).arg( pythonArgs.replaceInStrings( QChar( '\'' ), QStringLiteral( "\\'" ) ).join( "','" ) ) );
  }

  if ( !pythonfile.isEmpty() )
  {
#ifdef Q_OS_WIN
    //replace backslashes with forward slashes
    pythonfile.replace( '\\', '/' );
#endif
    QgsPythonRunner::run( QStringLiteral( "with open('%1','r') as f: exec(f.read())" ).arg( pythonfile ) );
  }

  /////////////////////////////////`////////////////////////////////////
  // Take a snapshot of the map view then exit if snapshot mode requested
  /////////////////////////////////////////////////////////////////////
  if ( !mySnapshotFileName.isEmpty() )
  {
    /*You must have at least one paintEvent() delivered for the window to be
      rendered properly.

      It looks like you don't run the event loop in non-interactive mode, so the
      event is never occurring.

      To achieve this without running the event loop: show the window, then call
      qApp->processEvents(), grab the pixmap, save it, hide the window and exit.
      */
    //qgis->show();
    QgsApplication::processEvents();
    QPixmap *myQPixmap = new QPixmap( mySnapshotWidth, mySnapshotHeight );
    myQPixmap->fill();
    qgis->saveMapAsImage( mySnapshotFileName, myQPixmap );
    QgsApplication::processEvents();
    qgis->hide();

    return 1;
  }

#pragma region "处理DXF文件：导出地图或图层为DXF文件"
  /*
  * 杨小兵-2024-02-21
  * 一、解释
    上述代码片段的作用是在QGIS中导出地图或图层为DXF文件。DXF（Drawing Exchange Format）是一个由Autodesk为AutoCAD创建的CAD数据文件格式，
  用于数据交换和互操作性。

  - **检查DXF输出文件名是否为空：** 通过`if ( !dxfOutputFile.isEmpty() )`判断是否已经指定了输出DXF文件的路径和名称。如果指定了，则继续
  执行后续操作。

  - **隐藏QGIS界面：** 执行`qgis->hide();`隐藏QGIS应用程序的界面。这可能是为了在执行导出操作时减少界面干扰或提高性能。
  
  - **配置DXF导出选项：** 创建一个`QgsDxfExport`对象，并通过调用其方法来配置导出选项。包括设置符号比例(`setSymbologyScale`)、设置符号导
  出模式(`setSymbologyExport`)和设置导出范围(`setExtent`)。
  
  - **准备图层列表：** 初始化`QStringList layerIds;`和`QList< QgsDxfExport::DxfLayer > layers;`准备存储要导出的图层的ID和图层对象。这
  表明接下来的步骤可能涉及选择特定的图层进行导出。

  - **确定导出目标：** 用户在导出图层或地图为DXF格式时，需要指定一个输出文件。`dxfOutputFile`提供了一种方法来存储这个目标文件的路径和
  名称，确保导出操作可以正确地将数据写入用户指定的位置。

  - **用户界面交互：** 在图形用户界面(GUI)中，用户通过文件对话框选择或输入文件路径。`dxfOutputFile`作为一个变量，可以存储这个由用户界
  面收集的信息，供导出功能使用。

  - **灵活性和兼容性：** DXF格式广泛用于CAD软件和其他地理信息系统中。提供DXF导出功能，使QGIS用户能够轻松地与这些系统交换图形数据，增强
  了QGIS的灵活性和兼容性。

    `QgsVectorLayer`是QGIS应用程序的一个核心类，用于表示和操作矢量数据。矢量数据是GIS（地理信息系统）中一种重要的数据类型，用于表示地理
  特征，如点（例如，地理位置）、线（例如，道路或河流）和多边形（例如，湖泊或行政边界）。`QgsVectorLayer`类提供了一系列的方法和属性，用于
  访问和修改这些矢量数据，包括查询特征、编辑几何形状、修改属性数据等。- **数据表示：** `QgsVectorLayer`代表一个矢量数据层，这个层可以来
  源于多种数据源，如文件（例如Shapefile、GeoJSON）、数据库（例如PostGIS）或网络服务（例如WMS）。这意味着`QgsVectorLayer`提供了一个抽
  象，允许QGIS以统一的方式处理不同来源的矢量数据。

  二、总结
  1、DXF（Drawing Exchange Format）是一个由Autodesk为AutoCAD创建的CAD数据文件格式
  2、这段代码是用来在QGIS中设置导出地图或图层为DXF文件格式的
  3、创建一个`QgsDxfExport`对象，并通过调用其方法来配置导出选项（setSymbologyScale、setSymbologyExport、setExtent）
  4、`QgsVectorLayer`是QGIS应用程序的一个核心类，用于表示和操作矢量数据
  5、矢量数据的种类很多，不仅仅是shapefile一种，还有geojson、postgis等等
  */
  if ( !dxfOutputFile.isEmpty() )
  {
    qgis->hide();

    QgsDxfExport dxfExport;
    dxfExport.setSymbologyScale( dxfScale );
    dxfExport.setSymbologyExport( dxfSymbologyMode );
    dxfExport.setExtent( dxfExtent );

    QStringList layerIds;
    QList< QgsDxfExport::DxfLayer > layers;


    if ( !dxfMapTheme.isEmpty() )
    {
      const auto constMapThemeVisibleLayers = QgsProject::instance()->mapThemeCollection()->mapThemeVisibleLayers( dxfMapTheme );
      for ( QgsMapLayer *layer : constMapThemeVisibleLayers )
      {
        QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( layer );
        if ( !vl )
          continue;
        layers << QgsDxfExport::DxfLayer( vl );
        layerIds << vl->id();
      }
    }
    else
    {
      const auto constMapLayers = QgsProject::instance()->mapLayers();
      for ( QgsMapLayer *ml : constMapLayers )
      {
        QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
        if ( !vl )
          continue;
        layers << QgsDxfExport::DxfLayer( vl );
        layerIds << vl->id();
      }
    }

    if ( !layers.isEmpty() )
    {
      dxfExport.addLayers( layers );
    }

    QFile dxfFile;
    if ( dxfOutputFile == QLatin1String( "-" ) )
    {
      if ( !dxfFile.open( stdout, QIODevice::WriteOnly | QIODevice::Truncate ) )
      {
        std::cerr << "could not open stdout" << std::endl;
        return 2;
      }
    }
    else
    {
      if ( !dxfOutputFile.endsWith( QLatin1String( ".dxf" ), Qt::CaseInsensitive ) )
        dxfOutputFile += QLatin1String( ".dxf" );
      dxfFile.setFileName( dxfOutputFile );
    }

    QgsDxfExport::ExportResult res = dxfExport.writeToFile( &dxfFile, dxfEncoding );
    switch ( res )
    {
      case QgsDxfExport::ExportResult::Success:
        break;

      case QgsDxfExport::ExportResult::DeviceNotWritableError:
        std::cerr << "dxf output failed, the device is not wriable" << std::endl;
        break;

      case QgsDxfExport::ExportResult::InvalidDeviceError:
        std::cerr << "dxf output failed, the device is invalid" << std::endl;
        break;

      case QgsDxfExport::ExportResult::EmptyExtentError:
        std::cerr << "dxf output failed, the extent could not be determined" << std::endl;
        break;
    }

    delete qgis;

    return static_cast<int>( res );
  }

#pragma endregion

  // make sure we don't have a dirty blank project after launch
  QgsProject::instance()->setDirty( false );

#pragma endregion

#pragma region "设置存放opencl的文件夹"
  /*
  * 杨小兵-2024-02-21
  一、解释
    OpenCL（Open Computing Language）是一个用于在异构平台上执行并行编程的框架，支持通过CPU、GPU等处理器来加速计算密集型任务。
  在QGIS这样的地理信息系统软件中，OpenCL可以用来加速空间数据处理和分析任务，提高软件的性能和响应速度。

  1. **设置OpenCL程序文件夹的路径：** 这段代码的主要功能是设置OpenCL源代码文件的路径。这些源代码文件包含了OpenCL程序，QGIS可以
  利用这些程序来执行特定的并行计算任务。
  
  2. **命令行参数：** 首先，代码检查是否通过命令行参数`--openclprogramfolder`指定了OpenCL程序文件夹的路径。如果指定了，就使用这个路径。
  
  3. **环境变量：** 如果没有通过命令行参数指定路径，代码将尝试读取环境变量`QGIS_OPENCL_PROGRAM_FOLDER`的值作为OpenCL程序文件夹的路径。
  
  4. **默认位置：** 如果上述两种方法都没有提供路径，代码将使用QGIS应用程序初始化时设置的默认位置作为OpenCL程序文件夹的路径。
  
  5. **应用路径：** 如果找到了OpenCL程序文件夹的路径（不为空），则通过`QgsOpenClUtils::setSourcePath()`方法设置这个路径，以便QGIS
  知道从哪里加载OpenCL程序。

    【注】不同的硬件和操作系统可能需要不同的OpenCL程序实现。通过允许用户指定程序文件夹的路径，QGIS可以更好地在多种硬件和系统环境下运行，确保软
  件的兼容性和稳定性。

  二、总结
  1、利用opencl来增强性能
  2、方法一、设置opencl文件位置（这个文件中包含了opencl的程序）
  3、方法二、通过命令行参数指定opencl程序文件夹的位置
  4、方法三、环境变量设置
  5、方法四、默认位置

  */
#ifdef HAVE_OPENCL

  // Overrides the OpenCL path to the folder containing the programs
  // - use the path specified with --openclprogramfolder,
  // - use the environment QGIS_OPENCL_PROGRAM_FOLDER if not found
  // - use a default location as a fallback (this is set in QgsApplication initialization)
  if ( openClProgramFolder.isEmpty() )
  {
    openClProgramFolder = getenv( "QGIS_OPENCL_PROGRAM_FOLDER" );
  }

  if ( ! openClProgramFolder.isEmpty() )
  {
    QgsOpenClUtils::setSourcePath( openClProgramFolder );
  }

#endif
  //  杨小兵-2024-02-21：debugging
  //std::string str_openClProgramFolder = openClProgramFolder.toStdString();

#pragma endregion


  if ( takeScreenShots )
  {
    qgis->takeAppScreenShots( screenShotsPath, screenShotsCategories );
  }

  /////////////////////////////////////////////////////////////////////
  // Continue on to interactive gui...
  /////////////////////////////////////////////////////////////////////
  //  显示QGIS界面
  qgis->show();
  QgsApplication::connect( &myApp, SIGNAL( lastWindowClosed() ), &myApp, SLOT( quit() ) );

  //  当主界面准备好之后可以关闭“启动界面”图标
  mypSplash->finish( qgis );
  delete mypSplash;

  qgis->completeInitialization();

#pragma region "针对不同的平台进行不同的代码处理"
#if defined(ANDROID)
  // fix for Qt Ministro hiding app's menubar in favor of native Android menus
  qgis->menuBar()->setNativeMenuBar( false );
  qgis->menuBar()->setVisible( true );
#endif

#if !defined(Q_OS_WIN)
  UnixSignalWatcher sigwatch;
  sigwatch.watchForSignal( SIGINT );

  QObject::connect( &sigwatch, &UnixSignalWatcher::unixSignal, &myApp, [ ]( int signal )
  {
    switch ( signal )
    {
      case SIGINT:
        QgsApplication::exit( 1 );
        break;

      default:
        break;
    }
  } );
#endif
#pragma endregion
  //  用来真正运行QGIS（exec函数是QApplication类中的一个函数，用来运行程序）
  int retval = QgsApplication::exec();
  delete qgis;
  return retval;
}
