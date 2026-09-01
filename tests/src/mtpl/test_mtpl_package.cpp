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
#include "qgstest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSignalSpy>

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
    void probeSfpFixtures_data();
    void probeSfpFixtures();
    void probeVtpReadinessStates();
    void probeMixedSfpWithMatchingAndWrongKeys();
    void probeEmptySfp();
    void probeUnsupportedAndUnreadable();
    void probeLongUnicodePath();
    void probePlainDirectory();
    void probeDirectoryNonrecursively();
    void probeExplicitKeyDirectory();
    void probeTaskLifecycle_data();
    void probeTaskLifecycle();
    void probeTaskCancellation_data();
    void probeTaskCancellation();

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
  QCOMPARE( descriptor.encryption, QgsMtpl::EncryptionState::Plain );
  QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
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
  QVERIFY( !result.error.isEmpty() );
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
  QTest::newRow( "vtp" ) << QStringLiteral( "vtp" );
  QTest::newRow( "sfp" ) << QStringLiteral( "sfp" );
}

void TestMtplPackage::probeTaskLifecycle()
{
  QFETCH( QString, kind );
  QString error;
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "task-complete-%1.%1" ).arg( kind ) );
  if ( kind == QLatin1String( "vtp" ) )
  {
    QVERIFY2( QgsMtplTest::writeTileFixture( packagePath, QgsMtpl::PackageFormat::Vtp,
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
  QTest::newRow( "vtp" ) << QStringLiteral( "vtp" );
  QTest::newRow( "sfp" ) << QStringLiteral( "sfp" );
}

void TestMtplPackage::probeTaskCancellation()
{
  QFETCH( QString, kind );
  QString error;
  const QString packagePath = mWorkspace.filePath( QStringLiteral( "task-cancel-%1.%1" ).arg( kind ) );
  if ( kind == QLatin1String( "vtp" ) )
  {
    QVERIFY2( QgsMtplTest::writeTileFixture( packagePath, QgsMtpl::PackageFormat::Vtp,
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

QGSTEST_MAIN( TestMtplPackage )
#include "test_mtpl_package.moc"
