/***************************************************************************
  qgsmtplrasterinterface.cpp
  --------------------------
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

#include "qgsmtplrasterinterface.h"

#include "../services/qgsmtplpackageservice.h"
#include "../services/qgsmtplstatus.h"

#include "qgscoordinatereferencesystem.h"
#include "qgsrasterblock.h"

#include <mtpl/mtpl.h>

#include <QCache>
#include <QBuffer>
#include <QColor>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QSet>
#include <QVector>
#include <QWaitCondition>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <utility>

class QgsMtplStructureValidationCache::Private
{
  public:
    enum class Status
    {
      Validating,
      Valid,
      Invalid
    };

    struct Entry
    {
      quint64 fileSize = 0;
      qint64 fileLastModifiedMs = -1;
      quint64 serial = 0;
      Status status = Status::Validating;
      QString error;
    };

    QMutex mutex;
    QWaitCondition condition;
    QHash<QString, Entry> entries;
    quint64 generation = 0;
    quint64 nextSerial = 0;
    quint64 validationAttempts = 0;
};

QgsMtplStructureValidationCache::QgsMtplStructureValidationCache()
  : d( std::make_unique<Private>() )
{}

QgsMtplStructureValidationCache::~QgsMtplStructureValidationCache() = default;

QString QgsMtplStructureValidationCache::packageIdentity( int packageIndex,
                                                          const QString &path,
                                                          const QString &structureSummary )
{
  const QFileInfo info( path );
  QString normalizedPath = info.canonicalFilePath();
  if ( normalizedPath.isEmpty() )
    normalizedPath = info.absoluteFilePath();
  normalizedPath = QDir::cleanPath( QDir::fromNativeSeparators( normalizedPath ) );
#ifdef Q_OS_WIN
  normalizedPath = normalizedPath.toCaseFolded();
#endif
  return QStringLiteral( "%1|%2|%3" )
    .arg( packageIndex )
    .arg( normalizedPath, structureSummary );
}

bool QgsMtplStructureValidationCache::validate( const QString &packageIdentity,
                                                quint64 fileSize,
                                                qint64 fileLastModifiedMs,
                                                QgsRasterBlockFeedback *feedback,
                                                const Validator &validator,
                                                QString &error,
                                                bool &wasCanceled )
{
  error.clear();
  wasCanceled = false;
  quint64 claimedSerial = 0;
  quint64 claimedGeneration = 0;

  for ( ;; )
  {
    if ( feedback && feedback->isCanceled() )
    {
      wasCanceled = true;
      return false;
    }

    QMutexLocker locker( &d->mutex );
    auto it = d->entries.find( packageIdentity );
    if ( it != d->entries.end() &&
         it->fileSize == fileSize &&
         it->fileLastModifiedMs == fileLastModifiedMs )
    {
      if ( it->status == Private::Status::Valid )
        return true;
      if ( it->status == Private::Status::Invalid )
      {
        error = it->error;
        return false;
      }

      d->condition.wait( &d->mutex, 25 );
      continue;
    }

    Private::Entry entry;
    entry.fileSize = fileSize;
    entry.fileLastModifiedMs = fileLastModifiedMs;
    entry.serial = ++d->nextSerial;
    entry.status = Private::Status::Validating;
    claimedSerial = entry.serial;
    claimedGeneration = d->generation;
    d->entries.insert( packageIdentity, entry );
    ++d->validationAttempts;
    break;
  }

  const bool valid = validator && validator( error, wasCanceled );
  {
    QMutexLocker locker( &d->mutex );
    auto it = d->entries.find( packageIdentity );
    if ( claimedGeneration == d->generation &&
         it != d->entries.end() &&
         it->serial == claimedSerial )
    {
      if ( wasCanceled )
      {
        d->entries.erase( it );
      }
      else
      {
        it->status = valid ? Private::Status::Valid : Private::Status::Invalid;
        it->error = error;
      }
    }
    d->condition.wakeAll();
  }
  return valid;
}

void QgsMtplStructureValidationCache::invalidate()
{
  QMutexLocker locker( &d->mutex );
  ++d->generation;
  d->entries.clear();
  d->condition.wakeAll();
}

void QgsMtplStructureValidationCache::invalidatePackage( const QString &packageIdentity )
{
  QMutexLocker locker( &d->mutex );
  d->entries.remove( packageIdentity );
  d->condition.wakeAll();
}

quint64 QgsMtplStructureValidationCache::validationAttemptCount() const
{
  QMutexLocker locker( &d->mutex );
  return d->validationAttempts;
}

bool QgsMtplStructureValidationCache::validatedFileStamp( const QString &packageIdentity,
                                                          quint64 &fileSize,
                                                          qint64 &fileLastModifiedMs ) const
{
  QMutexLocker locker( &d->mutex );
  const auto it = d->entries.constFind( packageIdentity );
  if ( it == d->entries.constEnd() || it->status != Private::Status::Valid )
    return false;
  fileSize = it->fileSize;
  fileLastModifiedMs = it->fileLastModifiedMs;
  return true;
}

namespace
{

struct MtplBuffer
{
  mtpl_buffer_t buffer = { nullptr, 0 };

  ~MtplBuffer()
  {
    mtpl_buffer_release( &buffer );
  }

  QByteArray toByteArray() const
  {
    if ( !buffer.data || buffer.size == 0 )
      return QByteArray();
    return QByteArray( reinterpret_cast<const char *>( buffer.data ), static_cast<qsizetype>( buffer.size ) );
  }
};

class TileReader final
{
  public:
    explicit TileReader( QgsMtpl::PackageFormat format )
      : mFormat( format )
    {}

    ~TileReader()
    {
      close();
    }

    mtpl_status_t open( const QString &path, const QgsMtpl::CryptoKeys &keys )
    {
      const QByteArray utf8Path = path.toUtf8();
      mtpl_crypto_options_t crypto = {};
      const mtpl_crypto_options_t *cryptoPtr = nullptr;
      if ( keys.isValid() )
      {
        crypto.private_key = reinterpret_cast<const uint8_t *>( keys.privateKey.constData() );
        crypto.private_key_size = static_cast<size_t>( keys.privateKey.size() );
        crypto.device_key = reinterpret_cast<const uint8_t *>( keys.deviceKey.constData() );
        crypto.device_key_size = static_cast<size_t>( keys.deviceKey.size() );
        cryptoPtr = &crypto;
      }

      switch ( mFormat )
      {
        case QgsMtpl::PackageFormat::Ptp:
          return mtpl_ptp_reader_open( utf8Path.constData(), cryptoPtr, &mPtp );
        case QgsMtpl::PackageFormat::Dtp:
          return mtpl_dtp_reader_open( utf8Path.constData(), cryptoPtr, &mDtp );
        case QgsMtpl::PackageFormat::Vtp:
          return mtpl_vtp_reader_open( utf8Path.constData(), cryptoPtr, &mVtp );
        case QgsMtpl::PackageFormat::Sfp:
        case QgsMtpl::PackageFormat::Unknown:
          return MTPL_STATUS_FORMAT_MISMATCH;
      }
      return MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t readTile( const mtpl_tile_coordinate_t &coordinate, mtpl_buffer_t *buffer ) const
    {
      switch ( mFormat )
      {
        case QgsMtpl::PackageFormat::Ptp:
          return mtpl_ptp_reader_read_tile( mPtp, &coordinate, buffer );
        case QgsMtpl::PackageFormat::Dtp:
          return mtpl_dtp_reader_read_tile( mDtp, &coordinate, buffer );
        case QgsMtpl::PackageFormat::Vtp:
          return mtpl_vtp_reader_read_tile( mVtp, &coordinate, buffer );
        case QgsMtpl::PackageFormat::Sfp:
        case QgsMtpl::PackageFormat::Unknown:
          return MTPL_STATUS_FORMAT_MISMATCH;
      }
      return MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t readMetadata( mtpl_buffer_t *buffer ) const
    {
      return mFormat == QgsMtpl::PackageFormat::Ptp && mPtp
               ? mtpl_ptp_reader_read_metadata( mPtp, buffer )
               : MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t tileSize( uint32_t *tileSize ) const
    {
      return mFormat == QgsMtpl::PackageFormat::Ptp && mPtp
               ? mtpl_ptp_reader_get_tile_size( mPtp, tileSize )
               : MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t storageMode( mtpl_storage_mode_t *mode ) const
    {
      return mFormat == QgsMtpl::PackageFormat::Ptp && mPtp
               ? mtpl_ptp_reader_get_storage_mode( mPtp, mode )
               : MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t rangeCount( size_t *count ) const
    {
      return mFormat == QgsMtpl::PackageFormat::Ptp && mPtp
               ? mtpl_ptp_reader_get_range_count( mPtp, count )
               : MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t rangeInfo( size_t rangeIndex, mtpl_tile_range_info_t *info ) const
    {
      return mFormat == QgsMtpl::PackageFormat::Ptp && mPtp
               ? mtpl_ptp_reader_get_range_info( mPtp, rangeIndex, info )
               : MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t entryInfo( size_t rangeIndex, size_t slotIndex, mtpl_tile_entry_info_t *info ) const
    {
      return mFormat == QgsMtpl::PackageFormat::Ptp && mPtp
               ? mtpl_ptp_reader_get_entry_info( mPtp, rangeIndex, slotIndex, info )
               : MTPL_STATUS_FORMAT_MISMATCH;
    }

  private:
    void close()
    {
      if ( mPtp )
      {
        mtpl_ptp_reader_close( mPtp );
        mPtp = nullptr;
      }
      if ( mDtp )
      {
        mtpl_dtp_reader_close( mDtp );
        mDtp = nullptr;
      }
      if ( mVtp )
      {
        mtpl_vtp_reader_close( mVtp );
        mVtp = nullptr;
      }
    }

    QgsMtpl::PackageFormat mFormat = QgsMtpl::PackageFormat::Unknown;
    mtpl_ptp_reader_t *mPtp = nullptr;
    mtpl_dtp_reader_t *mDtp = nullptr;
    mtpl_vtp_reader_t *mVtp = nullptr;
};

QString metadataString( const QVariantMap &metadata, const QStringList &names, const QString &fallback )
{
  for ( auto it = metadata.constBegin(); it != metadata.constEnd(); ++it )
  {
    for ( const QString &name : names )
    {
      if ( it.key().compare( name, Qt::CaseInsensitive ) == 0 && !it.value().toString().isEmpty() )
        return it.value().toString();
    }
  }
  return fallback;
}

QString elevationDataType( const QgsMtpl::PackageDescriptor &descriptor )
{
  const QString value = metadataString( descriptor.metadata,
                                        { QStringLiteral( "dataType" ), QStringLiteral( "data_type" ), QStringLiteral( "datatype" ) },
                                        QStringLiteral( "uint16" ) ).toLower();
  if ( value == QLatin1String( "float" ) || value == QLatin1String( "float32" ) )
    return QStringLiteral( "float32" );
  if ( value == QLatin1String( "int16" ) || value == QLatin1String( "signed16" ) )
    return QStringLiteral( "int16" );
  return QStringLiteral( "uint16" );
}

QString elevationEndianness( const QgsMtpl::PackageDescriptor &descriptor )
{
  const QString value = metadataString( descriptor.metadata,
                                        { QStringLiteral( "endianness" ), QStringLiteral( "byteOrder" ), QStringLiteral( "byte_order" ) },
                                        QStringLiteral( "little" ) ).toLower();
  return value == QLatin1String( "big" ) || value == QLatin1String( "be" ) || value == QLatin1String( "big-endian" )
           ? QStringLiteral( "big" )
           : QStringLiteral( "little" );
}

QColor elevationColor( double normalized )
{
  normalized = std::clamp( normalized, 0.0, 1.0 );
  const int hue = static_cast<int>( std::lround( 240.0 * ( 1.0 - normalized ) ) );
  return QColor::fromHsv( hue, 220, 235 );
}

double sampleValue( const uchar *data, qsizetype index, const QString &dataType, bool bigEndian )
{
  if ( dataType == QLatin1String( "float32" ) || dataType == QLatin1String( "float" ) )
  {
    const quint32 bits = bigEndian ? qFromBigEndian<quint32>( data + index * 4 ) : qFromLittleEndian<quint32>( data + index * 4 );
    float value = 0;
    std::memcpy( &value, &bits, sizeof( value ) );
    return static_cast<double>( value );
  }

  const quint16 bits = bigEndian ? qFromBigEndian<quint16>( data + index * 2 ) : qFromLittleEndian<quint16>( data + index * 2 );
  if ( dataType == QLatin1String( "int16" ) || dataType == QLatin1String( "signed16" ) )
    return static_cast<double>( static_cast<qint16>( bits ) );
  return static_cast<double>( bits );
}

QImage elevationImage( const QByteArray &bytes, const QgsMtpl::PackageDescriptor &descriptor )
{
  const QString dataType = elevationDataType( descriptor );
  const bool floatData = dataType == QLatin1String( "float32" ) || dataType == QLatin1String( "float" );
  const int bytesPerSample = floatData ? 4 : 2;
  if ( bytes.size() < bytesPerSample )
    return QImage();

  const qsizetype sampleCount = bytes.size() / bytesPerSample;
  const int sourceSide = static_cast<int>( std::llround( std::sqrt( static_cast<double>( sampleCount ) ) ) );
  if ( sourceSide <= 0 || static_cast<qsizetype>( sourceSide ) * sourceSide != sampleCount ||
       ( descriptor.tileSize > 0 && sourceSide != descriptor.tileSize ) )
    return QImage();

  const int side = sourceSide == 33 || sourceSide == 129 ? sourceSide - 1 : sourceSide;
  const bool bigEndian = elevationEndianness( descriptor ) == QLatin1String( "big" );
  const uchar *raw = reinterpret_cast<const uchar *>( bytes.constData() );

  QVector<double> samples( side * side );
  double minimum = std::numeric_limits<double>::infinity();
  double maximum = -std::numeric_limits<double>::infinity();
  for ( int y = 0; y < side; ++y )
  {
    for ( int x = 0; x < side; ++x )
    {
      const qsizetype sourceIndex = static_cast<qsizetype>( y ) * sourceSide + x;
      const double rawValue = sampleValue( raw, sourceIndex, dataType, bigEndian );
      const bool noData = descriptor.hasNoData && qgsDoubleNear( rawValue, descriptor.noData );
      const double value = noData ? std::numeric_limits<double>::quiet_NaN() : rawValue * descriptor.scale + descriptor.offset;
      samples[y * side + x] = value;
      if ( std::isfinite( value ) )
      {
        minimum = std::min( minimum, value );
        maximum = std::max( maximum, value );
      }
    }
  }

  if ( !std::isfinite( minimum ) || !std::isfinite( maximum ) )
    return QImage();

  QImage image( side, side, QImage::Format_ARGB32_Premultiplied );
  const double span = std::max( maximum - minimum, std::numeric_limits<double>::epsilon() );
  for ( int y = 0; y < side; ++y )
  {
    QRgb *line = reinterpret_cast<QRgb *>( image.scanLine( y ) );
    for ( int x = 0; x < side; ++x )
    {
      const double value = samples[y * side + x];
      line[x] = std::isfinite( value ) ? elevationColor( ( value - minimum ) / span ).rgba() : qRgba( 0, 0, 0, 0 );
    }
  }
  return image;
}

int boundedRasterSize( quint64 tiles, int tileSize )
{
  if ( tileSize <= 0 || tiles == 0 )
    return 0;
  const quint64 maximum = static_cast<quint64>( std::numeric_limits<int>::max() );
  if ( tiles > maximum / static_cast<quint64>( tileSize ) )
    return std::numeric_limits<int>::max();
  return static_cast<int>( tiles * static_cast<quint64>( tileSize ) );
}

bool feedbackCanceled( const QgsRasterBlockFeedback *feedback )
{
  return feedback && feedback->isCanceled();
}

bool metadataKeyMatches( const QString &key, const QStringList &names )
{
  return std::any_of( names.constBegin(), names.constEnd(), [&key]( const QString &name )
  {
    return key.compare( name, Qt::CaseInsensitive ) == 0;
  } );
}

QVariant metadataValue( const QVariantMap &metadata, const QStringList &names )
{
  // Aliases are ordered from most to least authoritative. Keep the same
  // deterministic precedence as the initial package probe.
  for ( const QString &name : names )
  {
    for ( auto it = metadata.constBegin(); it != metadata.constEnd(); ++it )
    {
      if ( it.key().compare( name, Qt::CaseInsensitive ) == 0 )
        return it.value();
    }
  }
  return QVariant();
}

bool metadataContains( const QVariantMap &metadata, const QStringList &names )
{
  for ( auto it = metadata.constBegin(); it != metadata.constEnd(); ++it )
  {
    if ( metadataKeyMatches( it.key(), names ) )
      return true;
  }
  return false;
}

QVariantMap decodedPtpMetadata( const MtplBuffer &buffer )
{
  const QByteArray bytes = buffer.toByteArray();
  if ( bytes.isEmpty() )
    return QVariantMap();

  const QJsonDocument document = QJsonDocument::fromJson( bytes );
  if ( document.isObject() )
    return document.object().toVariantMap();

  QVariantMap metadata;
  metadata.insert( QStringLiteral( "rawMetadata" ), QString::fromUtf8( bytes ) );
  return metadata;
}

bool hasOriginalMetadataSnapshot( const QVariantMap &metadata )
{
  for ( auto it = metadata.constBegin(); it != metadata.constEnd(); ++it )
  {
    if ( it.key().compare( QLatin1String( "_mtplRangeBounds" ), Qt::CaseInsensitive ) == 0 )
      return true;
  }
  return false;
}

QVariantMap originalMetadata( const QVariantMap &metadata )
{
  static const QStringList probeKeys {
    QStringLiteral( "formatVersion" ),
    QStringLiteral( "fileSize" ),
    QStringLiteral( "rangeCount" ),
    QStringLiteral( "entryCount" ),
    QStringLiteral( "presentTileCount" ),
    QStringLiteral( "_mtplRangeBounds" ),
    QStringLiteral( "name" ),
    QStringLiteral( "description" ),
    QStringLiteral( "attribution" ),
    QStringLiteral( "center" )
  };
  const QStringList externalDisplayKeys = metadata.value(
    QStringLiteral( "_mtplExternalDisplayKeys" ) ).toStringList();

  QVariantMap result;
  for ( auto it = metadata.constBegin(); it != metadata.constEnd(); ++it )
  {
    const bool internalKey = it.key().startsWith( QLatin1String( "_mtpl" ), Qt::CaseInsensitive );
    if ( !internalKey && !metadataKeyMatches( it.key(), probeKeys ) &&
         !metadataKeyMatches( it.key(), externalDisplayKeys ) )
      result.insert( it.key(), it.value() );
  }
  return result;
}

QgsMtpl::PayloadType declaredPayload( const QVariantMap &metadata )
{
  const QString type = metadataValue( metadata,
    { QStringLiteral( "tile_file_ext" ), QStringLiteral( "tileFormat" ),
      QStringLiteral( "payload" ), QStringLiteral( "format" ), QStringLiteral( "type" ) } )
                         .toString().trimmed().toLower();
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

void applyElevationMetadata( QgsMtpl::PackageDescriptor &descriptor )
{
  bool ok = false;
  const double scale = metadataValue( descriptor.metadata, { QStringLiteral( "scale" ) } ).toDouble( &ok );
  if ( ok )
    descriptor.scale = scale;

  const double offset = metadataValue( descriptor.metadata, { QStringLiteral( "offset" ) } ).toDouble( &ok );
  if ( ok )
    descriptor.offset = offset;

  const double noData = metadataValue( descriptor.metadata,
    { QStringLiteral( "noData" ), QStringLiteral( "nodata" ), QStringLiteral( "no_data" ) } ).toDouble( &ok );
  if ( ok )
  {
    descriptor.hasNoData = true;
    descriptor.noData = noData;
  }
}

bool nearlyEqual( double left, double right )
{
  const double magnitude = std::max( { 1.0, std::fabs( left ), std::fabs( right ) } );
  return std::fabs( left - right ) <= magnitude * 1e-12;
}

bool matrixMatchesDatasetSnapshot( const QgsMtpl::TileMatrixDefinition &actual,
                                   const QgsMtpl::TileMatrixDefinition &expected,
                                   const QgsMtpl::TilePackageSourceContract *sourceContract )
{
  const QgsCoordinateReferenceSystem actualCrs( actual.crsAuthId );
  const QgsCoordinateReferenceSystem expectedCrs( expected.crsAuthId );
  if ( !actualCrs.isValid() || !expectedCrs.isValid() || actualCrs != expectedCrs ||
       actual.scheme != expected.scheme || actual.tileSize != expected.tileSize ||
       !nearlyEqual( actual.topLeftX, expected.topLeftX ) ||
       !nearlyEqual( actual.topLeftY, expected.topLeftY ) ||
       !nearlyEqual( actual.z0TileSpan, expected.z0TileSpan ) ||
       actual.z0MatrixWidth != expected.z0MatrixWidth ||
       actual.z0MatrixHeight != expected.z0MatrixHeight ||
       actual.scaleToZoomMethod.compare( expected.scaleToZoomMethod, Qt::CaseInsensitive ) != 0 )
    return false;

  if ( sourceContract && sourceContract->available )
  {
    if ( actual.zoomRangeExplicit != sourceContract->zoomRangeExplicit ||
         ( actual.zoomRangeExplicit &&
           ( actual.minimumZoom != sourceContract->minimumZoom ||
             actual.maximumZoom != sourceContract->maximumZoom ) ) ||
         actual.legacyFallbackApplied != sourceContract->legacyFallbackApplied ||
         actual.hasBounds != sourceContract->hasBounds )
      return false;
    if ( actual.hasBounds )
    {
      const QgsCoordinateReferenceSystem actualBoundsCrs( actual.boundsCrsAuthId );
      const QgsCoordinateReferenceSystem expectedBoundsCrs( sourceContract->boundsCrsAuthId );
      if ( !actualBoundsCrs.isValid() || !expectedBoundsCrs.isValid() ||
           actualBoundsCrs != expectedBoundsCrs ||
           !nearlyEqual( actual.boundsXMinimum, sourceContract->boundsXMinimum ) ||
           !nearlyEqual( actual.boundsYMinimum, sourceContract->boundsYMinimum ) ||
           !nearlyEqual( actual.boundsXMaximum, sourceContract->boundsXMaximum ) ||
           !nearlyEqual( actual.boundsYMaximum, sourceContract->boundsYMaximum ) )
        return false;
    }
  }
  else
  {
    if ( actual.zoomRangeExplicit &&
         ( !expected.zoomRangeExplicit || actual.minimumZoom != expected.minimumZoom ||
           actual.maximumZoom != expected.maximumZoom ) )
      return false;
    if ( actual.legacyFallbackApplied && !expected.legacyFallbackApplied )
      return false;
    if ( actual.hasBounds )
    {
      const QgsCoordinateReferenceSystem actualBoundsCrs( actual.boundsCrsAuthId );
      const QgsCoordinateReferenceSystem expectedBoundsCrs( expected.boundsCrsAuthId );
      if ( !expected.hasBounds || !actualBoundsCrs.isValid() || !expectedBoundsCrs.isValid() ||
           actualBoundsCrs != expectedBoundsCrs ||
           !nearlyEqual( actual.boundsXMinimum, expected.boundsXMinimum ) ||
           !nearlyEqual( actual.boundsYMinimum, expected.boundsYMinimum ) ||
           !nearlyEqual( actual.boundsXMaximum, expected.boundsXMaximum ) ||
           !nearlyEqual( actual.boundsYMaximum, expected.boundsYMaximum ) )
        return false;
    }
  }
  return true;
}

bool rangeRecordsEqual( const QgsMtpl::TileRangeRecord &left,
                        const QgsMtpl::TileRangeRecord &right )
{
  return left.zoom == right.zoom &&
         left.xMin == right.xMin && left.xMax == right.xMax &&
         left.yMin == right.yMin && left.yMax == right.yMax &&
         left.presentXMin == right.presentXMin && left.presentXMax == right.presentXMax &&
         left.presentYMin == right.presentYMin && left.presentYMax == right.presentYMax &&
         left.presentCount == right.presentCount;
}

struct RangeSnapshotKey
{
  int zoom = -1;
  quint32 xMin = 0;
  quint32 xMax = 0;
  quint32 yMin = 0;
  quint32 yMax = 0;
  quint32 presentXMin = 0;
  quint32 presentXMax = 0;
  quint32 presentYMin = 0;
  quint32 presentYMax = 0;
  quint64 presentCount = 0;

  bool operator==( const RangeSnapshotKey &other ) const
  {
    return zoom == other.zoom &&
           xMin == other.xMin && xMax == other.xMax &&
           yMin == other.yMin && yMax == other.yMax &&
           presentXMin == other.presentXMin && presentXMax == other.presentXMax &&
           presentYMin == other.presentYMin && presentYMax == other.presentYMax &&
           presentCount == other.presentCount;
  }
};

size_t qHash( const RangeSnapshotKey &key, size_t seed = 0 ) noexcept
{
  return qHashMulti( seed,
                     key.zoom,
                     key.xMin,
                     key.xMax,
                     key.yMin,
                     key.yMax,
                     key.presentXMin,
                     key.presentXMax,
                     key.presentYMin,
                     key.presentYMax,
                     key.presentCount );
}

RangeSnapshotKey rangeSnapshotKey( const QgsMtpl::TileRangeRecord &record )
{
  return RangeSnapshotKey {
    record.zoom,
    record.xMin,
    record.xMax,
    record.yMin,
    record.yMax,
    record.presentXMin,
    record.presentXMax,
    record.presentYMin,
    record.presentYMax,
    record.presentCount
  };
}

bool readActualRangeRecords( const TileReader &reader,
                             QgsRasterBlockFeedback *feedback,
                             QList<QgsMtpl::TileRangeRecord> &records,
                             int &observedMinimumZoom,
                             int &observedMaximumZoom,
                             QString &error,
                             bool &wasCanceled )
{
  records.clear();
  observedMinimumZoom = std::numeric_limits<int>::max();
  observedMaximumZoom = std::numeric_limits<int>::min();
  wasCanceled = false;

  size_t rangeCount = 0;
  mtpl_status_t status = reader.rangeCount( &rangeCount );
  if ( status != MTPL_STATUS_OK )
  {
    error = QObject::tr( "无法读取 PTP 范围数量（%1）。" ).arg( QgsMtpl::statusText( status ) );
    return false;
  }
  if ( rangeCount > static_cast<size_t>( std::numeric_limits<int>::max() ) )
  {
    error = QObject::tr( "PTP 范围数量超出 QGIS 支持范围。" );
    return false;
  }
  records.reserve( static_cast<qsizetype>( rangeCount ) );

  for ( size_t rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex )
  {
    if ( feedbackCanceled( feedback ) )
    {
      wasCanceled = true;
      return false;
    }

    mtpl_tile_range_info_t info = {};
    status = reader.rangeInfo( rangeIndex, &info );
    if ( status != MTPL_STATUS_OK )
    {
      error = QObject::tr( "无法读取 PTP 范围结构（%1）。" ).arg( QgsMtpl::statusText( status ) );
      return false;
    }
    if ( info.bounds.zoom > 30 || info.bounds.x_min > info.bounds.x_max ||
         info.bounds.y_min > info.bounds.y_max || info.present_count > info.slot_count )
    {
      error = QObject::tr( "PTP 范围结构包含无效边界或计数。" );
      return false;
    }

    const quint64 width = static_cast<quint64>( info.bounds.x_max ) - info.bounds.x_min + 1ULL;
    const quint64 height = static_cast<quint64>( info.bounds.y_max ) - info.bounds.y_min + 1ULL;
    if ( height != 0 && width > std::numeric_limits<quint64>::max() / height )
    {
      error = QObject::tr( "PTP 范围容量溢出。" );
      return false;
    }
    const quint64 capacity = width * height;
    if ( capacity != static_cast<quint64>( info.slot_count ) )
    {
      error = QObject::tr( "PTP 范围槽位数量与声明边界不一致。" );
      return false;
    }

    QgsMtpl::TileRangeRecord record;
    record.zoom = static_cast<int>( info.bounds.zoom );
    record.xMin = info.bounds.x_min;
    record.xMax = info.bounds.x_max;
    record.yMin = info.bounds.y_min;
    record.yMax = info.bounds.y_max;
    record.presentXMin = info.bounds.x_min;
    record.presentXMax = info.bounds.x_max;
    record.presentYMin = info.bounds.y_min;
    record.presentYMax = info.bounds.y_max;
    record.presentCount = static_cast<quint64>( info.present_count );

    if ( info.present_count > 0 && info.present_count < info.slot_count )
    {
      quint64 foundCount = 0;
      record.presentXMin = std::numeric_limits<quint32>::max();
      record.presentXMax = 0;
      record.presentYMin = std::numeric_limits<quint32>::max();
      record.presentYMax = 0;
      for ( size_t slotIndex = 0; slotIndex < info.slot_count; ++slotIndex )
      {
        if ( feedbackCanceled( feedback ) )
        {
          wasCanceled = true;
          return false;
        }
        mtpl_tile_entry_info_t entry = {};
        status = reader.entryInfo( rangeIndex, slotIndex, &entry );
        if ( status != MTPL_STATUS_OK )
        {
          error = QObject::tr( "无法读取 PTP 范围条目（%1）。" ).arg( QgsMtpl::statusText( status ) );
          return false;
        }
        if ( !entry.present )
          continue;
        if ( entry.coordinate.zoom != info.bounds.zoom ||
             entry.coordinate.x < info.bounds.x_min || entry.coordinate.x > info.bounds.x_max ||
             entry.coordinate.y < info.bounds.y_min || entry.coordinate.y > info.bounds.y_max )
        {
          error = QObject::tr( "PTP 范围包含越界瓦片条目。" );
          return false;
        }
        ++foundCount;
        record.presentXMin = std::min( record.presentXMin, entry.coordinate.x );
        record.presentXMax = std::max( record.presentXMax, entry.coordinate.x );
        record.presentYMin = std::min( record.presentYMin, entry.coordinate.y );
        record.presentYMax = std::max( record.presentYMax, entry.coordinate.y );
      }
      if ( foundCount != record.presentCount )
      {
        error = QObject::tr( "PTP 范围的实际瓦片数量与范围头不一致。" );
        return false;
      }
    }

    if ( record.presentCount > 0 )
    {
      observedMinimumZoom = std::min( observedMinimumZoom, record.zoom );
      observedMaximumZoom = std::max( observedMaximumZoom, record.zoom );
    }
    records.append( record );
  }

  if ( observedMinimumZoom == std::numeric_limits<int>::max() )
  {
    observedMinimumZoom = -1;
    observedMaximumZoom = -1;
  }
  return true;
}

bool rangeSnapshotsMatch( const QList<QgsMtpl::TileRangeRecord> &actual,
                          const QList<QgsMtpl::TileRangeRecord> &expected,
                          QgsRasterBlockFeedback *feedback,
                          bool &wasCanceled )
{
  wasCanceled = false;
  if ( feedbackCanceled( feedback ) )
  {
    wasCanceled = true;
    return false;
  }
  if ( actual.size() != expected.size() )
    return false;

  constexpr qsizetype cancellationCheckInterval = 1024;
  bool sameOrder = true;
  for ( qsizetype index = 0; index < actual.size(); ++index )
  {
    if ( index % cancellationCheckInterval == 0 && feedbackCanceled( feedback ) )
    {
      wasCanceled = true;
      return false;
    }
    if ( !rangeRecordsEqual( actual.at( index ), expected.at( index ) ) )
    {
      sameOrder = false;
      break;
    }
  }
  if ( sameOrder )
  {
    if ( feedbackCanceled( feedback ) )
    {
      wasCanceled = true;
      return false;
    }
    return true;
  }

  // Range order is not part of the dataset contract. Count exact keys so that
  // reordered snapshots compare in linear time while duplicate ranges retain
  // multiset semantics.
  QHash<RangeSnapshotKey, qsizetype> expectedCounts;
  expectedCounts.reserve( expected.size() );
  for ( qsizetype index = 0; index < expected.size(); ++index )
  {
    if ( index % cancellationCheckInterval == 0 && feedbackCanceled( feedback ) )
    {
      wasCanceled = true;
      return false;
    }
    const RangeSnapshotKey key = rangeSnapshotKey( expected.at( index ) );
    auto it = expectedCounts.find( key );
    if ( it == expectedCounts.end() )
      expectedCounts.insert( key, 1 );
    else
      ++it.value();
  }

  for ( qsizetype index = 0; index < actual.size(); ++index )
  {
    if ( index % cancellationCheckInterval == 0 && feedbackCanceled( feedback ) )
    {
      wasCanceled = true;
      return false;
    }
    auto it = expectedCounts.find( rangeSnapshotKey( actual.at( index ) ) );
    if ( it == expectedCounts.end() )
      return false;
    if ( it.value() == 1 )
      expectedCounts.erase( it );
    else
      --it.value();
  }

  if ( feedbackCanceled( feedback ) )
  {
    wasCanceled = true;
    return false;
  }
  return expectedCounts.isEmpty();
}

bool validateOpenedPtpSnapshot( const TileReader &reader,
                                const QgsMtpl::TileDatasetDescriptor &dataset,
                                int packageIndex,
                                QgsRasterBlockFeedback *feedback,
                                QString &error,
                                bool &wasCanceled )
{
  wasCanceled = false;
  if ( packageIndex < 0 || packageIndex >= dataset.packages.size() )
  {
    error = QObject::tr( "MTPL 数据集包含无效的数据包索引。" );
    return false;
  }
  const QgsMtpl::PackageDescriptor &expected = dataset.packages.at( packageIndex ).descriptor;

  uint32_t actualTileSize = 0;
  mtpl_storage_mode_t actualStorage = MTPL_STORAGE_PLAIN;
  mtpl_status_t status = reader.tileSize( &actualTileSize );
  if ( status == MTPL_STATUS_OK )
    status = reader.storageMode( &actualStorage );
  if ( status != MTPL_STATUS_OK || actualTileSize != static_cast<uint32_t>( expected.tileSize ) ||
       ( actualStorage == MTPL_STORAGE_PLAIN ) != ( expected.encryption == QgsMtpl::EncryptionState::Plain ) )
  {
    error = QObject::tr( "PTP 数据包头与加载时快照不一致。" );
    return false;
  }
  if ( feedbackCanceled( feedback ) )
  {
    wasCanceled = true;
    return false;
  }

  MtplBuffer metadataBuffer;
  status = reader.readMetadata( &metadataBuffer.buffer );
  if ( status != MTPL_STATUS_OK )
  {
    error = QObject::tr( "无法读取 PTP 元数据（%1）。" ).arg( QgsMtpl::statusText( status ) );
    return false;
  }
  const QVariantMap actualMetadata = decodedPtpMetadata( metadataBuffer );

  QList<QgsMtpl::TileRangeRecord> actualRanges;
  int observedMinimumZoom = -1;
  int observedMaximumZoom = -1;
  if ( !readActualRangeRecords( reader, feedback, actualRanges, observedMinimumZoom,
                                observedMaximumZoom, error, wasCanceled ) )
    return false;
  if ( !rangeSnapshotsMatch( actualRanges, dataset.packages.at( packageIndex ).ranges,
                             feedback, wasCanceled ) )
  {
    if ( wasCanceled )
      return false;
    error = QObject::tr( "PTP 数据包范围结构与加载时快照不一致。" );
    return false;
  }

  const bool originalMetadataAvailable = hasOriginalMetadataSnapshot( expected.metadata );
  if ( originalMetadataAvailable &&
       originalMetadata( actualMetadata ) != originalMetadata( expected.metadata ) )
  {
    error = QObject::tr( "PTP 元数据与加载时快照不一致。" );
    return false;
  }

  QgsMtpl::PackageDescriptor actualDescriptor;
  actualDescriptor.format = QgsMtpl::PackageFormat::Ptp;
  actualDescriptor.tileSize = static_cast<int>( actualTileSize );
  actualDescriptor.minimumZoom = observedMinimumZoom;
  actualDescriptor.maximumZoom = observedMaximumZoom;
  actualDescriptor.metadata = actualMetadata;
  actualDescriptor.payload = declaredPayload( actualMetadata );
  if ( actualDescriptor.payload == QgsMtpl::PayloadType::Elevation )
    applyElevationMetadata( actualDescriptor );
  actualDescriptor.displayOverridesApplied = expected.displayOverridesApplied;
  actualDescriptor.crsAuthId = expected.crsAuthId;
  actualDescriptor.scheme = expected.scheme;
  QgsMtpl::TileMatrixDefinition actualMatrix;
  QString matrixError;
  const QgsMtpl::TilePackageSourceContract &sourceContract = dataset.packages.at( packageIndex ).sourceContract;
  if ( !QgsMtpl::TileMatrixDefinition::fromPackageDescriptor( actualDescriptor, actualMatrix, &matrixError ) ||
       !matrixMatchesDatasetSnapshot(
         actualMatrix,
         dataset.matrix,
         sourceContract.available ? &sourceContract : nullptr ) )
  {
    error = QObject::tr( "PTP 瓦片矩阵与加载时快照不一致：%1" ).arg( matrixError );
    return false;
  }

  static const QStringList mapIdNames { QStringLiteral( "map_id" ), QStringLiteral( "mapId" ) };
  static const QStringList payloadNames {
    QStringLiteral( "tile_file_ext" ), QStringLiteral( "tileFormat" ),
    QStringLiteral( "payload" ), QStringLiteral( "format" ), QStringLiteral( "type" )
  };
  const bool actualMapIdDeclared = metadataContains( actualMetadata, mapIdNames );
  const QString actualMapId = metadataValue( actualMetadata, mapIdNames ).toString().trimmed();
  if ( sourceContract.available &&
       ( actualMapIdDeclared != sourceContract.mapIdDeclared ||
         ( actualMapIdDeclared && actualMapId != sourceContract.mapId ) ) )
  {
    error = QObject::tr( "PTP map_id 的声明状态或值与加载时快照不一致。" );
    return false;
  }
  if ( !sourceContract.available && !actualMapId.isEmpty() && actualMapId != dataset.mapId )
  {
    error = QObject::tr( "PTP map_id 与加载时快照不一致。" );
    return false;
  }

  const QgsMtpl::PayloadType metadataPayload = actualDescriptor.payload;
  const bool actualPayloadDeclared = metadataContains( actualMetadata, payloadNames );
  if ( sourceContract.available &&
       ( actualPayloadDeclared != sourceContract.payloadDeclared ||
         ( actualPayloadDeclared && metadataPayload != sourceContract.payload ) ) )
  {
    error = QObject::tr( "PTP 载荷字段的声明状态或类型与加载时快照不一致。" );
    return false;
  }
  if ( !sourceContract.available && !expected.displayOverridesApplied &&
       metadataPayload != QgsMtpl::PayloadType::Unknown && metadataPayload != dataset.payload )
  {
    error = QObject::tr( "PTP 载荷类型与加载时快照不一致。" );
    return false;
  }
  if ( !originalMetadataAvailable && !expected.displayOverridesApplied &&
       expected.payload == QgsMtpl::PayloadType::Elevation &&
       ( metadataPayload != QgsMtpl::PayloadType::Elevation ||
         !nearlyEqual( actualDescriptor.scale, expected.scale ) ||
         !nearlyEqual( actualDescriptor.offset, expected.offset ) ||
         actualDescriptor.hasNoData != expected.hasNoData ||
         ( expected.hasNoData && !nearlyEqual( actualDescriptor.noData, expected.noData ) ) ||
         elevationDataType( actualDescriptor ) != elevationDataType( expected ) ||
         elevationEndianness( actualDescriptor ) != elevationEndianness( expected ) ) )
  {
    error = QObject::tr( "PTP 高程解码参数与加载时快照不一致。" );
    return false;
  }
  return true;
}

QString datasetStructureSummary( const QgsMtpl::TileDatasetDescriptor &dataset, int packageIndex )
{
  if ( packageIndex < 0 || packageIndex >= dataset.packages.size() )
    return QString();

  const QgsMtpl::TileDatasetPackage &package = dataset.packages.at( packageIndex );
  const QgsMtpl::PackageDescriptor &descriptor = package.descriptor;
  QByteArray material;
  QDataStream stream( &material, QIODevice::WriteOnly );
  stream.setVersion( QDataStream::Qt_6_0 );
  stream << static_cast<qint32>( descriptor.format )
         << static_cast<qint32>( descriptor.payload )
         << static_cast<qint32>( descriptor.encryption )
         << descriptor.tileSize
         << descriptor.minimumZoom
         << descriptor.maximumZoom
         << descriptor.crsAuthId
         << descriptor.scheme
         << descriptor.displayOverridesApplied
         << descriptor.scale
         << descriptor.offset
         << descriptor.hasNoData
         << descriptor.noData
         << dataset.mapId
         << static_cast<qint32>( dataset.matrix.scheme )
         << dataset.matrix.tileSize
         << dataset.matrix.topLeftX
         << dataset.matrix.topLeftY
         << dataset.matrix.z0TileSpan
         << dataset.matrix.z0MatrixWidth
         << dataset.matrix.z0MatrixHeight
         << dataset.matrix.scaleToZoomMethod
         << dataset.matrix.minimumZoom
         << dataset.matrix.maximumZoom
         << dataset.matrix.hasBounds
         << dataset.matrix.boundsXMinimum
         << dataset.matrix.boundsYMinimum
         << dataset.matrix.boundsXMaximum
         << dataset.matrix.boundsYMaximum
         << dataset.matrix.boundsCrsAuthId
         << package.address.minZoom
         << package.address.maxZoom
         << package.address.baseZoom
         << package.address.packageX
         << package.address.packageY
         << package.sourceContract.available
         << package.sourceContract.zoomRangeExplicit
         << package.sourceContract.minimumZoom
         << package.sourceContract.maximumZoom
         << package.sourceContract.hasBounds
         << package.sourceContract.boundsXMinimum
         << package.sourceContract.boundsYMinimum
         << package.sourceContract.boundsXMaximum
         << package.sourceContract.boundsYMaximum
         << package.sourceContract.boundsCrsAuthId
         << package.sourceContract.legacyFallbackApplied
         << package.sourceContract.mapIdDeclared
         << package.sourceContract.mapId
         << package.sourceContract.payloadDeclared
         << static_cast<qint32>( package.sourceContract.payload )
         << hasOriginalMetadataSnapshot( descriptor.metadata )
         << originalMetadata( descriptor.metadata );
  for ( const QgsMtpl::TileRangeRecord &range : package.ranges )
  {
    stream << range.zoom
           << range.xMin << range.xMax << range.yMin << range.yMax
           << range.presentXMin << range.presentXMax
           << range.presentYMin << range.presentYMax
           << range.presentCount;
  }
  return QString::fromLatin1(
    QCryptographicHash::hash( material, QCryptographicHash::Sha256 ).toHex() );
}

} // namespace

QString QgsMtplStructureValidationCache::structureSummary( const QgsMtpl::TileDatasetDescriptor &dataset,
                                                            int packageIndex )
{
  // Keep producers and consumers of cache identities on one serialization.
  return datasetStructureSummary( dataset, packageIndex );
}

class QgsMtplRasterInterface::Private
{
  public:
    struct FileStamp
    {
      quint64 size = 0;
      qint64 lastModifiedMs = -1;

      bool operator==( const FileStamp &other ) const
      {
        return size == other.size && lastModifiedMs == other.lastModifiedMs;
      }

      bool operator!=( const FileStamp &other ) const
      {
        return !( *this == other );
      }
    };

    struct ReaderState
    {
      bool attempted = false;
      bool snapshotValidated = false;
      bool hasFileStamp = false;
      FileStamp fileStamp;
      mtpl_status_t openStatus = MTPL_STATUS_OK;
      std::unique_ptr<TileReader> reader;
    };

    struct TileRenderRequest
    {
      int column = 0;
      int row = 0;
      QgsMtpl::ResolvedTile resolved;
    };

    Private( const QgsMtpl::TileDatasetDescriptor &datasetSnapshot,
             const QgsTileMatrixSet &matrixSetSnapshot,
             const QList<QgsMtpl::CryptoKeys> &keys,
             int requestedZoom,
             const std::shared_ptr<QgsMtplStructureValidationCache> &sharedValidationCache )
      : dataset( datasetSnapshot )
      , matrixSet( matrixSetSnapshot )
      , packageKeys( keys )
      , zoomLevel( requestedZoom )
      , validationCache( sharedValidationCache ? sharedValidationCache : std::make_shared<QgsMtplStructureValidationCache>() )
    {
      decodedTiles.setMaxCost( 32 * 1024 );
      packageStructureSummaries.reserve( dataset.packages.size() );
      for ( int packageIndex = 0; packageIndex < dataset.packages.size(); ++packageIndex )
        packageStructureSummaries.append( QgsMtplStructureValidationCache::structureSummary( dataset, packageIndex ) );
      ptpOnly = !dataset.packages.isEmpty() && std::all_of(
        dataset.packages.cbegin(), dataset.packages.cend(), []( const QgsMtpl::TileDatasetPackage &package )
        {
          return package.descriptor.format == QgsMtpl::PackageFormat::Ptp;
        } );
    }

    ~Private()
    {
      for ( QgsMtpl::CryptoKeys &keys : packageKeys )
        keys.clear();
    }

    void reportError( const QString &key, const QString &message )
    {
      constexpr int maximumErrors = 16;
      if ( message.isEmpty() || reportedErrorKeys.contains( key ) || errorMessages.size() >= maximumErrors )
        return;
      reportedErrorKeys.insert( key );
      errorMessages.append( ptpOnly ? QObject::tr( "图层已降级：%1" ).arg( message ) : message );
    }

    void closeReader( int packageIndex )
    {
      const auto it = readers.find( packageIndex );
      if ( it != readers.end() )
        it->second.reader.reset();
      openReaderLru.removeAll( packageIndex );
    }

    void touchOpenReader( int packageIndex )
    {
      openReaderLru.removeAll( packageIndex );
      openReaderLru.append( packageIndex );
    }

    void makeRoomForReader( int packageIndex )
    {
      openReaderLru.removeAll( packageIndex );
      while ( openReaderLru.size() >= maximumOpenReaders )
      {
        const int leastRecentlyUsed = openReaderLru.takeFirst();
        const auto it = readers.find( leastRecentlyUsed );
        if ( it != readers.end() )
          it->second.reader.reset();
      }
    }

    QString packageIdentity( int packageIndex ) const
    {
      return QgsMtplStructureValidationCache::packageIdentity(
        packageIndex,
        dataset.packages.at( packageIndex ).descriptor.path,
        packageIndex < packageStructureSummaries.size() ? packageStructureSummaries.at( packageIndex ) : QString() );
    }

    bool updateFileStamp( int packageIndex, ReaderState &state, FileStamp &stamp )
    {
      const QgsMtpl::TileDatasetPackage &package = dataset.packages.at( packageIndex );
      const QgsMtpl::PackageDescriptor &descriptor = package.descriptor;
      const QFileInfo fileInfo( descriptor.path );
      if ( !fileInfo.exists() )
      {
        validationCache->invalidatePackage( packageIdentity( packageIndex ) );
        state.attempted = true;
        state.snapshotValidated = false;
        state.hasFileStamp = false;
        state.openStatus = MTPL_STATUS_FILE_NOT_FOUND;
        closeReader( packageIndex );
        decodedTiles.clear();
        // The resolver handles a theoretical package address which was never
        // present in the dataset as a transparent tile. Reaching this branch
        // means that a package recorded in the validated snapshot disappeared.
        reportError( QStringLiteral( "missing-file-%1" ).arg( packageIndex ),
                     QObject::tr( "MTPL 数据包 %1 已缺失，已透明显示。" ).arg( descriptor.displayName ) );
        return false;
      }

      stamp.size = static_cast<quint64>( fileInfo.size() );
      stamp.lastModifiedMs = fileInfo.lastModified().toMSecsSinceEpoch();
      if ( state.hasFileStamp && state.fileStamp != stamp )
      {
        closeReader( packageIndex );
        decodedTiles.clear();
        state.attempted = false;
        state.snapshotValidated = false;
        state.openStatus = MTPL_STATUS_OK;
      }
      else if ( !state.hasFileStamp && state.openStatus == MTPL_STATUS_FILE_NOT_FOUND )
      {
        state.attempted = false;
        state.openStatus = MTPL_STATUS_OK;
      }
      state.fileStamp = stamp;
      state.hasFileStamp = true;
      return true;
    }

    TileReader *readerForPackage( int packageIndex, QgsRasterBlockFeedback *feedback )
    {
      if ( packageIndex < 0 || packageIndex >= dataset.packages.size() )
      {
        reportError( QStringLiteral( "package-index" ), QObject::tr( "MTPL 数据集包含无效的数据包索引。" ) );
        return nullptr;
      }
      if ( feedbackCanceled( feedback ) )
        return nullptr;

      ReaderState &state = readers[packageIndex];
      FileStamp fileStamp;
      if ( !updateFileStamp( packageIndex, state, fileStamp ) )
        return nullptr;
      if ( state.attempted && state.openStatus != MTPL_STATUS_OK )
        return nullptr;
      if ( state.reader )
      {
        touchOpenReader( packageIndex );
        return state.reader.get();
      }

      const QgsMtpl::TileDatasetPackage &package = dataset.packages.at( packageIndex );
      const QgsMtpl::PackageDescriptor &descriptor = package.descriptor;
      const QgsMtpl::CryptoKeys keys = packageIndex < packageKeys.size() ? packageKeys.at( packageIndex ) : QgsMtpl::CryptoKeys();
      state.attempted = true;
      if ( descriptor.encryption != QgsMtpl::EncryptionState::Plain && !keys.isValid() )
      {
        state.openStatus = MTPL_STATUS_KEY_REQUIRED;
        reportError( QStringLiteral( "open-key-%1" ).arg( packageIndex ),
                     QObject::tr( "MTPL 数据包 %1 需要匹配的密钥。" ).arg( descriptor.displayName ) );
        return nullptr;
      }
      makeRoomForReader( packageIndex );
      state.reader = std::make_unique<TileReader>( descriptor.format );
      state.openStatus = state.reader->open( descriptor.path, keys );
      if ( state.openStatus == MTPL_STATUS_KEY_REQUIRED || state.openStatus == MTPL_STATUS_CRYPTO_ERROR )
      {
        reportError( QStringLiteral( "open-key-%1" ).arg( packageIndex ),
                     QObject::tr( "无法解密 MTPL 数据包 %1。密钥可能不匹配，或数据包已损坏。" ).arg( descriptor.displayName ) );
      }
      else if ( state.openStatus != MTPL_STATUS_OK )
      {
        reportError( QStringLiteral( "open-%1" ).arg( packageIndex ),
                     QObject::tr( "无法打开 MTPL 数据包 %1（%2）。" ).arg( descriptor.displayName, QgsMtpl::statusText( state.openStatus ) ) );
      }
      if ( state.openStatus != MTPL_STATUS_OK )
        state.reader.reset();
      else if ( feedbackCanceled( feedback ) )
      {
        // Cancellation belongs to this block request, not to the package.
        // Leave the state retryable for a later block with fresh feedback.
        state.attempted = false;
        state.openStatus = MTPL_STATUS_OK;
        state.reader.reset();
      }
      else if ( !state.snapshotValidated )
      {
        QString validationError;
        bool validationCanceled = false;
        const bool snapshotValid = validationCache->validate(
          packageIdentity( packageIndex ),
          fileStamp.size,
          fileStamp.lastModifiedMs,
          feedback,
          [this, packageIndex, feedback, &state]( QString &error, bool &wasCanceled )
          {
            return validateOpenedPtpSnapshot( *state.reader, dataset, packageIndex, feedback, error, wasCanceled );
          },
          validationError,
          validationCanceled );
        if ( !snapshotValid )
        {
          state.reader.reset();
          if ( validationCanceled )
          {
            state.attempted = false;
            state.snapshotValidated = false;
            state.openStatus = MTPL_STATUS_OK;
          }
          else
          {
            state.openStatus = MTPL_STATUS_CORRUPT_DATA;
            reportError( QStringLiteral( "snapshot-%1" ).arg( packageIndex ),
                         QObject::tr( "MTPL 数据包 %1 已发生结构变化，已跳过读取：%2" )
                           .arg( descriptor.displayName, validationError ) );
          }
        }
        else
        {
          state.snapshotValidated = true;
        }
      }
      if ( state.reader )
        touchOpenReader( packageIndex );
      return state.reader.get();
    }

    bool resolveTile( int zoom,
                      int column,
                      int row,
                      QgsMtpl::ResolvedTile &resolved,
                      QgsRasterBlockFeedback *feedback )
    {
      if ( feedbackCanceled( feedback ) )
        return false;

      QString resolveError;
      if ( !QgsMtpl::TileResolver::resolve( dataset,
                                            zoom,
                                            static_cast<quint64>( column ),
                                            static_cast<quint64>( row ),
                                            resolved,
                                            &resolveError ) ||
           !resolved.isValid() )
      {
        if ( resolveError.isEmpty() )
          missingTilesEncountered = true;
        reportError( QStringLiteral( "resolve" ), resolveError );
        return false;
      }

      if ( resolved.packageIndex < 0 || resolved.packageIndex >= dataset.packages.size() )
      {
        reportError( QStringLiteral( "resolved-package-index" ), QObject::tr( "MTPL 瓦片解析到了无效的数据包索引。" ) );
        return false;
      }
      return true;
    }

    bool readerIsOpen( int packageIndex, const TileReader *reader ) const
    {
      const auto it = readers.find( packageIndex );
      return it != readers.end() && it->second.reader.get() == reader;
    }

    QImage imageForResolvedTile( int zoom,
                                 const QgsMtpl::ResolvedTile &resolved,
                                 TileReader &reader,
                                 QgsRasterBlockFeedback *feedback )
    {
      if ( feedbackCanceled( feedback ) )
        return QImage();

      const QString cacheKey = QStringLiteral( "%1/%2/%3/%4" )
                                 .arg( resolved.packageIndex )
                                 .arg( zoom )
                                 .arg( resolved.storageX )
                                 .arg( resolved.storageY );
      if ( const QImage *cached = decodedTiles.object( cacheKey ) )
        return *cached;

      const mtpl_tile_coordinate_t coordinate = {
        static_cast<uint32_t>( zoom ),
        resolved.storageX,
        resolved.storageY
      };
      MtplBuffer tile;
      const mtpl_status_t status = reader.readTile( coordinate, &tile.buffer );
      if ( feedbackCanceled( feedback ) )
        return QImage();
      if ( status == MTPL_STATUS_NOT_FOUND || status == MTPL_STATUS_OUT_OF_RANGE )
      {
        missingTilesEncountered = true;
        return QImage();
      }
      if ( status != MTPL_STATUS_OK )
      {
        ReaderState &state = readers[resolved.packageIndex];
        if ( status == MTPL_STATUS_KEY_REQUIRED || status == MTPL_STATUS_CRYPTO_ERROR )
        {
          state.openStatus = status;
          closeReader( resolved.packageIndex );
          reportError( QStringLiteral( "read-key-%1" ).arg( resolved.packageIndex ),
                       QObject::tr( "无法解密 MTPL 数据包 %1。密钥可能不匹配，或数据包已损坏。" )
                         .arg( dataset.packages.at( resolved.packageIndex ).descriptor.displayName ) );
        }
        else
        {
          reportError( QStringLiteral( "read-%1" ).arg( resolved.packageIndex ),
                       QObject::tr( "无法读取 MTPL 数据包 %1 中的瓦片（%2）。" )
                         .arg( dataset.packages.at( resolved.packageIndex ).descriptor.displayName, QgsMtpl::statusText( status ) ) );
        }
        return QImage();
      }

      const QByteArray bytes = tile.toByteArray();
      if ( bytes.isEmpty() )
      {
        if ( dataset.packages.at( resolved.packageIndex ).descriptor.format == QgsMtpl::PackageFormat::Ptp )
        {
          reportError( QStringLiteral( "decode-format-%1" ).arg( resolved.packageIndex ),
                       QObject::tr( "MTPL 数据包 %1 中的 PTP 瓦片为空。PTP 仅支持 PNG、JPEG 或 WebP 影像瓦片。" )
                         .arg( dataset.packages.at( resolved.packageIndex ).descriptor.displayName ) );
        }
        return QImage();
      }
      if ( feedbackCanceled( feedback ) )
        return QImage();

      const QgsMtpl::PackageDescriptor &descriptor = dataset.packages.at( resolved.packageIndex ).descriptor;
      QImage image;
      if ( descriptor.format == QgsMtpl::PackageFormat::Dtp || descriptor.payload == QgsMtpl::PayloadType::Elevation )
        image = elevationImage( bytes, descriptor );
      else if ( descriptor.format != QgsMtpl::PackageFormat::Vtp && descriptor.payload != QgsMtpl::PayloadType::VectorTile )
      {
        QByteArray imageFormat;
        if ( descriptor.format == QgsMtpl::PackageFormat::Ptp )
        {
          imageFormat = QgsMtplPackageService::ptpImageFormatFromMagic( bytes );
          if ( imageFormat.isEmpty() )
          {
            reportError( QStringLiteral( "decode-format-%1" ).arg( resolved.packageIndex ),
                         QObject::tr( "MTPL 数据包 %1 中的 PTP 瓦片格式不受支持。PTP 仅支持 PNG、JPEG 或 WebP 影像瓦片。" )
                           .arg( descriptor.displayName ) );
            return QImage();
          }
          if ( !QgsMtplPackageService::ptpImageHasCompleteStructure( bytes ) )
          {
            reportError( QStringLiteral( "decode-structure-%1" ).arg( resolved.packageIndex ),
                         QObject::tr( "MTPL 数据包 %1 中的 PTP 影像瓦片文件不完整或结构损坏。" )
                           .arg( descriptor.displayName ) );
            return QImage();
          }
        }
        QBuffer encodedBuffer;
        encodedBuffer.setData( bytes );
        encodedBuffer.open( QIODevice::ReadOnly );
        QImageReader imageReader( &encodedBuffer, imageFormat );
        const QSize encodedSize = imageReader.size();
        if ( !encodedSize.isValid() || encodedSize.width() != descriptor.tileSize || encodedSize.height() != descriptor.tileSize )
        {
          reportError( QStringLiteral( "decode-size-%1" ).arg( resolved.packageIndex ),
                       QObject::tr( "MTPL 数据包 %1 中的影像尺寸与包头瓦片尺寸不一致。" ).arg( descriptor.displayName ) );
          return QImage();
        }
        image = imageReader.read();
      }
      if ( feedbackCanceled( feedback ) )
        return QImage();

      if ( image.isNull() )
      {
        reportError( QStringLiteral( "decode-%1" ).arg( resolved.packageIndex ),
                     QObject::tr( "无法将 MTPL 数据包 %1 中的瓦片解码为栅格影像。" ).arg( descriptor.displayName ) );
        return QImage();
      }

      image = image.convertToFormat( QImage::Format_ARGB32_Premultiplied );
      const int cost = std::max( 1, static_cast<int>( std::min<qsizetype>( image.sizeInBytes() / 1024, std::numeric_limits<int>::max() ) ) );
      decodedTiles.insert( cacheKey, new QImage( image ), cost );
      return image;
    }

    QgsMtpl::TileDatasetDescriptor dataset;
    QgsTileMatrixSet matrixSet;
    QList<QgsMtpl::CryptoKeys> packageKeys;
    int zoomLevel = -1;
    static constexpr qsizetype maximumOpenReaders = 64;
    std::map<int, ReaderState> readers;
    QList<int> openReaderLru;
    QCache<QString, QImage> decodedTiles;
    QSet<QString> reportedErrorKeys;
    QStringList errorMessages;
    QStringList packageStructureSummaries;
    bool missingTilesEncountered = false;
    bool ptpOnly = false;
    std::shared_ptr<QgsMtplStructureValidationCache> validationCache;
};

QgsMtplRasterInterface::QgsMtplRasterInterface( const QgsMtpl::TileDatasetDescriptor &dataset,
                                                 const QgsTileMatrixSet &matrixSet,
                                                 const QList<QgsMtpl::CryptoKeys> &packageKeys,
                                                 int zoomLevel,
                                                 const std::shared_ptr<QgsMtplStructureValidationCache> &validationCache )
  : QgsRasterInterface( nullptr )
  , d( std::make_unique<Private>( dataset, matrixSet, packageKeys, zoomLevel, validationCache ) )
{}

QgsMtplRasterInterface::~QgsMtplRasterInterface() = default;

QgsMtplRasterInterface *QgsMtplRasterInterface::clone() const
{
  return new QgsMtplRasterInterface( d->dataset, d->matrixSet, d->packageKeys, d->zoomLevel, d->validationCache );
}

Qgis::DataType QgsMtplRasterInterface::dataType( int bandNo ) const
{
  return bandNo == 1 ? Qgis::DataType::ARGB32_Premultiplied : Qgis::DataType::UnknownDataType;
}

Qgis::DataType QgsMtplRasterInterface::sourceDataType( int bandNo ) const
{
  return dataType( bandNo );
}

int QgsMtplRasterInterface::bandCount() const
{
  return 1;
}

QgsRectangle QgsMtplRasterInterface::extent() const
{
  if ( d->dataset.hasExtent )
  {
    return QgsRectangle( d->dataset.extentXMinimum,
                         d->dataset.extentYMinimum,
                         d->dataset.extentXMaximum,
                         d->dataset.extentYMaximum );
  }
  if ( !d->matrixSet.isEmpty() )
    return d->matrixSet.rootMatrix().extent();
  return QgsRectangle();
}

int QgsMtplRasterInterface::xBlockSize() const
{
  return std::max( 1, d->dataset.matrix.tileSize );
}

int QgsMtplRasterInterface::yBlockSize() const
{
  return xBlockSize();
}

int QgsMtplRasterInterface::xSize() const
{
  bool ok = false;
  const quint64 count = d->dataset.matrix.matrixWidth( std::max( 0, d->zoomLevel ), &ok );
  return ok ? boundedRasterSize( count, d->dataset.matrix.tileSize ) : 0;
}

int QgsMtplRasterInterface::ySize() const
{
  bool ok = false;
  const quint64 count = d->dataset.matrix.matrixHeight( std::max( 0, d->zoomLevel ), &ok );
  return ok ? boundedRasterSize( count, d->dataset.matrix.tileSize ) : 0;
}

QgsRasterBlock *QgsMtplRasterInterface::block( int bandNo,
                                               const QgsRectangle &requestedExtent,
                                               int width,
                                               int height,
                                               QgsRasterBlockFeedback *feedback )
{
  if ( bandNo != 1 || width <= 0 || height <= 0 || requestedExtent.isEmpty() || d->zoomLevel < 0 )
    return new QgsRasterBlock();

  QImage output( width, height, QImage::Format_ARGB32_Premultiplied );
  output.fill( Qt::transparent );
  if ( feedback && feedback->isCanceled() )
  {
    auto block = std::make_unique<QgsRasterBlock>();
    block->setImage( &output );
    return block.release();
  }

  const QgsTileMatrix matrix = d->matrixSet.tileMatrix( d->zoomLevel );
  if ( matrix.zoomLevel() != d->zoomLevel || matrix.matrixWidth() <= 0 || matrix.matrixHeight() <= 0 )
  {
    d->reportError( QStringLiteral( "matrix" ), QObject::tr( "MTPL 数据集缺少缩放级别 %1 的瓦片矩阵。" ).arg( d->zoomLevel ) );
    auto block = std::make_unique<QgsRasterBlock>();
    block->setImage( &output );
    return block.release();
  }

  const QgsTileRange range = matrix.tileRangeFromExtent( requestedExtent );
  if ( !range.isValid() )
  {
    auto block = std::make_unique<QgsRasterBlock>();
    block->setImage( &output );
    return block.release();
  }

  const qint64 rangeWidth = static_cast<qint64>( range.endColumn() ) - range.startColumn() + 1;
  const qint64 rangeHeight = static_cast<qint64>( range.endRow() ) - range.startRow() + 1;
  constexpr qint64 maximumTilesPerRequest = 65536;
  if ( rangeWidth <= 0 || rangeHeight <= 0 ||
       rangeWidth > maximumTilesPerRequest || rangeHeight > maximumTilesPerRequest ||
       rangeWidth > maximumTilesPerRequest / rangeHeight )
  {
    d->reportError( QStringLiteral( "request-tile-limit" ),
                    QObject::tr( "请求的瓦片数量过大，已跳过本次绘制。请放大后重试。" ) );
    auto block = std::make_unique<QgsRasterBlock>();
    block->setImage( &output );
    return block.release();
  }

  // Resolve first so all tiles from the same package are read together. This
  // keeps each package to one open operation per block request and prevents a
  // wide, multi-row view from repeatedly cycling through the bounded LRU.
  std::map<int, QVector<Private::TileRenderRequest>> tileRequestsByPackage;
  for ( qint64 row = range.startRow(); row <= range.endRow(); ++row )
  {
    for ( qint64 column = range.startColumn(); column <= range.endColumn(); ++column )
    {
      if ( feedbackCanceled( feedback ) )
        break;

      QgsMtpl::ResolvedTile resolved;
      if ( !d->resolveTile( d->zoomLevel,
                            static_cast<int>( column ),
                            static_cast<int>( row ),
                            resolved,
                            feedback ) )
        continue;
      tileRequestsByPackage[resolved.packageIndex].append(
        Private::TileRenderRequest { static_cast<int>( column ), static_cast<int>( row ), resolved } );
    }
    if ( feedbackCanceled( feedback ) )
      break;
  }

  QPainter painter( &output );
  painter.setCompositionMode( QPainter::CompositionMode_SourceOver );
  painter.setRenderHint( QPainter::SmoothPixmapTransform, true );

  const double xScale = static_cast<double>( width ) / requestedExtent.width();
  const double yScale = static_cast<double>( height ) / requestedExtent.height();
  for ( const auto &packageRequests : std::as_const( tileRequestsByPackage ) )
  {
    if ( feedbackCanceled( feedback ) )
      break;

    const int packageIndex = packageRequests.first;
    TileReader *reader = d->readerForPackage( packageIndex, feedback );
    if ( !reader )
      continue;
    for ( const Private::TileRenderRequest &request : packageRequests.second )
    {
      if ( feedbackCanceled( feedback ) || !d->readerIsOpen( packageIndex, reader ) )
        break;

      const QImage tile = d->imageForResolvedTile( d->zoomLevel, request.resolved, *reader, feedback );
      if ( tile.isNull() )
        continue;

      const QgsRectangle tileExtent = matrix.tileExtent( QgsTileXYZ( request.column, request.row, d->zoomLevel ) );
      const QRectF target( ( tileExtent.xMinimum() - requestedExtent.xMinimum() ) * xScale,
                           ( requestedExtent.yMaximum() - tileExtent.yMaximum() ) * yScale,
                           tileExtent.width() * xScale,
                           tileExtent.height() * yScale );
      painter.drawImage( target, tile );
    }
  }
  painter.end();
  if ( d->missingTilesEncountered && !d->ptpOnly )
  {
    d->reportError( QStringLiteral( "missing-tiles" ),
                    QObject::tr( "当前 MTPL 视图包含缺失数据包或未写入瓦片，已透明显示。" ) );
  }

  auto block = std::make_unique<QgsRasterBlock>();
  block->setImage( &output );
  return block.release();
}

QStringList QgsMtplRasterInterface::errors() const
{
  return d->errorMessages;
}

QgsMtplRasterSourceProvider::QgsMtplRasterSourceProvider( const QgsMtpl::TileDatasetDescriptor &dataset,
                                                         const QgsTileMatrixSet &matrixSet,
                                                         const QList<QgsMtpl::CryptoKeys> &packageKeys,
                                                         int zoomLevel,
                                                         const std::shared_ptr<QgsMtplStructureValidationCache> &validationCache )
  : QgsMtplRasterSourceProvider( std::make_unique<QgsMtplRasterInterface>( dataset, matrixSet, packageKeys, zoomLevel, validationCache ),
                                 matrixSet.crs() )
{}

QgsMtplRasterSourceProvider::QgsMtplRasterSourceProvider( std::unique_ptr<QgsMtplRasterInterface> source,
                                                         const QgsCoordinateReferenceSystem &crs )
  : QgsRasterDataProvider()
  , mSource( std::move( source ) )
  , mCrs( crs )
{}

QgsMtplRasterSourceProvider::~QgsMtplRasterSourceProvider() = default;

QgsMtplRasterSourceProvider *QgsMtplRasterSourceProvider::clone() const
{
  auto *provider = new QgsMtplRasterSourceProvider( std::unique_ptr<QgsMtplRasterInterface>( mSource->clone() ), mCrs );
  provider->copyBaseSettings( *this );
  return provider;
}

Qgis::RasterInterfaceCapabilities QgsMtplRasterSourceProvider::capabilities() const
{
  // The extent can cover only part of the matrix, while xSize()/ySize() describe
  // the entire matrix. Advertising Size would make the projector infer an
  // incorrect native resolution from these deliberately different domains.
  return Qgis::RasterInterfaceCapability::NoCapabilities;
}

QgsCoordinateReferenceSystem QgsMtplRasterSourceProvider::crs() const
{
  return mCrs;
}

bool QgsMtplRasterSourceProvider::isValid() const
{
  const QgsRectangle sourceExtent = extent();
  return mCrs.isValid() && sourceExtent.isFinite() && !sourceExtent.isEmpty();
}

QString QgsMtplRasterSourceProvider::name() const
{
  return QStringLiteral( "mtpl_raster_source" );
}

QString QgsMtplRasterSourceProvider::description() const
{
  return QObject::tr( "MTPL 栅格数据源" );
}

QString QgsMtplRasterSourceProvider::lastErrorTitle()
{
  return errors().isEmpty() ? QString() : description();
}

QString QgsMtplRasterSourceProvider::lastError()
{
  return errors().join( QLatin1Char( '\n' ) );
}

Qgis::DataType QgsMtplRasterSourceProvider::dataType( int bandNo ) const
{
  return mSource->dataType( bandNo );
}

Qgis::DataType QgsMtplRasterSourceProvider::sourceDataType( int bandNo ) const
{
  return mSource->sourceDataType( bandNo );
}

int QgsMtplRasterSourceProvider::bandCount() const
{
  return mSource->bandCount();
}

QgsRectangle QgsMtplRasterSourceProvider::extent() const
{
  return mSource->extent();
}

int QgsMtplRasterSourceProvider::xBlockSize() const
{
  return mSource->xBlockSize();
}

int QgsMtplRasterSourceProvider::yBlockSize() const
{
  return mSource->yBlockSize();
}

int QgsMtplRasterSourceProvider::xSize() const
{
  return mSource->xSize();
}

int QgsMtplRasterSourceProvider::ySize() const
{
  return mSource->ySize();
}

QgsRasterBlock *QgsMtplRasterSourceProvider::block( int bandNo,
                                                  const QgsRectangle &requestedExtent,
                                                  int width,
                                                  int height,
                                                  QgsRasterBlockFeedback *feedback )
{
  return mSource->block( bandNo, requestedExtent, width, height, feedback );
}

QStringList QgsMtplRasterSourceProvider::errors() const
{
  return mSource->errors();
}
