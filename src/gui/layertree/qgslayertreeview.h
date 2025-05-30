/***************************************************************************
  qgslayertreeview.h
  --------------------------------------
  Date                 : May 2014
  Copyright            : (C) 2014 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSLAYERTREEVIEW_H
#define QGSLAYERTREEVIEW_H

#include <QTreeView>
#include "qgis.h"
#include "qgis_gui.h"

class QgsLayerTreeGroup;
class QgsLayerTreeLayer;
class QgsLayerTreeModel;
class QgsLayerTreeNode;
class QgsLayerTreeModelLegendNode;
class QgsLayerTreeViewDefaultActions;
class QgsLayerTreeViewIndicator;
class QgsLayerTreeViewMenuProvider;
class QgsMapLayer;
class QgsMessageBar;
class QgsLayerTreeFilterProxyModel;


#include <QSortFilterProxyModel>

/**
 * \ingroup gui
 *
 * \brief The QgsLayerTreeProxyModel class is a proxy model for QgsLayerTreeModel, supports
 * private layers and text filtering.
 *
 * \since QGIS 3.18
 */
class GUI_EXPORT QgsLayerTreeProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

  public:

    /**
     * Constructs QgsLayerTreeProxyModel with source model \a treeModel and a \a parent
     */
    QgsLayerTreeProxyModel( QgsLayerTreeModel *treeModel, QObject *parent );

    /**
     * Sets filter to \a filterText.
     */
    void setFilterText( const QString &filterText = QString() );

    /**
     * Returns if private layers are shown.
     */
    bool showPrivateLayers() const;

    /**
     * Determines if private layers are shown.
     */
    void setShowPrivateLayers( bool showPrivate );

  protected:

    bool filterAcceptsRow( int sourceRow, const QModelIndex &sourceParent ) const override;

  private:

    bool nodeShown( QgsLayerTreeNode *node ) const;

    QgsLayerTreeModel *mLayerTreeModel = nullptr;
    QString mFilterText;
    bool mShowPrivateLayers = false;

};

/*
* 杨小兵-2024-02-23
一、解释
  上述注释是对`QgsLayerTreeView`类的详细描述，该类在QGIS的图形用户界面（GUI）模块中。让我们逐点解析这段注释，以更好地理解`QgsLayerTreeView`类
的功能和应用场景。
  - **扩展自`QTreeView`**：`QgsLayerTreeView`是基于Qt的`QTreeView`类的扩展，这意味着它继承了`QTreeView`的所有功能，并在此基础上增加了额外的
  功能，专门用于处理图层树（layer tree）。

  - **额外的功能**：`QgsLayerTreeView`提供了一些在处理图层树时非常有用的附加功能。这些功能包括更新图层树节点的展开状态、监听图层树中展开状态的
  变化、跟踪当前图层并在当前图层改变时发出信号等。

1. **更新和监听展开状态**：`QgsLayerTreeView`能够更新图层树中节点的展开状态，并且能够监听图层树中展开状态的改变。这意味着如果用户在图层树视图中
展开或折叠了某个节点，`QgsLayerTreeView`会相应地更新其显示，并且如果图层树的结构在程序的其他部分发生变化，`QgsLayerTreeView`也能够捕捉到这些变
化并更新视图。

2. **跟踪当前图层并发出信号**：当当前选中的图层发生变化时，`QgsLayerTreeView`会跟踪这一变化并发出一个信号。这对于开发者来说非常有用，因为他们可以
监听这个信号并根据当前选中的图层更新应用程序的其他部分，如属性面板、状态栏等。

3. **自定义上下文菜单**：`QgsLayerTreeView`允许客户端（即使用`QgsLayerTreeView`的开发者）指定一个上下文菜单提供者，以添加自定义操作到图层树视图
的上下文菜单中。这为开发者提供了高度的灵活性，使他们能够根据应用程序的需要为用户提供定制的功能。

4. **默认动作集合**：除了支持自定义操作，`QgsLayerTreeView`还自带了一套默认的动作，这些动作可以在构建上下文菜单时使用。这些默认动作涵盖了一些常见
的图层操作需求，如添加、删除图层等。

5. **与`QgsLayerTreeModel`的关系**：注释中提到了`QgsLayerTreeModel`，这是因为`QgsLayerTreeView`通常与`QgsLayerTreeModel`一起使用。
`QgsLayerTreeModel`提供了图层树的数据模型，而`QgsLayerTreeView`则是这个模型的视图表示。这种模型-视图架构使得图层树的表示与数据逻辑分离，
便于管理和更新。(数据+视图)

二、QGIS全部的模块
1、core library:The CORE library contains all basic GIS functionality
2、gui library	:The GUI library is build on top of the CORE library and adds reusable GUI widgets
3、analysis library:The ANALYSIS library is built on top of CORE library and provides high level tools for carrying out spatial analysis on vector and raster data
4、server library:The SERVER library is built on top of the CORE library and adds map server components to QGIS
5、3D library:The 3D library is build on top of the CORE library and Qt 3D framework
6、plugin classes:Contains classes related to implementation of QGIS plugins
7、QgsQuick library:The QgsQuick library is built on top of the CORE library and Qt Quick/QML framework

三、适用版本
- **自QGIS 2.4起引入**：这表明`QgsLayerTreeView`类自QGIS版本2.4起就已经存在，并在后续版本中继续得到支持和更新。

*/
/**
 * \ingroup gui
 * \brief The QgsLayerTreeView class extends QTreeView and provides some additional functionality
 * when working with a layer tree.
 *
 * The view updates expanded state of layer tree nodes and also listens to changes
 * to expanded states in the layer tree.
 *
 * The view keeps track of the current layer and emits a signal when the current layer has changed.
 *
 * Allows the client to specify a context menu provider with custom actions. Also it comes
 * with a set of default actions that can be used when building context menu.
 *
 * \see QgsLayerTreeModel
 * \since QGIS 2.4
 */
