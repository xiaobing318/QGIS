/***************************************************************************
    qcopilots_mcp_bridge.cpp
    ------------------------
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

#include "qcopilots_mcp_bridge.h"

#include "qcopilots_mcp_catalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QWebEnginePage>

#include "moc_qcopilots_mcp_bridge.cpp"

namespace
{
  constexpr qsizetype sMaximumRequestBodyBytes = 1024 * 1024;
  constexpr qsizetype sMaximumResponseBodyBytes = 16 * 1024 * 1024;
  constexpr qsizetype sMaximumHeaderValueBytes = 8192;
  constexpr int sMinimumTimeoutMs = 100;
  constexpr int sMaximumTimeoutMs = 300000;

  const QRegularExpression &requestIdPattern()
  {
    static const QRegularExpression sPattern( QStringLiteral( "^[A-Za-z0-9._:-]{1,128}$" ) );
    return sPattern;
  }

  int defaultPortForScheme( const QString &scheme )
  {
    if ( scheme == QLatin1String( "http" ) )
      return 80;
    if ( scheme == QLatin1String( "https" ) )
      return 443;
    return -1;
  }

  bool sameOrigin( const QUrl &left, const QUrl &right )
  {
    if ( !left.isValid() || !right.isValid() )
      return false;

    const QString leftScheme = left.scheme().toLower();
    const QString rightScheme = right.scheme().toLower();
    if ( ( leftScheme != QLatin1String( "http" ) && leftScheme != QLatin1String( "https" ) )
         || ( rightScheme != QLatin1String( "http" ) && rightScheme != QLatin1String( "https" ) ) )
      return false;

    return leftScheme == rightScheme
           && left.host().compare( right.host(), Qt::CaseInsensitive ) == 0
           && left.port( defaultPortForScheme( leftScheme ) ) == right.port( defaultPortForScheme( rightScheme ) );
  }

  const QSet<QByteArray> &allowedRequestHeaders()
  {
    static const QSet<QByteArray> sHeaders = {
      QByteArrayLiteral( "accept" ),
      QByteArrayLiteral( "content-type" ),
      QByteArrayLiteral( "last-event-id" ),
      QByteArrayLiteral( "mcp-protocol-version" ),
      QByteArrayLiteral( "mcp-session-id" ),
    };
    return sHeaders;
  }

  const QSet<QByteArray> &allowedResponseHeaders()
  {
    static const QSet<QByteArray> sHeaders = {
      QByteArrayLiteral( "cache-control" ),
      QByteArrayLiteral( "content-type" ),
      QByteArrayLiteral( "mcp-protocol-version" ),
      QByteArrayLiteral( "mcp-session-id" ),
      QByteArrayLiteral( "retry-after" ),
    };
    return sHeaders;
  }

  QString canonicalResponseHeaderName( const QByteArray &name )
  {
    const QByteArray lower = name.toLower();
    if ( lower == QByteArrayLiteral( "content-type" ) )
      return QStringLiteral( "Content-Type" );
    if ( lower == QByteArrayLiteral( "cache-control" ) )
      return QStringLiteral( "Cache-Control" );
    if ( lower == QByteArrayLiteral( "last-event-id" ) )
      return QStringLiteral( "Last-Event-ID" );
    if ( lower == QByteArrayLiteral( "mcp-protocol-version" ) )
      return QStringLiteral( "MCP-Protocol-Version" );
    if ( lower == QByteArrayLiteral( "mcp-session-id" ) )
      return QStringLiteral( "MCP-Session-Id" );
    if ( lower == QByteArrayLiteral( "retry-after" ) )
      return QStringLiteral( "Retry-After" );
    return QString::fromLatin1( name );
  }

  bool headerValueIsSafe( const QByteArray &value )
  {
    return value.size() <= sMaximumHeaderValueBytes
           && !value.contains( '\r' )
           && !value.contains( '\n' )
           && !value.contains( '\0' );
  }

  bool isRedirectStatus( int status )
  {
    switch ( status )
    {
      case 301:
      case 302:
      case 303:
      case 305:
      case 307:
      case 308:
        return true;

      default:
        return false;
    }
  }
}

QgsQCopilotsMcpBridge::QgsQCopilotsMcpBridge( QgsQCopilotsMcpCatalog *catalog, QWebEnginePage *page, QObject *parent )
  : QObject( parent )
  , mCatalog( catalog )
  , mPage( page )
  , mNetworkAccessManager( new QNetworkAccessManager( this ) )
{
  mNetworkAccessManager->setProxy( QNetworkProxy::NoProxy );

  if ( mCatalog )
  {
    connect( mCatalog, &QgsQCopilotsMcpCatalog::catalogChanged, this, [this]() {
      cancelAllRequests( QStringLiteral( "catalog_changed" ), tr( "The local MCP service catalog changed." ) );
      updatePublicCatalog();
    } );
  }
  if ( mPage )
  {
    connect( mPage, &QWebEnginePage::loadStarted, this, [this]() {
      cancelAllRequests( QStringLiteral( "navigation_started" ), tr( "The top-level page started navigating." ) );
    } );
    connect( mPage, &QWebEnginePage::urlChanged, this, [this]() {
      if ( !trustedTopLevelOrigin() )
        cancelAllRequests( QStringLiteral( "origin_not_allowed" ), tr( "The top-level page left the configured QCopilots origin." ) );
    } );
  }
  updatePublicCatalog();
}

QgsQCopilotsMcpBridge::~QgsQCopilotsMcpBridge()
{
  cancelAllRequests( QStringLiteral( "bridge_closed" ), tr( "The local MCP bridge was closed." ) );
}

void QgsQCopilotsMcpBridge::setConfiguredUrl( const QUrl &url )
{
  if ( mConfiguredUrl == url )
    return;

  mConfiguredUrl = url;
  cancelAllRequests( QStringLiteral( "origin_changed" ), tr( "The configured QCopilots origin changed." ) );
}

QString QgsQCopilotsMcpBridge::catalog() const
{
  return mPublicCatalogJson;
}

void QgsQCopilotsMcpBridge::request( const QString &requestId, const QString &method, const QString &virtualUrl, const QVariantMap &headers, const QString &body, int timeoutMs )
{
  const auto reject = [this, &requestId]( const QString &code, const QString &message ) {
    emit requestFailed( requestId, code, message );
  };

  if ( !requestIdPattern().match( requestId ).hasMatch() || mPendingRequests.contains( requestId ) )
  {
    reject( QStringLiteral( "invalid_request_id" ), tr( "The local MCP request id is invalid or already active." ) );
    return;
  }

  if ( !trustedTopLevelOrigin() )
  {
    reject( QStringLiteral( "origin_not_allowed" ), tr( "The top-level page origin does not match the configured QCopilots origin." ) );
    return;
  }

  const QString normalizedMethod = method.toUpper();
  if ( normalizedMethod != QLatin1String( "GET" )
       && normalizedMethod != QLatin1String( "POST" )
       && normalizedMethod != QLatin1String( "DELETE" ) )
  {
    reject( QStringLiteral( "method_not_allowed" ), tr( "Only GET, POST, and DELETE are allowed for local MCP requests." ) );
    return;
  }

  if ( timeoutMs < sMinimumTimeoutMs || timeoutMs > sMaximumTimeoutMs )
  {
    reject( QStringLiteral( "invalid_timeout" ), tr( "The local MCP request timeout is outside the allowed range." ) );
    return;
  }

  const QUrl parsedVirtualUrl = QUrl::fromEncoded( virtualUrl.toUtf8(), QUrl::StrictMode );
  QgsQCopilotsMcpService service;
  if ( !mCatalog || !mCatalog->serviceForVirtualUrl( parsedVirtualUrl, service )
       || parsedVirtualUrl.toString( QUrl::FullyEncoded ) != service.virtualUrl.toString( QUrl::FullyEncoded ) )
  {
    reject( QStringLiteral( "unknown_service" ), tr( "The virtual MCP URL is not present in the current runtime catalog." ) );
    return;
  }

  if ( service.state != QLatin1String( "running" ) )
  {
    reject( QStringLiteral( "service_not_running" ), tr( "The selected local MCP service is not running." ) );
    return;
  }

  const QByteArray requestBody = body.toUtf8();
  if ( requestBody.size() > sMaximumRequestBodyBytes )
  {
    reject( QStringLiteral( "body_too_large" ), tr( "The local MCP request body exceeds the 1 MiB limit." ) );
    return;
  }

  if ( normalizedMethod != QLatin1String( "POST" ) && !requestBody.isEmpty() )
  {
    reject( QStringLiteral( "body_not_allowed" ), tr( "Only POST local MCP requests may contain a body." ) );
    return;
  }

  QNetworkRequest networkRequest( service.targetUrl );
  networkRequest.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy );
  networkRequest.setAttribute( QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork );
  networkRequest.setTransferTimeout( timeoutMs );

  QSet<QByteArray> seenHeaders;
  for ( auto iterator = headers.constBegin(); iterator != headers.constEnd(); ++iterator )
  {
    const QByteArray headerName = iterator.key().toLatin1().toLower();
    if ( headerName == QByteArrayLiteral( "authorization" ) )
      continue;

    if ( iterator.value().metaType().id() != QMetaType::QString )
    {
      reject( QStringLiteral( "invalid_header" ), tr( "Local MCP request header values must be strings." ) );
      return;
    }

    const QByteArray headerValue = iterator.value().toString().toLatin1();
    if ( !allowedRequestHeaders().contains( headerName ) || seenHeaders.contains( headerName ) || !headerValueIsSafe( headerValue ) )
    {
      reject( QStringLiteral( "invalid_header" ), tr( "The local MCP request contains an unsupported or invalid header." ) );
      return;
    }

    seenHeaders.insert( headerName );
    networkRequest.setRawHeader( headerName, headerValue );
  }

  networkRequest.setRawHeader( QByteArrayLiteral( "authorization" ), QByteArrayLiteral( "Bearer " ) + service.authToken );

  QNetworkReply *reply = nullptr;
  if ( normalizedMethod == QLatin1String( "GET" ) )
    reply = mNetworkAccessManager->get( networkRequest );
  else if ( normalizedMethod == QLatin1String( "DELETE" ) )
    reply = mNetworkAccessManager->deleteResource( networkRequest );
  else
    reply = mNetworkAccessManager->post( networkRequest, requestBody );

  auto *pending = new PendingRequest;
  pending->reply = reply;
  pending->timer = new QTimer( this );
  pending->timer->setSingleShot( true );
  pending->timer->setInterval( timeoutMs );
  mPendingRequests.insert( requestId, pending );

  connect( pending->timer, &QTimer::timeout, this, [this, requestId]() {
    failRequest( requestId, QStringLiteral( "timeout" ), tr( "The local MCP request timed out." ) );
  } );
  connect( reply, &QNetworkReply::readyRead, this, [this, requestId]() {
    const auto found = mPendingRequests.constFind( requestId );
    if ( found == mPendingRequests.constEnd() || !( *found )->reply )
      return;

    PendingRequest *pendingRequest = *found;
    pendingRequest->responseBody.append( pendingRequest->reply->readAll() );
    if ( pendingRequest->responseBody.size() > sMaximumResponseBodyBytes )
      failRequest( requestId, QStringLiteral( "response_too_large" ), tr( "The local MCP response body exceeds the 16 MiB limit." ) );
  } );
  connect( reply, &QNetworkReply::finished, this, [this, requestId]() {
    finishRequest( requestId );
  } );
  pending->timer->start();
}

void QgsQCopilotsMcpBridge::cancel( const QString &requestId )
{
  if ( !mPendingRequests.contains( requestId ) )
    return;

  failRequest( requestId, QStringLiteral( "canceled" ), tr( "The local MCP request was canceled." ) );
}

void QgsQCopilotsMcpBridge::updatePublicCatalog()
{
  const QgsQCopilotsMcpCatalogSnapshot snapshot = mCatalog
                                                   ? mCatalog->snapshot()
                                                   : QgsQCopilotsMcpCatalogSnapshot();
  if ( !snapshot.valid && !mPublicCatalogJson.isEmpty() )
    return;

  QJsonObject root;
  root.insert( QStringLiteral( "schemaVersion" ), 1 );
  root.insert( QStringLiteral( "generation" ), mLastPublicGeneration );
  root.insert( QStringLiteral( "startupComplete" ), false );
  QJsonArray services;

  if ( snapshot.valid )
  {
    mLastPublicGeneration = snapshot.generation;
    root.insert( QStringLiteral( "generation" ), snapshot.generation );
    root.insert( QStringLiteral( "startupComplete" ), snapshot.startupComplete );
    for ( const QgsQCopilotsMcpService &service : snapshot.services )
    {
      QJsonObject publicService;
      publicService.insert( QStringLiteral( "id" ), service.id );
      publicService.insert( QStringLiteral( "displayName" ), service.displayName );
      publicService.insert( QStringLiteral( "state" ), service.state );
      publicService.insert( QStringLiteral( "virtualUrl" ), service.virtualUrl.toString( QUrl::FullyEncoded ) );
      services.append( publicService );
    }
  }

  root.insert( QStringLiteral( "services" ), services );
  const QString serialized = QString::fromUtf8( QJsonDocument( root ).toJson( QJsonDocument::Compact ) );
  if ( serialized == mPublicCatalogJson )
    return;

  mPublicCatalogJson = serialized;
  emit catalogChanged( mPublicCatalogJson );
}

void QgsQCopilotsMcpBridge::finishRequest( const QString &requestId )
{
  const auto found = mPendingRequests.find( requestId );
  if ( found == mPendingRequests.end() )
    return;

  PendingRequest *pending = found.value();
  if ( !trustedTopLevelOrigin() )
  {
    failRequest( requestId, QStringLiteral( "origin_not_allowed" ), tr( "The top-level page origin no longer matches the configured QCopilots origin." ) );
    return;
  }

  QNetworkReply *reply = pending->reply;
  if ( !reply )
  {
    failRequest( requestId, QStringLiteral( "network_error" ), tr( "The local MCP network reply disappeared." ), false );
    return;
  }

  pending->responseBody.append( reply->readAll() );
  if ( pending->responseBody.size() > sMaximumResponseBodyBytes )
  {
    failRequest( requestId, QStringLiteral( "response_too_large" ), tr( "The local MCP response body exceeds the 16 MiB limit." ) );
    return;
  }

  const int status = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
  const bool redirected = reply->attribute( QNetworkRequest::RedirectionTargetAttribute ).isValid()
                          || isRedirectStatus( status );
  if ( redirected )
  {
    failRequest( requestId, QStringLiteral( "redirect_not_allowed" ), tr( "Redirects are not allowed for local MCP requests." ) );
    return;
  }

  if ( reply->error() != QNetworkReply::NoError && status <= 0 )
  {
    failRequest( requestId, QStringLiteral( "network_error" ), tr( "The local MCP request failed: %1" ).arg( reply->errorString() ) );
    return;
  }

  QVariantMap responseHeaders;
  for ( const auto &header : reply->rawHeaderPairs() )
  {
    const QByteArray lowerName = header.first.toLower();
    if ( allowedResponseHeaders().contains( lowerName ) && headerValueIsSafe( header.second ) )
      responseHeaders.insert( canonicalResponseHeaderName( lowerName ), QString::fromLatin1( header.second ) );
  }

  const QString statusText = reply->attribute( QNetworkRequest::HttpReasonPhraseAttribute ).toString();
  const QString bodyBase64 = QString::fromLatin1( pending->responseBody.toBase64() );

  mPendingRequests.erase( found );
  if ( pending->timer )
    pending->timer->deleteLater();
  reply->deleteLater();
  delete pending;

  emit response( requestId, status, statusText, responseHeaders, bodyBase64 );
}

void QgsQCopilotsMcpBridge::failRequest( const QString &requestId, const QString &code, const QString &message, bool abortReply )
{
  const auto found = mPendingRequests.find( requestId );
  if ( found == mPendingRequests.end() )
  {
    emit requestFailed( requestId, code, message );
    return;
  }

  PendingRequest *pending = found.value();
  mPendingRequests.erase( found );
  if ( pending->timer )
    pending->timer->deleteLater();
  if ( pending->reply )
  {
    disconnect( pending->reply, nullptr, this, nullptr );
    if ( abortReply )
      pending->reply->abort();
    pending->reply->deleteLater();
  }
  delete pending;
  emit requestFailed( requestId, code, message );
}

void QgsQCopilotsMcpBridge::cancelAllRequests( const QString &code, const QString &message )
{
  const QStringList requestIds = mPendingRequests.keys();
  for ( const QString &requestId : requestIds )
    failRequest( requestId, code, message );
}

bool QgsQCopilotsMcpBridge::trustedTopLevelOrigin() const
{
  return mPage && sameOrigin( mPage->url(), mConfiguredUrl );
}
