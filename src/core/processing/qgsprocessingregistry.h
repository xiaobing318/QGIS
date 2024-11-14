/***************************************************************************
                         qgsprocessingregistry.h
                         ------------------------
    begin                : December 2016
    copyright            : (C) 2016 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSPROCESSINGREGISTRY_H
#define QGSPROCESSINGREGISTRY_H

#pragma region "包含头文件"
#include "qgis_core.h"
#include "qgis.h"
#include "qgsprocessingprovider.h"
#include <QMap>
#pragma endregion

#pragma region "类的前向声明"
class QgsProcessingParameterType;
class QgsProcessingAlgorithmConfigurationWidgetFactory;
#pragma endregion

#pragma region "类的介绍"
/*
* 杨小兵-2024-03-26
一、应用场景
  `QgsProcessingRegistry`类在QGIS的处理框架中扮演了一个核心的角色。这个框架用于执行各种地理信息处理任务，如空间分析、数据转换、地图生成等。具体来说，这个注册表用
于管理处理组件，这包括但不限于：
- **处理提供者**（Providers）：这些是执行具体处理算法的模块。每个提供者可能包含多个算法。
- **算法**（Algorithms）：执行具体的数据处理任务，如计算区域、路径查找、数据转换等。
- **参数和输出**（Parameters and Outputs）：算法运行所需的输入参数和它们产生的输出。

二、数学角度的解释
  从数学角度看，`QgsProcessingRegistry`可以视为一个集合，这个集合中的元素包括处理提供者、算法、参数和输出。这些元素共同构成了QGIS处理框架的数学模型，允许用户定义
和执行地理信息处理的复杂操作序列。

三、计算机实现
  在计算机实现上，`QgsProcessingRegistry`是一个类，它提供了一系列方法来注册和管理处理组件。这个类是QGIS处理框架的核心，因为它允许框架动态地添加、查询和管理不同的
处理算法和资源。具体来说，它包括了以下几个方面的功能：
- **注册和管理提供者**：提供者是实现特定处理功能的库或模块。`QgsProcessingRegistry`允许将这些提供者动态添加到QGIS中，从而扩展其处理功能。
- **查询和访问算法**：通过注册表，用户和开发者可以查询可用的处理算法，并根据需要执行这些算法。
- **管理参数和输出**：处理算法通常需要输入参数，并产生输出。注册表也涉及到这些参数和输出的管理，确保算法能够以正确的方式执行。

  通常情况下，开发者和用户不需要直接创建`QgsProcessingRegistry`实例，因为QGIS提供了一个全局可访问的处理注册表实例，可以通过`QgsApplication::processingRegistry()`
方法访问。这样的设计允许在QGIS的不同部分共享和使用相同的处理资源，从而提高了代码的重用性和整体架构的一致性。注释中提到`since QGIS 3.0`，意味着这个类是在QGIS 3.0版本
中引入的，标示着QGIS处理框架的一个重要更新，进一步增强了QGIS作为一个地理信息系统平台的能力和灵活性。
*/
/**
 * \class QgsProcessingRegistry
 * \ingroup core
 * \brief Registry for various processing components, including providers, algorithms
 * and various parameters and outputs.
 *
 * QgsProcessingRegistry is not usually directly created, but rather accessed through
 * QgsApplication::processingRegistry().
 * \since QGIS 3.0
 */
#pragma endregion
class CORE_EXPORT QgsProcessingRegistry : public QObject
{
    Q_OBJECT

#pragma region "公开成员函数"
  public:

    /**
     * Constructor for QgsProcessingRegistry.
     */
    QgsProcessingRegistry( QObject *parent SIP_TRANSFERTHIS = nullptr );

    ~QgsProcessingRegistry() override;

    //! Registry cannot be copied
    QgsProcessingRegistry( const QgsProcessingRegistry &other ) = delete;
    //! Registry cannot be copied
    QgsProcessingRegistry &operator=( const QgsProcessingRegistry &other ) = delete;

    /**
     * Gets list of available providers.
     */
    QList<QgsProcessingProvider *> providers() const { return mProviders.values(); }

    /**
     * Add a processing provider to the registry. Ownership of the provider is transferred to the registry,
     * and the provider's parent will be set to the registry.
     * Returns FALSE if the provider could not be added (eg if a provider with a duplicate ID already exists
     * in the registry).
     * Adding a provider to the registry automatically triggers the providers QgsProcessingProvider::load()
     * method to populate the provider with algorithms.
     * \see removeProvider()
     */
    bool addProvider( QgsProcessingProvider *provider SIP_TRANSFER );

    /**
     * Removes a provider implementation from the registry (the provider object is deleted).
     * Returns FALSE if the provider could not be removed (eg provider does not exist in the registry).
     * \see addProvider()
     */
    bool removeProvider( QgsProcessingProvider *provider );

    /**
     * Removes a provider implementation from the registry (the provider object is deleted).
     * Returns FALSE if the provider could not be removed (eg provider does not exist in the registry).
     * \see addProvider()
     */
    bool removeProvider( const QString &providerId );

