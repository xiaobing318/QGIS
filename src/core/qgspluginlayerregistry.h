/***************************************************************************
                    qgspluginlayerregistry.cpp - class for
                    registering plugin layer creators
                             -------------------
    begin                : Mon Nov 30 2009
    copyright            : (C) 2009 by Mathias Walker, Sourcepole
    email                : mwa at sourcepole.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*
* 杨小兵-2024-03-26
  定义了两个关键的类，`QgsPluginLayerType`和`QgsPluginLayerRegistry`，这两个类在QGIS框架中用于管理和注册插件图层类型。

一、`QgsPluginLayerType`
  这是一个为特定插件图层创建的基类。它定义了一些基本的接口和功能，允许开发者为自己的QGIS插件创建特定类型的图层。每种插件图层类型都应该继承此类并实现相应的方法。
- **构造函数** (`QgsPluginLayerType(const QString &name)`): 用于创建一个新的插件图层类型实例，需要一个图层类型的名称。
- **`name()`方法**: 返回图层类型的名称。
- **`createLayer()`方法**: 创建并返回一个新的图层实例。如果创建失败，则返回`NULLPTR`。这是一个虚函数，意味着继承此类的子类需要实现这个方法。
- **`createLayer(const QString &uri)`方法**: 使用给定的URI创建并返回一个新的图层实例。这个方法允许通过特定的URI来创建图层，例如从一个文件或网络位置。这个方法
自QGIS 2.10版本引入。
- **`showLayerProperties(QgsPluginLayer *layer)`方法**: 显示插件图层的属性对话框。如果无法显示属性对话框，则返回`FALSE`。这个方法也是虚函数，需要在子类中实现。

二、`QgsPluginLayerRegistry`
  这个类提供了一个注册表，用于注册和管理所有的插件图层类型。通常不会直接创建这个类的实例，而是通过`QgsApplication::pluginLayerRegistry()`来访问。
- **构造函数和析构函数**: 构造函数初始化一个空的注册表，而析构函数则负责清理注册表中的资源。
- **`addPluginLayerType(QgsPluginLayerType *pluginLayerType)`方法**: 添加一个新的插件图层类型到注册表中，并取得对该类型的所有权。如果成功添加，则返回`TRUE`。
- **`removePluginLayerType(const QString &typeName)`方法**: 从注册表中移除一个插件图层类型。如果成功移除，则返回`TRUE`。
- **`pluginLayerType(const QString &typeName)`方法**: 根据图层类型名称返回一个`QgsPluginLayerType`的实例，如果该类型不存在，则返回`NULLPTR`。
- **`createLayer(const QString &typeName, const QString &uri = QString())`方法**: 根据类型名称和可选的URI创建一个新的插件图层。如果找到相应的插件并成功创建
图层，则返回图层实例；否则返回`NULLPTR`。
  通过这两个类的定义和实现，QGIS框架支持扩展性和灵活性，允许第三方开发者通过插件来增加新的图层类型，丰富QGIS的功能。

*/
#ifndef QGSPLUGINLAYERREGSITRY_H
#define QGSPLUGINLAYERREGSITRY_H

#include <QMap>
#include "qgis_sip.h"
#include <QDomNode>

#include "qgis_core.h"

class QgsPluginLayer;

/**
 * \ingroup core
 * \brief Class for creating plugin specific layers
*/
class CORE_EXPORT QgsPluginLayerType
{
  public:

    QgsPluginLayerType( const QString &name );
    virtual ~QgsPluginLayerType() = default;

    QString name() const;

    //! Returns new layer of this type. Return NULLPTR on error
    virtual QgsPluginLayer *createLayer() SIP_FACTORY;

    /**
     * Returns new layer of this type, using layer URI (specific to this plugin layer type). Return NULLPTR on error.
     * \since QGIS 2.10
     */
    virtual QgsPluginLayer *createLayer( const QString &uri ) SIP_FACTORY;

    //! Show plugin layer properties dialog. Return FALSE if the dialog cannot be shown.
    virtual bool showLayerProperties( QgsPluginLayer *layer );

  protected:
    QString mName;
};

//=============================================================================

/**
 * \ingroup core
 * \brief A registry of plugin layers types.
 *
 * QgsPluginLayerRegistry is not usually directly created, but rather accessed through
 * QgsApplication::pluginLayerRegistry().
*/
class CORE_EXPORT QgsPluginLayerRegistry
{
  public:

    /**
     * Constructor for QgsPluginLayerRegistry.
     */
    QgsPluginLayerRegistry() = default;
    ~QgsPluginLayerRegistry();

    //! QgsPluginLayerRegistry cannot be copied.
    QgsPluginLayerRegistry( const QgsPluginLayerRegistry &rh ) = delete;
    //! QgsPluginLayerRegistry cannot be copied.
    QgsPluginLayerRegistry &operator=( const QgsPluginLayerRegistry &rh ) = delete;

    /**
     * List all known layer types
     */
    QStringList pluginLayerTypes();

    //! Add plugin layer type (take ownership) and return TRUE on success
    bool addPluginLayerType( QgsPluginLayerType *pluginLayerType SIP_TRANSFER );

    //! Remove plugin layer type and return TRUE on success
    bool removePluginLayerType( const QString &typeName );

    //! Returns plugin layer type metadata or NULLPTR if doesn't exist
    QgsPluginLayerType *pluginLayerType( const QString &typeName );

    /**
     * Returns new layer if corresponding plugin has been found else returns NULLPTR.
     * \note parameter uri has been added in QGIS 2.10
     */
    QgsPluginLayer *createLayer( const QString &typeName, const QString &uri = QString() ) SIP_FACTORY;

  private:
#ifdef SIP_RUN
    QgsPluginLayerRegistry( const QgsPluginLayerRegistry &rh );
#endif

    typedef QMap<QString, QgsPluginLayerType *> PluginLayerTypes;

    PluginLayerTypes mPluginLayerTypes;
};

#endif // QGSPLUGINLAYERREGSITRY_H
