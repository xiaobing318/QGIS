/***************************************************************************
    qcopilots_container_utils.h
    ---------------------------
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

#ifndef QCOPILOTS_CONTAINER_UTILS_H
#define QCOPILOTS_CONTAINER_UTILS_H

#include "qgis.h"

#include <QString>
#include <QUrl>

namespace QgsQCopilotsUtils
{
  QString logCategory();

  QUrl defaultServerUrl();
  bool isSupportedWebUiUrl( const QUrl &url );
  QUrl urlFromUserInput( const QString &input );
  QUrl normalizedWebUiUrl( const QUrl &url );
  QUrl normalizedWebUiUrl( const QUrl &url, const QUrl &fallback );
  QString displayUrl( const QUrl &url );

  QString pluginDataDirectory();
  QString logsDirectory();
  QString logFilePath();
  QString webEngineProfileName();
  QString webEngineStorageDirectory();
  QString webEngineCacheDirectory();

  void logMessage( const QString &message, Qgis::MessageLevel level = Qgis::Info );
}

#endif // QCOPILOTS_CONTAINER_UTILS_H
