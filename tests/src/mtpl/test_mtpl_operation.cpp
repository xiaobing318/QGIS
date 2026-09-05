/***************************************************************************
  test_mtpl_operation.cpp
  -----------------------
  Atomicity, cancellation, and round-trip tests for MTPL package operations.
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

#include "qgsmtplpackageoperation.h"
#include "qgsmtplpackageservice.h"
#include "qgsmtpltileset.h"
#include "qgstest.h"

#include <mtpl/sfp.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVector>

namespace
{
  QByteArray fileSha256( const QString &path )
  {
    QFile file( path );
    if ( !file.open( QIODevice::ReadOnly ) )
      return QByteArray();

    QCryptographicHash hash( QCryptographicHash::Sha256 );
    while ( !file.atEnd() )
      hash.addData( file.read( 1024 * 1024 ) );
    return hash.result();
  }

  bool writeSourceTile( const QString &root, const QString &relativePath, const QByteArray &bytes )
  {
    const QString path = QDir( root ).filePath( relativePath );
    if ( !QDir().mkpath( QFileInfo( path ).absolutePath() ) )
      return false;
    QFile file( path );
    return file.open( QIODevice::WriteOnly | QIODevice::NewOnly ) && file.write( bytes ) == bytes.size();
  }
}

class TestMtplOperation : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void rejectsIncompleteRequest();
    void honorsEarlyCancellation();
    void latchesTransientCancellation();
    void cancelsAfterStagingWork();
    void cancelsSfpWriteBetweenEntries();
    void cancelsSfpVerificationBetweenChunks();
    void honorsSkipReasonWithoutWriting();
    void protectsExistingOutput_data();
    void protectsExistingOutput();
    void failureProgressIsMonotonic();
    void batchPartiallySucceeds();
    void writesVerifiedSidecar_data();
    void writesVerifiedSidecar();
    void createsAllFormats();
    void createsLoadablePtp_data();
    void createsLoadablePtp();
    void rejectsInvalidPtp_data();
    void rejectsInvalidPtp();
    void cancelsPtpCreation_data();
    void cancelsPtpCreation();
    void rejectsEncryptedEmptySfp();
    void rekeysMixedSfp();
    void packageTaskLifecycle_data();
    void packageTaskLifecycle();
    void encryptDecryptRoundTrip_data();
    void encryptDecryptRoundTrip();

  private:
    QgsMtplTest::ArtifactWorkspace mWorkspace;
};

void TestMtplOperation::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();

  QString error;
  if ( !mWorkspace.initialize( QStringLiteral( "operation" ), error ) )
    QFAIL( qPrintable( error ) );
}

void TestMtplOperation::cleanupTestCase()
{
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
  QgsApplication::exitQgis();
}

void TestMtplOperation::rejectsIncompleteRequest()
{
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.outputPath = mWorkspace.filePath( QStringLiteral( "must-not-exist.vtp" ) );

  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY( !result.ok );
  QVERIFY( !result.canceled );
  QVERIFY( !result.error.isEmpty() );
  QVERIFY( result.outputPaths.isEmpty() );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
}

void TestMtplOperation::honorsEarlyCancellation()
{
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::CreateSfpPackage;
  request.sourcePath = QStringLiteral( "source-is-never-read" );
  request.outputPath = mWorkspace.filePath( QStringLiteral( "canceled.sfp" ) );

  QVector<double> progressValues;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute(
    request,
    [] { return true; },
    [&progressValues]( double progress ) { progressValues.append( progress ); } );

  QVERIFY( !result.ok );
  QVERIFY( result.canceled );
  QVERIFY( !result.error.isEmpty() );
  QVERIFY( result.outputPaths.isEmpty() );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
  QCOMPARE( progressValues, QVector<double>( { 0.0 } ) );
}

void TestMtplOperation::latchesTransientCancellation()
{
  const QString sourceDirectory = mWorkspace.filePath( QStringLiteral( "transient-cancel-source" ) );
  QVERIFY( QDir().mkpath( sourceDirectory ) );
  QFile sourceFile( QDir( sourceDirectory ).filePath( QStringLiteral( "payload.txt" ) ) );
  QVERIFY( sourceFile.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  QCOMPARE( sourceFile.write( QByteArrayLiteral( "cancel-me" ) ), 9 );
  sourceFile.close();

  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::CreateSfpPackage;
  request.sourcePath = sourceDirectory;
  request.outputPath = mWorkspace.filePath( QStringLiteral( "transient-cancel.sfp" ) );

  int cancelChecks = 0;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute(
    request, [&cancelChecks] { return ++cancelChecks == 2; } );

  QVERIFY( !result.ok );
  QVERIFY( result.canceled );
  QVERIFY( cancelChecks >= 2 );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
}

void TestMtplOperation::cancelsAfterStagingWork()
{
  const QString sourcePath = mWorkspace.filePath( QStringLiteral( "cancel-after-write-source.vtp" ) );
  QString fixtureError;
  QVERIFY2( QgsMtplTest::writeTileFixture(
              sourcePath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, fixtureError ),
            qPrintable( fixtureError ) );

  QgsMtpl::ConversionInput input;
  input.path = sourcePath;
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.inputs.append( input );
  request.outputPath = mWorkspace.filePath( QStringLiteral( "cancel-after-write.vtp" ) );
  request.encryptOutput = true;
  request.generateOutputKeys = true;
  request.writeSidecar = false;

  bool cancelRequested = false;
  bool nonzeroProgressObserved = false;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute(
    request,
    [&cancelRequested] { return cancelRequested; },
    [&cancelRequested, &nonzeroProgressObserved]( double progress )
    {
      if ( progress > 0.0 )
      {
        nonzeroProgressObserved = true;
        cancelRequested = true;
      }
    } );

  QVERIFY( nonzeroProgressObserved );
  QVERIFY( !result.ok );
  QVERIFY( result.canceled );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
  const QStringList stagingFiles = QDir( mWorkspace.path() ).entryList(
    QStringList() << QStringLiteral( ".cancel-after-write.vtp.mtpl-staging-*" ), QDir::Files | QDir::Hidden );
  QVERIFY( stagingFiles.isEmpty() );
}

void TestMtplOperation::cancelsSfpWriteBetweenEntries()
{
  const QString sourceDirectory = mWorkspace.filePath( QStringLiteral( "cancel-sfp-source" ) );
  QVERIFY( QDir().mkpath( sourceDirectory ) );
  for ( int index = 0; index < 3; ++index )
  {
    QFile sourceFile( QDir( sourceDirectory ).filePath( QStringLiteral( "entry-%1.bin" ).arg( index ) ) );
    QVERIFY( sourceFile.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
    const QByteArray payload( 128 * 1024, static_cast<char>( 'a' + index ) );
    QCOMPARE( sourceFile.write( payload ), static_cast<qint64>( payload.size() ) );
  }

  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::CreateSfpPackage;
  request.sourcePath = sourceDirectory;
  request.outputPath = mWorkspace.filePath( QStringLiteral( "create-sfp-canceled.sfp" ) );
  request.encryptOutput = false;
  request.writeSidecar = false;

  bool cancelRequested = false;
  bool entryProgressObserved = false;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute(
    request,
    [&cancelRequested] { return cancelRequested; },
    [&cancelRequested, &entryProgressObserved]( double progress )
    {
      if ( progress > 0.0 )
      {
        entryProgressObserved = true;
        cancelRequested = true;
      }
    } );

  QVERIFY( entryProgressObserved );
  QVERIFY( !result.ok );
  QVERIFY( result.canceled );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
  const QStringList stagingFiles = QDir( mWorkspace.path() ).entryList(
    QStringList() << QStringLiteral( ".create-sfp-canceled.sfp.mtpl-staging-*" )
                  << QStringLiteral( "*.mtpl-tmp-*" ),
    QDir::Files | QDir::Hidden );
  QVERIFY( stagingFiles.isEmpty() );
}

void TestMtplOperation::cancelsSfpVerificationBetweenChunks()
{
  const QString sourceDirectory = mWorkspace.filePath( QStringLiteral( "cancel-sfp-verification-source" ) );
  QVERIFY( QDir().mkpath( sourceDirectory ) );
  QFile sourceFile( QDir( sourceDirectory ).filePath( QStringLiteral( "large-entry.bin" ) ) );
  QVERIFY( sourceFile.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  const QByteArray payload( 3 * 1024 * 1024, 'v' );
  QCOMPARE( sourceFile.write( payload ), static_cast<qint64>( payload.size() ) );
  sourceFile.close();

  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::CreateSfpPackage;
  request.sourcePath = sourceDirectory;
  request.outputPath = mWorkspace.filePath( QStringLiteral( "create-sfp-verification-canceled.sfp" ) );
  request.encryptOutput = false;
  request.writeSidecar = false;

  bool cancelRequested = false;
  bool verificationProgressObserved = false;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute(
    request,
    [&cancelRequested] { return cancelRequested; },
    [&cancelRequested, &verificationProgressObserved]( double progress )
    {
      if ( progress > 80.0 )
      {
        verificationProgressObserved = true;
        cancelRequested = true;
      }
    } );

  QVERIFY( verificationProgressObserved );
  QVERIFY( !result.ok );
  QVERIFY( result.canceled );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
  const QStringList stagingFiles = QDir( mWorkspace.path() ).entryList(
    QStringList() << QStringLiteral( ".create-sfp-verification-canceled.sfp.mtpl-staging-*" )
                  << QStringLiteral( "*.mtpl-tmp-*" ),
    QDir::Files | QDir::Hidden );
  QVERIFY( stagingFiles.isEmpty() );
}

void TestMtplOperation::honorsSkipReasonWithoutWriting()
{
  QgsMtpl::ConversionInput input;
  input.path = QStringLiteral( "source-is-never-read.vtp" );
  input.skipReason = QStringLiteral( "test-requested-skip" );

  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.inputs.append( input );
  request.outputPath = mWorkspace.filePath( QStringLiteral( "skipped.vtp" ) );

  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY( !result.ok );
  QVERIFY( !result.canceled );
  QCOMPARE( result.items.size(), 1 );
  QCOMPARE( result.items.constFirst().status, QgsMtpl::PackageOperationItemStatus::Skipped );
  QCOMPARE( result.error, input.skipReason );
  QVERIFY( result.outputPaths.isEmpty() );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
}

void TestMtplOperation::protectsExistingOutput_data()
{
  QTest::addColumn<bool>( "sfp" );
  QTest::addColumn<QString>( "suffix" );
  QTest::newRow( "vtp" ) << false << QStringLiteral( "vtp" );
  QTest::newRow( "ptp" ) << false << QStringLiteral( "ptp" );
  QTest::newRow( "sfp" ) << true << QStringLiteral( "sfp" );
}

void TestMtplOperation::protectsExistingOutput()
{
  QFETCH( bool, sfp );
  QFETCH( QString, suffix );
  const QString sentinelPath = mWorkspace.filePath(
    QStringLiteral( "existing-%1.%1" ).arg( suffix ) );
  const QByteArray sentinelContents = QByteArrayLiteral( "existing-output-must-not-be-overwritten" );
  QFile sentinel( sentinelPath );
  QVERIFY( sentinel.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  QCOMPARE( sentinel.write( sentinelContents ), sentinelContents.size() );
  sentinel.close();

  QgsMtpl::PackageOperationRequest request;
  request.type = sfp ? QgsMtpl::PackageOperationType::CreateSfpPackage
                     : QgsMtpl::PackageOperationType::CreateTilePackage;
  request.sourcePath = QStringLiteral( "source-must-not-be-read" );
  request.outputPath = sentinelPath;
  request.outputFormat = QgsMtpl::packageFormatFromPath( sentinelPath );

  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY( !result.ok );
  QVERIFY( !result.canceled );
  QVERIFY( result.outputPaths.isEmpty() );
  QCOMPARE( fileSha256( sentinelPath ), QCryptographicHash::hash( sentinelContents, QCryptographicHash::Sha256 ) );
}

void TestMtplOperation::failureProgressIsMonotonic()
{
  const QString outputDirectory = mWorkspace.filePath( QStringLiteral( "failed-batch" ) );
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.outputDirectory = outputDirectory;
  request.inputs = {
    { mWorkspace.filePath( QStringLiteral( "missing-a.vtp" ) ), {}, {} },
    { mWorkspace.filePath( QStringLiteral( "missing-b.vtp" ) ), {}, {} }
  };

  QVector<double> progressValues;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute(
    request, {}, [&progressValues]( double progress ) { progressValues.append( progress ); } );

  QVERIFY( !result.ok );
  QVERIFY( !result.canceled );
  QVERIFY( progressValues.size() >= 3 );
  QCOMPARE( progressValues.constFirst(), 0.0 );
  for ( qsizetype index = 1; index < progressValues.size(); ++index )
    QVERIFY( progressValues.at( index ) >= progressValues.at( index - 1 ) );
}

void TestMtplOperation::batchPartiallySucceeds()
{
  const QString sourcePath = mWorkspace.filePath( QStringLiteral( "partial-batch-source.vtp" ) );
  QString fixtureError;
  QVERIFY2( QgsMtplTest::writeTileFixture(
              sourcePath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, fixtureError ),
            qPrintable( fixtureError ) );
  const QByteArray sourceHash = fileSha256( sourcePath );
  QVERIFY( !sourceHash.isEmpty() );
  const QString outputDirectory = mWorkspace.filePath( QStringLiteral( "partial-batch-output" ) );

  QgsMtpl::ConversionInput validInput;
  validInput.path = sourcePath;
  QgsMtpl::ConversionInput missingInput;
  missingInput.path = mWorkspace.filePath( QStringLiteral( "missing-partial.vtp" ) );
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.inputs = { validInput, missingInput };
  request.outputDirectory = outputDirectory;
  request.encryptOutput = false;
  request.writeSidecar = false;

  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QVERIFY( !result.canceled );
  QCOMPARE( result.items.size(), 2 );
  QCOMPARE( result.outputPaths.size(), 1 );
  QCOMPARE( result.items.at( 0 ).status, QgsMtpl::PackageOperationItemStatus::Succeeded );
  QCOMPARE( result.items.at( 1 ).status, QgsMtpl::PackageOperationItemStatus::Skipped );
  QVERIFY( QFileInfo::exists( result.outputPaths.constFirst() ) );
  QCOMPARE( fileSha256( sourcePath ), sourceHash );
  const QStringList stagingFiles = QDir( outputDirectory ).entryList(
    QStringList() << QStringLiteral( ".*.mtpl-staging-*" ), QDir::Files | QDir::Hidden );
  QVERIFY( stagingFiles.isEmpty() );
}

void TestMtplOperation::writesVerifiedSidecar_data()
{
  QTest::addColumn<bool>( "sfp" );
  QTest::addColumn<QString>( "suffix" );
  QTest::newRow( "vtp" ) << false << QStringLiteral( "vtp" );
  QTest::newRow( "ptp" ) << false << QStringLiteral( "ptp" );
  QTest::newRow( "sfp" ) << true << QStringLiteral( "sfp" );
}

void TestMtplOperation::writesVerifiedSidecar()
{
  QFETCH( bool, sfp );
  QFETCH( QString, suffix );

  QTemporaryDir secretWorkspace;
  QVERIFY( secretWorkspace.isValid() );
  const QString sourcePath = QDir( secretWorkspace.path() ).filePath(
    QStringLiteral( "sidecar-source.%1" ).arg( suffix ) );
  QString fixtureError;
  if ( sfp )
  {
    const QList<QgsMtplTest::SfpFixtureEntry> entries = {
      { QStringLiteral( "plain/readme.txt" ), QByteArrayLiteral( "portable sidecar fixture" ), MTPL_STORAGE_PLAIN }
    };
    QVERIFY2( QgsMtplTest::writeSfpFixture( sourcePath, entries, {}, fixtureError ), qPrintable( fixtureError ) );
  }
  else
  {
    QVERIFY2( QgsMtplTest::writeTileFixture(
                sourcePath, QgsMtpl::packageFormatFromPath( sourcePath ), MTPL_STORAGE_PLAIN, {}, fixtureError ),
              qPrintable( fixtureError ) );
  }

  QgsMtpl::ConversionInput input;
  input.path = sourcePath;
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.inputs.append( input );
  request.outputPath = QDir( secretWorkspace.path() ).filePath(
    QStringLiteral( "sidecar-encrypted.%1" ).arg( suffix ) );
  request.encryptOutput = true;
  request.generateOutputKeys = true;
  request.writeSidecar = true;

  QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QVERIFY( QFileInfo::exists( request.outputPath ) );
  QVERIFY( !result.sidecarPath.isEmpty() );
  QVERIFY( QFileInfo( result.sidecarPath ).isFile() );
  QVERIFY( result.outputKeys.isValid() );
  QgsMtpl::KeySidecar discoveredSidecar;
  QString discoveredSidecarPath;
  QString sidecarError;
  QVERIFY2( QgsMtpl::KeySidecarStore::discover(
              request.outputPath, discoveredSidecar, discoveredSidecarPath, sidecarError ),
            qPrintable( sidecarError ) );
  QCOMPARE( QFileInfo( discoveredSidecarPath ).absoluteFilePath(), QFileInfo( result.sidecarPath ).absoluteFilePath() );
  QVERIFY( discoveredSidecar.keys.isValid() );
  QCOMPARE( discoveredSidecar.packages.size(), 1 );
  const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
    request.outputPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( probe.ok, qPrintable( probe.error ) );
  QCOMPARE( probe.packages.size(), 1 );
  QVERIFY( probe.packages.constFirst().isReady() );
  QCOMPARE( probe.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyVerified );
  QCOMPARE( probe.packages.constFirst().credentialSource, QgsMtpl::CredentialSource::Sidecar );
  QCOMPARE( QFileInfo( probe.packages.constFirst().sidecarPath ).absoluteFilePath(),
            QFileInfo( result.sidecarPath ).absoluteFilePath() );
  if ( suffix == QLatin1String( "ptp" ) )
  {
    const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset( request.outputPath, false, probe.packages );
    QVERIFY2( dataset.ok(), qPrintable( dataset.errorString() ) );
  }
  discoveredSidecar.keys.clear();
  result.outputKeys.clear();
}

void TestMtplOperation::createsAllFormats()
{
  const QString tileSourceRoot = mWorkspace.filePath( QStringLiteral( "tile-source" ) );
  const QString tileLeaf = QDir( tileSourceRoot ).filePath( QStringLiteral( "0/0" ) );
  QVERIFY( QDir().mkpath( tileLeaf ) );
  QFile tileFile( QDir( tileLeaf ).filePath( QStringLiteral( "0.bin" ) ) );
  QVERIFY( tileFile.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  QImage tileImage( 256, 256, QImage::Format_ARGB32 );
  tileImage.fill( qRgb( 36, 112, 214 ) );
  QByteArray tileBytes;
  QBuffer tileBuffer( &tileBytes );
  QVERIFY( tileBuffer.open( QIODevice::WriteOnly ) );
  QVERIFY( tileImage.save( &tileBuffer, "PNG" ) );
  QCOMPARE( tileFile.write( tileBytes ), static_cast<qint64>( tileBytes.size() ) );
  tileFile.close();

  const QList<QPair<QgsMtpl::PackageFormat, QString>> tileFormats = {
    { QgsMtpl::PackageFormat::Ptp, QStringLiteral( "ptp" ) },
    { QgsMtpl::PackageFormat::Dtp, QStringLiteral( "dtp" ) },
    { QgsMtpl::PackageFormat::Vtp, QStringLiteral( "vtp" ) }
  };
  for ( const auto &format : tileFormats )
  {
    for ( const bool encrypted : { false, true } )
    {
      QgsMtpl::PackageOperationRequest request;
      request.type = QgsMtpl::PackageOperationType::CreateTilePackage;
      request.sourcePath = tileSourceRoot;
      request.outputPath = mWorkspace.filePath(
        QStringLiteral( "created-%1.%2" ).arg( encrypted ? QStringLiteral( "encrypted" ) : QStringLiteral( "plain" ), format.second ) );
      request.outputFormat = format.first;
      request.tileSize = format.first == QgsMtpl::PackageFormat::Dtp ? 33 : 256;
      request.metadata = QByteArrayLiteral( "{\"source\":\"qgis-mtpl-test\"}" );
      request.encryptOutput = encrypted;
      request.generateOutputKeys = encrypted;
      request.writeSidecar = false;

      QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
      QVERIFY2( result.ok, qPrintable( result.error ) );
      QVERIFY( QFileInfo::exists( request.outputPath ) );
      QgsMtpl::CryptoKeys probeKeys = encrypted ? result.outputKeys.cryptoKeys() : QgsMtpl::CryptoKeys();
      const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
        request.outputPath, probeKeys, true,
        encrypted ? QgsMtpl::CredentialSource::Explicit : QgsMtpl::CredentialSource::None );
      QVERIFY2( probe.ok, qPrintable( probe.error ) );
      QCOMPARE( probe.packages.constFirst().format, format.first );
      QVERIFY( probe.packages.constFirst().isReady() );
      if ( format.first == QgsMtpl::PackageFormat::Ptp )
      {
        const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset( request.outputPath, false, probe.packages );
        QVERIFY2( dataset.ok(), qPrintable( dataset.errorString() ) );
      }
      probeKeys.clear();
      result.outputKeys.clear();
    }
  }

  const QString sfpSource = mWorkspace.filePath( QStringLiteral( "sfp-source" ) );
  QVERIFY( QDir().mkpath( sfpSource ) );
  QFile textFile( QDir( sfpSource ).filePath( QStringLiteral( "地图-Delta.txt" ) ) );
  QVERIFY( textFile.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  QCOMPARE( textFile.write( QByteArrayLiteral( "spatial package payload" ) ), 23 );
  textFile.close();
  QFile emptyFile( QDir( sfpSource ).filePath( QStringLiteral( "empty.dat" ) ) );
  QVERIFY( emptyFile.open( QIODevice::WriteOnly | QIODevice::NewOnly ) );
  emptyFile.close();

  for ( const bool encrypted : { false, true } )
  {
    QgsMtpl::PackageOperationRequest sfpRequest;
    sfpRequest.type = QgsMtpl::PackageOperationType::CreateSfpPackage;
    sfpRequest.sourcePath = sfpSource;
    sfpRequest.outputPath = mWorkspace.filePath(
      encrypted ? QStringLiteral( "created-encrypted.sfp" ) : QStringLiteral( "created-plain.sfp" ) );
    sfpRequest.encryptOutput = encrypted;
    sfpRequest.generateOutputKeys = encrypted;
    sfpRequest.writeSidecar = false;
    QgsMtpl::PackageOperationResult sfpResult = QgsMtpl::PackageOperations::execute( sfpRequest );
    QVERIFY2( sfpResult.ok, qPrintable( sfpResult.error ) );
    QVERIFY( QFileInfo::exists( sfpRequest.outputPath ) );
    QgsMtpl::CryptoKeys probeKeys = encrypted ? sfpResult.outputKeys.cryptoKeys() : QgsMtpl::CryptoKeys();
    const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
      sfpRequest.outputPath, probeKeys, true,
      encrypted ? QgsMtpl::CredentialSource::Explicit : QgsMtpl::CredentialSource::None );
    QVERIFY2( probe.ok, qPrintable( probe.error ) );
    QCOMPARE( probe.packages.constFirst().format, QgsMtpl::PackageFormat::Sfp );
    QVERIFY( probe.packages.constFirst().isReady() );
    QCOMPARE( probe.packages.constFirst().sfpEntries.size(), 2 );
    probeKeys.clear();
    sfpResult.outputKeys.clear();
  }
}

void TestMtplOperation::createsLoadablePtp_data()
{
  QTest::addColumn<QByteArray>( "encoding" );
  QTest::addColumn<QString>( "suffix" );
  QTest::addColumn<int>( "tileSize" );
  QTest::addColumn<QByteArray>( "metadata" );
  QTest::addColumn<QString>( "coordinate" );
  QTest::addColumn<QString>( "outputName" );
  QTest::addColumn<bool>( "encrypted" );

  const QByteArray geographic = QByteArrayLiteral(
    "{\"mtpl_tile_matrix\":{\"version\":1,\"crs\":\"EPSG:4326\",\"scheme\":\"xyz\","
    "\"top_left\":[-180,90],\"z0_tile_span\":180,\"z0_matrix_width\":2,\"z0_matrix_height\":1}}" );
  for ( const bool encrypted : { false, true } )
  {
    const QByteArray mode = encrypted ? QByteArrayLiteral( "-encrypted" ) : QByteArrayLiteral( "-plain" );
    for ( const QByteArray &encoding : { QByteArrayLiteral( "png" ), QByteArrayLiteral( "jpeg" ), QByteArrayLiteral( "webp" ) } )
    {
      QTest::newRow( QByteArray( encoding + mode ).constData() )
        << encoding << QString::fromLatin1( encoding ) << 256 << QByteArrayLiteral( "{}" )
        << QStringLiteral( "0/0/0" ) << QStringLiteral( "created.ptp" ) << encrypted;
    }
    QTest::newRow( QByteArray( QByteArrayLiteral( "png-bin" ) + mode ).constData() )
      << QByteArrayLiteral( "png" ) << QStringLiteral( "bin" ) << 256 << QByteArrayLiteral( "{}" )
      << QStringLiteral( "0/0/0" ) << QStringLiteral( "created.ptp" ) << encrypted;
    QTest::newRow( QByteArray( QByteArrayLiteral( "multilevel" ) + mode ).constData() )
      << QByteArrayLiteral( "png" ) << QStringLiteral( "png" ) << 256 << QByteArrayLiteral( "{}" )
      << QStringLiteral( "0/0/0" ) << QStringLiteral( "multilevel.ptp" ) << encrypted;
    QTest::newRow( QByteArray( QByteArrayLiteral( "small-png" ) + mode ).constData() )
      << QByteArrayLiteral( "png" ) << QStringLiteral( "png" ) << 33 << QByteArrayLiteral( "{\"tile_size\":33}" )
      << QStringLiteral( "0/0/0" ) << QStringLiteral( "created.ptp" ) << encrypted;
    QTest::newRow( QByteArray( QByteArrayLiteral( "geographic-root" ) + mode ).constData() )
      << QByteArrayLiteral( "png" ) << QStringLiteral( "png" ) << 256 << geographic
      << QStringLiteral( "0/1/0" ) << QStringLiteral( "created.ptp" ) << encrypted;
    QTest::newRow( QByteArray( QByteArrayLiteral( "tms" ) + mode ).constData() )
      << QByteArrayLiteral( "png" ) << QStringLiteral( "png" ) << 256 << QByteArrayLiteral( "{\"scheme\":\"tms\"}" )
      << QStringLiteral( "1/0/1" ) << QStringLiteral( "created.ptp" ) << encrypted;
    QTest::newRow( QByteArray( QByteArrayLiteral( "partition-name" ) + mode ).constData() )
      << QByteArrayLiteral( "png" ) << QStringLiteral( "png" ) << 256 << QByteArrayLiteral( "{}" )
      << QStringLiteral( "8/32/32" ) << QStringLiteral( "8-11-3-1-1.ptp" ) << encrypted;
  }
}

void TestMtplOperation::createsLoadablePtp()
{
  QFETCH( QByteArray, encoding );
  QFETCH( QString, suffix );
  QFETCH( int, tileSize );
  QFETCH( QByteArray, metadata );
  QFETCH( QString, coordinate );
  QFETCH( QString, outputName );
  QFETCH( bool, encrypted );
  QTemporaryDir workspace;
  QVERIFY( workspace.isValid() );
  QString fixtureError;
  const QByteArray bytes = QgsMtplTest::rasterTileImage( encoding, tileSize, tileSize, 1, fixtureError );
  QVERIFY2( !bytes.isEmpty(), qPrintable( fixtureError ) );
  const QString source = QDir( workspace.path() ).filePath( QStringLiteral( "source" ) );
  QVERIFY( writeSourceTile( source, coordinate + QLatin1Char( '.' ) + suffix, bytes ) );
  if ( outputName == QLatin1String( "multilevel.ptp" ) )
    QVERIFY( writeSourceTile( source, QStringLiteral( "1/0/0.png" ), bytes ) );

  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::CreateTilePackage;
  request.sourcePath = source;
  request.outputPath = QDir( workspace.path() ).filePath( outputName );
  request.outputFormat = QgsMtpl::PackageFormat::Ptp;
  request.tileSize = tileSize;
  request.metadata = metadata;
  request.encryptOutput = encrypted;
  request.writeSidecar = encrypted;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QCOMPARE( result.outputPaths, QStringList( { request.outputPath } ) );
  QCOMPARE( !result.sidecarPath.isEmpty(), encrypted );

  // Sidecar discovery is the same path a beginner uses after creating an encrypted package.
  const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
    request.outputPath, {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( probe.ok, qPrintable( probe.error ) );
  QCOMPARE( probe.packages.size(), 1 );
  QVERIFY( probe.packages.constFirst().isReady() );
  const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset( request.outputPath, false, probe.packages );
  QVERIFY2( dataset.ok(), qPrintable( dataset.errorString() ) );
  QCOMPARE( dataset.dataset.matrix.tileSize, tileSize );
  QCOMPARE( dataset.dataset.payload, QgsMtpl::PayloadType::RasterImage );
  QCOMPARE( dataset.dataset.packages.size(), 1 );
  if ( outputName == QLatin1String( "multilevel.ptp" ) )
  {
    QCOMPARE( dataset.dataset.minimumZoom, 0 );
    QCOMPARE( dataset.dataset.maximumZoom, 1 );
  }
  QVERIFY( QDir( workspace.path() ).entryList(
    { QStringLiteral( ".*.mtpl-staging-*" ) }, QDir::Files | QDir::Hidden ).isEmpty() );
}

void TestMtplOperation::rejectsInvalidPtp_data()
{
  QTest::addColumn<QString>( "scenario" );
  QTest::addColumn<bool>( "encrypted" );
  const QStringList scenarios {
    QStringLiteral( "empty-tile" ), QStringLiteral( "unsupported-image" ), QStringLiteral( "truncated-image" ),
    QStringLiteral( "truncated-jpeg" ), QStringLiteral( "truncated-png-tail" ),
    QStringLiteral( "size-mismatch" ), QStringLiteral( "nonsquare-image" ), QStringLiteral( "second-bad-image" ),
    QStringLiteral( "second-size-mismatch" ), QStringLiteral( "duplicate-coordinate" ),
    QStringLiteral( "zoom-31" ), QStringLiteral( "outside-matrix" ),
    QStringLiteral( "invalid-json" ), QStringLiteral( "nonobject-json" ), QStringLiteral( "invalid-matrix" ),
    QStringLiteral( "metadata-size" ), QStringLiteral( "metadata-zoom" ), QStringLiteral( "missing-geographic-matrix" ),
    QStringLiteral( "declared-elevation" ), QStringLiteral( "partition-zoom" ), QStringLiteral( "partition-coordinate" )
  };
  for ( const QString &scenario : scenarios )
  {
    for ( const bool encrypted : { false, true } )
      QTest::newRow( QByteArray( scenario.toLatin1() + ( encrypted ? "-encrypted" : "-plain" ) ).constData() ) << scenario << encrypted;
  }
}

void TestMtplOperation::rejectsInvalidPtp()
{
  QFETCH( QString, scenario );
  QFETCH( bool, encrypted );
  QTemporaryDir workspace;
  QVERIFY( workspace.isValid() );
  QString fixtureError;
  QByteArray bytes = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 256, 256, 1, fixtureError );
  QVERIFY2( !bytes.isEmpty(), qPrintable( fixtureError ) );
  QByteArray metadata = QByteArrayLiteral( "{}" );
  QString relativePath = QStringLiteral( "0/0/0.png" );
  QString outputName = QStringLiteral( "rejected.ptp" );
  if ( scenario == QLatin1String( "empty-tile" ) )
    bytes.clear();
  else if ( scenario == QLatin1String( "unsupported-image" ) )
    bytes = QByteArrayLiteral( "This is not an image." );
  else if ( scenario == QLatin1String( "truncated-image" ) )
    bytes = bytes.left( 32 );
  else if ( scenario == QLatin1String( "truncated-jpeg" ) )
  {
    bytes = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "jpeg" ), 256, 256, 1, fixtureError );
    QVERIFY2( !bytes.isEmpty(), qPrintable( fixtureError ) );
    bytes.chop( bytes.size() / 3 );
    relativePath = QStringLiteral( "0/0/0.jpeg" );
  }
  else if ( scenario == QLatin1String( "truncated-png-tail" ) )
    bytes.chop( 2 );
  else if ( scenario == QLatin1String( "size-mismatch" ) || scenario == QLatin1String( "nonsquare-image" ) )
  {
    bytes = QgsMtplTest::rasterTileImage(
      QByteArrayLiteral( "png" ), 129, scenario == QLatin1String( "nonsquare-image" ) ? 256 : 129, 1, fixtureError );
    QVERIFY2( !bytes.isEmpty(), qPrintable( fixtureError ) );
  }
  else if ( scenario == QLatin1String( "zoom-31" ) )
    relativePath = QStringLiteral( "31/0/0.png" );
  else if ( scenario == QLatin1String( "outside-matrix" ) )
    relativePath = QStringLiteral( "0/1/0.png" );
  else if ( scenario == QLatin1String( "invalid-json" ) )
    metadata = QByteArrayLiteral( "{" );
  else if ( scenario == QLatin1String( "nonobject-json" ) )
    metadata = QByteArrayLiteral( "[]" );
  else if ( scenario == QLatin1String( "invalid-matrix" ) )
    metadata = QByteArrayLiteral( "{\"mtpl_tile_matrix\":{}}" );
  else if ( scenario == QLatin1String( "metadata-size" ) )
    metadata = QByteArrayLiteral( "{\"tile_size\":129}" );
  else if ( scenario == QLatin1String( "metadata-zoom" ) )
    metadata = QByteArrayLiteral(
      "{\"mtpl_tile_matrix\":{\"version\":1,\"crs\":\"EPSG:3857\",\"scheme\":\"xyz\","
      "\"top_left\":[-20037508.342789244,20037508.342789244],\"z0_tile_span\":40075016.685578488,"
      "\"z0_matrix_width\":1,\"z0_matrix_height\":1,\"min_zoom\":1,\"max_zoom\":2}}" );
  else if ( scenario == QLatin1String( "missing-geographic-matrix" ) )
    metadata = QByteArrayLiteral( "{\"crs\":\"EPSG:4326\"}" );
  else if ( scenario == QLatin1String( "declared-elevation" ) )
    metadata = QByteArrayLiteral( "{\"payload\":\"elevation\"}" );
  else if ( scenario == QLatin1String( "partition-zoom" ) )
    outputName = QStringLiteral( "8-11-3-1-1.ptp" );
  else if ( scenario == QLatin1String( "partition-coordinate" ) )
  {
    outputName = QStringLiteral( "8-11-3-1-1.ptp" );
    relativePath = QStringLiteral( "8/0/0.png" );
  }
  const QString source = QDir( workspace.path() ).filePath( QStringLiteral( "source" ) );
  QVERIFY( writeSourceTile( source, relativePath, bytes ) );
  if ( scenario == QLatin1String( "second-bad-image" ) )
    QVERIFY( writeSourceTile( source, QStringLiteral( "1/0/0.png" ), QByteArrayLiteral( "broken second image" ) ) );
  else if ( scenario == QLatin1String( "second-size-mismatch" ) )
  {
    const QByteArray secondBytes = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 129, 129, 2, fixtureError );
    QVERIFY2( !secondBytes.isEmpty(), qPrintable( fixtureError ) );
    QVERIFY( writeSourceTile( source, QStringLiteral( "1/0/0.png" ), secondBytes ) );
  }
  else if ( scenario == QLatin1String( "duplicate-coordinate" ) )
    QVERIFY( writeSourceTile( source, QStringLiteral( "0/0/00.png" ), bytes ) );

  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::CreateTilePackage;
  request.sourcePath = source;
  request.outputPath = QDir( workspace.path() ).filePath( outputName );
  request.outputFormat = QgsMtpl::PackageFormat::Ptp;
  request.metadata = metadata;
  request.encryptOutput = encrypted;
  request.writeSidecar = true;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY( !result.ok );
  QVERIFY( !result.canceled );
  QVERIFY( !result.error.isEmpty() );
  if ( scenario.startsWith( QLatin1String( "second-" ) ) )
    QVERIFY2( result.error.contains( QStringLiteral( "1/0/0.png" ) ), qPrintable( result.error ) );
  QVERIFY( result.outputPaths.isEmpty() );
  QVERIFY( result.sidecarPath.isEmpty() );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
  QVERIFY( !QFileInfo::exists( QgsMtpl::KeySidecarStore::singleSidecarPath( request.outputPath ) ) );
  QVERIFY( QDir( workspace.path() ).entryList(
    { QStringLiteral( ".*.mtpl-staging-*" ) }, QDir::Files | QDir::Hidden ).isEmpty() );
}

void TestMtplOperation::cancelsPtpCreation_data()
{
  QTest::addColumn<bool>( "afterStaging" );
  QTest::newRow( "during-scan" ) << false;
  QTest::newRow( "after-staging" ) << true;
}

void TestMtplOperation::cancelsPtpCreation()
{
  QFETCH( bool, afterStaging );
  QTemporaryDir workspace;
  QVERIFY( workspace.isValid() );
  QString fixtureError;
  const QByteArray bytes = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 256, 256, 1, fixtureError );
  QVERIFY2( !bytes.isEmpty(), qPrintable( fixtureError ) );
  const QString source = QDir( workspace.path() ).filePath( QStringLiteral( "source" ) );
  for ( int x = 0; x < 32; ++x )
    QVERIFY( writeSourceTile( source, QStringLiteral( "5/%1/0.png" ).arg( x ), bytes ) );
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::CreateTilePackage;
  request.sourcePath = source;
  request.outputPath = QDir( workspace.path() ).filePath( QStringLiteral( "canceled.ptp" ) );
  request.outputFormat = QgsMtpl::PackageFormat::Ptp;
  request.encryptOutput = true;
  request.writeSidecar = true;
  int checks = 0;
  bool stagedFileObserved = false;
  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request, [&]
  {
    ++checks;
    if ( !afterStaging )
      return checks == 8;
    const QFileInfoList files = QDir( workspace.path() ).entryInfoList(
      { QStringLiteral( ".*.mtpl-staging-*" ) }, QDir::Files | QDir::Hidden );
    for ( const QFileInfo &file : files )
      stagedFileObserved = stagedFileObserved || file.size() > 0;
    return stagedFileObserved;
  } );
  QVERIFY( !result.ok );
  QVERIFY( result.canceled );
  if ( afterStaging )
    QVERIFY( stagedFileObserved );
  else
    QVERIFY( checks < 32 );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
  QVERIFY( result.outputPaths.isEmpty() );
  QVERIFY( result.sidecarPath.isEmpty() );
  QVERIFY( !QFileInfo::exists( QgsMtpl::KeySidecarStore::singleSidecarPath( request.outputPath ) ) );
  QVERIFY( QDir( workspace.path() ).entryList(
    { QStringLiteral( ".*.mtpl-staging-*" ) }, QDir::Files | QDir::Hidden ).isEmpty() );
}

void TestMtplOperation::rejectsEncryptedEmptySfp()
{
  const QString sourcePath = mWorkspace.filePath( QStringLiteral( "empty-source.sfp" ) );
  mtpl_sfp_builder_t *builder = nullptr;
  QVERIFY( mtpl_sfp_builder_create( nullptr, &builder ) == MTPL_STATUS_OK );
  QVERIFY( builder );
  const QByteArray sourceUtf8 = sourcePath.toUtf8();
  const mtpl_status_t writeStatus = mtpl_sfp_builder_write( builder, sourceUtf8.constData() );
  mtpl_sfp_builder_destroy( builder );
  QVERIFY( writeStatus == MTPL_STATUS_OK );

  QgsMtpl::ConversionInput input;
  input.path = sourcePath;
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.inputs.append( input );
  request.outputPath = mWorkspace.filePath( QStringLiteral( "encrypted-empty.sfp" ) );
  request.encryptOutput = true;
  request.generateOutputKeys = true;
  request.writeSidecar = true;

  const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY( !result.ok );
  QVERIFY( !result.canceled );
  QVERIFY( result.error.contains( QStringLiteral( "空 SFP" ) ) );
  QVERIFY( !QFileInfo::exists( request.outputPath ) );
  QVERIFY( result.sidecarPath.isEmpty() );
  const QStringList stagingFiles = QDir( mWorkspace.path() ).entryList(
    QStringList() << QStringLiteral( ".encrypted-empty.sfp.mtpl-staging-*" ), QDir::Files | QDir::Hidden );
  QVERIFY( stagingFiles.isEmpty() );
}

void TestMtplOperation::rekeysMixedSfp()
{
  const QString sourcePath = mWorkspace.filePath( QStringLiteral( "mixed-source.sfp" ) );
  QgsMtpl::CryptoKeys sourceKeys = QgsMtplTest::fixtureKeys();
  QVERIFY( sourceKeys.isValid() );
  const QList<QgsMtplTest::SfpFixtureEntry> entries = {
    { QStringLiteral( "plain/readme.txt" ), QByteArrayLiteral( "plain fixture entry" ), MTPL_STORAGE_PLAIN },
    { QStringLiteral( "secure/data.bin" ), QByteArrayLiteral( "encrypted fixture entry" ), MTPL_STORAGE_ENCRYPTED }
  };
  QString fixtureError;
  QVERIFY2( QgsMtplTest::writeSfpFixture( sourcePath, entries, sourceKeys, fixtureError ),
            qPrintable( fixtureError ) );

  QgsMtpl::ConversionInput input;
  input.path = sourcePath;
  input.keys = sourceKeys;
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.inputs.append( input );
  request.outputPath = mWorkspace.filePath( QStringLiteral( "rekeyed-mixed.sfp" ) );
  request.encryptOutput = true;
  request.preserveSfpMixedStorage = true;
  request.generateOutputKeys = true;
  request.writeSidecar = false;

  QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
  QVERIFY2( result.ok, qPrintable( result.error ) );
  QVERIFY( result.outputKeys.isValid() );
  QVERIFY( QFileInfo::exists( request.outputPath ) );

  QgsMtpl::CryptoKeys outputKeys = result.outputKeys.cryptoKeys();
  const QgsMtpl::ProbeResult sourceProbe = QgsMtplPackageService::probePath(
    sourcePath, sourceKeys, true, QgsMtpl::CredentialSource::Explicit );
  const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
    request.outputPath, outputKeys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( sourceProbe.ok, qPrintable( sourceProbe.error ) );
  QVERIFY2( probe.ok, qPrintable( probe.error ) );
  QCOMPARE( sourceProbe.packages.size(), 1 );
  QCOMPARE( probe.packages.size(), 1 );
  QVERIFY( probe.packages.constFirst().isReady() );
  const QList<QgsMtpl::SfpEntryDescriptor> &sourceEntries = sourceProbe.packages.constFirst().sfpEntries;
  const QList<QgsMtpl::SfpEntryDescriptor> &outputEntries = probe.packages.constFirst().sfpEntries;
  QCOMPARE( outputEntries.size(), sourceEntries.size() );
  for ( qsizetype index = 0; index < sourceEntries.size(); ++index )
  {
    QCOMPARE( outputEntries.at( index ).path, sourceEntries.at( index ).path );
    QCOMPARE( outputEntries.at( index ).logicalSize, sourceEntries.at( index ).logicalSize );
    QCOMPARE( outputEntries.at( index ).encrypted, sourceEntries.at( index ).encrypted );
  }

  sourceKeys.clear();
  outputKeys.clear();
  result.outputKeys.clear();
}

void TestMtplOperation::packageTaskLifecycle_data()
{
  QTest::addColumn<bool>( "sfp" );
  QTest::addColumn<QString>( "suffix" );
  QTest::newRow( "vtp" ) << false << QStringLiteral( "vtp" );
  QTest::newRow( "ptp" ) << false << QStringLiteral( "ptp" );
  QTest::newRow( "sfp" ) << true << QStringLiteral( "sfp" );
}

void TestMtplOperation::packageTaskLifecycle()
{
  QFETCH( bool, sfp );
  QFETCH( QString, suffix );

  const QString sourcePath = mWorkspace.filePath(
    QStringLiteral( "task-source-%1.%1" ).arg( suffix ) );
  QString fixtureError;
  if ( sfp )
  {
    const QList<QgsMtplTest::SfpFixtureEntry> entries = {
      { QStringLiteral( "task/payload.txt" ), QByteArrayLiteral( "task fixture payload" ), MTPL_STORAGE_PLAIN }
    };
    QVERIFY2( QgsMtplTest::writeSfpFixture( sourcePath, entries, {}, fixtureError ), qPrintable( fixtureError ) );
  }
  else
  {
    QVERIFY2( QgsMtplTest::writeTileFixture(
                sourcePath, QgsMtpl::packageFormatFromPath( sourcePath ), MTPL_STORAGE_PLAIN, {}, fixtureError ),
              qPrintable( fixtureError ) );
  }

  QgsMtpl::ConversionInput input;
  input.path = sourcePath;
  QgsMtpl::PackageOperationRequest request;
  request.type = QgsMtpl::PackageOperationType::Convert;
  request.inputs.append( input );
  request.outputPath = mWorkspace.filePath(
    QStringLiteral( "task-encrypted-%1.%1" ).arg( suffix ) );
  request.encryptOutput = true;
  request.generateOutputKeys = true;
  request.writeSidecar = false;

  auto *task = new QgsMtplPackageOperationTask( request );
  QSignalSpy completedSpy( task, &QgsTask::taskCompleted );
  QSignalSpy terminatedSpy( task, &QgsTask::taskTerminated );
  QSignalSpy progressSpy( task, &QgsTask::progressChanged );
  QgsMtpl::PackageOperationResult capturedResult;
  bool finished = false;
  connect( task, &QgsTask::taskCompleted, this, [task, &capturedResult, &finished]
  {
    capturedResult = task->result();
    finished = true;
  } );
  connect( task, &QgsTask::taskTerminated, this, [task, &capturedResult, &finished]
  {
    capturedResult = task->result();
    finished = true;
  } );
  QVERIFY( QgsApplication::taskManager()->addTask( task ) );
  QTRY_VERIFY_WITH_TIMEOUT( finished, 30000 );

  QCOMPARE( completedSpy.count(), 1 );
  QCOMPARE( terminatedSpy.count(), 0 );
  QVERIFY( !progressSpy.isEmpty() );
  QVERIFY2( capturedResult.ok, qPrintable( capturedResult.error ) );
  QCOMPARE( capturedResult.outputPaths,
            QStringList( { QFileInfo( request.outputPath ).absoluteFilePath() } ) );
  QVERIFY( capturedResult.outputKeys.isValid() );
  QgsMtpl::CryptoKeys outputKeys = capturedResult.outputKeys.cryptoKeys();
  const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
    request.outputPath, outputKeys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( probe.ok, qPrintable( probe.error ) );
  QCOMPARE( probe.packages.size(), 1 );
  QCOMPARE( probe.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyVerified );
  if ( suffix == QLatin1String( "ptp" ) )
  {
    const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset( request.outputPath, false, probe.packages );
    QVERIFY2( dataset.ok(), qPrintable( dataset.errorString() ) );
  }
  outputKeys.clear();
  capturedResult.outputKeys.clear();
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplOperation::encryptDecryptRoundTrip_data()
{
  QTest::addColumn<bool>( "sfp" );
  QTest::addColumn<QString>( "formatName" );

  QTest::newRow( "vtp" ) << false << QStringLiteral( "vtp" );
  QTest::newRow( "ptp" ) << false << QStringLiteral( "ptp" );
  QTest::newRow( "sfp" ) << true << QStringLiteral( "sfp" );
}

void TestMtplOperation::encryptDecryptRoundTrip()
{
  QFETCH( bool, sfp );
  QFETCH( QString, formatName );
  const QString sourcePath = mWorkspace.filePath(
    QStringLiteral( "round-trip-source-%1.%1" ).arg( formatName ) );
  QString fixtureError;
  if ( sfp )
  {
    const QList<QgsMtplTest::SfpFixtureEntry> entries = {
      { QStringLiteral( "plain/readme.txt" ), QByteArrayLiteral( "portable round-trip fixture" ), MTPL_STORAGE_PLAIN },
      { QStringLiteral( "nested/empty.dat" ), QByteArray(), MTPL_STORAGE_PLAIN }
    };
    QVERIFY2( QgsMtplTest::writeSfpFixture( sourcePath, entries, {}, fixtureError ), qPrintable( fixtureError ) );
  }
  else
  {
    QVERIFY2( QgsMtplTest::writeTileFixture(
                sourcePath, QgsMtpl::packageFormatFromPath( sourcePath ), MTPL_STORAGE_PLAIN, {}, fixtureError ),
              qPrintable( fixtureError ) );
  }

  const QByteArray sourceHashBefore = fileSha256( sourcePath );
  QVERIFY( !sourceHashBefore.isEmpty() );

  const QString roundTripDirectory = mWorkspace.filePath( QStringLiteral( "round-trip" ) );
  QVERIFY( QDir().mkpath( roundTripDirectory ) );
  const QString encryptedPath = QDir( roundTripDirectory ).filePath( QStringLiteral( "encrypted-%1.%1" ).arg( formatName ) );
  const QString rekeyedPath = QDir( roundTripDirectory ).filePath( QStringLiteral( "rekeyed-%1.%1" ).arg( formatName ) );
  const QString decryptedPath = QDir( roundTripDirectory ).filePath( QStringLiteral( "decrypted-%1.%1" ).arg( formatName ) );

  QgsMtpl::ConversionInput encryptInput;
  encryptInput.path = sourcePath;
  QgsMtpl::PackageOperationRequest encryptRequest;
  encryptRequest.type = QgsMtpl::PackageOperationType::Convert;
  encryptRequest.inputs.append( encryptInput );
  encryptRequest.outputPath = encryptedPath;
  encryptRequest.encryptOutput = true;
  encryptRequest.generateOutputKeys = true;
  encryptRequest.writeSidecar = false;

  QVector<double> encryptProgress;
  QgsMtpl::PackageOperationResult encryptResult = QgsMtpl::PackageOperations::execute(
    encryptRequest, {}, [&encryptProgress]( double progress ) { encryptProgress.append( progress ); } );
  QVERIFY2( encryptResult.ok, "Encryption did not produce a committed output." );
  QVERIFY( !encryptResult.canceled );
  QCOMPARE( encryptResult.outputPaths, QStringList( { QFileInfo( encryptedPath ).absoluteFilePath() } ) );
  QVERIFY( QFileInfo( encryptedPath ).isFile() );
  QVERIFY( encryptResult.sidecarPath.isEmpty() );
  QVERIFY( encryptResult.outputKeys.isValid() );
  QVERIFY( !encryptProgress.isEmpty() );
  QCOMPARE( encryptProgress.constFirst(), 0.0 );
  QCOMPARE( encryptProgress.constLast(), 100.0 );
  for ( qsizetype index = 0; index < encryptProgress.size(); ++index )
  {
    QVERIFY( encryptProgress.at( index ) >= 0.0 );
    QVERIFY( encryptProgress.at( index ) <= 100.0 );
    if ( index > 0 )
      QVERIFY( encryptProgress.at( index ) >= encryptProgress.at( index - 1 ) );
  }

  const QgsMtpl::ProbeResult lockedProbe = QgsMtplPackageService::probePath(
    encryptedPath, QgsMtpl::CryptoKeys(), false, QgsMtpl::CredentialSource::None );
  QVERIFY( lockedProbe.ok );
  QCOMPARE( lockedProbe.packages.size(), 1 );
  QCOMPARE( lockedProbe.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRequired );

  QgsMtpl::CryptoKeys generatedKeys = encryptResult.outputKeys.cryptoKeys();
  QVERIFY( generatedKeys.isValid() );
  const QgsMtpl::ProbeResult verifiedProbe = QgsMtplPackageService::probePath(
    encryptedPath, generatedKeys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY( verifiedProbe.ok );
  QCOMPARE( verifiedProbe.packages.size(), 1 );
  QCOMPARE( verifiedProbe.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyVerified );

  QgsMtpl::ConversionInput rekeyInput;
  rekeyInput.path = encryptedPath;
  rekeyInput.keys = generatedKeys;
  QgsMtpl::PackageOperationRequest rekeyRequest;
  rekeyRequest.type = QgsMtpl::PackageOperationType::Convert;
  rekeyRequest.inputs.append( rekeyInput );
  rekeyRequest.outputPath = rekeyedPath;
  rekeyRequest.encryptOutput = true;
  rekeyRequest.preserveSfpMixedStorage = true;
  rekeyRequest.generateOutputKeys = true;
  rekeyRequest.writeSidecar = false;

  QgsMtpl::PackageOperationResult rekeyResult = QgsMtpl::PackageOperations::execute( rekeyRequest );
  QVERIFY2( rekeyResult.ok, qPrintable( rekeyResult.error ) );
  QVERIFY( rekeyResult.outputKeys.isValid() );
  QgsMtpl::CryptoKeys rekeyedKeys = rekeyResult.outputKeys.cryptoKeys();
  const QgsMtpl::ProbeResult oldKeyProbe = QgsMtplPackageService::probePath(
    rekeyedPath, generatedKeys, false, QgsMtpl::CredentialSource::Explicit );
  QVERIFY( oldKeyProbe.ok );
  QCOMPARE( oldKeyProbe.packages.size(), 1 );
  QCOMPARE( oldKeyProbe.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRejectedOrCorrupt );
  const QgsMtpl::ProbeResult rekeyedProbe = QgsMtplPackageService::probePath(
    rekeyedPath, rekeyedKeys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY2( rekeyedProbe.ok, qPrintable( rekeyedProbe.error ) );
  QCOMPARE( rekeyedProbe.packages.size(), 1 );
  QCOMPARE( rekeyedProbe.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyVerified );

  QgsMtpl::ConversionInput decryptInput;
  decryptInput.path = rekeyedPath;
  decryptInput.keys = rekeyedKeys;
  QgsMtpl::PackageOperationRequest decryptRequest;
  decryptRequest.type = QgsMtpl::PackageOperationType::Convert;
  decryptRequest.inputs.append( decryptInput );
  decryptRequest.outputPath = decryptedPath;
  decryptRequest.encryptOutput = false;
  decryptRequest.writeSidecar = false;

  const QgsMtpl::PackageOperationResult decryptResult = QgsMtpl::PackageOperations::execute( decryptRequest );
  QVERIFY2( decryptResult.ok, "Decryption did not produce a committed output." );
  QVERIFY( !decryptResult.canceled );
  QCOMPARE( decryptResult.outputPaths, QStringList( { QFileInfo( decryptedPath ).absoluteFilePath() } ) );
  QVERIFY( QFileInfo( decryptedPath ).isFile() );

  const QgsMtpl::ProbeResult sourceProbe = QgsMtplPackageService::probePath(
    sourcePath, QgsMtpl::CryptoKeys(), true, QgsMtpl::CredentialSource::None );
  const QgsMtpl::ProbeResult decryptedProbe = QgsMtplPackageService::probePath(
    decryptedPath, QgsMtpl::CryptoKeys(), true, QgsMtpl::CredentialSource::None );
  QVERIFY( sourceProbe.ok );
  QVERIFY( decryptedProbe.ok );
  QCOMPARE( sourceProbe.packages.size(), 1 );
  QCOMPARE( decryptedProbe.packages.size(), 1 );
  const QgsMtpl::PackageDescriptor &sourceDescriptor = sourceProbe.packages.constFirst();
  const QgsMtpl::PackageDescriptor &decryptedDescriptor = decryptedProbe.packages.constFirst();
  QCOMPARE( decryptedDescriptor.format, sourceDescriptor.format );
  QCOMPARE( decryptedDescriptor.payload, sourceDescriptor.payload );
  QCOMPARE( decryptedDescriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
  QCOMPARE( decryptedDescriptor.tileSize, sourceDescriptor.tileSize );
  QCOMPARE( decryptedDescriptor.minimumZoom, sourceDescriptor.minimumZoom );
  QCOMPARE( decryptedDescriptor.maximumZoom, sourceDescriptor.maximumZoom );
  QCOMPARE( decryptedDescriptor.metadata.value( QStringLiteral( "rangeCount" ) ),
            sourceDescriptor.metadata.value( QStringLiteral( "rangeCount" ) ) );
  if ( formatName == QLatin1String( "ptp" ) )
  {
    for ( const QgsMtpl::ProbeResult &probe : { sourceProbe, verifiedProbe, rekeyedProbe, decryptedProbe } )
    {
      const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset(
        probe.packages.constFirst().path, false, probe.packages );
      QVERIFY2( dataset.ok(), qPrintable( dataset.errorString() ) );
    }
  }
  if ( sfp )
  {
    QCOMPARE( decryptedDescriptor.sfpEntries.size(), sourceDescriptor.sfpEntries.size() );
    for ( qsizetype index = 0; index < sourceDescriptor.sfpEntries.size(); ++index )
    {
      QCOMPARE( decryptedDescriptor.sfpEntries.at( index ).path,
                sourceDescriptor.sfpEntries.at( index ).path );
      QCOMPARE( decryptedDescriptor.sfpEntries.at( index ).logicalSize,
                sourceDescriptor.sfpEntries.at( index ).logicalSize );
      QVERIFY( !decryptedDescriptor.sfpEntries.at( index ).encrypted );
    }
  }

  QCOMPARE( fileSha256( sourcePath ), sourceHashBefore );
  const QStringList stagingFiles = QDir( roundTripDirectory ).entryList(
    QStringList() << QStringLiteral( ".*.mtpl-staging-*" ), QDir::Files | QDir::Hidden );
  QVERIFY( stagingFiles.isEmpty() );
  rekeyedKeys.clear();
  rekeyResult.outputKeys.clear();
  generatedKeys.clear();
  encryptResult.outputKeys.clear();
}

QGSTEST_MAIN( TestMtplOperation )
#include "test_mtpl_operation.moc"
