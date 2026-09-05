/***************************************************************************
  test_mtpl_credentials.cpp
  -------------------------
  Isolated Authentication Manager tests for MTPL credential persistence.
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

#include "qgsapplication.h"
#include "qgsauthconfig.h"
#include "qgsauthmanager.h"
#include "qgsmtplcredentialstore.h"
#include "qgssettings.h"
#include "qgstest.h"

#include <QDirIterator>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

class TestMtplCredentials : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void legacyPlaintextIsDeletedWithoutMigration();
    void foreignAuthenticationConfigIsNeverDeleted();
    void encryptedRoundTripAndMasterPasswordChange();

  private:
    bool authFilesContain( const QByteArray &needle ) const;

    QTemporaryDir mAuthDirectory;
    const QString mInitialPassword = QStringLiteral( "mtpl-isolated-password" );
};

void TestMtplCredentials::initTestCase()
{
  QVERIFY( mAuthDirectory.isValid() );
  qputenv( "QGIS_AUTH_DB_DIR_PATH", mAuthDirectory.path().toUtf8() );

  QgsApplication::init();
  QgsApplication::initQgis();
  QgsMtplCredentialStore::setBackendForTesting( nullptr );

  QgsAuthManager *manager = QgsApplication::authManager();
  QVERIFY( manager );
  QVERIFY2( !manager->isDisabled(), qPrintable( manager->disabledMessage() ) );
  QVERIFY2( manager->setMasterPassword( mInitialPassword, true ),
            "Could not initialize the isolated authentication database" );
  QVERIFY2( manager->authMethodsKeys().contains( QStringLiteral( "Mtpl" ) ),
            "The dedicated MTPL authentication method was not discovered" );
  QgsMtplCredentialStore::clear();
}

void TestMtplCredentials::cleanupTestCase()
{
  QgsMtplCredentialStore::clear();
  QgsApplication::exitQgis();
  qunsetenv( "QGIS_AUTH_DB_DIR_PATH" );
}

void TestMtplCredentials::legacyPlaintextIsDeletedWithoutMigration()
{
  QgsMtplCredentialStore::clear();
  QgsSettings settings;
  settings.setValue( QStringLiteral( "mtpl/credentials/remember" ), true,
                     QgsSettings::Section::Plugins );
  settings.setValue( QStringLiteral( "mtpl/credentials/privateKey" ),
                     QStringLiteral( "legacy-private-secret" ), QgsSettings::Section::Plugins );
  settings.setValue( QStringLiteral( "mtpl/credentials/deviceKey" ),
                     QStringLiteral( "legacy-device-secret" ), QgsSettings::Section::Plugins );
  settings.sync();

  QVERIFY( !QgsMtplCredentialStore::hasRememberedKeys() );
  QVERIFY( !settings.contains( QStringLiteral( "mtpl/credentials/remember" ),
                               QgsSettings::Section::Plugins ) );
  QVERIFY( !settings.contains( QStringLiteral( "mtpl/credentials/privateKey" ),
                               QgsSettings::Section::Plugins ) );
  QVERIFY( !settings.contains( QStringLiteral( "mtpl/credentials/deviceKey" ),
                               QgsSettings::Section::Plugins ) );
  QVERIFY( !settings.contains( QStringLiteral( "mtpl/credentials/authcfg" ),
                               QgsSettings::Section::Plugins ) );
}

bool TestMtplCredentials::authFilesContain( const QByteArray &needle ) const
{
  QDirIterator iterator( mAuthDirectory.path(), QDir::Files, QDirIterator::Subdirectories );
  while ( iterator.hasNext() )
  {
    QFile file( iterator.next() );
    if ( file.open( QIODevice::ReadOnly ) && file.readAll().contains( needle ) )
      return true;
  }
  return false;
}

void TestMtplCredentials::foreignAuthenticationConfigIsNeverDeleted()
{
  QgsMtplCredentialStore::clear();
  QgsAuthManager *manager = QgsApplication::authManager();
  QVERIFY( manager );

  QgsAuthMethodConfig foreignConfig;
  foreignConfig.setName( QStringLiteral( "Unrelated Basic authentication" ) );
  foreignConfig.setMethod( QStringLiteral( "Basic" ) );
  foreignConfig.setUri( QStringLiteral( "https://example.invalid" ) );
  foreignConfig.setConfig( QStringLiteral( "username" ), QStringLiteral( "unrelated-user" ) );
  foreignConfig.setConfig( QStringLiteral( "password" ), QStringLiteral( "unrelated-password" ) );
  QVERIFY( foreignConfig.isValid() );
  QVERIFY( manager->storeAuthenticationConfig( foreignConfig ) );
  const QString foreignId = foreignConfig.id();
  QCOMPARE( foreignId.size(), 7 );

  QgsSettings settings;
  settings.setValue( QStringLiteral( "mtpl/credentials/authcfg" ), foreignId,
                     QgsSettings::Section::Plugins );
  settings.sync();

  QVERIFY( !QgsMtplCredentialStore::hasRememberedKeys() );
  QString error;
  QVERIFY2( QgsMtplCredentialStore::clearRememberedKeys( &error ), qPrintable( error ) );
  QVERIFY( !settings.contains( QStringLiteral( "mtpl/credentials/authcfg" ),
                               QgsSettings::Section::Plugins ) );
  QVERIFY( manager->configIds().contains( foreignId ) );

  QgsAuthMethodConfig preservedConfig;
  QVERIFY( manager->loadAuthenticationConfig( foreignId, preservedConfig, false ) );
  QCOMPARE( preservedConfig.method(), QStringLiteral( "Basic" ) );

  settings.setValue( QStringLiteral( "mtpl/credentials/authcfg" ), foreignId,
                     QgsSettings::Section::Plugins );
  settings.sync();

  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QVERIFY2( QgsMtplCredentialStore::saveManualKeys(
              keys, QStringLiteral( "C:/isolated/foreign-reference.ptp" ), error ),
            qPrintable( error ) );
  const QString mtplId = settings.value(
    QStringLiteral( "mtpl/credentials/authcfg" ), QString(),
    QgsSettings::Section::Plugins ).toString();
  QVERIFY( !mtplId.isEmpty() );
  QVERIFY( mtplId != foreignId );
  QVERIFY( manager->configIds().contains( foreignId ) );
  QVERIFY( manager->configIds().contains( mtplId ) );

  QVERIFY2( QgsMtplCredentialStore::clearRememberedKeys( &error ), qPrintable( error ) );
  QVERIFY( !manager->configIds().contains( mtplId ) );
  QVERIFY( manager->configIds().contains( foreignId ) );
  QVERIFY( manager->removeAuthenticationConfig( foreignId ) );
  keys.clear();
}

void TestMtplCredentials::encryptedRoundTripAndMasterPasswordChange()
{
  QgsMtplCredentialStore::clear();
  QVERIFY( !QgsMtplCredentialStore::rememberEnabled() );

  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QVERIFY( keys.isValid() );
  const QString sourcePath = QStringLiteral( "C:/isolated/credential-test.ptp" );
  QString error;
  QVERIFY2( QgsMtplCredentialStore::saveManualKeys( keys, sourcePath, error ), qPrintable( error ) );
  QVERIFY( QgsMtplCredentialStore::hasRememberedKeys() );

  QgsSettings settings;
  const QString firstAuthConfigId = settings.value(
    QStringLiteral( "mtpl/credentials/authcfg" ), QString(), QgsSettings::Section::Plugins ).toString();
  QCOMPARE( firstAuthConfigId.size(), 7 );
  QSettings persistedSettings( settings.fileName(), QSettings::defaultFormat() );
  persistedSettings.sync();
  QCOMPARE( persistedSettings.status(), QSettings::NoError );
  QCOMPARE( persistedSettings.value( settings.prefixedKey(
              QStringLiteral( "mtpl/credentials/authcfg" ), QgsSettings::Section::Plugins ) ).toString(),
            firstAuthConfigId );
  QCOMPARE( QgsMtplCredentialStore::lastPath(), sourcePath );
  QVERIFY( !settings.contains( QStringLiteral( "mtpl/credentials/privateKey" ),
                               QgsSettings::Section::Plugins ) );
  QVERIFY( !settings.contains( QStringLiteral( "mtpl/credentials/deviceKey" ),
                               QgsSettings::Section::Plugins ) );

  QgsAuthMethodConfig storedConfig;
  QVERIFY( QgsApplication::authManager()->loadAuthenticationConfig( firstAuthConfigId, storedConfig, true ) );
  QCOMPARE( storedConfig.method(), QStringLiteral( "Mtpl" ) );
  QCOMPARE( storedConfig.config( QStringLiteral( "privateKeyBase64" ) ).toLatin1(), keys.privateKey );
  QCOMPARE( storedConfig.config( QStringLiteral( "deviceKeyHex" ) ).toLatin1(), keys.deviceKey );
  QVERIFY( !storedConfig.configMap().contains( QStringLiteral( "privateKey" ) ) );
  QVERIFY( !storedConfig.configMap().contains( QStringLiteral( "deviceKey" ) ) );
  QVERIFY( !authFilesContain( keys.privateKey ) );
  QVERIFY( !authFilesContain( keys.deviceKey ) );

  QgsMtpl::CryptoKeys replacementKeys = QgsMtplTest::differentFixtureKeys();
  const QString replacementPath = QStringLiteral( "C:/isolated/replacement.ptp" );
  QVERIFY2( QgsMtplCredentialStore::saveManualKeys( replacementKeys, replacementPath, error ),
            qPrintable( error ) );
  const QString authConfigId = settings.value(
    QStringLiteral( "mtpl/credentials/authcfg" ), QString(), QgsSettings::Section::Plugins ).toString();
  QVERIFY( authConfigId != firstAuthConfigId );
  persistedSettings.sync();
  QCOMPARE( persistedSettings.status(), QSettings::NoError );
  QCOMPARE( persistedSettings.value( settings.prefixedKey(
              QStringLiteral( "mtpl/credentials/authcfg" ), QgsSettings::Section::Plugins ) ).toString(),
            authConfigId );
  QVERIFY( !QgsApplication::authManager()->configIds().contains( firstAuthConfigId ) );
  QVERIFY( QgsApplication::authManager()->configIds().contains( authConfigId ) );
  QCOMPARE( QgsMtplCredentialStore::lastPath(), replacementPath );

  int mtplConfigCount = 0;
  const QStringList configIds = QgsApplication::authManager()->configIds();
  for ( const QString &configId : configIds )
  {
    QgsAuthMethodConfig config;
    if ( QgsApplication::authManager()->loadAuthenticationConfig( configId, config, false ) &&
         config.method() == QStringLiteral( "Mtpl" ) )
      ++mtplConfigCount;
  }
  QCOMPARE( mtplConfigCount, 1 );
  QVERIFY( !authFilesContain( replacementKeys.privateKey ) );
  QVERIFY( !authFilesContain( replacementKeys.deviceKey ) );

  QgsMtpl::CryptoKeys transactionKeys = QgsMtplTest::fixtureKeys();
  transactionKeys.deviceKey[0] = transactionKeys.deviceKey.at( 0 ) == '0' ? '1' : '0';
  const QString transactionPath = QStringLiteral( "C:/isolated/transaction-failure.ptp" );
  QgsMtplCredentialStore::failNextCredentialReferenceWriteForTesting();
  QVERIFY( !QgsMtplCredentialStore::saveManualKeys( transactionKeys, transactionPath, error ) );
  QVERIFY( !error.isEmpty() );
  QCOMPARE( settings.value(
              QStringLiteral( "mtpl/credentials/authcfg" ), QString(), QgsSettings::Section::Plugins ).toString(),
            authConfigId );
  QVERIFY( QgsApplication::authManager()->configIds().contains( authConfigId ) );
  QCOMPARE( QgsMtplCredentialStore::lastPath(), replacementPath );
  mtplConfigCount = 0;
  for ( const QString &configId : QgsApplication::authManager()->configIds() )
  {
    QgsAuthMethodConfig config;
    if ( QgsApplication::authManager()->loadAuthenticationConfig( configId, config, false ) &&
         config.method() == QStringLiteral( "Mtpl" ) )
      ++mtplConfigCount;
  }
  QCOMPARE( mtplConfigCount, 1 );

  settings.setValue( QStringLiteral( "mtpl/credentials/remember" ), true,
                     QgsSettings::Section::Plugins );
  settings.setValue( QStringLiteral( "mtpl/credentials/privateKey" ),
                     QStringLiteral( "legacy-private-secret" ), QgsSettings::Section::Plugins );
  settings.setValue( QStringLiteral( "mtpl/credentials/deviceKey" ),
                     QStringLiteral( "legacy-device-secret" ), QgsSettings::Section::Plugins );
  settings.sync();
  QgsMtplCredentialStore::failNextLegacyCleanupForTesting();
  error.clear();
  QVERIFY( !QgsMtplCredentialStore::saveManualKeys( transactionKeys, transactionPath, error ) );
  QVERIFY( !error.isEmpty() );
  QCOMPARE( settings.value(
              QStringLiteral( "mtpl/credentials/authcfg" ), QString(), QgsSettings::Section::Plugins ).toString(),
            authConfigId );
  QVERIFY( settings.contains( QStringLiteral( "mtpl/credentials/privateKey" ),
                              QgsSettings::Section::Plugins ) );
  QCOMPARE( QgsMtplCredentialStore::lastPath(), replacementPath );
  QString cleanupDiagnostic;
  QgsMtplCredentialStore::failNextLegacyCleanupForTesting();
  QVERIFY( !QgsMtplCredentialStore::hasRememberedKeys( &cleanupDiagnostic ) );
  QVERIFY( !cleanupDiagnostic.isEmpty() );
  cleanupDiagnostic.clear();
  QVERIFY( QgsMtplCredentialStore::hasRememberedKeys( &cleanupDiagnostic ) );
  QVERIFY( cleanupDiagnostic.isEmpty() );
  QVERIFY( !settings.contains( QStringLiteral( "mtpl/credentials/privateKey" ),
                               QgsSettings::Section::Plugins ) );
  transactionKeys.clear();

  QgsMtpl::CryptoKeys invalidKeys;
  const QString failedPath = QStringLiteral( "C:/isolated/failed-save.ptp" );
  QVERIFY( !QgsMtplCredentialStore::saveManualKeys( invalidKeys, failedPath, error ) );
  QCOMPARE( QgsMtplCredentialStore::lastPath(), replacementPath );
  error.clear();

  QgsAuthManager *manager = QgsApplication::authManager();
  manager->clearMasterPassword();
  QVERIFY( !manager->masterPasswordIsSet() );
  QgsMtpl::CryptoKeys silentlyLoaded = QgsMtplCredentialStore::rememberedKeysIfUnlocked( &error );
  QVERIFY( silentlyLoaded.isEmpty() );
  QVERIFY( !error.isEmpty() );
  QVERIFY( !manager->masterPasswordIsSet() );
  QVERIFY( QgsMtplCredentialStore::hasRememberedKeys() );
  QVERIFY2( manager->setMasterPassword( mInitialPassword, true ),
            "Could not unlock the isolated authentication database" );
  error.clear();

  const QString replacementPassword = QStringLiteral( "mtpl-replacement-password" );
  QVERIFY2( manager->resetMasterPassword(
              replacementPassword, mInitialPassword, false ),
            "Could not change the isolated authentication master password" );

  QgsMtpl::CryptoKeys restored = QgsMtplCredentialStore::rememberedKeys( &error );
  QVERIFY2( error.isEmpty(), qPrintable( error ) );
  QCOMPARE( restored.privateKey, replacementKeys.privateKey );
  QCOMPARE( restored.deviceKey, replacementKeys.deviceKey );
  restored.clear();

  QVERIFY( QgsMtplCredentialStore::clearRememberedKeys( &error ) );
  QVERIFY2( error.isEmpty(), qPrintable( error ) );
  QVERIFY( !QgsApplication::authManager()->configIds().contains( authConfigId ) );
  QVERIFY( !QgsMtplCredentialStore::hasRememberedKeys() );
  replacementKeys.clear();
  keys.clear();
}

QGSTEST_MAIN( TestMtplCredentials )
#include "test_mtpl_credentials.moc"
