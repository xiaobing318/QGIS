/***************************************************************************
  qgsauthmtplmethod.cpp
  ---------------------
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

#include "qgsauthmtplmethod.h"

#include "qgsauthconfig.h"

#include "moc_qgsauthmtplmethod.cpp"

#ifdef HAVE_GUI
#include "qgsauthmtpledit.h"
#endif

const QString QgsAuthMtplMethod::AUTH_METHOD_KEY = QStringLiteral( "Mtpl" );
const QString QgsAuthMtplMethod::AUTH_METHOD_DESCRIPTION = QStringLiteral( "MTPL package credentials" );
const QString QgsAuthMtplMethod::AUTH_METHOD_DISPLAY_DESCRIPTION = QObject::tr( "MTPL 数据包密钥" );

QgsAuthMtplMethod::QgsAuthMtplMethod()
{
  setVersion( 1 );
  setExpansions( QgsAuthMethod::Expansions() );
  setDataProviders( QStringList() << QStringLiteral( "mtpl" ) );
}

QString QgsAuthMtplMethod::key() const
{
  return AUTH_METHOD_KEY;
}

QString QgsAuthMtplMethod::description() const
{
  return AUTH_METHOD_DESCRIPTION;
}

QString QgsAuthMtplMethod::displayDescription() const
{
  return AUTH_METHOD_DISPLAY_DESCRIPTION;
}

void QgsAuthMtplMethod::clearCachedConfig( const QString &authcfg )
{
  Q_UNUSED( authcfg )
}

void QgsAuthMtplMethod::updateMethodConfig( QgsAuthMethodConfig &config )
{
  Q_UNUSED( config )
}

#ifdef HAVE_GUI
QWidget *QgsAuthMtplMethod::editWidget( QWidget *parent ) const
{
  return new QgsAuthMtplEdit( parent );
}
#endif

QgsAuthMtplMethodMetadata::QgsAuthMtplMethodMetadata()
  : QgsAuthMethodMetadata( QgsAuthMtplMethod::AUTH_METHOD_KEY,
                           QgsAuthMtplMethod::AUTH_METHOD_DESCRIPTION )
{
}

QgsAuthMtplMethod *QgsAuthMtplMethodMetadata::createAuthMethod() const
{
  return new QgsAuthMtplMethod;
}

#ifndef HAVE_STATIC_PROVIDERS
QGISEXTERN QgsAuthMethodMetadata *authMethodMetadataFactory()
{
  return new QgsAuthMtplMethodMetadata;
}
#endif
