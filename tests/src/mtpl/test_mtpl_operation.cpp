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
#include "qgstest.h"

#include <mtpl/sfp.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
  request.outputFormat = sfp ? QgsMtpl::PackageFormat::Sfp : QgsMtpl::PackageFormat::Vtp;

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
                sourcePath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, fixtureError ),
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
  QCOMPARE( tileFile.write( QByteArrayLiteral( "synthetic-tile" ) ), 14 );
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
                sourcePath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, fixtureError ),
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
  outputKeys.clear();
  capturedResult.outputKeys.clear();
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplOperation::encryptDecryptRoundTrip_data()
{
  QTest::addColumn<bool>( "sfp" );
  QTest::addColumn<QString>( "formatName" );

  QTest::newRow( "vtp" ) << false << QStringLiteral( "vtp" );
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
                sourcePath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, fixtureError ),
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
