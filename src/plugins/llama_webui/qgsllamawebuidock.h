/***************************************************************************
 *  qgsllamawebuidock.h                                                    *
 *  --------------------                                                   *
 *  Dock widget that hosts a QWebEngineView for llama.cpp web UI.          *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#ifndef QGS_LLAMA_WEBUI_DOCK_H
#define QGS_LLAMA_WEBUI_DOCK_H

#include "qgsdockwidget.h"

#include <QUrl>

class QWebEngineView;

class QgsLlamaWebuiDockWidget : public QgsDockWidget
{
    Q_OBJECT

  public:
    explicit QgsLlamaWebuiDockWidget( QWidget *parent = nullptr );

    void loadUrl( const QUrl &url );

  private:
    QWebEngineView *mWebView = nullptr;
};

#endif // QGS_LLAMA_WEBUI_DOCK_H
