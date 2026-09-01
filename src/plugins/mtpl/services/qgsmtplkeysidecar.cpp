/***************************************************************************
  qgsmtplkeysidecar.cpp
  ---------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmtplkeysidecar.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <Aclapi.h>
#endif

namespace
{
  QByteArray ownedByteArrayCopy( const QByteArray &source )
  {
    return source.isEmpty() ? QByteArray() : QByteArray( source.constData(), source.size() );
  }

  void secureClearByteArray( QByteArray &value ) noexcept
  {
    if ( !value.isEmpty() && value.isDetached() )
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

  bool applyOwnerOnlyPermissions( const QString &path, QString &error )
  {
#ifdef Q_OS_WIN
    std::wstring nativePath = QDir::toNativeSeparators( path ).toStdWString();
    PSID ownerSid = nullptr;
    PSECURITY_DESCRIPTOR securityDescriptor = nullptr;
    DWORD status = GetNamedSecurityInfoW( nativePath.data(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                                          &ownerSid, nullptr, nullptr, nullptr, &securityDescriptor );
    if ( status != ERROR_SUCCESS || !ownerSid )
    {
      if ( securityDescriptor )
        LocalFree( securityDescriptor );
      error = QStringLiteral( "无法确定密钥附属文件的所有者（Windows 错误 %1）。" ).arg( status );
      return false;
    }

    EXPLICIT_ACCESSW ownerAccess = {};
    ownerAccess.grfAccessPermissions = FILE_ALL_ACCESS;
    ownerAccess.grfAccessMode = SET_ACCESS;
    ownerAccess.grfInheritance = NO_INHERITANCE;
    BuildTrusteeWithSidW( &ownerAccess.Trustee, ownerSid );

    PACL ownerAcl = nullptr;
    status = SetEntriesInAclW( 1, &ownerAccess, nullptr, &ownerAcl );
    if ( status == ERROR_SUCCESS )
    {
      status = SetNamedSecurityInfoW( nativePath.data(), SE_FILE_OBJECT,
                                      DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                      nullptr, nullptr, ownerAcl, nullptr );
    }

    if ( ownerAcl )
      LocalFree( ownerAcl );
    LocalFree( securityDescriptor );
    if ( status != ERROR_SUCCESS )
    {
      error = QStringLiteral( "无法将密钥附属文件的访问权限限制为仅所有者可用（Windows 错误 %1）。" ).arg( status );
      return false;
    }
    return true;
#else
    if ( !QFile::setPermissions( path, QFileDevice::ReadOwner | QFileDevice::WriteOwner ) )
    {
      error = QStringLiteral( "无法将密钥附属文件的访问权限限制为仅所有者可用。" );
      return false;
    }
    return true;
#endif
  }

  QString formatName( QgsMtpl::PackageFormat format )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return QStringLiteral( "ptp" );
      case QgsMtpl::PackageFormat::Dtp:
        return QStringLiteral( "dtp" );
      case QgsMtpl::PackageFormat::Vtp:
        return QStringLiteral( "vtp" );
      case QgsMtpl::PackageFormat::Sfp:
        return QStringLiteral( "sfp" );
      case QgsMtpl::PackageFormat::Unknown:
        break;
    }
    return QString();
  }

  QgsMtpl::PackageFormat parseFormat( const QString &value )
  {
    const QString lowered = value.toLower();
    if ( lowered == QLatin1String( "ptp" ) )
      return QgsMtpl::PackageFormat::Ptp;
    if ( lowered == QLatin1String( "dtp" ) )
      return QgsMtpl::PackageFormat::Dtp;
    if ( lowered == QLatin1String( "vtp" ) )
      return QgsMtpl::PackageFormat::Vtp;
    if ( lowered == QLatin1String( "sfp" ) )
      return QgsMtpl::PackageFormat::Sfp;
    return QgsMtpl::PackageFormat::Unknown;
  }

  bool parseExactKeys( const QJsonObject &object, QgsMtpl::KeyMaterial &keys, QString &error )
  {
    const QJsonValue keyIdValue = object.value( QStringLiteral( "keyId" ) );
    const QJsonValue privateValue = object.value( QStringLiteral( "privateKey" ) );
    const QJsonValue deviceValue = object.value( QStringLiteral( "deviceKey" ) );
    if ( !keyIdValue.isString() || !privateValue.isObject() || !deviceValue.isObject() )
    {
      error = QStringLiteral( "密钥附属文件缺少密钥字段，或字段类型无效。" );
      return false;
    }

    const QJsonObject privateObject = privateValue.toObject();
    const QJsonObject deviceObject = deviceValue.toObject();
    if ( privateObject.value( QStringLiteral( "encoding" ) ).toString() != QLatin1String( "base64" ) ||
         deviceObject.value( QStringLiteral( "encoding" ) ).toString() != QLatin1String( "hex-text" ) ||
         !privateObject.value( QStringLiteral( "value" ) ).isString() ||
         !deviceObject.value( QStringLiteral( "value" ) ).isString() )
    {
      error = QStringLiteral( "不支持密钥附属文件中使用的密钥编码。" );
      return false;
    }

    keys.keyId = keyIdValue.toString();
    keys.privateKeyBase64 = privateObject.value( QStringLiteral( "value" ) ).toString().toLatin1();
    keys.deviceKeyHex = deviceObject.value( QStringLiteral( "value" ) ).toString().toLatin1();
    return keys.isValid( &error );
  }

  bool parseUnsigned( const QJsonValue &value, quint64 &result )
  {
    if ( !value.isString() )
      return false;

    static const QRegularExpression digits( QStringLiteral( "^[0-9]+$" ) );
    const QString text = value.toString();
    if ( !digits.match( text ).hasMatch() )
      return false;

    bool ok = false;
    result = text.toULongLong( &ok );
    return ok;
  }

  bool normalizeRelativePath( const QString &path, QString &normalized )
  {
    if ( path.isEmpty() || QDir::isAbsolutePath( path ) )
      return false;

    normalized = QDir::cleanPath( QString( path ).replace( '\\', '/' ) );
    if ( normalized == QLatin1String( "." ) || normalized == QLatin1String( ".." ) ||
         normalized.startsWith( QLatin1String( "../" ) ) || normalized.startsWith( '/' ) )
      return false;

    const QStringList parts = normalized.split( '/', Qt::KeepEmptyParts );
    for ( const QString &part : parts )
    {
      if ( part.isEmpty() || part == QLatin1String( "." ) || part == QLatin1String( ".." ) || part.contains( ':' ) )
        return false;
    }
    return true;
  }

  bool recordsMatchPackage( const QgsMtpl::KeySidecar &sidecar,
                            const QString &sidecarFile,
                            const QString &packagePath,
                            QString &error,
                            const std::function<bool()> &cancelCheck )
  {
    const QFileInfo packageInfo( packagePath );
    const QString absolutePackage = QDir::cleanPath( packageInfo.absoluteFilePath() );
    const QDir sidecarDirectory( QFileInfo( sidecarFile ).absolutePath() );
    QString relative = sidecarDirectory.relativeFilePath( absolutePackage );
    if ( !normalizeRelativePath( relative, relative ) )
      return false;

    const auto recordIt = std::find_if( sidecar.packages.cbegin(), sidecar.packages.cend(), [&relative]( const QgsMtpl::SidecarPackageRecord &record )
    {
      return record.relativePath.compare( relative, Qt::CaseInsensitive ) == 0;
    } );
    if ( recordIt == sidecar.packages.cend() )
      return false;

    if ( recordIt->format != parseFormat( packageInfo.suffix() ) )
    {
      error = QStringLiteral( "密钥附属文件记录的数据包格式与文件扩展名不匹配。" );
      return false;
    }

    if ( recordIt->size != static_cast<quint64>( packageInfo.size() ) )
    {
      error = QStringLiteral( "密钥附属文件记录的数据包大小与实际文件不匹配。" );
      return false;
    }

    QString hashError;
    const QByteArray packageHash = QgsMtpl::KeySidecarStore::sha256( absolutePackage, hashError, cancelCheck );
    if ( packageHash.isEmpty() )
    {
      error = hashError;
      return false;
    }
    if ( packageHash != recordIt->sha256 )
    {
      error = QStringLiteral( "密钥附属文件记录的校验和与实际文件不匹配。" );
      return false;
    }
    return true;
  }
}

QgsMtpl::KeyMaterial::KeyMaterial( const KeyMaterial &other )
  : keyId( other.keyId )
  , privateKeyBase64( ownedByteArrayCopy( other.privateKeyBase64 ) )
  , deviceKeyHex( ownedByteArrayCopy( other.deviceKeyHex ) )
{
}

QgsMtpl::KeyMaterial &QgsMtpl::KeyMaterial::operator=( const KeyMaterial &other )
{
  if ( this == &other )
    return *this;

  const QString keyIdCopy = other.keyId;
  QByteArray privateKeyCopy = ownedByteArrayCopy( other.privateKeyBase64 );
  QByteArray deviceKeyCopy = ownedByteArrayCopy( other.deviceKeyHex );
  clear();
  keyId = keyIdCopy;
  privateKeyBase64 = std::move( privateKeyCopy );
  deviceKeyHex = std::move( deviceKeyCopy );
  return *this;
}

QgsMtpl::KeyMaterial::KeyMaterial( KeyMaterial &&other ) noexcept
  : keyId( std::move( other.keyId ) )
  , privateKeyBase64( std::move( other.privateKeyBase64 ) )
  , deviceKeyHex( std::move( other.deviceKeyHex ) )
{
}

QgsMtpl::KeyMaterial &QgsMtpl::KeyMaterial::operator=( KeyMaterial &&other ) noexcept
{
  if ( this == &other )
    return *this;

  clear();
  keyId = std::move( other.keyId );
  privateKeyBase64 = std::move( other.privateKeyBase64 );
  deviceKeyHex = std::move( other.deviceKeyHex );
  return *this;
}

QgsMtpl::KeyMaterial::~KeyMaterial() noexcept
{
  clear();
}

void QgsMtpl::KeyMaterial::clear() noexcept
{
  keyId.clear();
  secureClearByteArray( privateKeyBase64 );
  secureClearByteArray( deviceKeyHex );
}

bool QgsMtpl::KeyMaterial::isValid( QString *error ) const
{
  const QUuid uuid( keyId );
  const QString canonicalUuid = uuid.toString( QUuid::WithoutBraces ).toLower();
  if ( uuid.isNull() || canonicalUuid != keyId.toLower() )
  {
    if ( error )
      *error = QStringLiteral( "密钥标识不是规范的 UUID。" );
    return false;
  }

  return cryptoKeys().isValid( error );
}

QgsMtpl::CryptoKeys QgsMtpl::KeyMaterial::cryptoKeys() const
{
  CryptoKeys result;
  result.privateKey = ownedByteArrayCopy( privateKeyBase64 );
  result.deviceKey = ownedByteArrayCopy( deviceKeyHex );
  return result;
}

QgsMtpl::KeyMaterial QgsMtpl::KeySidecarStore::generateKeys()
{
  auto randomBytes = []( int size )
  {
    QByteArray bytes( size, '\0' );
    for ( int offset = 0; offset < size; offset += static_cast<int>( sizeof( quint32 ) ) )
    {
      const quint32 value = QRandomGenerator::system()->generate();
      const int copySize = std::min( static_cast<int>( sizeof( value ) ), size - offset );
      memcpy( bytes.data() + offset, &value, static_cast<size_t>( copySize ) );
    }
    return bytes;
  };

  QByteArray privateKeyBytes = randomBytes( 32 );
  ScopedByteArrayClear privateKeyBytesClear( privateKeyBytes );
  QByteArray deviceKeyBytes = randomBytes( 16 );
  ScopedByteArrayClear deviceKeyBytesClear( deviceKeyBytes );

  KeyMaterial result;
  result.keyId = QUuid::createUuid().toString( QUuid::WithoutBraces ).toLower();
  result.privateKeyBase64 = privateKeyBytes.toBase64();
  result.deviceKeyHex = deviceKeyBytes.toHex();
  return result;
}

QString QgsMtpl::KeySidecarStore::singleSidecarPath( const QString &packagePath )
{
  return packagePath + QStringLiteral( ".mtpl-key.json" );
}

QString QgsMtpl::KeySidecarStore::batchSidecarName( const QString &keyId )
{
  return QStringLiteral( "mtpl-keyset-%1.json" ).arg( keyId.toLower() );
}

QByteArray QgsMtpl::KeySidecarStore::sha256( const QString &path,
                                             QString &error,
                                             const std::function<bool()> &cancelCheck )
{
  QFile file( path );
  if ( !file.open( QIODevice::ReadOnly ) )
  {
    error = QStringLiteral( "计算校验和时无法打开数据包：%1" ).arg( file.errorString() );
    return QByteArray();
  }

  QCryptographicHash hash( QCryptographicHash::Sha256 );
  while ( !file.atEnd() )
  {
    if ( cancelCheck && cancelCheck() )
    {
      error = QStringLiteral( "检查已取消。" );
      return QByteArray();
    }
    const QByteArray block = file.read( 1024 * 1024 );
    if ( block.isEmpty() && file.error() != QFileDevice::NoError )
    {
      error = QStringLiteral( "计算校验和时无法读取数据包：%1" ).arg( file.errorString() );
      return QByteArray();
    }
    hash.addData( block );
  }
  return hash.result().toHex();
}

bool QgsMtpl::KeySidecarStore::createForPackages( const QString &sidecarPath,
                                                  const QStringList &packagePaths,
                                                  const KeyMaterial &keys,
                                                  KeySidecar &sidecar,
                                                  QString &error,
                                                  const std::function<bool()> &cancelCheck )
{
  if ( packagePaths.isEmpty() || !keys.isValid( &error ) )
  {
    if ( packagePaths.isEmpty() )
      error = QStringLiteral( "创建密钥附属文件至少需要一个数据包。" );
    return false;
  }

  sidecar = KeySidecar();
  sidecar.keys = keys;
  const QDir baseDirectory( QFileInfo( sidecarPath ).absolutePath() );
  for ( const QString &packagePath : packagePaths )
  {
    if ( cancelCheck && cancelCheck() )
    {
      error = QStringLiteral( "数据包操作已取消。" );
      return false;
    }
    const QFileInfo info( packagePath );
    if ( !info.isFile() )
    {
      error = QStringLiteral( "密钥附属文件对应的数据包不存在，或不是常规文件。" );
      return false;
    }

    SidecarPackageRecord record;
    record.relativePath = baseDirectory.relativeFilePath( info.absoluteFilePath() ).replace( '\\', '/' );
    if ( !normalizeRelativePath( record.relativePath, record.relativePath ) )
    {
      error = QStringLiteral( "无法使用安全的相对路径在密钥附属文件中记录数据包。" );
      return false;
    }
    record.format = parseFormat( info.suffix() );
    if ( record.format == PackageFormat::Unknown )
    {
      error = QStringLiteral( "密钥附属文件对应的数据包使用了不支持的扩展名。" );
      return false;
    }
    record.size = static_cast<quint64>( info.size() );
    QFile file( info.absoluteFilePath() );
    if ( !file.open( QIODevice::ReadOnly ) )
    {
      error = QStringLiteral( "计算校验和时无法打开数据包：%1" ).arg( file.errorString() );
      return false;
    }
    QCryptographicHash hash( QCryptographicHash::Sha256 );
    while ( !file.atEnd() )
    {
      if ( cancelCheck && cancelCheck() )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        return false;
      }
      const QByteArray block = file.read( 1024 * 1024 );
      if ( block.isEmpty() && file.error() != QFileDevice::NoError )
      {
        error = QStringLiteral( "计算校验和时无法读取数据包：%1" ).arg( file.errorString() );
        return false;
      }
      hash.addData( block );
    }
    record.sha256 = hash.result().toHex();
    sidecar.packages.append( record );
  }

  std::sort( sidecar.packages.begin(), sidecar.packages.end(), []( const SidecarPackageRecord &left, const SidecarPackageRecord &right )
  {
    return left.relativePath < right.relativePath;
  } );
  return true;
}

bool QgsMtpl::KeySidecarStore::write( const QString &sidecarPath, const KeySidecar &sidecar, QString &error )
{
  if ( sidecar.schemaVersion != SchemaVersion || !sidecar.keys.isValid( &error ) || sidecar.packages.isEmpty() )
  {
    if ( sidecar.packages.isEmpty() )
      error = QStringLiteral( "密钥附属文件必须至少包含一个数据包记录。" );
    return false;
  }

  QJsonObject root;
  root.insert( QStringLiteral( "schemaVersion" ), SchemaVersion );
  root.insert( QStringLiteral( "keyId" ), sidecar.keys.keyId );
  root.insert( QStringLiteral( "privateKey" ), QJsonObject {
    { QStringLiteral( "encoding" ), QStringLiteral( "base64" ) },
    { QStringLiteral( "value" ), QString::fromLatin1( sidecar.keys.privateKeyBase64 ) }
  } );
  root.insert( QStringLiteral( "deviceKey" ), QJsonObject {
    { QStringLiteral( "encoding" ), QStringLiteral( "hex-text" ) },
    { QStringLiteral( "value" ), QString::fromLatin1( sidecar.keys.deviceKeyHex ) }
  } );

  QJsonArray packageArray;
  QSet<QString> uniquePaths;
  for ( const SidecarPackageRecord &record : sidecar.packages )
  {
    QString normalizedPath;
    const QString format = formatName( record.format );
    const QString foldedPath = record.relativePath.toCaseFolded();
    if ( !normalizeRelativePath( record.relativePath, normalizedPath ) || normalizedPath != record.relativePath ||
         format.isEmpty() || record.sha256.size() != 64 || uniquePaths.contains( foldedPath ) )
    {
      error = QStringLiteral( "密钥附属文件中的数据包记录无效。" );
      return false;
    }
    uniquePaths.insert( foldedPath );
    packageArray.append( QJsonObject {
      { QStringLiteral( "path" ), record.relativePath },
      { QStringLiteral( "format" ), format },
      { QStringLiteral( "size" ), QString::number( record.size ) },
      { QStringLiteral( "sha256" ), QString::fromLatin1( record.sha256 ) }
    } );
  }
  root.insert( QStringLiteral( "packages" ), packageArray );

  QSaveFile file( sidecarPath );
  if ( !file.open( QIODevice::WriteOnly ) )
  {
    error = QStringLiteral( "无法创建密钥附属文件：%1" ).arg( file.errorString() );
    return false;
  }
  QByteArray json = QJsonDocument( root ).toJson( QJsonDocument::Indented );
  ScopedByteArrayClear jsonClear( json );
  if ( file.write( json ) != json.size() )
  {
    error = QStringLiteral( "无法写入密钥附属文件：%1" ).arg( file.errorString() );
    file.cancelWriting();
    return false;
  }
  if ( !file.commit() )
  {
    error = QStringLiteral( "无法以原子方式保存密钥附属文件：%1" ).arg( file.errorString() );
    return false;
  }
  if ( !applyOwnerOnlyPermissions( sidecarPath, error ) )
  {
    QFile::remove( sidecarPath );
    return false;
  }
  return true;
}

bool QgsMtpl::KeySidecarStore::read( const QString &sidecarPath, KeySidecar &sidecar, QString &error )
{
  QFile file( sidecarPath );
  if ( !file.open( QIODevice::ReadOnly ) )
  {
    error = QStringLiteral( "无法打开密钥附属文件：%1" ).arg( file.errorString() );
    return false;
  }
  if ( file.size() > 1024 * 1024 )
  {
    error = QStringLiteral( "密钥附属文件超过 1 MiB 安全限制。" );
    return false;
  }

  QByteArray json = file.readAll();
  ScopedByteArrayClear jsonClear( json );
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson( json, &parseError );
  if ( parseError.error != QJsonParseError::NoError || !document.isObject() )
  {
    error = QStringLiteral( "密钥附属文件不是有效的 JSON。" );
    return false;
  }

  const QJsonObject root = document.object();
  if ( root.value( QStringLiteral( "schemaVersion" ) ).toInt( -1 ) != SchemaVersion ||
       !root.value( QStringLiteral( "packages" ) ).isArray() )
  {
    error = QStringLiteral( "密钥附属文件的架构版本或数据包列表无效。" );
    return false;
  }

  KeySidecar result;
  if ( !parseExactKeys( root, result.keys, error ) )
    return false;

  QSet<QString> uniquePaths;
  const QJsonArray packageArray = root.value( QStringLiteral( "packages" ) ).toArray();
  if ( packageArray.isEmpty() || packageArray.size() > 100000 )
  {
    error = QStringLiteral( "密钥附属文件中的数据包数量无效。" );
    return false;
  }
  for ( const QJsonValue &value : packageArray )
  {
    if ( !value.isObject() )
    {
      error = QStringLiteral( "密钥附属文件中的数据包记录不是对象。" );
      return false;
    }
    const QJsonObject object = value.toObject();
    if ( !object.value( QStringLiteral( "path" ) ).isString() ||
         !object.value( QStringLiteral( "format" ) ).isString() ||
         !object.value( QStringLiteral( "sha256" ) ).isString() )
    {
      error = QStringLiteral( "密钥附属文件中的数据包记录字段类型无效。" );
      return false;
    }

    SidecarPackageRecord record;
    record.relativePath = object.value( QStringLiteral( "path" ) ).toString();
    record.format = parseFormat( object.value( QStringLiteral( "format" ) ).toString() );
    record.sha256 = object.value( QStringLiteral( "sha256" ) ).toString().toLatin1();
    static const QRegularExpression hashPattern( QStringLiteral( "^[0-9a-f]{64}$" ) );
    QString normalizedPath;
    if ( !normalizeRelativePath( record.relativePath, normalizedPath ) || normalizedPath != record.relativePath ||
         record.format == PackageFormat::Unknown || !parseUnsigned( object.value( QStringLiteral( "size" ) ), record.size ) ||
         !hashPattern.match( QString::fromLatin1( record.sha256 ) ).hasMatch() ||
         uniquePaths.contains( record.relativePath.toCaseFolded() ) )
    {
      error = QStringLiteral( "密钥附属文件中的数据包记录未通过严格校验。" );
      return false;
    }
    uniquePaths.insert( record.relativePath.toCaseFolded() );
    result.packages.append( record );
  }

  sidecar = result;
  return true;
}

bool QgsMtpl::KeySidecarStore::readForPackage( const QString &sidecarPath,
                                                const QString &packagePath,
                                                KeySidecar &sidecar,
                                                QString &error,
                                                const std::function<bool()> &cancelCheck )
{
  KeySidecar result;
  if ( !read( sidecarPath, result, error ) ||
       !recordsMatchPackage( result, sidecarPath, packagePath, error, cancelCheck ) )
    return false;

  sidecar = result;
  return true;
}

bool QgsMtpl::KeySidecarStore::discover( const QString &packagePath,
                                          KeySidecar &sidecar,
                                          QString &sidecarPath,
                                          QString &error,
                                          const std::function<bool()> &cancelCheck )
{
  const QFileInfo packageInfo( packagePath );
  if ( !packageInfo.isFile() )
  {
    error = QStringLiteral( "数据包不存在，或不是常规文件。" );
    return false;
  }

  QStringList candidates;
  const QString exactPath = singleSidecarPath( packageInfo.absoluteFilePath() );
  if ( QFileInfo::exists( exactPath ) )
    candidates.append( exactPath );
  const QDir directory( packageInfo.absolutePath() );
  const QFileInfoList keysets = directory.entryInfoList( QStringList() << QStringLiteral( "mtpl-keyset-*.json" ),
                                                         QDir::Files | QDir::Readable,
                                                         QDir::Name );
  for ( const QFileInfo &keyset : keysets )
    candidates.append( keyset.absoluteFilePath() );

  bool found = false;
  KeySidecar foundSidecar;
  QString foundPath;
  QString exactError;
  for ( const QString &candidate : std::as_const( candidates ) )
  {
    if ( cancelCheck && cancelCheck() )
    {
      error = QStringLiteral( "检查已取消。" );
      return false;
    }
    KeySidecar parsed;
    QString candidateError;
    if ( !read( candidate, parsed, candidateError ) )
    {
      if ( candidate == exactPath )
        exactError = candidateError;
      continue;
    }
    if ( !recordsMatchPackage( parsed, candidate, packageInfo.absoluteFilePath(), candidateError, cancelCheck ) )
    {
      if ( candidate == exactPath && !candidateError.isEmpty() )
        exactError = candidateError;
      continue;
    }
    if ( found && parsed.keys.keyId != foundSidecar.keys.keyId )
    {
      error = QStringLiteral( "有多个密钥附属文件与此数据包匹配。" );
      return false;
    }
    found = true;
    foundSidecar = parsed;
    foundPath = candidate;
  }

  if ( !found )
  {
    error = exactError.isEmpty() ? QStringLiteral( "未找到匹配的密钥附属文件。" ) : exactError;
    return false;
  }
  sidecar = foundSidecar;
  sidecarPath = foundPath;
  return true;
}
