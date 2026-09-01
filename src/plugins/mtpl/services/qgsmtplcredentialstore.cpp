/***************************************************************************
  qgsmtplcredentialstore.cpp
  --------------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmtplcredentialstore.h"

#include "qgssettings.h"

namespace
{
  const QString sRememberKey = QStringLiteral( "mtpl/credentials/remember" );
  const QString sPrivateKey = QStringLiteral( "mtpl/credentials/privateKey" );
  const QString sDeviceKey = QStringLiteral( "mtpl/credentials/deviceKey" );
  const QString sLastPathKey = QStringLiteral( "mtpl/lastPath" );

  void removeCredentialValues( QgsSettings &settings )
  {
    settings.remove( sPrivateKey, QgsSettings::Section::Plugins );
    settings.remove( sDeviceKey, QgsSettings::Section::Plugins );
  }
}

bool QgsMtplCredentialStore::rememberEnabled()
{
  const QgsSettings settings;
  return settings.value( sRememberKey, true, QgsSettings::Section::Plugins ).toBool();
}

void QgsMtplCredentialStore::setRememberEnabled( bool enabled )
{
  QgsSettings settings;
  settings.setValue( sRememberKey, enabled, QgsSettings::Section::Plugins );
  if ( !enabled )
    removeCredentialValues( settings );
  settings.sync();
}

QgsMtpl::CryptoKeys QgsMtplCredentialStore::rememberedKeys( QString *error )
{
  if ( error )
    error->clear();

  QgsMtpl::CryptoKeys keys;
  if ( !rememberEnabled() )
    return keys;

  const QgsSettings settings;
  keys.privateKey = settings.value( sPrivateKey, QString(), QgsSettings::Section::Plugins ).toString().toLatin1();
  keys.deviceKey = settings.value( sDeviceKey, QString(), QgsSettings::Section::Plugins ).toString().toLatin1();
  if ( keys.isEmpty() )
    return keys;

  QString validationError;
  if ( !keys.isValid( &validationError ) )
  {
    keys.clear();
    if ( error )
      *error = validationError;
  }
  return keys;
}

bool QgsMtplCredentialStore::saveManualKeys( const QgsMtpl::CryptoKeys &keys,
                                              const QString &lastPath,
                                              QString &error )
{
  if ( !keys.isValid( &error ) )
    return false;

  const bool shouldRemember = rememberEnabled();
  QgsSettings settings;
  settings.setValue( sLastPathKey, lastPath, QgsSettings::Section::Plugins );
  if ( shouldRemember )
  {
    settings.setValue( sPrivateKey, QString::fromLatin1( keys.privateKey ), QgsSettings::Section::Plugins );
    settings.setValue( sDeviceKey, QString::fromLatin1( keys.deviceKey ), QgsSettings::Section::Plugins );
  }
  else
  {
    removeCredentialValues( settings );
  }
  settings.sync();
  error.clear();
  return true;
}

QString QgsMtplCredentialStore::lastPath()
{
  const QgsSettings settings;
  return settings.value( sLastPathKey, QString(), QgsSettings::Section::Plugins ).toString();
}

void QgsMtplCredentialStore::setLastPath( const QString &path )
{
  QgsSettings settings;
  settings.setValue( sLastPathKey, path, QgsSettings::Section::Plugins );
  settings.sync();
}

void QgsMtplCredentialStore::clearRememberedKeys()
{
  QgsSettings settings;
  removeCredentialValues( settings );
  settings.sync();
}

void QgsMtplCredentialStore::clear()
{
  QgsSettings settings;
  settings.remove( QStringLiteral( "mtpl/credentials" ), QgsSettings::Section::Plugins );
  settings.remove( sLastPathKey, QgsSettings::Section::Plugins );
  settings.sync();
}
