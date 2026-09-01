/***************************************************************************
  qgsmtplcredentialstore.h
  ------------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
  Profile-scoped credential persistence for the built-in MTPL plugin.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMTPLCREDENTIALSTORE_H
#define QGSMTPLCREDENTIALSTORE_H

#include "../qgsmtplpackage.h"

/**
 * Stores the last manually entered MTPL credentials in the active QGIS
 * profile. Values are intentionally stored as readable text in QgsSettings
 * and must never be copied to a project or written to logs.
 */
class QgsMtplCredentialStore
{
  public:
    static bool rememberEnabled();
    static void setRememberEnabled( bool enabled );

    static QgsMtpl::CryptoKeys rememberedKeys( QString *error = nullptr );

    /**
     * Saves manually entered credentials without considering package
     * authentication. This allows both accepted and rejected attempts to be
     * reused. Malformed encodings are rejected before anything is stored.
     */
    static bool saveManualKeys( const QgsMtpl::CryptoKeys &keys,
                                const QString &lastPath,
                                QString &error );

    static QString lastPath();
    static void setLastPath( const QString &path );

    static void clearRememberedKeys();
    static void clear();
};

#endif // QGSMTPLCREDENTIALSTORE_H
