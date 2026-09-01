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

#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
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
    for ( auto it = map.constBegin(); it != map.constEnd(); ++it )
    {
      for ( const QString &name : names )
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
    if ( type.contains( QLatin1String( "pbf" ) ) || type.contains( QLatin1String( "mvt" ) ) ||
         type.contains( QLatin1String( "vector" ) ) )
      return QgsMtpl::PayloadType::VectorTile;
    if ( type.contains( QLatin1String( "png" ) ) || type.contains( QLatin1String( "jpg" ) ) ||
         type.contains( QLatin1String( "jpeg" ) ) || type.contains( QLatin1String( "webp" ) ) ||
         type.contains( QLatin1String( "image" ) ) || type.contains( QLatin1String( "raster" ) ) )
      return QgsMtpl::PayloadType::RasterImage;
    return QgsMtpl::PayloadType::Unknown;
  }

  QgsMtpl::PayloadType payloadFromBytes( const QByteArray &bytes )
  {
    if ( bytes.startsWith( QByteArray::fromHex( "89504e470d0a1a0a" ) ) ||
         bytes.startsWith( QByteArray::fromHex( "ffd8ff" ) ) ||
         ( bytes.size() >= 12 && bytes.startsWith( "RIFF" ) && bytes.mid( 8, 4 ) == QByteArrayLiteral( "WEBP" ) ) )
      return QgsMtpl::PayloadType::RasterImage;
    return QgsMtpl::PayloadType::Unknown;
  }

  void applySpatialMetadata( QgsMtpl::PackageDescriptor &descriptor )
  {
    const QVariant crs = valueCaseInsensitive( descriptor.metadata, QStringList()
      << QStringLiteral( "crs" ) << QStringLiteral( "crsAuthId" ) << QStringLiteral( "srs" ) );
    if ( crs.isValid() && !crs.toString().isEmpty() )
      descriptor.crsAuthId = crs.toString();

    const QVariant scheme = valueCaseInsensitive( descriptor.metadata, QStringList() << QStringLiteral( "scheme" ) );
    if ( scheme.isValid() && ( scheme.toString().compare( QLatin1String( "xyz" ), Qt::CaseInsensitive ) == 0 ||
                               scheme.toString().compare( QLatin1String( "tms" ), Qt::CaseInsensitive ) == 0 ) )
      descriptor.scheme = scheme.toString().toLower();

    if ( descriptor.format == QgsMtpl::PackageFormat::Dtp )
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
      descriptor.metadata = document.object().toVariantMap();
    else if ( !bytes.isEmpty() )
      descriptor.metadata.insert( QStringLiteral( "rawMetadata" ), QString::fromUtf8( bytes ) );
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
      if ( info.present_count == 0 )
        continue;

      if ( info.present_count > info.slot_count )
        return MTPL_STATUS_CORRUPT_DATA;

      mtpl_tile_range_t presentBounds = info.bounds;
      if ( info.present_count < info.slot_count )
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
    if ( descriptor.minimumZoom >= 0 )
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
                                                       const ProgressCallback &progress )
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
    result.error = QStringLiteral( "所选路径中未找到支持的 MTPL 数据包。" );
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
      } );
    if ( canceled( cancelCheck ) )
    {
      result.canceled = true;
      result.error = QStringLiteral( "检查已取消。" );
      return result;
    }
    // Unknown files in a directory are unrelated selection noise. Files with a
    // supported MTPL suffix must remain visible even when their contents are
    // corrupt or use an unsupported version, so users can diagnose them.
    const bool unrelatedDirectoryFile = !inspected &&
                                        QgsMtpl::packageFormatFromPath( packagePath ) == QgsMtpl::PackageFormat::Unknown;
    if ( result.isDirectorySelection && !inspected && unrelatedDirectoryFile )
    {
      ++result.ignoredFileCount;
      continue;
    }
    result.packages.append( descriptor );
  }

  if ( result.packages.isEmpty() )
  {
    result.error = QStringLiteral( "所选路径中未找到支持的 MTPL 数据包。" );
    return result;
  }

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
                                          const ProgressCallback &progress )
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
  const bool rememberedKeysProvided = explicitKeysProvided && suppliedSource == QgsMtpl::CredentialSource::Remembered;
  if ( explicitKeysProvided && !rememberedKeysProvided )
  {
    descriptor.credentialSource = suppliedSource == QgsMtpl::CredentialSource::None || suppliedSource == QgsMtpl::CredentialSource::Sidecar
      ? QgsMtpl::CredentialSource::Explicit
      : suppliedSource;
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
  else if ( rememberedKeysProvided )
  {
    descriptor.credentialSource = QgsMtpl::CredentialSource::Remembered;
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
      descriptor.encryption = QgsMtpl::EncryptionState::Locked;
      setReadiness( descriptor, QgsMtpl::ReadinessState::UnverifiableEmpty,
                    QStringLiteral( "此加密数据包没有可用于验证密钥的载荷。" ) );
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

  if ( status == MTPL_STATUS_OK )
  {
    applySpatialMetadata( descriptor );
    status = storeTileRangeSummary(
      descriptor.format, reader, descriptor, cancelCheck,
      [&progress]( double value ) { reportProgress( progress, 25.0 + value * 0.65 ); } );
  }

  if ( descriptor.format == QgsMtpl::PackageFormat::Dtp )
    descriptor.payload = QgsMtpl::PayloadType::Elevation;
  else if ( descriptor.format == QgsMtpl::PackageFormat::Vtp )
    descriptor.payload = QgsMtpl::PayloadType::VectorTile;
  else
    descriptor.payload = payloadFromMetadata( descriptor.metadata );

  const bool detectUnknownPtpPayload = status == MTPL_STATUS_OK && descriptor.format == QgsMtpl::PackageFormat::Ptp &&
                                       descriptor.payload == QgsMtpl::PayloadType::Unknown &&
                                       descriptor.isReady();
  if ( detectUnknownPtpPayload )
  {
    mtpl_tile_coordinate_t coordinate = {};
    bool found = false;
    status = firstPresentTile( descriptor.format, reader, coordinate, found, cancelCheck );
    if ( status == MTPL_STATUS_OK && found )
    {
      mtpl_buffer_t tile = {};
      status = tileRead( descriptor.format, reader, &coordinate, &tile );
      if ( status == MTPL_STATUS_OK && detectUnknownPtpPayload )
      {
        const size_t byteCount = std::min<size_t>( tile.size, static_cast<size_t>( std::numeric_limits<int>::max() ) );
        const QByteArray bytes( reinterpret_cast<const char *>( tile.data ), static_cast<int>( byteCount ) );
        descriptor.payload = payloadFromBytes( bytes );
      }
      mtpl_buffer_release( &tile );
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

  error.clear();
  reportProgress( progress, 100.0 );
  return true;
}

QgsMtplProbeTask::QgsMtplProbeTask( const QString &path,
                                    const QgsMtpl::CryptoKeys &keys,
                                    bool readMetadata,
                                    QgsMtpl::CredentialSource suppliedSource )
  : QgsTask( tr( "检查 MTPL 数据包" ), QgsTask::CanCancel | QgsTask::CancelWithoutPrompt )
  , mPath( path )
  , mKeys( keys )
  , mReadMetadata( readMetadata )
  , mSuppliedSource( suppliedSource )
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
    [this]( double value ) { setProgress( value ); } );
  mKeys.clear();
  return mResult.ok && !mResult.canceled;
}
