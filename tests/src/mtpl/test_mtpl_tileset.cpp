/***************************************************************************
  test_mtpl_tileset.cpp
  ---------------------
  Contract tests for MTPL partition rules and logical tile datasets.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmtplpartitionrule.h"
#include "qgsmtpltileset.h"
#include "qgssettings.h"
#include "qgstest.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

#include <algorithm>

namespace
{
  QVariantMap legacyMetadata( const QString &mapId = QStringLiteral( "example-map" ),
                              const QString &scheme = QStringLiteral( "xyz" ) )
  {
    return {
      { QStringLiteral( "tile_file_ext" ), QStringLiteral( "png" ) },
      { QStringLiteral( "min_zoom_level" ), 0 },
      { QStringLiteral( "max_zoom_level" ), 19 },
      { QStringLiteral( "map_id" ), mapId },
      { QStringLiteral( "scheme" ), scheme },
    };
  }

  QVariantMap tileMatrixContract( const QString &crs = QStringLiteral( "EPSG:4326" ),
                              const QString &scheme = QStringLiteral( "xyz" ) )
  {
    return {
      { QStringLiteral( "version" ), 1 },
      { QStringLiteral( "crs" ), crs },
      { QStringLiteral( "scheme" ), scheme },
      { QStringLiteral( "top_left" ), QVariantList { -180.0, 90.0 } },
      { QStringLiteral( "z0_tile_span" ), 180.0 },
      { QStringLiteral( "z0_matrix_width" ), 2 },
      { QStringLiteral( "z0_matrix_height" ), 1 },
      { QStringLiteral( "scale_to_zoom_method" ), QStringLiteral( "mapbox" ) },
      { QStringLiteral( "min_zoom" ), 0 },
      { QStringLiteral( "max_zoom" ), 2 },
    };
  }

  QgsMtpl::PackageDescriptor packageDescriptor( const QString &path,
                                                int minimumZoom,
                                                int maximumZoom,
                                                const QVariantMap &metadata = legacyMetadata() )
  {
    QgsMtpl::PackageDescriptor package;
    package.path = path;
    package.displayName = QFileInfo( path ).completeBaseName();
    package.format = QgsMtpl::PackageFormat::Ptp;
    package.payload = QgsMtpl::PayloadType::RasterImage;
    package.encryption = QgsMtpl::EncryptionState::Plain;
    package.readiness = QgsMtpl::ReadinessState::PlainReady;
    package.metadata = metadata;
    package.tileSize = 256;
    package.minimumZoom = minimumZoom;
    package.maximumZoom = maximumZoom;
    package.fileLastModifiedMs = 1234567;
    package.sidecarPath = path + QStringLiteral( ".keys.json" );
    return package;
  }

  QVariantMap rangeSummary( int zoom, quint32 xMin, quint32 xMax, quint32 yMin, quint32 yMax )
  {
    const quint64 count = ( static_cast<quint64>( xMax ) - xMin + 1ULL ) *
                          ( static_cast<quint64>( yMax ) - yMin + 1ULL );
    return {
      { QStringLiteral( "zoom" ), zoom },
      { QStringLiteral( "xMin" ), static_cast<qulonglong>( xMin ) },
      { QStringLiteral( "xMax" ), static_cast<qulonglong>( xMax ) },
      { QStringLiteral( "yMin" ), static_cast<qulonglong>( yMin ) },
      { QStringLiteral( "yMax" ), static_cast<qulonglong>( yMax ) },
      { QStringLiteral( "presentXMin" ), static_cast<qulonglong>( xMin ) },
      { QStringLiteral( "presentXMax" ), static_cast<qulonglong>( xMax ) },
      { QStringLiteral( "presentYMin" ), static_cast<qulonglong>( yMin ) },
      { QStringLiteral( "presentYMax" ), static_cast<qulonglong>( yMax ) },
      { QStringLiteral( "presentCount" ), static_cast<qulonglong>( count ) },
    };
  }

  QVariantMap emptyRangeSummary( int zoom, quint32 xMin, quint32 xMax, quint32 yMin, quint32 yMax )
  {
    QVariantMap summary = rangeSummary( zoom, xMin, xMax, yMin, yMax );
    summary.insert( QStringLiteral( "presentCount" ), static_cast<qulonglong>( 0 ) );
    return summary;
  }

  bool hasIssue( const QgsMtpl::TileDatasetBuildResult &result,
                 const QString &code,
                 QgsMtpl::TileDatasetIssueSeverity severity )
  {
    return std::any_of( result.issues.cbegin(), result.issues.cend(),
      [&code, severity]( const QgsMtpl::TileDatasetIssue &issue )
      {
        return issue.code == code && issue.severity == severity;
      } );
  }
}

class TestMtplTileset : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void builtInRules();
    void ruleValidation();
    void jsonAndSettingsRoundTrip();
    void legacyMatrixFallback();
    void matrixContract();
    void matrixContractFailures();
    void packageDescriptorOverrides();
    void packageFileNames();
    void xyzAndTmsAddressing();
    void nonSquareRootMatrix();
    void matrixRangeIntersectsPartitionBand();
    void buildDirectoryDataset();
    void rejectNonRasterPtpContracts();
    void emptyPtpPayloadInheritance();
    void atomicBuildIsOrderIndependent();
    void rejectDatasetConflicts();
    void singleFileCompatibility();
};

void TestMtplTileset::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestMtplTileset::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestMtplTileset::builtInRules()
{
  const QList<QgsMtpl::PartitionRule> rules = QgsMtpl::PartitionRuleStore::builtInRules();
  QCOMPARE( rules.size(), 2 );
  QCOMPARE( rules.at( 0 ).id, QStringLiteral( "org.qgis.mtpl.partition-rule.1" ) );
  QCOMPARE( rules.at( 1 ).id, QStringLiteral( "org.qgis.mtpl.partition-rule.2" ) );
  QVERIFY( rules.at( 0 ).builtIn );
  QVERIFY( rules.at( 1 ).builtIn );
  QCOMPARE( rules.at( 0 ).bands,
            QList<QgsMtpl::PartitionBand>( { { 0, 7, 0 }, { 8, 11, 3 }, { 12, 15, 7 }, { 16, 19, 11 } } ) );
  QCOMPARE( rules.at( 1 ).bands,
            QList<QgsMtpl::PartitionBand>( { { 0, 9, 0 }, { 10, 14, 7 }, { 15, 19, 11 } } ) );
  QString error;
  QVERIFY2( rules.at( 0 ).isValid( &error ), qPrintable( error ) );
  QVERIFY2( rules.at( 1 ).isValid( &error ), qPrintable( error ) );
}

void TestMtplTileset::ruleValidation()
{
  QgsMtpl::PartitionRule rule;
  rule.id = QStringLiteral( "custom.rule" );
  rule.name = QStringLiteral( "自定义规则" );
  rule.bands = { { 0, 4, 0 }, { 5, 8, 2 } };
  QString error;
  QVERIFY2( rule.isValid( &error ), qPrintable( error ) );
  QCOMPARE( rule.bandForZoom( 6 )->baseZoom, 2 );
  QVERIFY( rule.bandForZoom( 9 ) == nullptr );

  QgsMtpl::PartitionRule gap = rule;
  gap.bands[1].minZoom = 6;
  QVERIFY( !gap.isValid( &error ) );
  QVERIFY( !error.isEmpty() );

  QgsMtpl::PartitionRule invalidBase = rule;
  invalidBase.bands[1].baseZoom = 6;
  QVERIFY( !invalidBase.isValid( &error ) );

  QgsMtpl::PartitionRule partial;
  partial.id = QStringLiteral( "custom.partial" );
  partial.name = QStringLiteral( "局部级别规则" );
  partial.bands = { { 8, 11, 3 }, { 12, 15, 7 } };
  QVERIFY2( partial.isValid( &error ), qPrintable( error ) );

  QgsMtpl::PartitionRule protectedId = rule;
  protectedId.id = QgsMtpl::PartitionRuleStore::ruleOneId();
  QVERIFY( !QgsMtpl::PartitionRuleStore::validateCustomRules( { protectedId }, &error ) );

  QgsMtpl::PartitionRule duplicateName = rule;
  duplicateName.id = QStringLiteral( "custom.duplicate-name" );
  duplicateName.name = rule.name.toUpper();
  QVERIFY( !QgsMtpl::PartitionRuleStore::validateCustomRules( { rule, duplicateName }, &error ) );

  QgsMtpl::PartitionRule builtInName = rule;
  builtInName.id = QStringLiteral( "custom.builtin-name" );
  builtInName.name = QgsMtpl::PartitionRuleStore::builtInRules().constFirst().name;
  QVERIFY( !QgsMtpl::PartitionRuleStore::validateCustomRules( { builtInName }, &error ) );
}

void TestMtplTileset::jsonAndSettingsRoundTrip()
{
  QgsMtpl::PartitionRule rule;
  rule.id = QStringLiteral( "custom.persisted" );
  rule.name = QStringLiteral( "持久化规则" );
  rule.bands = { { 0, 5, 0 }, { 6, 10, 3 } };
  QString error;
  const QByteArray json = QgsMtpl::PartitionRuleStore::toJson( { rule }, &error );
  QVERIFY2( !json.isEmpty(), qPrintable( error ) );
  QList<QgsMtpl::PartitionRule> decoded;
  QVERIFY2( QgsMtpl::PartitionRuleStore::fromJson( json, decoded, &error ), qPrintable( error ) );
  QCOMPARE( decoded, QList<QgsMtpl::PartitionRule>( { rule } ) );

  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsSettings settings( directory.filePath( QStringLiteral( "rules.ini" ) ), QSettings::IniFormat );
  QVERIFY2( QgsMtpl::PartitionRuleStore::saveCustomRules( settings, { rule }, &error ), qPrintable( error ) );
  QCOMPARE( QgsMtpl::PartitionRuleStore::loadCustomRules( settings, &error ),
            QList<QgsMtpl::PartitionRule>( { rule } ) );
  QCOMPARE( QgsMtpl::PartitionRuleStore::combineWithBuiltIns( decoded, &error ).size(), 3 );

  QVERIFY( !QgsMtpl::PartitionRuleStore::fromJson( QByteArrayLiteral( "{bad" ), decoded, &error ) );
  QVERIFY( decoded.isEmpty() );
}

void TestMtplTileset::legacyMatrixFallback()
{
  QgsMtpl::TileMatrixDefinition matrix;
  QString error;
  QVERIFY2( QgsMtpl::TileMatrixDefinition::fromMetadata( legacyMetadata(), 256, 0, 15, matrix, &error ),
            qPrintable( error ) );
  QVERIFY( matrix.legacyFallbackApplied );
  QVERIFY( !matrix.zoomRangeExplicit );
  QCOMPARE( matrix.minimumZoom, 0 );
  QCOMPARE( matrix.maximumZoom, 30 );
  QCOMPARE( matrix.crsAuthId, QStringLiteral( "EPSG:3857" ) );
  QCOMPARE( matrix.scheme, QgsMtpl::TileScheme::Xyz );
  QCOMPARE( matrix.tileSize, 256 );
  QCOMPARE( matrix.matrixWidth( 8 ), 256ULL );
  QCOMPARE( matrix.matrixHeight( 8 ), 256ULL );
  QVERIFY( qAbs( matrix.tileSpan( 1 ) - 20037508.342789244 ) < 1e-6 );

  QVariantMap nonMercator = legacyMetadata();
  nonMercator.insert( QStringLiteral( "crs" ), QStringLiteral( "EPSG:4326" ) );
  QVERIFY( !QgsMtpl::TileMatrixDefinition::fromMetadata( nonMercator, 256, 0, 2, matrix, &error ) );
  QVERIFY( error.contains( QStringLiteral( "mtpl_tile_matrix" ) ) );
}

void TestMtplTileset::matrixContract()
{
  QVariantMap contract = tileMatrixContract();
  contract.insert( QStringLiteral( "scaleToZoomMethod" ), QStringLiteral( "esri" ) );
  contract.insert( QStringLiteral( "bounds" ), QVariantList { 70.0, 10.0, 140.0, 60.0 } );
  contract.insert( QStringLiteral( "bounds_crs" ), QStringLiteral( "EPSG:4326" ) );
  QVariantMap metadata;
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), contract );

  QgsMtpl::TileMatrixDefinition matrix;
  QString error;
  QVERIFY2( QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ),
            qPrintable( error ) );
  QVERIFY( !matrix.legacyFallbackApplied );
  QCOMPARE( matrix.crsAuthId, QStringLiteral( "EPSG:4326" ) );
  QCOMPARE( matrix.z0MatrixWidth, 2ULL );
  QCOMPARE( matrix.z0MatrixHeight, 1ULL );
  QCOMPARE( matrix.matrixWidth( 2 ), 8ULL );
  QCOMPARE( matrix.matrixHeight( 2 ), 4ULL );
  QCOMPARE( matrix.tileSpan( 2 ), 45.0 );
  QCOMPARE( matrix.scaleToZoomMethod, QStringLiteral( "mapbox" ) );
  QVERIFY( matrix.hasBounds );
  QCOMPARE( matrix.boundsCrsAuthId, QStringLiteral( "EPSG:4326" ) );

  contract.insert( QStringLiteral( "scale_to_zoom_method" ), QStringLiteral( "esri" ) );
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), contract );
  QVERIFY2( QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ),
            qPrintable( error ) );
  QCOMPARE( matrix.scaleToZoomMethod, QStringLiteral( "esri" ) );

  contract.remove( QStringLiteral( "min_zoom" ) );
  contract.remove( QStringLiteral( "max_zoom" ) );
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), contract );
  metadata.insert( QStringLiteral( "minimumZoom" ), 0 );
  metadata.insert( QStringLiteral( "maximumZoom" ), 2 );
  QVERIFY2( QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ),
            qPrintable( error ) );
  QVERIFY( !matrix.zoomRangeExplicit );
  QCOMPARE( matrix.maximumZoom, 30 );
}

void TestMtplTileset::matrixContractFailures()
{
  QString error;
  QgsMtpl::TileMatrixDefinition matrix;
  QVariantMap metadata;
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), QVariant() );
  QVERIFY( !QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ) );

  QVariantMap contract = tileMatrixContract();
  contract.remove( QStringLiteral( "top_left" ) );
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), contract );
  QVERIFY( !QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ) );

  contract = tileMatrixContract();
  contract.insert( QStringLiteral( "bounds" ), QVariantList { 0.0, 0.0, 1.0, 1.0 } );
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), contract );
  QVERIFY( !QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ) );

  contract = tileMatrixContract();
  contract.insert( QStringLiteral( "tile_size" ), 512 );
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), contract );
  QVERIFY( !QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ) );

  contract = tileMatrixContract();
  contract.insert( QStringLiteral( "max_zoom" ), 30 );
  contract.insert( QStringLiteral( "z0_matrix_width" ), 5 );
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), contract );
  QVERIFY( !QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ) );
}

void TestMtplTileset::packageDescriptorOverrides()
{
  QVariantMap contract = tileMatrixContract();
  contract.insert( QStringLiteral( "scale_to_zoom_method" ), QStringLiteral( "esri" ) );
  contract.insert( QStringLiteral( "bounds" ), QVariantList { -170.0, -80.0, 170.0, 80.0 } );
  contract.insert( QStringLiteral( "bounds_crs" ), QStringLiteral( "EPSG:4326" ) );

  QVariantMap metadata;
  metadata.insert( QStringLiteral( "mtplTileMatrix" ), contract );
  metadata.insert( QStringLiteral( "crsAuthId" ), QStringLiteral( "EPSG:4326" ) );
  metadata.insert( QStringLiteral( "scheme" ), QStringLiteral( "xyz" ) );
  metadata.insert( QStringLiteral( "map_id" ), QStringLiteral( "override-test" ) );

  QgsMtpl::PackageDescriptor package = packageDescriptor(
    QStringLiteral( "0-2-0-0-0.ptp" ), 0, 2, metadata );
  package.displayOverridesApplied = true;
  package.crsAuthId = QStringLiteral( "EPSG:3413" );
  package.scheme = QStringLiteral( "tms" );

  QgsMtpl::TileMatrixDefinition matrix;
  QString error;
  QVERIFY2( QgsMtpl::TileMatrixDefinition::fromPackageDescriptor( package, matrix, &error ),
            qPrintable( error ) );
  QCOMPARE( matrix.crsAuthId, QStringLiteral( "EPSG:3413" ) );
  QCOMPARE( matrix.scheme, QgsMtpl::TileScheme::Tms );
  QCOMPARE( matrix.topLeftX, -180.0 );
  QCOMPARE( matrix.topLeftY, 90.0 );
  QCOMPARE( matrix.z0TileSpan, 180.0 );
  QCOMPARE( matrix.z0MatrixWidth, 2ULL );
  QCOMPARE( matrix.z0MatrixHeight, 1ULL );
  QCOMPARE( matrix.scaleToZoomMethod, QStringLiteral( "esri" ) );
  QVERIFY( matrix.hasBounds );
  QCOMPARE( matrix.boundsCrsAuthId, QStringLiteral( "EPSG:4326" ) );
  QCOMPARE( matrix.boundsXMinimum, -170.0 );
  QCOMPARE( matrix.boundsYMinimum, -80.0 );
  QCOMPARE( matrix.boundsXMaximum, 170.0 );
  QCOMPARE( matrix.boundsYMaximum, 80.0 );

  const QVariantMap originalContract = package.metadata.value( QStringLiteral( "mtplTileMatrix" ) ).toMap();
  QCOMPARE( originalContract.value( QStringLiteral( "crs" ) ).toString(), QStringLiteral( "EPSG:4326" ) );
  QCOMPARE( originalContract.value( QStringLiteral( "scheme" ) ).toString(), QStringLiteral( "xyz" ) );
}

void TestMtplTileset::packageFileNames()
{
  QgsMtpl::TilePackageAddress address;
  QString error;
  QVERIFY2( QgsMtpl::TileResolver::parsePackageFileName( QStringLiteral( "8-11-3-1-1.PTP" ), address, &error ),
            qPrintable( error ) );
  const QgsMtpl::PartitionBand expectedBand { 8, 11, 3 };
  QCOMPARE( address.band(), expectedBand );
  QCOMPARE( address.packageX, 1U );
  QCOMPARE( address.packageY, 1U );
  QCOMPARE( address.fileName(), QStringLiteral( "8-11-3-1-1.ptp" ) );

  QVERIFY( !QgsMtpl::TileResolver::parsePackageFileName( QStringLiteral( "8-11-12-1-1.ptp" ), address, &error ) );
  QVERIFY( !QgsMtpl::TileResolver::parsePackageFileName( QStringLiteral( "8-11-3-4294967296-1.ptp" ), address, &error ) );
  QVERIFY( !QgsMtpl::TileResolver::parsePackageFileName( QStringLiteral( "not-a-package.ptp" ), address, &error ) );
}

void TestMtplTileset::xyzAndTmsAddressing()
{
  const QgsMtpl::PartitionRule rule = QgsMtpl::PartitionRuleStore::builtInRules().constFirst();
  QgsMtpl::TileMatrixDefinition xyz;
  QString error;
  QVERIFY2( QgsMtpl::TileMatrixDefinition::fromMetadata( legacyMetadata(), 256, 0, 19, xyz, &error ),
            qPrintable( error ) );
  QgsMtpl::TilePackageAddress address;
  quint32 storedX = 0;
  quint32 storedY = 0;
  QVERIFY2( QgsMtpl::TileResolver::packageAddressForTile( rule, xyz, 8, 35, 36, address,
                                                          &storedX, &storedY, &error ), qPrintable( error ) );
  QCOMPARE( address.fileName(), QStringLiteral( "8-11-3-1-1.ptp" ) );
  QCOMPARE( storedX, 35U );
  QCOMPARE( storedY, 36U );

  QgsMtpl::TileMatrixDefinition tms;
  QVERIFY2( QgsMtpl::TileMatrixDefinition::fromMetadata( legacyMetadata( QStringLiteral( "example-map" ),
                                                                         QStringLiteral( "tms" ) ),
                                                         256, 0, 19, tms, &error ), qPrintable( error ) );
  QVERIFY2( QgsMtpl::TileResolver::packageAddressForTile( rule, tms, 8, 35, 219, address,
                                                          &storedX, &storedY, &error ), qPrintable( error ) );
  QCOMPARE( storedY, 36U );
  QCOMPARE( address.fileName(), QStringLiteral( "8-11-3-1-1.ptp" ) );
  QVERIFY( !QgsMtpl::TileResolver::packageAddressForTile( rule, xyz, 31, 0, 0, address,
                                                          nullptr, nullptr, &error ) );
}

void TestMtplTileset::nonSquareRootMatrix()
{
  QVariantMap metadata;
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), tileMatrixContract() );
  QgsMtpl::TileMatrixDefinition matrix;
  QString error;
  QVERIFY2( QgsMtpl::TileMatrixDefinition::fromMetadata( metadata, 256, 0, 2, matrix, &error ),
            qPrintable( error ) );

  QgsMtpl::PartitionRule rule;
  rule.id = QStringLiteral( "world-geodetic" );
  rule.name = QStringLiteral( "World Geodetic" );
  rule.bands = { { 0, 2, 0 } };
  QgsMtpl::TilePackageAddress address;
  QVERIFY2( QgsMtpl::TileResolver::packageAddressForTile( rule, matrix, 1, 3, 1, address,
                                                          nullptr, nullptr, &error ), qPrintable( error ) );
  QCOMPARE( address.packageX, 1U );
  QCOMPARE( address.packageY, 0U );
  QVERIFY( !QgsMtpl::TileResolver::packageAddressForTile( rule, matrix, 1, 4, 1, address,
                                                          nullptr, nullptr, &error ) );
}

void TestMtplTileset::matrixRangeIntersectsPartitionBand()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );

  QVariantMap metadata;
  metadata.insert( QStringLiteral( "tile_file_ext" ), QStringLiteral( "png" ) );
  metadata.insert( QStringLiteral( "presentTileCount" ), static_cast<qulonglong>( 1 ) );
  metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), tileMatrixContract() );

  QVariantMap westMetadata = metadata;
  westMetadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                       QVariantList { rangeSummary( 0, 0, 0, 0, 0 ) } );
  const QgsMtpl::PackageDescriptor west = packageDescriptor(
    directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 0, 0, westMetadata );

  QVariantMap eastMetadata = metadata;
  eastMetadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                       QVariantList { rangeSummary( 0, 1, 1, 0, 0 ) } );
  const QgsMtpl::PackageDescriptor east = packageDescriptor(
    directory.filePath( QStringLiteral( "0-7-0-1-0.ptp" ) ), 0, 0, eastMetadata );

  const QgsMtpl::TileDatasetBuildResult result = QgsMtpl::buildTileDataset(
    directory.path(), true, { west, east } );
  QVERIFY2( result.ok(), qPrintable( result.errorString() ) );
  QCOMPARE( result.dataset.partitionRule.id, QgsMtpl::PartitionRuleStore::ruleOneId() );
  QCOMPARE( result.dataset.minimumZoom, 0 );
  QCOMPARE( result.dataset.maximumZoom, 2 );
  QCOMPARE( result.dataset.packages.size(), 2 );
  QCOMPARE( result.dataset.packageIndexByAddress.size(), 2 );
  QVERIFY2( result.dataset.isValid(), qPrintable( result.errorString() ) );

  QgsMtpl::ResolvedTile resolved;
  QString error;
  QVERIFY2( QgsMtpl::TileResolver::resolve( result.dataset, 0, 0, 0, resolved, &error ), qPrintable( error ) );
  QCOMPARE( resolved.packageIndex, 0 );
  QVERIFY2( QgsMtpl::TileResolver::resolve( result.dataset, 0, 1, 0, resolved, &error ), qPrintable( error ) );
  QCOMPARE( resolved.packageIndex, 1 );
  QVERIFY( !QgsMtpl::TileResolver::resolve( result.dataset, 3, 0, 0, resolved, &error ) );
  QVERIFY( error.isEmpty() );

  QgsMtpl::TileDatasetDescriptor tampered = result.dataset;
  QgsMtpl::TileDatasetPackage disjointPackage = tampered.packages.constFirst();
  disjointPackage.descriptor.path = directory.filePath( QStringLiteral( "8-11-3-0-0.ptp" ) );
  disjointPackage.descriptor.displayName = QStringLiteral( "8-11-3-0-0" );
  disjointPackage.descriptor.minimumZoom = -1;
  disjointPackage.descriptor.maximumZoom = -1;
  disjointPackage.address = { 8, 11, 3, 0, 0 };
  disjointPackage.relativePath = QStringLiteral( "8-11-3-0-0.ptp" );
  disjointPackage.ranges.clear();
  tampered.packages.append( disjointPackage );
  tampered.packageIndexByAddress.insert( disjointPackage.address.key(), 2 );
  QVERIFY( !tampered.isValid( &error ) );
  QVERIFY( error.contains( QStringLiteral( "不相交" ) ) );

  QVariantMap disjointMetadata;
  disjointMetadata.insert( QStringLiteral( "tile_file_ext" ), QStringLiteral( "png" ) );
  disjointMetadata.insert( QStringLiteral( "presentTileCount" ), static_cast<qulonglong>( 0 ) );
  disjointMetadata.insert( QStringLiteral( "mtpl_tile_matrix" ), tileMatrixContract() );
  const QgsMtpl::PackageDescriptor disjoint = packageDescriptor(
    directory.filePath( QStringLiteral( "8-11-3-0-0.ptp" ) ), -1, -1, disjointMetadata );
  const QgsMtpl::TileDatasetBuildResult disjointResult = QgsMtpl::buildTileDataset(
    directory.path(), true, { disjoint } );
  QVERIFY( !disjointResult.ok() );
  QVERIFY( hasIssue( disjointResult, QStringLiteral( "package-zoom-out-of-range" ),
                     QgsMtpl::TileDatasetIssueSeverity::Error ) );

  QVariantMap rangeOutsideMetadata = metadata;
  rangeOutsideMetadata.insert( QStringLiteral( "presentTileCount" ), static_cast<qulonglong>( 0 ) );
  rangeOutsideMetadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                               QVariantList { emptyRangeSummary( 3, 0, 15, 0, 7 ) } );
  const QgsMtpl::PackageDescriptor rangeOutsideMatrix = packageDescriptor(
    directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), -1, -1, rangeOutsideMetadata );
  const QgsMtpl::TileDatasetBuildResult rangeOutsideResult = QgsMtpl::buildTileDataset(
    directory.path(), true, { rangeOutsideMatrix } );
  QVERIFY( !rangeOutsideResult.ok() );
  QVERIFY( hasIssue( rangeOutsideResult, QStringLiteral( "range-outside-matrix" ),
                     QgsMtpl::TileDatasetIssueSeverity::Error ) );
  QVERIFY( rangeOutsideResult.errorString().contains( QStringLiteral( "0-2" ) ) );

  QVariantMap explicitWebMercator = legacyMetadata();
  explicitWebMercator.insert( QStringLiteral( "presentTileCount" ), static_cast<qulonglong>( 1 ) );
  explicitWebMercator.insert( QStringLiteral( "mtpl_tile_matrix" ), QVariantMap {
    { QStringLiteral( "version" ), 1 },
    { QStringLiteral( "crs" ), QStringLiteral( "EPSG:3857" ) },
    { QStringLiteral( "scheme" ), QStringLiteral( "xyz" ) },
    { QStringLiteral( "top_left" ), QVariantList { -20037508.342789244, 20037508.342789244 } },
    { QStringLiteral( "z0_tile_span" ), 40075016.685578488 },
    { QStringLiteral( "z0_matrix_width" ), 1 },
    { QStringLiteral( "z0_matrix_height" ), 1 },
    { QStringLiteral( "scale_to_zoom_method" ), QStringLiteral( "mapbox" ) },
    { QStringLiteral( "min_zoom" ), 8 },
    { QStringLiteral( "max_zoom" ), 10 },
  } );
  const QgsMtpl::PackageDescriptor explicitPackage = packageDescriptor(
    directory.filePath( QStringLiteral( "8-11-3-1-0.ptp" ) ), 8, 8, explicitWebMercator );

  QVariantMap legacyWithoutRanges = legacyMetadata();
  legacyWithoutRanges.insert( QStringLiteral( "presentTileCount" ), static_cast<qulonglong>( 1 ) );
  const QgsMtpl::PackageDescriptor legacyOutsideCommonMatrix = packageDescriptor(
    directory.filePath( QStringLiteral( "8-11-3-0-0.ptp" ) ), 11, 11, legacyWithoutRanges );
  const QgsMtpl::TileDatasetBuildResult mixedContractResult = QgsMtpl::buildTileDataset(
    directory.path(), true, { legacyOutsideCommonMatrix, explicitPackage } );
  QVERIFY( !mixedContractResult.ok() );
  QVERIFY( hasIssue( mixedContractResult, QStringLiteral( "observed-zoom-outside-matrix" ),
                     QgsMtpl::TileDatasetIssueSeverity::Error ) );
}

void TestMtplTileset::buildDirectoryDataset()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QVariantMap rootMetadata = legacyMetadata();
  rootMetadata.insert( QStringLiteral( "mapId" ), QStringLiteral( "legacy-root-map" ) );
  rootMetadata.insert( QStringLiteral( "min_zoom_level" ), 0 );
  rootMetadata.insert( QStringLiteral( "max_zoom_level" ), 0 );
  QgsMtpl::PackageDescriptor root = packageDescriptor( directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 0, 0,
                                                       rootMetadata );
  root.metadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                        QVariantList { rangeSummary( 0, 0, 0, 0, 0 ) } );
  QVariantMap childMetadata = legacyMetadata();
  childMetadata.insert( QStringLiteral( "mapId" ), QStringLiteral( "legacy-child-map" ) );
  childMetadata.insert( QStringLiteral( "min_zoom_level" ), 8 );
  childMetadata.insert( QStringLiteral( "max_zoom_level" ), 8 );
  QgsMtpl::PackageDescriptor child = packageDescriptor( directory.filePath( QStringLiteral( "8-11-3-1-1.ptp" ) ), 8, 8,
                                                        childMetadata );
  child.metadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                         QVariantList { rangeSummary( 8, 32, 63, 32, 63 ) } );

  const QgsMtpl::TileDatasetBuildResult result = QgsMtpl::buildTileDataset(
    directory.path(), true, { root, child } );
  QVERIFY2( result.ok(), qPrintable( result.errorString() ) );
  QCOMPARE( result.dataset.partitionRule.id, QgsMtpl::PartitionRuleStore::ruleOneId() );
  QCOMPARE( result.dataset.packages.size(), 2 );
  QCOMPARE( result.dataset.minimumZoom, 0 );
  QCOMPARE( result.dataset.maximumZoom, 19 );
  QCOMPARE( result.dataset.mapId, QStringLiteral( "example-map" ) );
  QVERIFY( !result.warningMessages().isEmpty() );
  QCOMPARE( result.dataset.packages.at( 1 ).relativePath, QStringLiteral( "8-11-3-1-1.ptp" ) );
  QCOMPARE( result.dataset.packages.at( 1 ).fileLastModifiedMs, 1234567 );
  QCOMPARE( result.dataset.packages.at( 1 ).sidecarPath, child.sidecarPath );
  QCOMPARE( result.dataset.packageIndexByAddress.size(), 2 );
  QCOMPARE( result.dataset.packageIndexByAddress.value( QStringLiteral( "8/11/3/1/1" ) ), 1 );

  QgsMtpl::ResolvedTile resolved;
  QString error;
  QVERIFY2( QgsMtpl::TileResolver::resolve( result.dataset, 8, 35, 36, resolved, &error ), qPrintable( error ) );
  QCOMPARE( resolved.packageIndex, 1 );
  QVERIFY( !QgsMtpl::TileResolver::resolve( result.dataset, 16, 0, 0, resolved, &error ) );
  QVERIFY( error.isEmpty() );
  QVERIFY( !QgsMtpl::TileResolver::resolve( result.dataset, 8, 100, 36, resolved, &error ) );
  QVERIFY( error.isEmpty() );
}

void TestMtplTileset::rejectNonRasterPtpContracts()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );

  const QList<QgsMtpl::PayloadType> invalidPtpPayloads = {
    QgsMtpl::PayloadType::Unknown,
    QgsMtpl::PayloadType::Elevation,
    QgsMtpl::PayloadType::VectorTile,
    QgsMtpl::PayloadType::Files,
  };
  for ( const QgsMtpl::PayloadType payload : invalidPtpPayloads )
  {
    QgsMtpl::PackageDescriptor package = packageDescriptor(
      directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 0, 0 );
    package.payload = payload;
    if ( payload == QgsMtpl::PayloadType::Unknown )
      package.metadata.insert( QStringLiteral( "presentTileCount" ), static_cast<qulonglong>( 1 ) );
    const QgsMtpl::TileDatasetBuildResult result = QgsMtpl::buildTileDataset(
      directory.path(), true, { package } );
    QVERIFY( !result.ok() );
    QVERIFY( result.dataset.packages.isEmpty() );
    QVERIFY( result.dataset.sourcePath.isEmpty() );
    QCOMPARE( result.issues.constFirst().code, QStringLiteral( "invalid-ptp-payload" ) );
  }

  const QList<QPair<QgsMtpl::PackageFormat, QgsMtpl::PayloadType>> otherFormats = {
    { QgsMtpl::PackageFormat::Dtp, QgsMtpl::PayloadType::Elevation },
    { QgsMtpl::PackageFormat::Vtp, QgsMtpl::PayloadType::VectorTile },
    { QgsMtpl::PackageFormat::Sfp, QgsMtpl::PayloadType::Files },
  };
  for ( const auto &formatAndPayload : otherFormats )
  {
    QgsMtpl::PackageDescriptor package = packageDescriptor(
      directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 0, 0 );
    package.format = formatAndPayload.first;
    package.payload = formatAndPayload.second;
    const QgsMtpl::TileDatasetBuildResult result = QgsMtpl::buildTileDataset(
      directory.path(), true, { package } );
    QVERIFY( !result.ok() );
    QVERIFY( result.dataset.packages.isEmpty() );
    QCOMPARE( result.issues.constFirst().code, QStringLiteral( "non-ptp-package" ) );
  }

  QgsMtpl::PackageDescriptor valid = packageDescriptor(
    directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 0, 0 );
  QgsMtpl::PackageDescriptor dtp = packageDescriptor(
    directory.filePath( QStringLiteral( "8-11-3-1-1.dtp" ) ), 8, 8 );
  dtp.format = QgsMtpl::PackageFormat::Dtp;
  dtp.payload = QgsMtpl::PayloadType::Elevation;
  const QgsMtpl::TileDatasetBuildResult mixed = QgsMtpl::buildTileDataset(
    directory.path(), true, { valid, dtp } );
  QVERIFY( !mixed.ok() );
  QVERIFY( mixed.dataset.packages.isEmpty() );
  QVERIFY( mixed.dataset.sourcePath.isEmpty() );
}

void TestMtplTileset::emptyPtpPayloadInheritance()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );

  QVariantMap emptyMetadata = legacyMetadata();
  emptyMetadata.remove( QStringLiteral( "tile_file_ext" ) );
  emptyMetadata.insert( QStringLiteral( "presentTileCount" ), static_cast<qulonglong>( 0 ) );
  emptyMetadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                        QVariantList { emptyRangeSummary( 8, 32, 63, 32, 63 ) } );
  QgsMtpl::PackageDescriptor empty = packageDescriptor(
    directory.filePath( QStringLiteral( "8-11-3-1-1.ptp" ) ), -1, -1, emptyMetadata );
  empty.payload = QgsMtpl::PayloadType::Unknown;

  const QgsMtpl::TileDatasetBuildResult standalone = QgsMtpl::buildTileDataset(
    directory.path(), true, { empty } );
  QVERIFY( !standalone.ok() );
  QVERIFY( standalone.dataset.packages.isEmpty() );
  QVERIFY( standalone.dataset.sourcePath.isEmpty() );
  QVERIFY( hasIssue( standalone, QStringLiteral( "unconfirmed-empty-image" ),
                     QgsMtpl::TileDatasetIssueSeverity::Error ) );
  QVERIFY( !hasIssue( standalone, QStringLiteral( "empty-image-inherited" ),
                      QgsMtpl::TileDatasetIssueSeverity::Warning ) );

  QVariantMap rasterMetadata = legacyMetadata();
  rasterMetadata.insert( QStringLiteral( "presentTileCount" ), static_cast<qulonglong>( 1 ) );
  rasterMetadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                         QVariantList { rangeSummary( 0, 0, 0, 0, 0 ) } );
  const QgsMtpl::PackageDescriptor raster = packageDescriptor(
    directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 0, 0, rasterMetadata );

  const QgsMtpl::TileDatasetBuildResult forward = QgsMtpl::buildTileDataset(
    directory.path(), true, { raster, empty } );
  const QgsMtpl::TileDatasetBuildResult reverse = QgsMtpl::buildTileDataset(
    directory.path(), true, { empty, raster } );
  QVERIFY2( forward.ok(), qPrintable( forward.errorString() ) );
  QVERIFY2( reverse.ok(), qPrintable( reverse.errorString() ) );
  QCOMPARE( forward.warningMessages(), reverse.warningMessages() );
  QCOMPARE( forward.dataset.packages.size(), 2 );
  QCOMPARE( reverse.dataset.packages.size(), 2 );
  for ( int index = 0; index < forward.dataset.packages.size(); ++index )
  {
    QCOMPARE( forward.dataset.packages.at( index ).descriptor.path,
              reverse.dataset.packages.at( index ).descriptor.path );
    QCOMPARE( forward.dataset.packages.at( index ).descriptor.payload,
              reverse.dataset.packages.at( index ).descriptor.payload );
  }
  QCOMPARE( forward.dataset.payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( forward.dataset.packages.at( 1 ).descriptor.path, empty.path );
  QCOMPARE( forward.dataset.packages.at( 1 ).descriptor.payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( reverse.dataset.packages.at( 1 ).descriptor.payload, QgsMtpl::PayloadType::RasterImage );
  QVERIFY( hasIssue( forward, QStringLiteral( "empty-image-inherited" ),
                     QgsMtpl::TileDatasetIssueSeverity::Warning ) );
  QVERIFY( hasIssue( reverse, QStringLiteral( "empty-image-inherited" ),
                     QgsMtpl::TileDatasetIssueSeverity::Warning ) );

  QgsMtpl::PackageDescriptor badRaster = raster;
  badRaster.minimumZoom = 8;
  badRaster.maximumZoom = 8;
  badRaster.metadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                             QVariantList { rangeSummary( 8, 0, 0, 0, 0 ) } );
  const QgsMtpl::TileDatasetBuildResult withBadRaster = QgsMtpl::buildTileDataset(
    directory.path(), true, { empty, badRaster } );
  QVERIFY( !withBadRaster.ok() );
  QVERIFY( withBadRaster.dataset.packages.isEmpty() );
  QVERIFY( hasIssue( withBadRaster, QStringLiteral( "observed-zoom-out-of-band" ),
                     QgsMtpl::TileDatasetIssueSeverity::Error ) );
  QVERIFY( !hasIssue( withBadRaster, QStringLiteral( "empty-image-inherited" ),
                      QgsMtpl::TileDatasetIssueSeverity::Warning ) );

  QgsMtpl::PackageDescriptor vector = raster;
  vector.payload = QgsMtpl::PayloadType::VectorTile;
  const QgsMtpl::TileDatasetBuildResult withVector = QgsMtpl::buildTileDataset(
    directory.path(), true, { empty, vector } );
  QVERIFY( !withVector.ok() );
  QVERIFY( withVector.dataset.packages.isEmpty() );
  QVERIFY( hasIssue( withVector, QStringLiteral( "invalid-ptp-payload" ),
                     QgsMtpl::TileDatasetIssueSeverity::Error ) );
  QVERIFY( !hasIssue( withVector, QStringLiteral( "empty-image-inherited" ),
                      QgsMtpl::TileDatasetIssueSeverity::Warning ) );

  QgsMtpl::PackageDescriptor encryptedEmpty = empty;
  encryptedEmpty.encryption = QgsMtpl::EncryptionState::Encrypted;
  encryptedEmpty.readiness = QgsMtpl::ReadinessState::UnverifiableEmpty;
  encryptedEmpty.credentialSource = QgsMtpl::CredentialSource::Explicit;
  QVERIFY( encryptedEmpty.isCredentialedEmptyPtp() );

  const QgsMtpl::TileDatasetBuildResult encryptedInherited = QgsMtpl::buildTileDataset(
    directory.path(), true, { raster, encryptedEmpty } );
  QVERIFY2( encryptedInherited.ok(), qPrintable( encryptedInherited.errorString() ) );
  QCOMPARE( encryptedInherited.dataset.packages.at( 1 ).descriptor.payload,
            QgsMtpl::PayloadType::RasterImage );
  QVERIFY( encryptedInherited.dataset.packages.at( 1 ).descriptor.isCredentialedEmptyPtp() );

  QgsMtpl::PackageDescriptor encryptedDeclaredImage = encryptedEmpty;
  encryptedDeclaredImage.payload = QgsMtpl::PayloadType::RasterImage;
  const QgsMtpl::TileDatasetBuildResult encryptedDeclared = QgsMtpl::buildTileDataset(
    directory.path(), true, { encryptedDeclaredImage } );
  QVERIFY2( encryptedDeclared.ok(), qPrintable( encryptedDeclared.errorString() ) );
  QVERIFY2( encryptedDeclared.dataset.isValid(), qPrintable( encryptedDeclared.errorString() ) );

  QgsMtpl::PackageDescriptor missingCredentialSource = encryptedDeclaredImage;
  missingCredentialSource.credentialSource = QgsMtpl::CredentialSource::None;
  QVERIFY( !missingCredentialSource.isCredentialedEmptyPtp() );
  const QgsMtpl::TileDatasetBuildResult missingCredentialResult = QgsMtpl::buildTileDataset(
    directory.path(), true, { missingCredentialSource } );
  QVERIFY( !missingCredentialResult.ok() );
  QVERIFY( hasIssue( missingCredentialResult, QStringLiteral( "package-not-ready" ),
                     QgsMtpl::TileDatasetIssueSeverity::Error ) );

  QgsMtpl::PackageDescriptor lockedEmpty = encryptedDeclaredImage;
  lockedEmpty.encryption = QgsMtpl::EncryptionState::Locked;
  lockedEmpty.readiness = QgsMtpl::ReadinessState::KeyRequired;
  lockedEmpty.credentialSource = QgsMtpl::CredentialSource::None;
  QVERIFY( !lockedEmpty.isCredentialedEmptyPtp() );
  const QgsMtpl::TileDatasetBuildResult lockedResult = QgsMtpl::buildTileDataset(
    directory.path(), true, { lockedEmpty } );
  QVERIFY( !lockedResult.ok() );
  QVERIFY( hasIssue( lockedResult, QStringLiteral( "package-not-ready" ),
                     QgsMtpl::TileDatasetIssueSeverity::Error ) );
}

void TestMtplTileset::atomicBuildIsOrderIndependent()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );

  QVariantMap invalidMetadata = legacyMetadata( QStringLiteral( "map-a" ) );
  invalidMetadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                          QVariantList { rangeSummary( 8, 0, 0, 0, 0 ) } );
  QgsMtpl::PackageDescriptor invalid = packageDescriptor(
    directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 8, 8, invalidMetadata );

  QVariantMap validMetadata = legacyMetadata( QStringLiteral( "map-b" ) );
  validMetadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                        QVariantList { rangeSummary( 8, 32, 63, 32, 63 ) } );
  QgsMtpl::PackageDescriptor valid = packageDescriptor(
    directory.filePath( QStringLiteral( "8-11-3-1-1.ptp" ) ), 8, 8, validMetadata );

  const QgsMtpl::TileDatasetBuildResult forward = QgsMtpl::buildTileDataset(
    directory.path(), true, { invalid, valid } );
  const QgsMtpl::TileDatasetBuildResult reverse = QgsMtpl::buildTileDataset(
    directory.path(), true, { valid, invalid } );
  QVERIFY( !forward.ok() );
  QVERIFY( !reverse.ok() );
  QVERIFY( forward.dataset.packages.isEmpty() );
  QVERIFY( reverse.dataset.packages.isEmpty() );
  QVERIFY( forward.dataset.sourcePath.isEmpty() );
  QVERIFY( reverse.dataset.sourcePath.isEmpty() );

  QStringList forwardCodes;
  QStringList reverseCodes;
  for ( const QgsMtpl::TileDatasetIssue &issue : forward.issues )
    forwardCodes.append( issue.code );
  for ( const QgsMtpl::TileDatasetIssue &issue : reverse.issues )
    reverseCodes.append( issue.code );
  QCOMPARE( forwardCodes, reverseCodes );
  QCOMPARE( forwardCodes, QStringList( { QStringLiteral( "observed-zoom-out-of-band" ) } ) );
}

void TestMtplTileset::rejectDatasetConflicts()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::PackageDescriptor first = packageDescriptor( directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 0, 7,
                                                        legacyMetadata( QStringLiteral( "map-a" ) ) );
  QgsMtpl::PackageDescriptor second = packageDescriptor( directory.filePath( QStringLiteral( "8-11-3-1-1.ptp" ) ), 8, 8,
                                                         legacyMetadata( QStringLiteral( "map-b" ) ) );
  QgsMtpl::TileDatasetBuildResult result = QgsMtpl::buildTileDataset( directory.path(), true, { first, second } );
  QVERIFY( !result.ok() );
  QVERIFY( result.errorString().contains( QStringLiteral( "map_id" ) ) );
  QVERIFY( result.dataset.packages.isEmpty() );
  QVERIFY( result.dataset.sourcePath.isEmpty() );

  first = packageDescriptor( directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 0, 7 );
  second = packageDescriptor( directory.filePath( QStringLiteral( "8-11-3-1-1.ptp" ) ), 8, 8 );
  second.metadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                          QVariantList { rangeSummary( 8, 100, 101, 32, 33 ) } );
  result = QgsMtpl::buildTileDataset( directory.path(), true, { first, second } );
  QVERIFY( !result.ok() );
  QVERIFY( result.errorString().contains( QStringLiteral( "范围" ) ) );
  QVERIFY( result.dataset.packages.isEmpty() );

  QgsMtpl::PackageDescriptor badName = packageDescriptor( directory.filePath( QStringLiteral( "tiles.ptp" ) ), 0, 0 );
  result = QgsMtpl::buildTileDataset( directory.path(), true, { badName } );
  QVERIFY( !result.ok() );

  QgsMtpl::PackageDescriptor validEmpty = packageDescriptor(
    directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), -1, -1 );
  validEmpty.metadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                              QVariantList { emptyRangeSummary( 7, 0, 127, 0, 127 ) } );
  result = QgsMtpl::buildTileDataset( directory.path(), true, { validEmpty } );
  QVERIFY2( result.ok(), qPrintable( result.errorString() ) );
  QCOMPARE( result.dataset.packages.constFirst().ranges.size(), 1 );
  QCOMPARE( result.dataset.packages.constFirst().ranges.constFirst().presentCount, 0ULL );

  QgsMtpl::PackageDescriptor invalidEmpty = validEmpty;
  invalidEmpty.metadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                                QVariantList { emptyRangeSummary( 8, 0, 255, 0, 255 ) } );
  result = QgsMtpl::buildTileDataset( directory.path(), true, { invalidEmpty } );
  QVERIFY( !result.ok() );
  QVERIFY( result.errorString().contains( QStringLiteral( "文件名" ) ) );
}

void TestMtplTileset::singleFileCompatibility()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::PackageDescriptor package = packageDescriptor( directory.filePath( QStringLiteral( "legacy-name.ptp" ) ), 3, 3 );
  package.metadata.insert( QStringLiteral( "_mtplRangeBounds" ),
                           QVariantList { rangeSummary( 3, 2, 3, 4, 5 ) } );
  const QgsMtpl::TileDatasetBuildResult result = QgsMtpl::buildTileDataset( package.path, false, { package } );
  QVERIFY2( result.ok(), qPrintable( result.errorString() ) );
  QVERIFY( result.dataset.compatibilityMode );
  QCOMPARE( result.dataset.packages.size(), 1 );
  QCOMPARE( result.dataset.minimumZoom, 3 );
  QCOMPARE( result.dataset.maximumZoom, 3 );

  QgsMtpl::ResolvedTile resolved;
  QString error;
  QVERIFY2( QgsMtpl::TileResolver::resolve( result.dataset, 3, 2, 4, resolved, &error ), qPrintable( error ) );
  QCOMPARE( resolved.packageIndex, 0 );
  QCOMPARE( resolved.storageX, 2U );
  QCOMPARE( resolved.storageY, 4U );
  QVERIFY( !QgsMtpl::TileResolver::resolve( result.dataset, 3, 7, 7, resolved, &error ) );
  QVERIFY( error.isEmpty() );
}

QGSTEST_MAIN( TestMtplTileset )
#include "test_mtpl_tileset.moc"
