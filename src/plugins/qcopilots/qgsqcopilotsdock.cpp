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
#include "qgssettings.h"

#include <QDir>
#include <QLayout>
#include <QPointer>

#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

namespace
{
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

QgsQCopilotsDock::QgsQCopilotsDock( QWidget *parent )
  : QDockWidget( parent )
{
  setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea );
  setFeatures( QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable );

  auto *container = new QWidget( this );
  mUi.setupUi( container );
  setWindowTitle( container->windowTitle() );
  setWidget( container );

  if ( QLayout *layout = container->layout() ){
    layout->setContentsMargins( 0, 0, 0, 0 );
  }

  mWebView = mUi.webEngineView;
  if ( mWebView ){
    QWebEngineProfile *profile = qcopilotsProfile();
    mWebView->setPage( new QWebEnginePage( profile, mWebView ) );

    connect( mWebView, &QWebEngineView::loadFinished, this, &QgsQCopilotsDock::handleLoadFinished );
  }

  const QgsSettings settings;
  loadUrl( QUrl( settings.value( QStringLiteral( "QCopilots/QCopilotServerUrl" ), QStringLiteral( "http://127.0.0.1:8080" ), QgsSettings::Section::Plugins ).toString() ) );
}

QUrl QgsQCopilotsDock::serverUrl() const
{
  return mServerUrl;
}

void QgsQCopilotsDock::loadUrl( const QUrl &url )
{
  if ( !mWebView )
    return;

  mServerUrl = url;
  if ( !mServerUrl.isValid() || mServerUrl.scheme().isEmpty() )
    mServerUrl = QUrl( QStringLiteral( "http://127.0.0.1:8080" ) );

  QgsSettings settings;
  settings.setValue( QStringLiteral( "QCopilots/QCopilotServerUrl" ), mServerUrl.toString(), QgsSettings::Section::Plugins );

  mWebView->setUrl( mServerUrl );
}

void QgsQCopilotsDock::handleLoadFinished( bool ok )
{
  if ( !ok )
    emit loadFailed( mServerUrl );
}
