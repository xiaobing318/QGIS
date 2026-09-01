/***************************************************************************
  qgsmtplkeysidecar.h
  -------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
  Key generation and sidecar persistence for the built-in MTPL plugin.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMTPLKEYSIDECAR_H
#define QGSMTPLKEYSIDECAR_H

#include "../qgsmtplpackage.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <functional>

namespace QgsMtpl
{

  struct KeyMaterial
  {
    KeyMaterial() = default;
    KeyMaterial( const KeyMaterial &other );
    KeyMaterial &operator=( const KeyMaterial &other );
    KeyMaterial( KeyMaterial &&other ) noexcept;
    KeyMaterial &operator=( KeyMaterial &&other ) noexcept;
    ~KeyMaterial() noexcept;

    QString keyId;
    QByteArray privateKeyBase64;
    QByteArray deviceKeyHex;

    bool isValid( QString *error = nullptr ) const;
    CryptoKeys cryptoKeys() const;
    void clear() noexcept;
  };

  struct SidecarPackageRecord
  {
    QString relativePath;
    PackageFormat format = PackageFormat::Unknown;
    quint64 size = 0;
    QByteArray sha256;
  };

  struct KeySidecar
  {
    int schemaVersion = 1;
    KeyMaterial keys;
    QList<SidecarPackageRecord> packages;
  };

  class KeySidecarStore
  {
    public:
      static constexpr int SchemaVersion = 1;

      static KeyMaterial generateKeys();
      static QString singleSidecarPath( const QString &packagePath );
      static QString batchSidecarName( const QString &keyId );

      static bool createForPackages( const QString &sidecarPath,
                                     const QStringList &packagePaths,
                                     const KeyMaterial &keys,
                                     KeySidecar &sidecar,
                                     QString &error,
                                     const std::function<bool()> &cancelCheck = {} );

      static bool write( const QString &sidecarPath, const KeySidecar &sidecar, QString &error );
      static bool read( const QString &sidecarPath, KeySidecar &sidecar, QString &error );

      //! Reads a sidecar and verifies that it exactly fingerprints packagePath.
      static bool readForPackage( const QString &sidecarPath,
                                  const QString &packagePath,
                                  KeySidecar &sidecar,
                                  QString &error,
                                  const std::function<bool()> &cancelCheck = {} );

      //! Finds a valid sidecar which contains and exactly fingerprints packagePath.
      static bool discover( const QString &packagePath,
                            KeySidecar &sidecar,
                            QString &sidecarPath,
                            QString &error,
                            const std::function<bool()> &cancelCheck = {} );

      static QByteArray sha256( const QString &path,
                                QString &error,
                                const std::function<bool()> &cancelCheck = {} );
  };

} // namespace QgsMtpl

#endif // QGSMTPLKEYSIDECAR_H
