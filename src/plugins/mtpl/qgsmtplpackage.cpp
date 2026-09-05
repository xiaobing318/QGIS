/***************************************************************************
  qgsmtplpackage.cpp
  ------------------
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

#include "qgsmtplpackage.h"

#include <QFileInfo>

#include <utility>

namespace
{
  QByteArray ownedByteArrayCopy( const QByteArray &source )
  {
    return source.isEmpty() ? QByteArray() : QByteArray( source.constData(), source.size() );
  }

  bool isLowerHex( const QByteArray &value )
  {
    for ( const char character : value )
    {
      if ( !( character >= '0' && character <= '9' ) &&
           !( character >= 'a' && character <= 'f' ) )
        return false;
    }
    return true;
  }

  void secureClearByteArray( QByteArray &value ) noexcept
  {
    if ( value.isEmpty() )
    {
      value.clear();
      value.squeeze();
      return;
    }

    // Every CryptoKeys copy made through its value semantics owns its backing
    // allocation. Never detach here because that would wipe only a new copy
    // and leave the previously shared allocation unchanged.
    if ( value.isDetached() )
    {
      volatile char *data = value.data();
      const qsizetype size = value.size();
      for ( qsizetype index = 0; index < size; ++index )
        data[index] = '\0';
    }
    value.clear();
    value.squeeze();
  }

  class ScopedByteArrayClear
  {
    public:
      explicit ScopedByteArrayClear( QByteArray &value )
        : mValue( value )
      {
      }

      ~ScopedByteArrayClear()
      {
        secureClearByteArray( mValue );
      }

      ScopedByteArrayClear( const ScopedByteArrayClear & ) = delete;
      ScopedByteArrayClear &operator=( const ScopedByteArrayClear & ) = delete;

    private:
      QByteArray &mValue;
  };
}

QgsMtpl::CryptoKeys::CryptoKeys( const CryptoKeys &other )
  : privateKey( ownedByteArrayCopy( other.privateKey ) )
  , deviceKey( ownedByteArrayCopy( other.deviceKey ) )
{
}

QgsMtpl::CryptoKeys &QgsMtpl::CryptoKeys::operator=( const CryptoKeys &other )
{
  if ( this == &other )
    return *this;

  QByteArray privateKeyCopy = ownedByteArrayCopy( other.privateKey );
  QByteArray deviceKeyCopy = ownedByteArrayCopy( other.deviceKey );
  clear();
  privateKey = std::move( privateKeyCopy );
  deviceKey = std::move( deviceKeyCopy );
  return *this;
}

QgsMtpl::CryptoKeys::CryptoKeys( CryptoKeys &&other ) noexcept
  : privateKey( std::move( other.privateKey ) )
  , deviceKey( std::move( other.deviceKey ) )
{
}

QgsMtpl::CryptoKeys &QgsMtpl::CryptoKeys::operator=( CryptoKeys &&other ) noexcept
{
  if ( this == &other )
    return *this;

  clear();
  privateKey = std::move( other.privateKey );
  deviceKey = std::move( other.deviceKey );
  return *this;
}

QgsMtpl::CryptoKeys::~CryptoKeys() noexcept
{
  clear();
}

bool QgsMtpl::CryptoKeys::isValid( QString *error ) const
{
  if ( privateKey.isEmpty() )
  {
    if ( error )
      *error = QStringLiteral( "缺少私钥。" );
    return false;
  }

  if ( deviceKey.isEmpty() )
  {
    if ( error )
      *error = QStringLiteral( "缺少设备密钥。" );
    return false;
  }

  QByteArray decodedPrivateKey = QByteArray::fromBase64( privateKey );
  ScopedByteArrayClear decodedPrivateKeyClear( decodedPrivateKey );
  QByteArray canonicalPrivateKey = decodedPrivateKey.toBase64();
  ScopedByteArrayClear canonicalPrivateKeyClear( canonicalPrivateKey );
  if ( decodedPrivateKey.size() < 16 || decodedPrivateKey.size() > 4096 || canonicalPrivateKey != privateKey )
  {
    if ( error )
      *error = QStringLiteral( "私钥必须是规范的 Base64，解码后的长度应为 16 至 4096 字节。" );
    return false;
  }

  if ( deviceKey.size() < 2 || deviceKey.size() > 512 || deviceKey.size() % 2 != 0 ||
       !isLowerHex( deviceKey ) )
  {
    if ( error )
      *error = QStringLiteral( "设备密钥必须由偶数个小写十六进制字符组成。" );
    return false;
  }

  if ( error )
    error->clear();
  return true;
}

bool QgsMtpl::CryptoKeys::isEmpty() const
{
  return privateKey.isEmpty() && deviceKey.isEmpty();
}

bool QgsMtpl::CryptoKeys::hasAnyValue() const
{
  return !isEmpty();
}

void QgsMtpl::CryptoKeys::clear() noexcept
{
  secureClearByteArray( privateKey );
  secureClearByteArray( deviceKey );
}

bool QgsMtpl::PackageDescriptor::isSpatial() const
{
  return format == PackageFormat::Ptp || format == PackageFormat::Dtp || format == PackageFormat::Vtp;
}

bool QgsMtpl::PackageDescriptor::isLocked() const
{
  return encryption == EncryptionState::Locked;
}

bool QgsMtpl::PackageDescriptor::isReady() const
{
  return readiness == ReadinessState::PlainReady || readiness == ReadinessState::KeyVerified;
}

bool QgsMtpl::PackageDescriptor::isCredentialedEmptyPtp() const
{
  bool presentCountOk = false;
  const quint64 presentTileCount = metadata.value( QStringLiteral( "presentTileCount" ) )
                                     .toULongLong( &presentCountOk );
  return format == PackageFormat::Ptp &&
         encryption == EncryptionState::Encrypted &&
         readiness == ReadinessState::UnverifiableEmpty &&
         credentialSource != CredentialSource::None &&
         metadata.contains( QStringLiteral( "presentTileCount" ) ) &&
         presentCountOk && presentTileCount == 0;
}

bool QgsMtpl::PackageDescriptor::requiresKey() const
{
  return readiness == ReadinessState::KeyRequired;
}

bool QgsMtpl::ProbeResult::hasReadyPackages() const
{
  return readyPackageCount() > 0;
}

int QgsMtpl::ProbeResult::readyPackageCount() const
{
  int count = 0;
  for ( const PackageDescriptor &package : packages )
  {
    if ( package.isReady() )
      ++count;
  }
  return count;
}

QgsMtpl::PackageFormat QgsMtpl::packageFormatFromPath( const QString &path )
{
  const QString suffix = QFileInfo( path ).suffix().toLower();
  if ( suffix == QLatin1String( "ptp" ) )
    return PackageFormat::Ptp;
  if ( suffix == QLatin1String( "dtp" ) )
    return PackageFormat::Dtp;
  if ( suffix == QLatin1String( "vtp" ) )
    return PackageFormat::Vtp;
  if ( suffix == QLatin1String( "sfp" ) )
    return PackageFormat::Sfp;
  return PackageFormat::Unknown;
}

QString QgsMtpl::packageFormatName( PackageFormat format )
{
  switch ( format )
  {
    case PackageFormat::Ptp:
      return QStringLiteral( "PTP" );
    case PackageFormat::Dtp:
      return QStringLiteral( "DTP" );
    case PackageFormat::Vtp:
      return QStringLiteral( "VTP" );
    case PackageFormat::Sfp:
      return QStringLiteral( "SFP" );
    case PackageFormat::Unknown:
      return QStringLiteral( "未知" );
  }
  return QStringLiteral( "未知" );
}

QString QgsMtpl::payloadTypeName( PayloadType payload )
{
  switch ( payload )
  {
    case PayloadType::RasterImage:
      return QStringLiteral( "栅格影像" );
    case PayloadType::VectorTile:
      return QStringLiteral( "矢量瓦片" );
    case PayloadType::Elevation:
      return QStringLiteral( "高程" );
    case PayloadType::Files:
      return QStringLiteral( "文件" );
    case PayloadType::Unknown:
      return QStringLiteral( "未知" );
  }
  return QStringLiteral( "未知" );
}

QString QgsMtpl::encryptionStateName( EncryptionState state )
{
  switch ( state )
  {
    case EncryptionState::Plain:
      return QStringLiteral( "未加密" );
    case EncryptionState::Encrypted:
      return QStringLiteral( "已加密" );
    case EncryptionState::Locked:
      return QStringLiteral( "已锁定" );
    case EncryptionState::Unknown:
      return QStringLiteral( "未知" );
  }
  return QStringLiteral( "未知" );
}

QString QgsMtpl::readinessStateName( ReadinessState state )
{
  switch ( state )
  {
    case ReadinessState::PlainReady:
      return QStringLiteral( "就绪" );
    case ReadinessState::KeyVerified:
      return QStringLiteral( "密钥已验证" );
    case ReadinessState::KeyRequired:
      return QStringLiteral( "需要密钥" );
    case ReadinessState::KeyRejectedOrCorrupt:
      return QStringLiteral( "密钥不匹配，或数据包已损坏" );
    case ReadinessState::UnverifiableEmpty:
      return QStringLiteral( "空数据包，无法验证" );
    case ReadinessState::Unsupported:
      return QStringLiteral( "不支持" );
    case ReadinessState::Unreadable:
      return QStringLiteral( "无法读取" );
  }
  return QStringLiteral( "无法读取" );
}

QString QgsMtpl::credentialSourceName( CredentialSource source )
{
  switch ( source )
  {
    case CredentialSource::None:
      return QStringLiteral( "无" );
    case CredentialSource::Explicit:
      return QStringLiteral( "手动输入" );
    case CredentialSource::Remembered:
      return QStringLiteral( "已记住" );
    case CredentialSource::Sidecar:
      return QStringLiteral( "密钥附属文件" );
  }
  return QStringLiteral( "无" );
}

QString QgsMtpl::packageFileFilter()
{
  return QStringLiteral( "MTPL 数据包 (*.ptp *.dtp *.vtp *.sfp);;所有文件 (*)" );
}
