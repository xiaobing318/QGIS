/***************************************************************************
                         qgslogger.h  -  description
                             -------------------
    begin                : April 2006
    copyright            : (C) 2006 by Marco Hugentobler
    email                : marco.hugentobler at karto dot baug dot ethz dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSLOGGER_H
#define QGSLOGGER_H

#pragma region "包含头文件"
#include <iostream>
#include "qgis_sip.h"
#include <sstream>
#include <QString>
#include <QTime>

#include "qgis_core.h"
#include "qgsconfig.h"
#pragma endregion

#pragma region "类的前向声明"
class QFile;
#pragma endregion

#pragma region "根据是否定义QGISDEBUG宏来定义不同的“宏函数”"
#ifdef QGISDEBUG
#define QgsDebugMsg(str) QgsLogger::debug(QString(str), 1, __FILE__, __FUNCTION__, __LINE__)
#define QgsDebugMsgLevel(str, level) if ( level <= QgsLogger::debugLevel() ) { QgsLogger::debug(QString(str), (level), __FILE__, __FUNCTION__, __LINE__); }(void)(0)
#define QgsDebugCall QgsScopeLogger _qgsScopeLogger(__FILE__, __FUNCTION__, __LINE__)
#else
#define QgsDebugCall do {} while(false)
#define QgsDebugMsg(str) do {} while(false)
#define QgsDebugMsgLevel(str, level) do {} while(false)
#endif
#pragma endregion

#pragma region "类注释解释说明"
/**
 * \ingroup core
 * \brief QgsLogger is a class to print debug/warning/error messages to the console.
 *
 * The advantage of this class over iostream & co. is that the
 * output can be controlled with environment variables:
 * QGIS_DEBUG is an int describing what debug messages are written to the console.
 * If the debug level of a message is <= QGIS_DEBUG, the message is written to the
 * console. It the variable QGIS_DEBUG is not defined, it defaults to 1 for debug
 * mode and to 0 for release mode
 * QGIS_DEBUG_FILE may contain a file name. Only the messages from this file are
 * printed (provided they have the right debuglevel). If QGIS_DEBUG_FILE is not
 * set, messages from all files are printed
 *
 * QGIS_LOG_FILE may contain a file name. If set, all messages will be appended
 * to this file rather than to stdout.
*/
/*
* 杨小兵-2024-03-07
一、解释上述注释内容
  注释中描述了`QgsLogger`类的功能和用法。`QgsLogger`是QGIS核心库的一部分，用于向控制台打印调试、警告和错误消息。与标准输入输出流（如iostream等）相比，
`QgsLogger`的优势在于其输出可以通过环境变量进行控制，从而提供更灵活的调试信息管理方式。

  环境变量`QGIS_DEBUG`是一个整数值，用于指定哪些调试消息将被写入控制台。只有当消息的调试级别小于或等于`QGIS_DEBUG`指定的值时，这些消息才会被输出。如果
没有定义`QGIS_DEBUG`环境变量，它的默认值在调试模式下为1，在发布模式下为0。

`QGIS_DEBUG_FILE`环境变量可以指定一个文件名，只有来自该文件的消息（且符合指定的调试级别）会被打印出来。如果没有设置`QGIS_DEBUG_FILE`，则所有文件的消息
都会被打印。

`QGIS_LOG_FILE`环境变量也可以指定一个文件名。如果设置了此环境变量，所有消息将被追加到该文件中，而不是输出到标准输出（stdout）。

### 有条理总结上述注释内容
1. **`QgsLogger`类的作用**：用于向控制台输出调试、警告和错误消息。
2. **环境变量控制**：
   - `QGIS_DEBUG`：控制输出到控制台的调试消息级别。
   - `QGIS_DEBUG_FILE`：限制只有来自特定文件的消息才会被输出。
   - `QGIS_LOG_FILE`：指定一个文件，所有消息将追加到这个文件而不是标准输出。
3. **默认行为**：
   - 如果未定义`QGIS_DEBUG`，其默认值在调试模式下为1，在发布模式下为0。
   - 如果未设置`QGIS_DEBUG_FILE`，则所有文件的消息都将被输出。
   - 如果未设置`QGIS_LOG_FILE`，消息输出到标准输出。

### 需要注意的地方
1. **环境变量的设置**：正确设置环境变量是使用`QgsLogger`进行有效调试的关键。开发者需要根据需要调整`QGIS_DEBUG`的值，以及可选地设置
`QGIS_DEBUG_FILE`和`QGIS_LOG_FILE`来控制消息的输出。
2. **调试级别的理解**：理解不同调试消息的级别及其与`QGIS_DEBUG`环境变量值之间的关系对于有效地过滤输出很重要。
3. **性能考虑**：虽然`QgsLogger`提供了强大的调试功能，但在性能敏感的应用中，过多的日志记录可能会对性能产生影响。因此，在发布版本的应
用中通常会降低调试级别或关闭日志记录。
4. **日志文件管理**：如果使用`QGIS_LOG_FILE`指定了日志文件，需要注意日志文件的管理，防止因为日志过多而消耗大量磁盘空间。

总结：
1、QgsLogger是qgis_core中的一部分（是QGIS核心库中的一部分）
2、与标准输入输出流（如iostream等）相比，`QgsLogger`的优势在于其输出可以通过环境变量进行控制，从而提供更灵活的调试信息管理方式。
3、可以通过设置QGIS_DEBUG_FILE和QGIS_LOG_FILE设置需要输出的日志等级和日志的输出文件
*/
#pragma endregion
class CORE_EXPORT QgsLogger
{
  public:

