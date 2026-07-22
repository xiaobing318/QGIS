/***************************************************************************
    qcopilots_container_utils.cpp
    -----------------------------
    begin                : July 2026
    copyright            : (C) 2026
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qcopilots_container_utils.h"

#include "qgsapplication.h"
#include "qgsmessagelog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QStringConverter>
#include <QTextStream>

namespace
{
  const qint64 sMaximumLogFileSize = 1024 * 1024;
  const QString sStableWebEngineProfileName = QStringLiteral( "qcopilots" );

  QString ensureDirectory( const QString &path )
  {
    QDir().mkpath( path );
    return path;
  }

  QString webEngineRootDirectory()
  {
    return ensureDirectory( QDir( QgsQCopilotsUtils::pluginDataDirectory() ).filePath( QStringLiteral( "webengine" ) ) );
  }

  QString processWebEngineProfileName()
  {
    return QStringLiteral( "qcopilots-%1" ).arg( QCoreApplication::applicationPid() );
  }

  bool ownsStableWebEngineProfile()
  {
    static QLockFile sLock( QDir( webEngineRootDirectory() ).filePath( QStringLiteral( "profile.lock" ) ) );
    static bool sLockAttempted = false;
    static bool sOwnsLock = false;
    if ( !sLockAttempted )
    {
      sLock.setStaleLockTime( 0 );
      sOwnsLock = sLock.tryLock( 0 );
      sLockAttempted = true;
    }
    return sOwnsLock;
  }

  QString webEngineProfileDirectory()
  {
    const QString profileName = QgsQCopilotsUtils::webEngineProfileName();
    if ( profileName == sStableWebEngineProfileName )
      return webEngineRootDirectory();

    return ensureDirectory( QDir( webEngineRootDirectory() ).filePath( profileName ) );
  }

  QString levelName( Qgis::MessageLevel level )
  {
    switch ( level )
    {
      case Qgis::Info:
        return QStringLiteral( "INFO" );
      case Qgis::Warning:
        return QStringLiteral( "WARNING" );
      case Qgis::Critical:
        return QStringLiteral( "CRITICAL" );
      case Qgis::Success:
        return QStringLiteral( "SUCCESS" );
      case Qgis::NoLevel:
        return QStringLiteral( "NOLEVEL" );
    }
    return QStringLiteral( "UNKNOWN" );
  }

  void rotateLogFileIfNeeded( const QString &path )
  {
    const QFileInfo info( path );
    if ( !info.exists() || info.size() < sMaximumLogFileSize )
      return;

    const QString rotatedPath = path + QStringLiteral( ".1" );
    QFile::remove( rotatedPath );
    QFile::rename( path, rotatedPath );
  }
}

QString QgsQCopilotsUtils::logCategory()
{
  return QStringLiteral( "QCopilots" );
}

QUrl QgsQCopilotsUtils::defaultServerUrl()
{
  return QUrl( QStringLiteral( "http://127.0.0.1:8282" ) );
}

bool QgsQCopilotsUtils::isSupportedWebUiUrl( const QUrl &url )
{
  const QString scheme = url.scheme().toLower();
  return url.isValid() && ( scheme == QLatin1String( "http" ) || scheme == QLatin1String( "https" ) );
}

QUrl QgsQCopilotsUtils::urlFromUserInput( const QString &input )
{
  const QString trimmed = input.trimmed();
  if ( trimmed.isEmpty() )
    return QUrl();

  const QUrl url = QUrl::fromUserInput( trimmed );
  if ( !isSupportedWebUiUrl( url ) )
    return QUrl();

  return url;
}

QUrl QgsQCopilotsUtils::normalizedWebUiUrl( const QUrl &url )
{
  return normalizedWebUiUrl( url, defaultServerUrl() );
}

QUrl QgsQCopilotsUtils::normalizedWebUiUrl( const QUrl &url, const QUrl &fallback )
{
  QUrl candidate = url;
  if ( candidate.isRelative() || candidate.scheme().isEmpty() )
    candidate = QUrl::fromUserInput( candidate.toString() );

  if ( isSupportedWebUiUrl( candidate ) )
    return candidate.adjusted( QUrl::NormalizePathSegments );

  return fallback;
}

QString QgsQCopilotsUtils::displayUrl( const QUrl &url )
{
  if ( !url.isValid() )
    return QString();

  QUrl sanitized( url );
  sanitized.setUserInfo( QString() );
  sanitized.setQuery( QString() );
  sanitized.setFragment( QString() );
  return sanitized.toString();
}

QString QgsQCopilotsUtils::pluginDataDirectory()
{
  return ensureDirectory( QDir( QgsApplication::qgisSettingsDirPath() ).filePath( QStringLiteral( "qcopilots" ) ) );
}

QString QgsQCopilotsUtils::logsDirectory()
{
  return ensureDirectory( QDir( pluginDataDirectory() ).filePath( QStringLiteral( "logs" ) ) );
}

QString QgsQCopilotsUtils::logFilePath()
{
  return QDir( logsDirectory() ).filePath( QStringLiteral( "qcopilots-container.log" ) );
}

QString QgsQCopilotsUtils::webEngineProfileName()
{
  static const QString sProfileName = ownsStableWebEngineProfile()
                                      ? sStableWebEngineProfileName
                                      : processWebEngineProfileName();
  return sProfileName;
}

QString QgsQCopilotsUtils::webEngineStorageDirectory()
{
  return ensureDirectory( QDir( webEngineProfileDirectory() ).filePath( QStringLiteral( "storage" ) ) );
}

QString QgsQCopilotsUtils::webEngineCacheDirectory()
{
  return ensureDirectory( QDir( webEngineProfileDirectory() ).filePath( QStringLiteral( "cache" ) ) );
}

void QgsQCopilotsUtils::logMessage( const QString &message, Qgis::MessageLevel level )
{
  const QString timestamp = QDateTime::currentDateTime().toString( QStringLiteral( "yyyy-MM-dd HH:mm:ss.zzz" ) );
  const QString line = QStringLiteral( "[%1] [%2] %3\n" ).arg( timestamp, levelName( level ), message );

  const QString path = logFilePath();
  rotateLogFileIfNeeded( path );

  QFile file( path );
  if ( file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
  {
    QTextStream stream( &file );
    stream.setEncoding( QStringConverter::Utf8 );
    stream << line;
  }

  QgsMessageLog::logMessage( message, logCategory(), level );
}
