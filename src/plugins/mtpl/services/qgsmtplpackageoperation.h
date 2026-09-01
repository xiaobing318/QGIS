/***************************************************************************
  qgsmtplpackageoperation.h
  -------------------------
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

#ifndef QGSMTPLPACKAGEOPERATION_H
#define QGSMTPLPACKAGEOPERATION_H

#include "../qgsmtplpackage.h"
#include "qgsmtplkeysidecar.h"

#include "qgstaskmanager.h"

#include <functional>

namespace QgsMtpl
{

  enum class PackageOperationType
  {
    Convert,
    CreateTilePackage,
    CreateSfpPackage
  };

  struct ConversionInput
  {
    QString path;
    CryptoKeys keys;
    //! Non-empty when the requested operation is already satisfied for this source.
    QString skipReason;
  };

  struct PackageOperationRequest
  {
    PackageOperationType type = PackageOperationType::Convert;

    //! Convert accepts one input for outputPath, or multiple inputs for outputDirectory.
    QList<ConversionInput> inputs;
    //! Optional source path to probe in the operation task before conversion.
    //! This keeps directory, SFP, and key-sidecar I/O off the GUI thread.
    QString conversionSourcePath;
    CryptoKeys suppliedSourceKeys;
    CredentialSource suppliedCredentialSource = CredentialSource::None;
    //! Source tile tree or source SFP directory for creation operations.
    QString sourcePath;
    QString outputPath;
    QString outputDirectory;

    PackageFormat outputFormat = PackageFormat::Unknown;
    int tileSize = 256;
    QByteArray metadata = QByteArrayLiteral( "{}" );

    bool encryptOutput = false;
    //! Re-key operations preserve each entry's plain/encrypted mode in mixed SFP packages.
    bool preserveSfpMixedStorage = false;
    KeyMaterial outputKeys;
    bool generateOutputKeys = true;
    bool writeSidecar = true;
  };

  enum class PackageOperationItemStatus
  {
    Succeeded,
    Skipped,
    Failed
  };

  struct PackageOperationItemResult
  {
    QString inputPath;
    QString outputPath;
    PackageOperationItemStatus status = PackageOperationItemStatus::Failed;
    QString message;
  };

  struct PackageOperationResult
  {
    //! TRUE when the operation was not canceled and at least one output was
    //! committed. Batch results can still contain skipped or failed items.
    bool ok = false;
    bool canceled = false;
    QString error;
    QStringList outputPaths;
    QString sidecarPath;
    KeyMaterial outputKeys;
    QList<PackageOperationItemResult> items;
    //! TRUE when explicitly entered source keys were verified against at least one encrypted package.
    bool explicitSourceKeysVerified = false;
  };

  class PackageOperations
  {
    public:
      using CancelCheck = std::function<bool()>;
      using ProgressCallback = std::function<void( double )>;

      static PackageOperationResult execute( const PackageOperationRequest &request,
                                             const CancelCheck &isCanceled = CancelCheck(),
                                             const ProgressCallback &progress = ProgressCallback() );
  };

} // namespace QgsMtpl

class QgsMtplPackageOperationTask final : public QgsTask
{
  public:
    explicit QgsMtplPackageOperationTask( const QgsMtpl::PackageOperationRequest &request );
    ~QgsMtplPackageOperationTask() override;

    const QgsMtpl::PackageOperationResult &result() const { return mResult; }
    //! Cancels all MTPL operation tasks and does not return until they are destroyed.
    static void cancelAndWaitForAllActiveTasks();

  protected:
    bool run() override;

  private:
    QgsMtpl::PackageOperationRequest mRequest;
    QgsMtpl::PackageOperationResult mResult;
};

#endif // QGSMTPLPACKAGEOPERATION_H