    /**
     * Goes to qDebug.
     * \param msg the message to be printed
     * \param debuglevel
     * \param file file name where the message comes from
     * \param function function where the message comes from
     * \param line place in file where the message comes from
    */
    static void debug( const QString &msg, int debuglevel = 1, const char *file = nullptr, const char *function = nullptr, int line = -1 );

    //! Similar to the previous method, but prints a variable int-value pair
    static void debug( const QString &var, int val, int debuglevel = 1, const char *file = nullptr, const char *function = nullptr, int line = -1 );

    /**
     * Similar to the previous method, but prints a variable double-value pair
     * \note not available in Python bindings
     */
    static void debug( const QString &var, double val, int debuglevel = 1, const char *file = nullptr, const char *function = nullptr, int line = -1 ) SIP_SKIP SIP_SKIP;

    /**
     * Prints out a variable/value pair for types with overloaded operator<<
     * \note not available in Python bindings
     */
    template <typename T> static void debug( const QString &var, T val, const char *file = nullptr, const char *function = nullptr,
        int line = -1, int debuglevel = 1 ) SIP_SKIP SIP_SKIP
    {
      std::ostringstream os;
      os << var.toLocal8Bit().data() << " = " << val;
      debug( var, os.str().c_str(), file, function, line, debuglevel );
    }

    //! Goes to qWarning.
    static void warning( const QString &msg );

    //! Goes to qCritical.
    static void critical( const QString &msg );

    //! Goes to qFatal.
    static void fatal( const QString &msg );

    /**
     * Reads the environment variable QGIS_DEBUG and converts it to int. If QGIS_DEBUG is not set,
     * the function returns 1 if QGISDEBUG is defined and 0 if not.
    */
    static int debugLevel()
    {
      if ( sDebugLevel == -999 )
        init();
      return sDebugLevel;
    }

    //! Logs the message passed in to the logfile defined in QGIS_LOG_FILE if any.
    static void logMessageToFile( const QString &message );

    /**
     * Reads the environment variable QGIS_LOG_FILE. Returns NULL if the variable is not set,
     * otherwise returns a file name for writing log messages to.
    */
    static QString logFile();

  private:
    static void init();

    //! Current debug level
    static int sDebugLevel;
    static int sPrefixLength;
};

/**
 * \ingroup core
 */
class CORE_EXPORT QgsScopeLogger // clazy:exclude=rule-of-three
{
  public:
    QgsScopeLogger( const char *file, const char *func, int line )
      : _file( file )
      , _func( func )
      , _line( line )
    {
      QgsLogger::debug( QStringLiteral( "Entering." ), 2, _file, _func, _line );
    }
    ~QgsScopeLogger()
    {
      QgsLogger::debug( QStringLiteral( "Leaving." ), 2, _file, _func, _line );
    }
  private:
    const char *_file = nullptr;
    const char *_func = nullptr;
    int _line;
};

#endif
