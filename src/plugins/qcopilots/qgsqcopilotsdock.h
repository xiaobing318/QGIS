/***************************************************************************
 *  qgsqcopilotsdock.h                                                    *
 *  --------------------                                                   *
 *  Dock widget that hosts a QWebEngineView for llama.cpp web UI.          *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#ifndef QGS_QCOPILOTS_DOCK_H
#define QGS_QCOPILOTS_DOCK_H

#include "ui_qgsqcopilotsdock.h"

#include <QDockWidget>
#include <QUrl>

class QWebEngineView;

class QgsQCopilotsDock : public QDockWidget
{
    Q_OBJECT

  public:
    explicit QgsQCopilotsDock( QWidget *parent = nullptr );

    QUrl serverUrl() const;
    void loadUrl( const QUrl &url );

  public slots:
    void reload();

  signals:
    void loadFailed( const QUrl &url );

  private:
    void handleLoadFinished( bool ok );

    Ui::qgsqcopilotsdock mUi;
    QWebEngineView *mWebView = nullptr;
    QUrl mServerUrl;
};

#endif // QGS_QCOPILOTS_DOCK_H
