/***************************************************************************
  qgsmtplcredentialstore.cpp
  --------------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmtplcredentialstore.h"

#include "qgsapplication.h"
#include "qgsauthconfig.h"
#include "qgsauthconfigurationstorage.h"
#include "qgsauthconfigurationstorageregistry.h"
#include "qgsauthmanager.h"
#include "qgssettings.h"

#include <QObject>
#include <QSettings>
#include <algorithm>
#include <utility>

namespace
{
  const QString sAuthConfigKey = QStringLiteral( "mtpl/credentials/authcfg" );
  const QString sLegacyRememberKey = QStringLiteral( "mtpl/credentials/remember" );
  const QString sLegacyPrivateKey = QStringLiteral( "mtpl/credentials/privateKey" );
  const QString sLegacyDeviceKey = QStringLiteral( "mtpl/credentials/deviceKey" );
  const QString sLastPathKey = QStringLiteral( "mtpl/lastPath" );
  const QString sAuthMethodKey = QStringLiteral( "Mtpl" );
  const QString sPrivateConfigKey = QStringLiteral( "privateKeyBase64" );
  const QString sDeviceConfigKey = QStringLiteral( "deviceKeyHex" );
  bool sFailNextLegacyCleanup = false;
  bool sFailNextCredentialReferenceWrite = false;

  bool removeLegacyPlaintextValues( QString &error )
  {
    if ( std::exchange( sFailNextLegacyCleanup, false ) )
    {
      error = QObject::tr( "无法确认旧版 MTPL 明文密钥已从设置中删除。" );
      return false;
    }

    QgsSettings settings;
    settings.remove( sLegacyRememberKey, QgsSettings::Section::Plugins );
    settings.remove( sLegacyPrivateKey, QgsSettings::Section::Plugins );
    settings.remove( sLegacyDeviceKey, QgsSettings::Section::Plugins );
    settings.sync();

    QSettings persistedSettings( settings.fileName(), QSettings::defaultFormat() );
    persistedSettings.sync();
    const QStringList legacyKeys = {
      settings.prefixedKey( sLegacyRememberKey, QgsSettings::Section::Plugins ),
      settings.prefixedKey( sLegacyPrivateKey, QgsSettings::Section::Plugins ),
      settings.prefixedKey( sLegacyDeviceKey, QgsSettings::Section::Plugins )
    };
    const bool removed = std::none_of( legacyKeys.cbegin(), legacyKeys.cend(),
                                      [&persistedSettings]( const QString &key )
    {
      return persistedSettings.contains( key );
    } ) &&
      !settings.contains( sLegacyRememberKey, QgsSettings::Section::Plugins ) &&
      !settings.contains( sLegacyPrivateKey, QgsSettings::Section::Plugins ) &&
      !settings.contains( sLegacyDeviceKey, QgsSettings::Section::Plugins );
    if ( persistedSettings.status() != QSettings::NoError || !removed )
    {
      error = QObject::tr( "无法确认旧版 MTPL 明文密钥已从设置中删除。" );
      return false;
    }
    error.clear();
    return true;
  }

  QString storedCredentialId()
  {
    const QgsSettings settings;
    return settings.value( sAuthConfigKey, QString(), QgsSettings::Section::Plugins ).toString().trimmed();
  }

  bool setStoredCredentialId( const QString &credentialId, QString &error )
  {
    if ( std::exchange( sFailNextCredentialReferenceWrite, false ) )
    {
      error = QObject::tr( "无法安全更新 MTPL 认证配置引用。" );
      return false;
    }

    QgsSettings settings;
    if ( credentialId.isEmpty() )
      settings.remove( sAuthConfigKey, QgsSettings::Section::Plugins );
    else
      settings.setValue( sAuthConfigKey, credentialId, QgsSettings::Section::Plugins );
    settings.sync();

    QSettings persistedSettings( settings.fileName(), QSettings::defaultFormat() );
    persistedSettings.sync();
    const QString persistedKey = settings.prefixedKey( sAuthConfigKey, QgsSettings::Section::Plugins );
    const bool persisted = credentialId.isEmpty()
                             ? !persistedSettings.contains( persistedKey )
                             : persistedSettings.value( persistedKey ).toString().trimmed() == credentialId;
    if ( persistedSettings.status() != QSettings::NoError || !persisted || storedCredentialId() != credentialId )
    {
      error = QObject::tr( "无法安全更新 MTPL 认证配置引用。" );
      return false;
    }
    error.clear();
    return true;
  }

  class QgsMtplAuthCredentialBackend final : public QgsMtplCredentialBackend
  {
    public:
      bool contains( const QString &credentialId ) const override
      {
        QgsAuthManager *manager = QgsApplication::authManager();
        if ( credentialId.isEmpty() || !manager || !manager->ensureInitialized() ||
             manager->isDisabled() || !manager->configIds().contains( credentialId ) )
          return false;

        QgsAuthMethodConfig config;
        return manager->loadAuthenticationConfig( credentialId, config, false ) &&
               config.method() == sAuthMethodKey;
      }

      LoadResult load( const QString &credentialId, bool allowUnlock ) override
      {
        LoadResult result;
        if ( credentialId.isEmpty() )
          return result;

        QgsAuthManager *manager = QgsApplication::authManager();
        if ( !manager || !manager->ensureInitialized() || manager->isDisabled() )
        {
          result.status = LoadStatus::Unavailable;
          result.error = manager ? manager->disabledMessage()
                                 : QObject::tr( "QGIS 认证管理器不可用。" );
          return result;
        }

        if ( !manager->configIds().contains( credentialId ) )
        {
          result.status = LoadStatus::NotFound;
          result.error = QObject::tr( "安全保存的 MTPL 密钥不存在。" );
          return result;
        }

        QgsAuthMethodConfig baseConfig;
        if ( !manager->loadAuthenticationConfig( credentialId, baseConfig, false ) )
        {
          result.status = LoadStatus::StorageError;
          result.error = QObject::tr( "无法读取安全保存的 MTPL 密钥配置。" );
          return result;
        }
        if ( baseConfig.method() != sAuthMethodKey )
        {
          result.status = LoadStatus::InvalidData;
          result.error = QObject::tr( "安全保存的认证配置不是 MTPL 密钥。" );
          return result;
        }

        if ( !allowUnlock && !manager->masterPasswordIsSet() )
        {
          result.status = LoadStatus::UnlockFailed;
          result.error = QObject::tr( "QGIS 认证数据库尚未解锁，已保持 MTPL 图层锁定。" );
          return result;
        }

        QgsAuthMethodConfig fullConfig;
        if ( !manager->loadAuthenticationConfig( credentialId, fullConfig, true ) )
        {
          result.status = LoadStatus::UnlockFailed;
          result.error = QObject::tr( "未能解锁 QGIS 认证数据库，已忽略安全保存的 MTPL 密钥。" );
          return result;
        }

        result.keys.privateKey = fullConfig.config( sPrivateConfigKey ).toLatin1();
        result.keys.deviceKey = fullConfig.config( sDeviceConfigKey ).toLatin1();
        QString validationError;
        if ( !result.keys.isValid( &validationError ) )
        {
          result.keys.clear();
          result.status = LoadStatus::InvalidData;
          result.error = QObject::tr( "安全保存的 MTPL 密钥无效：%1" ).arg( validationError );
          return result;
        }

        result.status = LoadStatus::Success;
        return result;
      }

      bool store( const QgsMtpl::CryptoKeys &keys,
                  QString &credentialId,
                  QString &error ) override
      {
        credentialId.clear();
        QgsAuthManager *manager = QgsApplication::authManager();
        if ( !manager || !manager->ensureInitialized() || manager->isDisabled() )
        {
          error = manager ? manager->disabledMessage()
                          : QObject::tr( "QGIS 认证管理器不可用。" );
          return false;
        }

        QgsAuthConfigurationStorageRegistry *registry = manager->authConfigurationStorageRegistry();
        QgsAuthConfigurationStorage *storage = registry
          ? registry->firstReadyStorageWithCapability( Qgis::AuthConfigurationStorageCapability::CreateConfiguration )
          : nullptr;
        if ( !storage )
        {
          error = QObject::tr( "没有可写的 QGIS 认证存储。" );
          return false;
        }
        if ( !storage->isEncrypted() )
        {
          error = QObject::tr( "默认 QGIS 认证存储未启用加密，拒绝保存 MTPL 密钥。" );
          return false;
        }

        QgsAuthMethodConfig config( sAuthMethodKey, 1 );
        config.setName( QObject::tr( "MTPL 数据包密钥" ) );
        config.setUri( QStringLiteral( "mtpl://credentials" ) );
        config.setConfig( sPrivateConfigKey, QString::fromLatin1( keys.privateKey ) );
        config.setConfig( sDeviceConfigKey, QString::fromLatin1( keys.deviceKey ) );
        if ( !manager->storeAuthenticationConfig( config ) )
        {
          error = QObject::tr( "无法将 MTPL 密钥写入 QGIS 认证数据库。" );
          return false;
        }

        credentialId = config.id();
        error.clear();
        return true;
      }

      bool remove( const QString &credentialId, QString &error ) override
      {
        error.clear();
        if ( credentialId.isEmpty() )
          return true;

        QgsAuthManager *manager = QgsApplication::authManager();
        if ( !manager || !manager->ensureInitialized() || manager->isDisabled() )
        {
          error = manager ? manager->disabledMessage()
                          : QObject::tr( "QGIS 认证管理器不可用。" );
          return false;
        }
        if ( !manager->configIds().contains( credentialId ) )
          return true;

        QgsAuthMethodConfig config;
        if ( !manager->loadAuthenticationConfig( credentialId, config, false ) )
        {
          error = QObject::tr( "无法确认待删除认证配置属于 MTPL。" );
          return false;
        }
        if ( config.method() != sAuthMethodKey )
          return true;

        if ( manager->removeAuthenticationConfig( credentialId ) )
          return true;

        error = QObject::tr( "无法从 QGIS 认证数据库删除 MTPL 密钥。" );
        return false;
      }
  };

  QgsMtplCredentialBackend *sBackendOverride = nullptr;

  QgsMtplCredentialBackend &credentialBackend()
  {
    static QgsMtplAuthCredentialBackend sBackend;
    return sBackendOverride ? *sBackendOverride : sBackend;
  }
}

bool QgsMtplCredentialStore::rememberEnabled()
{
  return hasRememberedKeys();
}

void QgsMtplCredentialStore::setRememberEnabled( bool enabled )
{
  QString ignoredError;
  removeLegacyPlaintextValues( ignoredError );
  if ( !enabled )
    clearRememberedKeys();
}

bool QgsMtplCredentialStore::hasRememberedKeys( QString *error )
{
  QString cleanupError;
  if ( !removeLegacyPlaintextValues( cleanupError ) )
  {
    if ( error )
      *error = cleanupError;
    return false;
  }
  if ( error )
    error->clear();
  const QString credentialId = storedCredentialId();
  if ( credentialId.isEmpty() )
    return false;
  return credentialBackend().contains( credentialId );
}

QgsMtpl::CryptoKeys QgsMtplCredentialStore::rememberedKeys( QString *error )
{
  if ( error )
    error->clear();
  QString cleanupError;
  if ( !removeLegacyPlaintextValues( cleanupError ) )
  {
    if ( error )
      *error = cleanupError;
    return QgsMtpl::CryptoKeys();
  }

  const QString credentialId = storedCredentialId();
  if ( credentialId.isEmpty() )
    return QgsMtpl::CryptoKeys();

  QgsMtplCredentialBackend::LoadResult result = credentialBackend().load( credentialId, true );
  if ( !result.succeeded() )
  {
    if ( result.status == QgsMtplCredentialBackend::LoadStatus::NotFound )
    {
      QString ignoredError;
      setStoredCredentialId( QString(), ignoredError );
    }
    if ( error )
      *error = result.error;
    result.keys.clear();
    return QgsMtpl::CryptoKeys();
  }
  return std::move( result.keys );
}

QgsMtpl::CryptoKeys QgsMtplCredentialStore::rememberedKeysIfUnlocked( QString *error )
{
  if ( error )
    error->clear();
  QString cleanupError;
  if ( !removeLegacyPlaintextValues( cleanupError ) )
  {
    if ( error )
      *error = cleanupError;
    return QgsMtpl::CryptoKeys();
  }

  const QString credentialId = storedCredentialId();
  if ( credentialId.isEmpty() )
    return QgsMtpl::CryptoKeys();

  QgsMtplCredentialBackend::LoadResult result = credentialBackend().load( credentialId, false );
  if ( !result.succeeded() )
  {
    if ( result.status == QgsMtplCredentialBackend::LoadStatus::NotFound )
    {
      QString ignoredError;
      setStoredCredentialId( QString(), ignoredError );
    }
    if ( error )
      *error = result.error;
    result.keys.clear();
    return QgsMtpl::CryptoKeys();
  }
  return std::move( result.keys );
}

bool QgsMtplCredentialStore::saveManualKeys( const QgsMtpl::CryptoKeys &keys,
                                              const QString &lastPath,
                                              QString &error )
{
  if ( !removeLegacyPlaintextValues( error ) )
    return false;
  if ( !keys.isValid( &error ) )
    return false;

  const QString previousId = storedCredentialId();
  QString replacementId;
  if ( !credentialBackend().store( keys, replacementId, error ) || replacementId.isEmpty() )
  {
    if ( error.isEmpty() )
      error = QObject::tr( "认证后端未返回 MTPL 密钥标识。" );
    return false;
  }

  QgsMtplCredentialBackend::LoadResult verification = credentialBackend().load( replacementId, true );
  const bool verified = verification.succeeded() &&
                        verification.keys.privateKey == keys.privateKey &&
                        verification.keys.deviceKey == keys.deviceKey;
  verification.keys.clear();
  if ( !verified )
  {
    QString rollbackError;
    credentialBackend().remove( replacementId, rollbackError );
    error = verification.error.isEmpty()
      ? QObject::tr( "无法验证刚刚安全保存的 MTPL 密钥。" )
      : verification.error;
    return false;
  }

  QString pointerError;
  if ( !setStoredCredentialId( replacementId, pointerError ) )
  {
    QString restoreError;
    const bool pointerRestored = setStoredCredentialId( previousId, restoreError );
    if ( pointerRestored )
    {
      QString rollbackError;
      credentialBackend().remove( replacementId, rollbackError );
    }
    error = pointerError;
    if ( !pointerRestored )
      error += QObject::tr( " 原认证配置引用也无法恢复：%1" ).arg( restoreError );
    return false;
  }
  if ( !previousId.isEmpty() && previousId != replacementId )
  {
    QString removeError;
    if ( !credentialBackend().remove( previousId, removeError ) )
    {
      QString pointerRestoreError;
      const bool pointerRestored = setStoredCredentialId( previousId, pointerRestoreError );
      if ( pointerRestored )
      {
        QString rollbackError;
        credentialBackend().remove( replacementId, rollbackError );
      }
      error = removeError.isEmpty()
        ? QObject::tr( "无法替换原有的 MTPL 安全密钥。" )
        : removeError;
      if ( !pointerRestored )
        error += QObject::tr( " 原认证配置引用也无法恢复：%1" ).arg( pointerRestoreError );
      return false;
    }
  }

  setLastPath( lastPath );
  error.clear();
  return true;
}

QString QgsMtplCredentialStore::lastPath()
{
  QString ignoredError;
  if ( !removeLegacyPlaintextValues( ignoredError ) )
    return QString();
  const QgsSettings settings;
  return settings.value( sLastPathKey, QString(), QgsSettings::Section::Plugins ).toString();
}

void QgsMtplCredentialStore::setLastPath( const QString &path )
{
  QString ignoredError;
  if ( !removeLegacyPlaintextValues( ignoredError ) )
    return;
  QgsSettings settings;
  settings.setValue( sLastPathKey, path, QgsSettings::Section::Plugins );
  settings.sync();
}

bool QgsMtplCredentialStore::clearRememberedKeys( QString *error )
{
  if ( error )
    error->clear();
  QString cleanupError;
  if ( !removeLegacyPlaintextValues( cleanupError ) )
  {
    if ( error )
      *error = cleanupError;
    return false;
  }

  const QString credentialId = storedCredentialId();
  QString pointerError;
  if ( !setStoredCredentialId( QString(), pointerError ) )
  {
    if ( error )
      *error = pointerError;
    return false;
  }

  QString backendError;
  if ( !credentialBackend().remove( credentialId, backendError ) )
  {
    QString restoreError;
    const bool restored = setStoredCredentialId( credentialId, restoreError );
    if ( error )
    {
      *error = backendError;
      if ( !restored )
        *error += QObject::tr( " 原认证配置引用也无法恢复：%1" ).arg( restoreError );
    }
    return false;
  }
  return true;
}

void QgsMtplCredentialStore::clear()
{
  QString ignoredError;
  clearRememberedKeys( &ignoredError );
  QString ignoredCleanupError;
  removeLegacyPlaintextValues( ignoredCleanupError );
  QgsSettings settings;
  settings.remove( sLastPathKey, QgsSettings::Section::Plugins );
  settings.sync();
}

void QgsMtplCredentialStore::setBackendForTesting( QgsMtplCredentialBackend *backend )
{
  sBackendOverride = backend;
}

void QgsMtplCredentialStore::failNextLegacyCleanupForTesting()
{
  sFailNextLegacyCleanup = true;
}

void QgsMtplCredentialStore::failNextCredentialReferenceWriteForTesting()
{
  sFailNextCredentialReferenceWrite = true;
}