class GUI_EXPORT QgsLayerTreeView : public QTreeView
{

#ifdef SIP_RUN
    SIP_CONVERT_TO_SUBCLASS_CODE
    if ( sipCpp->inherits( "QgsLayerTreeView" ) )
      sipType = sipType_QgsLayerTreeView;
    else
      sipType = 0;
    SIP_END
#endif


    Q_OBJECT
  public:

    //! Constructor for QgsLayerTreeView
    explicit QgsLayerTreeView( QWidget *parent SIP_TRANSFERTHIS = nullptr );
    ~QgsLayerTreeView() override;

    //! Overridden setModel() from base class. Only QgsLayerTreeModel is an acceptable model.
    void setModel( QAbstractItemModel *model ) override;

    //! Gets access to the model casted to QgsLayerTreeModel
    QgsLayerTreeModel *layerTreeModel() const;

    /**
     * Returns the proxy model used by the view.
     *
     * This can be used to set filters controlling which layers are shown in the view.
     *
     * \since QGIS 3.18
     */
    QgsLayerTreeProxyModel *proxyModel() const;

    /**
     * Returns layer tree node for given proxy model tree \a index. Returns root node for invalid index.
     * Returns NULLPTR if index does not refer to a layer tree node (e.g. it is a legend node)
     *
     * Unlike QgsLayerTreeModel::index2Node(), calling this method correctly accounts
     * for mapping the view indexes through the view's proxy model to the source model.
     *
     * \since QGIS 3.18
     */
    QgsLayerTreeNode *index2node( const QModelIndex &index ) const;

    /**
     * Returns proxy model index for a given node. If the node does not belong to the layer tree, the result is undefined
     *
     * Unlike QgsLayerTreeModel::node2index(), calling this method correctly accounts
     * for mapping the view indexes through the view's proxy model to the source model.
     *
     * \since QGIS 3.18
     */
    QModelIndex node2index( QgsLayerTreeNode *node ) const;


    /**
     * Returns source model index for a given node. If the node does not belong to the layer tree, the result is undefined
     *
     * \since QGIS 3.18
     */
    QModelIndex node2sourceIndex( QgsLayerTreeNode *node ) const;


    /**
     * Returns legend node for given proxy model tree \a index. Returns NULLPTR for invalid index
     *
     * Unlike QgsLayerTreeModel::index2legendNode(), calling this method correctly accounts
     * for mapping the view indexes through the view's proxy model to the source model.
     *
     * \since QGIS 3.18
     */
    QgsLayerTreeModelLegendNode *index2legendNode( const QModelIndex &index ) const;

    /**
     * Returns proxy model index for a given legend node. If the legend node does not belong to the layer tree, the result is undefined.
     * If the legend node is belongs to the tree but it is filtered out, invalid model index is returned.
     *
     * Unlike QgsLayerTreeModel::legendNode2index(), calling this method correctly accounts
     * for mapping the view indexes through the view's proxy model to the source model.
     *
     * \since QGIS 3.18
     */
    QModelIndex legendNode2index( QgsLayerTreeModelLegendNode *legendNode );

