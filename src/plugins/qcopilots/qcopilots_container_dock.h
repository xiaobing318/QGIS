/***************************************************************************
    qcopilots_container_dock.h
    --------------------------
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

#ifndef QCOPILOTS_CONTAINER_DOCK_H
#define QCOPILOTS_CONTAINER_DOCK_H

#include "ui_qcopilots_container_dock.h"

#include "qgis.h"

#include <QDockWidget>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QWebEngineDownloadRequest;
class QWebEngineFileSystemAccessRequest;
class QWebEnginePage;
class QWebEngineView;

class QgsQCopilotsDock : public QDockWidget
{
    Q_OBJECT

  public:
    explicit QgsQCopilotsDock( QWidget *parent = nullptr );
    ~QgsQCopilotsDock() override;

    QUrl configuredUrl() const;
    QUrl currentUrl() const;
    QString lastFailureSummary() const;

  public slots:
    void loadUrl( const QUrl &url, bool persistAsConfigured = false );
    void loadDefaultUrl();

  signals:
    void configureUrlRequested();
    void loadFailed( const QUrl &url );

  private:
    void setConfiguredUrl( const QUrl &url );
    void handleLoadStarted();
    void handleLoadFinished( bool ok );
    void handleDownloadRequested( QWebEngineDownloadRequest *download );
    void handleFeaturePermissionRequested( QWebEnginePage *page, const QUrl &origin, int feature );
    void handleFileSystemAccessRequested( QWebEngineFileSystemAccessRequest request );
    bool isTrustedWebUiOrigin( const QUrl &origin ) const;
    void appendDiagnosticLog( const QString &message, Qgis::MessageLevel level = Qgis::Info );

    void resetProbeState();
    void startConnectivityProbe( const QUrl &url );
    void finalizeConnectivityProbe( QNetworkReply *reply );
    QString httpStatusText() const;
    QString diagnosticHintText() const;

    Ui::QCopilotsContainerDock mUi;
    QWebEngineView *mWebView = nullptr;
    QNetworkAccessManager *mNetworkAccessManager = nullptr;
    QNetworkReply *mProbeReply = nullptr;

    QUrl mConfiguredUrl;
    QUrl mPendingUrl;
    QUrl mLastSuccessfulUrl;
    bool mIsLoading = false;

    int mLastHttpStatusCode = -1;
    QString mLastHttpReason;
    QString mLastFinalUrl;
    QString mLastNetworkError;
    QString mLastSslError;
    QString mLastProxyHint;
    QString mLastFailureSummary;
};

#endif // QCOPILOTS_CONTAINER_DOCK_H
