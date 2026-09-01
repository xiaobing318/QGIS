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

#include "qgspluginlayer.h"
#include "qgspluginlayerregistry.h"

class QgsMtplPluginLayer final : public QgsPluginLayer
{
    Q_OBJECT

  public:
    static QString layerTypeKey();

    explicit QgsMtplPluginLayer( const QgsMtpl::PackageDescriptor &descriptor = QgsMtpl::PackageDescriptor(), const QgsMtpl::CryptoKeys &keys = QgsMtpl::CryptoKeys() );
    ~QgsMtplPluginLayer() override;

    QgsMtplPluginLayer *clone() const override;
    QgsMapLayerRenderer *createMapRenderer( QgsRenderContext &rendererContext ) override;
    bool isSpatial() const override;
    bool readSymbology( const QDomNode &node, QString &errorMessage, QgsReadWriteContext &context, StyleCategories categories = AllStyleCategories ) override;
    bool writeSymbology( QDomNode &node, QDomDocument &document, QString &errorMessage, const QgsReadWriteContext &context, StyleCategories categories = AllStyleCategories ) const override;
    void setTransformContext( const QgsCoordinateTransformContext &transformContext ) override;

    const QgsMtpl::PackageDescriptor &descriptor() const;
    void setDescriptor( const QgsMtpl::PackageDescriptor &descriptor );
    void setCryptoKeys( const QgsMtpl::CryptoKeys &keys );
    void clearCryptoKeys();
    bool hasCryptoKeys() const;

  protected:
    bool readXml( const QDomNode &layerNode, QgsReadWriteContext &context ) override;
    bool writeXml( QDomNode &layerNode, QDomDocument &document, const QgsReadWriteContext &context ) const override;
    QString encodedSource( const QString &source, const QgsReadWriteContext &context ) const override;
    QString decodedSource( const QString &source, const QString &dataProvider, const QgsReadWriteContext &context ) const override;

  private:
    void applyDescriptor();

    QgsMtpl::PackageDescriptor mDescriptor;
    QgsMtpl::CryptoKeys mKeys;
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
