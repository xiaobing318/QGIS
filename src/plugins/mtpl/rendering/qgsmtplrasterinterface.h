/***************************************************************************
  qgsmtplrasterinterface.h
  ------------------------
  Raster bridge for a logical MTPL tile dataset.
  copyright            : (C) 2026 QGIS Project
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMTPLRASTERINTERFACE_H
#define QGSMTPLRASTERINTERFACE_H

#include "../services/qgsmtpltileset.h"

#include "qgsrasterdataprovider.h"
#include "qgsrasterinterface.h"
#include "qgstiles.h"

#include <QList>
#include <QStringList>

#include <functional>
#include <memory>

/**
 * Thread-safe cache for the structural validation of PTP packages.
 *
 * The cache stores validation outcomes only. MTPL reader handles remain owned
 * by individual QgsMtplRasterInterface instances and are never shared between
 * renderer threads. Entries are scoped by package identity and the observed
 * file stamp, so replacing a package causes a new validation automatically.
 */
class QgsMtplStructureValidationCache final
{
  public:
    using Validator = std::function<bool( QString &error, bool &wasCanceled )>;

    QgsMtplStructureValidationCache();
    ~QgsMtplStructureValidationCache();

    static QString packageIdentity( int packageIndex,
                                    const QString &path,
                                    const QString &structureSummary = QString() );

    //! Stable digest of the package structure expected by a dataset snapshot.
    static QString structureSummary( const QgsMtpl::TileDatasetDescriptor &dataset,
                                     int packageIndex );

    bool validate( const QString &packageIdentity,
                   quint64 fileSize,
                   qint64 fileLastModifiedMs,
                   QgsRasterBlockFeedback *feedback,
                   const Validator &validator,
                   QString &error,
                   bool &wasCanceled );

    //! Removes all cached outcomes without resetting the diagnostic counter.
    void invalidate();

    //! Removes the cached outcome for one package identity.
    void invalidatePackage( const QString &packageIdentity );

    //! Returns the number of validators actually executed since construction.
    quint64 validationAttemptCount() const;

    //! Returns the latest successfully validated stamp for a package.
    bool validatedFileStamp( const QString &packageIdentity,
                             quint64 &fileSize,
                             qint64 &fileLastModifiedMs ) const;

  private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * Private raster source used by an MTPL plugin-layer renderer.
 *
 * The object owns a value snapshot of all dataset addressing information.
 * Reader handles and decoded image cache entries are created per instance, so
 * a renderer thread never shares an MTPL reader with another renderer.
 */
class QgsMtplRasterInterface final : public QgsRasterInterface
{
  public:
    QgsMtplRasterInterface( const QgsMtpl::TileDatasetDescriptor &dataset,
                            const QgsTileMatrixSet &matrixSet,
                            const QList<QgsMtpl::CryptoKeys> &packageKeys,
                            int zoomLevel,
                            const std::shared_ptr<QgsMtplStructureValidationCache> &validationCache = nullptr );
    ~QgsMtplRasterInterface() override;

    QgsMtplRasterInterface *clone() const override;
    Qgis::DataType dataType( int bandNo ) const override;
    Qgis::DataType sourceDataType( int bandNo ) const override;
    int bandCount() const override;
    QgsRectangle extent() const override;
    int xBlockSize() const override;
    int yBlockSize() const override;
    int xSize() const override;
    int ySize() const override;
    QgsRasterBlock *block( int bandNo,
                           const QgsRectangle &extent,
                           int width,
                           int height,
                           QgsRasterBlockFeedback *feedback = nullptr ) override;

    QStringList errors() const;

  private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * Private provider adapter which exposes the MTPL source extent to the raster projector.
 *
 * This provider is not registered with QGIS. A renderer creates and destroys it
 * in its own thread, together with the owned raster interface. The provider has
 * no input so sourceInput() identifies this adapter as the source of the pipe.
 */
class QgsMtplRasterSourceProvider final : public QgsRasterDataProvider
{
  public:
    QgsMtplRasterSourceProvider( const QgsMtpl::TileDatasetDescriptor &dataset,
                                 const QgsTileMatrixSet &matrixSet,
                                 const QList<QgsMtpl::CryptoKeys> &packageKeys,
                                 int zoomLevel,
                                 const std::shared_ptr<QgsMtplStructureValidationCache> &validationCache = nullptr );
    ~QgsMtplRasterSourceProvider() override;

    QgsMtplRasterSourceProvider *clone() const override;
    Qgis::RasterInterfaceCapabilities capabilities() const override;
    QgsCoordinateReferenceSystem crs() const override;
    bool isValid() const override;
    QString name() const override;
    QString description() const override;
    QString lastErrorTitle() override;
    QString lastError() override;
    Qgis::DataType dataType( int bandNo ) const override;
    Qgis::DataType sourceDataType( int bandNo ) const override;
    int bandCount() const override;
    QgsRectangle extent() const override;
    int xBlockSize() const override;
    int yBlockSize() const override;
    int xSize() const override;
    int ySize() const override;
    QgsRasterBlock *block( int bandNo,
                           const QgsRectangle &extent,
                           int width,
                           int height,
                           QgsRasterBlockFeedback *feedback = nullptr ) override;

    QStringList errors() const;

  private:
    QgsMtplRasterSourceProvider( std::unique_ptr<QgsMtplRasterInterface> source,
                                 const QgsCoordinateReferenceSystem &crs );

    std::unique_ptr<QgsMtplRasterInterface> mSource;
    QgsCoordinateReferenceSystem mCrs;
};

#endif // QGSMTPLRASTERINTERFACE_H
