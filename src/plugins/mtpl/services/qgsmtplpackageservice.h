/***************************************************************************
  qgsmtplpackageservice.h
  -----------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
  Non-mutating package discovery and metadata inspection.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMTPLPACKAGESERVICE_H
#define QGSMTPLPACKAGESERVICE_H

#include "../qgsmtplpackage.h"
#include "qgstaskmanager.h"

#include <functional>

class QgsMtplPackageService
{
  public:
    using CancelCheck = std::function<bool()>;
    using ProgressCallback = std::function<void( double )>;

    static QgsMtpl::ProbeResult probePath( const QString &path,
                                           const QgsMtpl::CryptoKeys &keys = QgsMtpl::CryptoKeys(),
                                           bool readMetadata = true,
                                           QgsMtpl::CredentialSource suppliedSource = QgsMtpl::CredentialSource::Explicit,
                                           const CancelCheck &cancelCheck = CancelCheck(),
                                           const ProgressCallback &progress = ProgressCallback() );

    static bool probePackage( const QString &path,
                              QgsMtpl::PackageDescriptor &descriptor,
                              QString &error,
                              const QgsMtpl::CryptoKeys &keys = QgsMtpl::CryptoKeys(),
                              bool readMetadata = true,
                              QgsMtpl::CredentialSource suppliedSource = QgsMtpl::CredentialSource::Explicit,
                              const CancelCheck &cancelCheck = CancelCheck(),
                              const ProgressCallback &progress = ProgressCallback() );

    static QStringList supportedSuffixes();
    static QStringList supportedNameFilters();
};

//! Background-only MTPL and file I/O probe. No provider registry access is performed.
class QgsMtplProbeTask final : public QgsTask
{
  public:
    QgsMtplProbeTask( const QString &path,
                      const QgsMtpl::CryptoKeys &keys,
                      bool readMetadata,
                      QgsMtpl::CredentialSource suppliedSource );
    ~QgsMtplProbeTask() override;

    const QgsMtpl::ProbeResult &result() const { return mResult; }
    static void cancelAndWaitForAllActiveTasks();

  protected:
    bool run() override;

  private:
    QString mPath;
    QgsMtpl::CryptoKeys mKeys;
    bool mReadMetadata = true;
    QgsMtpl::CredentialSource mSuppliedSource = QgsMtpl::CredentialSource::None;
    QgsMtpl::ProbeResult mResult;
};

#endif // QGSMTPLPACKAGESERVICE_H
