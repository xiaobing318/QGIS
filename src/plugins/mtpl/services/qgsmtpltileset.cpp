/***************************************************************************
  qgsmtpltileset.cpp
  ------------------
  MTPL tile matrix, package addressing, and dataset validation contracts.
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

#include "qgsmtpltileset.h"

#include "qgscoordinatereferencesystem.h"
#include "qgsrectangle.h"
#include "qgsunittypes.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
  constexpr int sMinimumZoom = 0;
  constexpr int sMaximumZoom = 30;
  constexpr double sWebMercatorHalfWorld = 20037508.342789244;
  constexpr quint64 sMaximumMatrixDimension = static_cast<quint64>( std::numeric_limits<quint32>::max() ) + 1ULL;

  void setError( QString *error, const QString &message )
  {
    if ( error )
      *error = message;
  }

  QVariant valueCaseInsensitive( const QVariantMap &map, const QStringList &names )
  {
    // Alias lists are ordered from the canonical field to legacy fallbacks.
    // Honor that order instead of QVariantMap's key sort order.
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

  bool containsKeyCaseInsensitive( const QVariantMap &map, const QStringList &names )
  {
    for ( auto it = map.constBegin(); it != map.constEnd(); ++it )
    {
      for ( const QString &name : names )
      {
        if ( it.key().compare( name, Qt::CaseInsensitive ) == 0 )
          return true;
      }
    }
    return false;
  }

  void replaceValueCaseInsensitive( QVariantMap &map, const QStringList &names,
                                    const QString &canonicalName, const QVariant &value )
  {
    for ( auto it = map.begin(); it != map.end(); )
    {
      const bool matches = std::any_of( names.constBegin(), names.constEnd(),
        [&it]( const QString &name )
        {
          return it.key().compare( name, Qt::CaseInsensitive ) == 0;
        } );
      if ( matches )
        it = map.erase( it );
      else
        ++it;
    }
    map.insert( canonicalName, value );
  }

  bool integerValue( const QVariant &variant, int &value )
  {
    if ( !variant.isValid() || variant.isNull() )
      return false;
    bool ok = false;
    const qlonglong number = variant.toLongLong( &ok );
    if ( !ok || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max() )
      return false;
    bool doubleOk = false;
    const double doubleValue = variant.toDouble( &doubleOk );
    if ( !doubleOk || !std::isfinite( doubleValue ) || doubleValue != static_cast<double>( number ) )
      return false;
    value = static_cast<int>( number );
    return true;
  }

  bool unsignedValue( const QVariant &variant, quint64 &value )
  {
    if ( !variant.isValid() || variant.isNull() || variant.toString().trimmed().startsWith( QLatin1Char( '-' ) ) )
      return false;
    bool ok = false;
    const quint64 number = variant.toULongLong( &ok );
    if ( !ok )
      return false;
    bool doubleOk = false;
    const double doubleValue = variant.toDouble( &doubleOk );
    if ( !doubleOk || !std::isfinite( doubleValue ) || doubleValue != static_cast<double>( number ) )
      return false;
    value = number;
    return true;
  }

  bool unsigned32Value( const QVariant &variant, quint32 &value )
  {
    quint64 number = 0;
    if ( !unsignedValue( variant, number ) || number > std::numeric_limits<quint32>::max() )
      return false;
    value = static_cast<quint32>( number );
    return true;
  }

  bool finiteDoubleValue( const QVariant &variant, double &value )
  {
    bool ok = false;
    const double number = variant.toDouble( &ok );
    if ( !ok || !std::isfinite( number ) )
      return false;
    value = number;
    return true;
  }

  bool coordinatePair( const QVariant &variant, double &x, double &y )
  {
    const QVariantList values = variant.toList();
    return values.size() == 2 && finiteDoubleValue( values.at( 0 ), x ) && finiteDoubleValue( values.at( 1 ), y );
  }

  bool coordinateBounds( const QVariant &variant, double &xMin, double &yMin, double &xMax, double &yMax )
  {
    const QVariantList values = variant.toList();
    return values.size() == 4 &&
           finiteDoubleValue( values.at( 0 ), xMin ) && finiteDoubleValue( values.at( 1 ), yMin ) &&
           finiteDoubleValue( values.at( 2 ), xMax ) && finiteDoubleValue( values.at( 3 ), yMax ) &&
           xMin <= xMax && yMin <= yMax;
  }

  bool parseScheme( const QVariant &variant, QgsMtpl::TileScheme &scheme )
  {
    const QString value = variant.toString().trimmed().toLower();
    if ( value == QLatin1String( "xyz" ) )
    {
      scheme = QgsMtpl::TileScheme::Xyz;
      return true;
    }
    if ( value == QLatin1String( "tms" ) )
    {
      scheme = QgsMtpl::TileScheme::Tms;
      return true;
    }
    return false;
  }

  bool parseCrs( const QString &definition, QgsCoordinateReferenceSystem &crs )
  {
    return !definition.trimmed().isEmpty() && crs.createFromUserInput( definition.trimmed() ) && crs.isValid();
  }

  bool sameCrs( const QString &left, const QString &right )
  {
    QgsCoordinateReferenceSystem leftCrs;
    QgsCoordinateReferenceSystem rightCrs;
    return parseCrs( left, leftCrs ) && parseCrs( right, rightCrs ) && leftCrs == rightCrs;
  }

  bool isWebMercator( const QString &definition )
  {
    return sameCrs( definition, QStringLiteral( "EPSG:3857" ) );
  }

  bool nearlyEqual( double left, double right )
  {
    const double magnitude = std::max( { 1.0, std::fabs( left ), std::fabs( right ) } );
    return std::fabs( left - right ) <= magnitude * 1e-12;
  }

  bool sameMatrixGeometry( const QgsMtpl::TileMatrixDefinition &left,
                           const QgsMtpl::TileMatrixDefinition &right,
                           QString *difference )
  {
    if ( !sameCrs( left.crsAuthId, right.crsAuthId ) )
    {
      setError( difference, QStringLiteral( "坐标参考系不一致" ) );
      return false;
    }
    if ( left.scheme != right.scheme )
    {
      setError( difference, QStringLiteral( "XYZ/TMS 行号方案不一致" ) );
      return false;
    }
    if ( left.tileSize != right.tileSize )
    {
      setError( difference, QStringLiteral( "瓦片尺寸不一致" ) );
      return false;
    }
    if ( !nearlyEqual( left.topLeftX, right.topLeftX ) || !nearlyEqual( left.topLeftY, right.topLeftY ) ||
         !nearlyEqual( left.z0TileSpan, right.z0TileSpan ) ||
         left.z0MatrixWidth != right.z0MatrixWidth || left.z0MatrixHeight != right.z0MatrixHeight )
    {
      setError( difference, QStringLiteral( "瓦片矩阵原点、跨度或根矩阵尺寸不一致" ) );
      return false;
    }
    if ( left.scaleToZoomMethod.compare( right.scaleToZoomMethod, Qt::CaseInsensitive ) != 0 )
    {
      setError( difference, QStringLiteral( "比例尺到缩放级别的映射方法不一致" ) );
      return false;
    }
    if ( left.zoomRangeExplicit && right.zoomRangeExplicit &&
         ( left.minimumZoom != right.minimumZoom || left.maximumZoom != right.maximumZoom ) )
    {
      setError( difference, QStringLiteral( "声明的缩放级别范围不一致" ) );
      return false;
    }
    if ( left.hasBounds && right.hasBounds &&
         ( !sameCrs( left.boundsCrsAuthId, right.boundsCrsAuthId ) ||
           !nearlyEqual( left.boundsXMinimum, right.boundsXMinimum ) ||
           !nearlyEqual( left.boundsYMinimum, right.boundsYMinimum ) ||
           !nearlyEqual( left.boundsXMaximum, right.boundsXMaximum ) ||
           !nearlyEqual( left.boundsYMaximum, right.boundsYMaximum ) ) )
    {
      setError( difference, QStringLiteral( "数据边界不一致" ) );
      return false;
    }
    if ( difference )
      difference->clear();
    return true;
  }

  bool standardWebMercatorGeometry( const QgsMtpl::TileMatrixDefinition &matrix )
  {
    return isWebMercator( matrix.crsAuthId ) && matrix.z0MatrixWidth == 1 && matrix.z0MatrixHeight == 1 &&
           nearlyEqual( matrix.topLeftX, -sWebMercatorHalfWorld ) &&
           nearlyEqual( matrix.topLeftY, sWebMercatorHalfWorld ) &&
           nearlyEqual( matrix.z0TileSpan, 2.0 * sWebMercatorHalfWorld );
  }

  void addIssue( QgsMtpl::TileDatasetBuildResult &result,
                 QgsMtpl::TileDatasetIssueSeverity severity,
                 const QString &code,
                 const QString &message,
                 const QString &path = QString() )
  {
    QgsMtpl::TileDatasetIssue issue;
    issue.severity = severity;
    issue.code = code;
    issue.message = message;
    issue.packagePath = path;
    result.issues.append( issue );
  }

  bool hasErrors( const QgsMtpl::TileDatasetBuildResult &result )
  {
    return std::any_of( result.issues.constBegin(), result.issues.constEnd(),
      []( const QgsMtpl::TileDatasetIssue &issue )
      {
        return issue.severity == QgsMtpl::TileDatasetIssueSeverity::Error;
      } );
  }

  QString metadataString( const QVariantMap &metadata, const QStringList &names )
  {
    return valueCaseInsensitive( metadata, names ).toString().trimmed();
  }

  QgsMtpl::PayloadType metadataPayload( const QVariantMap &metadata )
  {
    const QString type = valueCaseInsensitive(
      metadata,
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

  bool parseRangeRecords( const QgsMtpl::PackageDescriptor &package,
                          QList<QgsMtpl::TileRangeRecord> &records,
                          QString *error )
  {
    records.clear();
    const QVariant rangesValue = valueCaseInsensitive( package.metadata, { QStringLiteral( "_mtplRangeBounds" ) } );
    if ( !rangesValue.isValid() )
      return true;

    const QVariantList ranges = rangesValue.toList();
    for ( qsizetype index = 0; index < ranges.size(); ++index )
    {
      const QVariantMap range = ranges.at( index ).toMap();
      QgsMtpl::TileRangeRecord record;
      if ( range.isEmpty() ||
           !integerValue( valueCaseInsensitive( range, { QStringLiteral( "zoom" ) } ), record.zoom ) ||
           !unsigned32Value( valueCaseInsensitive( range, { QStringLiteral( "xMin" ) } ), record.xMin ) ||
           !unsigned32Value( valueCaseInsensitive( range, { QStringLiteral( "xMax" ) } ), record.xMax ) ||
           !unsigned32Value( valueCaseInsensitive( range, { QStringLiteral( "yMin" ) } ), record.yMin ) ||
           !unsigned32Value( valueCaseInsensitive( range, { QStringLiteral( "yMax" ) } ), record.yMax ) ||
           !unsigned32Value( valueCaseInsensitive( range, { QStringLiteral( "presentXMin" ) } ), record.presentXMin ) ||
           !unsigned32Value( valueCaseInsensitive( range, { QStringLiteral( "presentXMax" ) } ), record.presentXMax ) ||
           !unsigned32Value( valueCaseInsensitive( range, { QStringLiteral( "presentYMin" ) } ), record.presentYMin ) ||
           !unsigned32Value( valueCaseInsensitive( range, { QStringLiteral( "presentYMax" ) } ), record.presentYMax ) ||
           !unsignedValue( valueCaseInsensitive( range, { QStringLiteral( "presentCount" ) } ), record.presentCount ) )
      {
        setError( error, QStringLiteral( "第 %1 个 MTPL 范围摘要缺少有效字段。" ).arg( index + 1 ) );
        return false;
      }
      QString recordError;
      if ( !record.isValid( &recordError ) )
      {
        setError( error, QStringLiteral( "第 %1 个 MTPL 范围摘要无效：%2" ).arg( index + 1 ).arg( recordError ) );
        return false;
      }
      records.append( record );
    }
    if ( error )
      error->clear();
    return true;
  }

  bool rangeMatchesPackage( const QgsMtpl::TileRangeRecord &range,
                            const QgsMtpl::TilePackageAddress &address,
                            const QgsMtpl::TileMatrixDefinition &matrix,
                            QString *error )
  {
    if ( range.zoom < address.minZoom || range.zoom > address.maxZoom )
    {
      setError( error, QStringLiteral( "范围缩放级别 %1 不在文件名声明的 %2-%3 内。" )
                .arg( range.zoom ).arg( address.minZoom ).arg( address.maxZoom ) );
      return false;
    }

    bool widthOk = false;
    bool heightOk = false;
    const quint64 width = matrix.matrixWidth( range.zoom, &widthOk );
    const quint64 height = matrix.matrixHeight( range.zoom, &heightOk );
    if ( !widthOk || !heightOk || range.xMax >= width || range.yMax >= height )
    {
      setError( error, QStringLiteral( "范围坐标超出缩放级别 %1 的瓦片矩阵。" ).arg( range.zoom ) );
      return false;
    }

    const int shift = range.zoom - address.baseZoom;
    if ( shift < 0 ||
         ( static_cast<quint64>( range.xMin ) >> shift ) != address.packageX ||
         ( static_cast<quint64>( range.xMax ) >> shift ) != address.packageX ||
         ( static_cast<quint64>( range.yMin ) >> shift ) != address.packageY ||
         ( static_cast<quint64>( range.yMax ) >> shift ) != address.packageY )
    {
      setError( error, QStringLiteral( "范围坐标不属于文件名指定的基础瓦片 %1-%2-%3。" )
                .arg( address.baseZoom ).arg( address.packageX ).arg( address.packageY ) );
      return false;
    }
    if ( error )
      error->clear();
    return true;
  }

  bool rangeFitsMatrix( const QgsMtpl::TileRangeRecord &range,
                        const QgsMtpl::TileMatrixDefinition &matrix,
                        QString *error )
  {
    if ( range.zoom < matrix.minimumZoom || range.zoom > matrix.maximumZoom )
    {
      setError( error, QStringLiteral( "缩放级别 %1 不在瓦片矩阵声明的 %2-%3 内。" )
                .arg( range.zoom ).arg( matrix.minimumZoom ).arg( matrix.maximumZoom ) );
      return false;
    }
    bool widthOk = false;
    bool heightOk = false;
    const quint64 width = matrix.matrixWidth( range.zoom, &widthOk );
    const quint64 height = matrix.matrixHeight( range.zoom, &heightOk );
    if ( !widthOk || !heightOk || range.xMax >= width || range.yMax >= height )
    {
      setError( error, QStringLiteral( "缩放级别 %1 的范围坐标超出瓦片矩阵。" ).arg( range.zoom ) );
      return false;
    }
    if ( error )
      error->clear();
    return true;
  }

  bool bandsEqual( const QgsMtpl::PartitionRule &left, const QgsMtpl::PartitionRule &right )
  {
    return left.bands == right.bands;
  }

  QList<QgsMtpl::PartitionRule> candidateRules( const QgsMtpl::TileDatasetBuildOptions &options,
                                                QgsMtpl::TileDatasetBuildResult &result )
  {
    QList<QgsMtpl::PartitionRule> rules = QgsMtpl::PartitionRuleStore::builtInRules();
    QSet<QString> ids;
    QSet<QString> names;
    for ( const QgsMtpl::PartitionRule &rule : rules )
    {
      ids.insert( rule.id.toCaseFolded() );
      names.insert( rule.name.trimmed().toCaseFolded() );
    }

    for ( const QgsMtpl::PartitionRule &rule : options.availableRules )
    {
      const QString folded = rule.id.toCaseFolded();
      if ( ids.contains( folded ) )
      {
        QgsMtpl::PartitionRule existing;
        QgsMtpl::PartitionRuleStore::findRule( rules, rule.id, existing );
        if ( !existing.id.isEmpty() && existing.builtIn && rule.builtIn && bandsEqual( existing, rule ) )
          continue;
        addIssue( result, QgsMtpl::TileDatasetIssueSeverity::Error, QStringLiteral( "duplicate-rule-id" ),
                  QStringLiteral( "可用规则中存在重复或受保护的 ID“%1”。" ).arg( rule.id ) );
        continue;
      }
      QString ruleError;
      if ( rule.builtIn || !rule.isValid( &ruleError ) || names.contains( rule.name.trimmed().toCaseFolded() ) )
      {
        if ( ruleError.isEmpty() && names.contains( rule.name.trimmed().toCaseFolded() ) )
          ruleError = QStringLiteral( "规则名称重复或与内置规则冲突" );
        addIssue( result, QgsMtpl::TileDatasetIssueSeverity::Error, QStringLiteral( "invalid-rule" ),
                  QStringLiteral( "可用规则“%1”无效：%2" ).arg( rule.name, ruleError ) );
        continue;
      }
      rules.append( rule );
      ids.insert( folded );
      names.insert( rule.name.trimmed().toCaseFolded() );
    }
    return rules;
  }

  bool selectRule( const QList<QgsMtpl::TilePackageAddress> &addresses,
                   const QgsMtpl::TileDatasetBuildOptions &options,
                   QgsMtpl::TileDatasetBuildResult &result,
                   QgsMtpl::PartitionRule &selected )
  {
    const QList<QgsMtpl::PartitionRule> rules = candidateRules( options, result );
    if ( !options.ruleSnapshot.id.isEmpty() )
    {
      QString ruleError;
      if ( !options.ruleSnapshot.isValid( &ruleError ) )
      {
        addIssue( result, QgsMtpl::TileDatasetIssueSeverity::Error, QStringLiteral( "invalid-rule-snapshot" ),
                  QStringLiteral( "所选规则快照无效：%1" ).arg( ruleError ) );
        return false;
      }
      const QList<QgsMtpl::PartitionRule> builtIns = QgsMtpl::PartitionRuleStore::builtInRules();
      for ( const QgsMtpl::PartitionRule &builtIn : builtIns )
      {
        if ( builtIn.id.compare( options.ruleSnapshot.id, Qt::CaseInsensitive ) == 0 &&
             ( builtIn.id != options.ruleSnapshot.id || !options.ruleSnapshot.builtIn ||
               !bandsEqual( builtIn, options.ruleSnapshot ) ) )
        {
          addIssue( result, QgsMtpl::TileDatasetIssueSeverity::Error, QStringLiteral( "protected-rule-snapshot" ),
                    QStringLiteral( "规则快照不能覆盖内置规则“%1”。" ).arg( builtIn.name ) );
          return false;
        }
      }
      if ( !options.selectedRuleId.isEmpty() && options.selectedRuleId != options.ruleSnapshot.id )
      {
        addIssue( result, QgsMtpl::TileDatasetIssueSeverity::Error, QStringLiteral( "rule-selection-conflict" ),
                  QStringLiteral( "所选规则 ID 与规则快照 ID 不一致。" ) );
        return false;
      }
      selected = options.ruleSnapshot;
    }
    else if ( !options.selectedRuleId.isEmpty() )
    {
      if ( !QgsMtpl::PartitionRuleStore::findRule( rules, options.selectedRuleId, selected ) )
      {
        addIssue( result, QgsMtpl::TileDatasetIssueSeverity::Error, QStringLiteral( "unknown-rule" ),
                  QStringLiteral( "找不到 ID 为“%1”的分包规则。" ).arg( options.selectedRuleId ) );
        return false;
      }
    }
    else
    {
      QList<QgsMtpl::PartitionRule> matches;
      for ( const QgsMtpl::PartitionRule &rule : rules )
      {
        bool matchesAll = true;
        for ( const QgsMtpl::TilePackageAddress &address : addresses )
        {
          const QgsMtpl::PartitionBand *band = rule.bandForZoom( address.minZoom );
          if ( !band || *band != address.band() )
          {
            matchesAll = false;
            break;
          }
        }
        if ( matchesAll )
          matches.append( rule );
      }
      if ( matches.size() != 1 )
      {
        const QString message = matches.isEmpty()
          ? QStringLiteral( "数据包文件名不匹配任何可用分包规则。" )
          : QStringLiteral( "数据包文件名匹配多个规则，请明确选择一个分包规则。" );
        addIssue( result, QgsMtpl::TileDatasetIssueSeverity::Error, QStringLiteral( "rule-inference-failed" ), message );
        return false;
      }
      selected = matches.constFirst();
    }

    for ( const QgsMtpl::TilePackageAddress &address : addresses )
    {
      const QgsMtpl::PartitionBand *band = selected.bandForZoom( address.minZoom );
      if ( !band || *band != address.band() )
      {
        addIssue( result, QgsMtpl::TileDatasetIssueSeverity::Error, QStringLiteral( "package-rule-mismatch" ),
                  QStringLiteral( "数据包“%1”不属于规则“%2”。" ).arg( address.fileName(), selected.name ) );
        return false;
      }
    }
    return true;
  }

  bool isPathInside( const QString &directory, const QString &path, QString &relativePath )
  {
    relativePath = QDir::fromNativeSeparators( QDir( directory ).relativeFilePath( path ) );
    return !QDir::isAbsolutePath( relativePath ) && relativePath != QLatin1String( ".." ) &&
           !relativePath.startsWith( QLatin1String( "../" ) );
  }
}

bool QgsMtpl::TileMatrixDefinition::isValid( QString *error ) const
{
  QgsCoordinateReferenceSystem crs;
  if ( !parseCrs( crsAuthId, crs ) )
  {
    setError( error, QStringLiteral( "瓦片矩阵缺少有效的坐标参考系。" ) );
    return false;
  }
  if ( tileSize <= 0 || tileSize > 16384 )
  {
    setError( error, QStringLiteral( "瓦片像素尺寸必须位于 1 至 16384。" ) );
    return false;
  }
  if ( !std::isfinite( topLeftX ) || !std::isfinite( topLeftY ) ||
       !std::isfinite( z0TileSpan ) || z0TileSpan <= 0.0 )
  {
    setError( error, QStringLiteral( "瓦片矩阵原点和 0 级瓦片跨度必须是有效有限数值。" ) );
    return false;
  }
  if ( z0MatrixWidth == 0 || z0MatrixHeight == 0 )
  {
    setError( error, QStringLiteral( "0 级瓦片矩阵宽度和高度必须大于 0。" ) );
    return false;
  }
  const long double rootXMaximum = static_cast<long double>( topLeftX ) +
                                   static_cast<long double>( z0TileSpan ) * z0MatrixWidth;
  const long double rootYMinimum = static_cast<long double>( topLeftY ) -
                                   static_cast<long double>( z0TileSpan ) * z0MatrixHeight;
  const long double maximumDouble = std::numeric_limits<double>::max();
  if ( !std::isfinite( rootXMaximum ) || !std::isfinite( rootYMinimum ) ||
       std::abs( rootXMaximum ) > maximumDouble || std::abs( rootYMinimum ) > maximumDouble ||
       static_cast<double>( rootXMaximum ) <= topLeftX ||
       static_cast<double>( rootYMinimum ) >= topLeftY )
  {
    setError( error, QStringLiteral( "瓦片根矩阵范围端点超出有限双精度坐标范围。" ) );
    return false;
  }
  constexpr long double pixelSizeMeters = 0.00028L;
  const long double unitToMeters = QgsUnitTypes::fromUnitToUnitFactor( crs.mapUnits(), Qgis::DistanceUnit::Meters );
  const long double qgisScale = static_cast<long double>( z0TileSpan ) * unitToMeters /
                                ( pixelSizeMeters * static_cast<long double>( tileSize ) );
  if ( !std::isfinite( qgisScale ) || qgisScale <= 0.0L || qgisScale > maximumDouble )
  {
    setError( error, QStringLiteral( "瓦片矩阵派生的 QGIS 比例尺无效或溢出。" ) );
    return false;
  }
  if ( minimumZoom < sMinimumZoom || maximumZoom > sMaximumZoom || minimumZoom > maximumZoom )
  {
    setError( error, QStringLiteral( "瓦片矩阵缩放级别必须是 0 至 30 内的有效区间。" ) );
    return false;
  }
  if ( scaleToZoomMethod.compare( QLatin1String( "mapbox" ), Qt::CaseInsensitive ) != 0 &&
       scaleToZoomMethod.compare( QLatin1String( "esri" ), Qt::CaseInsensitive ) != 0 )
  {
    setError( error, QStringLiteral( "比例尺到缩放级别映射必须是 mapbox 或 esri。" ) );
    return false;
  }
  if ( hasBounds )
  {
    QgsCoordinateReferenceSystem boundsCrs;
    if ( !parseCrs( boundsCrsAuthId, boundsCrs ) ||
         !std::isfinite( boundsXMinimum ) || !std::isfinite( boundsYMinimum ) ||
         !std::isfinite( boundsXMaximum ) || !std::isfinite( boundsYMaximum ) ||
         boundsXMinimum >= boundsXMaximum || boundsYMinimum >= boundsYMaximum )
    {
      setError( error, QStringLiteral( "bounds 与 bounds_crs 必须成对提供且内容有效。" ) );
      return false;
    }
  }
  else if ( !boundsCrsAuthId.isEmpty() )
  {
    setError( error, QStringLiteral( "提供 bounds_crs 时也必须提供 bounds。" ) );
    return false;
  }
  if ( hasBounds && sameCrs( boundsCrsAuthId, crsAuthId ) )
  {
    if ( !std::isfinite( rootXMaximum ) || !std::isfinite( rootYMinimum ) ||
         static_cast<long double>( boundsXMinimum ) < static_cast<long double>( topLeftX ) ||
         static_cast<long double>( boundsXMaximum ) > rootXMaximum ||
         static_cast<long double>( boundsYMinimum ) < rootYMinimum ||
         static_cast<long double>( boundsYMaximum ) > static_cast<long double>( topLeftY ) )
    {
      setError( error, QStringLiteral( "bounds 超出瓦片根矩阵范围。" ) );
      return false;
    }
  }

  bool widthOk = false;
  bool heightOk = false;
  const quint64 width = matrixWidth( maximumZoom, &widthOk );
  const quint64 height = matrixHeight( maximumZoom, &heightOk );
  if ( !widthOk || !heightOk || width > sMaximumMatrixDimension || height > sMaximumMatrixDimension )
  {
    setError( error, QStringLiteral( "瓦片矩阵尺寸在最大缩放级别超出 MTPL 的 32 位坐标范围。" ) );
    return false;
  }
  bool spanOk = false;
  tileSpan( maximumZoom, &spanOk );
  if ( !spanOk )
  {
    setError( error, QStringLiteral( "瓦片跨度在最大缩放级别无效。" ) );
    return false;
  }
  if ( error )
    error->clear();
  return true;
}

quint64 QgsMtpl::TileMatrixDefinition::matrixWidth( int zoom, bool *ok ) const
{
  const bool valid = zoom >= sMinimumZoom && zoom <= sMaximumZoom && z0MatrixWidth > 0 &&
                     z0MatrixWidth <= ( std::numeric_limits<quint64>::max() >> zoom );
  if ( ok )
    *ok = valid;
  return valid ? z0MatrixWidth << zoom : 0;
}

quint64 QgsMtpl::TileMatrixDefinition::matrixHeight( int zoom, bool *ok ) const
{
  const bool valid = zoom >= sMinimumZoom && zoom <= sMaximumZoom && z0MatrixHeight > 0 &&
                     z0MatrixHeight <= ( std::numeric_limits<quint64>::max() >> zoom );
  if ( ok )
    *ok = valid;
  return valid ? z0MatrixHeight << zoom : 0;
}

double QgsMtpl::TileMatrixDefinition::tileSpan( int zoom, bool *ok ) const
{
  const double span = zoom >= sMinimumZoom && zoom <= sMaximumZoom
    ? std::ldexp( z0TileSpan, -zoom )
    : 0.0;
  const bool valid = zoom >= sMinimumZoom && zoom <= sMaximumZoom &&
                     std::isfinite( z0TileSpan ) && z0TileSpan > 0.0 &&
                     std::isfinite( span ) && span > 0.0;
  if ( ok )
    *ok = valid;
  return valid ? span : 0.0;
}

bool QgsMtpl::TileMatrixDefinition::fromMetadata( const QVariantMap &metadata,
                                                  int packageTileSize,
                                                  int observedMinimumZoom,
                                                  int observedMaximumZoom,
                                                  TileMatrixDefinition &definition,
                                                  QString *error )
{
  definition = TileMatrixDefinition();
  if ( packageTileSize <= 0 || packageTileSize > 16384 )
  {
    setError( error, QStringLiteral( "数据包头中的瓦片尺寸无效。" ) );
    return false;
  }
  definition.tileSize = packageTileSize;

  const QStringList matrixNames { QStringLiteral( "mtpl_tile_matrix" ), QStringLiteral( "mtplTileMatrix" ) };
  const QVariant matrixValue = valueCaseInsensitive( metadata, matrixNames );
  const bool hasMatrixContract = containsKeyCaseInsensitive( metadata, matrixNames );
  QVariantMap matrixMetadata;
  if ( hasMatrixContract )
  {
    if ( !matrixValue.isValid() || matrixValue.isNull() )
    {
      setError( error, QStringLiteral( "mtpl_tile_matrix 必须是非空对象。" ) );
      return false;
    }
    matrixMetadata = matrixValue.toMap();
    if ( matrixMetadata.isEmpty() )
    {
      setError( error, QStringLiteral( "mtpl_tile_matrix 必须是非空对象。" ) );
      return false;
    }

    int version = -1;
    if ( !integerValue( valueCaseInsensitive( matrixMetadata, { QStringLiteral( "version" ) } ), version ) || version != 1 )
    {
      setError( error, QStringLiteral( "mtpl_tile_matrix.version 必须为 1。" ) );
      return false;
    }

    definition.crsAuthId = valueCaseInsensitive( matrixMetadata, { QStringLiteral( "crs" ) } ).toString().trimmed();
    if ( definition.crsAuthId.isEmpty() )
    {
      setError( error, QStringLiteral( "mtpl_tile_matrix.crs 不能为空。" ) );
      return false;
    }
    const QVariant schemeValue = valueCaseInsensitive( matrixMetadata, { QStringLiteral( "scheme" ) } );
    if ( !parseScheme( schemeValue, definition.scheme ) )
    {
      setError( error, QStringLiteral( "mtpl_tile_matrix.scheme 必须是 xyz 或 tms。" ) );
      return false;
    }
    const QString outerCrs = metadataString( metadata,
      { QStringLiteral( "crs" ), QStringLiteral( "crsAuthId" ), QStringLiteral( "srs" ) } );
    if ( !outerCrs.isEmpty() && !sameCrs( outerCrs, definition.crsAuthId ) )
    {
      setError( error, QStringLiteral( "顶层 CRS 与 mtpl_tile_matrix.crs 不一致。" ) );
      return false;
    }
    const QVariant outerSchemeValue = valueCaseInsensitive( metadata, { QStringLiteral( "scheme" ) } );
    if ( outerSchemeValue.isValid() )
    {
      TileScheme outerScheme = TileScheme::Xyz;
      if ( !parseScheme( outerSchemeValue, outerScheme ) || outerScheme != definition.scheme )
      {
        setError( error, QStringLiteral( "顶层 scheme 与 mtpl_tile_matrix.scheme 不一致。" ) );
        return false;
      }
    }
    if ( !coordinatePair( valueCaseInsensitive( matrixMetadata, { QStringLiteral( "top_left" ), QStringLiteral( "topLeft" ) } ),
                          definition.topLeftX, definition.topLeftY ) ||
         !finiteDoubleValue( valueCaseInsensitive( matrixMetadata, { QStringLiteral( "z0_tile_span" ), QStringLiteral( "z0TileSpan" ) } ),
                             definition.z0TileSpan ) || definition.z0TileSpan <= 0.0 ||
         !unsignedValue( valueCaseInsensitive( matrixMetadata, { QStringLiteral( "z0_matrix_width" ), QStringLiteral( "z0MatrixWidth" ) } ),
                         definition.z0MatrixWidth ) ||
         !unsignedValue( valueCaseInsensitive( matrixMetadata, { QStringLiteral( "z0_matrix_height" ), QStringLiteral( "z0MatrixHeight" ) } ),
                         definition.z0MatrixHeight ) )
    {
      setError( error, QStringLiteral( "mtpl_tile_matrix 的 top_left、z0_tile_span 或根矩阵尺寸无效。" ) );
      return false;
    }

    const QVariant methodValue = valueCaseInsensitive( matrixMetadata,
      { QStringLiteral( "scale_to_zoom_method" ), QStringLiteral( "scaleToZoomMethod" ) } );
    definition.scaleToZoomMethod = methodValue.isValid()
      ? methodValue.toString().trimmed().toLower()
      : QStringLiteral( "mapbox" );

    const QVariant declaredTileSize = valueCaseInsensitive( matrixMetadata,
      { QStringLiteral( "tile_size" ), QStringLiteral( "tileSize" ) } );
    if ( declaredTileSize.isValid() )
    {
      int metadataTileSize = 0;
      if ( !integerValue( declaredTileSize, metadataTileSize ) || metadataTileSize != packageTileSize )
      {
        setError( error, QStringLiteral( "mtpl_tile_matrix.tile_size 与数据包头中的瓦片尺寸不一致。" ) );
        return false;
      }
    }

    const QVariant boundsValue = valueCaseInsensitive( matrixMetadata, { QStringLiteral( "bounds" ) } );
    const QVariant boundsCrsValue = valueCaseInsensitive( matrixMetadata,
      { QStringLiteral( "bounds_crs" ), QStringLiteral( "boundsCrs" ) } );
    if ( boundsValue.isValid() != boundsCrsValue.isValid() )
    {
      setError( error, QStringLiteral( "mtpl_tile_matrix.bounds 与 bounds_crs 必须成对提供。" ) );
      return false;
    }
    if ( boundsValue.isValid() )
    {
      definition.hasBounds = true;
      definition.boundsCrsAuthId = boundsCrsValue.toString().trimmed();
      if ( !coordinateBounds( boundsValue, definition.boundsXMinimum, definition.boundsYMinimum,
                              definition.boundsXMaximum, definition.boundsYMaximum ) ||
           definition.boundsCrsAuthId.isEmpty() )
      {
        setError( error, QStringLiteral( "mtpl_tile_matrix.bounds 或 bounds_crs 无效。" ) );
        return false;
      }
    }
    definition.legacyFallbackApplied = false;
  }
  else
  {
    const QString declaredCrs = metadataString( metadata,
      { QStringLiteral( "crs" ), QStringLiteral( "crsAuthId" ), QStringLiteral( "srs" ) } );
    definition.crsAuthId = declaredCrs.isEmpty() ? QStringLiteral( "EPSG:3857" ) : declaredCrs;
    if ( !isWebMercator( definition.crsAuthId ) )
    {
      setError( error, QStringLiteral( "非 EPSG:3857 数据包必须提供 mtpl_tile_matrix version 1 元数据。" ) );
      return false;
    }

    const QVariant schemeValue = valueCaseInsensitive( metadata, { QStringLiteral( "scheme" ) } );
    if ( schemeValue.isValid() && !parseScheme( schemeValue, definition.scheme ) )
    {
      setError( error, QStringLiteral( "旧版数据包的 scheme 必须是 xyz 或 tms。" ) );
      return false;
    }
    definition.topLeftX = -sWebMercatorHalfWorld;
    definition.topLeftY = sWebMercatorHalfWorld;
    definition.z0TileSpan = 2.0 * sWebMercatorHalfWorld;
    definition.z0MatrixWidth = 1;
    definition.z0MatrixHeight = 1;
    definition.scaleToZoomMethod = QStringLiteral( "mapbox" );
    definition.legacyFallbackApplied = true;

    const QVariant declaredTileSize = valueCaseInsensitive( metadata,
      { QStringLiteral( "tile_size" ), QStringLiteral( "tileSize" ) } );
    if ( declaredTileSize.isValid() )
    {
      int metadataTileSize = 0;
      if ( !integerValue( declaredTileSize, metadataTileSize ) || metadataTileSize != packageTileSize )
      {
        setError( error, QStringLiteral( "元数据中的 tileSize 与数据包头中的瓦片尺寸不一致。" ) );
        return false;
      }
    }
  }

  // Legacy top-level zoom fields describe the tiles observed in one PTP file,
  // not the common tile matrix. Only the versioned matrix contract can narrow
  // the matrix zoom range shared by every package in a dataset.
  const QVariant minimumValue = hasMatrixContract
    ? valueCaseInsensitive( matrixMetadata,
        { QStringLiteral( "min_zoom" ), QStringLiteral( "minZoom" ), QStringLiteral( "min_zoom_level" ),
          QStringLiteral( "minimumZoom" ) } )
    : QVariant();
  const QVariant maximumValue = hasMatrixContract
    ? valueCaseInsensitive( matrixMetadata,
        { QStringLiteral( "max_zoom" ), QStringLiteral( "maxZoom" ), QStringLiteral( "max_zoom_level" ),
          QStringLiteral( "maximumZoom" ) } )
    : QVariant();
  if ( minimumValue.isValid() != maximumValue.isValid() )
  {
    setError( error, QStringLiteral( "最小和最大缩放级别必须成对提供。" ) );
    return false;
  }
  if ( minimumValue.isValid() )
  {
    if ( !integerValue( minimumValue, definition.minimumZoom ) ||
         !integerValue( maximumValue, definition.maximumZoom ) )
    {
      setError( error, QStringLiteral( "元数据中的最小和最大缩放级别必须是整数。" ) );
      return false;
    }
    definition.zoomRangeExplicit = true;
  }

  if ( ( observedMinimumZoom < 0 ) != ( observedMaximumZoom < 0 ) ||
       observedMinimumZoom > observedMaximumZoom || observedMaximumZoom > sMaximumZoom )
  {
    setError( error, QStringLiteral( "数据包探测得到的缩放级别范围无效。" ) );
    return false;
  }
  if ( definition.zoomRangeExplicit && observedMinimumZoom >= 0 &&
       ( observedMinimumZoom < definition.minimumZoom || observedMaximumZoom > definition.maximumZoom ) )
  {
    setError( error, QStringLiteral( "数据包实际缩放级别超出元数据声明范围。" ) );
    return false;
  }
  return definition.isValid( error );
}

bool QgsMtpl::TileMatrixDefinition::fromPackageDescriptor( const PackageDescriptor &package,
                                                           TileMatrixDefinition &definition,
                                                           QString *error )
{
  QVariantMap metadata = package.metadata;
  if ( package.displayOverridesApplied )
  {
    const QStringList matrixNames { QStringLiteral( "mtpl_tile_matrix" ), QStringLiteral( "mtplTileMatrix" ) };
    const QVariant matrixValue = valueCaseInsensitive( metadata, matrixNames );
    if ( matrixValue.isValid() && !matrixValue.isNull() )
    {
      QVariantMap matrixMetadata = matrixValue.toMap();
      if ( !matrixMetadata.isEmpty() )
      {
        replaceValueCaseInsensitive( matrixMetadata, { QStringLiteral( "crs" ) },
                                     QStringLiteral( "crs" ), package.crsAuthId );
        replaceValueCaseInsensitive( matrixMetadata, { QStringLiteral( "scheme" ) },
                                     QStringLiteral( "scheme" ), package.scheme );
        replaceValueCaseInsensitive( metadata, matrixNames,
                                     QStringLiteral( "mtpl_tile_matrix" ), matrixMetadata );
      }
    }

    replaceValueCaseInsensitive( metadata,
                                 { QStringLiteral( "crs" ), QStringLiteral( "crsAuthId" ), QStringLiteral( "srs" ) },
                                 QStringLiteral( "crs" ), package.crsAuthId );
    replaceValueCaseInsensitive( metadata, { QStringLiteral( "scheme" ) },
                                 QStringLiteral( "scheme" ), package.scheme );
  }
  return fromMetadata( metadata, package.tileSize, package.minimumZoom, package.maximumZoom,
                       definition, error );
}

QgsMtpl::PartitionBand QgsMtpl::TilePackageAddress::band() const
{
  return { minZoom, maxZoom, baseZoom };
}

QString QgsMtpl::TilePackageAddress::fileName() const
{
  return QStringLiteral( "%1-%2-%3-%4-%5.ptp" )
    .arg( minZoom ).arg( maxZoom ).arg( baseZoom ).arg( packageX ).arg( packageY );
}

QString QgsMtpl::TilePackageAddress::key() const
{
  return QStringLiteral( "%1/%2/%3/%4/%5" )
    .arg( minZoom ).arg( maxZoom ).arg( baseZoom ).arg( packageX ).arg( packageY );
}

bool QgsMtpl::TilePackageAddress::isValid( QString *error ) const
{
  return band().isValid( error );
}

bool QgsMtpl::TilePackageAddress::operator==( const TilePackageAddress &other ) const
{
  return minZoom == other.minZoom && maxZoom == other.maxZoom && baseZoom == other.baseZoom &&
         packageX == other.packageX && packageY == other.packageY;
}

bool QgsMtpl::TileRangeRecord::isValid( QString *error ) const
{
  if ( zoom < sMinimumZoom || zoom > sMaximumZoom )
  {
    setError( error, QStringLiteral( "范围缩放级别必须位于 0 至 30。" ) );
    return false;
  }
  if ( xMin > xMax || yMin > yMax )
  {
    setError( error, QStringLiteral( "范围边界顺序无效。" ) );
    return false;
  }
  const quint64 width = static_cast<quint64>( xMax ) - xMin + 1ULL;
  const quint64 height = static_cast<quint64>( yMax ) - yMin + 1ULL;
  if ( height != 0 && width > std::numeric_limits<quint64>::max() / height )
  {
    setError( error, QStringLiteral( "范围容量溢出。" ) );
    return false;
  }
  if ( presentCount > 0 &&
       ( presentXMin > presentXMax || presentYMin > presentYMax ||
         presentXMin < xMin || presentXMax > xMax || presentYMin < yMin || presentYMax > yMax ) )
  {
    setError( error, QStringLiteral( "实际瓦片边界顺序无效或超出声明范围。" ) );
    return false;
  }
  if ( presentCount > width * height )
  {
    setError( error, QStringLiteral( "实际瓦片数量超出范围容量。" ) );
    return false;
  }
  if ( error )
    error->clear();
  return true;
}

bool QgsMtpl::TileDatasetDescriptor::isValid( QString *error ) const
{
  if ( sourcePath.trimmed().isEmpty() || packages.isEmpty() )
  {
    setError( error, QStringLiteral( "瓦片数据集必须包含来源路径和至少一个数据包。" ) );
    return false;
  }
  QString matrixError;
  if ( !matrix.isValid( &matrixError ) )
  {
    setError( error, QStringLiteral( "瓦片矩阵无效：%1" ).arg( matrixError ) );
    return false;
  }
  if ( minimumZoom < sMinimumZoom || maximumZoom > sMaximumZoom || minimumZoom > maximumZoom )
  {
    setError( error, QStringLiteral( "数据集缩放级别范围无效。" ) );
    return false;
  }
  if ( minimumZoom < matrix.minimumZoom || maximumZoom > matrix.maximumZoom )
  {
    setError( error, QStringLiteral( "数据集缩放级别超出瓦片矩阵声明范围。" ) );
    return false;
  }
  bool qgisWidthOk = false;
  bool qgisHeightOk = false;
  const quint64 qgisWidth = matrix.matrixWidth( maximumZoom, &qgisWidthOk );
  const quint64 qgisHeight = matrix.matrixHeight( maximumZoom, &qgisHeightOk );
  if ( !qgisWidthOk || !qgisHeightOk ||
       qgisWidth > static_cast<quint64>( std::numeric_limits<int>::max() ) ||
       qgisHeight > static_cast<quint64>( std::numeric_limits<int>::max() ) )
  {
    setError( error, QStringLiteral( "数据集最大缩放级别的瓦片矩阵尺寸超出 QGIS 支持范围。" ) );
    return false;
  }
  if ( payload != PayloadType::RasterImage )
  {
    setError( error, QStringLiteral( "PTP 瓦片数据集只支持已确认的 PNG、JPEG 或 WebP 栅格影像载荷。" ) );
    return false;
  }
  if ( hasExtent )
  {
    const QgsRectangle datasetExtent( extentXMinimum, extentYMinimum, extentXMaximum, extentYMaximum );
    const double rootXMaximum = static_cast<double>( static_cast<long double>( matrix.topLeftX ) +
                                                     static_cast<long double>( matrix.z0TileSpan ) * matrix.z0MatrixWidth );
    const double rootYMinimum = static_cast<double>( static_cast<long double>( matrix.topLeftY ) -
                                                     static_cast<long double>( matrix.z0TileSpan ) * matrix.z0MatrixHeight );
    const QgsRectangle rootExtent( matrix.topLeftX, rootYMinimum, rootXMaximum, matrix.topLeftY );
    if ( !std::isfinite( extentXMinimum ) || !std::isfinite( extentYMinimum ) ||
         !std::isfinite( extentXMaximum ) || !std::isfinite( extentYMaximum ) ||
         datasetExtent.isEmpty() || !rootExtent.contains( datasetExtent ) )
    {
      setError( error, QStringLiteral( "数据集范围无效或超出瓦片根矩阵。" ) );
      return false;
    }
  }

  for ( const TileDatasetPackage &package : packages )
  {
    if ( package.descriptor.format != PackageFormat::Ptp ||
         package.descriptor.payload != PayloadType::RasterImage ||
         ( !package.descriptor.isReady() && !package.descriptor.isCredentialedEmptyPtp() ) )
    {
      setError( error, QStringLiteral( "数据集包含格式或载荷类型不一致的数据包。" ) );
      return false;
    }

    const bool hasValidDescriptorZoomRange =
      ( package.descriptor.minimumZoom == -1 && package.descriptor.maximumZoom == -1 ) ||
      ( package.descriptor.minimumZoom >= 0 &&
        package.descriptor.maximumZoom >= package.descriptor.minimumZoom );
    if ( !hasValidDescriptorZoomRange )
    {
      setError( error, QStringLiteral( "数据包实际缩放级别必须同时未知，或构成有效的有序区间。" ) );
      return false;
    }

    if ( package.descriptor.minimumZoom >= 0 &&
         ( package.descriptor.minimumZoom < minimumZoom || package.descriptor.maximumZoom > maximumZoom ) )
    {
      setError( error, QStringLiteral( "数据包实际缩放级别超出聚合数据集范围。" ) );
      return false;
    }

    int summarizedMinimum = std::numeric_limits<int>::max();
    int summarizedMaximum = std::numeric_limits<int>::min();
    for ( const TileRangeRecord &range : package.ranges )
    {
      QString rangeError;
      if ( !range.isValid( &rangeError ) || range.zoom < minimumZoom || range.zoom > maximumZoom ||
           !rangeFitsMatrix( range, matrix, &rangeError ) ||
           ( !compatibilityMode && !rangeMatchesPackage( range, package.address, matrix, &rangeError ) ) )
      {
        setError( error, QStringLiteral( "数据包范围无效：%1" ).arg( rangeError ) );
        return false;
      }
      if ( range.presentCount > 0 )
      {
        summarizedMinimum = std::min( summarizedMinimum, range.zoom );
        summarizedMaximum = std::max( summarizedMaximum, range.zoom );
      }
    }
    if ( !package.ranges.isEmpty() )
    {
      const bool hasPresentRange = summarizedMinimum != std::numeric_limits<int>::max();
      if ( ( hasPresentRange &&
             ( package.descriptor.minimumZoom != summarizedMinimum ||
               package.descriptor.maximumZoom != summarizedMaximum ) ) ||
           ( !hasPresentRange &&
             ( package.descriptor.minimumZoom != -1 || package.descriptor.maximumZoom != -1 ) ) )
      {
        setError( error, QStringLiteral( "数据包实际缩放摘要与范围记录不一致。" ) );
        return false;
      }
    }
  }
  if ( compatibilityMode )
  {
    if ( packages.size() != 1 )
    {
      setError( error, QStringLiteral( "兼容模式只能包含一个数据包。" ) );
      return false;
    }
  }
  else
  {
    QString ruleError;
    if ( !partitionRule.isValid( &ruleError ) )
    {
      setError( error, QStringLiteral( "数据集分包规则无效：%1" ).arg( ruleError ) );
      return false;
    }
    const int expectedMinimumZoom = std::max( partitionRule.bands.constFirst().minZoom, matrix.minimumZoom );
    const int expectedMaximumZoom = std::min( partitionRule.bands.constLast().maxZoom, matrix.maximumZoom );
    if ( expectedMinimumZoom > expectedMaximumZoom )
    {
      setError( error, QStringLiteral( "分包规则与瓦片矩阵声明的缩放范围不相交。" ) );
      return false;
    }
    if ( minimumZoom != expectedMinimumZoom || maximumZoom != expectedMaximumZoom )
    {
      setError( error, QStringLiteral( "聚合数据集缩放范围必须与分包规则和瓦片矩阵的交集一致。" ) );
      return false;
    }
    QSet<QString> keys;
    for ( qsizetype index = 0; index < packages.size(); ++index )
    {
      const TileDatasetPackage &package = packages.at( index );
      QString addressError;
      if ( !package.address.isValid( &addressError ) )
      {
        setError( error, QStringLiteral( "数据包地址无效：%1" ).arg( addressError ) );
        return false;
      }
      if ( !partitionRule.bands.contains( package.address.band() ) )
      {
        setError( error, QStringLiteral( "数据包地址不属于分包规则快照。" ) );
        return false;
      }
      if ( package.address.maxZoom < minimumZoom || package.address.minZoom > maximumZoom )
      {
        setError( error, QStringLiteral( "数据包文件名缩放范围与聚合数据集范围不相交。" ) );
        return false;
      }
      bool baseWidthOk = false;
      bool baseHeightOk = false;
      const quint64 baseWidth = matrix.matrixWidth( package.address.baseZoom, &baseWidthOk );
      const quint64 baseHeight = matrix.matrixHeight( package.address.baseZoom, &baseHeightOk );
      if ( !baseWidthOk || !baseHeightOk || package.address.packageX >= baseWidth || package.address.packageY >= baseHeight )
      {
        setError( error, QStringLiteral( "数据包基础瓦片坐标超出瓦片矩阵。" ) );
        return false;
      }
      if ( keys.contains( package.address.key() ) )
      {
        setError( error, QStringLiteral( "数据集包含重复的数据包地址。" ) );
        return false;
      }
      keys.insert( package.address.key() );
      if ( packageIndexByAddress.value( package.address.key(), -1 ) != index )
      {
        setError( error, QStringLiteral( "数据集的分包地址索引缺失或不一致。" ) );
        return false;
      }
    }
    if ( packageIndexByAddress.size() != packages.size() )
    {
      setError( error, QStringLiteral( "数据集的分包地址索引包含无效条目。" ) );
      return false;
    }
  }
  if ( error )
    error->clear();
  return true;
}

bool QgsMtpl::TileResolver::parsePackageFileName( const QString &fileName,
                                                  TilePackageAddress &address,
                                                  QString *error )
{
  address = TilePackageAddress();
  static const QRegularExpression pattern(
    QStringLiteral( "^([0-9]+)-([0-9]+)-([0-9]+)-([0-9]+)-([0-9]+)\\.ptp$" ),
    QRegularExpression::CaseInsensitiveOption );
  const QRegularExpressionMatch match = pattern.match( QFileInfo( fileName ).fileName() );
  if ( !match.hasMatch() )
  {
    setError( error, QStringLiteral( "文件名必须使用 minZ-maxZ-baseZ-X-Y.ptp 格式。" ) );
    return false;
  }

  quint64 values[5] = {};
  for ( int index = 0; index < 5; ++index )
  {
    bool ok = false;
    values[index] = match.captured( index + 1 ).toULongLong( &ok );
    if ( !ok || ( index < 3 && values[index] > static_cast<quint64>( sMaximumZoom ) ) ||
         ( index >= 3 && values[index] > std::numeric_limits<quint32>::max() ) )
    {
      setError( error, QStringLiteral( "文件名中的数值超出支持范围。" ) );
      return false;
    }
  }
  address.minZoom = static_cast<int>( values[0] );
  address.maxZoom = static_cast<int>( values[1] );
  address.baseZoom = static_cast<int>( values[2] );
  address.packageX = static_cast<quint32>( values[3] );
  address.packageY = static_cast<quint32>( values[4] );
  return address.isValid( error );
}

bool QgsMtpl::TileResolver::storageCoordinate( const TileMatrixDefinition &matrix,
                                               int zoom,
                                               quint64 requestX,
                                               quint64 requestY,
                                               quint32 &storageX,
                                               quint32 &storageY,
                                               QString *error )
{
  storageX = 0;
  storageY = 0;
  if ( zoom < matrix.minimumZoom || zoom > matrix.maximumZoom || zoom < sMinimumZoom || zoom > sMaximumZoom )
  {
    setError( error, QStringLiteral( "请求的缩放级别超出瓦片矩阵范围。" ) );
    return false;
  }
  bool widthOk = false;
  bool heightOk = false;
  const quint64 width = matrix.matrixWidth( zoom, &widthOk );
  const quint64 height = matrix.matrixHeight( zoom, &heightOk );
  if ( !widthOk || !heightOk || requestX >= width || requestY >= height )
  {
    setError( error, QStringLiteral( "请求的瓦片坐标超出瓦片矩阵范围。" ) );
    return false;
  }

  const quint64 transformedY = matrix.scheme == TileScheme::Tms ? height - 1ULL - requestY : requestY;
  if ( requestX > std::numeric_limits<quint32>::max() || transformedY > std::numeric_limits<quint32>::max() )
  {
    setError( error, QStringLiteral( "请求的瓦片坐标超出 MTPL 的 32 位坐标范围。" ) );
    return false;
  }
  storageX = static_cast<quint32>( requestX );
  storageY = static_cast<quint32>( transformedY );
  if ( error )
    error->clear();
  return true;
}

bool QgsMtpl::TileResolver::packageAddressForTile( const PartitionRule &rule,
                                                   const TileMatrixDefinition &matrix,
                                                   int zoom,
                                                   quint64 requestX,
                                                   quint64 requestY,
                                                   TilePackageAddress &address,
                                                   quint32 *storageX,
                                                   quint32 *storageY,
                                                   QString *error )
{
  address = TilePackageAddress();
  QString ruleError;
  if ( !rule.isValid( &ruleError ) )
  {
    setError( error, QStringLiteral( "分包规则无效：%1" ).arg( ruleError ) );
    return false;
  }
  const PartitionBand *band = rule.bandForZoom( zoom );
  if ( !band )
  {
    setError( error, QStringLiteral( "分包规则未覆盖缩放级别 %1。" ).arg( zoom ) );
    return false;
  }

  quint32 storedX = 0;
  quint32 storedY = 0;
  if ( !storageCoordinate( matrix, zoom, requestX, requestY, storedX, storedY, error ) )
    return false;
  const int shift = zoom - band->baseZoom;
  address.minZoom = band->minZoom;
  address.maxZoom = band->maxZoom;
  address.baseZoom = band->baseZoom;
  address.packageX = storedX >> shift;
  address.packageY = storedY >> shift;
  if ( storageX )
    *storageX = storedX;
  if ( storageY )
    *storageY = storedY;
  if ( error )
    error->clear();
  return true;
}

bool QgsMtpl::TileResolver::resolve( const TileDatasetDescriptor &dataset,
                                     int zoom,
                                     quint64 requestX,
                                     quint64 requestY,
                                     ResolvedTile &resolved,
                                     QString *error )
{
  resolved = ResolvedTile();
  if ( zoom < dataset.minimumZoom || zoom > dataset.maximumZoom )
  {
    if ( error )
      error->clear();
    return false;
  }

  quint32 storedX = 0;
  quint32 storedY = 0;
  if ( dataset.compatibilityMode )
  {
    if ( dataset.packages.size() != 1 ||
         !storageCoordinate( dataset.matrix, zoom, requestX, requestY, storedX, storedY, error ) )
      return false;
    const TileDatasetPackage &package = dataset.packages.constFirst();
    if ( !package.ranges.isEmpty() )
    {
      bool insideRange = false;
      for ( const TileRangeRecord &range : package.ranges )
      {
        if ( range.presentCount > 0 && range.zoom == zoom &&
             storedX >= range.presentXMin && storedX <= range.presentXMax &&
             storedY >= range.presentYMin && storedY <= range.presentYMax )
        {
          insideRange = true;
          break;
        }
      }
      if ( !insideRange )
      {
        if ( error )
          error->clear();
        return false;
      }
    }
    resolved.packageIndex = 0;
    resolved.storageX = storedX;
    resolved.storageY = storedY;
    if ( error )
      error->clear();
    return true;
  }

  TilePackageAddress address;
  if ( !packageAddressForTile( dataset.partitionRule, dataset.matrix, zoom, requestX, requestY,
                               address, &storedX, &storedY, error ) )
    return false;
  const int packageIndex = dataset.packageIndexByAddress.value( address.key(), -1 );
  if ( packageIndex >= 0 && packageIndex < dataset.packages.size() &&
       dataset.packages.at( packageIndex ).address == address )
  {
    resolved.packageIndex = packageIndex;
    resolved.packageAddress = address;
    resolved.storageX = storedX;
    resolved.storageY = storedY;
    if ( error )
      error->clear();
    return true;
  }

  // A missing package is an ordinary transparent-tile result, not a corrupt dataset.
  if ( error )
    error->clear();
  return false;
}

bool QgsMtpl::TileDatasetBuildResult::ok() const
{
  for ( const TileDatasetIssue &issue : issues )
  {
    if ( issue.severity == TileDatasetIssueSeverity::Error )
      return false;
  }
  return !dataset.packages.isEmpty();
}

QString QgsMtpl::TileDatasetBuildResult::errorString() const
{
  QStringList messages;
  for ( const TileDatasetIssue &issue : issues )
  {
    if ( issue.severity == TileDatasetIssueSeverity::Error )
      messages.append( issue.message );
  }
  return messages.join( QLatin1Char( '\n' ) );
}

QStringList QgsMtpl::TileDatasetBuildResult::warningMessages() const
{
  QStringList messages;
  for ( const TileDatasetIssue &issue : issues )
  {
    if ( issue.severity == TileDatasetIssueSeverity::Warning )
      messages.append( issue.message );
  }
  return messages;
}

QgsMtpl::TileDatasetBuildResult QgsMtpl::buildTileDataset( const QString &sourcePath,
                                                           bool isDirectory,
                                                           const QList<PackageDescriptor> &packages,
                                                           const TileDatasetBuildOptions &options )
{
  TileDatasetBuildResult result;
  if ( sourcePath.trimmed().isEmpty() )
  {
    addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "empty-source" ),
              QStringLiteral( "数据集来源路径不能为空。" ) );
    return result;
  }
  if ( packages.isEmpty() )
  {
    addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "empty-selection" ),
              QStringLiteral( "没有可用于构建瓦片数据集的数据包。" ) );
    return result;
  }
  if ( !isDirectory && packages.size() != 1 )
  {
    addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "single-file-count" ),
              QStringLiteral( "单文件数据集必须且只能包含一个探测结果。" ) );
    return result;
  }

  TileDatasetDescriptor dataset;
  dataset.sourcePath = QFileInfo( sourcePath ).absoluteFilePath();
  dataset.directorySource = isDirectory;

  struct CandidatePackage
  {
    TileDatasetPackage package;
    TileMatrixDefinition matrix;
    QString mapId;
    bool unconfirmedEmptyImage = false;
  };

  QList<PackageDescriptor> orderedPackages = packages;
  std::sort( orderedPackages.begin(), orderedPackages.end(),
    []( const PackageDescriptor &left, const PackageDescriptor &right )
    {
      const int folded = left.path.toCaseFolded().compare( right.path.toCaseFolded() );
      if ( folded != 0 )
        return folded < 0;
      return left.path < right.path;
    } );

  QList<CandidatePackage> candidates;
  bool compatibilityMode = !isDirectory && options.singleFileCompatibility;
  for ( const PackageDescriptor &package : std::as_const( orderedPackages ) )
  {
    if ( package.format != PackageFormat::Ptp )
    {
      addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "non-ptp-package" ),
                QStringLiteral( "“%1”不是 PTP 数据包。此影像数据集不接受 DTP、VTP 或 SFP。" )
                  .arg( package.displayName ),
                package.path );
      continue;
    }
    if ( !package.isReady() && !package.isCredentialedEmptyPtp() )
    {
      addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "package-not-ready" ),
                package.readinessMessage.isEmpty()
                  ? QStringLiteral( "数据包“%1”尚未解锁或无法读取。" ).arg( package.displayName )
                  : QStringLiteral( "数据包“%1”不可读取：%2" )
                      .arg( package.displayName, package.readinessMessage ),
                package.path );
      continue;
    }
    bool presentCountOk = false;
    const quint64 presentTileCount = package.metadata.value( QStringLiteral( "presentTileCount" ) )
                                       .toULongLong( &presentCountOk );
    const bool unconfirmedEmptyImage = package.payload == PayloadType::Unknown && presentCountOk &&
                                       package.metadata.contains( QStringLiteral( "presentTileCount" ) ) &&
                                       presentTileCount == 0;
    if ( package.payload != PayloadType::RasterImage && !unconfirmedEmptyImage )
    {
      const QString detail = package.payload == PayloadType::Elevation
        ? QStringLiteral( "高程载荷应使用 DTP" )
        : package.payload == PayloadType::VectorTile
          ? QStringLiteral( "PBF/MVT 矢量载荷应使用 VTP" )
          : QStringLiteral( "尚未确认 PNG、JPEG 或 WebP 影像载荷" );
      addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "invalid-ptp-payload" ),
                QStringLiteral( "PTP 数据包“%1”无效：%2。" ).arg( package.displayName, detail ),
                package.path );
      continue;
    }

    TilePackageAddress address;
    QString addressError;
    if ( !compatibilityMode &&
         !TileResolver::parsePackageFileName( QFileInfo( package.path ).fileName(), address, &addressError ) )
    {
      if ( isDirectory )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "invalid-package-name" ),
                  QStringLiteral( "目录中的 PTP 数据包“%1”命名无效：%2" )
                    .arg( QFileInfo( package.path ).fileName(), addressError ), package.path );
        continue;
      }
      compatibilityMode = true;
    }

    TileMatrixDefinition packageMatrix;
    QString matrixError;
    if ( !TileMatrixDefinition::fromPackageDescriptor( package, packageMatrix, &matrixError ) )
    {
      addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "invalid-tile-matrix" ),
                QStringLiteral( "数据包“%1”的瓦片矩阵无效：%2" ).arg( package.displayName, matrixError ),
                package.path );
      continue;
    }

    TileDatasetPackage datasetPackage;
    datasetPackage.descriptor = package;
    datasetPackage.address = address;
    datasetPackage.fileLastModifiedMs = package.fileLastModifiedMs;
    datasetPackage.sidecarPath = package.sidecarPath;
    datasetPackage.sourceContract.available = true;
    datasetPackage.sourceContract.zoomRangeExplicit = packageMatrix.zoomRangeExplicit;
    datasetPackage.sourceContract.minimumZoom = packageMatrix.minimumZoom;
    datasetPackage.sourceContract.maximumZoom = packageMatrix.maximumZoom;
    datasetPackage.sourceContract.hasBounds = packageMatrix.hasBounds;
    datasetPackage.sourceContract.boundsXMinimum = packageMatrix.boundsXMinimum;
    datasetPackage.sourceContract.boundsYMinimum = packageMatrix.boundsYMinimum;
    datasetPackage.sourceContract.boundsXMaximum = packageMatrix.boundsXMaximum;
    datasetPackage.sourceContract.boundsYMaximum = packageMatrix.boundsYMaximum;
    datasetPackage.sourceContract.boundsCrsAuthId = packageMatrix.boundsCrsAuthId;
    datasetPackage.sourceContract.legacyFallbackApplied = packageMatrix.legacyFallbackApplied;
    datasetPackage.sourceContract.mapIdDeclared = containsKeyCaseInsensitive(
      package.metadata, { QStringLiteral( "map_id" ), QStringLiteral( "mapId" ) } );
    datasetPackage.sourceContract.mapId = metadataString(
      package.metadata, { QStringLiteral( "map_id" ), QStringLiteral( "mapId" ) } );
    datasetPackage.sourceContract.payloadDeclared = containsKeyCaseInsensitive(
      package.metadata,
      { QStringLiteral( "tile_file_ext" ), QStringLiteral( "tileFormat" ),
        QStringLiteral( "payload" ), QStringLiteral( "format" ), QStringLiteral( "type" ) } );
    datasetPackage.sourceContract.payload = metadataPayload( package.metadata );
    if ( isDirectory )
    {
      if ( !isPathInside( dataset.sourcePath, package.path, datasetPackage.relativePath ) )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "package-outside-source" ),
                  QStringLiteral( "数据包路径不在所选目录内。" ), package.path );
        continue;
      }
    }
    else
    {
      datasetPackage.relativePath = QFileInfo( package.path ).fileName();
    }

    QString rangesError;
    if ( !parseRangeRecords( package, datasetPackage.ranges, &rangesError ) )
    {
      addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "invalid-range-summary" ),
                QStringLiteral( "数据包“%1”的范围摘要无效：%2" ).arg( package.displayName, rangesError ),
                package.path );
      continue;
    }

    bool rangesFit = true;
    int summarizedMinimum = std::numeric_limits<int>::max();
    int summarizedMaximum = std::numeric_limits<int>::min();
    for ( const TileRangeRecord &range : datasetPackage.ranges )
    {
      QString rangeError;
      if ( !rangeFitsMatrix( range, packageMatrix, &rangeError ) )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "range-outside-matrix" ),
                  QStringLiteral( "数据包“%1”的范围无效：%2" ).arg( package.displayName, rangeError ),
                  package.path );
        rangesFit = false;
        break;
      }
      if ( range.presentCount > 0 )
      {
        summarizedMinimum = std::min( summarizedMinimum, range.zoom );
        summarizedMaximum = std::max( summarizedMaximum, range.zoom );
      }
    }
    if ( !rangesFit )
      continue;
    const bool hasPresentRange = summarizedMinimum != std::numeric_limits<int>::max();
    if ( !datasetPackage.ranges.isEmpty() &&
         ( ( hasPresentRange &&
             ( package.minimumZoom != summarizedMinimum || package.maximumZoom != summarizedMaximum ) ) ||
           ( !hasPresentRange && ( package.minimumZoom != -1 || package.maximumZoom != -1 ) ) ) )
    {
      addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "range-zoom-summary-conflict" ),
                QStringLiteral( "数据包“%1”的实际缩放范围与范围摘要不一致。" ).arg( package.displayName ),
                package.path );
      continue;
    }

    if ( !compatibilityMode )
    {
      bool baseWidthOk = false;
      bool baseHeightOk = false;
      const quint64 baseWidth = packageMatrix.matrixWidth( address.baseZoom, &baseWidthOk );
      const quint64 baseHeight = packageMatrix.matrixHeight( address.baseZoom, &baseHeightOk );
      if ( !baseWidthOk || !baseHeightOk || address.packageX >= baseWidth || address.packageY >= baseHeight )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "package-address-out-of-range" ),
                  QStringLiteral( "数据包“%1”的基础瓦片坐标超出瓦片矩阵。" ).arg( package.displayName ), package.path );
        continue;
      }
      if ( packageMatrix.zoomRangeExplicit &&
           ( address.maxZoom < packageMatrix.minimumZoom || address.minZoom > packageMatrix.maximumZoom ) )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "package-zoom-out-of-range" ),
                  QStringLiteral( "数据包“%1”的文件名缩放范围与矩阵声明范围不相交。" ).arg( package.displayName ),
                  package.path );
        continue;
      }
      if ( package.minimumZoom >= 0 &&
           ( package.minimumZoom < address.minZoom || package.maximumZoom > address.maxZoom ) )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "observed-zoom-out-of-band" ),
                  QStringLiteral( "数据包“%1”的实际缩放级别超出文件名声明范围。" ).arg( package.displayName ),
                  package.path );
        continue;
      }
      bool rangesMatch = true;
      for ( const TileRangeRecord &range : datasetPackage.ranges )
      {
        QString rangeError;
        if ( !rangeMatchesPackage( range, address, packageMatrix, &rangeError ) )
        {
          addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "range-address-mismatch" ),
                    QStringLiteral( "数据包“%1”的范围与文件名不一致：%2" ).arg( package.displayName, rangeError ),
                    package.path );
          rangesMatch = false;
          break;
        }
      }
      if ( !rangesMatch )
        continue;
    }

    CandidatePackage candidate;
    candidate.package = std::move( datasetPackage );
    candidate.matrix = packageMatrix;
    candidate.mapId = metadataString( package.metadata,
      { QStringLiteral( "map_id" ), QStringLiteral( "mapId" ) } );
    candidate.unconfirmedEmptyImage = unconfirmedEmptyImage;
    candidates.append( std::move( candidate ) );
  }

  if ( candidates.isEmpty() )
  {
    addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "no-ready-ptp" ),
              QStringLiteral( "所选来源中没有通过校验的 PTP 影像包。" ) );
    return result;
  }
  if ( hasErrors( result ) )
    return result;

  dataset.compatibilityMode = compatibilityMode;
  QList<TilePackageAddress> parsedAddresses;
  parsedAddresses.reserve( candidates.size() );
  for ( const CandidatePackage &candidate : std::as_const( candidates ) )
    parsedAddresses.append( candidate.package.address );
  if ( !compatibilityMode && !selectRule( parsedAddresses, options, result, dataset.partitionRule ) )
    return result;
  if ( hasErrors( result ) )
    return result;

  TileMatrixDefinition matrix = candidates.constFirst().matrix;
  QString mapId;
  for ( qsizetype index = 0; index < candidates.size(); ++index )
  {
    const CandidatePackage &candidate = candidates.at( index );
    if ( index > 0 )
    {
      QString difference;
      if ( !sameMatrixGeometry( matrix, candidate.matrix, &difference ) )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "matrix-conflict" ),
                  QStringLiteral( "数据包“%1”的%2。" )
                    .arg( candidate.package.descriptor.displayName, difference ),
                  candidate.package.descriptor.path );
        continue;
      }
      if ( !matrix.zoomRangeExplicit && candidate.matrix.zoomRangeExplicit )
      {
        matrix.minimumZoom = candidate.matrix.minimumZoom;
        matrix.maximumZoom = candidate.matrix.maximumZoom;
        matrix.zoomRangeExplicit = true;
      }
      if ( !matrix.hasBounds && candidate.matrix.hasBounds )
      {
        matrix.hasBounds = true;
        matrix.boundsXMinimum = candidate.matrix.boundsXMinimum;
        matrix.boundsYMinimum = candidate.matrix.boundsYMinimum;
        matrix.boundsXMaximum = candidate.matrix.boundsXMaximum;
        matrix.boundsYMaximum = candidate.matrix.boundsYMaximum;
        matrix.boundsCrsAuthId = candidate.matrix.boundsCrsAuthId;
      }
      matrix.legacyFallbackApplied = matrix.legacyFallbackApplied || candidate.matrix.legacyFallbackApplied;
    }

    if ( mapId.isEmpty() )
      mapId = candidate.mapId;
    else if ( !candidate.mapId.isEmpty() && candidate.mapId != mapId )
    {
      addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "map-id-conflict" ),
                QStringLiteral( "数据包“%1”的 map_id 与数据集中的其他包不一致。" )
                  .arg( candidate.package.descriptor.displayName ),
                candidate.package.descriptor.path );
    }
  }
  if ( hasErrors( result ) )
    return result;

  const bool hasUnconfirmedEmptyPackage = std::any_of(
    candidates.cbegin(), candidates.cend(), []( const CandidatePackage &candidate )
    {
      return candidate.unconfirmedEmptyImage;
    } );
  const bool hasVerifiedImagePayload = std::any_of(
    candidates.cbegin(), candidates.cend(), []( const CandidatePackage &candidate )
    {
      bool presentCountOk = false;
      const quint64 presentTileCount = candidate.package.descriptor.metadata
                                         .value( QStringLiteral( "presentTileCount" ) )
                                         .toULongLong( &presentCountOk );
      return candidate.package.descriptor.payload == PayloadType::RasterImage &&
             presentCountOk && presentTileCount > 0;
    } );
  if ( hasUnconfirmedEmptyPackage && !hasVerifiedImagePayload )
  {
    const CandidatePackage &first = candidates.constFirst();
    addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "unconfirmed-empty-image" ),
              QStringLiteral( "PTP 数据集仅包含未声明影像格式的空包，无法确认 PNG、JPEG 或 WebP 载荷。" ),
              first.package.descriptor.path );
    return result;
  }
  for ( CandidatePackage &candidate : candidates )
  {
    if ( !candidate.unconfirmedEmptyImage )
      continue;
    candidate.package.descriptor.payload = PayloadType::RasterImage;
    addIssue( result, TileDatasetIssueSeverity::Warning, QStringLiteral( "empty-image-inherited" ),
              QStringLiteral( "空包“%1”未声明影像格式，已从同一兼容数据集的已验证影像包继承。" )
                .arg( candidate.package.descriptor.displayName ),
              candidate.package.descriptor.path );
  }

  QSet<QString> packageKeys;
  int availableMinimum = std::numeric_limits<int>::max();
  int availableMaximum = std::numeric_limits<int>::min();
  for ( const CandidatePackage &candidate : std::as_const( candidates ) )
  {
    const TileDatasetPackage &package = candidate.package;
    if ( !compatibilityMode )
    {
      bool baseWidthOk = false;
      bool baseHeightOk = false;
      const quint64 baseWidth = matrix.matrixWidth( package.address.baseZoom, &baseWidthOk );
      const quint64 baseHeight = matrix.matrixHeight( package.address.baseZoom, &baseHeightOk );
      if ( !baseWidthOk || !baseHeightOk ||
           package.address.packageX >= baseWidth || package.address.packageY >= baseHeight )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "package-address-out-of-range" ),
                  QStringLiteral( "数据包“%1”的基础瓦片坐标超出公共瓦片矩阵。" )
                    .arg( package.descriptor.displayName ), package.descriptor.path );
        continue;
      }
      if ( matrix.zoomRangeExplicit &&
           ( package.address.maxZoom < matrix.minimumZoom || package.address.minZoom > matrix.maximumZoom ) )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "package-zoom-out-of-range" ),
                  QStringLiteral( "数据包“%1”的文件名缩放范围与公共矩阵声明范围不相交。" )
                    .arg( package.descriptor.displayName ), package.descriptor.path );
        continue;
      }
      if ( package.descriptor.minimumZoom >= 0 &&
           ( package.descriptor.minimumZoom < matrix.minimumZoom ||
             package.descriptor.maximumZoom > matrix.maximumZoom ) )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "observed-zoom-outside-matrix" ),
                  QStringLiteral( "数据包“%1”的实际缩放级别超出公共矩阵声明范围。" )
                    .arg( package.descriptor.displayName ), package.descriptor.path );
        continue;
      }
      bool rangesMatch = true;
      for ( const TileRangeRecord &range : package.ranges )
      {
        QString rangeError;
        if ( !rangeFitsMatrix( range, matrix, &rangeError ) ||
             !rangeMatchesPackage( range, package.address, matrix, &rangeError ) )
        {
          addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "range-address-mismatch" ),
                    QStringLiteral( "数据包“%1”的范围与公共矩阵不一致：%2" )
                      .arg( package.descriptor.displayName, rangeError ), package.descriptor.path );
          rangesMatch = false;
          break;
        }
      }
      if ( !rangesMatch )
        continue;
      if ( packageKeys.contains( package.address.key() ) )
      {
        addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "duplicate-package-address" ),
                  QStringLiteral( "多个文件声明了相同的数据包地址“%1”。" ).arg( package.address.key() ),
                  package.descriptor.path );
        continue;
      }
      packageKeys.insert( package.address.key() );
      availableMinimum = std::min( availableMinimum, package.address.minZoom );
      availableMaximum = std::max( availableMaximum, package.address.maxZoom );
    }
    else if ( package.descriptor.minimumZoom >= 0 )
    {
      availableMinimum = std::min( availableMinimum, package.descriptor.minimumZoom );
      availableMaximum = std::max( availableMaximum, package.descriptor.maximumZoom );
    }
  }
  if ( hasErrors( result ) )
    return result;

  dataset.matrix = matrix;
  dataset.payload = PayloadType::RasterImage;
  dataset.mapId = mapId;
  for ( const CandidatePackage &candidate : std::as_const( candidates ) )
    dataset.packages.append( candidate.package );

  if ( !compatibilityMode )
  {
    for ( qsizetype index = 0; index < dataset.packages.size(); ++index )
      dataset.packageIndexByAddress.insert( dataset.packages.at( index ).address.key(), static_cast<int>( index ) );
  }

  if ( !compatibilityMode )
  {
    dataset.minimumZoom = std::max( dataset.partitionRule.bands.constFirst().minZoom, matrix.minimumZoom );
    dataset.maximumZoom = std::min( dataset.partitionRule.bands.constLast().maxZoom, matrix.maximumZoom );
  }
  else if ( availableMinimum == std::numeric_limits<int>::max() )
  {
    dataset.minimumZoom = matrix.minimumZoom;
    dataset.maximumZoom = matrix.maximumZoom;
    addIssue( result, TileDatasetIssueSeverity::Warning, QStringLiteral( "empty-range-summary" ),
              QStringLiteral( "数据包未报告实际瓦片范围，将由读取结果判断空瓦片。" ) );
  }
  else
  {
    dataset.minimumZoom = availableMinimum;
    dataset.maximumZoom = availableMaximum;
  }

  if ( matrix.hasBounds && sameCrs( matrix.boundsCrsAuthId, matrix.crsAuthId ) )
  {
    dataset.hasExtent = true;
    dataset.extentXMinimum = matrix.boundsXMinimum;
    dataset.extentYMinimum = matrix.boundsYMinimum;
    dataset.extentXMaximum = matrix.boundsXMaximum;
    dataset.extentYMaximum = matrix.boundsYMaximum;
  }
  else if ( standardWebMercatorGeometry( matrix ) )
  {
    for ( const TileDatasetPackage &package : std::as_const( dataset.packages ) )
    {
      if ( !package.descriptor.hasExtent )
        continue;
      if ( !dataset.hasExtent )
      {
        dataset.hasExtent = true;
        dataset.extentXMinimum = package.descriptor.extentXMinimum;
        dataset.extentYMinimum = package.descriptor.extentYMinimum;
        dataset.extentXMaximum = package.descriptor.extentXMaximum;
        dataset.extentYMaximum = package.descriptor.extentYMaximum;
      }
      else
      {
        dataset.extentXMinimum = std::min( dataset.extentXMinimum, package.descriptor.extentXMinimum );
        dataset.extentYMinimum = std::min( dataset.extentYMinimum, package.descriptor.extentYMinimum );
        dataset.extentXMaximum = std::max( dataset.extentXMaximum, package.descriptor.extentXMaximum );
        dataset.extentYMaximum = std::max( dataset.extentYMaximum, package.descriptor.extentYMaximum );
      }
    }
  }

  if ( isDirectory && !compatibilityMode )
  {
    for ( const PartitionBand &band : dataset.partitionRule.bands )
    {
      if ( band.maxZoom < dataset.minimumZoom || band.minZoom > dataset.maximumZoom )
        continue;
      bool found = false;
      for ( const TileDatasetPackage &package : std::as_const( dataset.packages ) )
      {
        if ( package.address.band() == band )
        {
          found = true;
          break;
        }
      }
      if ( !found )
      {
        const int missingMinimumZoom = std::max( band.minZoom, dataset.minimumZoom );
        const int missingMaximumZoom = std::min( band.maxZoom, dataset.maximumZoom );
        addIssue( result, TileDatasetIssueSeverity::Warning, QStringLiteral( "missing-band" ),
                  QStringLiteral( "未发现 %1-%2 级分包，该级别范围将显示为透明。" )
                    .arg( missingMinimumZoom ).arg( missingMaximumZoom ) );
      }
    }
  }

  QString datasetError;
  if ( !dataset.isValid( &datasetError ) )
  {
    addIssue( result, TileDatasetIssueSeverity::Error, QStringLiteral( "invalid-dataset" ), datasetError );
    return result;
  }
  result.dataset = std::move( dataset );
  return result;
}
