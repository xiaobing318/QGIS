/***************************************************************************
  qgsmtplcredentialstore.h
  ------------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
  Profile-scoped credential persistence for the built-in MTPL plugin.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMTPLCREDENTIALSTORE_H
#define QGSMTPLCREDENTIALSTORE_H

#include "../qgsmtplpackage.h"

/**
 * Narrow persistence interface used by QgsMtplCredentialStore.
 *
 * The interface deliberately exposes no QGIS authentication classes. This
 * keeps callers independent of the production backend and permits isolated
 * tests which cannot open a user's authentication database.
 */
class QgsMtplCredentialBackend
{
  public:
    enum class LoadStatus
    {
      Success,
      NotFound,
      UnlockFailed,
      Unavailable,
      InvalidData,
      StorageError
    };

    struct LoadResult
    {
      LoadStatus status = LoadStatus::NotFound;
      QgsMtpl::CryptoKeys keys;
      QString error;

      bool succeeded() const { return status == LoadStatus::Success; }
    };

    virtual ~QgsMtplCredentialBackend() = default;

    //! Checks only metadata and must not request the authentication master password.
    virtual bool contains( const QString &credentialId ) const = 0;

    /**
     * Loads and decrypts a credential.
     *
     * When \a allowUnlock is FALSE, implementations must not request a master
     * password and may only use an authentication database which is already
     * unlocked.
     */
    virtual LoadResult load( const QString &credentialId, bool allowUnlock ) = 0;

    //! Creates a new encrypted credential and returns its new identifier.
    virtual bool store( const QgsMtpl::CryptoKeys &keys,
                        QString &credentialId,
                        QString &error ) = 0;

    //! Removes a credential. Removing an already absent credential succeeds.
    virtual bool remove( const QString &credentialId, QString &error ) = 0;
};

/**
 * Stores one optional MTPL key pair in the QGIS Authentication Manager.
 *
 * Keys are session-only unless saveManualKeys() is explicitly called. Only
 * the non-secret authentication configuration identifier is kept in
 * QgsSettings. Obsolete plaintext settings are erased without migration.
 */
class QgsMtplCredentialStore
{
  public:
    //! Compatibility accessor. TRUE means that a secure stored key pair exists.
    static bool rememberEnabled();

    /**
     * Compatibility setter. Disabling removes the stored pair. Enabling does
     * not persist anything, because secure persistence is always explicit.
     */
    static void setRememberEnabled( bool enabled );

    //! Checks for stored credential metadata without unlocking it.
    static bool hasRememberedKeys( QString *error = nullptr );

    //! Lazily unlocks and returns the securely stored pair.
    static QgsMtpl::CryptoKeys rememberedKeys( QString *error = nullptr );

    //! Returns the stored pair only when Authentication Manager is already unlocked.
    static QgsMtpl::CryptoKeys rememberedKeysIfUnlocked( QString *error = nullptr );

    /**
     * Securely persists a validated key pair in the Authentication Manager.
     * The previous pair remains active if replacement cannot be completed.
     */
    static bool saveManualKeys( const QgsMtpl::CryptoKeys &keys,
                                const QString &lastPath,
                                QString &error );

    static QString lastPath();
    static void setLastPath( const QString &path );

    //! Removes the securely stored pair and reports backend failures.
    static bool clearRememberedKeys( QString *error = nullptr );
    static void clear();

    /**
     * Installs a non-owning backend override for isolated tests.
     * Passing nullptr restores the production Authentication Manager backend.
     */
    static void setBackendForTesting( QgsMtplCredentialBackend *backend );

    //! Makes the next legacy-cleanup verification fail. Tests only.
    static void failNextLegacyCleanupForTesting();

    //! Makes the next stored-authentication-reference write fail. Tests only.
    static void failNextCredentialReferenceWriteForTesting();
};

#endif // QGSMTPLCREDENTIALSTORE_H
