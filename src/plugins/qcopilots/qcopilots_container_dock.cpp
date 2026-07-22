/***************************************************************************
    qcopilots_container_dock.cpp
    ----------------------------
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

#include "qcopilots_container_dock.h"

#include "qgsapplication.h"
#include "qcopilots_container_utils.h"
#include "qgssettings.h"

#include <QAuthenticator>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLayout>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSslError>
#include <QStandardPaths>
#include <QStringList>
#include <QVariant>

#include <QWebEngineDownloadRequest>
#include <QWebEngineFileSystemAccessRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include "moc_qcopilots_container_dock.cpp"

namespace
{
  const QString sServerUrlKey = QStringLiteral( "QCopilots/serverUrl" );
  const QString sLastSuccessfulUrlKey = QStringLiteral( "QCopilots/lastSuccessfulUrl" );

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

  int defaultPortForScheme( const QString &scheme )
  {
    if ( scheme.compare( QLatin1String( "http" ), Qt::CaseInsensitive ) == 0 )
      return 80;

    if ( scheme.compare( QLatin1String( "https" ), Qt::CaseInsensitive ) == 0 )
      return 443;

    return -1;
  }

  int effectivePort( const QUrl &url )
  {
    return url.port( defaultPortForScheme( url.scheme() ) );
  }

  bool sameWebUiOrigin( const QUrl &left, const QUrl &right )
  {
    if ( !QgsQCopilotsUtils::isSupportedWebUiUrl( left ) || !QgsQCopilotsUtils::isSupportedWebUiUrl( right ) )
      return false;

    return left.scheme().compare( right.scheme(), Qt::CaseInsensitive ) == 0
           && left.host().compare( right.host(), Qt::CaseInsensitive ) == 0
           && effectivePort( left ) == effectivePort( right );
  }

  void configureQCopilotsWebSettings( QWebEngineSettings *settings )
  {
    if ( !settings )
      return;

    settings->setAttribute( QWebEngineSettings::AutoLoadImages, true );
    settings->setAttribute( QWebEngineSettings::JavascriptEnabled, true );
    settings->setAttribute( QWebEngineSettings::JavascriptCanOpenWindows, false );
    settings->setAttribute( QWebEngineSettings::JavascriptCanAccessClipboard, true );
    settings->setAttribute( QWebEngineSettings::JavascriptCanPaste, false );
    settings->setAttribute( QWebEngineSettings::LocalStorageEnabled, true );
    settings->setAttribute( QWebEngineSettings::FocusOnNavigationEnabled, true );
    settings->setAttribute( QWebEngineSettings::ScrollAnimatorEnabled, false );
    settings->setAttribute( QWebEngineSettings::FullScreenSupportEnabled, true );
    settings->setAttribute( QWebEngineSettings::ScreenCaptureEnabled, false );
    settings->setAttribute( QWebEngineSettings::WebGLEnabled, true );
    settings->setAttribute( QWebEngineSettings::Accelerated2dCanvasEnabled, true );
    settings->setAttribute( QWebEngineSettings::PlaybackRequiresUserGesture, true );
    settings->setAttribute( QWebEngineSettings::DnsPrefetchEnabled, false );
    settings->setAttribute( QWebEngineSettings::PdfViewerEnabled, true );
  }

  QT_WARNING_PUSH
  QT_WARNING_DISABLE_DEPRECATED
  int clipboardReadWriteFeatureValue()
  {
    bool ok = false;
    const QMetaEnum featureEnum = QMetaEnum::fromType< QWebEnginePage::Feature >();
    const int value = featureEnum.keyToValue( "ClipboardReadWrite", &ok );
    return ok ? value : -1;
  }
  QT_WARNING_POP

  QString defaultDownloadDirectory()
  {
    const QString downloads = QStandardPaths::writableLocation( QStandardPaths::DownloadLocation );
    return downloads.isEmpty() ? QDir::homePath() : downloads;
  }

  QWebEngineProfile *qcopilotsProfile()
  {
    static QPointer< QWebEngineProfile > sProfile;
    if ( !sProfile )
    {
      sProfile = new QWebEngineProfile( QgsQCopilotsUtils::webEngineProfileName(), QgsApplication::instance() );
      sProfile->setPersistentStoragePath( QgsQCopilotsUtils::webEngineStorageDirectory() );
      sProfile->setCachePath( QgsQCopilotsUtils::webEngineCacheDirectory() );
      sProfile->setHttpCacheType( QWebEngineProfile::DiskHttpCache );
      sProfile->setPersistentCookiesPolicy( QWebEngineProfile::AllowPersistentCookies );
      sProfile->setDownloadPath( defaultDownloadDirectory() );
      configureQCopilotsWebSettings( sProfile->settings() );
    }
    return sProfile;
  }

  QString qcopilotsWebFixupScriptSource()
  {
    return QStringLiteral( R"JS(
(function () {
  if (window.__qcopilotsDesktopFixupsInstalled) {
    return;
  }
  window.__qcopilotsDesktopFixupsInstalled = true;

  function editableTarget(element) {
    if (!element || element.disabled || element.readOnly) {
      return false;
    }
    var tagName = (element.tagName || '').toLowerCase();
    if (tagName === 'textarea') {
      return true;
    }
    if (tagName === 'input') {
      return ['text', 'search', 'url', 'email', 'password'].indexOf((element.type || 'text').toLowerCase()) !== -1;
    }
    return element.isContentEditable || element.getAttribute('role') === 'textbox';
  }

  function visibleEnabledButton(button) {
    if (!button || button.disabled || button.getAttribute('aria-disabled') === 'true') {
      return false;
    }
    var style = window.getComputedStyle(button);
    return style.display !== 'none' && style.visibility !== 'hidden';
  }

  function sendButtonFrom(root) {
    if (!root) {
      return null;
    }
    var selectors = [
      'button[type="submit"]',
      'button[aria-label*="send" i]',
      'button[title*="send" i]',
      '[role="button"][aria-label*="send" i]',
      '[data-testid*="send" i]',
      '[data-test*="send" i]'
    ];
    for (var selectorIndex = 0; selectorIndex < selectors.length; ++selectorIndex) {
      var candidates = root.querySelectorAll(selectors[selectorIndex]);
      for (var index = candidates.length - 1; index >= 0; --index) {
        if (visibleEnabledButton(candidates[index])) {
          return candidates[index];
        }
      }
    }
    return null;
  }

  function submitFrom(input) {
    var form = input.closest && input.closest('form');
    var composer = input.closest && input.closest('form, footer, [role="form"], [data-testid*="chat" i], [class*="chat" i]');
    var sendButton = sendButtonFrom(form) || sendButtonFrom(composer) || sendButtonFrom(document);

    if (sendButton) {
      sendButton.click();
      return true;
    }

    if (form) {
      if (typeof form.requestSubmit === 'function') {
        form.requestSubmit();
      } else {
        form.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
      }
      return true;
    }
    return false;
  }

  document.addEventListener('keydown', function (event) {
    if (event.defaultPrevented || event.key !== 'Enter' || event.shiftKey || event.ctrlKey || event.metaKey || event.altKey || event.isComposing) {
      return;
    }
    var path = event.composedPath ? event.composedPath() : [event.target];
    var target = path && path.length ? path[0] : event.target;
    if (!editableTarget(target)) {
      return;
    }
    if (submitFrom(target)) {
      event.preventDefault();
      event.stopImmediatePropagation();
    }
  }, true);
})();
)JS" );
  }

  void installQCopilotsWebFixupScript( QWebEnginePage *page )
  {
    if ( !page )
      return;

    QWebEngineScript script;
    script.setName( QStringLiteral( "qcopilots-desktop-web-fixups" ) );
    script.setInjectionPoint( QWebEngineScript::DocumentReady );
    script.setWorldId( QWebEngineScript::MainWorld );
    script.setRunsOnSubFrames( false );
    script.setSourceCode( qcopilotsWebFixupScriptSource() );
    page->scripts().insert( script );
  }
}

QgsQCopilotsDock::QgsQCopilotsDock( QWidget *parent )
  : QDockWidget( parent )
{
  setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea );
  setFeatures( QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable );

  auto *container = new QWidget( this );
  mUi.setupUi( container );
  setWindowTitle( container->windowTitle() );
  setWidget( container );

  if ( QLayout *layout = container->layout() )
    layout->setContentsMargins( 0, 0, 0, 0 );

  mNetworkAccessManager = new QNetworkAccessManager( this );
  connect( mNetworkAccessManager, &QNetworkAccessManager::proxyAuthenticationRequired, this, [this]( const QNetworkProxy &, QAuthenticator * ) {
    mLastProxyHint = tr( "Proxy authentication is required for this URL." );
    appendDiagnosticLog( mLastProxyHint, Qgis::Warning );
  } );

  mWebView = mUi.webEngineView;
  if ( mWebView )
  {
    QWebEnginePage *page = new QWebEnginePage( qcopilotsProfile(), mWebView );
    mWebView->setPage( page );
    configureQCopilotsWebSettings( mWebView->settings() );
    installQCopilotsWebFixupScript( mWebView->page() );
    connect( qcopilotsProfile(), &QWebEngineProfile::downloadRequested, this, &QgsQCopilotsDock::handleDownloadRequested );
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    connect( page, &QWebEnginePage::featurePermissionRequested, this, [this, page]( const QUrl &securityOrigin, QWebEnginePage::Feature feature ) {
      handleFeaturePermissionRequested( page, securityOrigin, static_cast< int >( feature ) );
    } );
    QT_WARNING_POP
    connect( page, &QWebEnginePage::fileSystemAccessRequested, this, &QgsQCopilotsDock::handleFileSystemAccessRequested );
    connect( mWebView, &QWebEngineView::loadStarted, this, &QgsQCopilotsDock::handleLoadStarted );
    connect( mWebView, &QWebEngineView::loadFinished, this, &QgsQCopilotsDock::handleLoadFinished );
    connect( mWebView, &QWebEngineView::urlChanged, this, [this]( const QUrl &url ) {
      mLastFinalUrl = url.toString();
    } );
  }

  const QgsSettings settings;
  mConfiguredUrl = QgsQCopilotsUtils::normalizedWebUiUrl( QUrl( settings.value( sServerUrlKey, QgsQCopilotsUtils::defaultServerUrl().toString(), QgsSettings::Section::Plugins ).toString() ) );
  mLastSuccessfulUrl = QgsQCopilotsUtils::normalizedWebUiUrl( QUrl( settings.value( sLastSuccessfulUrlKey, mConfiguredUrl.toString(), QgsSettings::Section::Plugins ).toString() ), mConfiguredUrl );

  appendDiagnosticLog( tr( "QCopilots file logging initialized." ), Qgis::Info );
  loadUrl( mConfiguredUrl, false );
}

QgsQCopilotsDock::~QgsQCopilotsDock()
{
  if ( mProbeReply )
  {
    mProbeReply->abort();
    mProbeReply->deleteLater();
    mProbeReply = nullptr;
  }
}

QUrl QgsQCopilotsDock::configuredUrl() const
{
  return mConfiguredUrl.isValid() ? mConfiguredUrl : QgsQCopilotsUtils::defaultServerUrl();
}

QUrl QgsQCopilotsDock::currentUrl() const
{
  if ( mWebView && mWebView->url().isValid() )
    return mWebView->url();
  return configuredUrl();
}

QString QgsQCopilotsDock::lastFailureSummary() const
{
  return mLastFailureSummary;
}

void QgsQCopilotsDock::setConfiguredUrl( const QUrl &url )
{
  const QUrl normalized = QgsQCopilotsUtils::normalizedWebUiUrl( url );
  mConfiguredUrl = normalized;

  QgsSettings settings;
  settings.setValue( sServerUrlKey, normalized.toString(), QgsSettings::Section::Plugins );
}

void QgsQCopilotsDock::loadUrl( const QUrl &url, bool persistAsConfigured )
{
  if ( !mWebView )
    return;

  const QUrl normalized = QgsQCopilotsUtils::normalizedWebUiUrl( url );
  if ( persistAsConfigured )
    setConfiguredUrl( normalized );

  resetProbeState();
  mPendingUrl = normalized;
  mLastFailureSummary.clear();

  appendDiagnosticLog( tr( "Loading QCopilots URL: %1" ).arg( QgsQCopilotsUtils::displayUrl( normalized ) ), Qgis::Info );
  startConnectivityProbe( normalized );
  mWebView->load( normalized );
}

void QgsQCopilotsDock::loadDefaultUrl()
{
  const QUrl defaultUrl = QgsQCopilotsUtils::defaultServerUrl();
  appendDiagnosticLog( tr( "Restoring default QCopilots URL: %1" ).arg( QgsQCopilotsUtils::displayUrl( defaultUrl ) ), Qgis::Info );
  loadUrl( defaultUrl, true );
}

void QgsQCopilotsDock::handleLoadStarted()
{
  mIsLoading = true;
}

void QgsQCopilotsDock::handleLoadFinished( bool ok )
{
  mIsLoading = false;

  if ( ok )
  {
    const QUrl successfulUrl = currentUrl();
    if ( QgsQCopilotsUtils::isSupportedWebUiUrl( successfulUrl ) )
    {
      mLastSuccessfulUrl = successfulUrl;
      QgsSettings settings;
      settings.setValue( sLastSuccessfulUrlKey, successfulUrl.toString(), QgsSettings::Section::Plugins );
    }

    appendDiagnosticLog( tr( "QCopilots web view loaded: %1" ).arg( QgsQCopilotsUtils::displayUrl( currentUrl() ) ), Qgis::Info );
    return;
  }

  const QUrl failedUrl = mPendingUrl.isValid() ? mPendingUrl : currentUrl();
  QStringList details;
  details << tr( "Configured URL: %1" ).arg( QgsQCopilotsUtils::displayUrl( configuredUrl() ) );
  details << tr( "Attempted URL: %1" ).arg( QgsQCopilotsUtils::displayUrl( failedUrl ) );
  if ( !mLastFinalUrl.isEmpty() )
    details << tr( "Final URL: %1" ).arg( QgsQCopilotsUtils::displayUrl( QUrl( mLastFinalUrl ) ) );
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
  appendDiagnosticLog( tr( "QCopilots web view load failed: %1" ).arg( mLastFailureSummary ), Qgis::Warning );
  emit loadFailed( failedUrl );
}

void QgsQCopilotsDock::handleDownloadRequested( QWebEngineDownloadRequest *download )
{
  if ( !download || !mWebView || !mWebView->page() )
    return;

  if ( download->page() != mWebView->page() )
    return;

  QString suggestedFileName = download->suggestedFileName();
  if ( suggestedFileName.isEmpty() )
    suggestedFileName = QStringLiteral( "qcopilots-export.json" );

  const QString initialPath = QDir( defaultDownloadDirectory() ).filePath( suggestedFileName );
  const QString filePath = QFileDialog::getSaveFileName( this, tr( "Save QCopilots export" ), initialPath );
  if ( filePath.isEmpty() )
  {
    appendDiagnosticLog( tr( "QCopilots download was canceled by the user." ), Qgis::Info );
    download->cancel();
    return;
  }

  const QFileInfo fileInfo( filePath );
  download->setDownloadDirectory( fileInfo.absolutePath() );
  download->setDownloadFileName( fileInfo.fileName() );

  const QPointer< QWebEngineDownloadRequest > downloadPointer( download );
  connect( download, &QWebEngineDownloadRequest::stateChanged, this, [this, downloadPointer]( QWebEngineDownloadRequest::DownloadState state ) {
    if ( !downloadPointer )
      return;

    if ( state == QWebEngineDownloadRequest::DownloadCompleted )
    {
      appendDiagnosticLog( tr( "QCopilots download completed: %1" ).arg( downloadPointer->downloadFileName() ), Qgis::Success );
    }
    else if ( state == QWebEngineDownloadRequest::DownloadInterrupted )
    {
      appendDiagnosticLog( tr( "QCopilots download interrupted: %1" ).arg( downloadPointer->interruptReasonString() ), Qgis::Warning );
    }
    else if ( state == QWebEngineDownloadRequest::DownloadCancelled )
    {
      appendDiagnosticLog( tr( "QCopilots download canceled." ), Qgis::Info );
    }

    if ( state == QWebEngineDownloadRequest::DownloadCompleted
         || state == QWebEngineDownloadRequest::DownloadInterrupted
         || state == QWebEngineDownloadRequest::DownloadCancelled )
    {
      downloadPointer->deleteLater();
    }
  } );

  appendDiagnosticLog( tr( "QCopilots download accepted: %1" ).arg( fileInfo.fileName() ), Qgis::Info );
  download->accept();
}

void QgsQCopilotsDock::handleFeaturePermissionRequested( QWebEnginePage *page, const QUrl &origin, int feature )
{
  if ( !page )
    return;

  QT_WARNING_PUSH
  QT_WARNING_DISABLE_DEPRECATED
  const QString displayOrigin = QgsQCopilotsUtils::displayUrl( origin );
  const int clipboardReadWriteFeature = clipboardReadWriteFeatureValue();
  const QWebEnginePage::Feature webEngineFeature = static_cast< QWebEnginePage::Feature >( feature );
  if ( isTrustedWebUiOrigin( origin ) && clipboardReadWriteFeature >= 0 && feature == clipboardReadWriteFeature )
  {
    page->setFeaturePermission( origin, webEngineFeature, QWebEnginePage::PermissionGrantedByUser );
    appendDiagnosticLog( tr( "QCopilots web permission granted for %1." ).arg( displayOrigin ), Qgis::Info );
  }
  else
  {
    page->setFeaturePermission( origin, webEngineFeature, QWebEnginePage::PermissionDeniedByUser );
    appendDiagnosticLog( tr( "QCopilots web permission denied for unsupported origin or permission type %1." ).arg( displayOrigin ), Qgis::Warning );
  }
  QT_WARNING_POP
}

void QgsQCopilotsDock::handleFileSystemAccessRequested( QWebEngineFileSystemAccessRequest request )
{
  const QString origin = QgsQCopilotsUtils::displayUrl( request.origin() );
  if ( isTrustedWebUiOrigin( request.origin() )
       && request.handleType() == QWebEngineFileSystemAccessRequest::File
       && request.accessFlags() == QWebEngineFileSystemAccessRequest::Write )
  {
    request.accept();
    appendDiagnosticLog( tr( "QCopilots file system access granted for %1." ).arg( origin ), Qgis::Info );
  }
  else
  {
    request.reject();
    appendDiagnosticLog( tr( "QCopilots file system access denied for unsupported origin or request type %1." ).arg( origin ), Qgis::Warning );
  }
}

bool QgsQCopilotsDock::isTrustedWebUiOrigin( const QUrl &origin ) const
{
  return sameWebUiOrigin( origin, configuredUrl() )
         || sameWebUiOrigin( origin, mPendingUrl );
}

void QgsQCopilotsDock::appendDiagnosticLog( const QString &message, Qgis::MessageLevel level )
{
  QgsQCopilotsUtils::logMessage( message, level );
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
    hints << tr( "TLS or certificate validation failed. Check certificate trust and hostname." );

  if ( !mLastProxyHint.isEmpty() )
    hints << mLastProxyHint;

  if ( mLastHttpStatusCode == 401 || mLastHttpStatusCode == 403 )
    hints << tr( "The target endpoint requires authentication or rejected the request." );

  if ( mLastHttpStatusCode >= 500 )
    hints << tr( "The server returned a 5xx response. Check llama-server runtime logs." );

  if ( mLastHttpStatusCode <= 0 && !mLastNetworkError.isEmpty() && mLastSslError.isEmpty() && mLastProxyHint.isEmpty() )
    hints << tr( "Check host, port, firewall rules, and whether llama-server is already running." );

  if ( hints.isEmpty() )
    hints << tr( "No obvious TLS, proxy, or server status issue was detected." );

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

void QgsQCopilotsDock::startConnectivityProbe( const QUrl &url )
{
  if ( !mNetworkAccessManager )
    return;

  if ( mProbeReply )
  {
    mProbeReply->abort();
    mProbeReply->deleteLater();
    mProbeReply = nullptr;
  }

  QNetworkRequest request( url );
  request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );

  mProbeReply = mNetworkAccessManager->head( request );
  const QPointer<QNetworkReply> replyPointer( mProbeReply );
  connect( mProbeReply, &QNetworkReply::sslErrors, this, [this, replyPointer]( const QList< QSslError > &errors ) {
    if ( !replyPointer || replyPointer != mProbeReply )
      return;
    if ( errors.isEmpty() )
      return;

    mLastSslError = errors.constFirst().errorString();
    appendDiagnosticLog( tr( "QCopilots connectivity probe SSL error: %1" ).arg( mLastSslError ), Qgis::Warning );
  } );

  connect( mProbeReply, &QNetworkReply::errorOccurred, this, [this, replyPointer]( QNetworkReply::NetworkError error ) {
    if ( !replyPointer || replyPointer != mProbeReply )
      return;

    mLastNetworkError = replyPointer->errorString();
    if ( isProxyRelatedError( error ) )
      mLastProxyHint = tr( "Proxy negotiation failed. Review system proxy settings and proxy credentials." );
  } );

  connect( mProbeReply, &QNetworkReply::finished, this, [this, replyPointer]() {
    if ( !replyPointer )
      return;

    QNetworkReply *finishedReply = replyPointer;
    if ( finishedReply != mProbeReply )
    {
      finishedReply->deleteLater();
      return;
    }
    mProbeReply = nullptr;
    finalizeConnectivityProbe( finishedReply );
    finishedReply->deleteLater();
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
    appendDiagnosticLog( tr( "QCopilots connectivity probe failed: %1" ).arg( mLastNetworkError ), Qgis::Warning );
  }
  else if ( isHttpSuccessStatus( mLastHttpStatusCode ) )
  {
    appendDiagnosticLog( tr( "QCopilots connectivity probe succeeded: %1 (%2)" ).arg( QgsQCopilotsUtils::displayUrl( QUrl( mLastFinalUrl ) ), httpStatusText() ), Qgis::Info );
  }
  else if ( mLastHttpStatusCode > 0 )
  {
    mLastNetworkError = tr( "HTTP probe returned %1." ).arg( httpStatusText() );
    appendDiagnosticLog( mLastNetworkError, Qgis::Warning );
  }
}
