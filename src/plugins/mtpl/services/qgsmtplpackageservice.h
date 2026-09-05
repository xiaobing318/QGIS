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
    enum class PtpContractFailure
    {
      None,
      ImageDecode,
      DeclaredElevation,
      DeclaredVector
    };

    using CancelCheck = std::function<bool()>;
    using ProgressCallback = std::function<void( double )>;

    //! preferPtpSidecar treats supplied PTP keys as a fallback for automatic selection or refresh.
    static QgsMtpl::ProbeResult probePath( const QString &path,
                                           const QgsMtpl::CryptoKeys &keys = QgsMtpl::CryptoKeys(),
                                           bool readMetadata = true,
                                           QgsMtpl::CredentialSource suppliedSource = QgsMtpl::CredentialSource::Explicit,
                                           const CancelCheck &cancelCheck = CancelCheck(),
                                           const ProgressCallback &progress = ProgressCallback(),
                                           bool preferPtpSidecar = false );

    //! Explicit key validation should leave preferPtpSidecar disabled.
    static bool probePackage( const QString &path,
                              QgsMtpl::PackageDescriptor &descriptor,
                              QString &error,
                              const QgsMtpl::CryptoKeys &keys = QgsMtpl::CryptoKeys(),
                              bool readMetadata = true,
                              QgsMtpl::CredentialSource suppliedSource = QgsMtpl::CredentialSource::Explicit,
                              const CancelCheck &cancelCheck = CancelCheck(),
                              const ProgressCallback &progress = ProgressCallback(),
                              bool supplementExternalDisplayMetadata = true,
                              bool inspectTileStructure = true,
                              bool preferPtpSidecar = false );

    //! Adds non-structural display fields from the TileJSON belonging to the selected file or leaf directory.
    static void applyExternalDisplayMetadata( const QString &selectionPath,
                                              bool directorySelection,
                                              QList<QgsMtpl::PackageDescriptor> &packages );

    //! Returns a machine-readable reason for a failed PTP raster contract check.
    static PtpContractFailure ptpContractFailure( const QgsMtpl::PackageDescriptor &descriptor );

    //! Returns png, jpeg, or webp when bytes have an allowed PTP image signature.
    static QByteArray ptpImageFormatFromMagic( const QByteArray &bytes );

    //! Checks image container boundaries, including PNG IEND and JPEG EOI.
    static bool ptpImageHasCompleteStructure( const QByteArray &bytes );

    //! Fully decodes an allowed PTP image and checks its dimensions against the package header.
    static bool validatePtpImagePayload( const QByteArray &bytes, int expectedTileSize, QString &error );

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
                      QgsMtpl::CredentialSource suppliedSource,
                      bool preferPtpSidecar = false );
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
    bool mPreferPtpSidecar = false;
    QgsMtpl::ProbeResult mResult;
};

#endif // QGSMTPLPACKAGESERVICE_H