    /**
     * Returns index for a given legend node. If the legend node does not belong to the layer tree, the result is undefined.
     * If the legend node is belongs to the tree but it is filtered out, invalid model index is returned.
     *
     * \since QGIS 3.18
     */
    QModelIndex legendNode2sourceIndex( QgsLayerTreeModelLegendNode *legendNode );

    //! Gets access to the default actions that may be used with the tree view
    QgsLayerTreeViewDefaultActions *defaultActions();

    //! Sets provider for context menu. Takes ownership of the instance
    void setMenuProvider( QgsLayerTreeViewMenuProvider *menuProvider SIP_TRANSFER );
    //! Returns pointer to the context menu provider. May be NULLPTR
    QgsLayerTreeViewMenuProvider *menuProvider() const { return mMenuProvider; }

    /**
     * Returns the currently selected layer, or NULLPTR if no layers is selected.
     *
     * \see setCurrentLayer()
     */
    QgsMapLayer *currentLayer() const;

    /**
     * Convenience methods which sets the visible state of the specified map \a layer.
     *
     * \see QgsLayerTreeNode::setItemVisibilityChecked()
     * \since QGIS 3.10
     */
    void setLayerVisible( QgsMapLayer *layer, bool visible );

    /**
     * Sets the currently selected \a layer.
     *
     * If \a layer is NULLPTR then all layers will be deselected.
     *
     * \see currentLayer()
     */
    void setCurrentLayer( QgsMapLayer *layer );

    //! Gets current node. May be NULLPTR
    QgsLayerTreeNode *currentNode() const;
    //! Gets current group node. If a layer is current node, the function will return parent group. May be NULLPTR.
    QgsLayerTreeGroup *currentGroupNode() const;

    /**
     * Gets current legend node. May be NULLPTR if current node is not a legend node.
     * \since QGIS 2.14
     */
    QgsLayerTreeModelLegendNode *currentLegendNode() const;

    /**
     * Returns list of selected nodes
     * \param skipInternal If TRUE, will ignore nodes which have an ancestor in the selection
     */
    QList<QgsLayerTreeNode *> selectedNodes( bool skipInternal = false ) const;
    //! Returns list of selected nodes filtered to just layer nodes
    QList<QgsLayerTreeLayer *> selectedLayerNodes() const;

    //! Gets list of selected layers
    QList<QgsMapLayer *> selectedLayers() const;

    /**
     * Gets list of selected layers, including those that are not directly selected, but their
     * ancestor groups is selected. If we have a group with two layers L1, L2 and just the group
     * node is selected, this method returns L1 and L2, while selectedLayers() returns an empty list.
     * \since QGIS 3.4
     */
    QList<QgsMapLayer *> selectedLayersRecursive() const;

    /**
     * Adds an indicator to the given layer tree node. Indicators are icons shown next to layer/group names
     * in the layer tree view. They can be used to show extra information with tree nodes and they allow
     * user interaction.
     *
     * Does not take ownership of the indicator. One indicator object may be used for multiple layer tree nodes.
     * \see removeIndicator
     * \see indicators
     * \since QGIS 3.2
     */
    void addIndicator( QgsLayerTreeNode *node, QgsLayerTreeViewIndicator *indicator );

    /**
     * Removes a previously added indicator to a layer tree node. Does not delete the indicator.
     * \see addIndicator
     * \see indicators
     * \since QGIS 3.2
     */
    void removeIndicator( QgsLayerTreeNode *node, QgsLayerTreeViewIndicator *indicator );

    /**
     * Returns list of indicators associated with a particular layer tree node.
     * \see addIndicator
     * \see removeIndicator
     * \since QGIS 3.2
     */
    QList<QgsLayerTreeViewIndicator *> indicators( QgsLayerTreeNode *node ) const;

    /**
     * Returns width of contextual menu mark, at right of layer node items.
     * \see setLayerMarkWidth
     * \since QGIS 3.8
     */
    int layerMarkWidth() const { return mLayerMarkWidth; }

///@cond PRIVATE

