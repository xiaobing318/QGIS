/***************************************************************************
 *  qgsllamawebuidock.cpp                                                  *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#include "qgsllamawebuidock.h"
#include "moc_qgsllamawebuidock.cpp"

#include <QVBoxLayout>
#include <QWebEngineView>

QgsLlamaWebuiDockWidget::QgsLlamaWebuiDockWidget( QWidget *parent )
  : QgsDockWidget( parent )
{
  setWindowTitle( tr( "LLM Web UI" ) );
  setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );

  auto *container = new QWidget( this );
  auto *layout = new QVBoxLayout( container );
  layout->setContentsMargins( 0, 0, 0, 0 );

  mWebView = new QWebEngineView( container );
  mWebView->setUrl( QUrl( QStringLiteral( "http://127.0.0.1:8080" ) ) );
  layout->addWidget( mWebView );

  setWidget( container );
}

void QgsLlamaWebuiDockWidget::loadUrl( const QUrl &url )
{
  if ( mWebView )
  {
    mWebView->setUrl( url );
  }
}
