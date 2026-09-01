/***************************************************************************
  mtpltestfixtures.h
  ------------------
  Self-contained fixtures for the built-in MTPL plugin tests.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MTPLTESTFIXTURES_H
#define MTPLTESTFIXTURES_H

#include "qgsmtplpackage.h"

#include <mtpl/types.h>

#include <QByteArray>
#include <QList>
#include <QString>

namespace QgsMtplTest
{

struct SfpFixtureEntry
{
  QString path;
  QByteArray data;
  mtpl_storage_mode_t storageMode = MTPL_STORAGE_PLAIN;
};

QgsMtpl::CryptoKeys fixtureKeys();
QgsMtpl::CryptoKeys differentFixtureKeys();
QByteArray minimalVectorTilePayload();

bool writeTileFixture( const QString &path,
                       QgsMtpl::PackageFormat format,
                       mtpl_storage_mode_t storageMode,
                       const QgsMtpl::CryptoKeys &keys,
                       QString &error );

bool writeSfpFixture( const QString &path,
                      const QList<SfpFixtureEntry> &entries,
                      const QgsMtpl::CryptoKeys &keys,
                      QString &error );

bool createTileSourceTree( const QString &root, QString &error );
bool createSfpSourceDirectory( const QString &root, QString &error );

} // namespace QgsMtplTest

#endif // MTPLTESTFIXTURES_H
