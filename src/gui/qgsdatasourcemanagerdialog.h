/***************************************************************************
    qgsdatasourcemanagerdialog.h - datasource manager dialog

    ---------------------
    begin                : May 19, 2017
    copyright            : (C) 2017 by Alessandro Pasotti
    email                : apasotti at itopen dot it
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSDATASOURCEMANAGERDIALOG_H
#define QGSDATASOURCEMANAGERDIALOG_H

#include <QList>
#include <QDialog>
#include "ui_qgsdatasourcemanagerdialog.h"
#include "qgsoptionsdialogbase.h"
#include "qgsguiutils.h"
#include "qgsmimedatautils.h"
#include "qgshelp.h"
#include "qgis_gui.h"

#define SIP_NO_FILE

class QgsBrowserDockWidget;
class QgsRasterLayer;
class QgsMapCanvas;
class QgsAbstractDataSourceWidget;
class QgsLayerMetadataSearchWidget;
class QgsBrowserGuiModel;
class QgsMessageBar;

/**
 * \ingroup gui
 * \brief The QgsDataSourceManagerDialog class embeds the browser panel and all
 * the provider dialogs.
 * The dialog does not handle layer addition directly but emits signals that
 * need to be forwarded to the QGIS application to be handled.
 * \note not available in Python bindings
 * \since QGIS 3.0
 */
/*
* 杨小兵-2024-02-22
  - **`Ui::QgsDataSourceManagerDialog`**：这是一个由Qt Designer生成的界面类，定义了
对话框的布局和界面元素，如按钮、列表框等。这个类通常在Qt Designer的UI文件中定义，并通
过uic（UI编译器）工具自动生成C++代码。
  - **`QgsDataSourceManagerDialog`**：这是一个实际的对话框类，用于实现数据源管理的逻辑
和功能。它使用`Ui::QgsDataSourceManagerDialog`来定义其界面，但包含了额外的逻辑，如信号
和槽的连接、用户交互的处理等。

- **`QgsBrowserGuiModel *browserModel`**：指向一个浏览器模型的指针，这个模型用于管理和
展示QGIS中可用的数据源。这允许`QgsDataSourceManagerDialog`展示和操作这些数据源。

- **`QWidget *parent = nullptr`**：指向父窗口的指针。这个参数用于设置对话框的父窗口，有
助于Qt管理窗口的层次关系和事件传递。默认为`nullptr`，意味着对话框没有父窗口。

- **`QgsMapCanvas *canvas = nullptr`**：指向地图画布的指针。地图画布是QGIS中用于展示地
图的组件。这个参数允许`QgsDataSourceManagerDialog`在需要时与地图画布交互，例如添加图层到
画布上。默认为`nullptr`。

- **`Qt::WindowFlags fl = Qt::Window`**：窗口标志，用于控制对话框的外观和行为。`Qt::Window`
是一个默认值，表示对话框是一个顶级窗口。这个参数可以用于设置对话框为模态、无边框等不同的窗口类型。
*/
class GUI_EXPORT QgsDataSourceManagerDialog : public QgsOptionsDialogBase, private Ui::QgsDataSourceManagerDialog
{
    Q_OBJECT

  public:

    /**
     * QgsDataSourceManagerDialog constructor
      * \param browserModel instance of the (shared) browser model
      * \param parent the object
      * \param canvas a pointer to the map canvas
      * \param fl window flags
      */
    explicit QgsDataSourceManagerDialog( QgsBrowserGuiModel *browserModel, QWidget *parent = nullptr, QgsMapCanvas *canvas = nullptr, Qt::WindowFlags fl = Qt::Window );
    ~QgsDataSourceManagerDialog() override;

    /**
     * Open a given page in the dialog
     * \param pageName the page name, usually the provider name or "browser" (for the browser panel)
     *        or "ogr" (vector layers) or "raster" (raster layers)
     */
    void openPage( const QString &pageName );

    //! Returns the dialog's message bar
    QgsMessageBar *messageBar() const;

  public slots:

    /**
     * Raise, unminimize and activate this window.
     *
     * \since QGIS 3.28
     */
    void activate();

    //! Sync current page with the leftbar list
    void setCurrentPage( int index );

    // TODO: use this with an internal source select dialog instead of forwarding the whole raster selection to app

    //! A raster layer was added: for signal forwarding to QgisApp
    void rasterLayerAdded( QString const &uri, QString const &baseName, QString const &providerKey );

    /**
     * One or more raster layer were added: for signal forwarding to QgisApp
     * \since QGIS 3.20
     */
    void rasterLayersAdded( const QStringList &layersList );
    //! A vector layer was added: for signal forwarding to QgisApp
    void vectorLayerAdded( const QString &vectorLayerPath, const QString &baseName, const QString &providerKey );
    //! One or more vector layer were added: for signal forwarding to QgisApp
    void vectorLayersAdded( const QStringList &layerQStringList, const QString &enc, const QString &dataSourceType );
    //! Reset current page to previously selected page
    void setPreviousPage();
    //! Refresh the browser view
    void refresh();

    /**
     * Resets the interface of the datasource manager after reopening the dialog.
     *
     * Will clear the selection of embedded all source selection widgets.
     *
     * \since QGIS 3.10
     */
    void reset();

