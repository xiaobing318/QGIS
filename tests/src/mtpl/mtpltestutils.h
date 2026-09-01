/***************************************************************************
  mtpltestutils.h
  ----------------
  Test-only artifact and settings helpers for the built-in MTPL plugin.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MTPLTESTUTILS_H
#define MTPLTESTUTILS_H

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>

#include "qgssettings.h"

#include <memory>

namespace QgsMtplTest
{

class ArtifactWorkspace
{
  public:
    bool initialize( const QString &suiteName, QString &error )
    {
      const QString artifactRoot = qEnvironmentVariable( "QGIS_MTPL_TEST_ARTIFACT_ROOT" ).trimmed();
      if ( artifactRoot.isEmpty() )
      {
        const QString pattern = QDir( QDir::tempPath() ).filePath(
          QStringLiteral( "qgis-mtpl-%1-XXXXXX" ).arg( suiteName ) );
        mTemporaryDirectory = std::make_unique<QTemporaryDir>( pattern );
        if ( !mTemporaryDirectory->isValid() )
        {
          error = QStringLiteral( "Unable to create a temporary MTPL test workspace." );
          return false;
        }
        mPath = mTemporaryDirectory->path();
        error.clear();
        return true;
      }

      const QFileInfo artifactInfo( artifactRoot );
      if ( artifactInfo.exists() && !artifactInfo.isDir() )
      {
        error = QStringLiteral( "QGIS_MTPL_TEST_ARTIFACT_ROOT is not a directory." );
        return false;
      }
      if ( !artifactInfo.exists() && !QDir().mkpath( artifactRoot ) )
      {
        error = QStringLiteral( "Unable to create QGIS_MTPL_TEST_ARTIFACT_ROOT." );
        return false;
      }

      mPath = QDir( artifactRoot ).filePath(
        QStringLiteral( "%1-%2" ).arg( suiteName, QUuid::createUuid().toString( QUuid::WithoutBraces ) ) );
      if ( !QDir().mkpath( mPath ) )
      {
        error = QStringLiteral( "Unable to create the MTPL test artifact workspace." );
        mPath.clear();
        return false;
      }
      mPath = QFileInfo( mPath ).absoluteFilePath();
      error.clear();
      return true;
    }

    QString path() const { return mPath; }

    QString filePath( const QString &relativePath ) const
    {
      return QDir( mPath ).filePath( relativePath );
    }

  private:
    std::unique_ptr<QTemporaryDir> mTemporaryDirectory;
    QString mPath;
};

class PluginSettingsSnapshot
{
  public:
    void capture( const QStringList &keys )
    {
      if ( mCaptured )
        return;

      QgsSettings settings;
      for ( const QString &key : keys )
      {
        Entry entry;
        entry.exists = settings.contains( key, QgsSettings::Section::Plugins );
        if ( entry.exists )
          entry.value = settings.value( key, QVariant(), QgsSettings::Section::Plugins );
        mEntries.insert( key, entry );
      }
      mCaptured = true;
    }

    void restore()
    {
      if ( !mCaptured )
        return;

      QgsSettings settings;
      for ( auto it = mEntries.cbegin(); it != mEntries.cend(); ++it )
      {
        if ( it.value().exists )
          settings.setValue( it.key(), it.value().value, QgsSettings::Section::Plugins );
        else
          settings.remove( it.key(), QgsSettings::Section::Plugins );
      }
      settings.sync();
      mEntries.clear();
      mCaptured = false;
    }

  private:
    struct Entry
    {
      bool exists = false;
      QVariant value;
    };

    QMap<QString, Entry> mEntries;
    bool mCaptured = false;
};

} // namespace QgsMtplTest

#endif // MTPLTESTUTILS_H
