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

#include "rendering/qgsmtplrasterinterface.h"
#include "services/qgsmtplcredentialstore.h"
#include "services/qgsmtplpackageservice.h"
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
#include "qgsrasterdrawer.h"
#include "qgsrasteriterator.h"
#include "qgsrasterprojector.h"
#include "qgsrasterviewport.h"
#include "qgsrendercontext.h"
#include "qgstiles.h"
#include "qgsthreadingutils.h"
#include "qgsvectortilebasicrenderer.h"
#include "qgsvectortileloader.h"
#include "qgsvectortilematrixset.h"
#include "qgsvectortilemvtdecoder.h"
#include "qgsvectortilerenderer.h"
#include "qgsvectortileutils.h"

#include <mtpl/mtpl.h>

#include <QApplication>
#include <QColor>
#include <QDateTime>
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
#include <QMutex>
#include <QMutexLocker>
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

bool isInternalMetadataKey( const QString &key )
{
  return key.startsWith( QLatin1String( "_mtpl" ), Qt::CaseInsensitive );
}

bool isSensitiveMetadataKey( const QString &key )
{
  QString normalized;
  normalized.reserve( key.size() );
  for ( const QChar character : key )
  {
    if ( character.isLetterOrNumber() )
      normalized.append( character.toCaseFolded() );
  }
  return normalized == QLatin1String( "privatekey" ) ||
         normalized == QLatin1String( "privatekeybase64" ) ||
         normalized == QLatin1String( "devicekey" ) ||
         normalized == QLatin1String( "devicekeyhex" ) ||
         normalized == QLatin1String( "keymaterial" ) ||
         normalized == QLatin1String( "password" ) ||
         normalized == QLatin1String( "passphrase" ) ||
         normalized == QLatin1String( "secret" ) ||
         normalized == QLatin1String( "credentials" );
}

QVariant sanitizedMetadataValue( const QVariant &value );

QVariantMap sanitizedFreshMetadata( const QVariantMap &metadata )
{
  QVariantMap result;
  for ( auto it = metadata.constBegin(); it != metadata.constEnd(); ++it )
  {
    if ( !isInternalMetadataKey( it.key() ) && !isSensitiveMetadataKey( it.key() ) )
      result.insert( it.key(), sanitizedMetadataValue( it.value() ) );
  }
  return result;
}

QVariant sanitizedMetadataValue( const QVariant &value )
{
  if ( value.metaType().id() == QMetaType::QVariantMap )
    return sanitizedFreshMetadata( value.toMap() );
  if ( value.metaType().id() == QMetaType::QVariantList )
  {
    QVariantList result;
    const QVariantList values = value.toList();
    result.reserve( values.size() );
    for ( const QVariant &item : values )
      result.append( sanitizedMetadataValue( item ) );
    return result;
  }
  return value;
}

void mergeFreshDescriptorState( QgsMtpl::PackageDescriptor &saved,
                                const QgsMtpl::PackageDescriptor &fresh,
                                QgsMtpl::CredentialSource credentialSource )
{
  saved.metadata = sanitizedFreshMetadata( fresh.metadata );
  saved.readinessMessage = fresh.readinessMessage;
  if ( !fresh.sidecarPath.isEmpty() )
    saved.sidecarPath = fresh.sidecarPath;
  if ( !fresh.keyId.isEmpty() )
    saved.keyId = fresh.keyId;

  if ( fresh.encryption == QgsMtpl::EncryptionState::Plain )
  {
    saved.encryption = QgsMtpl::EncryptionState::Plain;
    saved.readiness = QgsMtpl::ReadinessState::PlainReady;
    saved.credentialSource = QgsMtpl::CredentialSource::None;
  }
  else
  {
    saved.encryption = QgsMtpl::EncryptionState::Encrypted;
    saved.readiness = fresh.isCredentialedEmptyPtp()
                        ? QgsMtpl::ReadinessState::UnverifiableEmpty
                        : QgsMtpl::ReadinessState::KeyVerified;
    saved.credentialSource = credentialSource;
  }
}

QVariantMap visibleMetadata( const QVariantMap &metadata )
{
  return sanitizedFreshMetadata( metadata );
}

QVariantMap commonDatasetMetadata( const QgsMtpl::TileDatasetDescriptor &dataset )
{
  if ( dataset.packages.isEmpty() )
    return QVariantMap();

  QVariantMap common = visibleMetadata( dataset.packages.constFirst().descriptor.metadata );
  for ( qsizetype packageIndex = 1; packageIndex < dataset.packages.size() && !common.isEmpty(); ++packageIndex )
  {
    const QVariantMap packageMetadata = visibleMetadata( dataset.packages.at( packageIndex ).descriptor.metadata );
    for ( auto it = common.begin(); it != common.end(); )
    {
      const auto packageValue = packageMetadata.constFind( it.key() );
      if ( packageValue == packageMetadata.constEnd() || packageValue.value() != it.value() )
        it = common.erase( it );
      else
        ++it;
    }
  }
  return common;
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

QString tileSchemeToXml( QgsMtpl::TileScheme scheme )
{
  return scheme == QgsMtpl::TileScheme::Tms ? QStringLiteral( "tms" ) : QStringLiteral( "xyz" );
}

QgsMtpl::TileScheme tileSchemeFromXml( const QString &value )
{
  return value.compare( QLatin1String( "tms" ), Qt::CaseInsensitive ) == 0 ? QgsMtpl::TileScheme::Tms : QgsMtpl::TileScheme::Xyz;
}

bool xmlIntAttribute( const QDomElement &element, const QString &name, int &value )
{
  if ( !element.hasAttribute( name ) )
    return false;
  bool ok = false;
  value = element.attribute( name ).toInt( &ok );
  return ok;
}

bool xmlUIntAttribute( const QDomElement &element, const QString &name, quint32 &value )
{
  if ( !element.hasAttribute( name ) )
    return false;
  bool ok = false;
  value = element.attribute( name ).toUInt( &ok );
  return ok;
}

bool xmlULongLongAttribute( const QDomElement &element, const QString &name, quint64 &value )
{
  if ( !element.hasAttribute( name ) )
    return false;
  bool ok = false;
  value = element.attribute( name ).toULongLong( &ok );
  return ok;
}

bool xmlLongLongAttribute( const QDomElement &element, const QString &name, qint64 &value )
{
  if ( !element.hasAttribute( name ) )
    return false;
  bool ok = false;
  value = element.attribute( name ).toLongLong( &ok );
  return ok;
}

bool xmlDoubleAttribute( const QDomElement &element, const QString &name, double &value )
{
  if ( !element.hasAttribute( name ) )
    return false;
  bool ok = false;
  value = element.attribute( name ).toDouble( &ok );
  return ok && std::isfinite( value );
}

bool xmlBoolAttribute( const QDomElement &element, const QString &name, bool &value )
{
  if ( !element.hasAttribute( name ) )
    return false;
  const QString text = element.attribute( name );
  if ( text != QLatin1String( "0" ) && text != QLatin1String( "1" ) )
    return false;
  value = text == QLatin1String( "1" );
  return true;
}

bool tileMatrixSetFromDefinition( const QgsMtpl::TileMatrixDefinition &definition,
                                  QgsTileMatrixSet &matrixSet,
                                  QString *error = nullptr )
{
  QString definitionError;
  if ( !definition.isValid( &definitionError ) )
  {
    if ( error )
      *error = definitionError;
    return false;
  }

  const QgsCoordinateReferenceSystem crs( definition.crsAuthId );
  if ( !crs.isValid() )
  {
    if ( error )
      *error = QObject::tr( "MTPL 瓦片矩阵使用了无效的 CRS：%1" ).arg( definition.crsAuthId );
    return false;
  }
  if ( definition.z0MatrixWidth > static_cast<quint64>( std::numeric_limits<int>::max() ) ||
       definition.z0MatrixHeight > static_cast<quint64>( std::numeric_limits<int>::max() ) )
  {
    if ( error )
      *error = QObject::tr( "MTPL 零级瓦片矩阵尺寸超出 QGIS 支持的范围。" );
    return false;
  }

  QgsTileMatrixSet result;
  const QgsPointXY topLeft( definition.topLeftX, definition.topLeftY );
  const int rootWidth = static_cast<int>( definition.z0MatrixWidth );
  const int rootHeight = static_cast<int>( definition.z0MatrixHeight );
  QgsTileMatrix rootMatrix = QgsTileMatrix::fromCustomDef( 0, crs, topLeft, definition.z0TileSpan, rootWidth, rootHeight );
  rootMatrix.setScale( rootMatrix.scale() * 256.0 / std::max( 1, definition.tileSize ) );
  result.setRootMatrix( rootMatrix );

  for ( int zoom = definition.minimumZoom; zoom <= definition.maximumZoom; ++zoom )
  {
    bool widthOk = false;
    bool heightOk = false;
    const quint64 width = definition.matrixWidth( zoom, &widthOk );
    const quint64 height = definition.matrixHeight( zoom, &heightOk );
    if ( !widthOk || !heightOk || width > static_cast<quint64>( std::numeric_limits<int>::max() ) ||
         height > static_cast<quint64>( std::numeric_limits<int>::max() ) )
    {
      if ( error )
        *error = QObject::tr( "MTPL 缩放级别 %1 的瓦片矩阵尺寸超出 QGIS 支持的范围。" ).arg( zoom );
      return false;
    }

    QgsTileMatrix matrix = QgsTileMatrix::fromCustomDef( zoom, crs, topLeft, definition.z0TileSpan, rootWidth, rootHeight );
    matrix.setScale( matrix.scale() * 256.0 / std::max( 1, definition.tileSize ) );
    result.addMatrix( matrix );
  }
  result.setScaleToTileZoomMethod( definition.scaleToZoomMethod.compare( QLatin1String( "esri" ), Qt::CaseInsensitive ) == 0
                                     ? Qgis::ScaleToTileZoomLevelMethod::Esri
                                     : Qgis::ScaleToTileZoomLevelMethod::MapBox );
  matrixSet = result;
  return true;
}

bool tileMatricesMatch( const QgsTileMatrix &left, const QgsTileMatrix &right )
{
  const QgsRectangle leftExtent = left.extent();
  const QgsRectangle rightExtent = right.extent();
  return left.zoomLevel() == right.zoomLevel() &&
         left.matrixWidth() == right.matrixWidth() &&
         left.matrixHeight() == right.matrixHeight() &&
         left.crs() == right.crs() &&
         qgsDoubleNear( left.scale(), right.scale() ) &&
         qgsDoubleNear( leftExtent.xMinimum(), rightExtent.xMinimum() ) &&
         qgsDoubleNear( leftExtent.yMinimum(), rightExtent.yMinimum() ) &&
         qgsDoubleNear( leftExtent.xMaximum(), rightExtent.xMaximum() ) &&
         qgsDoubleNear( leftExtent.yMaximum(), rightExtent.yMaximum() );
}

bool tileMatrixSetsMatch( const QgsTileMatrixSet &left, const QgsTileMatrixSet &right )
{
  if ( left.isEmpty() || right.isEmpty() || left.minimumZoom() != right.minimumZoom() ||
       left.maximumZoom() != right.maximumZoom() || !tileMatricesMatch( left.rootMatrix(), right.rootMatrix() ) ||
       left.scaleToTileZoomMethod() != right.scaleToTileZoomMethod() )
    return false;

  for ( int zoom = left.minimumZoom(); zoom <= left.maximumZoom(); ++zoom )
  {
    if ( !tileMatricesMatch( left.tileMatrix( zoom ), right.tileMatrix( zoom ) ) )
      return false;
  }
  return true;
}

bool tileMatrixDefinitionMatchesSnapshot( const QgsMtpl::TileMatrixDefinition &actual,
                                          const QgsMtpl::TileMatrixDefinition &expected,
                                          const QgsMtpl::TilePackageSourceContract *sourceContract = nullptr )
{
  const QgsCoordinateReferenceSystem actualCrs( actual.crsAuthId );
  const QgsCoordinateReferenceSystem expectedCrs( expected.crsAuthId );
  if ( !actualCrs.isValid() || !expectedCrs.isValid() || actualCrs != expectedCrs ||
       actual.scheme != expected.scheme || actual.tileSize != expected.tileSize ||
       !qgsDoubleNear( actual.topLeftX, expected.topLeftX ) ||
       !qgsDoubleNear( actual.topLeftY, expected.topLeftY ) ||
       !qgsDoubleNear( actual.z0TileSpan, expected.z0TileSpan ) ||
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
           !qgsDoubleNear( actual.boundsXMinimum, sourceContract->boundsXMinimum ) ||
           !qgsDoubleNear( actual.boundsYMinimum, sourceContract->boundsYMinimum ) ||
           !qgsDoubleNear( actual.boundsXMaximum, sourceContract->boundsXMaximum ) ||
           !qgsDoubleNear( actual.boundsYMaximum, sourceContract->boundsYMaximum ) )
        return false;
    }
  }
  else
  {
    // Older project files did not retain per-package declaration state. Keep
    // their previous compatibility behavior until a successful probe can
    // capture a complete source contract in memory.
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
           !qgsDoubleNear( actual.boundsXMinimum, expected.boundsXMinimum ) ||
           !qgsDoubleNear( actual.boundsYMinimum, expected.boundsYMinimum ) ||
           !qgsDoubleNear( actual.boundsXMaximum, expected.boundsXMaximum ) ||
           !qgsDoubleNear( actual.boundsYMaximum, expected.boundsYMaximum ) )
        return false;
    }
  }
  return true;
}

