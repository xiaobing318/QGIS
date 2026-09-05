/***************************************************************************
  qgsmtplpluginlayer.h
  --------------------
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

#ifndef QGSMTPLPLUGINLAYER_H
#define QGSMTPLPLUGINLAYER_H

#include "qgsmtplpackage.h"
#include "services/qgsmtpltileset.h"

#include "qgspluginlayer.h"
#include "qgspluginlayerregistry.h"
#include "qgscoordinatetransformcontext.h"
#include "qgstiles.h"

#include <QList>
#include <QHash>

#include <memory>

class QMutex;
class QgsMtplStructureValidationCache;

class QgsMtplPluginLayer final : public QgsPluginLayer
{
    Q_OBJECT

  public:
    static QString layerTypeKey();

    explicit QgsMtplPluginLayer( const QgsMtpl::PackageDescriptor &descriptor = QgsMtpl::PackageDescriptor(), const QgsMtpl::CryptoKeys &keys = QgsMtpl::CryptoKeys() );
    QgsMtplPluginLayer( const QgsMtpl::TileDatasetDescriptor &dataset, const QList<QgsMtpl::CryptoKeys> &packageKeys );
    QgsMtplPluginLayer( const QgsMtpl::TileDatasetDescriptor &dataset, const QgsMtpl::CryptoKeys &sharedKeys );
    ~QgsMtplPluginLayer() override;

    QgsMtplPluginLayer *clone() const override;
    QgsMapLayerRenderer *createMapRenderer( QgsRenderContext &rendererContext ) override;
    void reload() override;
    bool isSpatial() const override;
    bool readSymbology( const QDomNode &node, QString &errorMessage, QgsReadWriteContext &context, StyleCategories categories = AllStyleCategories ) override;
    bool writeSymbology( QDomNode &node, QDomDocument &document, QString &errorMessage, const QgsReadWriteContext &context, StyleCategories categories = AllStyleCategories ) const override;
    void setTransformContext( const QgsCoordinateTransformContext &transformContext ) override;

    const QgsMtpl::PackageDescriptor &descriptor() const;
    void setDescriptor( const QgsMtpl::PackageDescriptor &descriptor );
    void setCryptoKeys( const QgsMtpl::CryptoKeys &keys );
    void clearCryptoKeys();
    bool hasCryptoKeys() const;
    bool isTileDataset() const;
    const QgsMtpl::TileDatasetDescriptor *tileDataset() const;
    bool replaceTileDataset( const QgsMtpl::TileDatasetDescriptor &dataset,
                             const QList<QgsMtpl::CryptoKeys> &packageKeys,
                             QString *error = nullptr );
    const QgsTileMatrixSet &tileMatrixSet() const;
    quint64 structuralValidationAttemptCount() const;

  protected:
    bool readXml( const QDomNode &layerNode, QgsReadWriteContext &context ) override;
    bool writeXml( QDomNode &layerNode, QDomDocument &document, const QgsReadWriteContext &context ) const override;
    QString encodedSource( const QString &source, const QgsReadWriteContext &context ) const override;
    QString decodedSource( const QString &source, const QString &dataProvider, const QgsReadWriteContext &context ) const override;

  private:
    bool restoreLegacyPtpDataset();
    void applyDescriptor();
    void applyDataset();

    QgsMtpl::PackageDescriptor mDescriptor;
    QgsMtpl::CryptoKeys mKeys;
    std::shared_ptr<const QgsMtpl::TileDatasetDescriptor> mDataset;
    QgsTileMatrixSet mTileMatrixSet;
    QList<QgsMtpl::CryptoKeys> mDatasetKeys;
    QgsCoordinateTransformContext mTransformContext;
    std::shared_ptr<QgsMtplStructureValidationCache> mStructureValidationCache;
    std::shared_ptr<QMutex> mRenderErrorMutex;
    std::shared_ptr<QHash<QString, qint64>> mRenderErrorLastReported;
};

class QgsMtplPluginLayerType final : public QgsPluginLayerType
{
  public:
    QgsMtplPluginLayerType();

    QgsPluginLayer *createLayer() override;
    QgsPluginLayer *createLayer( const QString &uri ) override;
    bool showLayerProperties( QgsPluginLayer *layer ) override;
};

#endif // QGSMTPLPLUGINLAYER_H