    /**
     * Returns a list of custom property keys which are considered as related to view operations
     * only. E.g. node expanded state.
     *
     * Changes to these keys will not mark a project as "dirty" and trigger unsaved changes
     * warnings.
     *
     * \since QGIS 3.2
     */
    static QStringList viewOnlyCustomProperties() SIP_SKIP;
///@endcond

  public slots:
    //! Force refresh of layer symbology. Normally not needed as the changes of layer's renderer are monitored by the model
    void refreshLayerSymbology( const QString &layerId );

    /**
     * Enhancement of QTreeView::expandAll() that also records expanded state in layer tree nodes
     * \since QGIS 2.18
     */
    void expandAllNodes();

    /**
     * Enhancement of QTreeView::collapseAll() that also records expanded state in layer tree nodes
     * \since QGIS 2.18
     */
    void collapseAllNodes();

    /**
     * Set width of contextual menu mark, at right of layer node items.
     * \see layerMarkWidth
     * \since QGIS 3.8
     */
    void setLayerMarkWidth( int width ) { mLayerMarkWidth = width; }

    /**
     * Set the message bar to display messages from the layer tree
     * \since QGIS 3.14
     */
    void setMessageBar( QgsMessageBar *messageBar );

    /**
     * Set the show private layers to \a showPrivate
     * \since QGIS 3.18
     */
    void setShowPrivateLayers( bool showPrivate );

    /**
     * Returns the show private layers status
     * \since QGIS 3.18
     */
    bool showPrivateLayers( );

  signals:
    //! Emitted when a current layer is changed
    void currentLayerChanged( QgsMapLayer *layer );

    /**
     * Emitted when the context menu is about to show.
     *
     * Allows customization of the menu.
     *
     * \since QGIS 3.32
     */
    void contextMenuAboutToShow( QMenu *menu );

  protected:
    void contextMenuEvent( QContextMenuEvent *event ) override;

    void updateExpandedStateFromNode( QgsLayerTreeNode *node );

    QgsMapLayer *layerForIndex( const QModelIndex &index ) const;

    void mouseReleaseEvent( QMouseEvent *event ) override;
    void keyPressEvent( QKeyEvent *event ) override;

    void dropEvent( QDropEvent *event ) override;

    void resizeEvent( QResizeEvent *event ) override;

  protected slots:

    void modelRowsInserted( const QModelIndex &index, int start, int end );
    void modelRowsRemoved();

    void updateExpandedStateToNode( const QModelIndex &index );

    void onCurrentChanged();
    void onExpandedChanged( QgsLayerTreeNode *node, bool expanded );
    void onModelReset();

  private slots:
    void onCustomPropertyChanged( QgsLayerTreeNode *node, const QString &key );
    //! Handles updating the viewport to avoid flicker
    void onHorizontalScroll( int value );

    void onDataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles );

  protected:
    //! helper class with default actions. Lazily initialized.
    QgsLayerTreeViewDefaultActions *mDefaultActions = nullptr;
    //! Context menu provider. Owned by the view.
    QgsLayerTreeViewMenuProvider *mMenuProvider = nullptr;
    //! Keeps track of current layer ID (to check when to emit signal about change of current layer)
    QString mCurrentLayerID;
    //! Storage of indicators used with the tree view
    QHash< QgsLayerTreeNode *, QList<QgsLayerTreeViewIndicator *> > mIndicators;
    //! Used by the item delegate for identification of which indicator has been clicked
    QPoint mLastReleaseMousePos;

    //! Width of contextual menu mark for layer nodes
    int mLayerMarkWidth;

  private:
    QgsLayerTreeProxyModel *mProxyModel = nullptr;

    QgsMessageBar *mMessageBar = nullptr;

    bool mShowPrivateLayers = false;

    // For model  debugging
    // void checkModel( );

    // friend so it can access viewOptions() method and mLastReleaseMousePos without making them public
    friend class QgsLayerTreeViewItemDelegate;
};


/**
 * \ingroup gui
 * \brief Implementation of this interface can be implemented to allow QgsLayerTreeView
 * instance to provide custom context menus (opened upon right-click).
 *
 * \see QgsLayerTreeView
 * \since QGIS 2.4
 */
class GUI_EXPORT QgsLayerTreeViewMenuProvider
{
  public:
    virtual ~QgsLayerTreeViewMenuProvider() = default;

    //! Returns a newly created menu instance (or NULLPTR on error)
    virtual QMenu *createContextMenu() = 0 SIP_FACTORY;
};


#endif // QGSLAYERTREEVIEW_H