QString datasetDisplayName( const QgsMtpl::TileDatasetDescriptor &dataset )
{
  if ( !dataset.mapId.isEmpty() )
    return dataset.mapId;
  const QFileInfo sourceInfo( dataset.sourcePath );
  return dataset.directorySource ? sourceInfo.fileName() : sourceInfo.completeBaseName();
}

bool restorableDatasetIsValid( const QgsMtpl::TileDatasetDescriptor &dataset, QString *error )
{
  QgsMtpl::TileDatasetDescriptor validationDataset = dataset;
  for ( QgsMtpl::TileDatasetPackage &package : validationDataset.packages )
  {
    if ( package.descriptor.encryption != QgsMtpl::EncryptionState::Plain &&
         package.descriptor.readiness == QgsMtpl::ReadinessState::KeyRequired )
      package.descriptor.readiness = QgsMtpl::ReadinessState::KeyVerified;
  }
  return validationDataset.isValid( error );
}

QList<QgsMtpl::CryptoKeys> repeatedKeys( int count, const QgsMtpl::CryptoKeys &keys )
{
  QList<QgsMtpl::CryptoKeys> result;
  result.reserve( std::max( 0, count ) );
  for ( int i = 0; i < count; ++i )
    result.append( keys );
  return result;
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

bool metadataContains( const QVariantMap &metadata, const QStringList &names )
{
  for ( auto it = metadata.constBegin(); it != metadata.constEnd(); ++it )
  {
    if ( std::any_of( names.constBegin(), names.constEnd(), [&it]( const QString &name )
         {
           return it.key().compare( name, Qt::CaseInsensitive ) == 0;
         } ) )
      return true;
  }
  return false;
}

QgsMtpl::PayloadType metadataPayload( const QVariantMap &metadata )
{
  const QString type = metadataString(
    metadata,
    { QStringLiteral( "tile_file_ext" ), QStringLiteral( "tileFormat" ),
      QStringLiteral( "payload" ), QStringLiteral( "format" ), QStringLiteral( "type" ) },
    QString() ).trimmed().toLower();
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

QgsMtpl::TilePackageSourceContract sourceContract(
  const QgsMtpl::PackageDescriptor &descriptor,
  const QgsMtpl::TileMatrixDefinition &matrix )
{
  static const QStringList mapIdNames { QStringLiteral( "map_id" ), QStringLiteral( "mapId" ) };
  static const QStringList payloadNames {
    QStringLiteral( "tile_file_ext" ), QStringLiteral( "tileFormat" ),
    QStringLiteral( "payload" ), QStringLiteral( "format" ), QStringLiteral( "type" )
  };
  QgsMtpl::TilePackageSourceContract result;
  result.available = true;
  result.zoomRangeExplicit = matrix.zoomRangeExplicit;
  result.minimumZoom = matrix.minimumZoom;
  result.maximumZoom = matrix.maximumZoom;
  result.hasBounds = matrix.hasBounds;
  result.boundsXMinimum = matrix.boundsXMinimum;
  result.boundsYMinimum = matrix.boundsYMinimum;
  result.boundsXMaximum = matrix.boundsXMaximum;
  result.boundsYMaximum = matrix.boundsYMaximum;
  result.boundsCrsAuthId = matrix.boundsCrsAuthId;
  result.legacyFallbackApplied = matrix.legacyFallbackApplied;
  result.mapIdDeclared = metadataContains( descriptor.metadata, mapIdNames );
  result.mapId = metadataString( descriptor.metadata, mapIdNames, QString() ).trimmed();
  result.payloadDeclared = metadataContains( descriptor.metadata, payloadNames );
  result.payload = metadataPayload( descriptor.metadata );
  return result;
}

bool sourceMetadataMatchesContract( const QgsMtpl::PackageDescriptor &actual,
                                    const QgsMtpl::TilePackageSourceContract &expected )
{
  if ( !expected.available )
    return true;
  const QgsMtpl::TilePackageSourceContract actualContract = sourceContract(
    actual, QgsMtpl::TileMatrixDefinition() );
  return actualContract.mapIdDeclared == expected.mapIdDeclared &&
         ( !actualContract.mapIdDeclared || actualContract.mapId == expected.mapId ) &&
         actualContract.payloadDeclared == expected.payloadDeclared &&
         ( !actualContract.payloadDeclared || actualContract.payload == expected.payload );
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
  // A valid plain package may intentionally contain only empty tile slots.
  // Opening it and walking all declared ranges is sufficient validation.
  return descriptor.encryption == QgsMtpl::EncryptionState::Plain;
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

bool loadKeysFromSidecar( QgsMtpl::PackageDescriptor &descriptor, QgsMtpl::CryptoKeys &keys )
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
  keys = candidate;
  candidate.clear();
  descriptor.sidecarPath = sidecarPath;
  descriptor.keyId = sidecar.keys.keyId;
  return true;
}

bool restoreKeysFromSidecar( QgsMtpl::PackageDescriptor &descriptor, QgsMtpl::CryptoKeys &keys )
{
  if ( !loadKeysFromSidecar( descriptor, keys ) )
    return false;

  const bool usable = descriptor.format == QgsMtpl::PackageFormat::Sfp
                        ? sfpPackageKeysAreUsable( descriptor, keys )
                        : tilePackageKeysAreUsable( descriptor, keys );
  if ( !usable )
  {
    keys.clear();
    return false;
  }

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
      if ( mDescriptor.format == QgsMtpl::PackageFormat::Ptp &&
           mDescriptor.payload != QgsMtpl::PayloadType::RasterImage )
      {
        if ( mDescriptor.payload == QgsMtpl::PayloadType::Elevation )
          mErrors << QObject::tr( "PTP 仅支持影像瓦片，高程载荷请使用 DTP。" );
        else if ( mDescriptor.payload == QgsMtpl::PayloadType::VectorTile )
          mErrors << QObject::tr( "PTP 仅支持影像瓦片，PBF/MVT 矢量载荷请使用 VTP。" );
        else
          mErrors << QObject::tr( "PTP 尚未确认 PNG、JPEG 或 WebP 影像载荷。" );
        return false;
      }
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

      const bool mayContainVectorTiles = mDescriptor.format == QgsMtpl::PackageFormat::Vtp;
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
        if ( mDescriptor.format == QgsMtpl::PackageFormat::Dtp )
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

class QgsMtplDatasetRenderer final : public QgsMapLayerRenderer
{
  public:
    QgsMtplDatasetRenderer( const QString &layerId,
                            QgsRenderContext *context,
                            const QgsMtpl::TileDatasetDescriptor &dataset,
                            const QgsTileMatrixSet &matrixSet,
                            const QList<QgsMtpl::CryptoKeys> &packageKeys,
                            const QgsRectangle &layerExtent,
                            const QgsCoordinateReferenceSystem &layerCrs,
                            const std::shared_ptr<QgsMtplStructureValidationCache> &validationCache,
                            const std::shared_ptr<QMutex> &errorMutex,
                            const std::shared_ptr<QHash<QString, qint64>> &errorLastReported )
      : QgsMapLayerRenderer( layerId, context )
      , mDataset( dataset )
      , mMatrixSet( matrixSet )
      , mPackageKeys( packageKeys )
      , mLayerExtent( layerExtent )
      , mLayerCrs( layerCrs )
      , mValidationCache( validationCache )
      , mErrorMutex( errorMutex )
      , mErrorLastReported( errorLastReported )
      , mFeedback( std::make_unique<QgsRasterBlockFeedback>() )
    {}

    ~QgsMtplDatasetRenderer() override
    {
      for ( QgsMtpl::CryptoKeys &keys : mPackageKeys )
        keys.clear();
    }

    QgsFeedback *feedback() const override
    {
      return mFeedback.get();
    }

    bool render() override
    {
      QgsRenderContext *context = renderContext();
      if ( !context || !context->painter() || mMatrixSet.isEmpty() )
        return false;
      if ( mFeedback->isCanceled() || context->renderingStopped() )
        return false;

      const double tileScale = mMatrixSet.scaleForRenderContext( *context );
      if ( !std::isfinite( tileScale ) || tileScale <= 0 )
      {
        mErrors << QObject::tr( "MTPL 数据集无法使用无效的地图比例尺渲染。" );
        return false;
      }
      const int minimumZoom = mMatrixSet.minimumZoom();
      const int maximumZoom = mMatrixSet.maximumZoom();
      const double scaleDivisor = mMatrixSet.scaleToTileZoomMethod() == Qgis::ScaleToTileZoomLevelMethod::MapBox ? 2.0 : 1.0;
      const double minimumScale = mMatrixSet.tileMatrix( maximumZoom ).scale() / scaleDivisor;
      const double maximumScale = mMatrixSet.tileMatrix( minimumZoom ).scale() / scaleDivisor;
      // Clamp before interpolation. The generic overzoom interpolation can loop
      // indefinitely at exact binary scale boundaries beyond its last matrix.
      const int selectedZoom = tileScale <= minimumScale ? maximumZoom
                                 : tileScale >= maximumScale ? minimumZoom
                                 : mMatrixSet.scaleToZoomLevel( tileScale );
      const QgsTileMatrix selectedMatrix = mMatrixSet.tileMatrix( selectedZoom );
      if ( selectedMatrix.zoomLevel() != selectedZoom )
      {
        mErrors << QObject::tr( "MTPL 数据集缺少当前比例尺需要的瓦片矩阵。" );
        return false;
      }

      QgsMapToPixel mapToPixel = context->mapToPixel();
      if ( mapToPixel.mapRotation() )
      {
        const QgsPointXY center = mapToPixel.toMapCoordinates( static_cast<int>( mapToPixel.mapWidth() / 2.0 ),
                                                               static_cast<int>( mapToPixel.mapHeight() / 2.0 ) );
        mapToPixel.setMapRotation( 0, center.x(), center.y() );
      }

      const QgsRectangle viewExtentInMapCrs = context->mapExtent();
      QgsRectangle layerExtentInMapCrs = mLayerExtent;
      const bool requiresTransform = context->coordinateTransform().isValid() &&
                                     !context->coordinateTransform().isShortCircuited();
      if ( requiresTransform )
      {
        try
        {
          QgsCoordinateTransform transform = context->coordinateTransform();
          transform.setBallparkTransformsAreAppropriate( true );
          layerExtentInMapCrs = transform.transformBoundingBox( mLayerExtent );
        }
        catch ( QgsCsException & )
        {
          layerExtentInMapCrs.setNull();
        }
      }

      QgsRectangle visibleExtent = viewExtentInMapCrs.intersect( layerExtentInMapCrs );
      if ( visibleExtent.isEmpty() && requiresTransform )
      {
        // A global layer extent can be impossible to transform as one bounding
        // box into a local projection. Mirror the native raster renderer's
        // inverse fallback by clipping in the layer CRS first.
        try
        {
          QgsCoordinateTransform transform = context->coordinateTransform();
          transform.setBallparkTransformsAreAppropriate( true );
          const QgsRectangle viewExtentInLayerCrs = transform.transformBoundingBox(
            context->mapExtent(), Qgis::TransformDirection::Reverse );
          const QgsRectangle visibleExtentInLayerCrs = viewExtentInLayerCrs.intersect( mLayerExtent );
          if ( !visibleExtentInLayerCrs.isEmpty() )
            visibleExtent = transform.transformBoundingBox( visibleExtentInLayerCrs );
        }
        catch ( QgsCsException &exception )
        {
          mErrors << QObject::tr( "无法将 MTPL 数据集范围转换到地图 CRS：%1" ).arg( exception.what() );
          return false;
        }
      }
      if ( visibleExtent.isEmpty() )
        return true;

      QgsRasterViewPort viewport;
      viewport.mDrawnExtent = visibleExtent;
      viewport.mSrcCRS = mLayerCrs;
      viewport.mDestCRS = context->coordinateTransform().isValid()
                                ? context->coordinateTransform().destinationCrs()
                                : mLayerCrs;
      viewport.mTransformContext = context->transformContext();
      viewport.mTopLeftPoint = mapToPixel.transform( visibleExtent.xMinimum(), visibleExtent.yMaximum() );
      viewport.mBottomRightPoint = mapToPixel.transform( visibleExtent.xMaximum(), visibleExtent.yMinimum() );
      viewport.mTopLeftPoint.setX( std::floor( viewport.mTopLeftPoint.x() ) );
      viewport.mTopLeftPoint.setY( std::floor( viewport.mTopLeftPoint.y() ) );
      viewport.mBottomRightPoint.setX( std::ceil( viewport.mBottomRightPoint.x() ) );
      viewport.mBottomRightPoint.setY( std::ceil( viewport.mBottomRightPoint.y() ) );
      viewport.mDrawnExtent.set(
        mapToPixel.toMapCoordinates( viewport.mTopLeftPoint.x(), viewport.mBottomRightPoint.y() ),
        mapToPixel.toMapCoordinates( viewport.mBottomRightPoint.x(), viewport.mTopLeftPoint.y() ) );
      viewport.mWidth = static_cast<qgssize>( std::abs( viewport.mBottomRightPoint.x() - viewport.mTopLeftPoint.x() ) );
      viewport.mHeight = static_cast<qgssize>( std::abs( viewport.mBottomRightPoint.y() - viewport.mTopLeftPoint.y() ) );
      if ( viewport.mWidth <= 0 || viewport.mHeight <= 0 )
        return true;

      QgsMtplRasterSourceProvider source( mDataset, mMatrixSet, mPackageKeys, selectedZoom, mValidationCache );
      QgsRasterProjector projector;
      projector.setInput( &source );
      projector.setCrs( viewport.mSrcCRS, viewport.mDestCRS, viewport.mTransformContext );

      QgsRasterIterator iterator( &projector );
      QgsRasterDrawer drawer( &iterator );
      const QgsScopedQPainterState painterState( context->painter() );
      context->painter()->setRenderHint( QPainter::SmoothPixmapTransform, false );
      context->painter()->setRenderHint( QPainter::Antialiasing, false );
      drawer.draw( *context, &viewport, mFeedback.get() );
      const bool canceled = mFeedback->isCanceled() || context->renderingStopped();
      // Canvas preview jobs do not surface renderer errors to the user. Do not
      // let an invisible preview consume the shared diagnostic throttle before
      // the corresponding main render can report it. Canceled jobs must not
      // consume that throttle either.
      if ( !canceled && !context->testFlag( Qgis::RenderContextFlag::RenderPreviewJob ) )
        appendThrottledErrors( source.errors() );
      return !canceled;
    }

  private:
    void appendThrottledErrors( const QStringList &errors )
    {
      if ( errors.isEmpty() )
        return;
      if ( !mErrorMutex || !mErrorLastReported )
      {
        mErrors.append( errors );
        return;
      }

      constexpr qint64 repeatWindowMs = 5000;
      const qint64 now = QDateTime::currentMSecsSinceEpoch();
      QMutexLocker locker( mErrorMutex.get() );
      for ( const QString &error : errors )
      {
        const auto previous = mErrorLastReported->constFind( error );
        if ( previous == mErrorLastReported->constEnd() || now - previous.value() >= repeatWindowMs )
        {
          mErrors.append( error );
          mErrorLastReported->insert( error, now );
        }
      }
      if ( mErrorLastReported->size() > 64 )
      {
        for ( auto it = mErrorLastReported->begin(); it != mErrorLastReported->end(); )
        {
          if ( now - it.value() >= repeatWindowMs )
            it = mErrorLastReported->erase( it );
          else
            ++it;
        }
      }
    }

    QgsMtpl::TileDatasetDescriptor mDataset;
    QgsTileMatrixSet mMatrixSet;
    QList<QgsMtpl::CryptoKeys> mPackageKeys;
    QgsRectangle mLayerExtent;
    QgsCoordinateReferenceSystem mLayerCrs;
    std::shared_ptr<QgsMtplStructureValidationCache> mValidationCache;
    std::shared_ptr<QMutex> mErrorMutex;
    std::shared_ptr<QHash<QString, qint64>> mErrorLastReported;
    std::unique_ptr<QgsRasterBlockFeedback> mFeedback;
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
  , mStructureValidationCache( std::make_shared<QgsMtplStructureValidationCache>() )
  , mRenderErrorMutex( std::make_shared<QMutex>() )
  , mRenderErrorLastReported( std::make_shared<QHash<QString, qint64>>() )
{
  applyDescriptor();
}

QgsMtplPluginLayer::QgsMtplPluginLayer( const QgsMtpl::TileDatasetDescriptor &dataset, const QList<QgsMtpl::CryptoKeys> &packageKeys )
  : QgsPluginLayer( layerTypeKey(), datasetDisplayName( dataset ) )
  , mDataset( std::make_shared<const QgsMtpl::TileDatasetDescriptor>( dataset ) )
  , mDatasetKeys( packageKeys )
  , mStructureValidationCache( std::make_shared<QgsMtplStructureValidationCache>() )
  , mRenderErrorMutex( std::make_shared<QMutex>() )
  , mRenderErrorLastReported( std::make_shared<QHash<QString, qint64>>() )
{
  while ( mDatasetKeys.size() < dataset.packages.size() )
    mDatasetKeys.append( QgsMtpl::CryptoKeys() );
  if ( mDatasetKeys.size() > dataset.packages.size() )
    mDatasetKeys = mDatasetKeys.mid( 0, dataset.packages.size() );
  if ( !dataset.packages.isEmpty() )
    mDescriptor = dataset.packages.constFirst().descriptor;
  applyDataset();
}

QgsMtplPluginLayer::QgsMtplPluginLayer( const QgsMtpl::TileDatasetDescriptor &dataset, const QgsMtpl::CryptoKeys &sharedKeys )
  : QgsMtplPluginLayer( dataset, repeatedKeys( dataset.packages.size(), sharedKeys ) )
{}

QgsMtplPluginLayer::~QgsMtplPluginLayer()
{
  clearCryptoKeys();
}

QgsMtplPluginLayer *QgsMtplPluginLayer::clone() const
{
  QgsMtplPluginLayer *layer = nullptr;
  if ( mDataset )
  {
    layer = new QgsMtplPluginLayer( *mDataset, mDatasetKeys );
    layer->mTileMatrixSet = mTileMatrixSet;
    layer->mTransformContext = mTransformContext;
  }
  else
  {
    layer = new QgsMtplPluginLayer( mDescriptor, mKeys );
  }
  layer->mStructureValidationCache = mStructureValidationCache;
  layer->mRenderErrorMutex = mRenderErrorMutex;
  layer->mRenderErrorLastReported = mRenderErrorLastReported;
  QgsMapLayer::clone( layer );
  if ( layer->mDataset )
    layer->applyDataset();
  else
    layer->applyDescriptor();
  return layer;
}

QgsMapLayerRenderer *QgsMtplPluginLayer::createMapRenderer( QgsRenderContext &rendererContext )
{
  if ( mDataset )
    return new QgsMtplDatasetRenderer( id(), &rendererContext, *mDataset, mTileMatrixSet, mDatasetKeys, extent(), crs(),
                                       mStructureValidationCache, mRenderErrorMutex, mRenderErrorLastReported );
  return new QgsMtplPackageRenderer( id(), &rendererContext, mDescriptor, mKeys );
}

void QgsMtplPluginLayer::reload()
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  if ( mStructureValidationCache )
    mStructureValidationCache->invalidate();
  if ( mRenderErrorMutex && mRenderErrorLastReported )
  {
    QMutexLocker locker( mRenderErrorMutex.get() );
    mRenderErrorLastReported->clear();
  }
  triggerRepaint();
}

bool QgsMtplPluginLayer::isSpatial() const
{
  return mDataset ? true : mDescriptor.isSpatial();
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
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  mTransformContext = transformContext;
  invalidateWgs84Extent();
  if ( mDataset )
    applyDataset();
}

const QgsMtpl::PackageDescriptor &QgsMtplPluginLayer::descriptor() const
{
  return mDescriptor;
}

void QgsMtplPluginLayer::setDescriptor( const QgsMtpl::PackageDescriptor &descriptor )
{
  mDataset.reset();
  mDatasetKeys.clear();
  mTileMatrixSet = QgsTileMatrixSet();
  mStructureValidationCache = std::make_shared<QgsMtplStructureValidationCache>();
  mDescriptor = descriptor;
  applyDescriptor();
  triggerRepaint();
}

void QgsMtplPluginLayer::setCryptoKeys( const QgsMtpl::CryptoKeys &keys )
{
  clearCryptoKeys();
  if ( mStructureValidationCache )
    mStructureValidationCache->invalidate();
  if ( mDataset )
  {
    mDatasetKeys = repeatedKeys( mDataset->packages.size(), keys );
    triggerRepaint();
    return;
  }
  mKeys = keys;
  if ( mKeys.isValid() && mDescriptor.encryption == QgsMtpl::EncryptionState::Locked )
    mDescriptor.encryption = QgsMtpl::EncryptionState::Encrypted;
  triggerRepaint();
}

void QgsMtplPluginLayer::clearCryptoKeys()
{
  mKeys.clear();
  for ( QgsMtpl::CryptoKeys &keys : mDatasetKeys )
    keys.clear();
  if ( mDescriptor.encryption == QgsMtpl::EncryptionState::Encrypted )
    mDescriptor.encryption = QgsMtpl::EncryptionState::Locked;
}

bool QgsMtplPluginLayer::hasCryptoKeys() const
{
  if ( mKeys.isValid() )
    return true;
  return std::any_of( mDatasetKeys.cbegin(), mDatasetKeys.cend(), []( const QgsMtpl::CryptoKeys &keys ) { return keys.isValid(); } );
}

bool QgsMtplPluginLayer::isTileDataset() const
{
  return static_cast<bool>( mDataset );
}

const QgsMtpl::TileDatasetDescriptor *QgsMtplPluginLayer::tileDataset() const
{
  return mDataset.get();
}

bool QgsMtplPluginLayer::replaceTileDataset( const QgsMtpl::TileDatasetDescriptor &dataset,
                                             const QList<QgsMtpl::CryptoKeys> &packageKeys,
                                             QString *error )
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  QString validationError;
  if ( !dataset.isValid( &validationError ) )
  {
    if ( error )
      *error = validationError;
    return false;
  }
  if ( packageKeys.size() != dataset.packages.size() )
  {
    if ( error )
      *error = tr( "PTP 数据集的密钥数量与数据包数量不一致。" );
    return false;
  }

  QgsMtpl::TileMatrixDefinition renderingDefinition = dataset.matrix;
  renderingDefinition.minimumZoom = dataset.minimumZoom;
  renderingDefinition.maximumZoom = dataset.maximumZoom;
  QgsTileMatrixSet candidateMatrixSet;
  if ( !tileMatrixSetFromDefinition( renderingDefinition, candidateMatrixSet, &validationError ) )
  {
    if ( error )
      *error = validationError;
    return false;
  }
  candidateMatrixSet.dropMatricesOutsideZoomRange( dataset.minimumZoom, dataset.maximumZoom );
  if ( candidateMatrixSet.isEmpty() || !candidateMatrixSet.crs().isValid() )
  {
    if ( error )
      *error = tr( "PTP 数据集没有可供 QGIS 使用的瓦片矩阵。" );
    return false;
  }

  // Everything above is side-effect free. Commit the fully validated snapshot
  // in one GUI-thread operation so a failed refresh leaves the current layer
  // and its credentials untouched.
  QList<QgsMtpl::CryptoKeys> replacementKeys = packageKeys;
  const std::shared_ptr<const QgsMtpl::TileDatasetDescriptor> replacementDataset =
    std::make_shared<const QgsMtpl::TileDatasetDescriptor>( dataset );
  QList<QgsMtpl::CryptoKeys> oldKeys = std::move( mDatasetKeys );
  mDatasetKeys = std::move( replacementKeys );
  mDataset = replacementDataset;
  mTileMatrixSet = candidateMatrixSet;
  mDescriptor = dataset.packages.constFirst().descriptor;
  mKeys.clear();
  // A replacement is a new logical snapshot. Give it a fresh cache so an
  // older renderer or a clone which still owns the previous snapshot cannot
  // repopulate validation results under the same path and file stamp.
  mStructureValidationCache = std::make_shared<QgsMtplStructureValidationCache>();
  mRenderErrorMutex = std::make_shared<QMutex>();
  mRenderErrorLastReported = std::make_shared<QHash<QString, qint64>>();
  applyDataset();

  for ( QgsMtpl::CryptoKeys &keys : oldKeys )
    keys.clear();
  triggerRepaint();
  if ( error )
    error->clear();
  return true;
}

const QgsTileMatrixSet &QgsMtplPluginLayer::tileMatrixSet() const
{
  return mTileMatrixSet;
}

quint64 QgsMtplPluginLayer::structuralValidationAttemptCount() const
{
  return mStructureValidationCache ? mStructureValidationCache->validationAttemptCount() : 0;
}

bool QgsMtplPluginLayer::readXml( const QDomNode &layerNode, QgsReadWriteContext &context )
{
  if ( !QgsMapLayer::readXml( layerNode, context ) )
    return false;

  mDataset.reset();
  mDatasetKeys.clear();
  mTileMatrixSet = QgsTileMatrixSet();
  mStructureValidationCache = std::make_shared<QgsMtplStructureValidationCache>();
  mKeys.clear();
  mDescriptor = QgsMtpl::PackageDescriptor();

  const QDomElement datasetElement = layerNode.firstChildElement( QStringLiteral( "mtpl-dataset" ) );
  if ( !datasetElement.isNull() )
  {
    if ( datasetElement.attribute( QStringLiteral( "version" ) ) != QLatin1String( "1" ) )
      return false;

    QgsMtpl::TileDatasetDescriptor dataset;
    dataset.sourcePath = source();
    if ( dataset.sourcePath.isEmpty() )
      dataset.sourcePath = context.pathResolver().readPath( datasetElement.attribute( QStringLiteral( "source" ) ) );
    if ( !datasetElement.hasAttribute( QStringLiteral( "minimumZoom" ) ) ||
         !datasetElement.hasAttribute( QStringLiteral( "maximumZoom" ) ) )
      return false;
    if ( !xmlBoolAttribute( datasetElement, QStringLiteral( "directorySource" ), dataset.directorySource ) ||
         !xmlBoolAttribute( datasetElement, QStringLiteral( "compatibilityMode" ), dataset.compatibilityMode ) ||
         !xmlBoolAttribute( datasetElement, QStringLiteral( "hasExtent" ), dataset.hasExtent ) )
      return false;
    if ( !datasetElement.hasAttribute( QStringLiteral( "payload" ) ) )
      return false;
    const QString datasetPayloadValue = datasetElement.attribute( QStringLiteral( "payload" ) ).trimmed().toLower();
    if ( datasetPayloadValue != QLatin1String( "raster" ) && datasetPayloadValue != QLatin1String( "elevation" ) &&
         datasetPayloadValue != QLatin1String( "unknown" ) )
      return false;
    dataset.payload = payloadFromXml( datasetElement.attribute( QStringLiteral( "payload" ) ) );
    dataset.mapId = datasetElement.attribute( QStringLiteral( "mapId" ) );
    bool datasetMinimumOk = false;
    bool datasetMaximumOk = false;
    dataset.minimumZoom = datasetElement.attribute( QStringLiteral( "minimumZoom" ) ).toInt( &datasetMinimumOk );
    dataset.maximumZoom = datasetElement.attribute( QStringLiteral( "maximumZoom" ) ).toInt( &datasetMaximumOk );
    if ( !datasetMinimumOk || !datasetMaximumOk )
      return false;
    if ( dataset.hasExtent )
    {
      if ( !xmlDoubleAttribute( datasetElement, QStringLiteral( "extentXMinimum" ), dataset.extentXMinimum ) ||
           !xmlDoubleAttribute( datasetElement, QStringLiteral( "extentYMinimum" ), dataset.extentYMinimum ) ||
           !xmlDoubleAttribute( datasetElement, QStringLiteral( "extentXMaximum" ), dataset.extentXMaximum ) ||
           !xmlDoubleAttribute( datasetElement, QStringLiteral( "extentYMaximum" ), dataset.extentYMaximum ) )
        return false;
    }

    const QDomElement ruleElement = datasetElement.firstChildElement( QStringLiteral( "partition-rule" ) );
    if ( !dataset.compatibilityMode && ruleElement.isNull() )
      return false;
    if ( !ruleElement.isNull() )
    {
      if ( !ruleElement.hasAttribute( QStringLiteral( "id" ) ) ||
           !ruleElement.hasAttribute( QStringLiteral( "name" ) ) ||
           !xmlBoolAttribute( ruleElement, QStringLiteral( "builtIn" ), dataset.partitionRule.builtIn ) )
        return false;
      dataset.partitionRule.id = ruleElement.attribute( QStringLiteral( "id" ) );
      dataset.partitionRule.name = ruleElement.attribute( QStringLiteral( "name" ) );
      for ( QDomElement bandElement = ruleElement.firstChildElement( QStringLiteral( "band" ) );
            !bandElement.isNull();
            bandElement = bandElement.nextSiblingElement( QStringLiteral( "band" ) ) )
      {
        QgsMtpl::PartitionBand band;
        if ( !xmlIntAttribute( bandElement, QStringLiteral( "minZoom" ), band.minZoom ) ||
             !xmlIntAttribute( bandElement, QStringLiteral( "maxZoom" ), band.maxZoom ) ||
             !xmlIntAttribute( bandElement, QStringLiteral( "baseZoom" ), band.baseZoom ) )
          return false;
        dataset.partitionRule.bands.append( band );
      }
    }

    const QDomElement definitionElement = datasetElement.firstChildElement( QStringLiteral( "tile-matrix" ) );
    if ( definitionElement.isNull() || definitionElement.attribute( QStringLiteral( "version" ) ) != QLatin1String( "1" ) )
      return false;
    const QStringList requiredMatrixAttributes = {
      QStringLiteral( "crs" ), QStringLiteral( "scheme" ),
      QStringLiteral( "tileSize" ), QStringLiteral( "topLeftX" ), QStringLiteral( "topLeftY" ),
      QStringLiteral( "z0TileSpan" ), QStringLiteral( "z0MatrixWidth" ), QStringLiteral( "z0MatrixHeight" ),
      QStringLiteral( "scaleToZoomMethod" ), QStringLiteral( "minimumZoom" ), QStringLiteral( "maximumZoom" ),
      QStringLiteral( "hasBounds" ), QStringLiteral( "zoomRangeExplicit" ), QStringLiteral( "legacyFallbackApplied" )
    };
    for ( const QString &attribute : requiredMatrixAttributes )
    {
      if ( !definitionElement.hasAttribute( attribute ) )
        return false;
    }
    const QString schemeValue = definitionElement.attribute( QStringLiteral( "scheme" ) ).trimmed().toLower();
    if ( schemeValue != QLatin1String( "xyz" ) && schemeValue != QLatin1String( "tms" ) )
      return false;

    QgsMtpl::TileMatrixDefinition &definition = dataset.matrix;
    definition.crsAuthId = definitionElement.attribute( QStringLiteral( "crs" ) ).trimmed();
    definition.scheme = tileSchemeFromXml( schemeValue );
    if ( definition.crsAuthId.isEmpty() ||
         !xmlIntAttribute( definitionElement, QStringLiteral( "tileSize" ), definition.tileSize ) ||
         !xmlDoubleAttribute( definitionElement, QStringLiteral( "topLeftX" ), definition.topLeftX ) ||
         !xmlDoubleAttribute( definitionElement, QStringLiteral( "topLeftY" ), definition.topLeftY ) ||
         !xmlDoubleAttribute( definitionElement, QStringLiteral( "z0TileSpan" ), definition.z0TileSpan ) ||
         !xmlULongLongAttribute( definitionElement, QStringLiteral( "z0MatrixWidth" ), definition.z0MatrixWidth ) ||
         !xmlULongLongAttribute( definitionElement, QStringLiteral( "z0MatrixHeight" ), definition.z0MatrixHeight ) ||
         !xmlIntAttribute( definitionElement, QStringLiteral( "minimumZoom" ), definition.minimumZoom ) ||
         !xmlIntAttribute( definitionElement, QStringLiteral( "maximumZoom" ), definition.maximumZoom ) ||
         !xmlBoolAttribute( definitionElement, QStringLiteral( "hasBounds" ), definition.hasBounds ) ||
         !xmlBoolAttribute( definitionElement, QStringLiteral( "zoomRangeExplicit" ), definition.zoomRangeExplicit ) ||
         !xmlBoolAttribute( definitionElement, QStringLiteral( "legacyFallbackApplied" ), definition.legacyFallbackApplied ) )
      return false;
    definition.scaleToZoomMethod = definitionElement.attribute( QStringLiteral( "scaleToZoomMethod" ) );
    if ( definition.hasBounds )
    {
      if ( !xmlDoubleAttribute( definitionElement, QStringLiteral( "boundsXMinimum" ), definition.boundsXMinimum ) ||
           !xmlDoubleAttribute( definitionElement, QStringLiteral( "boundsYMinimum" ), definition.boundsYMinimum ) ||
           !xmlDoubleAttribute( definitionElement, QStringLiteral( "boundsXMaximum" ), definition.boundsXMaximum ) ||
           !xmlDoubleAttribute( definitionElement, QStringLiteral( "boundsYMaximum" ), definition.boundsYMaximum ) ||
           !definitionElement.hasAttribute( QStringLiteral( "boundsCrs" ) ) )
        return false;
      definition.boundsCrsAuthId = definitionElement.attribute( QStringLiteral( "boundsCrs" ) ).trimmed();
    }
    if ( dataset.minimumZoom < definition.minimumZoom || dataset.maximumZoom > definition.maximumZoom )
      return false;

    const QString packageBasePath = dataset.directorySource ? dataset.sourcePath : QFileInfo( dataset.sourcePath ).absolutePath();
    const QDomElement packagesElement = datasetElement.firstChildElement( QStringLiteral( "packages" ) );
    if ( packagesElement.isNull() )
      return false;
    QSet<int> sourceStampChangedPackages;
    for ( QDomElement packageElement = packagesElement.firstChildElement( QStringLiteral( "package" ) );
          !packageElement.isNull();
          packageElement = packageElement.nextSiblingElement( QStringLiteral( "package" ) ) )
    {
      QgsMtpl::TileDatasetPackage package;
      if ( !packageElement.hasAttribute( QStringLiteral( "relativePath" ) ) ||
           !packageElement.hasAttribute( QStringLiteral( "path" ) ) ||
           !packageElement.hasAttribute( QStringLiteral( "displayName" ) ) ||
           !packageElement.hasAttribute( QStringLiteral( "format" ) ) ||
           !packageElement.hasAttribute( QStringLiteral( "payload" ) ) ||
           !packageElement.hasAttribute( QStringLiteral( "storage" ) ) )
        return false;
      package.relativePath = packageElement.attribute( QStringLiteral( "relativePath" ) );
      const QString normalizedRelativePath = QDir::cleanPath( QDir::fromNativeSeparators( package.relativePath ) );
      if ( dataset.directorySource &&
           ( package.relativePath.isEmpty() || QDir::isAbsolutePath( package.relativePath ) ||
             normalizedRelativePath == QLatin1String( ".." ) || normalizedRelativePath.startsWith( QLatin1String( "../" ) ) ) )
        return false;
      const QString fallbackPath = context.pathResolver().readPath( packageElement.attribute( QStringLiteral( "path" ) ) );
      package.descriptor.path = !package.relativePath.isEmpty()
                                  ? QDir( packageBasePath ).absoluteFilePath( package.relativePath )
                                  : fallbackPath;
      if ( package.descriptor.path.isEmpty() && dataset.compatibilityMode )
        package.descriptor.path = dataset.sourcePath;
      package.descriptor.path = QDir::cleanPath( package.descriptor.path );
      package.descriptor.displayName = packageElement.attribute( QStringLiteral( "displayName" ), QFileInfo( package.descriptor.path ).completeBaseName() );
      package.descriptor.format = formatFromXml( packageElement.attribute( QStringLiteral( "format" ) ) );
      if ( package.descriptor.format != QgsMtpl::PackageFormat::Ptp )
        return false;
      const QString packagePayloadValue = packageElement.attribute( QStringLiteral( "payload" ) ).trimmed().toLower();
      if ( packagePayloadValue != QLatin1String( "raster" ) && packagePayloadValue != QLatin1String( "elevation" ) &&
           packagePayloadValue != QLatin1String( "unknown" ) )
        return false;
      package.descriptor.payload = payloadFromXml( packagePayloadValue );
      const QString storage = packageElement.attribute( QStringLiteral( "storage" ) ).trimmed().toLower();
      if ( storage != QLatin1String( "plain" ) && storage != QLatin1String( "encrypted" ) )
        return false;
      package.descriptor.encryption = storage == QLatin1String( "encrypted" )
                                        ? QgsMtpl::EncryptionState::Locked
                                        : QgsMtpl::EncryptionState::Plain;
      package.descriptor.readiness = package.descriptor.encryption == QgsMtpl::EncryptionState::Plain
                                       ? QgsMtpl::ReadinessState::PlainReady
                                       : QgsMtpl::ReadinessState::KeyRequired;
      package.descriptor.sidecarPath = context.pathResolver().readPath( packageElement.attribute( QStringLiteral( "sidecar" ) ) );
      package.descriptor.keyId = packageElement.attribute( QStringLiteral( "keyId" ) );
      package.descriptor.crsAuthId = definition.crsAuthId;
      package.descriptor.scheme = tileSchemeToXml( definition.scheme );
      package.descriptor.stylePath = context.pathResolver().readPath( packageElement.attribute( QStringLiteral( "style" ) ) );
      if ( !xmlULongLongAttribute( packageElement, QStringLiteral( "fileSize" ), package.descriptor.fileSize ) ||
           package.descriptor.fileSize == 0 ||
           !xmlLongLongAttribute( packageElement, QStringLiteral( "lastModifiedMs" ), package.descriptor.fileLastModifiedMs ) ||
           package.descriptor.fileLastModifiedMs < 0 )
        return false;
      const QFileInfo currentFile( package.descriptor.path );
      if ( currentFile.exists() )
      {
        if ( !currentFile.isFile() )
          return false;
        if ( static_cast<quint64>( currentFile.size() ) != package.descriptor.fileSize ||
             currentFile.lastModified().toMSecsSinceEpoch() != package.descriptor.fileLastModifiedMs )
          sourceStampChangedPackages.insert( dataset.packages.size() );
      }
      package.descriptor.tileSize = definition.tileSize;
      if ( !xmlIntAttribute( packageElement, QStringLiteral( "minimumZoom" ), package.descriptor.minimumZoom ) ||
           !xmlIntAttribute( packageElement, QStringLiteral( "maximumZoom" ), package.descriptor.maximumZoom ) ||
           !xmlDoubleAttribute( packageElement, QStringLiteral( "scale" ), package.descriptor.scale ) ||
           !xmlDoubleAttribute( packageElement, QStringLiteral( "offset" ), package.descriptor.offset ) ||
           !xmlBoolAttribute( packageElement, QStringLiteral( "hasNoData" ), package.descriptor.hasNoData ) ||
           !xmlBoolAttribute( packageElement, QStringLiteral( "displayOverridesApplied" ), package.descriptor.displayOverridesApplied ) )
        return false;
      if ( package.descriptor.hasNoData &&
           !xmlDoubleAttribute( packageElement, QStringLiteral( "noData" ), package.descriptor.noData ) )
        return false;

      const QDomElement sourceContractElement = packageElement.firstChildElement( QStringLiteral( "source-contract" ) );
      if ( !sourceContractElement.isNull() )
      {
        QgsMtpl::TilePackageSourceContract &sourceContractSnapshot = package.sourceContract;
        if ( sourceContractElement.attribute( QStringLiteral( "version" ) ) != QLatin1String( "1" ) ||
             !xmlBoolAttribute( sourceContractElement, QStringLiteral( "zoomRangeExplicit" ), sourceContractSnapshot.zoomRangeExplicit ) ||
             !xmlBoolAttribute( sourceContractElement, QStringLiteral( "hasBounds" ), sourceContractSnapshot.hasBounds ) ||
             !xmlBoolAttribute( sourceContractElement, QStringLiteral( "legacyFallbackApplied" ), sourceContractSnapshot.legacyFallbackApplied ) ||
             !xmlBoolAttribute( sourceContractElement, QStringLiteral( "mapIdDeclared" ), sourceContractSnapshot.mapIdDeclared ) ||
             !xmlBoolAttribute( sourceContractElement, QStringLiteral( "payloadDeclared" ), sourceContractSnapshot.payloadDeclared ) )
          return false;
        sourceContractSnapshot.available = true;
        if ( sourceContractSnapshot.zoomRangeExplicit &&
             ( !xmlIntAttribute( sourceContractElement, QStringLiteral( "minimumZoom" ), sourceContractSnapshot.minimumZoom ) ||
               !xmlIntAttribute( sourceContractElement, QStringLiteral( "maximumZoom" ), sourceContractSnapshot.maximumZoom ) ) )
          return false;
        if ( sourceContractSnapshot.hasBounds )
        {
          if ( !xmlDoubleAttribute( sourceContractElement, QStringLiteral( "boundsXMinimum" ), sourceContractSnapshot.boundsXMinimum ) ||
               !xmlDoubleAttribute( sourceContractElement, QStringLiteral( "boundsYMinimum" ), sourceContractSnapshot.boundsYMinimum ) ||
               !xmlDoubleAttribute( sourceContractElement, QStringLiteral( "boundsXMaximum" ), sourceContractSnapshot.boundsXMaximum ) ||
               !xmlDoubleAttribute( sourceContractElement, QStringLiteral( "boundsYMaximum" ), sourceContractSnapshot.boundsYMaximum ) ||
               !sourceContractElement.hasAttribute( QStringLiteral( "boundsCrs" ) ) )
            return false;
          sourceContractSnapshot.boundsCrsAuthId = sourceContractElement.attribute( QStringLiteral( "boundsCrs" ) ).trimmed();
        }
        if ( sourceContractSnapshot.mapIdDeclared )
        {
          if ( !sourceContractElement.hasAttribute( QStringLiteral( "mapId" ) ) )
            return false;
          sourceContractSnapshot.mapId = sourceContractElement.attribute( QStringLiteral( "mapId" ) ).trimmed();
        }
        if ( sourceContractSnapshot.payloadDeclared )
        {
          if ( !sourceContractElement.hasAttribute( QStringLiteral( "payload" ) ) )
            return false;
          const QString sourcePayload = sourceContractElement.attribute( QStringLiteral( "payload" ) ).trimmed().toLower();
          if ( sourcePayload != QLatin1String( "raster" ) && sourcePayload != QLatin1String( "elevation" ) &&
               sourcePayload != QLatin1String( "vector" ) && sourcePayload != QLatin1String( "unknown" ) )
            return false;
          sourceContractSnapshot.payload = payloadFromXml( sourcePayload );
        }

        QgsMtpl::TileMatrixDefinition sourceDefinition = definition;
        sourceDefinition.zoomRangeExplicit = sourceContractSnapshot.zoomRangeExplicit;
        if ( sourceContractSnapshot.zoomRangeExplicit )
        {
          sourceDefinition.minimumZoom = sourceContractSnapshot.minimumZoom;
          sourceDefinition.maximumZoom = sourceContractSnapshot.maximumZoom;
        }
        sourceDefinition.hasBounds = sourceContractSnapshot.hasBounds;
        if ( sourceContractSnapshot.hasBounds )
        {
          sourceDefinition.boundsXMinimum = sourceContractSnapshot.boundsXMinimum;
          sourceDefinition.boundsYMinimum = sourceContractSnapshot.boundsYMinimum;
          sourceDefinition.boundsXMaximum = sourceContractSnapshot.boundsXMaximum;
          sourceDefinition.boundsYMaximum = sourceContractSnapshot.boundsYMaximum;
          sourceDefinition.boundsCrsAuthId = sourceContractSnapshot.boundsCrsAuthId;
        }
        sourceDefinition.legacyFallbackApplied = sourceContractSnapshot.legacyFallbackApplied;
        QString sourceDefinitionError;
        if ( !sourceDefinition.isValid( &sourceDefinitionError ) ||
             ( sourceContractSnapshot.legacyFallbackApplied &&
               ( sourceContractSnapshot.zoomRangeExplicit || sourceContractSnapshot.hasBounds ) ) )
          return false;
      }
      if ( package.descriptor.format == QgsMtpl::PackageFormat::Dtp ||
           package.descriptor.payload == QgsMtpl::PayloadType::Elevation )
      {
        package.descriptor.metadata.insert( QStringLiteral( "dataType" ), packageElement.attribute( QStringLiteral( "dataType" ), QStringLiteral( "uint16" ) ) );
        package.descriptor.metadata.insert( QStringLiteral( "endianness" ), packageElement.attribute( QStringLiteral( "endianness" ), QStringLiteral( "little" ) ) );
      }
      if ( !xmlIntAttribute( packageElement, QStringLiteral( "addressMinZoom" ), package.address.minZoom ) ||
           !xmlIntAttribute( packageElement, QStringLiteral( "addressMaxZoom" ), package.address.maxZoom ) ||
           !xmlIntAttribute( packageElement, QStringLiteral( "addressBaseZoom" ), package.address.baseZoom ) ||
           !xmlUIntAttribute( packageElement, QStringLiteral( "addressX" ), package.address.packageX ) ||
           !xmlUIntAttribute( packageElement, QStringLiteral( "addressY" ), package.address.packageY ) )
        return false;
      package.fileLastModifiedMs = package.descriptor.fileLastModifiedMs;
      package.sidecarPath = package.descriptor.sidecarPath;

      const QDomElement rangesElement = packageElement.firstChildElement( QStringLiteral( "ranges" ) );
      if ( rangesElement.isNull() )
        return false;
      for ( QDomElement rangeElement = rangesElement.firstChildElement( QStringLiteral( "range" ) );
            !rangeElement.isNull();
            rangeElement = rangeElement.nextSiblingElement( QStringLiteral( "range" ) ) )
      {
        QgsMtpl::TileRangeRecord range;
        if ( !xmlIntAttribute( rangeElement, QStringLiteral( "zoom" ), range.zoom ) ||
             !xmlUIntAttribute( rangeElement, QStringLiteral( "xMin" ), range.xMin ) ||
             !xmlUIntAttribute( rangeElement, QStringLiteral( "xMax" ), range.xMax ) ||
             !xmlUIntAttribute( rangeElement, QStringLiteral( "yMin" ), range.yMin ) ||
             !xmlUIntAttribute( rangeElement, QStringLiteral( "yMax" ), range.yMax ) ||
             !xmlUIntAttribute( rangeElement, QStringLiteral( "presentXMin" ), range.presentXMin ) ||
             !xmlUIntAttribute( rangeElement, QStringLiteral( "presentXMax" ), range.presentXMax ) ||
             !xmlUIntAttribute( rangeElement, QStringLiteral( "presentYMin" ), range.presentYMin ) ||
             !xmlUIntAttribute( rangeElement, QStringLiteral( "presentYMax" ), range.presentYMax ) ||
             !xmlULongLongAttribute( rangeElement, QStringLiteral( "presentCount" ), range.presentCount ) )
          return false;
        package.ranges.append( range );
      }
      dataset.packages.append( package );
    }

    QString matrixError;
    QgsMtpl::TileMatrixDefinition renderingDefinition = definition;
    renderingDefinition.minimumZoom = dataset.minimumZoom;
    renderingDefinition.maximumZoom = dataset.maximumZoom;
    QgsTileMatrixSet expectedMatrixSet;
    if ( !tileMatrixSetFromDefinition( renderingDefinition, expectedMatrixSet, &matrixError ) )
      return false;
    const QDomElement matrixSetElement = datasetElement.firstChildElement( QStringLiteral( "matrixSet" ) );
    if ( matrixSetElement.isNull() )
      return false;
    QgsTileMatrixSet persistedMatrixSet;
    if ( !persistedMatrixSet.readXml( matrixSetElement, context ) ||
         !tileMatrixSetsMatch( persistedMatrixSet, expectedMatrixSet ) )
      return false;
    mTileMatrixSet = expectedMatrixSet;

    if ( dataset.hasExtent )
    {
      const QgsRectangle datasetExtent( dataset.extentXMinimum,
                                        dataset.extentYMinimum,
                                        dataset.extentXMaximum,
                                        dataset.extentYMaximum );
      if ( !std::isfinite( dataset.extentXMinimum ) || !std::isfinite( dataset.extentYMinimum ) ||
           !std::isfinite( dataset.extentXMaximum ) || !std::isfinite( dataset.extentYMaximum ) ||
           datasetExtent.isEmpty() || !expectedMatrixSet.rootMatrix().extent().contains( datasetExtent ) )
        return false;
      const QgsCoordinateReferenceSystem boundsCrs( definition.boundsCrsAuthId );
      if ( definition.hasBounds && boundsCrs.isValid() && boundsCrs == expectedMatrixSet.crs() )
      {
        const QgsRectangle bounds( definition.boundsXMinimum,
                                   definition.boundsYMinimum,
                                   definition.boundsXMaximum,
                                   definition.boundsYMaximum );
        const bool compatibilityExtentOverride = dataset.compatibilityMode && dataset.packages.size() == 1 &&
          dataset.packages.constFirst().descriptor.displayOverridesApplied;
        if ( datasetExtent != bounds && !compatibilityExtentOverride )
          return false;
      }
    }

    if ( dataset.payload == QgsMtpl::PayloadType::VectorTile || dataset.payload == QgsMtpl::PayloadType::Files )
      return false;
    for ( const QgsMtpl::TileDatasetPackage &package : std::as_const( dataset.packages ) )
    {
      if ( package.descriptor.format != QgsMtpl::PackageFormat::Ptp ||
           ( dataset.payload != QgsMtpl::PayloadType::Unknown &&
             package.descriptor.payload != QgsMtpl::PayloadType::Unknown &&
             package.descriptor.payload != dataset.payload ) )
        return false;

      if ( !dataset.compatibilityMode )
      {
        QgsMtpl::TilePackageAddress fileAddress;
        QString addressError;
        if ( !QgsMtpl::TileResolver::parsePackageFileName( QFileInfo( package.descriptor.path ).fileName(), fileAddress, &addressError ) ||
             fileAddress != package.address || !dataset.partitionRule.bands.contains( package.address.band() ) )
          return false;
      }
      for ( const QgsMtpl::TileRangeRecord &range : package.ranges )
      {
        QString rangeError;
        if ( !range.isValid( &rangeError ) || range.zoom < dataset.minimumZoom || range.zoom > dataset.maximumZoom ||
             ( !dataset.compatibilityMode && ( range.zoom < package.address.minZoom || range.zoom > package.address.maxZoom ) ) )
          return false;
      }
    }

    if ( !dataset.compatibilityMode )
    {
      for ( int index = 0; index < dataset.packages.size(); ++index )
      {
        const QString addressKey = dataset.packages.at( index ).address.key();
        if ( dataset.packageIndexByAddress.contains( addressKey ) )
          return false;
        dataset.packageIndexByAddress.insert( addressKey, index );
      }
    }

    QString datasetError;
    if ( !restorableDatasetIsValid( dataset, &datasetError ) )
      return false;

    const bool hasLockedPackages = std::any_of(
      dataset.packages.cbegin(), dataset.packages.cend(), []( const QgsMtpl::TileDatasetPackage &package )
      {
        return package.descriptor.encryption == QgsMtpl::EncryptionState::Locked;
      } );
    QgsMtpl::CryptoKeys rememberedKeys = hasLockedPackages
                                           ? QgsMtplCredentialStore::rememberedKeysIfUnlocked()
                                           : QgsMtpl::CryptoKeys();
    mDatasetKeys.reserve( dataset.packages.size() );
    QList<QgsMtpl::PackageDescriptor> freshDescriptors;
    QList<int> freshSavedIndexes;
    for ( int packageIndex = 0; packageIndex < dataset.packages.size(); ++packageIndex )
    {
      QgsMtpl::TileDatasetPackage &package = dataset.packages[packageIndex];
      const bool expectedPlainPackage = package.descriptor.encryption == QgsMtpl::EncryptionState::Plain;
      const bool sourceStampChanged = sourceStampChangedPackages.contains( packageIndex );
      QgsMtpl::CryptoKeys packageKeys;
      QgsMtpl::CredentialSource candidateSource = QgsMtpl::CredentialSource::None;
      if ( !expectedPlainPackage )
      {
        if ( loadKeysFromSidecar( package.descriptor, packageKeys ) )
          candidateSource = QgsMtpl::CredentialSource::Sidecar;
        else if ( rememberedKeys.isValid() )
        {
          packageKeys = rememberedKeys;
          candidateSource = QgsMtpl::CredentialSource::Remembered;
        }
      }
      if ( QFileInfo::exists( package.descriptor.path ) && !sourceStampChanged )
      {
        QgsMtpl::PackageDescriptor freshDescriptor;
        QString probeError;
        const QgsMtpl::CredentialSource probeSource = packageKeys.isValid()
          ? candidateSource
          : QgsMtpl::CredentialSource::None;
        const bool inspected = QgsMtplPackageService::probePackage(
          package.descriptor.path,
          freshDescriptor,
          probeError,
          packageKeys,
          true,
          probeSource,
          QgsMtplPackageService::CancelCheck(),
          QgsMtplPackageService::ProgressCallback(),
          false,
          false );
        if ( freshDescriptor.format != QgsMtpl::PackageFormat::Ptp ||
             freshDescriptor.tileSize != definition.tileSize ||
             freshDescriptor.fileSize != package.descriptor.fileSize ||
             expectedPlainPackage != ( freshDescriptor.encryption == QgsMtpl::EncryptionState::Plain ) )
          return false;

        const bool unavailableBecauseLocked = !expectedPlainPackage && !freshDescriptor.isReady() &&
          !freshDescriptor.isCredentialedEmptyPtp() &&
          ( freshDescriptor.readiness == QgsMtpl::ReadinessState::KeyRequired ||
            freshDescriptor.readiness == QgsMtpl::ReadinessState::KeyRejectedOrCorrupt ||
            freshDescriptor.readiness == QgsMtpl::ReadinessState::UnverifiableEmpty );
        if ( unavailableBecauseLocked )
        {
          packageKeys.clear();
          package.descriptor.encryption = QgsMtpl::EncryptionState::Locked;
          package.descriptor.readiness = QgsMtpl::ReadinessState::KeyRequired;
          package.descriptor.credentialSource = QgsMtpl::CredentialSource::None;
        }
        else if ( !inspected )
        {
          return false;
        }
        else
        {
          freshDescriptor.credentialSource = expectedPlainPackage
                                               ? QgsMtpl::CredentialSource::None
                                               : candidateSource;
          QgsMtpl::PackageDescriptor matrixDescriptor = freshDescriptor;
          matrixDescriptor.displayOverridesApplied = package.descriptor.displayOverridesApplied;
          matrixDescriptor.crsAuthId = package.descriptor.crsAuthId;
          matrixDescriptor.scheme = package.descriptor.scheme;
          QgsMtpl::TileMatrixDefinition freshDefinition;
          QString freshMatrixError;
          if ( !QgsMtpl::TileMatrixDefinition::fromPackageDescriptor(
                 matrixDescriptor, freshDefinition, &freshMatrixError ) ||
               !tileMatrixDefinitionMatchesSnapshot(
                 freshDefinition,
                 definition,
                 package.sourceContract.available ? &package.sourceContract : nullptr ) ||
               !sourceMetadataMatchesContract( freshDescriptor, package.sourceContract ) )
            return false;
          if ( !package.sourceContract.available )
            package.sourceContract = sourceContract( freshDescriptor, freshDefinition );
          freshDescriptors.append( freshDescriptor );
          freshSavedIndexes.append( packageIndex );
        }
      }
      package.sidecarPath = package.descriptor.sidecarPath;
      mDatasetKeys.append( packageKeys );
      packageKeys.clear();
    }
    rememberedKeys.clear();

    if ( !freshDescriptors.isEmpty() )
    {
      QgsMtplPackageService::applyExternalDisplayMetadata(
        dataset.sourcePath, dataset.directorySource, freshDescriptors );

      QString freshMapId;
      for ( const QgsMtpl::PackageDescriptor &freshDescriptor : std::as_const( freshDescriptors ) )
      {
        const QString packageMapId = metadataString(
          freshDescriptor.metadata,
          { QStringLiteral( "map_id" ), QStringLiteral( "mapId" ) },
          QString() ).trimmed();
        if ( freshMapId.isEmpty() )
          freshMapId = packageMapId;
        else if ( !packageMapId.isEmpty() && packageMapId != freshMapId )
          return false;
      }
      const bool allPackagesInspected = freshDescriptors.size() == dataset.packages.size();
      if ( ( !freshMapId.isEmpty() && freshMapId != dataset.mapId ) ||
           ( allPackagesInspected && freshMapId != dataset.mapId ) )
        return false;

      for ( int freshIndex = 0; freshIndex < freshDescriptors.size(); ++freshIndex )
      {
        QgsMtpl::TileDatasetPackage &savedPackage = dataset.packages[freshSavedIndexes.at( freshIndex )];
        const QgsMtpl::PackageDescriptor &freshDescriptor = freshDescriptors.at( freshIndex );
        mergeFreshDescriptorState(
          savedPackage.descriptor, freshDescriptor, freshDescriptor.credentialSource );
        savedPackage.sidecarPath = savedPackage.descriptor.sidecarPath;
      }
    }

    if ( dataset.compatibilityMode && dataset.packages.size() == 1 )
    {
      QgsMtpl::PackageDescriptor &descriptor = dataset.packages[0].descriptor;
      descriptor.hasExtent = dataset.hasExtent;
      descriptor.extentXMinimum = dataset.extentXMinimum;
      descriptor.extentYMinimum = dataset.extentYMinimum;
      descriptor.extentXMaximum = dataset.extentXMaximum;
      descriptor.extentYMaximum = dataset.extentYMaximum;
    }
    mDataset = std::make_shared<const QgsMtpl::TileDatasetDescriptor>( dataset );
    if ( !dataset.packages.isEmpty() )
      mDescriptor = dataset.packages.constFirst().descriptor;
    applyDataset();
    return !dataset.sourcePath.isEmpty();
  }

  const QDomElement element = layerNode.firstChildElement( QStringLiteral( "mtpl-package" ) );
  mDescriptor.path = source();
  mDescriptor.displayName = name();
  mDescriptor.format = formatFromXml( element.attribute( QStringLiteral( "format" ) ) );
  if ( mDescriptor.format == QgsMtpl::PackageFormat::Unknown )
    mDescriptor.format = QgsMtpl::packageFormatFromPath( mDescriptor.path );
  mDescriptor.payload = payloadFromXml( element.attribute( QStringLiteral( "payload" ) ) );
  const QString storage = element.attribute( QStringLiteral( "storage" ) );
  mDescriptor.encryption = storage == QLatin1String( "encrypted" ) || storage == QLatin1String( "locked" ) ? QgsMtpl::EncryptionState::Locked : QgsMtpl::EncryptionState::Plain;
  mDescriptor.readiness = mDescriptor.encryption == QgsMtpl::EncryptionState::Plain
                            ? QgsMtpl::ReadinessState::PlainReady
                            : QgsMtpl::ReadinessState::KeyRequired;
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
  if ( mDescriptor.format == QgsMtpl::PackageFormat::Dtp )
  {
    mDescriptor.metadata.insert( QStringLiteral( "dataType" ), element.attribute( QStringLiteral( "dataType" ), QStringLiteral( "uint16" ) ) );
    mDescriptor.metadata.insert( QStringLiteral( "endianness" ), element.attribute( QStringLiteral( "endianness" ), QStringLiteral( "little" ) ) );
  }
  if ( mDescriptor.encryption == QgsMtpl::EncryptionState::Locked )
  {
    if ( restoreKeysFromSidecar( mDescriptor, mKeys ) )
    {
      mDescriptor.readiness = QgsMtpl::ReadinessState::KeyVerified;
      mDescriptor.credentialSource = QgsMtpl::CredentialSource::Sidecar;
    }
    else
    {
      QgsMtpl::CryptoKeys rememberedKeys = QgsMtplCredentialStore::rememberedKeysIfUnlocked();
      const bool usable = rememberedKeys.isValid() &&
                          ( mDescriptor.format == QgsMtpl::PackageFormat::Sfp
                              ? sfpPackageKeysAreUsable( mDescriptor, rememberedKeys )
                              : tilePackageKeysAreUsable( mDescriptor, rememberedKeys ) );
      if ( usable )
      {
        mKeys = rememberedKeys;
        mDescriptor.encryption = QgsMtpl::EncryptionState::Encrypted;
        mDescriptor.readiness = QgsMtpl::ReadinessState::KeyVerified;
        mDescriptor.credentialSource = QgsMtpl::CredentialSource::Remembered;
      }
      rememberedKeys.clear();
    }
  }
  if ( mDescriptor.format == QgsMtpl::PackageFormat::Ptp &&
       mDescriptor.payload == QgsMtpl::PayloadType::RasterImage )
  {
    mTransformContext = context.transformContext();
    if ( !restoreLegacyPtpDataset() )
      return false;
    applyDataset();
    return isValid();
  }
  applyDescriptor();
  return !mDescriptor.path.isEmpty();
}

bool QgsMtplPluginLayer::restoreLegacyPtpDataset()
{
  QgsMtpl::PackageDescriptor fresh;
  QString probeError;
  const bool inspected = QgsMtplPackageService::probePackage(
    mDescriptor.path, fresh, probeError, mKeys, true, mDescriptor.credentialSource,
    QgsMtplPackageService::CancelCheck(), QgsMtplPackageService::ProgressCallback(), false );
  const bool imageDecodeFailure = QgsMtplPackageService::ptpContractFailure( fresh ) ==
                                  QgsMtplPackageService::PtpContractFailure::ImageDecode;
  if ( ( !inspected && !imageDecodeFailure ) || fresh.format != QgsMtpl::PackageFormat::Ptp ||
       fresh.tileSize != mDescriptor.tileSize ||
       !fresh.metadata.contains( QStringLiteral( "_mtplRangeBounds" ) ) )
    return false;

  // The old project already records its raster contract and display overrides.
  // Refresh the source structure without replacing those saved settings or
  // discovering sibling packages. As for restored dataset snapshots, a bad
  // image remains transparent and is diagnosed by the raster interface.
  QgsMtpl::PackageDescriptor candidate = mDescriptor;
  candidate.metadata = fresh.metadata;
  candidate.fileSize = fresh.fileSize;
  candidate.fileLastModifiedMs = fresh.fileLastModifiedMs;
  candidate.minimumZoom = fresh.minimumZoom;
  candidate.maximumZoom = fresh.maximumZoom;
  candidate.readinessMessage = fresh.readinessMessage;
  const bool locked = fresh.encryption != QgsMtpl::EncryptionState::Plain &&
                      ( fresh.encryption == QgsMtpl::EncryptionState::Locked || !mKeys.isValid() );
  candidate.encryption = fresh.encryption == QgsMtpl::EncryptionState::Plain
                          ? QgsMtpl::EncryptionState::Plain
                          : QgsMtpl::EncryptionState::Encrypted;
  candidate.readiness = candidate.encryption == QgsMtpl::EncryptionState::Plain
                         ? QgsMtpl::ReadinessState::PlainReady
                         : QgsMtpl::ReadinessState::KeyVerified;
  if ( candidate.encryption == QgsMtpl::EncryptionState::Plain || locked )
    candidate.credentialSource = QgsMtpl::CredentialSource::None;

  QgsMtpl::TileDatasetBuildOptions options;
  options.singleFileCompatibility = true;
  QgsMtpl::TileDatasetBuildResult result = QgsMtpl::buildTileDataset(
    candidate.path, false, { candidate }, options );
  if ( !result.ok() )
    return false;

  QgsMtpl::TileDatasetDescriptor &dataset = result.dataset;
  if ( mDescriptor.minimumZoom >= 0 && mDescriptor.maximumZoom >= mDescriptor.minimumZoom )
  {
    dataset.minimumZoom = mDescriptor.minimumZoom;
    dataset.maximumZoom = mDescriptor.maximumZoom;
  }
  if ( mDescriptor.hasExtent )
  {
    dataset.hasExtent = true;
    dataset.extentXMinimum = mDescriptor.extentXMinimum;
    dataset.extentYMinimum = mDescriptor.extentYMinimum;
    dataset.extentXMaximum = mDescriptor.extentXMaximum;
    dataset.extentYMaximum = mDescriptor.extentYMaximum;
  }
  QgsMtpl::TileDatasetPackage &package = dataset.packages[0];
  package.descriptor.metadata = sanitizedFreshMetadata( fresh.metadata );
  if ( locked )
  {
    package.descriptor.encryption = QgsMtpl::EncryptionState::Locked;
    package.descriptor.readiness = QgsMtpl::ReadinessState::KeyRequired;
  }
  QString datasetError;
  if ( !restorableDatasetIsValid( dataset, &datasetError ) )
    return false;

  mDatasetKeys = repeatedKeys( 1, locked ? QgsMtpl::CryptoKeys() : mKeys );
  mKeys.clear();
  mDescriptor = package.descriptor;
  mDataset = std::make_shared<const QgsMtpl::TileDatasetDescriptor>( dataset );
  return true;
}

bool QgsMtplPluginLayer::writeXml( QDomNode &layerNode, QDomDocument &document, const QgsReadWriteContext &context ) const
{
  if ( !QgsMapLayer::writeXml( layerNode, document, context ) )
    return false;

  QDomElement layerElement = layerNode.toElement();
  layerElement.setAttribute( QStringLiteral( "type" ), QStringLiteral( "plugin" ) );
  layerElement.setAttribute( QStringLiteral( "name" ), layerTypeKey() );

  if ( mDataset )
  {
    QDomElement datasetElement = document.createElement( QStringLiteral( "mtpl-dataset" ) );
    datasetElement.setAttribute( QStringLiteral( "version" ), 1 );
    datasetElement.setAttribute( QStringLiteral( "source" ), context.pathResolver().writePath( mDataset->sourcePath ) );
    datasetElement.setAttribute( QStringLiteral( "directorySource" ), mDataset->directorySource ? 1 : 0 );
    datasetElement.setAttribute( QStringLiteral( "payload" ), payloadToXml( mDataset->payload ) );
    if ( !mDataset->mapId.isEmpty() )
      datasetElement.setAttribute( QStringLiteral( "mapId" ), mDataset->mapId );
    datasetElement.setAttribute( QStringLiteral( "minimumZoom" ), mDataset->minimumZoom );
    datasetElement.setAttribute( QStringLiteral( "maximumZoom" ), mDataset->maximumZoom );
    datasetElement.setAttribute( QStringLiteral( "compatibilityMode" ), mDataset->compatibilityMode ? 1 : 0 );
    datasetElement.setAttribute( QStringLiteral( "hasExtent" ), mDataset->hasExtent ? 1 : 0 );
    if ( mDataset->hasExtent )
    {
      datasetElement.setAttribute( QStringLiteral( "extentXMinimum" ), QString::number( mDataset->extentXMinimum, 'g', 17 ) );
      datasetElement.setAttribute( QStringLiteral( "extentYMinimum" ), QString::number( mDataset->extentYMinimum, 'g', 17 ) );
      datasetElement.setAttribute( QStringLiteral( "extentXMaximum" ), QString::number( mDataset->extentXMaximum, 'g', 17 ) );
      datasetElement.setAttribute( QStringLiteral( "extentYMaximum" ), QString::number( mDataset->extentYMaximum, 'g', 17 ) );
    }

    QDomElement ruleElement = document.createElement( QStringLiteral( "partition-rule" ) );
    ruleElement.setAttribute( QStringLiteral( "id" ), mDataset->partitionRule.id );
    ruleElement.setAttribute( QStringLiteral( "name" ), mDataset->partitionRule.name );
    ruleElement.setAttribute( QStringLiteral( "builtIn" ), mDataset->partitionRule.builtIn ? 1 : 0 );
    for ( const QgsMtpl::PartitionBand &band : mDataset->partitionRule.bands )
    {
      QDomElement bandElement = document.createElement( QStringLiteral( "band" ) );
      bandElement.setAttribute( QStringLiteral( "minZoom" ), band.minZoom );
      bandElement.setAttribute( QStringLiteral( "maxZoom" ), band.maxZoom );
      bandElement.setAttribute( QStringLiteral( "baseZoom" ), band.baseZoom );
      ruleElement.appendChild( bandElement );
    }
    datasetElement.appendChild( ruleElement );

    const QgsMtpl::TileMatrixDefinition &definition = mDataset->matrix;
    QDomElement definitionElement = document.createElement( QStringLiteral( "tile-matrix" ) );
    definitionElement.setAttribute( QStringLiteral( "version" ), 1 );
    definitionElement.setAttribute( QStringLiteral( "crs" ), definition.crsAuthId );
    definitionElement.setAttribute( QStringLiteral( "scheme" ), tileSchemeToXml( definition.scheme ) );
    definitionElement.setAttribute( QStringLiteral( "tileSize" ), definition.tileSize );
    definitionElement.setAttribute( QStringLiteral( "topLeftX" ), QString::number( definition.topLeftX, 'g', 17 ) );
    definitionElement.setAttribute( QStringLiteral( "topLeftY" ), QString::number( definition.topLeftY, 'g', 17 ) );
    definitionElement.setAttribute( QStringLiteral( "z0TileSpan" ), QString::number( definition.z0TileSpan, 'g', 17 ) );
    definitionElement.setAttribute( QStringLiteral( "z0MatrixWidth" ), QString::number( definition.z0MatrixWidth ) );
    definitionElement.setAttribute( QStringLiteral( "z0MatrixHeight" ), QString::number( definition.z0MatrixHeight ) );
    definitionElement.setAttribute( QStringLiteral( "scaleToZoomMethod" ), definition.scaleToZoomMethod );
    definitionElement.setAttribute( QStringLiteral( "minimumZoom" ), definition.minimumZoom );
    definitionElement.setAttribute( QStringLiteral( "maximumZoom" ), definition.maximumZoom );
    definitionElement.setAttribute( QStringLiteral( "hasBounds" ), definition.hasBounds ? 1 : 0 );
    if ( definition.hasBounds )
    {
      definitionElement.setAttribute( QStringLiteral( "boundsXMinimum" ), QString::number( definition.boundsXMinimum, 'g', 17 ) );
      definitionElement.setAttribute( QStringLiteral( "boundsYMinimum" ), QString::number( definition.boundsYMinimum, 'g', 17 ) );
      definitionElement.setAttribute( QStringLiteral( "boundsXMaximum" ), QString::number( definition.boundsXMaximum, 'g', 17 ) );
      definitionElement.setAttribute( QStringLiteral( "boundsYMaximum" ), QString::number( definition.boundsYMaximum, 'g', 17 ) );
      definitionElement.setAttribute( QStringLiteral( "boundsCrs" ), definition.boundsCrsAuthId );
    }
    definitionElement.setAttribute( QStringLiteral( "zoomRangeExplicit" ), definition.zoomRangeExplicit ? 1 : 0 );
    definitionElement.setAttribute( QStringLiteral( "legacyFallbackApplied" ), definition.legacyFallbackApplied ? 1 : 0 );
    datasetElement.appendChild( definitionElement );
    datasetElement.appendChild( mTileMatrixSet.writeXml( document, context ) );

    QDomElement packagesElement = document.createElement( QStringLiteral( "packages" ) );
    const QString packageBasePath = mDataset->directorySource ? mDataset->sourcePath : QFileInfo( mDataset->sourcePath ).absolutePath();
    for ( int packageIndex = 0; packageIndex < mDataset->packages.size(); ++packageIndex )
    {
      const QgsMtpl::TileDatasetPackage &package = mDataset->packages.at( packageIndex );
      const QgsMtpl::PackageDescriptor &descriptor = package.descriptor;
      QDomElement packageElement = document.createElement( QStringLiteral( "package" ) );
      QString relativePath = package.relativePath;
      if ( relativePath.isEmpty() && !descriptor.path.isEmpty() )
        relativePath = QDir( packageBasePath ).relativeFilePath( descriptor.path );
      packageElement.setAttribute( QStringLiteral( "relativePath" ), relativePath );
      packageElement.setAttribute( QStringLiteral( "path" ), context.pathResolver().writePath( descriptor.path ) );
      packageElement.setAttribute( QStringLiteral( "displayName" ), descriptor.displayName );
      packageElement.setAttribute( QStringLiteral( "format" ), formatToXml( descriptor.format ) );
      packageElement.setAttribute( QStringLiteral( "payload" ), payloadToXml( descriptor.payload ) );
      packageElement.setAttribute( QStringLiteral( "storage" ), descriptor.encryption == QgsMtpl::EncryptionState::Plain ? QStringLiteral( "plain" ) : QStringLiteral( "encrypted" ) );
      const QString sidecarPath = !package.sidecarPath.isEmpty() ? package.sidecarPath : descriptor.sidecarPath;
      if ( !sidecarPath.isEmpty() )
        packageElement.setAttribute( QStringLiteral( "sidecar" ), context.pathResolver().writePath( sidecarPath ) );
      if ( !descriptor.keyId.isEmpty() )
        packageElement.setAttribute( QStringLiteral( "keyId" ), descriptor.keyId );
      if ( !descriptor.stylePath.isEmpty() )
        packageElement.setAttribute( QStringLiteral( "style" ), context.pathResolver().writePath( descriptor.stylePath ) );
      quint64 fileSize = descriptor.fileSize;
      qint64 fileLastModifiedMs = package.fileLastModifiedMs >= 0
                                     ? package.fileLastModifiedMs
                                     : descriptor.fileLastModifiedMs;
      if ( mStructureValidationCache )
      {
        mStructureValidationCache->validatedFileStamp(
          QgsMtplStructureValidationCache::packageIdentity(
            packageIndex,
            descriptor.path,
            QgsMtplStructureValidationCache::structureSummary( *mDataset, packageIndex ) ),
          fileSize,
          fileLastModifiedMs );
      }
      packageElement.setAttribute( QStringLiteral( "fileSize" ), QString::number( fileSize ) );
      packageElement.setAttribute( QStringLiteral( "lastModifiedMs" ), QString::number( fileLastModifiedMs ) );
      packageElement.setAttribute( QStringLiteral( "minimumZoom" ), descriptor.minimumZoom );
      packageElement.setAttribute( QStringLiteral( "maximumZoom" ), descriptor.maximumZoom );
      packageElement.setAttribute( QStringLiteral( "scale" ), QString::number( descriptor.scale, 'g', 17 ) );
      packageElement.setAttribute( QStringLiteral( "offset" ), QString::number( descriptor.offset, 'g', 17 ) );
      packageElement.setAttribute( QStringLiteral( "hasNoData" ), descriptor.hasNoData ? 1 : 0 );
      packageElement.setAttribute( QStringLiteral( "displayOverridesApplied" ), descriptor.displayOverridesApplied ? 1 : 0 );
      if ( descriptor.hasNoData )
        packageElement.setAttribute( QStringLiteral( "noData" ), QString::number( descriptor.noData, 'g', 17 ) );
      if ( descriptor.format == QgsMtpl::PackageFormat::Dtp ||
           descriptor.payload == QgsMtpl::PayloadType::Elevation )
      {
        packageElement.setAttribute( QStringLiteral( "dataType" ), elevationDataType( descriptor ) );
        packageElement.setAttribute( QStringLiteral( "endianness" ), elevationEndianness( descriptor ) );
      }
      packageElement.setAttribute( QStringLiteral( "addressMinZoom" ), package.address.minZoom );
      packageElement.setAttribute( QStringLiteral( "addressMaxZoom" ), package.address.maxZoom );
      packageElement.setAttribute( QStringLiteral( "addressBaseZoom" ), package.address.baseZoom );
      packageElement.setAttribute( QStringLiteral( "addressX" ), package.address.packageX );
      packageElement.setAttribute( QStringLiteral( "addressY" ), package.address.packageY );

      if ( package.sourceContract.available )
      {
        const QgsMtpl::TilePackageSourceContract &sourceContractSnapshot = package.sourceContract;
        QDomElement sourceContractElement = document.createElement( QStringLiteral( "source-contract" ) );
        sourceContractElement.setAttribute( QStringLiteral( "version" ), 1 );
        sourceContractElement.setAttribute( QStringLiteral( "zoomRangeExplicit" ), sourceContractSnapshot.zoomRangeExplicit ? 1 : 0 );
        if ( sourceContractSnapshot.zoomRangeExplicit )
        {
          sourceContractElement.setAttribute( QStringLiteral( "minimumZoom" ), sourceContractSnapshot.minimumZoom );
          sourceContractElement.setAttribute( QStringLiteral( "maximumZoom" ), sourceContractSnapshot.maximumZoom );
        }
        sourceContractElement.setAttribute( QStringLiteral( "hasBounds" ), sourceContractSnapshot.hasBounds ? 1 : 0 );
        if ( sourceContractSnapshot.hasBounds )
        {
          sourceContractElement.setAttribute( QStringLiteral( "boundsXMinimum" ), QString::number( sourceContractSnapshot.boundsXMinimum, 'g', 17 ) );
          sourceContractElement.setAttribute( QStringLiteral( "boundsYMinimum" ), QString::number( sourceContractSnapshot.boundsYMinimum, 'g', 17 ) );
          sourceContractElement.setAttribute( QStringLiteral( "boundsXMaximum" ), QString::number( sourceContractSnapshot.boundsXMaximum, 'g', 17 ) );
          sourceContractElement.setAttribute( QStringLiteral( "boundsYMaximum" ), QString::number( sourceContractSnapshot.boundsYMaximum, 'g', 17 ) );
          sourceContractElement.setAttribute( QStringLiteral( "boundsCrs" ), sourceContractSnapshot.boundsCrsAuthId );
        }
        sourceContractElement.setAttribute( QStringLiteral( "legacyFallbackApplied" ), sourceContractSnapshot.legacyFallbackApplied ? 1 : 0 );
        sourceContractElement.setAttribute( QStringLiteral( "mapIdDeclared" ), sourceContractSnapshot.mapIdDeclared ? 1 : 0 );
        if ( sourceContractSnapshot.mapIdDeclared )
          sourceContractElement.setAttribute( QStringLiteral( "mapId" ), sourceContractSnapshot.mapId );
        sourceContractElement.setAttribute( QStringLiteral( "payloadDeclared" ), sourceContractSnapshot.payloadDeclared ? 1 : 0 );
        if ( sourceContractSnapshot.payloadDeclared )
          sourceContractElement.setAttribute( QStringLiteral( "payload" ), payloadToXml( sourceContractSnapshot.payload ) );
        packageElement.appendChild( sourceContractElement );
      }

      QDomElement rangesElement = document.createElement( QStringLiteral( "ranges" ) );
      for ( const QgsMtpl::TileRangeRecord &range : package.ranges )
      {
        QDomElement rangeElement = document.createElement( QStringLiteral( "range" ) );
        rangeElement.setAttribute( QStringLiteral( "zoom" ), range.zoom );
        rangeElement.setAttribute( QStringLiteral( "xMin" ), range.xMin );
        rangeElement.setAttribute( QStringLiteral( "xMax" ), range.xMax );
        rangeElement.setAttribute( QStringLiteral( "yMin" ), range.yMin );
        rangeElement.setAttribute( QStringLiteral( "yMax" ), range.yMax );
        rangeElement.setAttribute( QStringLiteral( "presentXMin" ), range.presentXMin );
        rangeElement.setAttribute( QStringLiteral( "presentXMax" ), range.presentXMax );
        rangeElement.setAttribute( QStringLiteral( "presentYMin" ), range.presentYMin );
        rangeElement.setAttribute( QStringLiteral( "presentYMax" ), range.presentYMax );
        rangeElement.setAttribute( QStringLiteral( "presentCount" ), QString::number( range.presentCount ) );
        rangesElement.appendChild( rangeElement );
      }
      packageElement.appendChild( rangesElement );
      packagesElement.appendChild( packageElement );
    }
    datasetElement.appendChild( packagesElement );
    layerNode.appendChild( datasetElement );
    return true;
  }

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
  const bool supportedPtpPayload = mDescriptor.format != QgsMtpl::PackageFormat::Ptp ||
                                   mDescriptor.payload == QgsMtpl::PayloadType::RasterImage;
  setValid( supportedPtpPayload && !mDescriptor.path.isEmpty() && QFileInfo::exists( mDescriptor.path ) );
}

void QgsMtplPluginLayer::applyDataset()
{
  if ( !mDataset )
    return;

  if ( mTileMatrixSet.isEmpty() )
  {
    QString ignoredError;
    QgsMtpl::TileMatrixDefinition renderingDefinition = mDataset->matrix;
    renderingDefinition.minimumZoom = mDataset->minimumZoom;
    renderingDefinition.maximumZoom = mDataset->maximumZoom;
    tileMatrixSetFromDefinition( renderingDefinition, mTileMatrixSet, &ignoredError );
  }
  if ( !mTileMatrixSet.isEmpty() && mDataset->minimumZoom >= 0 && mDataset->maximumZoom >= mDataset->minimumZoom )
    mTileMatrixSet.dropMatricesOutsideZoomRange( mDataset->minimumZoom, mDataset->maximumZoom );

  const QString displayName = datasetDisplayName( *mDataset );
  if ( name().isEmpty() )
    setName( displayName );
  setSource( mDataset->sourcePath );

  const QgsCoordinateReferenceSystem datasetCrs = !mTileMatrixSet.isEmpty()
                                                   ? mTileMatrixSet.crs()
                                                   : QgsCoordinateReferenceSystem( mDataset->matrix.crsAuthId );
  if ( datasetCrs.isValid() )
    setCrs( datasetCrs );

  if ( mDataset->hasExtent )
  {
    setExtent( QgsRectangle( mDataset->extentXMinimum,
                             mDataset->extentYMinimum,
                             mDataset->extentXMaximum,
                             mDataset->extentYMaximum ) );
  }
  else if ( mDataset->matrix.hasBounds )
  {
    const QgsRectangle bounds( mDataset->matrix.boundsXMinimum,
                               mDataset->matrix.boundsYMinimum,
                               mDataset->matrix.boundsXMaximum,
                               mDataset->matrix.boundsYMaximum );
    const QgsCoordinateReferenceSystem boundsCrs( mDataset->matrix.boundsCrsAuthId );
    if ( boundsCrs.isValid() && datasetCrs.isValid() && boundsCrs != datasetCrs )
    {
      try
      {
        QgsCoordinateTransform boundsTransform( boundsCrs, datasetCrs, mTransformContext );
        boundsTransform.setBallparkTransformsAreAppropriate( true );
        setExtent( boundsTransform.transformBoundingBox( bounds ) );
      }
      catch ( QgsCsException & )
      {
        setExtent( mTileMatrixSet.rootMatrix().extent() );
      }
    }
    else if ( boundsCrs.isValid() && boundsCrs == datasetCrs )
    {
      setExtent( bounds );
    }
    else
    {
      setExtent( mTileMatrixSet.rootMatrix().extent() );
    }
  }
  else if ( !mTileMatrixSet.isEmpty() )
  {
    setExtent( mTileMatrixSet.rootMatrix().extent() );
  }
  else
  {
    setExtent( QgsRectangle() );
  }

  QString datasetError;
  const bool structurallyValid = restorableDatasetIsValid( *mDataset, &datasetError );
  setValid( structurallyValid && !mTileMatrixSet.isEmpty() && datasetCrs.isValid() );
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

  const QgsMtpl::TileDatasetDescriptor *dataset = mtplLayer->tileDataset();
  QDialog dialog( QApplication::activeWindow() );
  dialog.setWindowTitle( dataset ? QObject::tr( "MTPL 数据集属性" ) : QObject::tr( "MTPL 数据包属性" ) );
  dialog.resize( 620, 480 );
  auto *layout = new QFormLayout( &dialog );
  auto addValue = [layout]( const QString &label, const QString &value ) {
    auto *widget = new QLabel( value );
    widget->setTextInteractionFlags( Qt::TextSelectableByMouse );
    widget->setWordWrap( true );
    layout->addRow( label, widget );
  };

  QVariantMap displayedMetadata;
  if ( dataset )
  {
    addValue( QObject::tr( "数据源" ), QDir::toNativeSeparators( dataset->sourcePath ) );
    addValue( QObject::tr( "格式" ), QObject::tr( "PTP 多级瓦片数据集" ) );
    addValue( QObject::tr( "载荷" ), QgsMtpl::payloadTypeName( dataset->payload ) );
    addValue( QObject::tr( "数据包数量" ), QString::number( dataset->packages.size() ) );
    addValue( QObject::tr( "分包规则" ), dataset->partitionRule.name.isEmpty()
                                             ? dataset->partitionRule.id
                                             : QStringLiteral( "%1 (%2)" ).arg( dataset->partitionRule.name, dataset->partitionRule.id ) );
    if ( !dataset->mapId.isEmpty() )
      addValue( QObject::tr( "地图 ID" ), dataset->mapId );
    addValue( QObject::tr( "CRS" ), dataset->matrix.crsAuthId );
    addValue( QObject::tr( "瓦片方案" ), dataset->matrix.scheme == QgsMtpl::TileScheme::Tms
                                                  ? QStringLiteral( "TMS" )
                                                  : QStringLiteral( "XYZ" ) );
    addValue( QObject::tr( "瓦片大小" ), QStringLiteral( "%1 × %1" ).arg( dataset->matrix.tileSize ) );
    addValue( QObject::tr( "缩放级别" ), QStringLiteral( "%1 – %2" ).arg( dataset->minimumZoom ).arg( dataset->maximumZoom ) );
    addValue( QObject::tr( "根矩阵" ), QStringLiteral( "%1 × %2" )
                                             .arg( dataset->matrix.z0MatrixWidth )
                                             .arg( dataset->matrix.z0MatrixHeight ) );
    addValue( QObject::tr( "左上角" ), QStringLiteral( "%1，%2" )
                                             .arg( dataset->matrix.topLeftX, 0, 'g', 16 )
                                             .arg( dataset->matrix.topLeftY, 0, 'g', 16 ) );
    addValue( QObject::tr( "Z0 瓦片跨度" ), QString::number( dataset->matrix.z0TileSpan, 'g', 16 ) );
    addValue( QObject::tr( "比例尺转级别" ), dataset->matrix.scaleToZoomMethod );

    quint64 totalSlots = 0;
    quint64 presentTiles = 0;
    bool sparseStatsOverflow = false;
    for ( const QgsMtpl::TileDatasetPackage &package : dataset->packages )
    {
      for ( const QgsMtpl::TileRangeRecord &range : package.ranges )
      {
        const quint64 width = static_cast<quint64>( range.xMax ) - range.xMin + 1ULL;
        const quint64 height = static_cast<quint64>( range.yMax ) - range.yMin + 1ULL;
        if ( height != 0 && width > std::numeric_limits<quint64>::max() / height )
        {
          sparseStatsOverflow = true;
          break;
        }
        const quint64 slotCount = width * height;
        if ( totalSlots > std::numeric_limits<quint64>::max() - slotCount ||
             presentTiles > std::numeric_limits<quint64>::max() - range.presentCount )
        {
          sparseStatsOverflow = true;
          break;
        }
        totalSlots += slotCount;
        presentTiles += range.presentCount;
      }
      if ( sparseStatsOverflow )
        break;
    }
    if ( sparseStatsOverflow || presentTiles > totalSlots )
    {
      addValue( QObject::tr( "稀疏统计" ), QObject::tr( "统计范围超出支持上限" ) );
    }
    else
    {
      const quint64 emptySlots = totalSlots - presentTiles;
      const double emptyPercent = totalSlots == 0
                                    ? 0.0
                                    : 100.0 * static_cast<double>( emptySlots ) / static_cast<double>( totalSlots );
      addValue( QObject::tr( "稀疏统计" ),
                QObject::tr( "总槽位 %1，存在瓦片 %2，空槽 %3（%4%）" )
                  .arg( totalSlots )
                  .arg( presentTiles )
                  .arg( emptySlots )
                  .arg( QString::number( emptyPercent, 'f', 2 ) ) );
    }

    int plainCount = 0;
    int protectedCount = 0;
    int lockedCount = 0;
    int unknownCount = 0;
    for ( const QgsMtpl::TileDatasetPackage &package : dataset->packages )
    {
      switch ( package.descriptor.encryption )
      {
        case QgsMtpl::EncryptionState::Plain:
          ++plainCount;
          break;
        case QgsMtpl::EncryptionState::Encrypted:
          ++protectedCount;
          break;
        case QgsMtpl::EncryptionState::Locked:
          ++lockedCount;
          break;
        case QgsMtpl::EncryptionState::Unknown:
          ++unknownCount;
          break;
      }
    }
    QStringList storageParts;
    if ( plainCount )
      storageParts << QObject::tr( "未加密 %1" ).arg( plainCount );
    if ( protectedCount )
      storageParts << QObject::tr( "已解锁加密 %1" ).arg( protectedCount );
    if ( lockedCount )
      storageParts << QObject::tr( "待解锁 %1" ).arg( lockedCount );
    if ( unknownCount )
      storageParts << QObject::tr( "未知 %1" ).arg( unknownCount );
    addValue( QObject::tr( "存储状态" ), storageParts.join( QObject::tr( "，" ) ) );
    displayedMetadata = commonDatasetMetadata( *dataset );
  }
  else
  {
    const QgsMtpl::PackageDescriptor &descriptor = mtplLayer->descriptor();
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
    displayedMetadata = visibleMetadata( descriptor.metadata );
  }

  auto *metadataTable = new QTableWidget( displayedMetadata.size(), 2, &dialog );
  metadataTable->setObjectName( QStringLiteral( "mtplMetadataTable" ) );
  metadataTable->setHorizontalHeaderLabels( { QObject::tr( "元数据" ), QObject::tr( "值" ) } );
  metadataTable->horizontalHeader()->setStretchLastSection( true );
  metadataTable->verticalHeader()->hide();
  metadataTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
  int row = 0;
  for ( auto it = displayedMetadata.constBegin(); it != displayedMetadata.constEnd(); ++it )
  {
    metadataTable->setItem( row, 0, new QTableWidgetItem( metadataDisplayName( it.key() ) ) );
    metadataTable->setItem( row, 1, new QTableWidgetItem( metadataValueText( it.value() ) ) );
    ++row;
  }
  layout->addRow( dataset ? QObject::tr( "共同元数据" ) : QObject::tr( "元数据" ), metadataTable );

  auto *buttons = new QDialogButtonBox( QDialogButtonBox::Close, &dialog );
  if ( QPushButton *closeButton = buttons->button( QDialogButtonBox::Close ) )
    closeButton->setText( QObject::tr( "关闭" ) );
  QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
  layout->addRow( buttons );
  dialog.exec();
  return true;
}
