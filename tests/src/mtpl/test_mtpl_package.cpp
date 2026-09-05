/***************************************************************************
  test_mtpl_package.cpp
  ---------------------
  Contract tests for portable, runtime-generated MTPL package discovery.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "mtpltestfixtures.h"
#include "mtpltestutils.h"

#include "qgsmtplpackage.h"
#include "qgsmtplpackageservice.h"
#include "qgsmtpltileset.h"
#include "qgstest.h"

#include <mtpl/mtpl.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>

namespace
{
  QList<QgsMtplTest::SfpFixtureEntry> sfpEntries( mtpl_storage_mode_t mode )
  {
    return {
      { QStringLiteral( "nested/readme.txt" ), QByteArrayLiteral( "portable fixture" ), mode },
      { QStringLiteral( "unicode/地图.json" ), QByteArrayLiteral( "{}" ), mode },
      { QStringLiteral( "empty.bin" ), QByteArray(), mode },
    };
  }

  QByteArray encodedImage( const QByteArray &format, int width, int height, QString &error )
  {
    QImage image( width, height, QImage::Format_ARGB32 );
    image.fill( qRgba( 40, 120, 200, 255 ) );

    QByteArray bytes;
    QBuffer buffer( &bytes );
    if ( !buffer.open( QIODevice::WriteOnly ) )
    {
      error = QStringLiteral( "Unable to open the image fixture buffer." );
      return QByteArray();
    }
    QImageWriter writer( &buffer, format );
    writer.setQuality( 90 );
    if ( !writer.write( image ) )
    {
      error = writer.errorString();
      return QByteArray();
    }
    error.clear();
    return bytes;
  }

  bool writeEmptyNonPtpTileFixture( const QString &path,
                                    QgsMtpl::PackageFormat format,
                                    quint32 tileSize,
                                    const QByteArray &metadata,
                                    bool addEmptyRange,
                                    QString &error )
  {
    const QByteArray pathUtf8 = QFileInfo( path ).absoluteFilePath().toUtf8();
    const mtpl_buffer_view_t metadataView = {
      metadata.isEmpty() ? nullptr : reinterpret_cast<const uint8_t *>( metadata.constData() ),
      static_cast<size_t>( metadata.size() )
    };
    const mtpl_tile_range_t range = { 3, 2, 4, 5, 6 };
    mtpl_status_t status = MTPL_STATUS_FORMAT_MISMATCH;

    if ( format == QgsMtpl::PackageFormat::Dtp )
    {
      mtpl_dtp_writer_t *writer = nullptr;
      status = mtpl_dtp_writer_create(
        pathUtf8.constData(), tileSize, metadataView, MTPL_STORAGE_PLAIN, nullptr, &writer );
      if ( status == MTPL_STATUS_OK && addEmptyRange )
        status = mtpl_dtp_writer_add_range( writer, &range );
      if ( writer )
      {
        const mtpl_status_t closeStatus = mtpl_dtp_writer_close( writer );
        if ( status == MTPL_STATUS_OK )
          status = closeStatus;
      }
    }
    else if ( format == QgsMtpl::PackageFormat::Vtp )
    {
      mtpl_vtp_writer_t *writer = nullptr;
      status = mtpl_vtp_writer_create(
        pathUtf8.constData(), tileSize, metadataView, MTPL_STORAGE_PLAIN, nullptr, &writer );
      if ( status == MTPL_STATUS_OK && addEmptyRange )
        status = mtpl_vtp_writer_add_range( writer, &range );
      if ( writer )
      {
        const mtpl_status_t closeStatus = mtpl_vtp_writer_close( writer );
        if ( status == MTPL_STATUS_OK )
          status = closeStatus;
      }
    }

    if ( status != MTPL_STATUS_OK )
    {
      const char *statusText = mtpl_status_string( status );
      error = QStringLiteral( "Unable to write empty non-PTP tile fixture: %1 (%2)." )
                .arg( statusText ? QString::fromUtf8( statusText ) : QStringLiteral( "unknown MTPL status" ) )
                .arg( static_cast<int>( status ) );
      QFile::remove( path );
      return false;
    }

    error.clear();
    return true;
  }
}

class TestMtplPackage : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void valueTypeContracts();
    void cryptoKeyValidation();
    void packageFormatContracts();
    void probeCancellation();
    void probeTileFixtures_data();
    void probeTileFixtures();
    void probePtpRasterImages_data();
    void probePtpRasterImages();
    void probeReservedMetadataCannotSpoofInternalState();
    void externalTileJsonIsDisplayOnly();
    void rejectInvalidPtpPayloads();
    void probeEncryptedEmptyPtp();
    void probePtpByMagic();
    void probeNonPtpTileContracts_data();
    void probeNonPtpTileContracts();
    void probeSfpByMagicWithEmptyEntry();
    void probeSfpFixtures_data();
    void probeSfpFixtures();
    void probeVtpReadinessStates();
    void probeMixedSfpWithMatchingAndWrongKeys();
    void probeEmptySfp();
    void probeUnsupportedAndUnreadable();
    void probeLongUnicodePath();
    void probeLongPtpPathReadsTile();
    void probePlainDirectory();
    void probeDirectoryNonrecursively();
    void probeExplicitKeyDirectory();
    void probeTaskLifecycle_data();
    void probeTaskLifecycle();
    void probeTaskCancellation_data();
    void probeTaskCancellation();
    void probePtpDirectoryAfterCanceledTask();

  private:
    QgsMtplTest::ArtifactWorkspace mWorkspace;
};

void TestMtplPackage::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();

  QString error;
  if ( !mWorkspace.initialize( QStringLiteral( "package" ), error ) )
    QFAIL( qPrintable( error ) );
}

void TestMtplPackage::cleanupTestCase()
{
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
  QgsApplication::exitQgis();
}

void TestMtplPackage::valueTypeContracts()
{
  QgsMtpl::PackageDescriptor descriptor;
  QVERIFY( !descriptor.isSpatial() );
  QVERIFY( !descriptor.isLocked() );
  QVERIFY( !descriptor.isReady() );
  QVERIFY( !descriptor.requiresKey() );

  descriptor.format = QgsMtpl::PackageFormat::Ptp;
  descriptor.readiness = QgsMtpl::ReadinessState::PlainReady;
  QVERIFY( descriptor.isSpatial() );
  QVERIFY( descriptor.isReady() );

  descriptor.format = QgsMtpl::PackageFormat::Sfp;
  descriptor.encryption = QgsMtpl::EncryptionState::Locked;
  descriptor.readiness = QgsMtpl::ReadinessState::KeyRequired;
  QVERIFY( !descriptor.isSpatial() );
  QVERIFY( descriptor.isLocked() );
  QVERIFY( !descriptor.isReady() );
  QVERIFY( descriptor.requiresKey() );

  QgsMtpl::PackageDescriptor ready;
  ready.readiness = QgsMtpl::ReadinessState::KeyVerified;
  QgsMtpl::PackageDescriptor unavailable;
  unavailable.readiness = QgsMtpl::ReadinessState::KeyRejectedOrCorrupt;
  QgsMtpl::ProbeResult result;
  result.packages = { ready, unavailable, descriptor };
  QCOMPARE( result.readyPackageCount(), 1 );
  QVERIFY( result.hasReadyPackages() );
}

void TestMtplPackage::cryptoKeyValidation()
{
  QgsMtpl::CryptoKeys keys;
  QString error;
  QVERIFY( keys.isEmpty() );
  QVERIFY( !keys.hasAnyValue() );
  QVERIFY( !keys.isValid( &error ) );
  QVERIFY( !error.isEmpty() );

  keys.privateKey = QByteArray( 32, 'p' ).toBase64();
  keys.deviceKey = QByteArrayLiteral( "00112233445566778899aabbccddeeff" );
  QVERIFY2( keys.isValid( &error ), qPrintable( error ) );
  QVERIFY( error.isEmpty() );

  QgsMtpl::CryptoKeys copy( keys );
  keys.clear();
  QVERIFY( keys.isEmpty() );
  QVERIFY( copy.isValid() );
  copy.deviceKey = QByteArrayLiteral( "001Z" );
  QVERIFY( !copy.isValid( &error ) );
  copy.clear();
  QVERIFY( copy.isEmpty() );

  QgsMtpl::CryptoKeys fixture = QgsMtplTest::fixtureKeys();
  QgsMtpl::CryptoKeys different = QgsMtplTest::differentFixtureKeys();
  QVERIFY2( fixture.isValid( &error ), qPrintable( error ) );
  QVERIFY2( different.isValid( &error ), qPrintable( error ) );
  QVERIFY( fixture.privateKey != different.privateKey || fixture.deviceKey != different.deviceKey );
}

void TestMtplPackage::packageFormatContracts()
{
  QCOMPARE( QgsMtpl::packageFormatFromPath( QStringLiteral( "sample.PTP" ) ), QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( QgsMtpl::packageFormatFromPath( QStringLiteral( "sample.dtp" ) ), QgsMtpl::PackageFormat::Dtp );
  QCOMPARE( QgsMtpl::packageFormatFromPath( QStringLiteral( "sample.VtP" ) ), QgsMtpl::PackageFormat::Vtp );
  QCOMPARE( QgsMtpl::packageFormatFromPath( QStringLiteral( "sample.sfp" ) ), QgsMtpl::PackageFormat::Sfp );
  QCOMPARE( QgsMtpl::packageFormatFromPath( QStringLiteral( "sample.zip" ) ), QgsMtpl::PackageFormat::Unknown );
  QCOMPARE( QgsMtplPackageService::supportedSuffixes(),
            QStringList( { QStringLiteral( "ptp" ), QStringLiteral( "dtp" ), QStringLiteral( "vtp" ), QStringLiteral( "sfp" ) } ) );
  QCOMPARE( QgsMtplPackageService::supportedNameFilters(),
            QStringList( { QStringLiteral( "*.ptp" ), QStringLiteral( "*.dtp" ), QStringLiteral( "*.vtp" ), QStringLiteral( "*.sfp" ) } ) );
  QVERIFY( QgsMtpl::packageFileFilter().contains( QStringLiteral( "*.ptp" ) ) );
  QVERIFY( QgsMtpl::packageFileFilter().contains( QStringLiteral( "*.sfp" ) ) );
}

void TestMtplPackage::probeCancellation()
{
  int progressCallCount = 0;
  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    QStringLiteral( "path-is-never-read" ), {}, true, QgsMtpl::CredentialSource::None,
    [] { return true; },
    [&progressCallCount]( double progress )
    {
      ++progressCallCount;
      QCOMPARE( progress, 0.0 );
    } );
  QVERIFY( result.canceled );
  QVERIFY( !result.ok );
  QVERIFY( result.packages.isEmpty() );
  QCOMPARE( progressCallCount, 1 );
}

void TestMtplPackage::probeTileFixtures_data()
{
  QTest::addColumn<int>( "formatValue" );
  QTest::addColumn<QString>( "suffix" );
  QTest::addColumn<int>( "payloadValue" );
  QTest::addColumn<int>( "tileSize" );
  QTest::newRow( "ptp" ) << static_cast<int>( QgsMtpl::PackageFormat::Ptp ) << QStringLiteral( "ptp" )
                          << static_cast<int>( QgsMtpl::PayloadType::RasterImage ) << 256;
  QTest::newRow( "dtp" ) << static_cast<int>( QgsMtpl::PackageFormat::Dtp ) << QStringLiteral( "dtp" )
                          << static_cast<int>( QgsMtpl::PayloadType::Elevation ) << 33;
  QTest::newRow( "vtp" ) << static_cast<int>( QgsMtpl::PackageFormat::Vtp ) << QStringLiteral( "vtp" )
                          << static_cast<int>( QgsMtpl::PayloadType::VectorTile ) << 256;
}

void TestMtplPackage::probeTileFixtures()
{
  QFETCH( int, formatValue );
  QFETCH( QString, suffix );
  QFETCH( int, payloadValue );
  QFETCH( int, tileSize );
  const QgsMtpl::PackageFormat format = static_cast<QgsMtpl::PackageFormat>( formatValue );
  QString error;

  const QString plainPath = mWorkspace.filePath( QStringLiteral( "plain-%1.%1" ).arg( suffix ) );
  QVERIFY2( QgsMtplTest::writeTileFixture( plainPath, format, MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  const QgsMtpl::ProbeResult plainResult = QgsMtplPackageService::probePath(
    plainPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( plainResult.ok, qPrintable( plainResult.error ) );
  QCOMPARE( plainResult.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &plain = plainResult.packages.constFirst();
  QCOMPARE( plain.format, format );
  QCOMPARE( static_cast<int>( plain.payload ), payloadValue );
  QCOMPARE( plain.encryption, QgsMtpl::EncryptionState::Plain );
  QCOMPARE( plain.readiness, QgsMtpl::ReadinessState::PlainReady );
  QCOMPARE( plain.credentialSource, QgsMtpl::CredentialSource::None );
  QCOMPARE( plain.tileSize, tileSize );
  QCOMPARE( plain.minimumZoom, 0 );
  QCOMPARE( plain.maximumZoom, 0 );
  QCOMPARE( plain.metadata.value( QStringLiteral( "presentTileCount" ) ).toULongLong(), 1ULL );
  const QVariantList rangeBounds = plain.metadata.value( QStringLiteral( "_mtplRangeBounds" ) ).toList();
  if ( format == QgsMtpl::PackageFormat::Ptp )
  {
    QCOMPARE( rangeBounds.size(), 1 );
    const QVariantMap range = rangeBounds.constFirst().toMap();
    QCOMPARE( range.value( QStringLiteral( "zoom" ) ).toUInt(), 0U );
    QCOMPARE( range.value( QStringLiteral( "presentXMin" ) ).toULongLong(), 0ULL );
    QCOMPARE( range.value( QStringLiteral( "presentXMax" ) ).toULongLong(), 0ULL );
    QCOMPARE( range.value( QStringLiteral( "presentYMin" ) ).toULongLong(), 0ULL );
    QCOMPARE( range.value( QStringLiteral( "presentYMax" ) ).toULongLong(), 0ULL );
  }
  else
  {
    QVERIFY( rangeBounds.isEmpty() );
    QCOMPARE( plain.metadata.value( QStringLiteral( "minimumZoom" ) ).toInt(), 0 );
    QCOMPARE( plain.metadata.value( QStringLiteral( "maximumZoom" ) ).toInt(), 0 );
  }
  QVERIFY( plain.isReady() );
  QVERIFY( plain.fileSize > 0 );

  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  const QString encryptedPath = mWorkspace.filePath( QStringLiteral( "encrypted-%1.%1" ).arg( suffix ) );
  QVERIFY2( QgsMtplTest::writeTileFixture( encryptedPath, format, MTPL_STORAGE_ENCRYPTED, keys, error ), qPrintable( error ) );
  const QgsMtpl::ProbeResult lockedResult = QgsMtplPackageService::probePath(
    encryptedPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( lockedResult.ok, qPrintable( lockedResult.error ) );
  QCOMPARE( lockedResult.packages.size(), 1 );
  QCOMPARE( lockedResult.packages.constFirst().format, format );
  QCOMPARE( lockedResult.packages.constFirst().encryption, QgsMtpl::EncryptionState::Locked );
  QCOMPARE( lockedResult.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRequired );
  QVERIFY( lockedResult.packages.constFirst().requiresKey() );

  const QgsMtpl::ProbeResult unlockedResult = QgsMtplPackageService::probePath(
    encryptedPath, keys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( unlockedResult.ok, qPrintable( unlockedResult.error ) );
  QCOMPARE( unlockedResult.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &unlocked = unlockedResult.packages.constFirst();
  QCOMPARE( unlocked.format, format );
  QCOMPARE( static_cast<int>( unlocked.payload ), payloadValue );
  QCOMPARE( unlocked.encryption, QgsMtpl::EncryptionState::Encrypted );
  QCOMPARE( unlocked.readiness, QgsMtpl::ReadinessState::KeyVerified );
  QCOMPARE( unlocked.credentialSource, QgsMtpl::CredentialSource::Explicit );
  QCOMPARE( unlocked.tileSize, tileSize );
  QVERIFY( unlocked.isReady() );
}

void TestMtplPackage::probePtpRasterImages_data()
{
  QTest::addColumn<QByteArray>( "imageFormat" );
  QTest::newRow( "png" ) << QByteArrayLiteral( "png" );
  QTest::newRow( "jpeg" ) << QByteArrayLiteral( "jpeg" );
  QTest::newRow( "webp" ) << QByteArrayLiteral( "webp" );
}

void TestMtplPackage::probePtpRasterImages()
{
  QFETCH( QByteArray, imageFormat );
  if ( !QImageWriter::supportedImageFormats().contains( imageFormat ) )
    QSKIP( qPrintable( QStringLiteral( "Qt image writer does not support %1." ).arg( QString::fromLatin1( imageFormat ) ) ) );

  QString error;
  const QByteArray image = encodedImage( imageFormat, 256, 256, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  const QString packagePath = mWorkspace.filePath(
    QStringLiteral( "ptp-raster-%1.ptp" ).arg( QString::fromLatin1( imageFormat ) ) );
  const QByteArray metadata = QByteArrayLiteral( "{\"payload\":\"pbf\",\"tile_file_ext\":\"" ) +
                              imageFormat + QByteArrayLiteral( "\"}" );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath, 256, metadata, MTPL_STORAGE_PLAIN, {},
              { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, image } }, error ),
            qPrintable( error ) );

  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QCOMPARE( result.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &descriptor = result.packages.constFirst();
  QCOMPARE( descriptor.format, QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( descriptor.payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( descriptor.tileSize, 256 );
  QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
  QVERIFY( descriptor.isReady() );
}

void TestMtplPackage::probeReservedMetadataCannotSpoofInternalState()
{
  QString error;
  const QByteArray image = encodedImage( QByteArrayLiteral( "png" ), 256, 256, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "reserved-metadata.ptp" ) );
  const QByteArray metadata = QByteArrayLiteral(
    "{\"tile_file_ext\":\"png\","
    "\"_MTPLRangeBounds\":[{\"zoom\":30,\"presentCount\":99}],"
    "\"_mtplPtpContractFailure\":\"declared-vector\"}" );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath, 256, metadata, MTPL_STORAGE_PLAIN, {},
              { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, image } }, error ),
            qPrintable( error ) );

  QgsMtpl::PackageDescriptor descriptor;
  QVERIFY2( QgsMtplPackageService::probePackage(
              packagePath, descriptor, error, {}, true, QgsMtpl::CredentialSource::None ),
            qPrintable( error ) );
  QCOMPARE( descriptor.payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( QgsMtplPackageService::ptpContractFailure( descriptor ),
            QgsMtplPackageService::PtpContractFailure::None );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "_MTPLRangeBounds" ) ) );
  const QVariantList ranges = descriptor.metadata.value( QStringLiteral( "_mtplRangeBounds" ) ).toList();
  QCOMPARE( ranges.size(), 1 );
  QCOMPARE( ranges.constFirst().toMap().value( QStringLiteral( "zoom" ) ).toUInt(), 0U );
  QCOMPARE( ranges.constFirst().toMap().value( QStringLiteral( "presentCount" ) ).toULongLong(), 1ULL );
}

void TestMtplPackage::externalTileJsonIsDisplayOnly()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QString error;
  const QByteArray image = encodedImage( QByteArrayLiteral( "png" ), 256, 256, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  const QString packagePath = directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath, 256,
              QByteArrayLiteral( "{\"tile_file_ext\":\"png\",\"name\":\"embedded-name\",\"crs\":\"EPSG:3857\",\"scheme\":\"xyz\"}" ),
              MTPL_STORAGE_PLAIN, {}, { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, image } }, error ),
            qPrintable( error ) );

  const QString tileJsonPath = directory.filePath( QStringLiteral( "tilejson.json" ) );
  QFile tileJson( tileJsonPath );
  QVERIFY( tileJson.open( QIODevice::WriteOnly ) );
  const QByteArray tileJsonBytes = QByteArrayLiteral(
    "{\"name\":\"external-name\",\"description\":\"external-description\","
    "\"attribution\":\"external-attribution\",\"center\":[12.5,34.5,4],"
    "\"crs\":\"EPSG:3413\",\"scheme\":\"tms\",\"minzoom\":9,\"maxzoom\":9,"
    "\"tileSize\":512,\"format\":\"pbf\",\"type\":\"vector\","
    "\"bounds\":[0,0,1,1],\"tiles\":[\"https://invalid.example/{z}/{x}/{y}\"]}" );
  QCOMPARE( tileJson.write( tileJsonBytes ), static_cast<qint64>( tileJsonBytes.size() ) );
  tileJson.close();

  QFile packageTileJson( directory.filePath( QStringLiteral( "0-7-0-0-0.json" ) ) );
  QVERIFY( packageTileJson.open( QIODevice::WriteOnly ) );
  const QByteArray packageTileJsonBytes = QByteArrayLiteral( "{\"description\":\"per-package-description\"}" );
  QCOMPARE( packageTileJson.write( packageTileJsonBytes ),
            static_cast<qint64>( packageTileJsonBytes.size() ) );
  packageTileJson.close();

  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    directory.path(), {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QCOMPARE( result.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &descriptor = result.packages.constFirst();
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "name" ) ).toString(), QStringLiteral( "embedded-name" ) );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "description" ) ).toString(), QStringLiteral( "external-description" ) );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "attribution" ) ).toString(), QStringLiteral( "external-attribution" ) );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "center" ) ).toList(),
            QVariantList( { 12.5, 34.5, 4.0 } ) );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "tiles" ) ) );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "bounds" ) ) );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "minzoom" ) ) );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "maxzoom" ) ) );
  QCOMPARE( descriptor.payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( descriptor.crsAuthId, QStringLiteral( "EPSG:3857" ) );
  QCOMPARE( descriptor.scheme, QStringLiteral( "xyz" ) );
  QCOMPARE( descriptor.tileSize, 256 );
  QCOMPARE( descriptor.minimumZoom, 0 );
  QCOMPARE( descriptor.maximumZoom, 0 );

  QVERIFY( tileJson.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  const QByteArray malformed = QByteArrayLiteral( "{not-json" );
  QCOMPARE( tileJson.write( malformed ), static_cast<qint64>( malformed.size() ) );
  tileJson.close();
  const QgsMtpl::ProbeResult malformedResult = QgsMtplPackageService::probePath(
    directory.path(), {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( malformedResult.ok, qPrintable( malformedResult.error ) );
  QCOMPARE( malformedResult.packages.size(), 1 );
  QVERIFY( !malformedResult.packages.constFirst().metadata.contains( QStringLiteral( "description" ) ) );

  const QgsMtpl::ProbeResult singleFileResult = QgsMtplPackageService::probePath(
    packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( singleFileResult.ok, qPrintable( singleFileResult.error ) );
  QCOMPARE( singleFileResult.packages.constFirst().metadata.value( QStringLiteral( "description" ) ).toString(),
            QStringLiteral( "per-package-description" ) );
}

void TestMtplPackage::rejectInvalidPtpPayloads()
{
  QString error;
  const QByteArray image129 = encodedImage( QByteArrayLiteral( "png" ), 129, 129, error );
  QVERIFY2( !image129.isEmpty(), qPrintable( error ) );

  struct InvalidPtpCase
  {
    QString name;
    QByteArray metadata;
    QByteArray payload;
    QString expectedMessage;
  };
  const QList<InvalidPtpCase> cases = {
    { QStringLiteral( "metadata-vector" ), QByteArrayLiteral( "{\"tile_file_ext\":\"pbf\",\"type\":\"png\"}" ),
      QgsMtplTest::minimalVectorTilePayload(), QStringLiteral( "VTP" ) },
    { QStringLiteral( "metadata-elevation" ), QByteArrayLiteral( "{\"type\":\"elevation\"}" ),
      QByteArray( 256, '\0' ), QStringLiteral( "DTP" ) },
    { QStringLiteral( "pbf-disguised-as-png" ), QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
      QgsMtplTest::minimalVectorTilePayload(), QStringLiteral( "文件魔数" ) },
    { QStringLiteral( "wrong-size" ), QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
      image129, QStringLiteral( "129×129" ) },
    { QStringLiteral( "truncated-png" ), QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
      QByteArray::fromHex( "89504e470d0a1a0a0000000d49484452" ), QStringLiteral( "不完整" ) },
  };

  for ( const InvalidPtpCase &testCase : cases )
  {
    const QString packagePath = mWorkspace.filePath( testCase.name + QStringLiteral( ".ptp" ) );
    QVERIFY2( QgsMtplTest::writePtpFixture(
                packagePath, 256, testCase.metadata, MTPL_STORAGE_PLAIN, {},
                { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, testCase.payload } }, error ),
              qPrintable( error ) );

    QgsMtpl::PackageDescriptor descriptor;
    QVERIFY( !QgsMtplPackageService::probePackage(
      packagePath, descriptor, error, {}, true, QgsMtpl::CredentialSource::None ) );
    QCOMPARE( descriptor.format, QgsMtpl::PackageFormat::Ptp );
    QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::Unsupported );
    QVERIFY2( error.contains( testCase.expectedMessage ), qPrintable( error ) );
    QVERIFY2( descriptor.readinessMessage.contains( testCase.expectedMessage ),
              qPrintable( descriptor.readinessMessage ) );
  }

  const QString emptyDeclaredRaster = mWorkspace.filePath( QStringLiteral( "empty-declared-raster.ptp" ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              emptyDeclaredRaster, 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
              MTPL_STORAGE_PLAIN, {}, { { 0, 0, 0, 0, 0 } }, {}, error ),
            qPrintable( error ) );
  const QgsMtpl::ProbeResult emptyResult = QgsMtplPackageService::probePath(
    emptyDeclaredRaster, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( emptyResult.ok, qPrintable( emptyResult.error ) );
  QCOMPARE( emptyResult.packages.constFirst().payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( emptyResult.packages.constFirst().readiness, QgsMtpl::ReadinessState::PlainReady );

  const QString emptyUnknown = mWorkspace.filePath( QStringLiteral( "empty-unknown.ptp" ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              emptyUnknown, 256, QByteArrayLiteral( "{}" ), MTPL_STORAGE_PLAIN, {},
              { { 8, 32, 63, 32, 63 } }, {}, error ),
            qPrintable( error ) );
  const QgsMtpl::ProbeResult emptyUnknownResult = QgsMtplPackageService::probePath(
    emptyUnknown, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( emptyUnknownResult.ok, qPrintable( emptyUnknownResult.error ) );
  QCOMPARE( emptyUnknownResult.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &emptyUnknownDescriptor = emptyUnknownResult.packages.constFirst();
  QCOMPARE( emptyUnknownDescriptor.format, QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( emptyUnknownDescriptor.payload, QgsMtpl::PayloadType::Unknown );
  QCOMPARE( emptyUnknownDescriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
  QVERIFY( emptyUnknownDescriptor.isReady() );
  QCOMPARE( emptyUnknownDescriptor.minimumZoom, -1 );
  QCOMPARE( emptyUnknownDescriptor.maximumZoom, -1 );
  QCOMPARE( emptyUnknownDescriptor.metadata.value( QStringLiteral( "presentTileCount" ) ).toULongLong(), 0ULL );
  QVERIFY( emptyUnknownDescriptor.readinessMessage.contains( QStringLiteral( "同一数据集" ) ) );
}

void TestMtplPackage::probeEncryptedEmptyPtp()
{
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) );
  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QString error;
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath, 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
              MTPL_STORAGE_ENCRYPTED, keys, { { 0, 0, 0, 0, 0 } }, {}, error ),
            qPrintable( error ) );

  const QgsMtpl::ProbeResult lockedResult = QgsMtplPackageService::probePath(
    packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( lockedResult.ok, qPrintable( lockedResult.error ) );
  QCOMPARE( lockedResult.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &locked = lockedResult.packages.constFirst();
  QCOMPARE( locked.format, QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( locked.encryption, QgsMtpl::EncryptionState::Locked );
  QCOMPARE( locked.readiness, QgsMtpl::ReadinessState::KeyRequired );
  QCOMPARE( locked.credentialSource, QgsMtpl::CredentialSource::None );
  QVERIFY( !locked.isReady() );
  QVERIFY( !locked.isCredentialedEmptyPtp() );

  const QgsMtpl::ProbeResult unlockedResult = QgsMtplPackageService::probePath(
    packagePath, keys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( unlockedResult.ok, qPrintable( unlockedResult.error ) );
  QCOMPARE( unlockedResult.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &unlocked = unlockedResult.packages.constFirst();
  QCOMPARE( unlocked.format, QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( unlocked.payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( unlocked.encryption, QgsMtpl::EncryptionState::Encrypted );
  QCOMPARE( unlocked.readiness, QgsMtpl::ReadinessState::UnverifiableEmpty );
  QCOMPARE( unlocked.credentialSource, QgsMtpl::CredentialSource::Explicit );
  QCOMPARE( unlocked.metadata.value( QStringLiteral( "presentTileCount" ) ).toULongLong(), 0ULL );
  QVERIFY( !unlocked.isReady() );
  QVERIFY( unlocked.isCredentialedEmptyPtp() );

  const QgsMtpl::TileDatasetBuildResult datasetResult = QgsMtpl::buildTileDataset(
    mWorkspace.path(), true, { unlocked } );
  QVERIFY2( datasetResult.ok(), qPrintable( datasetResult.errorString() ) );
  QCOMPARE( datasetResult.dataset.packages.size(), 1 );
  QVERIFY( datasetResult.dataset.packages.constFirst().descriptor.isCredentialedEmptyPtp() );
  QVERIFY2( datasetResult.dataset.isValid( &error ), qPrintable( error ) );
  keys.clear();
}

void TestMtplPackage::probePtpByMagic()
{
  QString error;
  const QByteArray image = encodedImage( QByteArrayLiteral( "png" ), 256, 256, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  const QString directoryPath = mWorkspace.filePath( QStringLiteral( "ptp-magic-directory" ) );
  QVERIFY( QDir().mkpath( directoryPath ) );
  const QString packagePath = QDir( directoryPath ).filePath( QStringLiteral( "ptp-magic-with-bin-suffix.bin" ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath, 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
              MTPL_STORAGE_PLAIN, {}, { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, image } }, error ),
            qPrintable( error ) );

  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    directoryPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QVERIFY( result.isDirectorySelection );
  QCOMPARE( result.packages.size(), 1 );
  QCOMPARE( result.ignoredFileCount, 0 );
  QCOMPARE( result.packages.constFirst().format, QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( result.packages.constFirst().payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( result.packages.constFirst().tileSize, 256 );
}

void TestMtplPackage::probeNonPtpTileContracts_data()
{
  QTest::addColumn<int>( "formatValue" );
  QTest::addColumn<QString>( "misleadingSuffix" );
  QTest::addColumn<int>( "payloadValue" );
  QTest::addColumn<int>( "tileSize" );
  QTest::addColumn<bool>( "addEmptyRange" );

  QTest::newRow( "empty-dtp-with-ptp-suffix" )
    << static_cast<int>( QgsMtpl::PackageFormat::Dtp )
    << QStringLiteral( "ptp" )
    << static_cast<int>( QgsMtpl::PayloadType::Elevation )
    << 33
    << false;
  QTest::newRow( "empty-range-vtp-with-dtp-suffix" )
    << static_cast<int>( QgsMtpl::PackageFormat::Vtp )
    << QStringLiteral( "dtp" )
    << static_cast<int>( QgsMtpl::PayloadType::VectorTile )
    << 256
    << true;
}

void TestMtplPackage::probeNonPtpTileContracts()
{
  QFETCH( int, formatValue );
  QFETCH( QString, misleadingSuffix );
  QFETCH( int, payloadValue );
  QFETCH( int, tileSize );
  QFETCH( bool, addEmptyRange );

  const QgsMtpl::PackageFormat format = static_cast<QgsMtpl::PackageFormat>( formatValue );
  const QByteArray metadata = QByteArrayLiteral(
    R"({"tile_file_ext":"png","mtpl_tile_matrix":{"crs":"EPSG:4326","scheme":"tms"}})" );
  const QString packagePath = mWorkspace.filePath(
    QStringLiteral( "non-ptp-empty-%1.%2" ).arg( formatValue ).arg( misleadingSuffix ) );
  QVERIFY( QgsMtpl::packageFormatFromPath( packagePath ) != format );

  QString error;
  QVERIFY2( writeEmptyNonPtpTileFixture(
              packagePath, format, static_cast<quint32>( tileSize ), metadata, addEmptyRange, error ),
            qPrintable( error ) );

  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QVERIFY( result.error.isEmpty() );
  QCOMPARE( result.packages.size(), 1 );
  QCOMPARE( result.readyPackageCount(), 1 );

  const QgsMtpl::PackageDescriptor &descriptor = result.packages.constFirst();
  QCOMPARE( descriptor.format, format );
  QCOMPARE( static_cast<int>( descriptor.payload ), payloadValue );
  QCOMPARE( descriptor.encryption, QgsMtpl::EncryptionState::Plain );
  QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
  QCOMPARE( descriptor.credentialSource, QgsMtpl::CredentialSource::None );
  QVERIFY( descriptor.isReady() );
  QVERIFY( descriptor.isSpatial() );
  QCOMPARE( descriptor.tileSize, tileSize );
  // The PTP-only matrix contract must not change legacy DTP/VTP metadata
  // interpretation.
  QCOMPARE( descriptor.crsAuthId, QStringLiteral( "EPSG:3857" ) );
  QCOMPARE( descriptor.scheme, QStringLiteral( "xyz" ) );
  QCOMPARE( descriptor.minimumZoom, -1 );
  QCOMPARE( descriptor.maximumZoom, -1 );
  QVERIFY( !descriptor.hasExtent );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "presentTileCount" ) ).toULongLong(), 0ULL );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "rangeCount" ) ).toULongLong(),
            addEmptyRange ? 1ULL : 0ULL );

  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "_mtplRangeBounds" ) ) );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "minimumZoom" ) ) );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "maximumZoom" ) ) );
}

void TestMtplPackage::probeSfpByMagicWithEmptyEntry()
{
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "sfp-magic-with-ptp-suffix.ptp" ) );
  QCOMPARE( QgsMtpl::packageFormatFromPath( packagePath ), QgsMtpl::PackageFormat::Ptp );

  QString error;
  QVERIFY2( QgsMtplTest::writeSfpFixture(
              packagePath,
              { { QStringLiteral( "empty.bin" ), QByteArray(), MTPL_STORAGE_PLAIN } },
              {}, error ),
            qPrintable( error ) );
  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QCOMPARE( result.readyPackageCount(), 1 );

  const QgsMtpl::PackageDescriptor &descriptor = result.packages.constFirst();
  QCOMPARE( descriptor.format, QgsMtpl::PackageFormat::Sfp );
  QCOMPARE( descriptor.payload, QgsMtpl::PayloadType::Files );
  QCOMPARE( descriptor.encryption, QgsMtpl::EncryptionState::Plain );
  QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
  QVERIFY( descriptor.isReady() );
  QVERIFY( !descriptor.isSpatial() );
  QCOMPARE( descriptor.minimumZoom, -1 );
  QCOMPARE( descriptor.maximumZoom, -1 );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "_mtplRangeBounds" ) ) );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "rangeCount" ) ).toULongLong(), 0ULL );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "entryCount" ) ).toULongLong(), 1ULL );
  QCOMPARE( descriptor.sfpEntries.size(), 1 );
  QCOMPARE( descriptor.sfpEntries.constFirst().path, QStringLiteral( "empty.bin" ) );
  QCOMPARE( descriptor.sfpEntries.constFirst().logicalSize, 0ULL );
  QVERIFY( !descriptor.sfpEntries.constFirst().encrypted );
}

void TestMtplPackage::probeSfpFixtures_data()
{
  QTest::addColumn<bool>( "encrypted" );
  QTest::newRow( "plain" ) << false;
  QTest::newRow( "encrypted" ) << true;
}

void TestMtplPackage::probeSfpFixtures()
{
  QFETCH( bool, encrypted );
  const mtpl_storage_mode_t mode = encrypted ? MTPL_STORAGE_ENCRYPTED : MTPL_STORAGE_PLAIN;
  const QList<QgsMtplTest::SfpFixtureEntry> entries = sfpEntries( mode );
  QgsMtpl::CryptoKeys keys = encrypted ? QgsMtplTest::fixtureKeys() : QgsMtpl::CryptoKeys();
  QString error;
  const QString packagePath = mWorkspace.filePath( encrypted ? QStringLiteral( "encrypted.sfp" )
                                                             : QStringLiteral( "plain.sfp" ) );
  QVERIFY2( QgsMtplTest::writeSfpFixture( packagePath, entries, keys, error ), qPrintable( error ) );

  if ( encrypted )
  {
    const QgsMtpl::ProbeResult locked = QgsMtplPackageService::probePath(
      packagePath, {}, true, QgsMtpl::CredentialSource::None );
    QVERIFY2( locked.ok, qPrintable( locked.error ) );
    QCOMPARE( locked.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRequired );
    QCOMPARE( locked.packages.constFirst().encryption, QgsMtpl::EncryptionState::Locked );
  }

  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    packagePath, keys, true, encrypted ? QgsMtpl::CredentialSource::Explicit : QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QCOMPARE( result.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &descriptor = result.packages.constFirst();
  QCOMPARE( descriptor.format, QgsMtpl::PackageFormat::Sfp );
  QCOMPARE( descriptor.payload, QgsMtpl::PayloadType::Files );
  QCOMPARE( descriptor.readiness, encrypted ? QgsMtpl::ReadinessState::KeyVerified
                                           : QgsMtpl::ReadinessState::PlainReady );
  QCOMPARE( descriptor.encryption, encrypted ? QgsMtpl::EncryptionState::Encrypted
                                             : QgsMtpl::EncryptionState::Plain );
  QVERIFY( descriptor.isReady() );
  QVERIFY( !descriptor.isSpatial() );
  QCOMPARE( descriptor.minimumZoom, -1 );
  QCOMPARE( descriptor.maximumZoom, -1 );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "_mtplRangeBounds" ) ) );
  QCOMPARE( descriptor.sfpEntries.size(), entries.size() );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "entryCount" ) ).toULongLong(),
            static_cast<qulonglong>( entries.size() ) );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "encryptedEntryCount" ) ).toULongLong(),
            encrypted ? static_cast<qulonglong>( entries.size() ) : 0ULL );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "plainEntryCount" ) ).toULongLong(),
            encrypted ? 0ULL : static_cast<qulonglong>( entries.size() ) );
  for ( const QgsMtplTest::SfpFixtureEntry &expected : entries )
  {
    const auto found = std::find_if( descriptor.sfpEntries.cbegin(), descriptor.sfpEntries.cend(),
                                     [&expected]( const QgsMtpl::SfpEntryDescriptor &entry )
    {
      return entry.path == expected.path;
    } );
    QVERIFY2( found != descriptor.sfpEntries.cend(), qPrintable( expected.path ) );
    QCOMPARE( found->logicalSize, static_cast<quint64>( expected.data.size() ) );
    QCOMPARE( found->encrypted, encrypted );
  }
}

void TestMtplPackage::probeVtpReadinessStates()
{
  QString error;
  const QString plainPath = mWorkspace.filePath( QStringLiteral( "readiness-plain.vtp" ) );
  const QString encryptedPath = mWorkspace.filePath( QStringLiteral( "readiness-encrypted.vtp" ) );
  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QgsMtpl::CryptoKeys wrongKeys = QgsMtplTest::differentFixtureKeys();
  QVERIFY2( QgsMtplTest::writeTileFixture( plainPath, QgsMtpl::PackageFormat::Vtp,
                                           MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writeTileFixture( encryptedPath, QgsMtpl::PackageFormat::Vtp,
                                           MTPL_STORAGE_ENCRYPTED, keys, error ), qPrintable( error ) );

  const QgsMtpl::ProbeResult plain = QgsMtplPackageService::probePath( plainPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( plain.ok, qPrintable( plain.error ) );
  QCOMPARE( plain.packages.constFirst().readiness, QgsMtpl::ReadinessState::PlainReady );
  const QgsMtpl::ProbeResult required = QgsMtplPackageService::probePath( encryptedPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( required.ok, qPrintable( required.error ) );
  QCOMPARE( required.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRequired );
  const QgsMtpl::ProbeResult verified = QgsMtplPackageService::probePath( encryptedPath, keys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( verified.ok, qPrintable( verified.error ) );
  QCOMPARE( verified.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyVerified );
  const QgsMtpl::ProbeResult rejected = QgsMtplPackageService::probePath( encryptedPath, wrongKeys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( rejected.ok, qPrintable( rejected.error ) );
  QCOMPARE( rejected.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRejectedOrCorrupt );
  QCOMPARE( rejected.packages.constFirst().credentialSource, QgsMtpl::CredentialSource::Explicit );
  QVERIFY( !rejected.packages.constFirst().isReady() );
}

void TestMtplPackage::probeMixedSfpWithMatchingAndWrongKeys()
{
  const QList<QgsMtplTest::SfpFixtureEntry> entries = {
    { QStringLiteral( "plain.txt" ), QByteArrayLiteral( "plain" ), MTPL_STORAGE_PLAIN },
    { QStringLiteral( "protected/加密.txt" ), QByteArrayLiteral( "encrypted" ), MTPL_STORAGE_ENCRYPTED },
  };
  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QgsMtpl::CryptoKeys wrongKeys = QgsMtplTest::differentFixtureKeys();
  QString error;
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "mixed.sfp" ) );
  QVERIFY2( QgsMtplTest::writeSfpFixture( packagePath, entries, keys, error ), qPrintable( error ) );

  const QgsMtpl::ProbeResult required = QgsMtplPackageService::probePath( packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( required.ok, qPrintable( required.error ) );
  QCOMPARE( required.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRequired );
  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath( packagePath, keys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  const QgsMtpl::PackageDescriptor &descriptor = result.packages.constFirst();
  QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::KeyVerified );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "plainEntryCount" ) ).toULongLong(), 1ULL );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "encryptedEntryCount" ) ).toULongLong(), 1ULL );
  QCOMPARE( descriptor.sfpEntries.size(), 2 );
  QVERIFY( std::any_of( descriptor.sfpEntries.cbegin(), descriptor.sfpEntries.cend(),
                        []( const QgsMtpl::SfpEntryDescriptor &entry ) { return !entry.encrypted; } ) );
  QVERIFY( std::any_of( descriptor.sfpEntries.cbegin(), descriptor.sfpEntries.cend(),
                        []( const QgsMtpl::SfpEntryDescriptor &entry ) { return entry.encrypted; } ) );
  const QgsMtpl::ProbeResult rejected = QgsMtplPackageService::probePath( packagePath, wrongKeys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( rejected.ok, qPrintable( rejected.error ) );
  QCOMPARE( rejected.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRejectedOrCorrupt );
}

void TestMtplPackage::probeEmptySfp()
{
  QString error;
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "empty.sfp" ) );
  QVERIFY2( QgsMtplTest::writeSfpFixture( packagePath, {}, {}, error ), qPrintable( error ) );
  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath( packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  const QgsMtpl::PackageDescriptor &descriptor = result.packages.constFirst();
  QCOMPARE( descriptor.format, QgsMtpl::PackageFormat::Sfp );
  QCOMPARE( descriptor.payload, QgsMtpl::PayloadType::Files );
  QCOMPARE( descriptor.encryption, QgsMtpl::EncryptionState::Plain );
  QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
  QVERIFY( descriptor.isReady() );
  QVERIFY( !descriptor.isSpatial() );
  QCOMPARE( descriptor.minimumZoom, -1 );
  QCOMPARE( descriptor.maximumZoom, -1 );
  QVERIFY( !descriptor.metadata.contains( QStringLiteral( "_mtplRangeBounds" ) ) );
  QVERIFY( descriptor.sfpEntries.isEmpty() );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "entryCount" ) ).toULongLong(), 0ULL );
}

void TestMtplPackage::probeUnsupportedAndUnreadable()
{
  const QString corruptPath = mWorkspace.filePath( QStringLiteral( "wrong-magic.ptp" ) );
  QFile corrupt( corruptPath );
  QVERIFY( corrupt.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  QCOMPARE( corrupt.write( QByteArrayLiteral( "NOT-MTPL" ) ), 8 );
  corrupt.close();

  const QgsMtpl::ProbeResult unsupported = QgsMtplPackageService::probePath(
    corruptPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY( unsupported.ok );
  QCOMPARE( unsupported.packages.size(), 1 );
  QCOMPARE( unsupported.packages.constFirst().format, QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( unsupported.packages.constFirst().readiness, QgsMtpl::ReadinessState::Unsupported );
  QVERIFY( !unsupported.packages.constFirst().isReady() );

  QgsMtpl::PackageDescriptor unreadable;
  QString error;
  const QString missingPath = mWorkspace.filePath( QStringLiteral( "missing.vtp" ) );
  QVERIFY( !QgsMtplPackageService::probePackage(
    missingPath, unreadable, error, {}, true, QgsMtpl::CredentialSource::None ) );
  QCOMPARE( unreadable.format, QgsMtpl::PackageFormat::Vtp );
  QCOMPARE( unreadable.readiness, QgsMtpl::ReadinessState::Unreadable );
  QVERIFY( !error.isEmpty() );
}

void TestMtplPackage::probeLongUnicodePath()
{
  const QString unicodeDirectory = mWorkspace.filePath( QStringLiteral( "unicode-目录-层级" ) );
  QVERIFY( QDir().mkpath( unicodeDirectory ) );
  const QString fileName = QStringLiteral( "这是一个用于验证名称列和完整路径处理的运行时矢量瓦片数据包-地图-ß-é.vtp" );
  const QString packagePath = QDir( unicodeDirectory ).filePath( fileName );
  QString error;
  QVERIFY2( QgsMtplTest::writeTileFixture( packagePath, QgsMtpl::PackageFormat::Vtp,
                                           MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QCOMPARE( result.packages.size(), 1 );
  QVERIFY( result.packages.constFirst().isReady() );
  QCOMPARE( QFileInfo( result.packages.constFirst().path ).fileName(), fileName );
  QVERIFY( result.packages.constFirst().path.contains( QStringLiteral( "unicode-目录-层级" ) ) );
}

void TestMtplPackage::probeLongPtpPathReadsTile()
{
  QString directoryPath = mWorkspace.filePath( QStringLiteral( "long-ptp 中文路径 with spaces" ) );
  const QString fileName = QStringLiteral( "0-7-0-0-0.ptp" );
  while ( QDir( directoryPath ).filePath( fileName ).size() <= 280 )
    directoryPath = QDir( directoryPath ).filePath( QStringLiteral( "多级目录 long path segment" ) );
  QVERIFY( QDir().mkpath( directoryPath ) );
  const QString packagePath = QFileInfo( QDir( directoryPath ).filePath( fileName ) ).absoluteFilePath();
  QVERIFY2( packagePath.size() > 260, qPrintable( packagePath ) );

  QString error;
  const QByteArray image = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 256, 256, 29, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath, 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
              MTPL_STORAGE_PLAIN, {}, { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, image } }, error ),
            qPrintable( error ) );

  for ( const bool directorySelection : { false, true } )
  {
    const QString selectedPath = directorySelection ? directoryPath : packagePath;
    const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
      selectedPath, {}, true, QgsMtpl::CredentialSource::None );
    QVERIFY2( probe.ok, qPrintable( probe.error ) );
    QCOMPARE( probe.packages.size(), 1 );
    QVERIFY( probe.packages.constFirst().isReady() );
    const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset(
      selectedPath, directorySelection, probe.packages );
    QVERIFY2( dataset.ok(), qPrintable( dataset.errorString() ) );
    QCOMPARE( dataset.dataset.packages.size(), 1 );
    const QString resolvedPath = dataset.dataset.packages.constFirst().descriptor.path;
    QCOMPARE( QFileInfo( resolvedPath ).absoluteFilePath(), packagePath );
    QVERIFY( resolvedPath.size() > 260 );

    const QByteArray pathUtf8 = resolvedPath.toUtf8();
    mtpl_ptp_reader_t *reader = nullptr;
    mtpl_status_t status = mtpl_ptp_reader_open( pathUtf8.constData(), nullptr, &reader );
    mtpl_buffer_t tile = {};
    const mtpl_tile_coordinate_t coordinate = { 0, 0, 0 };
    if ( status == MTPL_STATUS_OK )
      status = mtpl_ptp_reader_read_tile( reader, &coordinate, &tile );
    const QByteArray actual( reinterpret_cast<const char *>( tile.data ), static_cast<int>( tile.size ) );
    mtpl_buffer_release( &tile );
    if ( reader )
      mtpl_ptp_reader_close( reader );
    QCOMPARE( status, MTPL_STATUS_OK );
    QCOMPARE( actual, image );
    QVERIFY( !QImage::fromData( actual, "PNG" ).isNull() );
  }
}

void TestMtplPackage::probePlainDirectory()
{
  const QString directoryPath = mWorkspace.filePath( QStringLiteral( "plain-directory" ) );
  QVERIFY( QDir().mkpath( directoryPath ) );
  QString error;
  QVERIFY2( QgsMtplTest::writeTileFixture( QDir( directoryPath ).filePath( QStringLiteral( "plain.ptp" ) ),
                                           QgsMtpl::PackageFormat::Ptp, MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writeTileFixture( QDir( directoryPath ).filePath( QStringLiteral( "plain.dtp" ) ),
                                           QgsMtpl::PackageFormat::Dtp, MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writeTileFixture( QDir( directoryPath ).filePath( QStringLiteral( "plain.vtp" ) ),
                                           QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  const QList<QgsMtplTest::SfpFixtureEntry> entries = {
    { QStringLiteral( "nested/file.txt" ), QByteArrayLiteral( "directory" ), MTPL_STORAGE_PLAIN },
  };
  QVERIFY2( QgsMtplTest::writeSfpFixture( QDir( directoryPath ).filePath( QStringLiteral( "plain.sfp" ) ),
                                          entries, {}, error ), qPrintable( error ) );
  QFile noise( QDir( directoryPath ).filePath( QStringLiteral( "notes.txt" ) ) );
  QVERIFY( noise.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  QCOMPARE( noise.write( QByteArrayLiteral( "unrelated" ) ), 9 );
  noise.close();

  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    directoryPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QVERIFY( result.isDirectorySelection );
  QCOMPARE( result.packages.size(), 4 );
  QCOMPARE( result.readyPackageCount(), 4 );
  QCOMPARE( result.ignoredFileCount, 1 );
  QSet<QgsMtpl::PackageFormat> formats;
  for ( const QgsMtpl::PackageDescriptor &descriptor : result.packages )
  {
    QVERIFY( descriptor.isReady() );
    formats.insert( descriptor.format );
  }
  QCOMPARE( formats.size(), 4 );
  QVERIFY( formats.contains( QgsMtpl::PackageFormat::Ptp ) );
  QVERIFY( formats.contains( QgsMtpl::PackageFormat::Dtp ) );
  QVERIFY( formats.contains( QgsMtpl::PackageFormat::Vtp ) );
  QVERIFY( formats.contains( QgsMtpl::PackageFormat::Sfp ) );
}

void TestMtplPackage::probeDirectoryNonrecursively()
{
  const QString directoryPath = mWorkspace.filePath( QStringLiteral( "nonrecursive" ) );
  const QString childDirectory = QDir( directoryPath ).filePath( QStringLiteral( "child" ) );
  QVERIFY( QDir().mkpath( childDirectory ) );
  QString error;
  QVERIFY2( QgsMtplTest::writeTileFixture( QDir( childDirectory ).filePath( QStringLiteral( "nested.vtp" ) ),
                                           QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    directoryPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY( !result.ok );
  QVERIFY( result.isDirectorySelection );
  QVERIFY( result.packages.isEmpty() );
  QVERIFY( result.error.contains( QStringLiteral( "不会递归" ) ) );
  QVERIFY( result.error.contains( QStringLiteral( "叶子文件夹" ) ) );
}

void TestMtplPackage::probeExplicitKeyDirectory()
{
  const QString directoryPath = mWorkspace.filePath( QStringLiteral( "explicit-key-directory" ) );
  QVERIFY( QDir().mkpath( directoryPath ) );
  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QString error;
  QVERIFY2( QgsMtplTest::writeTileFixture( QDir( directoryPath ).filePath( QStringLiteral( "protected.vtp" ) ),
                                           QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_ENCRYPTED, keys, error ), qPrintable( error ) );
  const QList<QgsMtplTest::SfpFixtureEntry> entries = {
    { QStringLiteral( "protected.bin" ), QByteArrayLiteral( "protected" ), MTPL_STORAGE_ENCRYPTED },
  };
  QVERIFY2( QgsMtplTest::writeSfpFixture( QDir( directoryPath ).filePath( QStringLiteral( "protected.sfp" ) ),
                                          entries, keys, error ), qPrintable( error ) );
  QFile noise( QDir( directoryPath ).filePath( QStringLiteral( "unrelated.bin" ) ) );
  QVERIFY( noise.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  QCOMPARE( noise.write( QByteArrayLiteral( "not a package" ) ), 13 );
  noise.close();

  const QgsMtpl::ProbeResult result = QgsMtplPackageService::probePath(
    directoryPath, keys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QVERIFY( result.isDirectorySelection );
  QCOMPARE( result.packages.size(), 2 );
  QCOMPARE( result.readyPackageCount(), 2 );
  QCOMPARE( result.ignoredFileCount, 1 );
  for ( const QgsMtpl::PackageDescriptor &descriptor : result.packages )
  {
    QVERIFY( descriptor.isReady() );
    QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::KeyVerified );
    QCOMPARE( descriptor.credentialSource, QgsMtpl::CredentialSource::Explicit );
  }
}

void TestMtplPackage::probeTaskLifecycle_data()
{
  QTest::addColumn<QString>( "kind" );
  QTest::newRow( "ptp" ) << QStringLiteral( "ptp" );
  QTest::newRow( "vtp" ) << QStringLiteral( "vtp" );
  QTest::newRow( "sfp" ) << QStringLiteral( "sfp" );
}

void TestMtplPackage::probeTaskLifecycle()
{
  QFETCH( QString, kind );
  QString error;
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "task-complete-%1.%1" ).arg( kind ) );
  if ( kind == QLatin1String( "vtp" ) || kind == QLatin1String( "ptp" ) )
  {
    QVERIFY2( QgsMtplTest::writeTileFixture( packagePath,
                                             kind == QLatin1String( "ptp" ) ? QgsMtpl::PackageFormat::Ptp : QgsMtpl::PackageFormat::Vtp,
                                             MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  }
  else
  {
    const QList<QgsMtplTest::SfpFixtureEntry> entries = {
      { QStringLiteral( "nested/task.txt" ), QByteArrayLiteral( "task" ), MTPL_STORAGE_PLAIN },
    };
    QVERIFY2( QgsMtplTest::writeSfpFixture( packagePath, entries, {}, error ), qPrintable( error ) );
  }

  QgsTaskManager manager;
  auto *task = new QgsMtplProbeTask( packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QSignalSpy completedSpy( task, &QgsTask::taskCompleted );
  QSignalSpy terminatedSpy( task, &QgsTask::taskTerminated );
  QSignalSpy progressSpy( task, &QgsTask::progressChanged );
  QgsMtpl::ProbeResult taskResult;
  connect( task, &QgsTask::taskCompleted, this, [task, &taskResult]
  {
    taskResult = task->result();
  } );

  QVERIFY( manager.addTask( task ) > 0 );
  QTRY_COMPARE_WITH_TIMEOUT( completedSpy.count(), 1, 10000 );
  QCOMPARE( terminatedSpy.count(), 0 );
  QTRY_COMPARE_WITH_TIMEOUT( manager.countActiveTasks(), 0, 10000 );
  QVERIFY2( taskResult.ok, qPrintable( taskResult.error ) );
  QVERIFY( !taskResult.canceled );
  QCOMPARE( taskResult.packages.size(), 1 );
  QCOMPARE( taskResult.packages.constFirst().format,
            kind == QLatin1String( "ptp" ) ? QgsMtpl::PackageFormat::Ptp :
            kind == QLatin1String( "vtp" ) ? QgsMtpl::PackageFormat::Vtp : QgsMtpl::PackageFormat::Sfp );
  QVERIFY( taskResult.packages.constFirst().isReady() );
  QVERIFY( !progressSpy.isEmpty() );
  double previousProgress = 0.0;
  for ( int index = 0; index < progressSpy.size(); ++index )
  {
    const double progress = progressSpy.at( index ).constFirst().toDouble();
    QVERIFY( progress >= previousProgress );
    QVERIFY( progress >= 0.0 && progress <= 100.0 );
    previousProgress = progress;
  }
  QCOMPARE( previousProgress, 100.0 );
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplPackage::probeTaskCancellation_data()
{
  QTest::addColumn<QString>( "kind" );
  QTest::newRow( "ptp" ) << QStringLiteral( "ptp" );
  QTest::newRow( "vtp" ) << QStringLiteral( "vtp" );
  QTest::newRow( "sfp" ) << QStringLiteral( "sfp" );
}

void TestMtplPackage::probeTaskCancellation()
{
  QFETCH( QString, kind );
  QString error;
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "task-cancel-%1.%1" ).arg( kind ) );
  if ( kind == QLatin1String( "vtp" ) || kind == QLatin1String( "ptp" ) )
  {
    QVERIFY2( QgsMtplTest::writeTileFixture( packagePath,
                                             kind == QLatin1String( "ptp" ) ? QgsMtpl::PackageFormat::Ptp : QgsMtpl::PackageFormat::Vtp,
                                             MTPL_STORAGE_PLAIN, {}, error ), qPrintable( error ) );
  }
  else
  {
    const QList<QgsMtplTest::SfpFixtureEntry> entries = {
      { QStringLiteral( "first.bin" ), QByteArray( 1024, 'a' ), MTPL_STORAGE_PLAIN },
      { QStringLiteral( "second.bin" ), QByteArray( 1024, 'b' ), MTPL_STORAGE_PLAIN },
    };
    QVERIFY2( QgsMtplTest::writeSfpFixture( packagePath, entries, {}, error ), qPrintable( error ) );
  }

  QgsTaskManager manager;
  auto *task = new QgsMtplProbeTask( packagePath, {}, true, QgsMtpl::CredentialSource::None );
  QSignalSpy completedSpy( task, &QgsTask::taskCompleted );
  QSignalSpy terminatedSpy( task, &QgsTask::taskTerminated );
  QSignalSpy progressSpy( task, &QgsTask::progressChanged );
  QgsMtpl::ProbeResult taskResult;
  connect( task, &QgsTask::progressChanged, task, [task]( double progress )
  {
    if ( progress > 0.0 )
      task->cancel();
  }, Qt::DirectConnection );
  connect( task, &QgsTask::taskTerminated, this, [task, &taskResult]
  {
    taskResult = task->result();
  } );

  QVERIFY( manager.addTask( task ) > 0 );
  QTRY_COMPARE_WITH_TIMEOUT( terminatedSpy.count(), 1, 10000 );
  QCOMPARE( completedSpy.count(), 0 );
  QTRY_COMPARE_WITH_TIMEOUT( manager.countActiveTasks(), 0, 10000 );
  QVERIFY( !taskResult.ok );
  QVERIFY( taskResult.canceled );
  QVERIFY( !taskResult.error.isEmpty() );
  QVERIFY( !progressSpy.isEmpty() );
  for ( int index = 0; index < progressSpy.size(); ++index )
  {
    const double progress = progressSpy.at( index ).constFirst().toDouble();
    QVERIFY( progress >= 0.0 && progress < 100.0 );
  }
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplPackage::probePtpDirectoryAfterCanceledTask()
{
  const QString path = mWorkspace.filePath( QStringLiteral( "ptp-cancel-and-reprobe" ) );
  QString error;
  const QByteArray image = QgsMtplTest::rasterTileImage( "png", 256, 256, 1, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  for ( quint32 x = 0; x < 9; ++x )
  {
    const quint32 tileX = ( x % 3 ) * 32;
    const quint32 tileY = ( x / 3 ) * 32;
    const QString packagePath = QDir( path ).filePath( QStringLiteral( "8-11-3-%1-%2.ptp" ).arg( x % 3 ).arg( x / 3 ) );
    QVERIFY2( QgsMtplTest::writePtpFixture( packagePath, 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
                                           MTPL_STORAGE_PLAIN, {}, { { 8, tileX, tileX, tileY, tileY } },
                                           { { 8, tileX, tileY, image } }, error ), qPrintable( error ) );
  }

  QgsTaskManager manager;
  auto *canceledTask = new QgsMtplProbeTask( path, {}, true, QgsMtpl::CredentialSource::None );
  QSignalSpy terminatedSpy( canceledTask, &QgsTask::taskTerminated );
  QSignalSpy canceledCompletedSpy( canceledTask, &QgsTask::taskCompleted );
  QgsMtpl::ProbeResult canceledResult;
  connect( canceledTask, &QgsTask::progressChanged, canceledTask, [canceledTask]( double progress )
  {
    if ( progress > 0.0 )
      canceledTask->cancel();
  }, Qt::DirectConnection );
  connect( canceledTask, &QgsTask::taskTerminated, this, [canceledTask, &canceledResult]
  {
    canceledResult = canceledTask->result();
  } );
  QVERIFY( manager.addTask( canceledTask ) > 0 );
  QTRY_COMPARE_WITH_TIMEOUT( terminatedSpy.count(), 1, 10000 );
  QCOMPARE( canceledCompletedSpy.count(), 0 );
  QVERIFY( canceledResult.canceled );
  QVERIFY( !canceledResult.ok );
  QTRY_COMPARE_WITH_TIMEOUT( manager.countActiveTasks(), 0, 10000 );

  auto *retryTask = new QgsMtplProbeTask( path, {}, true, QgsMtpl::CredentialSource::None );
  QSignalSpy completedSpy( retryTask, &QgsTask::taskCompleted );
  QSignalSpy retryTerminatedSpy( retryTask, &QgsTask::taskTerminated );
  QgsMtpl::ProbeResult retryResult;
  connect( retryTask, &QgsTask::taskCompleted, this, [retryTask, &retryResult]
  {
    retryResult = retryTask->result();
  } );
  QVERIFY( manager.addTask( retryTask ) > 0 );
  QTRY_COMPARE_WITH_TIMEOUT( completedSpy.count(), 1, 10000 );
  QCOMPARE( retryTerminatedSpy.count(), 0 );
  QTRY_COMPARE_WITH_TIMEOUT( manager.countActiveTasks(), 0, 10000 );
  QVERIFY2( retryResult.ok, qPrintable( retryResult.error ) );
  QVERIFY( !retryResult.canceled );
  QCOMPARE( retryResult.packages.size(), 9 );
  QCOMPARE( retryResult.readyPackageCount(), 9 );
  const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset( path, true, retryResult.packages );
  QVERIFY2( dataset.ok(), qPrintable( dataset.errorString() ) );
  QCOMPARE( dataset.dataset.packages.size(), 9 );
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

QGSTEST_MAIN( TestMtplPackage )
#include "test_mtpl_package.moc"
