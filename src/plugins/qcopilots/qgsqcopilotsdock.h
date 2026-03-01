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

#include "qgis.h"

#include <QDockWidget>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QWebEngineView;

class QgsQCopilotsDock : public QDockWidget
{
    Q_OBJECT

  public:
    explicit QgsQCopilotsDock( QWidget *parent = nullptr );

    // Get the currently loaded server URL
    QUrl serverUrl() const;

    // Load the given server URL. The URL is persisted only after a successful load.
    void loadUrl( const QUrl &url, bool persistOnSuccess = true );

    // Return the latest load failure summary for message bar display
    QString lastFailureSummary() const;

  public slots:

  signals:
    void loadFailed( const QUrl &url );

  private:
    static bool isAllowedScheme( const QUrl &url );
    static QUrl defaultServerUrl();

    QUrl normalizeUrl( const QUrl &url ) const;
    void persistSuccessfulUrl( const QUrl &url );
    void rollbackToLastKnownGoodUrl();

    void appendDiagnosticLog( const QString &message, Qgis::MessageLevel level = Qgis::Info );

    void resetProbeState();
    void startConnectivityProbe( const QUrl &url );
    void finalizeConnectivityProbe( QNetworkReply *reply );
    QString httpStatusText() const;
    QString diagnosticHintText() const;
    void handleLoadFinished( bool ok );

    Ui::qgsqcopilotsdock mUi;
    QWebEngineView *mWebView = nullptr;
    QNetworkAccessManager *mNetworkAccessManager = nullptr;
    QNetworkReply *mProbeReply = nullptr;

    QUrl mServerUrl;
    QUrl mPendingUrl;
    QUrl mLastKnownGoodUrl;
    bool mPersistPendingUrlOnSuccess = true;
    bool mRollbackInProgress = false;

    int mLastHttpStatusCode = -1;
    QString mLastHttpReason;
    QString mLastFinalUrl;
    QString mLastNetworkError;
    QString mLastSslError;
    QString mLastProxyHint;
    QString mLastFailureSummary;
};

#endif // QGS_QCOPILOTS_DOCK_H