  protected:
    void showEvent( QShowEvent *event ) override;

  signals:
    /*
    * 杨小兵-2024-02-22
    一、解释
      这些代码片段定义了`QgsDataSourceManagerDialog`类中的信号。在QGIS中，信号和槽机制是一种在对象之间进行通信的方式，
    其中一个对象（发射信号的对象）在特定事件发生时发射（emit）一个信号，而另一个或多个对象（连接到该信号的对象）接收该信
    号并响应。这些特定的信号被设计用于在用户通过`QgsDataSourceManagerDialog`选择添加各种类型的图层（如栅格图层、矢量图
    层、网格图层和矢量瓦片图层）时发射。

    - **层级添加的通知**：这些信号提供了一种机制，用于在用户从数据源管理器中选择一个或多个图层添加到项目中时，通知QGIS应
    用（如QgisApp）进行相应的操作。
    - **解耦UI和逻辑**：通过这种方式，`QgsDataSourceManagerDialog`不直接处理图层的添加逻辑，而是通过信号发射通知，这有
    助于保持代码的模块化和解耦，使得UI组件（如对话框）与应用程序的其他逻辑分离。
    - **灵活性和扩展性**：当QGIS应用接收到这些信号时，可以灵活地处理图层添加的逻辑，包括图层的加载、配置以及与已有项目的
    集成等。这也为将来添加新的图层类型或修改现有逻辑提供了便利。


    二、总结
    1、要使用这些信号，你需要在QGIS应用中的适当位置连接这些信号到相应的槽函数。
    */
    /**
     * Emitted when a one or more layer were selected for addition: for signal forwarding to QgisApp
     * \since QGIS 3.20
     */
    void addRasterLayers( const QStringList &layersList );
    //! Emitted when a raster layer was selected for addition: for signal forwarding to QgisApp
    void addRasterLayer( const QString &uri, const QString &baseName, const QString &providerKey );

    //! Emitted when a vector layer was selected for addition: for signal forwarding to QgisApp
    void addVectorLayer( const QString &vectorLayerPath, const QString &baseName, const QString &providerKey );

    /**
     * Emitted when a mesh layer was selected for addition: for signal forwarding to QgisApp
     * \since QGIS 3.4
     */
    void addMeshLayer( const QString &uri, const QString &baseName, const QString &providerKey );

    /**
     * Emitted when a vector tile layer was selected for addition: for signal forwarding to QgisApp
     * \since QGIS 3.14
     */
    void addVectorTileLayer( const QString &uri, const QString &baseName );

    /**
     * Emitted when a point cloud layer was selected for addition: for signal forwarding to QgisApp
     * \since QGIS 3.18
     */
    void addPointCloudLayer( const QString &pointCloudLayerPath, const QString &baseName, const QString &providerKey );

    //! Replace the selected layer by a vector layer defined by uri, layer name, data source uri
    void replaceSelectedVectorLayer( const QString &oldId, const QString &uri, const QString &layerName, const QString &provider );
    //! Emitted when a one or more layer were selected for addition: for signal forwarding to QgisApp
    void addVectorLayers( const QStringList &layerQStringList, const QString &enc, const QString &dataSourceType );

    //! Emitted when a status message needs to be shown: for signal forwarding to QgisApp
    void showStatusMessage( const QString &message );
    //! Emitted when a DB layer was selected for addition: for signal forwarding to QgisApp
    void addDatabaseLayers( const QStringList &layerPathList, const QString &providerKey );
    //! Emitted when a file needs to be opened
    void openFile( const QString &fileName, const QString &fileTypeHint = QString() );
    //! Emitted when drop uri list needs to be handled from the browser
    void handleDropUriList( const QgsMimeDataUtils::UriList & );
    //! Update project home directory
    void updateProjectHome();

    /**
     * Emitted when a connection has changed inside the provider dialogs
     * This signal is normally forwarded to the application to notify other
     * browsers that they need to refresh their connections list
     */
    void connectionsChanged();

    /**
     * One or more provider connections have changed and the
     * dialogs should be refreshed
     */
    void providerDialogsRefreshRequested();

  private:
    void addProviderDialog( QgsAbstractDataSourceWidget *dlg, const QString &providerKey, const QString &providerName, const QString &text, const QIcon &icon, const QString &toolTip = QString() );
    void removeProviderDialog( const QString &providerName );
    void makeConnections( QgsAbstractDataSourceWidget *dlg, const QString &providerKey );
    Ui::QgsDataSourceManagerDialog *ui = nullptr;
    QgsBrowserDockWidget *mBrowserWidget = nullptr;
    QgsLayerMetadataSearchWidget *mLayerMetadataSearchWidget = nullptr;
    int mPreviousRow;
    QStringList mPageProviderKeys;
    QStringList mPageProviderNames;
    // Map canvas
    QgsMapCanvas *mMapCanvas = nullptr;
    QgsMessageBar *mMessageBar = nullptr;
    QgsBrowserGuiModel *mBrowserModel = nullptr;

};

#endif // QGSDATASOURCEMANAGERDIALOG_H
