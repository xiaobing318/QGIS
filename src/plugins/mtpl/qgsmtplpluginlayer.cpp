/***************************************************************************
  qgsmtplpluginlayer.cpp
  ----------------------
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

#include "qgsmtplpluginlayer.h"

#include "qgsmtplkeysidecar.h"
#include "qgsmtplstatus.h"

#include "qgis.h"
#include "qgsapplication.h"
#include "qgscoordinatereferencesystem.h"
#include "qgscoordinatetransform.h"
#include "qgsexception.h"
#include "qgsmaplayerrenderer.h"
#include "qgsmapboxglstyleconverter.h"
#include "qgsmaptopixel.h"
#include "qgspathresolver.h"
#include "qgsreadwritecontext.h"
#include "qgsrectangle.h"
#include "qgsrendercontext.h"
#include "qgstiles.h"
#include "qgsvectortilebasicrenderer.h"
#include "qgsvectortileloader.h"
#include "qgsvectortilematrixset.h"
#include "qgsvectortilemvtdecoder.h"
#include "qgsvectortilerenderer.h"
#include "qgsvectortileutils.h"

#include <mtpl/mtpl.h>

#include <QApplication>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDomDocument>
#include <QFileInfo>
#include <QFile>
#include <QFormLayout>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPainter>
#include <QRegion>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#include "moc_qgsmtplpluginlayer.cpp"

namespace
{

constexpr double WEB_MERCATOR_HALF_WORLD = 20037508.342789244;

QString metadataValueText( const QVariant &value )
{
  if ( value.metaType().id() == QMetaType::QVariantMap )
    return QString::fromUtf8( QJsonDocument( QJsonValue::fromVariant( value ).toObject() ).toJson( QJsonDocument::Indented ) ).trimmed();
  if ( value.metaType().id() == QMetaType::QVariantList )
    return QString::fromUtf8( QJsonDocument( QJsonValue::fromVariant( value ).toArray() ).toJson( QJsonDocument::Indented ) ).trimmed();
  return value.toString();
}

QString metadataDisplayName( const QString &key )
{
  const QString normalized = key.toCaseFolded();
  if ( normalized == QLatin1String( "formatversion" ) )
    return QStringLiteral( "格式版本" );
  if ( normalized == QLatin1String( "filesize" ) )
    return QStringLiteral( "文件大小" );
  if ( normalized == QLatin1String( "rangecount" ) )
    return QStringLiteral( "范围数量" );
  if ( normalized == QLatin1String( "entrycount" ) )
    return QStringLiteral( "条目数量" );
  if ( normalized == QLatin1String( "encryptedentrycount" ) )
    return QStringLiteral( "加密条目数量" );
  if ( normalized == QLatin1String( "plainentrycount" ) )
    return QStringLiteral( "未加密条目数量" );
  if ( normalized == QLatin1String( "logicalsize" ) )
    return QStringLiteral( "原始大小" );
  if ( normalized == QLatin1String( "storedsize" ) )
    return QStringLiteral( "存储大小" );
  if ( normalized == QLatin1String( "presenttilecount" ) )
    return QStringLiteral( "已有瓦片数量" );
  if ( normalized == QLatin1String( "minimumzoom" ) || normalized == QLatin1String( "minzoom" ) )
    return QStringLiteral( "最小缩放级别" );
  if ( normalized == QLatin1String( "maximumzoom" ) || normalized == QLatin1String( "maxzoom" ) )
    return QStringLiteral( "最大缩放级别" );
  if ( normalized == QLatin1String( "rawmetadata" ) )
    return QStringLiteral( "原始元数据" );
  if ( normalized == QLatin1String( "datatype" ) || normalized == QLatin1String( "data_type" ) )
    return QStringLiteral( "数据类型" );
  if ( normalized == QLatin1String( "endianness" ) || normalized == QLatin1String( "byteorder" ) || normalized == QLatin1String( "byte_order" ) )
    return QStringLiteral( "字节序" );
  if ( normalized == QLatin1String( "name" ) )
    return QStringLiteral( "名称" );
  if ( normalized == QLatin1String( "description" ) )
    return QStringLiteral( "说明" );
  if ( normalized == QLatin1String( "version" ) )
    return QStringLiteral( "版本" );
  if ( normalized == QLatin1String( "attribution" ) )
    return QStringLiteral( "数据来源" );
  if ( normalized == QLatin1String( "bounds" ) )
    return QStringLiteral( "范围" );
  if ( normalized == QLatin1String( "center" ) )
    return QStringLiteral( "中心" );
  if ( normalized == QLatin1String( "tiles" ) )
    return QStringLiteral( "瓦片地址" );
  if ( normalized == QLatin1String( "tilesize" ) )
    return QStringLiteral( "瓦片大小" );
  if ( normalized == QLatin1String( "tile_file_ext" ) || normalized == QLatin1String( "tileformat" ) )
    return QStringLiteral( "瓦片格式" );
  if ( normalized == QLatin1String( "format" ) )
    return QStringLiteral( "格式" );
  if ( normalized == QLatin1String( "type" ) || normalized == QLatin1String( "payload" ) )
    return QStringLiteral( "载荷类型" );
  if ( normalized == QLatin1String( "scheme" ) )
    return QStringLiteral( "瓦片方案" );
  if ( normalized == QLatin1String( "crs" ) || normalized == QLatin1String( "crsauthid" ) || normalized == QLatin1String( "srs" ) )
    return QStringLiteral( "CRS" );
  if ( normalized == QLatin1String( "scale" ) )
    return QStringLiteral( "缩放系数" );
  if ( normalized == QLatin1String( "offset" ) )
    return QStringLiteral( "偏移量" );
  if ( normalized == QLatin1String( "nodata" ) || normalized == QLatin1String( "no_data" ) )
    return QStringLiteral( "NoData 值" );
  return key;
}

QString formatToXml( QgsMtpl::PackageFormat format )
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
      return QStringLiteral( "unknown" );
  }
  return QStringLiteral( "unknown" );
}

QgsMtpl::PackageFormat formatFromXml( const QString &value )
{
  const QString lower = value.toLower();
  if ( lower == QLatin1String( "ptp" ) )
    return QgsMtpl::PackageFormat::Ptp;
  if ( lower == QLatin1String( "dtp" ) )
    return QgsMtpl::PackageFormat::Dtp;
  if ( lower == QLatin1String( "vtp" ) )
    return QgsMtpl::PackageFormat::Vtp;
  if ( lower == QLatin1String( "sfp" ) )
    return QgsMtpl::PackageFormat::Sfp;
  return QgsMtpl::PackageFormat::Unknown;
}

QString payloadToXml( QgsMtpl::PayloadType payload )
{
  switch ( payload )
  {
    case QgsMtpl::PayloadType::RasterImage:
      return QStringLiteral( "raster" );
    case QgsMtpl::PayloadType::VectorTile:
      return QStringLiteral( "vector" );
    case QgsMtpl::PayloadType::Elevation:
      return QStringLiteral( "elevation" );
    case QgsMtpl::PayloadType::Files:
      return QStringLiteral( "files" );
    case QgsMtpl::PayloadType::Unknown:
      return QStringLiteral( "unknown" );
  }
  return QStringLiteral( "unknown" );
}

QgsMtpl::PayloadType payloadFromXml( const QString &value )
{
  const QString lower = value.toLower();
  if ( lower == QLatin1String( "raster" ) )
    return QgsMtpl::PayloadType::RasterImage;
  if ( lower == QLatin1String( "vector" ) )
    return QgsMtpl::PayloadType::VectorTile;
  if ( lower == QLatin1String( "elevation" ) )
    return QgsMtpl::PayloadType::Elevation;
  if ( lower == QLatin1String( "files" ) )
    return QgsMtpl::PayloadType::Files;
  return QgsMtpl::PayloadType::Unknown;
}

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

class TileReader
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

    mtpl_status_t rangeCount( size_t *count ) const
    {
      switch ( mFormat )
      {
        case QgsMtpl::PackageFormat::Ptp:
          return mtpl_ptp_reader_get_range_count( mPtp, count );
        case QgsMtpl::PackageFormat::Dtp:
          return mtpl_dtp_reader_get_range_count( mDtp, count );
        case QgsMtpl::PackageFormat::Vtp:
          return mtpl_vtp_reader_get_range_count( mVtp, count );
        case QgsMtpl::PackageFormat::Sfp:
        case QgsMtpl::PackageFormat::Unknown:
          return MTPL_STATUS_FORMAT_MISMATCH;
      }
      return MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t rangeInfo( size_t rangeIndex, mtpl_tile_range_info_t *info ) const
    {
      switch ( mFormat )
      {
        case QgsMtpl::PackageFormat::Ptp:
          return mtpl_ptp_reader_get_range_info( mPtp, rangeIndex, info );
        case QgsMtpl::PackageFormat::Dtp:
          return mtpl_dtp_reader_get_range_info( mDtp, rangeIndex, info );
        case QgsMtpl::PackageFormat::Vtp:
          return mtpl_vtp_reader_get_range_info( mVtp, rangeIndex, info );
        case QgsMtpl::PackageFormat::Sfp:
        case QgsMtpl::PackageFormat::Unknown:
          return MTPL_STATUS_FORMAT_MISMATCH;
      }
      return MTPL_STATUS_FORMAT_MISMATCH;
    }

    mtpl_status_t entryInfo( size_t rangeIndex, size_t slotIndex, mtpl_tile_entry_info_t *info ) const
    {
      switch ( mFormat )
      {
        case QgsMtpl::PackageFormat::Ptp:
          return mtpl_ptp_reader_get_entry_info( mPtp, rangeIndex, slotIndex, info );
        case QgsMtpl::PackageFormat::Dtp:
          return mtpl_dtp_reader_get_entry_info( mDtp, rangeIndex, slotIndex, info );
        case QgsMtpl::PackageFormat::Vtp:
          return mtpl_vtp_reader_get_entry_info( mVtp, rangeIndex, slotIndex, info );
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

  private:
    QgsMtpl::PackageFormat mFormat = QgsMtpl::PackageFormat::Unknown;
    mtpl_ptp_reader_t *mPtp = nullptr;
    mtpl_dtp_reader_t *mDtp = nullptr;
    mtpl_vtp_reader_t *mVtp = nullptr;
};

bool tilePackageKeysAreUsable( const QgsMtpl::PackageDescriptor &descriptor, const QgsMtpl::CryptoKeys &keys )
{
  TileReader reader( descriptor.format );
  if ( reader.open( descriptor.path, keys ) != MTPL_STATUS_OK )
    return false;

  size_t rangeCount = 0;
  if ( reader.rangeCount( &rangeCount ) != MTPL_STATUS_OK )
    return false;

  for ( size_t rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex )
  {
    mtpl_tile_range_info_t range = {};
    if ( reader.rangeInfo( rangeIndex, &range ) != MTPL_STATUS_OK )
      return false;
    for ( size_t slotIndex = 0; slotIndex < range.slot_count; ++slotIndex )
    {
      mtpl_tile_entry_info_t entry = {};
      if ( reader.entryInfo( rangeIndex, slotIndex, &entry ) != MTPL_STATUS_OK )
        return false;
      if ( !entry.present )
        continue;

      MtplBuffer tile;
      return reader.readTile( entry.coordinate, &tile.buffer ) == MTPL_STATUS_OK;
    }
  }
  return false;
}

bool sfpPackageKeysAreUsable( const QgsMtpl::PackageDescriptor &descriptor, const QgsMtpl::CryptoKeys &keys )
{
  mtpl_crypto_options_t crypto = {};
  crypto.private_key = reinterpret_cast<const uint8_t *>( keys.privateKey.constData() );
  crypto.private_key_size = static_cast<size_t>( keys.privateKey.size() );
  crypto.device_key = reinterpret_cast<const uint8_t *>( keys.deviceKey.constData() );
  crypto.device_key_size = static_cast<size_t>( keys.deviceKey.size() );

  mtpl_sfp_reader_t *reader = nullptr;
  const QByteArray path = descriptor.path.toUtf8();
  if ( mtpl_sfp_reader_open( path.constData(), &crypto, &reader ) != MTPL_STATUS_OK )
    return false;

  bool usable = false;
  size_t entryCount = 0;
  if ( mtpl_sfp_reader_get_entry_count( reader, &entryCount ) == MTPL_STATUS_OK )
  {
    for ( size_t entryIndex = 0; entryIndex < entryCount; ++entryIndex )
    {
      mtpl_sfp_entry_info_t entry = {};
      if ( mtpl_sfp_reader_get_entry_info( reader, entryIndex, &entry ) != MTPL_STATUS_OK )
        break;
      if ( entry.storage_mode != MTPL_STORAGE_ENCRYPTED || !entry.path )
        continue;

      MtplBuffer contents;
      usable = mtpl_sfp_reader_read_file( reader, entry.path, &contents.buffer ) == MTPL_STATUS_OK;
      break;
    }
  }
  mtpl_sfp_reader_close( reader );
  return usable;
}

bool restoreKeysFromSidecar( QgsMtpl::PackageDescriptor &descriptor, QgsMtpl::CryptoKeys &keys )
{
  QgsMtpl::KeySidecar sidecar;
  QString sidecarPath = descriptor.sidecarPath;
  QString ignoredError;
  bool found = !sidecarPath.isEmpty() &&
               QgsMtpl::KeySidecarStore::readForPackage( sidecarPath, descriptor.path, sidecar, ignoredError );
  if ( !found )
    found = QgsMtpl::KeySidecarStore::discover( descriptor.path, sidecar, sidecarPath, ignoredError );
  if ( !found || ( !descriptor.keyId.isEmpty() && descriptor.keyId.compare( sidecar.keys.keyId, Qt::CaseInsensitive ) != 0 ) )
  {
    sidecar.keys.privateKeyBase64.fill( '\0' );
    sidecar.keys.deviceKeyHex.fill( '\0' );
    return false;
  }

  QgsMtpl::CryptoKeys candidate = sidecar.keys.cryptoKeys();
  sidecar.keys.privateKeyBase64.fill( '\0' );
  sidecar.keys.deviceKeyHex.fill( '\0' );
  const bool usable = descriptor.format == QgsMtpl::PackageFormat::Sfp
                        ? sfpPackageKeysAreUsable( descriptor, candidate )
                        : tilePackageKeysAreUsable( descriptor, candidate );
  if ( !usable )
  {
    candidate.clear();
    return false;
  }

  keys = candidate;
  candidate.clear();
  descriptor.sidecarPath = sidecarPath;
  descriptor.keyId = sidecar.keys.keyId;
  descriptor.encryption = QgsMtpl::EncryptionState::Encrypted;
  return true;
}

struct TileEntry
{
  mtpl_tile_coordinate_t coordinate = { 0, 0, 0 };
  uint64_t storedSize = 0;
};

bool visibleExtentInLayerCrs( QgsRenderContext &context, QgsRectangle &visibleExtent )
{
  visibleExtent = context.mapExtent();
  const QgsCoordinateTransform transform = context.coordinateTransform();
  if ( transform.isValid() && !transform.isShortCircuited() )
  {
    try
    {
      visibleExtent = transform.transformBoundingBox( visibleExtent, Qgis::TransformDirection::Reverse );
    }
    catch ( QgsCsException & )
    {
      return false;
    }
  }
  return !visibleExtent.isEmpty();
}

int chooseZoom( const QSet<int> &availableZooms, QgsRenderContext &context, int tileSize, const QgsRectangle &visibleExtent )
{
  if ( availableZooms.isEmpty() )
    return -1;

  const int outputWidth = std::max( 1, context.outputSize().width() );
  const double unitsPerPixel = std::max( visibleExtent.width() / outputWidth, std::numeric_limits<double>::epsilon() );
  const double rawZoom = std::log2( ( 2.0 * WEB_MERCATOR_HALF_WORLD ) / ( std::max( 1, tileSize ) * unitsPerPixel ) );
  const int desiredZoom = std::max( 0, static_cast<int>( std::lround( rawZoom ) ) );

  int selectedZoom = *availableZooms.constBegin();
  int selectedDistance = std::abs( selectedZoom - desiredZoom );
  for ( int zoom : availableZooms )
  {
    const int distance = std::abs( zoom - desiredZoom );
    if ( distance < selectedDistance || ( distance == selectedDistance && zoom > selectedZoom ) )
    {
      selectedZoom = zoom;
      selectedDistance = distance;
    }
  }
  return selectedZoom;
}

struct VisibleTileWindow
{
  uint32_t xMinimum = 0;
  uint32_t xMaximum = 0;
  uint32_t yMinimum = 0;
  uint32_t yMaximum = 0;
};

bool visibleTileWindow( const QgsMtpl::PackageDescriptor &descriptor,
                        int zoom,
                        const QgsRectangle &visibleExtent,
                        VisibleTileWindow &window )
{
  if ( zoom < 0 || zoom > 30 )
    return false;

  const QgsTileRange visibleRange = QgsTileMatrix::fromWebMercator( zoom ).tileRangeFromExtent( visibleExtent );
  if ( !visibleRange.isValid() )
    return false;

  window.xMinimum = static_cast<uint32_t>( visibleRange.startColumn() );
  window.xMaximum = static_cast<uint32_t>( visibleRange.endColumn() );
  if ( descriptor.scheme.compare( QLatin1String( "tms" ), Qt::CaseInsensitive ) == 0 )
  {
    const uint32_t lastRow = ( uint32_t( 1 ) << zoom ) - 1;
    window.yMinimum = lastRow - static_cast<uint32_t>( visibleRange.endRow() );
    window.yMaximum = lastRow - static_cast<uint32_t>( visibleRange.startRow() );
  }
  else
  {
    window.yMinimum = static_cast<uint32_t>( visibleRange.startRow() );
    window.yMaximum = static_cast<uint32_t>( visibleRange.endRow() );
  }
  return true;
}

uint32_t displayRow( const QgsMtpl::PackageDescriptor &descriptor, const mtpl_tile_coordinate_t &coordinate )
{
  if ( descriptor.scheme.compare( QLatin1String( "tms" ), Qt::CaseInsensitive ) != 0 || coordinate.zoom >= 32 )
    return coordinate.y;

  const uint64_t matrixSize = uint64_t( 1 ) << coordinate.zoom;
  return coordinate.y < matrixSize ? static_cast<uint32_t>( matrixSize - 1 - coordinate.y ) : coordinate.y;
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

  // MTPL's convenience crop API assumes two-byte samples. Crop the common
  // overlap-border sizes here so float32 and other supported sample types use
  // the same 33->32 and 129->128 behavior. A 256 tile is used as-is.
  const int side = sourceSide == 33 || sourceSide == 129 ? sourceSide - 1 : sourceSide;

  const bool bigEndian = elevationEndianness( descriptor ) == QLatin1String( "big" );
  const uchar *raw = reinterpret_cast<const uchar *>( bytes.constData() );

  QVector<double> samples;
  samples.resize( side * side );
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

QPolygon tilePolygon( const TileEntry &entry, const QgsMtpl::PackageDescriptor &descriptor, QgsRenderContext &context )
{
  const uint32_t row = displayRow( descriptor, entry.coordinate );
  const QgsTileXYZ id( static_cast<int>( entry.coordinate.x ), static_cast<int>( row ), static_cast<int>( entry.coordinate.zoom ) );
  return QgsVectorTileUtils::tilePolygon( id, context.coordinateTransform(), QgsTileMatrix::fromWebMercator( id.zoomLevel() ), context.mapToPixel() );
}

class QgsMtplPackageRenderer final : public QgsMapLayerRenderer
{
  public:
    QgsMtplPackageRenderer( const QString &layerId, QgsRenderContext *context, const QgsMtpl::PackageDescriptor &descriptor, const QgsMtpl::CryptoKeys &keys )
      : QgsMapLayerRenderer( layerId, context )
      , mDescriptor( descriptor )
      , mKeys( keys )
    {}

    ~QgsMtplPackageRenderer() override
    {
      mKeys.clear();
    }

    bool render() override
    {
      QgsRenderContext *context = renderContext();
      if ( !context || !context->painter() )
        return false;
      if ( !mDescriptor.isSpatial() )
      {
        mErrors << QObject::tr( "此 MTPL 数据包是文件容器，无法直接渲染。" );
        return false;
      }
      if ( mDescriptor.isLocked() )
      {
        mErrors << QObject::tr( "此 MTPL 数据包需要匹配的密钥才能渲染。" );
        return false;
      }

      TileReader reader( mDescriptor.format );
      const mtpl_status_t openStatus = reader.open( mDescriptor.path, mKeys );
      if ( openStatus == MTPL_STATUS_KEY_REQUIRED || openStatus == MTPL_STATUS_CRYPTO_ERROR )
      {
        mErrors << QObject::tr( "无法解密 MTPL 数据包。密钥可能不匹配，或数据包已损坏。" );
        return false;
      }
      if ( openStatus != MTPL_STATUS_OK )
      {
        mErrors << QObject::tr( "无法打开 MTPL 数据包（%1）。" ).arg( QgsMtpl::statusText( openStatus ) );
        return false;
      }

      size_t rangeCount = 0;
      mtpl_status_t status = reader.rangeCount( &rangeCount );
      if ( status != MTPL_STATUS_OK )
      {
        mErrors << QObject::tr( "无法检查 MTPL 瓦片范围（%1）。" ).arg( QgsMtpl::statusText( status ) );
        return false;
      }

      QSet<int> availableZooms;
      QList<mtpl_tile_range_info_t> ranges;
      ranges.reserve( static_cast<int>( std::min<size_t>( rangeCount, static_cast<size_t>( std::numeric_limits<int>::max() ) ) ) );
      for ( size_t rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex )
      {
        mtpl_tile_range_info_t range = {};
        status = reader.rangeInfo( rangeIndex, &range );
        if ( status != MTPL_STATUS_OK )
        {
          mErrors << QObject::tr( "无法检查 MTPL 瓦片范围（%1）。" ).arg( QgsMtpl::statusText( status ) );
          return false;
        }
        if ( range.present_count > 0 )
          availableZooms.insert( static_cast<int>( range.bounds.zoom ) );
        ranges.append( range );
      }

      QgsRectangle visibleExtent;
      if ( !visibleExtentInLayerCrs( *context, visibleExtent ) )
      {
        mErrors << QObject::tr( "无法确定 MTPL 图层的可见范围。" );
        return false;
      }

      const int selectedZoom = chooseZoom( availableZooms, *context, mDescriptor.tileSize, visibleExtent );
      if ( selectedZoom < 0 )
        return true;

      VisibleTileWindow visibleWindow;
      if ( !visibleTileWindow( mDescriptor, selectedZoom, visibleExtent, visibleWindow ) )
        return true;

      QList<TileEntry> entries;
      int rangeListIndex = 0;
      for ( size_t rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex, ++rangeListIndex )
      {
        if ( context->renderingStopped() )
          return false;

        const mtpl_tile_range_info_t &range = ranges.at( rangeListIndex );
        if ( static_cast<int>( range.bounds.zoom ) != selectedZoom )
          continue;

        const uint32_t xMinimum = std::max( range.bounds.x_min, visibleWindow.xMinimum );
        const uint32_t xMaximum = std::min( range.bounds.x_max, visibleWindow.xMaximum );
        const uint32_t yMinimum = std::max( range.bounds.y_min, visibleWindow.yMinimum );
        const uint32_t yMaximum = std::min( range.bounds.y_max, visibleWindow.yMaximum );
        if ( xMinimum > xMaximum || yMinimum > yMaximum )
          continue;

        const uint64_t rangeWidth = static_cast<uint64_t>( range.bounds.x_max ) - range.bounds.x_min + 1;
        for ( uint64_t y = yMinimum; y <= yMaximum; ++y )
        {
          const uint64_t rowOffset = ( y - range.bounds.y_min ) * rangeWidth;
          for ( uint64_t x = xMinimum; x <= xMaximum; ++x )
          {
            if ( context->renderingStopped() )
              return false;

            const uint64_t slotIndex = rowOffset + x - range.bounds.x_min;
            if ( slotIndex >= range.slot_count || slotIndex > std::numeric_limits<size_t>::max() )
            {
              mErrors << QObject::tr( "MTPL 瓦片范围中包含无效的槽位索引。" );
              return false;
            }

            mtpl_tile_entry_info_t entryInfo = {};
            status = reader.entryInfo( rangeIndex, static_cast<size_t>( slotIndex ), &entryInfo );
            if ( status != MTPL_STATUS_OK )
            {
              mErrors << QObject::tr( "无法检查 MTPL 瓦片条目（%1）。" ).arg( QgsMtpl::statusText( status ) );
              return false;
            }
            if ( entryInfo.present )
              entries.append( TileEntry { entryInfo.coordinate, entryInfo.stored_size } );
          }
        }
      }

      if ( entries.isEmpty() )
        return true;

      int minColumn = std::numeric_limits<int>::max();
      int maxColumn = std::numeric_limits<int>::min();
      int minRow = std::numeric_limits<int>::max();
      int maxRow = std::numeric_limits<int>::min();
      for ( const TileEntry &entry : std::as_const( entries ) )
      {
        const int column = static_cast<int>( entry.coordinate.x );
        const int row = static_cast<int>( displayRow( mDescriptor, entry.coordinate ) );
        minColumn = std::min( minColumn, column );
        maxColumn = std::max( maxColumn, column );
        minRow = std::min( minRow, row );
        maxRow = std::max( maxRow, row );
      }

      const bool mayContainVectorTiles = mDescriptor.payload == QgsMtpl::PayloadType::VectorTile || mDescriptor.payload == QgsMtpl::PayloadType::Unknown || mDescriptor.format == QgsMtpl::PackageFormat::Vtp;
      std::unique_ptr<QgsVectorTileRenderer> vectorRenderer;
      const QgsVectorTileMatrixSet matrixSet = QgsVectorTileMatrixSet::fromWebMercator( 0, std::max( 0, selectedZoom ) );
      bool vectorRendererStarted = false;
      if ( mayContainVectorTiles )
      {
        if ( !mDescriptor.stylePath.isEmpty() )
        {
          QFile styleFile( mDescriptor.stylePath );
          if ( styleFile.open( QIODevice::ReadOnly ) )
          {
            QgsMapBoxGlStyleConverter converter;
            if ( converter.convert( QString::fromUtf8( styleFile.readAll() ) ) == QgsMapBoxGlStyleConverter::Success )
              vectorRenderer.reset( converter.renderer() );
          }
        }
        if ( !vectorRenderer )
        {
          auto basicRenderer = std::make_unique<QgsVectorTileBasicRenderer>();
          basicRenderer->setStyles( QgsVectorTileBasicRenderer::simpleStyle(
            QColor( 209, 224, 239, 170 ), QColor( 91, 120, 145 ), 0.3,
            QColor( 53, 92, 125 ), 0.6,
            QColor( 230, 126, 34 ), QColor( 255, 255, 255 ), 2.0 ) );
          vectorRenderer = std::move( basicRenderer );
        }
        vectorRenderer->startRender( *context, selectedZoom, QgsTileRange( minColumn, maxColumn, minRow, maxRow ) );
        vectorRenderer->renderBackground( *context );
        vectorRendererStarted = true;
      }

      bool success = true;
      for ( const TileEntry &entry : std::as_const( entries ) )
      {
        if ( context->renderingStopped() )
        {
          success = false;
          break;
        }

        MtplBuffer tile;
        status = reader.readTile( entry.coordinate, &tile.buffer );
        if ( status == MTPL_STATUS_NOT_FOUND )
          continue;
        if ( status != MTPL_STATUS_OK )
        {
          mErrors << QObject::tr( "无法读取 MTPL 瓦片（%1）。" ).arg( QgsMtpl::statusText( status ) );
          success = false;
          continue;
        }

        const QByteArray bytes = tile.toByteArray();
        if ( bytes.isEmpty() )
          continue;

        QImage image;
        if ( mDescriptor.format == QgsMtpl::PackageFormat::Dtp || mDescriptor.payload == QgsMtpl::PayloadType::Elevation )
          image = elevationImage( bytes, mDescriptor );
        else if ( mDescriptor.payload != QgsMtpl::PayloadType::VectorTile && mDescriptor.format != QgsMtpl::PackageFormat::Vtp )
          image = QImage::fromData( bytes );

        if ( !image.isNull() )
        {
          try
          {
            const QPolygon polygon = tilePolygon( entry, mDescriptor, *context );
            context->painter()->save();
            context->painter()->setRenderHint( QPainter::SmoothPixmapTransform, true );
            context->painter()->setClipRegion( QRegion( polygon ), Qt::IntersectClip );
            context->painter()->drawImage( QRectF( polygon.boundingRect() ), image );
            context->painter()->restore();
          }
          catch ( QgsCsException & )
          {
            success = false;
          }
          continue;
        }

        if ( !vectorRendererStarted )
        {
          mErrors << QObject::tr( "MTPL 瓦片载荷不是支持的影像或矢量瓦片。" );
          success = false;
          continue;
        }

        const uint32_t row = displayRow( mDescriptor, entry.coordinate );
        const QgsTileXYZ tileId( static_cast<int>( entry.coordinate.x ), static_cast<int>( row ), static_cast<int>( entry.coordinate.zoom ) );
        QgsVectorTileRawData rawTile( tileId, bytes );
        QgsVectorTileMVTDecoder decoder( matrixSet );
        if ( !decoder.decode( rawTile ) )
        {
          mErrors << QObject::tr( "无法解码 MTPL 矢量瓦片。" );
          success = false;
          continue;
        }

        QgsVectorTileRendererData rendererData( tileId );
        rendererData.setRenderZoomLevel( selectedZoom );
        const QMap<QString, QgsFields> fields;
        rendererData.setFields( fields );
        rendererData.setFeatures( decoder.layerFeatures( fields, context->coordinateTransform() ) );
        try
        {
          rendererData.setTilePolygon( QgsVectorTileUtils::tilePolygon( tileId, context->coordinateTransform(), matrixSet.tileMatrix( selectedZoom ), context->mapToPixel() ) );
        }
        catch ( QgsCsException & )
        {
          success = false;
          continue;
        }

        context->painter()->save();
        context->painter()->setClipRegion( QRegion( rendererData.tilePolygon() ), Qt::IntersectClip );
        vectorRenderer->renderTile( rendererData, *context );
        context->painter()->restore();
      }

      if ( vectorRendererStarted )
        vectorRenderer->stopRender( *context );
      return success;
    }

  private:
    QgsMtpl::PackageDescriptor mDescriptor;
    QgsMtpl::CryptoKeys mKeys;
};

} // namespace

QString QgsMtplPluginLayer::layerTypeKey()
{
  return QStringLiteral( "mtpl_package" );
}

QgsMtplPluginLayer::QgsMtplPluginLayer( const QgsMtpl::PackageDescriptor &descriptor, const QgsMtpl::CryptoKeys &keys )
  : QgsPluginLayer( layerTypeKey(), descriptor.displayName.isEmpty() ? QFileInfo( descriptor.path ).completeBaseName() : descriptor.displayName )
  , mDescriptor( descriptor )
  , mKeys( keys )
{
  applyDescriptor();
}

QgsMtplPluginLayer::~QgsMtplPluginLayer()
{
  clearCryptoKeys();
}

QgsMtplPluginLayer *QgsMtplPluginLayer::clone() const
{
  auto *layer = new QgsMtplPluginLayer( mDescriptor, mKeys );
  QgsMapLayer::clone( layer );
  layer->applyDescriptor();
  return layer;
}

QgsMapLayerRenderer *QgsMtplPluginLayer::createMapRenderer( QgsRenderContext &rendererContext )
{
  return new QgsMtplPackageRenderer( id(), &rendererContext, mDescriptor, mKeys );
}

bool QgsMtplPluginLayer::isSpatial() const
{
  return mDescriptor.isSpatial();
}

bool QgsMtplPluginLayer::readSymbology( const QDomNode &node, QString &errorMessage, QgsReadWriteContext &context, StyleCategories categories )
{
  Q_UNUSED( node )
  Q_UNUSED( errorMessage )
  Q_UNUSED( context )
  Q_UNUSED( categories )
  return true;
}

bool QgsMtplPluginLayer::writeSymbology( QDomNode &node, QDomDocument &document, QString &errorMessage, const QgsReadWriteContext &context, StyleCategories categories ) const
{
  Q_UNUSED( node )
  Q_UNUSED( document )
  Q_UNUSED( errorMessage )
  Q_UNUSED( context )
  Q_UNUSED( categories )
  return true;
}

void QgsMtplPluginLayer::setTransformContext( const QgsCoordinateTransformContext &transformContext )
{
  Q_UNUSED( transformContext )
}

const QgsMtpl::PackageDescriptor &QgsMtplPluginLayer::descriptor() const
{
  return mDescriptor;
}

void QgsMtplPluginLayer::setDescriptor( const QgsMtpl::PackageDescriptor &descriptor )
{
  mDescriptor = descriptor;
  applyDescriptor();
  triggerRepaint();
}

void QgsMtplPluginLayer::setCryptoKeys( const QgsMtpl::CryptoKeys &keys )
{
  clearCryptoKeys();
  mKeys = keys;
  if ( mKeys.isValid() && mDescriptor.encryption == QgsMtpl::EncryptionState::Locked )
    mDescriptor.encryption = QgsMtpl::EncryptionState::Encrypted;
  triggerRepaint();
}

void QgsMtplPluginLayer::clearCryptoKeys()
{
  mKeys.clear();
  if ( mDescriptor.encryption == QgsMtpl::EncryptionState::Encrypted )
    mDescriptor.encryption = QgsMtpl::EncryptionState::Locked;
}

bool QgsMtplPluginLayer::hasCryptoKeys() const
{
  return mKeys.isValid();
}

bool QgsMtplPluginLayer::readXml( const QDomNode &layerNode, QgsReadWriteContext &context )
{
  if ( !QgsMapLayer::readXml( layerNode, context ) )
    return false;

  const QDomElement element = layerNode.firstChildElement( QStringLiteral( "mtpl-package" ) );
  mDescriptor.path = source();
  mDescriptor.displayName = name();
  mDescriptor.format = formatFromXml( element.attribute( QStringLiteral( "format" ) ) );
  if ( mDescriptor.format == QgsMtpl::PackageFormat::Unknown )
    mDescriptor.format = QgsMtpl::packageFormatFromPath( mDescriptor.path );
  mDescriptor.payload = payloadFromXml( element.attribute( QStringLiteral( "payload" ) ) );
  const QString storage = element.attribute( QStringLiteral( "storage" ) );
  mDescriptor.encryption = storage == QLatin1String( "encrypted" ) || storage == QLatin1String( "locked" ) ? QgsMtpl::EncryptionState::Locked : QgsMtpl::EncryptionState::Plain;
  mDescriptor.sidecarPath = context.pathResolver().readPath( element.attribute( QStringLiteral( "sidecar" ) ) );
  mDescriptor.keyId = element.attribute( QStringLiteral( "keyId" ) );
  mDescriptor.crsAuthId = element.attribute( QStringLiteral( "crs" ), QStringLiteral( "EPSG:3857" ) );
  mDescriptor.scheme = element.attribute( QStringLiteral( "scheme" ), QStringLiteral( "xyz" ) );
  mDescriptor.stylePath = context.pathResolver().readPath( element.attribute( QStringLiteral( "style" ) ) );
  mDescriptor.tileSize = element.attribute( QStringLiteral( "tileSize" ), QStringLiteral( "256" ) ).toInt();
  mDescriptor.minimumZoom = element.attribute( QStringLiteral( "minimumZoom" ), QStringLiteral( "-1" ) ).toInt();
  mDescriptor.maximumZoom = element.attribute( QStringLiteral( "maximumZoom" ), QStringLiteral( "-1" ) ).toInt();
  mDescriptor.hasExtent = element.attribute( QStringLiteral( "hasExtent" ), QStringLiteral( "0" ) ).toInt() != 0;
  mDescriptor.extentXMinimum = element.attribute( QStringLiteral( "extentXMinimum" ), QStringLiteral( "0" ) ).toDouble();
  mDescriptor.extentYMinimum = element.attribute( QStringLiteral( "extentYMinimum" ), QStringLiteral( "0" ) ).toDouble();
  mDescriptor.extentXMaximum = element.attribute( QStringLiteral( "extentXMaximum" ), QStringLiteral( "0" ) ).toDouble();
  mDescriptor.extentYMaximum = element.attribute( QStringLiteral( "extentYMaximum" ), QStringLiteral( "0" ) ).toDouble();
  mDescriptor.scale = element.attribute( QStringLiteral( "scale" ), QStringLiteral( "1" ) ).toDouble();
  mDescriptor.offset = element.attribute( QStringLiteral( "offset" ), QStringLiteral( "0" ) ).toDouble();
  mDescriptor.hasNoData = element.attribute( QStringLiteral( "hasNoData" ), QStringLiteral( "0" ) ).toInt() != 0;
  mDescriptor.noData = element.attribute( QStringLiteral( "noData" ), QStringLiteral( "0" ) ).toDouble();
  mDescriptor.displayOverridesApplied = true;
  mKeys.clear();
  if ( mDescriptor.format == QgsMtpl::PackageFormat::Dtp )
  {
    mDescriptor.metadata.insert( QStringLiteral( "dataType" ), element.attribute( QStringLiteral( "dataType" ), QStringLiteral( "uint16" ) ) );
    mDescriptor.metadata.insert( QStringLiteral( "endianness" ), element.attribute( QStringLiteral( "endianness" ), QStringLiteral( "little" ) ) );
  }
  if ( mDescriptor.encryption == QgsMtpl::EncryptionState::Locked )
    restoreKeysFromSidecar( mDescriptor, mKeys );
  applyDescriptor();
  return !mDescriptor.path.isEmpty();
}

bool QgsMtplPluginLayer::writeXml( QDomNode &layerNode, QDomDocument &document, const QgsReadWriteContext &context ) const
{
  if ( !QgsMapLayer::writeXml( layerNode, document, context ) )
    return false;

  QDomElement layerElement = layerNode.toElement();
  layerElement.setAttribute( QStringLiteral( "type" ), QStringLiteral( "plugin" ) );
  layerElement.setAttribute( QStringLiteral( "name" ), layerTypeKey() );

  QDomElement element = document.createElement( QStringLiteral( "mtpl-package" ) );
  element.setAttribute( QStringLiteral( "format" ), formatToXml( mDescriptor.format ) );
  element.setAttribute( QStringLiteral( "payload" ), payloadToXml( mDescriptor.payload ) );
  element.setAttribute( QStringLiteral( "storage" ), mDescriptor.encryption == QgsMtpl::EncryptionState::Plain ? QStringLiteral( "plain" ) : QStringLiteral( "encrypted" ) );
  if ( !mDescriptor.sidecarPath.isEmpty() )
    element.setAttribute( QStringLiteral( "sidecar" ), context.pathResolver().writePath( mDescriptor.sidecarPath ) );
  if ( !mDescriptor.keyId.isEmpty() )
    element.setAttribute( QStringLiteral( "keyId" ), mDescriptor.keyId );
  element.setAttribute( QStringLiteral( "crs" ), mDescriptor.crsAuthId );
  element.setAttribute( QStringLiteral( "scheme" ), mDescriptor.scheme );
  if ( !mDescriptor.stylePath.isEmpty() )
    element.setAttribute( QStringLiteral( "style" ), context.pathResolver().writePath( mDescriptor.stylePath ) );
  element.setAttribute( QStringLiteral( "tileSize" ), mDescriptor.tileSize );
  element.setAttribute( QStringLiteral( "minimumZoom" ), mDescriptor.minimumZoom );
  element.setAttribute( QStringLiteral( "maximumZoom" ), mDescriptor.maximumZoom );
  element.setAttribute( QStringLiteral( "hasExtent" ), mDescriptor.hasExtent ? 1 : 0 );
  if ( mDescriptor.hasExtent )
  {
    element.setAttribute( QStringLiteral( "extentXMinimum" ), QString::number( mDescriptor.extentXMinimum, 'g', 17 ) );
    element.setAttribute( QStringLiteral( "extentYMinimum" ), QString::number( mDescriptor.extentYMinimum, 'g', 17 ) );
    element.setAttribute( QStringLiteral( "extentXMaximum" ), QString::number( mDescriptor.extentXMaximum, 'g', 17 ) );
    element.setAttribute( QStringLiteral( "extentYMaximum" ), QString::number( mDescriptor.extentYMaximum, 'g', 17 ) );
  }
  element.setAttribute( QStringLiteral( "scale" ), QString::number( mDescriptor.scale, 'g', 17 ) );
  element.setAttribute( QStringLiteral( "offset" ), QString::number( mDescriptor.offset, 'g', 17 ) );
  element.setAttribute( QStringLiteral( "hasNoData" ), mDescriptor.hasNoData ? 1 : 0 );
  if ( mDescriptor.hasNoData )
    element.setAttribute( QStringLiteral( "noData" ), QString::number( mDescriptor.noData, 'g', 17 ) );
  if ( mDescriptor.format == QgsMtpl::PackageFormat::Dtp )
  {
    element.setAttribute( QStringLiteral( "dataType" ), elevationDataType( mDescriptor ) );
    element.setAttribute( QStringLiteral( "endianness" ), elevationEndianness( mDescriptor ) );
  }
  layerNode.appendChild( element );
  return true;
}

QString QgsMtplPluginLayer::encodedSource( const QString &source, const QgsReadWriteContext &context ) const
{
  return context.pathResolver().writePath( source );
}

QString QgsMtplPluginLayer::decodedSource( const QString &source, const QString &dataProvider, const QgsReadWriteContext &context ) const
{
  Q_UNUSED( dataProvider )
  return context.pathResolver().readPath( source );
}

void QgsMtplPluginLayer::applyDescriptor()
{
  if ( mDescriptor.displayName.isEmpty() )
    mDescriptor.displayName = QFileInfo( mDescriptor.path ).completeBaseName();
  if ( name().isEmpty() )
    setName( mDescriptor.displayName );
  if ( mDescriptor.format == QgsMtpl::PackageFormat::Unknown )
    mDescriptor.format = QgsMtpl::packageFormatFromPath( mDescriptor.path );
  if ( mDescriptor.format == QgsMtpl::PackageFormat::Dtp && mDescriptor.payload == QgsMtpl::PayloadType::Unknown )
    mDescriptor.payload = QgsMtpl::PayloadType::Elevation;
  else if ( mDescriptor.format == QgsMtpl::PackageFormat::Vtp && mDescriptor.payload == QgsMtpl::PayloadType::Unknown )
    mDescriptor.payload = QgsMtpl::PayloadType::VectorTile;
  else if ( mDescriptor.format == QgsMtpl::PackageFormat::Sfp && mDescriptor.payload == QgsMtpl::PayloadType::Unknown )
    mDescriptor.payload = QgsMtpl::PayloadType::Files;

  if ( mDescriptor.format == QgsMtpl::PackageFormat::Dtp )
  {
    mDescriptor.metadata.insert( QStringLiteral( "dataType" ), elevationDataType( mDescriptor ) );
    mDescriptor.metadata.insert( QStringLiteral( "endianness" ), elevationEndianness( mDescriptor ) );
  }

  setSource( mDescriptor.path );
  const QgsCoordinateReferenceSystem crs( mDescriptor.crsAuthId );
  if ( crs.isValid() )
    setCrs( crs );
  if ( mDescriptor.isSpatial() )
  {
    if ( mDescriptor.hasExtent )
      setExtent( QgsRectangle( mDescriptor.extentXMinimum, mDescriptor.extentYMinimum, mDescriptor.extentXMaximum, mDescriptor.extentYMaximum ) );
    else
      setExtent( QgsRectangle( -WEB_MERCATOR_HALF_WORLD, -WEB_MERCATOR_HALF_WORLD, WEB_MERCATOR_HALF_WORLD, WEB_MERCATOR_HALF_WORLD ) );
  }
  else
    setExtent( QgsRectangle() );
  setValid( !mDescriptor.path.isEmpty() && QFileInfo::exists( mDescriptor.path ) );
}

QgsMtplPluginLayerType::QgsMtplPluginLayerType()
  : QgsPluginLayerType( QgsMtplPluginLayer::layerTypeKey() )
{}

QgsPluginLayer *QgsMtplPluginLayerType::createLayer()
{
  return new QgsMtplPluginLayer();
}

QgsPluginLayer *QgsMtplPluginLayerType::createLayer( const QString &uri )
{
  QgsMtpl::PackageDescriptor descriptor;
  descriptor.path = uri;
  descriptor.displayName = QFileInfo( uri ).completeBaseName();
  descriptor.format = QgsMtpl::packageFormatFromPath( uri );
  descriptor.encryption = QgsMtpl::EncryptionState::Unknown;
  return new QgsMtplPluginLayer( descriptor );
}

bool QgsMtplPluginLayerType::showLayerProperties( QgsPluginLayer *layer )
{
  auto *mtplLayer = dynamic_cast<QgsMtplPluginLayer *>( layer );
  if ( !mtplLayer )
    return false;

  const QgsMtpl::PackageDescriptor &descriptor = mtplLayer->descriptor();
  QDialog dialog( QApplication::activeWindow() );
  dialog.setWindowTitle( QObject::tr( "MTPL 数据包属性" ) );
  dialog.resize( 560, 420 );
  auto *layout = new QFormLayout( &dialog );
  auto addValue = [layout]( const QString &label, const QString &value ) {
    auto *widget = new QLabel( value );
    widget->setTextInteractionFlags( Qt::TextSelectableByMouse );
    widget->setWordWrap( true );
    layout->addRow( label, widget );
  };
  addValue( QObject::tr( "路径" ), QDir::toNativeSeparators( descriptor.path ) );
  addValue( QObject::tr( "格式" ), QgsMtpl::packageFormatName( descriptor.format ) );
  addValue( QObject::tr( "载荷" ), QgsMtpl::payloadTypeName( descriptor.payload ) );
  addValue( QObject::tr( "加密状态" ), QgsMtpl::encryptionStateName( descriptor.encryption ) );
  if ( descriptor.isSpatial() )
  {
    addValue( QObject::tr( "CRS" ), descriptor.crsAuthId );
    addValue( QObject::tr( "瓦片方案" ), descriptor.scheme.toUpper() );
  }
  if ( !descriptor.stylePath.isEmpty() )
    addValue( QObject::tr( "样式" ), QDir::toNativeSeparators( descriptor.stylePath ) );

  auto *metadataTable = new QTableWidget( descriptor.metadata.size(), 2, &dialog );
  metadataTable->setHorizontalHeaderLabels( { QObject::tr( "元数据" ), QObject::tr( "值" ) } );
  metadataTable->horizontalHeader()->setStretchLastSection( true );
  metadataTable->verticalHeader()->hide();
  metadataTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
  int row = 0;
  for ( auto it = descriptor.metadata.constBegin(); it != descriptor.metadata.constEnd(); ++it )
  {
    metadataTable->setItem( row, 0, new QTableWidgetItem( metadataDisplayName( it.key() ) ) );
    metadataTable->setItem( row, 1, new QTableWidgetItem( metadataValueText( it.value() ) ) );
    ++row;
  }
  layout->addRow( metadataTable );

  auto *buttons = new QDialogButtonBox( QDialogButtonBox::Close, &dialog );
  if ( QPushButton *closeButton = buttons->button( QDialogButtonBox::Close ) )
    closeButton->setText( QObject::tr( "关闭" ) );
  QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
  layout->addRow( buttons );
  dialog.exec();
  return true;
}
