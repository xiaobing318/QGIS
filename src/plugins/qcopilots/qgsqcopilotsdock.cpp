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

#include "qgssettings.h"

#include <QLayout>
#include <QWebEngineView>

QgsQCopilotsDock::QgsQCopilotsDock( QWidget *parent )
  : QDockWidget( parent )
{
  setWindowTitle( tr( "QCopilots" ) );
  setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea );
  setFeatures( QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable );

  auto *container = new QWidget( this );
  mUi.setupUi( container );
  setWidget( container );

  if ( QLayout *layout = container->layout() )
    layout->setContentsMargins( 0, 0, 0, 0 );

  mWebView = mUi.webEngineView;
  if ( mWebView )
    connect( mWebView, &QWebEngineView::loadFinished, this, &QgsQCopilotsDock::handleLoadFinished );

  const QgsSettings settings;
  loadUrl( QUrl( settings.value( QStringLiteral( "QCopilots/server_url" ), QStringLiteral( "http://127.0.0.1:8080" ), QgsSettings::Section::Plugins ).toString() ) );
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
  settings.setValue( QStringLiteral( "QCopilots/server_url" ), mServerUrl.toString(), QgsSettings::Section::Plugins );

  mWebView->setUrl( mServerUrl );
}

void QgsQCopilotsDock::reload()
{
  if ( mWebView )
    mWebView->reload();
}

void QgsQCopilotsDock::handleLoadFinished( bool ok )
{
  if ( !ok )
    emit loadFailed( mServerUrl );
}
