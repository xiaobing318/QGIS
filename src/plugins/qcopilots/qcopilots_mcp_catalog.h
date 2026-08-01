/***************************************************************************
    qcopilots_mcp_catalog.h
    -----------------------
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

#ifndef QCOPILOTS_MCP_CATALOG_H
#define QCOPILOTS_MCP_CATALOG_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class QCoreApplication;

struct QgsQCopilotsMcpService
{
  QString id;
  QString displayName;
  QString state;
  QUrl virtualUrl;
  QUrl targetUrl;
  QByteArray authToken;
};

struct QgsQCopilotsMcpCatalogSnapshot
{
  bool valid = false;
  qint64 generation = -1;
  bool startupComplete = false;
  QList<QgsQCopilotsMcpService> services;
};

class QgsQCopilotsMcpCatalog : public QObject
{
    Q_OBJECT

  public:
    explicit QgsQCopilotsMcpCatalog( QObject *parent = nullptr );
    ~QgsQCopilotsMcpCatalog() override;

    QgsQCopilotsMcpCatalogSnapshot snapshot() const;
    bool serviceForVirtualUrl( const QUrl &virtualUrl, QgsQCopilotsMcpService &service ) const;

  signals:
    void catalogChanged();

  protected:
    bool eventFilter( QObject *watched, QEvent *event ) override;

  private:
    void refresh();
    void clearCatalog( const QString &reason );

    QPointer<QCoreApplication> mApplication;
    QgsQCopilotsMcpCatalogSnapshot mSnapshot;
    QHash<QString, QgsQCopilotsMcpService> mServicesByVirtualUrl;
    QByteArray mSerializedCatalog;
    qint64 mHighestGeneration = -1;
};

#endif // QCOPILOTS_MCP_CATALOG_H
