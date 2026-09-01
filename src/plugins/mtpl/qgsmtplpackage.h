/***************************************************************************
  qgsmtplpackage.h
  ----------------
  Shared value types used by the built-in MTPL plugin.
  copyright            : (C) 2026 QGIS Project
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMTPLPACKAGE_H
#define QGSMTPLPACKAGE_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace QgsMtpl
{

enum class PackageFormat
{
  Unknown,
  Ptp,
  Dtp,
  Vtp,
  Sfp
};

enum class PayloadType
{
  Unknown,
  RasterImage,
  VectorTile,
  Elevation,
  Files
};

enum class EncryptionState
{
  Unknown,
  Plain,
  Encrypted,
  Locked
};

//! Describes whether a package can safely be loaded with the credentials used for the probe.
enum class ReadinessState
{
  PlainReady,
  KeyVerified,
  KeyRequired,
  KeyRejectedOrCorrupt,
  UnverifiableEmpty,
  Unsupported,
  Unreadable
};

//! Identifies where the credentials used for a package probe originated.
enum class CredentialSource
{
  None,
  Explicit,
  Remembered,
  Sidecar
};

struct CryptoKeys
{
  CryptoKeys() = default;
  CryptoKeys( const CryptoKeys &other );
  CryptoKeys &operator=( const CryptoKeys &other );
  CryptoKeys( CryptoKeys &&other ) noexcept;
  CryptoKeys &operator=( CryptoKeys &&other ) noexcept;
  ~CryptoKeys() noexcept;

  QByteArray privateKey;
  QByteArray deviceKey;

  //! Returns TRUE when both values use the canonical encodings accepted by MTPL.
  bool isValid( QString *error = nullptr ) const;
  bool isEmpty() const;
  bool hasAnyValue() const;
  void clear() noexcept;
};

//! Safe, immutable entry information collected while probing an SFP package.
struct SfpEntryDescriptor
{
  QString path;
  quint64 logicalSize = 0;
  quint64 storedSize = 0;
  bool encrypted = false;
};

struct PackageDescriptor
{
  QString path;
  QString displayName;
  PackageFormat format = PackageFormat::Unknown;
  PayloadType payload = PayloadType::Unknown;
  EncryptionState encryption = EncryptionState::Unknown;
  ReadinessState readiness = ReadinessState::Unsupported;
  CredentialSource credentialSource = CredentialSource::None;
  QString readinessMessage;
  QVariantMap metadata;
  QList<SfpEntryDescriptor> sfpEntries;
  QString sidecarPath;
  QString keyId;
  QString crsAuthId = QStringLiteral( "EPSG:3857" );
  QString scheme = QStringLiteral( "xyz" );
  QString stylePath;
  quint64 fileSize = 0;
  qint64 fileLastModifiedMs = -1;
  int tileSize = 256;
  int minimumZoom = -1;
  int maximumZoom = -1;
  bool hasExtent = false;
  double extentXMinimum = 0.0;
  double extentYMinimum = 0.0;
  double extentXMaximum = 0.0;
  double extentYMaximum = 0.0;
  double scale = 1.0;
  double offset = 0.0;
  bool hasNoData = false;
  double noData = 0.0;
  bool displayOverridesApplied = false;

  bool isSpatial() const;
  bool isLocked() const;
  bool isReady() const;
  bool requiresKey() const;
};

struct ProbeResult
{
  bool ok = false;
  bool canceled = false;
  QString error;
  QList<PackageDescriptor> packages;
  int ignoredFileCount = 0;

  bool isDirectorySelection = false;

  bool hasReadyPackages() const;
  int readyPackageCount() const;
};

PackageFormat packageFormatFromPath( const QString &path );
QString packageFormatName( PackageFormat format );
QString payloadTypeName( PayloadType payload );
QString encryptionStateName( EncryptionState state );
QString readinessStateName( ReadinessState state );
QString credentialSourceName( CredentialSource source );
QString packageFileFilter();

} // namespace QgsMtpl

Q_DECLARE_METATYPE( QgsMtpl::PackageDescriptor )
Q_DECLARE_METATYPE( QgsMtpl::ProbeResult )
Q_DECLARE_METATYPE( QgsMtpl::ReadinessState )
Q_DECLARE_METATYPE( QgsMtpl::CredentialSource )

#endif // QGSMTPLPACKAGE_H
