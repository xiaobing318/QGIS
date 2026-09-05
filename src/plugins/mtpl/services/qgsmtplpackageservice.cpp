/***************************************************************************
  qgsmtplpackageservice.cpp
  -------------------------
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

#include "qgsmtplpackageservice.h"
#include "qgsmtplkeysidecar.h"
#include "qgsmtplstatus.h"

#include <mtpl/mtpl.h>

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
  const QString sPtpContractFailureKey = QStringLiteral( "_mtplPtpContractFailure" );
  const QString sExternalDisplayKeysKey = QStringLiteral( "_mtplExternalDisplayKeys" );

  QMutex sActiveProbeTaskMutex;
  QSet<QgsMtplProbeTask *> sActiveProbeTasks;

  bool canceled( const QgsMtplPackageService::CancelCheck &cancelCheck )
  {
    return cancelCheck && cancelCheck();
  }

  void reportProgress( const QgsMtplPackageService::ProgressCallback &progress, double value )
  {
    if ( progress )
      progress( std::clamp( value, 0.0, 100.0 ) );
  }

  bool isReservedWindowsName( const QString &component )
  {
    const QString stem = component.section( QLatin1Char( '.' ), 0, 0 ).toUpper();
    if ( stem == QLatin1String( "CON" ) || stem == QLatin1String( "PRN" ) ||
         stem == QLatin1String( "AUX" ) || stem == QLatin1String( "NUL" ) )
      return true;
    return stem.size() == 4 &&
           ( stem.startsWith( QLatin1String( "COM" ) ) || stem.startsWith( QLatin1String( "LPT" ) ) ) &&
           stem.at( 3 ) >= QLatin1Char( '1' ) && stem.at( 3 ) <= QLatin1Char( '9' );
  }

  bool normalizedSafeSfpPath( const QByteArray &rawPath, QString &normalized )
  {
    normalized.clear();
    const QString decoded = QString::fromUtf8( rawPath );
    if ( decoded.toUtf8() != rawPath || decoded.isEmpty() || decoded.contains( QLatin1Char( '\\' ) ) ||
         decoded.contains( QLatin1Char( ':' ) ) || QDir::isAbsolutePath( decoded ) || decoded.startsWith( QLatin1Char( '/' ) ) )
      return false;

    const QString cleaned = QDir::cleanPath( decoded );
    if ( cleaned != decoded || cleaned == QLatin1String( "." ) || cleaned.startsWith( QLatin1String( "../" ) ) )
      return false;
    const QStringList components = cleaned.split( QLatin1Char( '/' ), Qt::KeepEmptyParts );
    for ( const QString &component : components )
    {
      if ( component.isEmpty() || component == QLatin1String( "." ) || component == QLatin1String( ".." ) ||
           component.endsWith( QLatin1Char( '.' ) ) || component.endsWith( QLatin1Char( ' ' ) ) ||
           isReservedWindowsName( component ) )
        return false;
      for ( const QChar character : component )
      {
        if ( character.isNull() || character.unicode() < 0x20 )
          return false;
      }
    }
    normalized = cleaned;
    return true;
  }

  class CryptoContext
  {
    public:
      explicit CryptoContext( const QgsMtpl::CryptoKeys &keys )
        : mKeys( keys )
      {
        mOptions.private_key = reinterpret_cast<const uint8_t *>( mKeys.privateKey.constData() );
        mOptions.private_key_size = static_cast<size_t>( mKeys.privateKey.size() );
        mOptions.device_key = reinterpret_cast<const uint8_t *>( mKeys.deviceKey.constData() );
        mOptions.device_key_size = static_cast<size_t>( mKeys.deviceKey.size() );
      }

      const mtpl_crypto_options_t *get() const
      {
        return mKeys.privateKey.isEmpty() || mKeys.deviceKey.isEmpty() ? nullptr : &mOptions;
      }

    private:
      QgsMtpl::CryptoKeys mKeys;
      mtpl_crypto_options_t mOptions = {};
  };

  QString statusError( const QString &action, mtpl_status_t status )
  {
    return QgsMtpl::statusError( action, status );
  }

  bool isUnsupportedStatus( mtpl_status_t status )
  {
    return status == MTPL_STATUS_FORMAT_MISMATCH ||
           status == MTPL_STATUS_UNSUPPORTED_VERSION ||
           status == MTPL_STATUS_UNSUPPORTED;
  }

  QgsMtpl::PackageFormat packageFormat( mtpl_package_format_t format )
  {
    switch ( format )
    {
      case MTPL_PACKAGE_FORMAT_PTP:
        return QgsMtpl::PackageFormat::Ptp;
      case MTPL_PACKAGE_FORMAT_DTP:
        return QgsMtpl::PackageFormat::Dtp;
      case MTPL_PACKAGE_FORMAT_VTP:
        return QgsMtpl::PackageFormat::Vtp;
      case MTPL_PACKAGE_FORMAT_SFP:
        return QgsMtpl::PackageFormat::Sfp;
      case MTPL_PACKAGE_FORMAT_UNKNOWN:
        return QgsMtpl::PackageFormat::Unknown;
    }
    return QgsMtpl::PackageFormat::Unknown;
  }

  bool isKeyOrIntegrityStatus( mtpl_status_t status )
  {
    return status == MTPL_STATUS_KEY_REQUIRED ||
           status == MTPL_STATUS_CRYPTO_ERROR ||
           status == MTPL_STATUS_CORRUPT_DATA ||
           status == MTPL_STATUS_COMPRESSION_ERROR ||
           status == MTPL_STATUS_DECOMPRESSION_ERROR;
  }

  void setReadiness( QgsMtpl::PackageDescriptor &descriptor,
                     QgsMtpl::ReadinessState readiness,
                     const QString &message = QString() )
  {
    descriptor.readiness = readiness;
    descriptor.readinessMessage = message;
  }

  bool applyInspectionFailure( QgsMtpl::PackageDescriptor &descriptor,
                               const QString &action,
                               mtpl_status_t status,
                               bool encrypted,
                               bool credentialsAttempted,
                               QString &error )
  {
    const QString message = statusError( action, status );
    if ( encrypted && isKeyOrIntegrityStatus( status ) )
    {
      descriptor.encryption = QgsMtpl::EncryptionState::Locked;
      const QgsMtpl::ReadinessState readiness = status == MTPL_STATUS_KEY_REQUIRED && !credentialsAttempted
        ? QgsMtpl::ReadinessState::KeyRequired
        : QgsMtpl::ReadinessState::KeyRejectedOrCorrupt;
      setReadiness( descriptor, readiness, message );
      error.clear();
      return true;
    }
    if ( isUnsupportedStatus( status ) )
      setReadiness( descriptor, QgsMtpl::ReadinessState::Unsupported, message );
    else
      setReadiness( descriptor, QgsMtpl::ReadinessState::Unreadable, message );
    error = message;
    return false;
  }

  QVariant valueCaseInsensitive( const QVariantMap &map, const QStringList &names )
  {
    // The caller supplies aliases from most to least authoritative. Iterating
    // the names first makes that priority independent of QVariantMap ordering.
    for ( const QString &name : names )
    {
      for ( auto it = map.constBegin(); it != map.constEnd(); ++it )
      {
        if ( it.key().compare( name, Qt::CaseInsensitive ) == 0 )
          return it.value();
      }
    }
    return QVariant();
  }

  QgsMtpl::PayloadType payloadFromMetadata( const QVariantMap &metadata )
  {
    const QVariant typeValue = valueCaseInsensitive( metadata, QStringList()
      << QStringLiteral( "tile_file_ext" ) << QStringLiteral( "tileFormat" )
      << QStringLiteral( "payload" ) << QStringLiteral( "format" )
      << QStringLiteral( "type" ) );
    const QString type = typeValue.toString().trimmed().toLower();
    if ( type.contains( QLatin1String( "elevation" ) ) || type.contains( QLatin1String( "terrain" ) ) ||
         type.contains( QLatin1String( "height" ) ) || type == QLatin1String( "dem" ) ||
         type.startsWith( QLatin1String( "dem_" ) ) )
      return QgsMtpl::PayloadType::Elevation;
    if ( type.contains( QLatin1String( "pbf" ) ) || type.contains( QLatin1String( "mvt" ) ) ||
         type.contains( QLatin1String( "vector" ) ) )
      return QgsMtpl::PayloadType::VectorTile;
    if ( type.contains( QLatin1String( "png" ) ) || type.contains( QLatin1String( "jpg" ) ) ||
         type.contains( QLatin1String( "jpeg" ) ) || type.contains( QLatin1String( "webp" ) ) ||
         type.contains( QLatin1String( "image" ) ) || type.contains( QLatin1String( "raster" ) ) )
      return QgsMtpl::PayloadType::RasterImage;
    return QgsMtpl::PayloadType::Unknown;
  }

  QByteArray imageFormatFromMagic( const QByteArray &bytes )
  {
    if ( bytes.startsWith( QByteArray::fromHex( "89504e470d0a1a0a" ) ) )
      return QByteArrayLiteral( "png" );
    if ( bytes.startsWith( QByteArray::fromHex( "ffd8ff" ) ) )
      return QByteArrayLiteral( "jpeg" );
    if ( bytes.size() >= 12 && bytes.startsWith( "RIFF" ) &&
         bytes.mid( 8, 4 ) == QByteArrayLiteral( "WEBP" ) )
      return QByteArrayLiteral( "webp" );
    return QByteArray();
  }

  bool hasCompleteImageStructure( const QByteArray &bytes, const QByteArray &format )
  {
    // Some Qt image handlers recover truncated files without reporting an error.
    // Check container boundaries before accepting their decoded pixels.
    const auto *data = reinterpret_cast<const uchar *>( bytes.constData() );
    if ( format == QByteArrayLiteral( "png" ) )
    {
      qsizetype offset = 8;
      while ( bytes.size() - offset >= 12 )
      {
        const quint32 length = qFromBigEndian<quint32>( data + offset );
        if ( length > static_cast<quint64>( bytes.size() - offset - 12 ) )
          return false;
        if ( bytes.mid( offset + 4, 4 ) == QByteArrayLiteral( "IEND" ) )
          return length == 0;
        offset += static_cast<qsizetype>( length ) + 12;
      }
      return false;
    }
    if ( format == QByteArrayLiteral( "webp" ) )
    {
      return bytes.size() >= 12 && qFromLittleEndian<quint32>( data + 4 ) >= 4 &&
             static_cast<quint64>( qFromLittleEndian<quint32>( data + 4 ) ) + 8 <= static_cast<quint64>( bytes.size() );
    }
    if ( format != QByteArrayLiteral( "jpeg" ) )
      return false;

    qsizetype offset = 2;
    bool inScan = false;
    bool seenScan = false;
    while ( offset < bytes.size() )
    {
      if ( inScan )
      {
        while ( offset < bytes.size() && data[offset] != 0xff )
          ++offset;
      }
      if ( offset >= bytes.size() || data[offset++] != 0xff )
        return false;
      while ( offset < bytes.size() && data[offset] == 0xff )
        ++offset;
      if ( offset >= bytes.size() )
        return false;
      const uchar marker = data[offset++];
      if ( marker == 0xd9 )
        return seenScan;
      if ( marker == 0x00 || ( marker >= 0xd0 && marker <= 0xd7 ) )
      {
        if ( !inScan )
          return false;
        continue;
      }
      if ( marker == 0x01 )
        continue;
      if ( marker == 0xd8 || bytes.size() - offset < 2 )
        return false;
      const quint16 length = qFromBigEndian<quint16>( data + offset );
      if ( length < 2 || length > bytes.size() - offset )
        return false;
      offset += length;
      // DNL may appear inside a scan. Other segments end the current scan.
      inScan = marker == 0xda || ( inScan && marker == 0xdc );
      seenScan = seenScan || marker == 0xda;
    }
    return false;
  }

  bool validatePtpImage( const QByteArray &bytes, int expectedTileSize, QString &error )
  {
    const QByteArray format = imageFormatFromMagic( bytes );
    if ( format.isEmpty() )
    {
      error = QStringLiteral( "PTP 仅支持 PNG、JPEG 或 WebP 影像瓦片，代表瓦片的文件魔数不受支持。" );
      return false;
    }

    if ( !hasCompleteImageStructure( bytes, format ) )
    {
      error = QStringLiteral( "PTP %1 影像瓦片文件不完整或结构损坏，请重新导出原始图片。" )
                .arg( QString::fromLatin1( format ).toUpper() );
      return false;
    }

    QBuffer buffer;
    buffer.setData( bytes );
    if ( !buffer.open( QIODevice::ReadOnly ) )
    {
      error = QStringLiteral( "无法读取 PTP 代表影像瓦片。" );
      return false;
    }

    QImageReader imageReader( &buffer, format );
    if ( !imageReader.canRead() )
    {
      error = QStringLiteral( "PTP 代表瓦片虽然具有 %1 文件魔数，但无法解码：%2" )
                .arg( QString::fromLatin1( format ).toUpper(), imageReader.errorString() );
      return false;
    }

    const QSize declaredSize = imageReader.size();
    if ( declaredSize.isValid() &&
         ( declaredSize.width() != expectedTileSize || declaredSize.height() != expectedTileSize ) )
    {
      error = QStringLiteral( "PTP 代表影像瓦片尺寸为 %1×%2，与数据包头声明的 %3×%3 不一致。" )
                .arg( declaredSize.width() ).arg( declaredSize.height() ).arg( expectedTileSize );
      return false;
    }

    const QImage image = imageReader.read();
    if ( image.isNull() )
    {
      error = QStringLiteral( "PTP 代表 %1 影像瓦片无法完整解码：%2" )
                .arg( QString::fromLatin1( format ).toUpper(), imageReader.errorString() );
      return false;
    }
    if ( image.width() != expectedTileSize || image.height() != expectedTileSize )
    {
      error = QStringLiteral( "PTP 代表影像瓦片解码尺寸为 %1×%2，与数据包头声明的 %3×%3 不一致。" )
                .arg( image.width() ).arg( image.height() ).arg( expectedTileSize );
      return false;
    }

    error.clear();
    return true;
  }

  QString emptyDirectoryDiagnostic()
  {
    return QStringLiteral(
      "所选文件夹当前层级未找到可识别的 MTPL 数据包。文件夹检查不会递归，请选择直接包含 PTP、DTP、VTP 或 SFP 文件的叶子文件夹。" );
  }

  void applySpatialMetadata( QgsMtpl::PackageDescriptor &descriptor )
  {
    const QVariant matrixValue = descriptor.format == QgsMtpl::PackageFormat::Ptp
      ? valueCaseInsensitive( descriptor.metadata, QStringList()
          << QStringLiteral( "mtpl_tile_matrix" ) << QStringLiteral( "mtplTileMatrix" ) )
      : QVariant();
    const QVariantMap matrixMetadata = matrixValue.toMap();
    const QVariantMap &spatialMetadata = matrixMetadata.isEmpty() ? descriptor.metadata : matrixMetadata;

    const QVariant crs = valueCaseInsensitive( spatialMetadata, QStringList()
      << QStringLiteral( "crs" ) << QStringLiteral( "crsAuthId" ) << QStringLiteral( "srs" ) );
    if ( crs.isValid() && !crs.toString().isEmpty() )
      descriptor.crsAuthId = crs.toString();

    const QVariant scheme = valueCaseInsensitive( spatialMetadata, QStringList() << QStringLiteral( "scheme" ) );
    if ( scheme.isValid() && ( scheme.toString().compare( QLatin1String( "xyz" ), Qt::CaseInsensitive ) == 0 ||
                               scheme.toString().compare( QLatin1String( "tms" ), Qt::CaseInsensitive ) == 0 ) )
      descriptor.scheme = scheme.toString().toLower();

    if ( descriptor.format == QgsMtpl::PackageFormat::Dtp ||
         descriptor.payload == QgsMtpl::PayloadType::Elevation )
    {
      bool ok = false;
      const QVariant scale = valueCaseInsensitive( descriptor.metadata, QStringList() << QStringLiteral( "scale" ) );
      const double scaleValue = scale.toDouble( &ok );
      if ( ok )
        descriptor.scale = scaleValue;

      const QVariant offset = valueCaseInsensitive( descriptor.metadata, QStringList() << QStringLiteral( "offset" ) );
      const double offsetValue = offset.toDouble( &ok );
      if ( ok )
        descriptor.offset = offsetValue;

      const QVariant noData = valueCaseInsensitive( descriptor.metadata, QStringList()
        << QStringLiteral( "noData" ) << QStringLiteral( "nodata" ) << QStringLiteral( "no_data" ) );
      const double noDataValue = noData.toDouble( &ok );
      if ( ok )
      {
        descriptor.hasNoData = true;
        descriptor.noData = noDataValue;
      }
    }
  }

  void storeMetadata( const mtpl_buffer_t &buffer, QgsMtpl::PackageDescriptor &descriptor )
  {
    const size_t byteCount = std::min<size_t>( buffer.size, static_cast<size_t>( std::numeric_limits<int>::max() ) );
    const QByteArray bytes( reinterpret_cast<const char *>( buffer.data ), static_cast<int>( byteCount ) );
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson( bytes, &parseError );
    if ( parseError.error == QJsonParseError::NoError && document.isObject() )
    {
      descriptor.metadata = document.object().toVariantMap();
      // Package-authored metadata must never impersonate probe-owned
      // structural summaries or failure classifications. Keep the complete
      // private namespace reserved, including case variants.
      for ( auto it = descriptor.metadata.begin(); it != descriptor.metadata.end(); )
      {
        if ( it.key().startsWith( QLatin1String( "_mtpl" ), Qt::CaseInsensitive ) )
          it = descriptor.metadata.erase( it );
        else
          ++it;
      }
    }
    else if ( !bytes.isEmpty() )
      descriptor.metadata.insert( QStringLiteral( "rawMetadata" ), QString::fromUtf8( bytes ) );
  }

  bool containsMetadataKeyCaseInsensitive( const QVariantMap &metadata, const QString &name )
  {
    for ( auto it = metadata.constBegin(); it != metadata.constEnd(); ++it )
    {
      if ( it.key().compare( name, Qt::CaseInsensitive ) == 0 )
        return true;
    }
    return false;
  }

  void applyExternalTileJsonDisplayMetadata( const QFileInfo &selection,
                                             bool directorySelection,
                                             QList<QgsMtpl::PackageDescriptor> &packages )
  {
    QStringList candidates;
    if ( directorySelection )
    {
      candidates.append( QDir( selection.absoluteFilePath() ).filePath( QStringLiteral( "tilejson.json" ) ) );
    }
    else
    {
      const QDir directory = selection.absoluteDir();
      candidates.append( directory.filePath( selection.completeBaseName() + QStringLiteral( ".tilejson.json" ) ) );
      candidates.append( directory.filePath( selection.completeBaseName() + QStringLiteral( ".json" ) ) );
      candidates.append( directory.filePath( QStringLiteral( "tilejson.json" ) ) );
    }

    QString tileJsonPath;
    for ( const QString &candidate : std::as_const( candidates ) )
    {
      const QFileInfo candidateInfo( candidate );
      if ( candidateInfo.isFile() )
      {
        tileJsonPath = candidateInfo.absoluteFilePath();
        break;
      }
    }
    if ( tileJsonPath.isEmpty() )
      return;

    QFile file( tileJsonPath );
    constexpr qint64 maximumTileJsonBytes = 4 * 1024 * 1024;
    if ( !file.open( QIODevice::ReadOnly ) || file.size() < 0 || file.size() > maximumTileJsonBytes )
      return;
    const QByteArray bytes = file.read( maximumTileJsonBytes + 1 );
    if ( bytes.size() > maximumTileJsonBytes )
      return;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson( bytes, &parseError );
    if ( parseError.error != QJsonParseError::NoError || !document.isObject() )
      return;

    const QJsonObject object = document.object();
    QVariantMap supplemental;
    for ( const QString &name : { QStringLiteral( "name" ), QStringLiteral( "description" ), QStringLiteral( "attribution" ) } )
    {
      const QJsonValue value = object.value( name );
      if ( value.isString() && value.toString().size() <= 8192 )
        supplemental.insert( name, value.toString() );
    }
    const QJsonValue centerValue = object.value( QStringLiteral( "center" ) );
    if ( centerValue.isArray() )
    {
      const QJsonArray center = centerValue.toArray();
      bool validCenter = center.size() == 2 || center.size() == 3;
      for ( const QJsonValue &coordinate : center )
        validCenter = validCenter && coordinate.isDouble() && std::isfinite( coordinate.toDouble() );
      if ( validCenter )
        supplemental.insert( QStringLiteral( "center" ), center.toVariantList() );
    }
    if ( supplemental.isEmpty() )
      return;

    for ( QgsMtpl::PackageDescriptor &package : packages )
    {
      if ( package.format != QgsMtpl::PackageFormat::Ptp )
        continue;
      QStringList appliedKeys;
      for ( auto it = supplemental.constBegin(); it != supplemental.constEnd(); ++it )
      {
        if ( !containsMetadataKeyCaseInsensitive( package.metadata, it.key() ) )
        {
          package.metadata.insert( it.key(), it.value() );
          appliedKeys.append( it.key() );
        }
      }
      if ( !appliedKeys.isEmpty() )
        package.metadata.insert( sExternalDisplayKeysKey, appliedKeys );
    }
  }

  void clearExternalTileJsonDisplayMetadata( QgsMtpl::PackageDescriptor &package )
  {
    const QStringList appliedKeys = package.metadata.take( sExternalDisplayKeysKey ).toStringList();
    for ( const QString &appliedKey : appliedKeys )
      package.metadata.remove( appliedKey );
  }

  mtpl_status_t openTileReader( QgsMtpl::PackageFormat format, const QByteArray &path, const mtpl_crypto_options_t *crypto, void **reader )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_open( path.constData(), crypto, reinterpret_cast<mtpl_ptp_reader_t **>( reader ) );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_open( path.constData(), crypto, reinterpret_cast<mtpl_dtp_reader_t **>( reader ) );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_open( path.constData(), crypto, reinterpret_cast<mtpl_vtp_reader_t **>( reader ) );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t closeTileReader( QgsMtpl::PackageFormat format, void *reader )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_close( static_cast<mtpl_ptp_reader_t *>( reader ) );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_close( static_cast<mtpl_dtp_reader_t *>( reader ) );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_close( static_cast<mtpl_vtp_reader_t *>( reader ) );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileMetadata( QgsMtpl::PackageFormat format, void *reader, mtpl_buffer_t *metadata )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_read_metadata( static_cast<mtpl_ptp_reader_t *>( reader ), metadata );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_read_metadata( static_cast<mtpl_dtp_reader_t *>( reader ), metadata );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_read_metadata( static_cast<mtpl_vtp_reader_t *>( reader ), metadata );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileRangeCount( QgsMtpl::PackageFormat format, void *reader, size_t *count )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_get_range_count( static_cast<mtpl_ptp_reader_t *>( reader ), count );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_get_range_count( static_cast<mtpl_dtp_reader_t *>( reader ), count );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_get_range_count( static_cast<mtpl_vtp_reader_t *>( reader ), count );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileRangeInfo( QgsMtpl::PackageFormat format, void *reader, size_t index, mtpl_tile_range_info_t *info )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_get_range_info( static_cast<mtpl_ptp_reader_t *>( reader ), index, info );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_get_range_info( static_cast<mtpl_dtp_reader_t *>( reader ), index, info );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_get_range_info( static_cast<mtpl_vtp_reader_t *>( reader ), index, info );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileEntryInfo( QgsMtpl::PackageFormat format, void *reader, size_t range, size_t slot, mtpl_tile_entry_info_t *info )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_get_entry_info( static_cast<mtpl_ptp_reader_t *>( reader ), range, slot, info );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_get_entry_info( static_cast<mtpl_dtp_reader_t *>( reader ), range, slot, info );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_get_entry_info( static_cast<mtpl_vtp_reader_t *>( reader ), range, slot, info );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileRead( QgsMtpl::PackageFormat format, void *reader, const mtpl_tile_coordinate_t *coordinate, mtpl_buffer_t *buffer )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_read_tile( static_cast<mtpl_ptp_reader_t *>( reader ), coordinate, buffer );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_read_tile( static_cast<mtpl_dtp_reader_t *>( reader ), coordinate, buffer );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_read_tile( static_cast<mtpl_vtp_reader_t *>( reader ), coordinate, buffer );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t firstPresentTile( QgsMtpl::PackageFormat format,
                                  void *reader,
                                  mtpl_tile_coordinate_t &coordinate,
                                  bool &found,
                                  const QgsMtplPackageService::CancelCheck &cancelCheck )
  {
    found = false;
    size_t rangeCount = 0;
    mtpl_status_t status = tileRangeCount( format, reader, &rangeCount );
    if ( status != MTPL_STATUS_OK )
      return status;
    for ( size_t rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex )
    {
      if ( canceled( cancelCheck ) )
        return MTPL_STATUS_CANCELED;
      mtpl_tile_range_info_t rangeInfo = {};
      status = tileRangeInfo( format, reader, rangeIndex, &rangeInfo );
      if ( status != MTPL_STATUS_OK )
        return status;
      if ( rangeInfo.present_count == 0 )
        continue;
      for ( size_t slot = 0; slot < rangeInfo.slot_count; ++slot )
      {
        if ( canceled( cancelCheck ) )
          return MTPL_STATUS_CANCELED;
        mtpl_tile_entry_info_t entry = {};
        status = tileEntryInfo( format, reader, rangeIndex, slot, &entry );
        if ( status != MTPL_STATUS_OK )
          return status;
        if ( entry.present )
        {
          coordinate = entry.coordinate;
          found = true;
          return MTPL_STATUS_OK;
        }
      }
    }
    return MTPL_STATUS_OK;
  }

  mtpl_status_t storeTileRangeSummary( QgsMtpl::PackageFormat format,
                                       void *reader,
                                       QgsMtpl::PackageDescriptor &descriptor,
                                       const QgsMtplPackageService::CancelCheck &cancelCheck,
                                       const QgsMtplPackageService::ProgressCallback &progress )
  {
    size_t rangeCount = 0;
    mtpl_status_t status = tileRangeCount( format, reader, &rangeCount );
    if ( status != MTPL_STATUS_OK )
      return status;

    constexpr double halfWorld = 20037508.342789244;
    double xMinimum = std::numeric_limits<double>::max();
    double yMinimum = std::numeric_limits<double>::max();
    double xMaximum = std::numeric_limits<double>::lowest();
    double yMaximum = std::numeric_limits<double>::lowest();
    quint64 presentTileCount = 0;
    QVariantList rangeSummaries;
    int minimumZoom = std::numeric_limits<int>::max();
    int maximumZoom = std::numeric_limits<int>::min();
    bool hasExtent = false;
    const bool tms = descriptor.scheme.compare( QLatin1String( "tms" ), Qt::CaseInsensitive ) == 0;

    for ( size_t index = 0; index < rangeCount; ++index )
    {
      if ( canceled( cancelCheck ) )
        return MTPL_STATUS_CANCELED;
      if ( rangeCount > 0 )
        reportProgress( progress, static_cast<double>( index ) * 100.0 / static_cast<double>( rangeCount ) );
      mtpl_tile_range_info_t info = {};
      status = tileRangeInfo( format, reader, index, &info );
      if ( status != MTPL_STATUS_OK )
        return status;

      presentTileCount += static_cast<quint64>( info.present_count );
      // Preserve the established DTP/VTP empty-range behavior. Only PTP
      // aggregation needs the complete slot/range structure for strict
      // cross-package validation.
      if ( format != QgsMtpl::PackageFormat::Ptp && info.present_count == 0 )
        continue;
      if ( info.present_count > info.slot_count )
        return MTPL_STATUS_CORRUPT_DATA;

      mtpl_tile_range_t presentBounds = info.bounds;
      if ( info.present_count > 0 && info.present_count < info.slot_count )
      {
        bool foundPresentEntry = false;
        presentBounds.x_min = std::numeric_limits<uint32_t>::max();
        presentBounds.x_max = 0;
        presentBounds.y_min = std::numeric_limits<uint32_t>::max();
        presentBounds.y_max = 0;
        for ( size_t slot = 0; slot < info.slot_count; ++slot )
        {
          if ( canceled( cancelCheck ) )
            return MTPL_STATUS_CANCELED;
          mtpl_tile_entry_info_t entry = {};
          status = tileEntryInfo( format, reader, index, slot, &entry );
          if ( status != MTPL_STATUS_OK )
            return status;
          if ( !entry.present )
            continue;

          foundPresentEntry = true;
          presentBounds.x_min = std::min( presentBounds.x_min, entry.coordinate.x );
          presentBounds.x_max = std::max( presentBounds.x_max, entry.coordinate.x );
          presentBounds.y_min = std::min( presentBounds.y_min, entry.coordinate.y );
          presentBounds.y_max = std::max( presentBounds.y_max, entry.coordinate.y );
        }
        if ( !foundPresentEntry )
          return MTPL_STATUS_CORRUPT_DATA;
      }

      if ( format == QgsMtpl::PackageFormat::Ptp )
      {
        QVariantMap rangeSummary;
        rangeSummary.insert( QStringLiteral( "zoom" ), static_cast<uint>( info.bounds.zoom ) );
        rangeSummary.insert( QStringLiteral( "xMin" ), static_cast<qulonglong>( info.bounds.x_min ) );
        rangeSummary.insert( QStringLiteral( "xMax" ), static_cast<qulonglong>( info.bounds.x_max ) );
        rangeSummary.insert( QStringLiteral( "yMin" ), static_cast<qulonglong>( info.bounds.y_min ) );
        rangeSummary.insert( QStringLiteral( "yMax" ), static_cast<qulonglong>( info.bounds.y_max ) );
        rangeSummary.insert( QStringLiteral( "presentXMin" ), static_cast<qulonglong>( presentBounds.x_min ) );
        rangeSummary.insert( QStringLiteral( "presentXMax" ), static_cast<qulonglong>( presentBounds.x_max ) );
        rangeSummary.insert( QStringLiteral( "presentYMin" ), static_cast<qulonglong>( presentBounds.y_min ) );
        rangeSummary.insert( QStringLiteral( "presentYMax" ), static_cast<qulonglong>( presentBounds.y_max ) );
        rangeSummary.insert( QStringLiteral( "slotCount" ), static_cast<qulonglong>( info.slot_count ) );
        rangeSummary.insert( QStringLiteral( "presentCount" ), static_cast<qulonglong>( info.present_count ) );
        rangeSummaries.append( rangeSummary );
      }

      // Empty ranges still carry structural information. Keep their declared
      // zoom and bounds so dataset validation can reject a package whose
      // header does not agree with its five-part file name or tile matrix.
      if ( info.present_count == 0 )
        continue;

      minimumZoom = std::min( minimumZoom, static_cast<int>( info.bounds.zoom ) );
      maximumZoom = std::max( maximumZoom, static_cast<int>( info.bounds.zoom ) );
      if ( descriptor.crsAuthId.compare( QLatin1String( "EPSG:3857" ), Qt::CaseInsensitive ) != 0 || info.bounds.zoom > 30 )
        continue;

      const double matrixSize = std::ldexp( 1.0, static_cast<int>( info.bounds.zoom ) );
      const double tileSpan = ( 2.0 * halfWorld ) / matrixSize;
      const double rangeXMinimum = -halfWorld + static_cast<double>( presentBounds.x_min ) * tileSpan;
      const double rangeXMaximum = -halfWorld + static_cast<double>( static_cast<uint64_t>( presentBounds.x_max ) + 1ULL ) * tileSpan;
      double rangeYMinimum = 0.0;
      double rangeYMaximum = 0.0;
      if ( tms )
      {
        rangeYMinimum = -halfWorld + static_cast<double>( presentBounds.y_min ) * tileSpan;
        rangeYMaximum = -halfWorld + static_cast<double>( static_cast<uint64_t>( presentBounds.y_max ) + 1ULL ) * tileSpan;
      }
      else
      {
        rangeYMaximum = halfWorld - static_cast<double>( presentBounds.y_min ) * tileSpan;
        rangeYMinimum = halfWorld - static_cast<double>( static_cast<uint64_t>( presentBounds.y_max ) + 1ULL ) * tileSpan;
      }
      xMinimum = std::min( xMinimum, rangeXMinimum );
      yMinimum = std::min( yMinimum, rangeYMinimum );
      xMaximum = std::max( xMaximum, rangeXMaximum );
      yMaximum = std::max( yMaximum, rangeYMaximum );
      hasExtent = true;
    }

    descriptor.minimumZoom = presentTileCount == 0 ? -1 : minimumZoom;
    descriptor.maximumZoom = presentTileCount == 0 ? -1 : maximumZoom;
    descriptor.metadata.insert( QStringLiteral( "rangeCount" ), static_cast<qulonglong>( rangeCount ) );
    descriptor.metadata.insert( QStringLiteral( "presentTileCount" ), presentTileCount );
    if ( format == QgsMtpl::PackageFormat::Ptp )
    {
      descriptor.metadata.insert( QStringLiteral( "_mtplRangeBounds" ), rangeSummaries );
    }
    else if ( descriptor.minimumZoom >= 0 )
    {
      descriptor.metadata.insert( QStringLiteral( "minimumZoom" ), descriptor.minimumZoom );
      descriptor.metadata.insert( QStringLiteral( "maximumZoom" ), descriptor.maximumZoom );
    }
    if ( hasExtent )
    {
      descriptor.hasExtent = true;
      descriptor.extentXMinimum = xMinimum;
      descriptor.extentYMinimum = yMinimum;
      descriptor.extentXMaximum = xMaximum;
      descriptor.extentYMaximum = yMaximum;
    }
    reportProgress( progress, 100.0 );
    return MTPL_STATUS_OK;
  }
}

QgsMtplPackageService::PtpContractFailure QgsMtplPackageService::ptpContractFailure(
  const QgsMtpl::PackageDescriptor &descriptor )
{
  const QString value = descriptor.metadata.value( sPtpContractFailureKey ).toString();
  if ( value == QLatin1String( "image-decode" ) )
    return PtpContractFailure::ImageDecode;
  if ( value == QLatin1String( "declared-elevation" ) )
    return PtpContractFailure::DeclaredElevation;
  if ( value == QLatin1String( "declared-vector" ) )
    return PtpContractFailure::DeclaredVector;
  return PtpContractFailure::None;
}

QByteArray QgsMtplPackageService::ptpImageFormatFromMagic( const QByteArray &bytes )
{
  return imageFormatFromMagic( bytes );
}

bool QgsMtplPackageService::validatePtpImagePayload( const QByteArray &bytes, int expectedTileSize, QString &error )
{
  return validatePtpImage( bytes, expectedTileSize, error );
}

bool QgsMtplPackageService::ptpImageHasCompleteStructure( const QByteArray &bytes )
{
  return hasCompleteImageStructure( bytes, imageFormatFromMagic( bytes ) );
}

QStringList QgsMtplPackageService::supportedSuffixes()
{
  return QStringList() << QStringLiteral( "ptp" ) << QStringLiteral( "dtp" ) << QStringLiteral( "vtp" ) << QStringLiteral( "sfp" );
}

QStringList QgsMtplPackageService::supportedNameFilters()
{
  return QStringList() << QStringLiteral( "*.ptp" ) << QStringLiteral( "*.dtp" ) << QStringLiteral( "*.vtp" ) << QStringLiteral( "*.sfp" );
}

QgsMtpl::ProbeResult QgsMtplPackageService::probePath( const QString &path,
                                                       const QgsMtpl::CryptoKeys &keys,
                                                       bool readMetadata,
                                                       QgsMtpl::CredentialSource suppliedSource,
                                                       const CancelCheck &cancelCheck,
                                                       const ProgressCallback &progress,
                                                       bool preferPtpSidecar )
{
  QgsMtpl::ProbeResult result;
  reportProgress( progress, 0.0 );
  if ( canceled( cancelCheck ) )
  {
    result.canceled = true;
    result.error = QStringLiteral( "检查已取消。" );
    return result;
  }
  const QFileInfo inputInfo( path );
  if ( !inputInfo.exists() )
  {
    result.error = QStringLiteral( "所选路径不存在。" );
    return result;
  }

  QStringList packagePaths;
  if ( inputInfo.isFile() )
  {
    packagePaths.append( inputInfo.absoluteFilePath() );
  }
  else if ( inputInfo.isDir() )
  {
    result.isDirectorySelection = true;
    QDirIterator iterator( inputInfo.absoluteFilePath(), QDir::Files | QDir::NoSymLinks );
    while ( iterator.hasNext() )
    {
      if ( canceled( cancelCheck ) )
      {
        result.canceled = true;
        result.error = QStringLiteral( "检查已取消。" );
        return result;
      }
      packagePaths.append( QFileInfo( iterator.next() ).absoluteFilePath() );
    }
    std::sort( packagePaths.begin(), packagePaths.end(), []( const QString &left, const QString &right )
    {
      const int folded = left.toCaseFolded().compare( right.toCaseFolded() );
      return folded == 0 ? left < right : folded < 0;
    } );
  }
  else
  {
    result.error = QStringLiteral( "所选路径既不是常规文件，也不是文件夹。" );
    return result;
  }

  if ( packagePaths.isEmpty() )
  {
    result.error = result.isDirectorySelection
      ? emptyDirectoryDiagnostic()
      : QStringLiteral( "所选路径中未找到支持的 MTPL 数据包。" );
    return result;
  }

  for ( qsizetype packageIndex = 0; packageIndex < packagePaths.size(); ++packageIndex )
  {
    if ( canceled( cancelCheck ) )
    {
      result.canceled = true;
      result.error = QStringLiteral( "检查已取消。" );
      return result;
    }
    const QString &packagePath = packagePaths.at( packageIndex );
    QgsMtpl::PackageDescriptor descriptor;
    QString packageError;
    const double packageBase = static_cast<double>( packageIndex ) * 100.0 / static_cast<double>( packagePaths.size() );
    const double packageSpan = 100.0 / static_cast<double>( packagePaths.size() );
    const bool inspected = probePackage(
      packagePath, descriptor, packageError, keys, readMetadata, suppliedSource, cancelCheck,
      [&progress, packageBase, packageSpan]( double value )
      {
        reportProgress( progress, packageBase + packageSpan * value / 100.0 );
      }, !result.isDirectorySelection, true, preferPtpSidecar );
    if ( canceled( cancelCheck ) )
    {
      result.canceled = true;
      result.error = QStringLiteral( "检查已取消。" );
      return result;
    }
    // Unknown files in a directory are unrelated selection noise. Files with a
    // supported MTPL suffix must remain visible even when their contents are
    // corrupt or use an unsupported version, so users can diagnose them.
    const bool unrelatedDirectoryFile = !inspected && descriptor.format == QgsMtpl::PackageFormat::Unknown;
    if ( result.isDirectorySelection && !inspected && unrelatedDirectoryFile )
    {
      ++result.ignoredFileCount;
      continue;
    }
    if ( result.isDirectorySelection )
      clearExternalTileJsonDisplayMetadata( descriptor );
    result.packages.append( descriptor );
  }

  if ( result.packages.isEmpty() )
  {
    result.error = result.isDirectorySelection
      ? emptyDirectoryDiagnostic()
      : QStringLiteral( "所选路径中未找到支持的 MTPL 数据包。" );
    return result;
  }

  if ( result.isDirectorySelection )
    applyExternalDisplayMetadata( inputInfo.absoluteFilePath(), true, result.packages );

  result.ok = true;
  reportProgress( progress, 100.0 );
  return result;
}

bool QgsMtplPackageService::probePackage( const QString &path,
                                          QgsMtpl::PackageDescriptor &descriptor,
                                          QString &error,
                                          const QgsMtpl::CryptoKeys &suppliedKeys,
                                          bool readMetadata,
                                          QgsMtpl::CredentialSource suppliedSource,
                                          const CancelCheck &cancelCheck,
                                          const ProgressCallback &progress,
                                          bool supplementExternalDisplayMetadata,
                                          bool inspectTileStructure,
                                          bool preferPtpSidecar )
{
  reportProgress( progress, 0.0 );
  if ( canceled( cancelCheck ) )
  {
    error = QStringLiteral( "检查已取消。" );
    return false;
  }
  const QFileInfo info( path );
  descriptor = QgsMtpl::PackageDescriptor();
  descriptor.path = info.absoluteFilePath();
  descriptor.displayName = info.completeBaseName();
  // Preserve the declared format for damaged packages so a supported suffix
  // is shown as an unavailable package instead of an unrelated file.
  descriptor.format = QgsMtpl::packageFormatFromPath( descriptor.path );

  if ( !info.isFile() )
  {
    error = QStringLiteral( "数据包路径不是常规文件。" );
    setReadiness( descriptor, QgsMtpl::ReadinessState::Unreadable, error );
    return false;
  }

  const QByteArray nativePath = descriptor.path.toUtf8();
  mtpl_package_info_t packageInfo = MTPL_PACKAGE_INFO_INIT;
  mtpl_status_t status = mtpl_package_probe( nativePath.constData(), &packageInfo );
  if ( status != MTPL_STATUS_OK )
  {
    return applyInspectionFailure( descriptor, QStringLiteral( "无法识别数据包" ), status,
                                   false, false, error );
  }
  descriptor.format = packageFormat( packageInfo.format );
  if ( descriptor.format == QgsMtpl::PackageFormat::Unknown )
  {
    error = QStringLiteral( "不支持此数据包格式。" );
    setReadiness( descriptor, QgsMtpl::ReadinessState::Unsupported, error );
    return false;
  }
  descriptor.tileSize = static_cast<int>( packageInfo.tile_size );
  descriptor.fileSize = static_cast<quint64>( packageInfo.file_size );
  descriptor.fileLastModifiedMs = info.lastModified().toMSecsSinceEpoch();
  descriptor.metadata.insert( QStringLiteral( "formatVersion" ), packageInfo.format_version );
  descriptor.metadata.insert( QStringLiteral( "fileSize" ), static_cast<qulonglong>( packageInfo.file_size ) );
  descriptor.metadata.insert( QStringLiteral( "rangeCount" ), static_cast<qulonglong>( packageInfo.range_count ) );
  descriptor.metadata.insert( QStringLiteral( "entryCount" ), static_cast<qulonglong>( packageInfo.entry_count ) );
  reportProgress( progress, 8.0 );

  const bool explicitKeysProvided = suppliedKeys.hasAnyValue();
  QString suppliedKeyError;
  const bool explicitKeysValid = explicitKeysProvided && suppliedKeys.isValid( &suppliedKeyError );
  QgsMtpl::CryptoKeys effectiveKeys;
  QgsMtpl::KeySidecar sidecar;
  QString discoveredSidecar;
  QString ignoredDiscoveryError;
  // Selection and refresh may reuse a key validated for a different dataset.
  // An exact PTP sidecar takes precedence over that cached key. Explicit key
  // validation leaves this option disabled so an entered wrong key still fails.
  const bool fallbackKeysProvided = explicitKeysProvided &&
    ( suppliedSource == QgsMtpl::CredentialSource::Remembered ||
      ( preferPtpSidecar && descriptor.format == QgsMtpl::PackageFormat::Ptp ) );
  const QgsMtpl::CredentialSource normalizedSuppliedSource =
    suppliedSource == QgsMtpl::CredentialSource::None || suppliedSource == QgsMtpl::CredentialSource::Sidecar
      ? QgsMtpl::CredentialSource::Explicit : suppliedSource;
  if ( explicitKeysProvided && !fallbackKeysProvided )
  {
    descriptor.credentialSource = normalizedSuppliedSource;
    if ( explicitKeysValid )
      effectiveKeys = suppliedKeys;
  }
  else if ( QgsMtpl::KeySidecarStore::discover( descriptor.path, sidecar, discoveredSidecar, ignoredDiscoveryError, cancelCheck ) )
  {
    descriptor.sidecarPath = discoveredSidecar;
    descriptor.keyId = sidecar.keys.keyId;
    descriptor.credentialSource = QgsMtpl::CredentialSource::Sidecar;

    // Transfer the discovered material into an RAII owner instead of making
    // another implicitly shared QByteArray copy. effectiveKeys then receives
    // its own allocation and both owners wipe their buffers on scope exit.
    QgsMtpl::CryptoKeys sidecarKeys;
    sidecarKeys.privateKey.swap( sidecar.keys.privateKeyBase64 );
    sidecarKeys.deviceKey.swap( sidecar.keys.deviceKeyHex );
    effectiveKeys = sidecarKeys;
  }
  else if ( fallbackKeysProvided )
  {
    descriptor.credentialSource = normalizedSuppliedSource;
    if ( explicitKeysValid )
      effectiveKeys = suppliedKeys;
  }
  if ( canceled( cancelCheck ) )
  {
    error = QStringLiteral( "检查已取消。" );
    return false;
  }
  const bool credentialsAvailable = effectiveKeys.isValid();
  const CryptoContext crypto( effectiveKeys );

  mtpl_key_validation_t keyValidation = MTPL_KEY_VALIDATION_REQUIRED;
  status = mtpl_package_validate_key( nativePath.constData(), crypto.get(), &keyValidation );
  reportProgress( progress, 18.0 );
  const bool invalidSelectedCredentials = explicitKeysProvided && !explicitKeysValid && !credentialsAvailable;
  if ( invalidSelectedCredentials && packageInfo.storage != MTPL_PACKAGE_STORAGE_PLAIN )
    keyValidation = MTPL_KEY_VALIDATION_REJECTED_OR_CORRUPT;

  const bool expectedValidationStatus =
    ( status == MTPL_STATUS_OK &&
      ( keyValidation == MTPL_KEY_VALIDATION_NOT_REQUIRED ||
        keyValidation == MTPL_KEY_VALIDATION_VALID ||
        keyValidation == MTPL_KEY_VALIDATION_UNVERIFIABLE_EMPTY ) ) ||
    ( status == MTPL_STATUS_KEY_REQUIRED && keyValidation == MTPL_KEY_VALIDATION_REQUIRED ) ||
    ( status == MTPL_STATUS_CRYPTO_ERROR && keyValidation == MTPL_KEY_VALIDATION_REJECTED_OR_CORRUPT );
  if ( !expectedValidationStatus && !invalidSelectedCredentials )
  {
    return applyInspectionFailure( descriptor, QStringLiteral( "无法验证数据包密钥" ), status,
                                   packageInfo.storage != MTPL_PACKAGE_STORAGE_PLAIN,
                                   explicitKeysProvided || credentialsAvailable, error );
  }

  switch ( keyValidation )
  {
    case MTPL_KEY_VALIDATION_NOT_REQUIRED:
      descriptor.encryption = QgsMtpl::EncryptionState::Plain;
      descriptor.credentialSource = QgsMtpl::CredentialSource::None;
      setReadiness( descriptor, QgsMtpl::ReadinessState::PlainReady,
                    QStringLiteral( "此数据包不需要密钥。" ) );
      break;
    case MTPL_KEY_VALIDATION_VALID:
      descriptor.encryption = QgsMtpl::EncryptionState::Encrypted;
      setReadiness( descriptor, QgsMtpl::ReadinessState::KeyVerified,
                    QStringLiteral( "已根据数据包内容验证密钥。" ) );
      break;
    case MTPL_KEY_VALIDATION_REQUIRED:
      descriptor.encryption = QgsMtpl::EncryptionState::Locked;
      setReadiness( descriptor, QgsMtpl::ReadinessState::KeyRequired,
                    QStringLiteral( "此数据包包含加密内容，需要密钥。" ) );
      break;
    case MTPL_KEY_VALIDATION_REJECTED_OR_CORRUPT:
      descriptor.encryption = QgsMtpl::EncryptionState::Locked;
      setReadiness( descriptor, QgsMtpl::ReadinessState::KeyRejectedOrCorrupt,
                    invalidSelectedCredentials ? suppliedKeyError
                                               : QStringLiteral( "密钥不匹配，或数据包已损坏。" ) );
      break;
    case MTPL_KEY_VALIDATION_UNVERIFIABLE_EMPTY:
      if ( credentialsAvailable )
      {
        descriptor.encryption = QgsMtpl::EncryptionState::Encrypted;
        setReadiness( descriptor, QgsMtpl::ReadinessState::UnverifiableEmpty,
                      QStringLiteral( "已提供有效格式的密钥，但此加密空包没有载荷可用于验证密钥。" ) );
      }
      else
      {
        descriptor.encryption = QgsMtpl::EncryptionState::Locked;
        descriptor.credentialSource = QgsMtpl::CredentialSource::None;
        setReadiness( descriptor, QgsMtpl::ReadinessState::KeyRequired,
                      QStringLiteral( "此加密空包需要密钥。空包没有载荷可用于验证密钥。" ) );
      }
      break;
  }

  if ( descriptor.format == QgsMtpl::PackageFormat::Sfp )
  {
    mtpl_sfp_reader_t *reader = nullptr;
    status = mtpl_sfp_reader_open( nativePath.constData(), crypto.get(), &reader );
    if ( status != MTPL_STATUS_OK )
      return applyInspectionFailure( descriptor, QStringLiteral( "无法打开 SFP 数据包" ), status,
                                     false, explicitKeysProvided || credentialsAvailable, error );

    size_t entryCount = 0;
    status = mtpl_sfp_reader_get_entry_count( reader, &entryCount );
    quint64 encryptedEntryCount = 0;
    quint64 logicalSize = 0;
    quint64 storedSize = 0;
    quint64 unsafeEntryCount = 0;
    if ( status == MTPL_STATUS_OK )
    {
      if ( entryCount > static_cast<size_t>( std::numeric_limits<qsizetype>::max() ) )
        status = MTPL_STATUS_CORRUPT_DATA;
      else
        descriptor.sfpEntries.reserve( static_cast<qsizetype>( entryCount ) );
      for ( size_t index = 0; status == MTPL_STATUS_OK && index < entryCount; ++index )
      {
        if ( canceled( cancelCheck ) )
        {
          status = MTPL_STATUS_CANCELED;
          break;
        }
        reportProgress( progress, 18.0 + static_cast<double>( index ) * 82.0 /
                                    static_cast<double>( std::max<size_t>( entryCount, 1 ) ) );
        mtpl_sfp_entry_info_t entry = {};
        status = mtpl_sfp_reader_get_entry_info( reader, index, &entry );
        if ( status != MTPL_STATUS_OK || !entry.path )
        {
          if ( status == MTPL_STATUS_OK )
            status = MTPL_STATUS_CORRUPT_DATA;
          break;
        }
        if ( entry.storage_mode == MTPL_STORAGE_ENCRYPTED )
          ++encryptedEntryCount;
        if ( std::numeric_limits<quint64>::max() - logicalSize < entry.logical_size ||
             std::numeric_limits<quint64>::max() - storedSize < entry.stored_size )
        {
          status = MTPL_STATUS_CORRUPT_DATA;
          break;
        }
        logicalSize += entry.logical_size;
        storedSize += entry.stored_size;

        QString safePath;
        if ( !normalizedSafeSfpPath( QByteArray( entry.path ), safePath ) )
        {
          ++unsafeEntryCount;
          continue;
        }
        QgsMtpl::SfpEntryDescriptor snapshot;
        snapshot.path = safePath;
        snapshot.logicalSize = entry.logical_size;
        snapshot.storedSize = entry.stored_size;
        snapshot.encrypted = entry.storage_mode == MTPL_STORAGE_ENCRYPTED;
        descriptor.sfpEntries.append( snapshot );
      }
    }
    descriptor.payload = QgsMtpl::PayloadType::Files;
    descriptor.metadata.insert( QStringLiteral( "entryCount" ), static_cast<qulonglong>( entryCount ) );
    descriptor.metadata.insert( QStringLiteral( "encryptedEntryCount" ), encryptedEntryCount );
    descriptor.metadata.insert( QStringLiteral( "plainEntryCount" ), static_cast<qulonglong>( entryCount ) - encryptedEntryCount );
    descriptor.metadata.insert( QStringLiteral( "logicalSize" ), logicalSize );
    descriptor.metadata.insert( QStringLiteral( "storedSize" ), storedSize );
    if ( unsafeEntryCount > 0 )
      descriptor.metadata.insert( QStringLiteral( "unsafeEntryCount" ), unsafeEntryCount );
    const mtpl_status_t closeStatus = mtpl_sfp_reader_close( reader );
    if ( status == MTPL_STATUS_OK )
      status = closeStatus;
    if ( status == MTPL_STATUS_CANCELED )
    {
      error = QStringLiteral( "检查已取消。" );
      return false;
    }
    if ( status != MTPL_STATUS_OK )
      return applyInspectionFailure( descriptor, QStringLiteral( "无法检查 SFP 数据包" ), status,
                                     packageInfo.storage != MTPL_PACKAGE_STORAGE_PLAIN,
                                     explicitKeysProvided || credentialsAvailable, error );
    error.clear();
    reportProgress( progress, 100.0 );
    return true;
  }

  void *reader = nullptr;
  status = openTileReader( descriptor.format, nativePath, crypto.get(), &reader );
  if ( status != MTPL_STATUS_OK )
    return applyInspectionFailure( descriptor, QStringLiteral( "无法打开瓦片数据包" ), status,
                                   packageInfo.storage != MTPL_PACKAGE_STORAGE_PLAIN,
                                   explicitKeysProvided || credentialsAvailable, error );

  if ( readMetadata )
  {
    mtpl_buffer_t metadata = {};
    status = tileMetadata( descriptor.format, reader, &metadata );
    if ( status == MTPL_STATUS_OK )
      storeMetadata( metadata, descriptor );
    mtpl_buffer_release( &metadata );
  }

  QString ptpContractError;
  QString ptpContractFailureKind;
  if ( status == MTPL_STATUS_OK )
  {
    if ( descriptor.format == QgsMtpl::PackageFormat::Dtp )
      descriptor.payload = QgsMtpl::PayloadType::Elevation;
    else if ( descriptor.format == QgsMtpl::PackageFormat::Vtp )
      descriptor.payload = QgsMtpl::PayloadType::VectorTile;
    else
    {
      descriptor.payload = payloadFromMetadata( descriptor.metadata );
      if ( descriptor.payload == QgsMtpl::PayloadType::Elevation )
      {
        ptpContractFailureKind = QStringLiteral( "declared-elevation" );
        ptpContractError = QStringLiteral(
          "PTP 仅支持 PNG、JPEG 或 WebP 影像瓦片。元数据声明的是高程载荷，请使用 DTP。" );
      }
      else if ( descriptor.payload == QgsMtpl::PayloadType::VectorTile )
      {
        ptpContractFailureKind = QStringLiteral( "declared-vector" );
        ptpContractError = QStringLiteral(
          "PTP 仅支持 PNG、JPEG 或 WebP 影像瓦片。元数据声明的是 PBF/MVT 矢量载荷，请使用 VTP。" );
      }
    }
    applySpatialMetadata( descriptor );
    if ( inspectTileStructure )
    {
      status = storeTileRangeSummary(
        descriptor.format, reader, descriptor, cancelCheck,
        [&progress]( double value ) { reportProgress( progress, 25.0 + value * 0.65 ); } );
    }
  }

  const bool validatePtpPayload = inspectTileStructure && status == MTPL_STATUS_OK &&
                                  descriptor.format == QgsMtpl::PackageFormat::Ptp &&
                                  ( descriptor.isReady() || descriptor.isCredentialedEmptyPtp() ) &&
                                  ptpContractError.isEmpty();
  if ( validatePtpPayload )
  {
    mtpl_tile_coordinate_t coordinate = {};
    bool found = false;
    status = firstPresentTile( descriptor.format, reader, coordinate, found, cancelCheck );
    if ( status == MTPL_STATUS_OK && found )
    {
      mtpl_buffer_t tile = {};
      status = tileRead( descriptor.format, reader, &coordinate, &tile );
      if ( status == MTPL_STATUS_OK )
      {
        if ( tile.size > static_cast<size_t>( std::numeric_limits<int>::max() ) )
        {
          ptpContractFailureKind = QStringLiteral( "image-decode" );
          ptpContractError = QStringLiteral( "PTP 代表影像瓦片过大，无法安全解码。" );
        }
        else
        {
          const QByteArray bytes( reinterpret_cast<const char *>( tile.data ), static_cast<int>( tile.size ) );
          if ( validatePtpImagePayload( bytes, descriptor.tileSize, ptpContractError ) )
            descriptor.payload = QgsMtpl::PayloadType::RasterImage;
          else
            ptpContractFailureKind = QStringLiteral( "image-decode" );
        }
      }
      mtpl_buffer_release( &tile );
    }
    else if ( status == MTPL_STATUS_OK && descriptor.payload != QgsMtpl::PayloadType::RasterImage )
    {
      // A structurally valid empty package is readable, but it cannot prove an
      // image encoding on its own. The aggregate builder may inherit the
      // image contract from a compatible non-empty package in the same leaf
      // dataset. A standalone empty package remains non-loadable.
      setReadiness(
        descriptor,
        descriptor.readiness,
        QStringLiteral( "数据包可读取，但空包未声明 PNG、JPEG 或 WebP。只有同一数据集存在已验证影像包时才能加载。" ) );
    }
  }

  const mtpl_status_t closeStatus = closeTileReader( descriptor.format, reader );
  if ( status == MTPL_STATUS_OK )
    status = closeStatus;
  if ( status == MTPL_STATUS_CANCELED )
  {
    error = QStringLiteral( "检查已取消。" );
    return false;
  }
  if ( status != MTPL_STATUS_OK )
    return applyInspectionFailure( descriptor, QStringLiteral( "无法检查瓦片数据包" ), status,
                                   packageInfo.storage != MTPL_PACKAGE_STORAGE_PLAIN,
                                   explicitKeysProvided || credentialsAvailable, error );
  if ( !ptpContractError.isEmpty() )
  {
    descriptor.metadata.insert( QStringLiteral( "_mtplPtpContractFailure" ), ptpContractFailureKind );
    setReadiness( descriptor, QgsMtpl::ReadinessState::Unsupported, ptpContractError );
    error = ptpContractError;
    return false;
  }

  if ( descriptor.format == QgsMtpl::PackageFormat::Ptp && supplementExternalDisplayMetadata )
  {
    QList<QgsMtpl::PackageDescriptor> packageList { descriptor };
    applyExternalTileJsonDisplayMetadata( info, false, packageList );
    descriptor = packageList.constFirst();
  }

  error.clear();
  reportProgress( progress, 100.0 );
  return true;
}

void QgsMtplPackageService::applyExternalDisplayMetadata( const QString &selectionPath,
                                                          bool directorySelection,
                                                          QList<QgsMtpl::PackageDescriptor> &packages )
{
  applyExternalTileJsonDisplayMetadata( QFileInfo( selectionPath ), directorySelection, packages );
}

QgsMtplProbeTask::QgsMtplProbeTask( const QString &path,
                                    const QgsMtpl::CryptoKeys &keys,
                                    bool readMetadata,
                                    QgsMtpl::CredentialSource suppliedSource,
                                    bool preferPtpSidecar )
  : QgsTask( tr( "检查 MTPL 数据包" ), QgsTask::CanCancel | QgsTask::CancelWithoutPrompt )
  , mPath( path )
  , mKeys( keys )
  , mReadMetadata( readMetadata )
  , mSuppliedSource( suppliedSource )
  , mPreferPtpSidecar( preferPtpSidecar )
{
  QMutexLocker locker( &sActiveProbeTaskMutex );
  sActiveProbeTasks.insert( this );
}

QgsMtplProbeTask::~QgsMtplProbeTask()
{
  mKeys.clear();
  QMutexLocker locker( &sActiveProbeTaskMutex );
  sActiveProbeTasks.remove( this );
}

void QgsMtplProbeTask::cancelAndWaitForAllActiveTasks()
{
  while ( true )
  {
    QList<QPointer<QgsMtplProbeTask>> tasks;
    {
      QMutexLocker locker( &sActiveProbeTaskMutex );
      tasks.reserve( sActiveProbeTasks.size() );
      for ( QgsMtplProbeTask *task : std::as_const( sActiveProbeTasks ) )
        tasks.append( task );
    }
    if ( tasks.isEmpty() )
      return;

    for ( const QPointer<QgsMtplProbeTask> &task : std::as_const( tasks ) )
    {
      if ( task )
        task->cancel();
    }
    for ( const QPointer<QgsMtplProbeTask> &task : std::as_const( tasks ) )
    {
      if ( task )
        task->waitForFinished( 0 );
    }
    QCoreApplication::sendPostedEvents( nullptr, 0 );
    QCoreApplication::sendPostedEvents( nullptr, QEvent::DeferredDelete );
  }
}

bool QgsMtplProbeTask::run()
{
  mResult = QgsMtplPackageService::probePath(
    mPath, mKeys, mReadMetadata, mSuppliedSource,
    [this]() { return isCanceled(); },
    [this]( double value ) { setProgress( value ); }, mPreferPtpSidecar );
  mKeys.clear();
  return mResult.ok && !mResult.canceled;
}
