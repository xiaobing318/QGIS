/***************************************************************************
 *  qgsqcopilotsdock.cpp                                                  *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#include "qgsqcopilotsdock.h"

#include "qgsapplication.h"
#include "qgsfileutils.h"
#include "qgsmessagelog.h"
#include "qgssettings.h"

#include <QAuthenticator>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLayout>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStringList>
#include <QStandardPaths>
#include <QSslError>

#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace
{
  const QString sServerUrlKey = QStringLiteral( "QCopilots/QCopilotServerUrl" );
  const QString sLastKnownGoodUrlKey = QStringLiteral( "QCopilots/QCopilotServerLastKnownGoodUrl" );
  const QString sDownloadLastDirKey = QStringLiteral( "QCopilots/QCopilotDownloadLastDir" );
  const QString sLogCategory = QStringLiteral( "QCopilots" );
  const QString sFallbackDownloadFileName = QStringLiteral( "qcopilots-download.txt" );

  bool isProxyRelatedError( QNetworkReply::NetworkError error )
  {
    switch ( error )
    {
      case QNetworkReply::ProxyAuthenticationRequiredError:
      case QNetworkReply::ProxyConnectionRefusedError:
      case QNetworkReply::ProxyConnectionClosedError:
      case QNetworkReply::ProxyNotFoundError:
      case QNetworkReply::ProxyTimeoutError:
        return true;
      default:
        return false;
    }
  }

  bool isHttpSuccessStatus( int statusCode )
  {
    return statusCode >= 200 && statusCode < 400;
  }

  QWebEngineProfile *qcopilotsProfile()
  {
    static QPointer< QWebEngineProfile > sProfile;
    if ( !sProfile )
    {
      sProfile = new QWebEngineProfile( QStringLiteral( "qcopilots" ), QgsApplication::instance() );

      const QString basePath = QDir( QgsApplication::qgisSettingsDirPath() ).filePath( QStringLiteral( "qcopilots/webengine" ) );
      QDir().mkpath( basePath );

      const QString storagePath = QDir( basePath ).filePath( QStringLiteral( "storage" ) );
      const QString cachePath = QDir( basePath ).filePath( QStringLiteral( "cache" ) );
      QDir().mkpath( storagePath );
      QDir().mkpath( cachePath );

      sProfile->setPersistentStoragePath( storagePath );
      sProfile->setCachePath( cachePath );
      sProfile->setHttpCacheType( QWebEngineProfile::DiskHttpCache );
      sProfile->setPersistentCookiesPolicy( QWebEngineProfile::AllowPersistentCookies );
    }
    return sProfile;
  }
} // namespace

bool QgsQCopilotsDock::isAllowedScheme( const QUrl &url )
{
  const QString scheme = url.scheme().toLower();
  return scheme == QLatin1String( "http" ) || scheme == QLatin1String( "https" );
}

QUrl QgsQCopilotsDock::defaultServerUrl()
{
  return QUrl( QStringLiteral( "http://127.0.0.1:8080" ) );
}

QgsQCopilotsDock::QgsQCopilotsDock( QWidget *parent )
  : QDockWidget( parent )
{
  // 允许停靠在任何边缘，允许移动、浮动和关闭
  setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea );
  setFeatures( QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable );

  auto *container = new QWidget( this );
  mUi.setupUi( container );
  setWindowTitle( container->windowTitle() );
  setWidget( container );

  if ( QLayout *layout = container->layout() )
  {
    layout->setContentsMargins( 0, 0, 0, 0 );
  }

  mNetworkAccessManager = new QNetworkAccessManager( this );
  connect( mNetworkAccessManager, &QNetworkAccessManager::proxyAuthenticationRequired, this, [this]( const QNetworkProxy &, QAuthenticator * )
  {
    mLastProxyHint = tr( "Proxy authentication is required for this URL." );
    appendDiagnosticLog( mLastProxyHint, Qgis::Warning );
  } );

  mWebView = mUi.webEngineView;
  if ( mWebView )
  {
    QWebEngineProfile *profile = qcopilotsProfile();
    QWebEnginePage *page = new QWebEnginePage( profile, mWebView );
    mWebView->setPage( page );
    connect( profile, &QWebEngineProfile::downloadRequested, this, &QgsQCopilotsDock::handleDownloadRequested );

    if ( QWebEngineSettings *pageSettings = page->settings() )
    {
      pageSettings->setAttribute( QWebEngineSettings::JavascriptCanAccessClipboard, true );
      pageSettings->setAttribute( QWebEngineSettings::JavascriptCanPaste, true );
      appendDiagnosticLog( tr( "Clipboard JavaScript permissions enabled for QCopilots dock pages." ), Qgis::Info );
    }

    connect( page, &QWebEnginePage::permissionRequested, this, [this]( const QWebEnginePermission &permission )
    {
      if ( !permission.isValid() )
        return;

      if ( permission.permissionType() != QWebEnginePermission::PermissionType::ClipboardReadWrite )
        return;

      permission.grant();
      appendDiagnosticLog(
        tr( "Granted clipboard permission for origin: %1" ).arg( permission.origin().toString() ),
        Qgis::Info );
    } );

    connect( mWebView, &QWebEngineView::loadFinished, this, &QgsQCopilotsDock::handleLoadFinished );
    connect( mWebView, &QWebEngineView::urlChanged, this, [this]( const QUrl &url )
    {
      mLastFinalUrl = url.toString();
    } );
  }

  const QgsSettings settings;
  const QUrl savedServerUrl = normalizeUrl( QUrl( settings.value( sServerUrlKey, defaultServerUrl().toString(), QgsSettings::Section::Plugins ).toString() ) );
  mLastKnownGoodUrl = normalizeUrl( QUrl( settings.value( sLastKnownGoodUrlKey, savedServerUrl.toString(), QgsSettings::Section::Plugins ).toString() ) );
  if ( !mLastKnownGoodUrl.isValid() )
    mLastKnownGoodUrl = savedServerUrl;

  loadUrl( savedServerUrl, false );
}

QgsQCopilotsDock::~QgsQCopilotsDock()
{
  cancelPendingProbe();
}

QUrl QgsQCopilotsDock::serverUrl() const
{
  if ( mServerUrl.isValid() )
    return mServerUrl;
  if ( mPendingUrl.isValid() )
    return mPendingUrl;
  return defaultServerUrl();
}

QString QgsQCopilotsDock::lastFailureSummary() const
{
  return mLastFailureSummary;
}

QUrl QgsQCopilotsDock::normalizeUrl( const QUrl &url ) const
{
  const QUrl defaultUrl = defaultServerUrl();
  if ( !url.isValid() || url.scheme().isEmpty() || !isAllowedScheme( url ) )
    return defaultUrl;

  return url;
}

void QgsQCopilotsDock::persistSuccessfulUrl( const QUrl &url )
{
  if ( !url.isValid() || !isAllowedScheme( url ) )
    return;

  QgsSettings settings;
  settings.setValue( sServerUrlKey, url.toString(), QgsSettings::Section::Plugins );
  settings.setValue( sLastKnownGoodUrlKey, url.toString(), QgsSettings::Section::Plugins );

  mLastKnownGoodUrl = url;
}

void QgsQCopilotsDock::rollbackToLastKnownGoodUrl()
{
  if ( mRollbackInProgress )
    return;

  if ( !mLastKnownGoodUrl.isValid() || mLastKnownGoodUrl == mPendingUrl )
    return;

  mRollbackInProgress = true;
  mPersistPendingUrlOnSuccess = false;
  appendDiagnosticLog( tr( "Rolling back to last known good URL: %1" ).arg( mLastKnownGoodUrl.toString() ), Qgis::Warning );
  loadUrl( mLastKnownGoodUrl, false );
}

void QgsQCopilotsDock::appendDiagnosticLog( const QString &message, Qgis::MessageLevel level )
{
  const QString timestamped = QStringLiteral( "[%1] %2" ).arg(
    QDateTime::currentDateTime().toString( QStringLiteral( "yyyy-MM-dd HH:mm:ss" ) ),
    message );
  QgsMessageLog::logMessage( timestamped, sLogCategory, level );
}

QString QgsQCopilotsDock::httpStatusText() const
{
  if ( mLastHttpStatusCode <= 0 )
    return tr( "Unknown" );

  if ( mLastHttpReason.isEmpty() )
    return QString::number( mLastHttpStatusCode );

  return tr( "%1 %2" ).arg( QString::number( mLastHttpStatusCode ), mLastHttpReason );
}

QString QgsQCopilotsDock::diagnosticHintText() const
{
  QStringList hints;

  if ( !mLastSslError.isEmpty() )
    hints << tr( "TLS/certificate check failed. Verify certificate trust chain and hostname." );

  if ( !mLastProxyHint.isEmpty() )
    hints << mLastProxyHint;

  if ( mLastHttpStatusCode == 401 || mLastHttpStatusCode == 403 )
    hints << tr( "The target endpoint requires authentication or rejected the request." );

  if ( mLastHttpStatusCode >= 500 )
    hints << tr( "The server returned a 5xx response. Check llama-server runtime logs." );

  if ( mLastHttpStatusCode <= 0 && !mLastNetworkError.isEmpty() && mLastSslError.isEmpty() && mLastProxyHint.isEmpty() )
    hints << tr( "Check host/port, firewall rules, and whether the server process is running." );

  if ( hints.isEmpty() )
    hints << tr( "No obvious certificate/proxy issue was detected." );

  return hints.join( QLatin1String( " " ) );
}

void QgsQCopilotsDock::resetProbeState()
{
  mLastHttpStatusCode = -1;
  mLastHttpReason.clear();
  mLastFinalUrl.clear();
  mLastNetworkError.clear();
  mLastSslError.clear();
  mLastProxyHint.clear();
}

void QgsQCopilotsDock::cancelPendingProbe()
{
  if ( !mProbeReply )
    return;

  QObject::disconnect( mProbeReply, nullptr, this, nullptr );
  mProbeReply->abort();
  mProbeReply->deleteLater();
  mProbeReply = nullptr;
}

void QgsQCopilotsDock::startConnectivityProbe( const QUrl &url )
{
  if ( !mNetworkAccessManager )
    return;

  cancelPendingProbe();

  QNetworkRequest request( url );
  request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );

  mProbeReply = mNetworkAccessManager->head( request );
  const QPointer< QNetworkReply > probeReply = mProbeReply;

  if ( !probeReply )
    return;

  connect( probeReply, &QNetworkReply::sslErrors, this, [this, probeReply]( const QList< QSslError > &errors )
  {
    if ( !probeReply )
      return;

    if ( errors.isEmpty() )
      return;

    mLastSslError = errors.constFirst().errorString();
    appendDiagnosticLog( tr( "SSL error: %1" ).arg( mLastSslError ), Qgis::Warning );
  } );

  connect( probeReply, &QNetworkReply::errorOccurred, this, [this, probeReply]( QNetworkReply::NetworkError error )
  {
    if ( !probeReply )
      return;

    mLastNetworkError = probeReply->errorString();
    if ( isProxyRelatedError( error ) )
      mLastProxyHint = tr( "Proxy negotiation failed. Review system proxy settings and proxy credentials." );
  } );

  connect( probeReply, &QNetworkReply::finished, this, [this, probeReply]()
  {
    if ( !probeReply )
      return;

    if ( mProbeReply == probeReply )
      mProbeReply = nullptr;

    finalizeConnectivityProbe( probeReply.data() );
    probeReply->deleteLater();
  } );
}

void QgsQCopilotsDock::finalizeConnectivityProbe( QNetworkReply *reply )
{
  if ( !reply )
    return;

  mLastFinalUrl = reply->url().toString();
  const QVariant statusCodeValue = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute );
  if ( statusCodeValue.isValid() )
    mLastHttpStatusCode = statusCodeValue.toInt();
  mLastHttpReason = reply->attribute( QNetworkRequest::HttpReasonPhraseAttribute ).toString();

  if ( reply->error() != QNetworkReply::NoError )
  {
    if ( mLastNetworkError.isEmpty() )
      mLastNetworkError = reply->errorString();
    appendDiagnosticLog( tr( "Connectivity probe failed: %1" ).arg( mLastNetworkError ), Qgis::Warning );
  }
  else if ( isHttpSuccessStatus( mLastHttpStatusCode ) )
  {
    appendDiagnosticLog( tr( "Connectivity probe succeeded: %1 (%2)" ).arg( mLastFinalUrl, httpStatusText() ), Qgis::Info );
  }
  else if ( mLastHttpStatusCode > 0 )
  {
    mLastNetworkError = tr( "HTTP probe returned %1." ).arg( httpStatusText() );
    appendDiagnosticLog( mLastNetworkError, Qgis::Warning );
  }
}

void QgsQCopilotsDock::handleDownloadRequested( QWebEngineDownloadRequest *download )
{
  if ( !download )
    return;

  const QString savePath = buildSuggestedSavePath( download );
  const QString selectedPath = QFileDialog::getSaveFileName(
    this,
    tr( "Save Download As" ),
    savePath );

  if ( selectedPath.isEmpty() )
  {
    appendDiagnosticLog(
      tr( "Download canceled by user: %1" ).arg( download->url().toString() ),
      Qgis::Info );
    download->cancel();
    return;
  }

  const QFileInfo targetInfo( selectedPath );
  if ( !targetInfo.dir().exists() )
    QDir().mkpath( targetInfo.dir().absolutePath() );

  QgsSettings settings;
  settings.setValue(
    sDownloadLastDirKey,
    targetInfo.dir().absolutePath(),
    QgsSettings::Section::Plugins );

  download->setDownloadDirectory( targetInfo.dir().absolutePath() );
  download->setDownloadFileName( targetInfo.fileName() );
  trackDownloadState( download );
  download->accept();

  appendDiagnosticLog(
    tr( "Download accepted: %1 -> %2" ).arg( download->url().toString(), selectedPath ),
    Qgis::Info );
}

QString QgsQCopilotsDock::buildSuggestedSavePath( const QWebEngineDownloadRequest *download ) const
{
  QString fileName;
  if ( download )
  {
    fileName = download->downloadFileName().trimmed();
    if ( fileName.isEmpty() )
      fileName = download->suggestedFileName().trimmed();
    if ( fileName.isEmpty() )
      fileName = QFileInfo( download->url().path() ).fileName().trimmed();
  }

  fileName = QgsFileUtils::stringToSafeFilename( fileName );
  if ( fileName.isEmpty() )
    fileName = sFallbackDownloadFileName;

  QgsSettings settings;
  QString downloadDir = settings.value( sDownloadLastDirKey, QString(), QgsSettings::Section::Plugins ).toString().trimmed();
  if ( downloadDir.isEmpty() )
    downloadDir = QStandardPaths::writableLocation( QStandardPaths::DownloadLocation );
  if ( downloadDir.isEmpty() )
    downloadDir = QDir::homePath();

  return QDir( downloadDir ).filePath( fileName );
}

void QgsQCopilotsDock::trackDownloadState( QWebEngineDownloadRequest *download )
{
  if ( !download )
    return;

  const QPointer< QWebEngineDownloadRequest > downloadPtr( download );
  connect( download, &QWebEngineDownloadRequest::stateChanged, this, [this, downloadPtr]( QWebEngineDownloadRequest::DownloadState state )
  {
    if ( !downloadPtr )
      return;

    switch ( state )
    {
      case QWebEngineDownloadRequest::DownloadInProgress:
        appendDiagnosticLog(
          tr( "Download started: %1" ).arg( downloadPtr->url().toString() ),
          Qgis::Info );
        break;
      case QWebEngineDownloadRequest::DownloadCompleted:
        appendDiagnosticLog(
          tr( "Download completed: %1/%2" ).arg( downloadPtr->downloadDirectory(), downloadPtr->downloadFileName() ),
          Qgis::Info );
        break;
      case QWebEngineDownloadRequest::DownloadCancelled:
        appendDiagnosticLog(
          tr( "Download canceled: %1" ).arg( downloadPtr->url().toString() ),
          Qgis::Warning );
        break;
      case QWebEngineDownloadRequest::DownloadInterrupted:
        appendDiagnosticLog(
          tr( "Download interrupted: %1 (reason: %2)" )
            .arg( downloadPtr->url().toString(), downloadPtr->interruptReasonString() ),
          Qgis::Warning );
        break;
      case QWebEngineDownloadRequest::DownloadRequested:
        break;
    }
  } );
}

void QgsQCopilotsDock::loadUrl( const QUrl &url, bool persistOnSuccess )
{
  if ( !mWebView )
    return;

  resetProbeState();
  mPendingUrl = normalizeUrl( url );
  mPersistPendingUrlOnSuccess = persistOnSuccess;
  mLastFailureSummary.clear();

  appendDiagnosticLog( tr( "Loading URL: %1" ).arg( mPendingUrl.toString() ), Qgis::Info );
  startConnectivityProbe( mPendingUrl );

  mWebView->setUrl( mPendingUrl );
}

void QgsQCopilotsDock::handleLoadFinished( bool ok )
{
  if ( ok )
  {
    mServerUrl = normalizeUrl( mWebView->url() );
    if ( mServerUrl.isValid() )
    {
      if ( mPersistPendingUrlOnSuccess || !mLastKnownGoodUrl.isValid() )
        persistSuccessfulUrl( mServerUrl );

      appendDiagnosticLog( tr( "Web view load succeeded: %1" ).arg( mServerUrl.toString() ), Qgis::Info );
    }
    mRollbackInProgress = false;
    return;
  }

  const QUrl failedUrl = mPendingUrl;
  QStringList details;
  details << tr( "Configured URL: %1" ).arg( mPendingUrl.toString() );
  if ( !mLastFinalUrl.isEmpty() )
    details << tr( "Final URL: %1" ).arg( mLastFinalUrl );
  if ( mLastHttpStatusCode > 0 )
    details << tr( "HTTP: %1" ).arg( httpStatusText() );
  if ( !mLastNetworkError.isEmpty() )
    details << tr( "Network: %1" ).arg( mLastNetworkError );
  if ( !mLastSslError.isEmpty() )
    details << tr( "TLS: %1" ).arg( mLastSslError );
  if ( !mLastProxyHint.isEmpty() )
    details << tr( "Proxy: %1" ).arg( mLastProxyHint );
  details << tr( "Hints: %1" ).arg( diagnosticHintText() );

  mLastFailureSummary = details.join( QLatin1String( " | " ) );
  appendDiagnosticLog( tr( "Web view load failed: %1" ).arg( mLastFailureSummary ), Qgis::Warning );

  const bool rollbackAttempted = mRollbackInProgress;
  if ( rollbackAttempted )
  {
    mRollbackInProgress = false;
  }
  else
  {
    rollbackToLastKnownGoodUrl();
  }

  emit loadFailed( failedUrl );
}