    /**
     * Returns a matching provider by provider ID.
     */
    QgsProcessingProvider *providerById( const QString &id );

    /**
     * Returns a list of all available algorithms from registered providers.
     * \see algorithmById()
     */
    QList< const QgsProcessingAlgorithm *> algorithms() const;

    /**
     * Finds an algorithm by its ID. If no matching algorithm is found, NULLPTR
     * is returned.
     * \see algorithms()
     * \see createAlgorithmById()
     */
    const QgsProcessingAlgorithm *algorithmById( const QString &id ) const;

    /*
     * IMPORTANT: While it seems like /Factory/ would be the correct annotation here, that's not
     * the case.
     * As per Phil Thomson's advice on https://www.riverbankcomputing.com/pipermail/pyqt/2017-July/039450.html:
     *
     * "
     * /Factory/ is used when the instance returned is guaranteed to be new to Python.
     * In this case it isn't because it has already been seen when being returned by QgsProcessingAlgorithm::createInstance()
     * (However for a different sub-class implemented in C++ then it would be the first time it was seen
     * by Python so the /Factory/ on create() would be correct.)
     *
     * You might try using /TransferBack/ on create() instead - that might be the best compromise.
     * "
     */

    /**
     * Creates a new instance of an algorithm by its ID. If no matching algorithm is found, NULLPTR
     * is returned. Callers take responsibility for deleting the returned object.
     *
     * The \a configuration argument allows passing of a map of configuration settings
     * to the algorithm, allowing it to dynamically adjust its initialized parameters
     * and outputs according to this configuration. This is generally used only for
     * algorithms in a model, allowing them to adjust their behavior at run time
     * according to some user configuration.
     *
     * \see algorithms()
     * \see algorithmById()
     */
    QgsProcessingAlgorithm *createAlgorithmById( const QString &id, const QVariantMap &configuration = QVariantMap() ) const SIP_TRANSFERBACK;

    /**
     * Adds a new alias to an existing algorithm.
     *
     * This allows algorithms to be referred to by a different provider ID and algorithm name to their actual underlying provider and algorithm name.
     * It provides a mechanism to allow algorithms to be moved between providers without breaking existing scripts or plugins.
     *
     * The \a aliasId argument specifies the "fake" algorithm id (eg "fake_provider:fake_alg") by which the algorithm can
     * be referred to, and the \a actualId argument specifies the real algorithm ID for the algorithm.
     *
     * \since QGIS 3.10
     */
    void addAlgorithmAlias( const QString &aliasId, const QString &actualId );

    /**
     * Register a new parameter type for processing.
     * Ownership is transferred to the registry.
     * Will emit parameterTypeAdded.
     *
     * \see removeParameterType
     *
     * \since QGIS 3.2
     */
    bool addParameterType( QgsProcessingParameterType *type SIP_TRANSFER );

    /**
     * Unregister a custom parameter type from processing.
     * The type will be deleted.
     * Will emit parameterTypeRemoved.
     *
     * \see addParameterType
     *
     * \since QGIS 3.2
     */
    void removeParameterType( QgsProcessingParameterType *type );

    /**
     * Returns the parameter type registered for \a id.
     *
     * \since QGIS 3.2
     */
    QgsProcessingParameterType *parameterType( const QString &id ) const;

    /**
     * Returns a list with all known parameter types.
     *
     * \since QGIS 3.2
     */
    QList<QgsProcessingParameterType *> parameterTypes() const;
#pragma endregion

#pragma region "信号"
/*
* 杨小兵-2024-03-26
  在Qt框架中，`signals`关键字是用于定义信号的特殊Qt宏，它本身不遵循C++的`public`、`private`或`protected`访问控制规则。信号是Qt对象间通信的一种方式，用于通知
其他对象某个事件的发生。当你声明了一个信号，无论它被放置在类定义的哪个区域（`public`、`private`或`protected`区域），它对于所有接收对象都是可见的。这意味着，你
不能通过改变信号在类定义中的位置来限制谁可以连接到这个信号。
*/
  signals:

    //! Emitted when a provider has been added to the registry.
    void providerAdded( const QString &id );

    //! Emitted when a provider is removed from the registry
    void providerRemoved( const QString &id );

    /**
     * Emitted when a new parameter type has been added to the registry.
     *
     * \since QGIS 3.2
     */
    void parameterTypeAdded( QgsProcessingParameterType *type );

    /**
     * Emitted when a parameter type has been removed from the
     * registry and is about to be deleted.
     *
     * \since QGIS 3.2
     */
    void parameterTypeRemoved( QgsProcessingParameterType *type );
#pragma endregion

#pragma region "私有成员函数"
  private:

    //! Map of available providers by id. This class owns the pointers
    QMap<QString, QgsProcessingProvider *> mProviders;

    //! Hash of available parameter types by id. This object owns the pointers.
    QMap<QString, QgsProcessingParameterType *> mParameterTypes;

    QMap< QString, QString > mAlgorithmAliases;

#ifdef SIP_RUN
    QgsProcessingRegistry( const QgsProcessingRegistry &other );
#endif
#pragma endregion

};

#endif // QGSPROCESSINGREGISTRY_H


