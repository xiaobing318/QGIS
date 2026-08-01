/***************************************************************************
    qcopilots_mcp_catalog.cpp
    -------------------------
    begin                : August 2026
    copyright            : (C) 2026
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qcopilots_mcp_catalog.h"

#include "qcopilots_container_utils.h"

#include <QCoreApplication>
#include <QDynamicPropertyChangeEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaType>
#include <QRegularExpression>
#include <QSet>

#include <cmath>
#include "moc_qcopilots_mcp_catalog.cpp"

namespace
{
  constexpr auto sCatalogPropertyName = "qcopilotsMcpRuntimeCatalog";
  constexpr qsizetype sMaximumCatalogBytes = 1024 * 1024;
  constexpr qsizetype sMaximumServices = 128;
  constexpr qint64 sMaximumSafeJsonInteger = 9007199254740991LL;

  const QRegularExpression &serviceIdPattern()
  {
    static const QRegularExpression sPattern( QStringLiteral( "^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$" ) );
    return sPattern;
  }

  bool hasExactKeys( const QJsonObject &object, QStringList expected )
  {
    QStringList actual = object.keys();
    actual.sort();
    expected.sort();
    return actual == expected;
  }

  bool isJsonInteger( const QJsonValue &value, qint64 minimum, qint64 maximum, qint64 &result )
  {
    if ( !value.isDouble() )
      return false;

    const double number = value.toDouble();
    if ( !std::isfinite( number ) || std::floor( number ) != number
         || number < static_cast<double>( minimum ) || number > static_cast<double>( maximum ) )
      return false;

    result = static_cast<qint64>( number );
    return true;
  }

  bool isCleanString( const QString &value, qsizetype maximumLength )
  {
    if ( value.isEmpty() || value.size() > maximumLength || value != value.trimmed() )
      return false;

    for ( const QChar character : value )
    {
      if ( character.unicode() < 0x20 || character.unicode() == 0x7f )
        return false;
    }
    return true;
  }

  bool isVirtualUrlForService( const QUrl &url, const QString &serviceId )
  {
    return url.isValid()
           && url.scheme() == QLatin1String( "https" )
           && url.host() == QLatin1String( "qcopilots.localmachine" )
           && url.port() == -1
           && url.userInfo().isEmpty()
           && url.query().isEmpty()
           && url.fragment().isEmpty()
           && url.path( QUrl::FullyDecoded ) == QStringLiteral( "/mcp/%1" ).arg( serviceId );
  }

  bool isLoopbackMcpTarget( const QUrl &url )
  {
    return url.isValid()
           && url.scheme() == QLatin1String( "http" )
           && url.host() == QLatin1String( "127.0.0.1" )
           && url.port() >= 1
           && url.port() <= 65535
           && url.userInfo().isEmpty()
           && url.path( QUrl::FullyEncoded ) == QLatin1String( "/mcp" )
           && url.query().isEmpty()
           && url.fragment().isEmpty();
  }

  bool isBearerToken( const QString &token )
  {
    if ( token.size() < 16 || token.size() > 4096 )
      return false;

    for ( const QChar character : token )
    {
      if ( character.isSpace() || character.unicode() < 0x21 || character.unicode() == 0x7f )
        return false;
    }
    return true;
  }

  bool isServiceState( const QString &state )
  {
    static const QSet<QString> sStates = {
      QStringLiteral( "failed" ),
      QStringLiteral( "running" ),
      QStringLiteral( "starting" ),
      QStringLiteral( "stopped" ),
    };
    return sStates.contains( state );
  }

  QString virtualUrlKey( const QUrl &url )
  {
    return url.toString( QUrl::FullyEncoded );
  }
}

QgsQCopilotsMcpCatalog::QgsQCopilotsMcpCatalog( QObject *parent )
  : QObject( parent )
  , mApplication( QCoreApplication::instance() )
{
  if ( mApplication )
  {
    mApplication->installEventFilter( this );
    refresh();
  }
}

QgsQCopilotsMcpCatalog::~QgsQCopilotsMcpCatalog()
{
  if ( mApplication )
    mApplication->removeEventFilter( this );
}

QgsQCopilotsMcpCatalogSnapshot QgsQCopilotsMcpCatalog::snapshot() const
{
  return mSnapshot;
}

bool QgsQCopilotsMcpCatalog::serviceForVirtualUrl( const QUrl &virtualUrl, QgsQCopilotsMcpService &service ) const
{
  if ( !mSnapshot.valid )
    return false;

  const auto found = mServicesByVirtualUrl.constFind( virtualUrlKey( virtualUrl ) );
  if ( found == mServicesByVirtualUrl.constEnd() )
    return false;

  service = found.value();
  return true;
}

bool QgsQCopilotsMcpCatalog::eventFilter( QObject *watched, QEvent *event )
{
  if ( watched == mApplication && event && event->type() == QEvent::DynamicPropertyChange )
  {
    const auto *propertyEvent = static_cast<QDynamicPropertyChangeEvent *>( event );
    if ( propertyEvent->propertyName() == sCatalogPropertyName )
      refresh();
  }

  return QObject::eventFilter( watched, event );
}

void QgsQCopilotsMcpCatalog::refresh()
{
  if ( !mApplication )
  {
    clearCatalog( tr( "QCopilots MCP runtime catalog is unavailable because the application object is missing." ) );
    return;
  }

  const QVariant property = mApplication->property( sCatalogPropertyName );
  if ( !property.isValid() || property.isNull() )
  {
    clearCatalog( QString() );
    return;
  }

  if ( property.metaType().id() != QMetaType::QString )
  {
    clearCatalog( tr( "QCopilots MCP runtime catalog must be a JSON string." ) );
    return;
  }

  const QByteArray serialized = property.toString().toUtf8();
  if ( serialized.isEmpty() || serialized.size() > sMaximumCatalogBytes )
  {
    clearCatalog( tr( "QCopilots MCP runtime catalog is empty or exceeds the 1 MiB limit." ) );
    return;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson( serialized, &parseError );
  if ( parseError.error != QJsonParseError::NoError || !document.isObject() )
  {
    clearCatalog( tr( "QCopilots MCP runtime catalog is not a valid JSON object." ) );
    return;
  }

  const QJsonObject root = document.object();
  if ( !hasExactKeys( root, { QStringLiteral( "schemaVersion" ), QStringLiteral( "generation" ), QStringLiteral( "startupComplete" ), QStringLiteral( "services" ) } ) )
  {
    clearCatalog( tr( "QCopilots MCP runtime catalog has an unexpected top-level shape." ) );
    return;
  }

  qint64 schemaVersion = 0;
  qint64 generation = -1;
  if ( !isJsonInteger( root.value( QStringLiteral( "schemaVersion" ) ), 1, 1, schemaVersion )
       || !isJsonInteger( root.value( QStringLiteral( "generation" ) ), 0, sMaximumSafeJsonInteger, generation )
       || !root.value( QStringLiteral( "startupComplete" ) ).isBool()
       || !root.value( QStringLiteral( "services" ) ).isArray() )
  {
    clearCatalog( tr( "QCopilots MCP runtime catalog has invalid schema fields." ) );
    return;
  }

  const QJsonArray serviceArray = root.value( QStringLiteral( "services" ) ).toArray();
  if ( serviceArray.size() > sMaximumServices )
  {
    clearCatalog( tr( "QCopilots MCP runtime catalog contains too many services." ) );
    return;
  }

  QgsQCopilotsMcpCatalogSnapshot candidate;
  candidate.valid = true;
  candidate.generation = generation;
  candidate.startupComplete = root.value( QStringLiteral( "startupComplete" ) ).toBool();

  QHash<QString, QgsQCopilotsMcpService> servicesByVirtualUrl;
  QSet<QString> serviceIds;
  QSet<QString> virtualUrls;
  for ( const QJsonValue &value : serviceArray )
  {
    if ( !value.isObject() )
    {
      clearCatalog( tr( "QCopilots MCP runtime catalog contains a non-object service." ) );
      return;
    }

    const QJsonObject object = value.toObject();
    if ( !hasExactKeys( object, { QStringLiteral( "id" ), QStringLiteral( "displayName" ), QStringLiteral( "state" ), QStringLiteral( "virtualUrl" ), QStringLiteral( "targetUrl" ), QStringLiteral( "authToken" ) } ) )
    {
      clearCatalog( tr( "QCopilots MCP runtime catalog contains a service with an unexpected shape." ) );
      return;
    }

    for ( const QString &key : { QStringLiteral( "id" ), QStringLiteral( "displayName" ), QStringLiteral( "state" ), QStringLiteral( "virtualUrl" ), QStringLiteral( "targetUrl" ), QStringLiteral( "authToken" ) } )
    {
      if ( !object.value( key ).isString() )
      {
        clearCatalog( tr( "QCopilots MCP runtime catalog service fields must be strings." ) );
        return;
      }
    }

    QgsQCopilotsMcpService service;
    service.id = object.value( QStringLiteral( "id" ) ).toString();
    service.displayName = object.value( QStringLiteral( "displayName" ) ).toString();
    service.state = object.value( QStringLiteral( "state" ) ).toString();
    const QString virtualUrlText = object.value( QStringLiteral( "virtualUrl" ) ).toString();
    const QString targetUrlText = object.value( QStringLiteral( "targetUrl" ) ).toString();
    const QString authToken = object.value( QStringLiteral( "authToken" ) ).toString();

    service.virtualUrl = QUrl::fromEncoded( virtualUrlText.toUtf8(), QUrl::StrictMode );
    service.targetUrl = QUrl::fromEncoded( targetUrlText.toUtf8(), QUrl::StrictMode );
    service.authToken = authToken.toUtf8();

    const bool runningServiceCredentialsValid = service.state == QLatin1String( "running" )
                                                && isLoopbackMcpTarget( service.targetUrl )
                                                && isBearerToken( authToken );
    const bool inactiveServiceCredentialsValid = service.state != QLatin1String( "running" )
                                                 && targetUrlText.isEmpty()
                                                 && authToken.isEmpty();
    if ( !serviceIdPattern().match( service.id ).hasMatch()
         || !isCleanString( service.displayName, 256 )
         || !isServiceState( service.state )
         || !isVirtualUrlForService( service.virtualUrl, service.id )
         || ( !runningServiceCredentialsValid && !inactiveServiceCredentialsValid ) )
    {
      clearCatalog( tr( "QCopilots MCP runtime catalog contains an invalid service." ) );
      return;
    }

    const QString urlKey = virtualUrlKey( service.virtualUrl );
    if ( serviceIds.contains( service.id ) || virtualUrls.contains( urlKey ) )
    {
      clearCatalog( tr( "QCopilots MCP runtime catalog contains a duplicate service id or virtual URL." ) );
      return;
    }

    serviceIds.insert( service.id );
    virtualUrls.insert( urlKey );
    if ( service.state == QLatin1String( "running" ) )
      servicesByVirtualUrl.insert( urlKey, service );
    candidate.services.append( service );
  }

  if ( generation < mHighestGeneration )
  {
    clearCatalog( tr( "QCopilots MCP runtime catalog generation moved backwards." ) );
    return;
  }

  if ( generation == mHighestGeneration )
  {
    if ( mSnapshot.valid && serialized == mSerializedCatalog )
      return;

    clearCatalog( tr( "QCopilots MCP runtime catalog changed without advancing its generation." ) );
    return;
  }

  mSnapshot = candidate;
  mServicesByVirtualUrl = servicesByVirtualUrl;
  mSerializedCatalog = serialized;
  mHighestGeneration = generation;
  emit catalogChanged();
}

void QgsQCopilotsMcpCatalog::clearCatalog( const QString &reason )
{
  const bool changed = mSnapshot.valid || !mSnapshot.services.isEmpty() || !mSerializedCatalog.isEmpty();
  mSnapshot = QgsQCopilotsMcpCatalogSnapshot();
  mServicesByVirtualUrl.clear();
  mSerializedCatalog.clear();

  if ( !reason.isEmpty() )
    QgsQCopilotsUtils::logMessage( reason, Qgis::Warning );

  if ( changed )
    emit catalogChanged();
}
