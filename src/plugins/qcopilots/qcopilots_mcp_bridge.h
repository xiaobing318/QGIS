/***************************************************************************
    qcopilots_mcp_bridge.h
    ----------------------
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

#ifndef QCOPILOTS_MCP_BRIDGE_H
#define QCOPILOTS_MCP_BRIDGE_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QWebEnginePage;
class QgsQCopilotsMcpCatalog;

class QgsQCopilotsMcpBridge : public QObject
{
    Q_OBJECT

  public:
    explicit QgsQCopilotsMcpBridge( QgsQCopilotsMcpCatalog *catalog, QWebEnginePage *page, QObject *parent = nullptr );
    ~QgsQCopilotsMcpBridge() override;

    void setConfiguredUrl( const QUrl &url );

    Q_INVOKABLE QString catalog() const;
    Q_INVOKABLE void request( const QString &requestId, const QString &method, const QString &virtualUrl, const QVariantMap &headers, const QString &body, int timeoutMs );
    Q_INVOKABLE void cancel( const QString &requestId );

  signals:
    void catalogChanged( const QString &catalogJson );
    void response( const QString &requestId, int status, const QString &statusText, const QVariantMap &headers, const QString &bodyBase64 );
    void requestFailed( const QString &requestId, const QString &code, const QString &message );

  private:
    struct PendingRequest
    {
      QPointer<QNetworkReply> reply;
      QPointer<QTimer> timer;
      QByteArray responseBody;
    };

    void updatePublicCatalog();
    void finishRequest( const QString &requestId );
    void failRequest( const QString &requestId, const QString &code, const QString &message, bool abortReply = true );
    void cancelAllRequests( const QString &code, const QString &message );
    bool trustedTopLevelOrigin() const;

    QPointer<QgsQCopilotsMcpCatalog> mCatalog;
    QPointer<QWebEnginePage> mPage;
    QNetworkAccessManager *mNetworkAccessManager = nullptr;
    QUrl mConfiguredUrl;
    QString mPublicCatalogJson;
    QHash<QString, PendingRequest *> mPendingRequests;
    qint64 mLastPublicGeneration = 0;
};

#endif // QCOPILOTS_MCP_BRIDGE_H
