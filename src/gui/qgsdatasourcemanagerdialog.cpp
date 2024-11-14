/***************************************************************************
    qgsdatasourcemanagerdialog.cpp - datasource manager dialog

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

#include <QListWidgetItem>

#include "qgsdatasourcemanagerdialog.h"
#include "ui_qgsdatasourcemanagerdialog.h"
#include "qgsbrowserdockwidget.h"
#include "qgslayermetadatasearchwidget.h"
#include "qgssettings.h"
#include "qgsproviderregistry.h"
#include "qgssourceselectprovider.h"
#include "qgssourceselectproviderregistry.h"
#include "qgsabstractdatasourcewidget.h"
#include "qgsmapcanvas.h"
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"
#include "qgsgui.h"
#include "qgsbrowserguimodel.h"
#include "qgsbrowserwidget.h"

QgsDataSourceManagerDialog::QgsDataSourceManagerDialog( QgsBrowserGuiModel *browserModel, QWidget *parent, QgsMapCanvas *canvas, Qt::WindowFlags fl )
  : QgsOptionsDialogBase( tr( "Data Source Manager" ), parent, fl )
  , ui( new Ui::QgsDataSourceManagerDialog )
  , mPreviousRow( -1 )
  , mMapCanvas( canvas )
  , mBrowserModel( browserModel )
{
  /*
  * 杨小兵-2024-02-22
  一、解释
    用于初始化一个数据源管理器对话框。这个对话框是一个用户界面元素，用于在QGIS中管理各种数据源。
  具体来说，它允许用户添加、编辑和删除地理数据源，如矢量图层、栅格图层等。

  - 设置对话框的基本属性和布局。
  - 创建并配置内部组件，如消息条（`QgsMessageBar`）、浏览器控件（`QgsBrowserDockWidget`）等。
  - 通过`QgsGui`的`sourceSelectProviderRegistry`注册的数据源选择提供器，动态添加各种数据源选择对话框。
  - 设置信号与槽的连接，以便在用户交互时执行相应的操作，如改变当前页面、处理文件拖放事件、打开文件、更新连接等。
  - 恢复上次会话的UI状态，如对话框的几何形状、分割器状态、当前选项卡状态等。

  */

#pragma region "设置对话框的基本属性和布局"
  /*
  * 杨小兵-2024-02-22
  一、解释
    这行代码是Qt用户界面编程中常见的一个步骤。`ui`是一个指向`Ui::QgsDataSourceManagerDialog`类的指针（在初始化列表中已经构建出
  来了），该类由Qt的UI编译器（uic）根据QT designer的`.ui`文件自动生成。`setupUi`函数负责将`.ui`文件中定义的所有窗口控件和布局初始
  化并加载到指定的父窗口中，在这里是`QgsDataSourceManagerDialog`对话框实例（通过`this`关键字指代）。这一步实质上是在构造函数中构
  建和配置了整个对话框的界面。

    这行代码设置了`verticalLayout_2`（一个垂直布局管理器）的间距为6像素。在Qt中，布局管理器用于自动管理窗口控件的位置和大小，以适应
  窗口的改变。`setSpacing`函数调整布局中子控件之间的间隔，这里的6像素间距意味着布局管理器会在其管理的控件之间保留至少6像素的空间，从
  而使界面看起来更加整洁、有序。

    这行代码设置了`verticalLayout_2`的内容边距为0像素。内容边距指的是布局内容（即布局中的所有控件）与布局容器边界之间的距离。通过调用
  `setContentsMargins`函数并传递四个0作为参数，这里指定了上、右、下、左四个方向的边距都为0，意味着布局中的控件会填满整个布局容器的空间，
  不留任何边距。这样做通常是为了最大化利用可用空间，特别是在需要紧凑布局的用户界面设计中

  二、总结
  1、setupUi函数：负责将.ui文件中定义的所有窗口控件和布局初始化、加载到指定的父窗口中
  2、在构造函数中构建和配置了整个对话框的界面
  */
  ui->setupUi( this );
  ui->verticalLayout_2->setSpacing( 6 );
  ui->verticalLayout_2->setContentsMargins( 0, 0, 0, 0 );

#pragma endregion

#pragma region "创建并配置内部组件，如消息条（`QgsMessageBar`）、浏览器控件（`QgsBrowserDockWidget`）等"
  mMessageBar = new QgsMessageBar( this );
  mMessageBar->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
  static_cast<QVBoxLayout *>( layout() )->insertWidget( 0, mMessageBar );

  /*
  * 杨小兵-2024-02-22
  // QgsOptionsDialogBase handles saving/restoring of geometry, splitter and current tab states,
  // switching vertical tabs between icon/text to icon-only modes (splitter collapsed to left),
  // and connecting QDialogButtonBox's accepted/rejected signals to dialog's accept/reject slots 
  一、解释
    这段注释解释了`QgsOptionsDialogBase`类的一些关键职责和功能，主要涉及界面状态的保存与恢复、布局调整以及信号与槽的连接。

   - **保存/恢复界面几何形状、分割器和当前标签状态**：`QgsOptionsDialogBase`负责保存对话框的当前几何形状（比如大小和位置）、
   分割器的位置（如果有的话，分割器用于调整界面中分区的大小比例）以及当前选中的标签页状态。这些信息在对话框关闭时保存，以便
   下次打开对话框时能够恢复到上次的状态，提升用户体验。
   
   - **在图标/文本到仅图标模式之间切换垂直标签**：注释还指出，`QgsOptionsDialogBase`能够管理垂直标签的显示模式。具体地，
   它可以在显示图标和文本到仅显示图标的模式之间切换。这种切换通常发生在用户调整界面布局（如把分割器完全拖动到左边）时，为用
   户提供更加灵活的界面布局选项。
   
   - **连接`QDialogButtonBox`的接受/拒绝信号到对话框的接受/拒绝槽**：最后，注释说明了`QgsOptionsDialogBase`还处理将标
   准对话框按钮（通常位于对话框底部的“OK”、“Cancel”按钮，集成在`QDialogButtonBox`控件中）的信号连接到对话框的相应槽函数。
   这意味着，当用户点击“OK”或“Cancel”按钮时，`QgsOptionsDialogBase`会负责执行接受或拒绝操作，比如保存更改或取消操作。
  */

  initOptionsBase( false );

  //  通过`connect`函数连接`mOptionsListWidget`的`currentRowChanged`信号到`setCurrentPage`槽函数，以实现在选择不同的列表项时切换显示的页面
  // Bind list index to the stacked dialogs
  connect( ui->mOptionsListWidget, &QListWidget::currentRowChanged, this, &QgsDataSourceManagerDialog::setCurrentPage );

  //  实例化`QgsBrowserDockWidget`，设置其特性，并将其添加到堆叠小部件中，以供用户浏览数据源（其中Browser是窗口标签名称）
  // BROWSER Add the browser widget to the first stacked widget page
  mBrowserWidget = new QgsBrowserDockWidget( QStringLiteral( "Browser" ), mBrowserModel, this );
  mBrowserWidget->setFeatures( QDockWidget::NoDockWidgetFeatures );
  ui->mOptionsStackedWidget->addWidget( mBrowserWidget );
  mPageProviderKeys.append( QStringLiteral( "browser" ) );
  mPageProviderNames.append( QStringLiteral( "browser" ) );

  /*
  * 杨小兵-2024-02-22
  一、解释
   - **handle**：通常在编程中用于表示“处理”的意思，这里指的是对某个事件或动作进行处理。
   - **Drop**：在图形用户界面中，拖放（Drag and Drop）是一种常见的交互方式，用户可以通
   过鼠标拖动一个对象并释放（drop）在另一个位置。在这个上下文中，“Drop”指的是拖放操作的
   释放动作。
   - **UriList**：URI（Uniform Resource Identifier，统一资源标识符）是一种用于标识某
   一互联网资源名称的字符串。一个URI列表则可能指的是多个这样的资源标识符的集合。在拖放文
   件时，这些URI可以代表被拖放的文件或其他资源的路径或标识。

    当用户通过拖放操作将一个或多个文件（或其他资源）释放到`QgsBrowserDockWidget`组件上时，
   `handleDropUriList`信号被触发，随后通过`connect`语句关联的槽函数（这里是`QgsDataSourceManagerDialog::handleDropUriList`）
   将被调用来具体处理这些URI，例如加载文件到QGIS项目中

  */
  //  通过`connect`函数连接浏览器控件的各种信号到相应的槽函数，以处理如文件拖放、打开文件、连接变化等事件
  // Forward all browser signals
  connect( mBrowserWidget, &QgsBrowserDockWidget::handleDropUriList, this, &QgsDataSourceManagerDialog::handleDropUriList );
  connect( mBrowserWidget, &QgsBrowserDockWidget::openFile, this, &QgsDataSourceManagerDialog::openFile );
  connect( mBrowserWidget, &QgsBrowserDockWidget::connectionsChanged, this, &QgsDataSourceManagerDialog::connectionsChanged );
  connect( this, &QgsDataSourceManagerDialog::updateProjectHome, mBrowserWidget->browserWidget(), &QgsBrowserWidget::updateProjectHome );
#pragma endregion

#pragma region "通过`QgsGui`的`sourceSelectProviderRegistry`注册的数据源选择提供器，动态添加各种数据源选择对话框"
  //  通过遍历`QgsGui::sourceSelectProviderRegistry()->providers()`返回的数据源选择提供器列表，为每个提供器创建对应的数据源选择对话框，并添加到界面中
  // Add registered source select dialogs
  const QList<QgsSourceSelectProvider *> sourceSelectProviders = QgsGui::sourceSelectProviderRegistry()->providers( );
  for ( QgsSourceSelectProvider *provider : sourceSelectProviders )
  {
    QgsAbstractDataSourceWidget *dlg = provider->createDataSourceWidget( this );
    if ( !dlg )
    {
      QgsMessageLog::logMessage( tr( "Cannot get %1 select dialog from source select provider %2." ).arg( provider->name(), provider->providerKey() ), QStringLiteral( "DataSourceManager" ), Qgis::MessageLevel::Critical );
      continue;
    }
    addProviderDialog( dlg, provider->providerKey(), provider->name(), provider->text(), provider->icon( ), provider->toolTip( ) );
  }

  //  设置信号与槽的连接，以便在用户交互时执行相应的操作，如改变当前页面、处理文件拖放事件、打开文件、更新连接等
  connect( QgsGui::sourceSelectProviderRegistry(), &QgsSourceSelectProviderRegistry::providerAdded, this, [ = ]( const QString & name )
  {
    if ( QgsSourceSelectProvider *provider = QgsGui::sourceSelectProviderRegistry()->providerByName( name ) )
    {
      QgsAbstractDataSourceWidget *dlg = provider->createDataSourceWidget( this );
      if ( !dlg )
      {
        QgsMessageLog::logMessage( tr( "Cannot get %1 select dialog from source select provider %2." ).arg( provider->name(), provider->providerKey() ), QStringLiteral( "DataSourceManager" ), Qgis::MessageLevel::Critical );
        return;
      }
      addProviderDialog( dlg, provider->providerKey(), provider->name(), provider->text(), provider->icon( ), provider->toolTip( ) );
    }
  } );

  connect( QgsGui::sourceSelectProviderRegistry(), &QgsSourceSelectProviderRegistry::providerRemoved, this, [ = ]( const QString & name )
  {
    removeProviderDialog( name );
  } );

  //  恢复上次会话的UI状态，如对话框的几何形状、分割器状态、当前选项卡状态等
  restoreOptionsBaseUi( tr( "Data Source Manager" ) );

#pragma endregion
}

QgsDataSourceManagerDialog::~QgsDataSourceManagerDialog()
{
  delete ui;
}

void QgsDataSourceManagerDialog::openPage( const QString &pageName )
{
  /*
  * 杨小兵-2024-02-22
  一、解释
  1. **寻找页面索引**：通过在`mPageProviderKeys`列表中查找`pageName`，获取对应的页面索引。
  2. **切换页面**：如果找到对应的索引（即`pageIdx != -1`），则使用`QTimer::singleShot`来延迟执行页面切换。
  这里延迟执行是因为可能需要确保对话框已完全初始化并且可以安全地更改当前显示的页面。

    在这个具体的例子中，延时被设置为0毫秒，意味着`setCurrentPage(pageIdx)`方法将会被尽可能快地调用，但调用会
  被安排在当前事件循环中当前事件处理完成之后。这种方式常用于将某个操作推迟到当前的函数调用栈返回之后，让当前
  的操作有机会完成或者更新UI元素之后再进行状态变更。

  `QTimer::singleShot`函数接受三个主要的参数：
  - **延时（int milliseconds）**：第一个参数是延时，表示在执行操作前等待的时间，以毫秒为单位。在这个例子中，
  延时是0毫秒，意味着操作会在当前事件处理完成后尽快执行。
  
  - **接收者（QObject *receiver）**：第二个参数指定了接收者，即当定时器超时时应该在哪个对象的上下文中执行指
  定的操作。这里`this`指的是`QgsDataSourceManagerDialog`对象，意味着`setCurrentPage`方法将在该对象的上下文中执行。
  
  - **操作（const Functor &functor）**：第三个参数是一个要执行的操作。这里使用了Lambda表达式`[=] { setCurrentPage(pageIdx); }`
  作为操作。Lambda表达式允许捕获当前作用域中的变量（在这个例子中是`pageIdx`），并在延时后执行。`[=]`表示以值捕获所有外部变量。

    通过使用`QTimer::singleShot`，可以非常灵活地在特定时间后执行几乎任何操作，这在需要延迟执行或者将某些操作推迟到事件循环的下
  一次迭代时非常有用。这对于处理复杂的UI更新或者在等待某些条件成熟后再执行操作尤其有价值。


  二、总结
  1、mPageProviderKeys的类型是QStringList，也就是说明其中存放的是多个QString，例如（"ogr"/"raster"/"mesh"等等矢量数据类型名称）
  2、QTimer::singleShot函数的作用就是用来延迟执行页面的切换（可能为了确保对话框已经完成初始化）
  */
  // TODO -- this is actually using provider keys, not provider names!
  const int pageIdx = mPageProviderKeys.indexOf( pageName );
  if ( pageIdx != -1 )
  {
    //  这行代码利用Qt的`QTimer`类来实现延时执行一个操作，这里的操作是调用`setCurrentPage`方法来设置当前页面为指定的页面。
    QTimer::singleShot( 0, this, [ = ] { setCurrentPage( pageIdx ); } );
  }
}

QgsMessageBar *QgsDataSourceManagerDialog::messageBar() const
{
  return mMessageBar;
}

void QgsDataSourceManagerDialog::activate()
{
  raise();
  setWindowState( windowState() & ~Qt::WindowMinimized );
  activateWindow();
}

void QgsDataSourceManagerDialog::setCurrentPage( int index )
{
  /*
  * 杨小兵-2024-02-22
  一、解释
    函数的作用是在数据源管理对话框（`QgsDataSourceManagerDialog`）中切换到指定的页面。
    - **保存当前页面索引**：首先，它保存当前页面的索引到成员变量`mPreviousRow`中。这一步是为了记录用户从哪个页面切换走，可能用于后续的逻辑处理，
    比如判断用户的导航路径或者在用户返回时恢复之前的页面状态。

    - **切换页面**：使用`ui->mOptionsStackedWidget->setCurrentIndex(index);`切换到指定的页面。这里`mOptionsStackedWidget`是一个`QStackedWidget`
    的实例，`setCurrentIndex`方法通过索引来切换显示的页面。
    
    - **更新窗口标题**：随后，它通过`setWindowTitle`方法更新对话框的标题，标题包含当前选中页面的名称。这是通过将固定文本`"Data Source Manager | "`
    与当前页面项的文本拼接而成，`ui->mOptionsListWidget->currentItem()->text()`获取当前选中列表项的文本。
    
    - **调整所有标签页尺寸**：最后，调用`resizeAlltabs(index)`方法，可能是为了根据当前页面的内容调整对话框的大小或布局。具体实现细节取决于
    `resizeAlltabs`方法的代码，但这通常意味着根据选中的页面内容动态调整对话框大小以适应不同的内容。

  二、总结

  */
  mPreviousRow = ui->mOptionsStackedWidget->currentIndex();
  ui->mOptionsStackedWidget->setCurrentIndex( index );
  setWindowTitle( tr( "Data Source Manager | %1" ).arg( ui->mOptionsListWidget->currentItem()->text() ) );
  resizeAlltabs( index );
}

void QgsDataSourceManagerDialog::setPreviousPage()
{
  const int prevPage = mPreviousRow != -1 ? mPreviousRow : 0;
  setCurrentPage( prevPage );
}

void QgsDataSourceManagerDialog::refresh()
{
  mBrowserWidget->browserWidget()->refresh();
  emit providerDialogsRefreshRequested();
}

void QgsDataSourceManagerDialog::reset()
{
  const int pageCount = ui->mOptionsStackedWidget->count();
  for ( int i = 0; i < pageCount; ++i )
  {
    QWidget *widget = ui->mOptionsStackedWidget->widget( i );
    QgsAbstractDataSourceWidget *dataSourceWidget = qobject_cast<QgsAbstractDataSourceWidget *>( widget );
    if ( dataSourceWidget )
      dataSourceWidget->reset();
  }
}

void QgsDataSourceManagerDialog::rasterLayerAdded( const QString &uri, const QString &baseName, const QString &providerKey )
{
  emit addRasterLayer( uri, baseName, providerKey );
}

void QgsDataSourceManagerDialog::rasterLayersAdded( const QStringList &layersList )
{
  emit addRasterLayers( layersList );
}

/*
* 杨小兵-2024-02-22
一、解释
  下列是`QgsDataSourceManagerDialog`类的一个成员函数`vectorLayerAdded`的实现，这个函数的目的是在一个矢量图层被添加时发出一个信号。这里的`emit`
关键字用于发出`addVectorLayer`信号，通知应用程序一个新的矢量图层已被选择添加。vectorLayerAdded是QgsDataSourceManagerDialog其中的一个public
槽函数，也就是说当addVectorLayer信号被触发的时候，那么这个函数将会被发射。

  当这个函数被调用时，它通过使用`emit`关键字发出`addVectorLayer`信号，并将接收到的参数传递给该信号。这意味着任何连接到这个信号的槽函数都将被
调用，并接收相同的参数值。在这个上下文中，`addVectorLayer`信号的作用是通知应用程序一个矢量图层已经被选中添加。应用程序中的其他部分，如主窗口
或图层管理器，可以连接到这个信号，并定义一个槽函数来响应信号，执行如添加图层到地图视图等操作。

二、参数说明
  - `vectorLayerPath`：一个字符串，表示矢量图层文件的路径。
  - `baseName`：矢量图层的基本名称，通常是不包含路径的文件名。
  - `providerKey`：用于标识图层数据提供者的键值，例如，对于从PostGIS数据库加载的图层，这个键值可能是`"postgres"`。

*/
void QgsDataSourceManagerDialog::vectorLayerAdded( const QString &vectorLayerPath, const QString &baseName, const QString &providerKey )
{
  emit addVectorLayer( vectorLayerPath, baseName, providerKey );
}

void QgsDataSourceManagerDialog::vectorLayersAdded( const QStringList &layerQStringList, const QString &enc, const QString &dataSourceType )
{
  emit addVectorLayers( layerQStringList, enc, dataSourceType );
}

void QgsDataSourceManagerDialog::addProviderDialog( QgsAbstractDataSourceWidget *dlg, const QString &providerKey, const QString &providerName, const QString &text, const QIcon &icon, const QString &toolTip )
{
  mPageProviderKeys.append( providerKey );
  mPageProviderNames.append( providerName );
  ui->mOptionsStackedWidget->addWidget( dlg );
  QListWidgetItem *layerItem = new QListWidgetItem( text, ui->mOptionsListWidget );
  layerItem->setData( Qt::UserRole, providerName );
  layerItem->setToolTip( toolTip.isEmpty() ? tr( "Add %1 layer" ).arg( providerName ) : toolTip );
  layerItem->setIcon( icon );
  // Set crs and extent from canvas
  if ( mMapCanvas )
  {
    dlg->setMapCanvas( mMapCanvas );
  }
  dlg->setBrowserModel( mBrowserModel );

  connect( dlg, &QgsAbstractDataSourceWidget::rejected, this, &QgsDataSourceManagerDialog::reject );
  connect( dlg, &QgsAbstractDataSourceWidget::accepted, this, &QgsDataSourceManagerDialog::accept );
  makeConnections( dlg, providerKey );
}

void QgsDataSourceManagerDialog::removeProviderDialog( const QString &providerName )
{
  const int pageIdx = mPageProviderNames.indexOf( providerName );
  if ( pageIdx != -1 )
  {
    ui->mOptionsStackedWidget->removeWidget( ui->mOptionsStackedWidget->widget( pageIdx ) );
    mPageProviderKeys.removeAt( pageIdx );
    mPageProviderNames.removeAt( pageIdx );
    ui->mOptionsListWidget->removeItemWidget( ui->mOptionsListWidget->item( pageIdx ) );
  }
}

void QgsDataSourceManagerDialog::makeConnections( QgsAbstractDataSourceWidget *dlg, const QString &providerKey )
{
  // DB
  connect( dlg, &QgsAbstractDataSourceWidget::addDatabaseLayers,
           this, &QgsDataSourceManagerDialog::addDatabaseLayers );
  connect( dlg, &QgsAbstractDataSourceWidget::progressMessage,
           this, &QgsDataSourceManagerDialog::showStatusMessage );
  // Vector
  connect( dlg, &QgsAbstractDataSourceWidget::addVectorLayer, this, [ = ]( const QString & vectorLayerPath, const QString & baseName, const QString & specifiedProvider )
  {
    const QString key = specifiedProvider.isEmpty() ? providerKey : specifiedProvider;
    this->vectorLayerAdded( vectorLayerPath, baseName, key );
  }
         );
  connect( dlg, &QgsAbstractDataSourceWidget::addVectorLayers,
           this, &QgsDataSourceManagerDialog::vectorLayersAdded );
  connect( dlg, &QgsAbstractDataSourceWidget::connectionsChanged, this, &QgsDataSourceManagerDialog::connectionsChanged );
  // Raster
  connect( dlg, &QgsAbstractDataSourceWidget::addRasterLayer,
           this, [ = ]( const QString & uri, const QString & baseName, const QString & providerKey )
  {
    addRasterLayer( uri, baseName, providerKey );
  } );
  connect( dlg, &QgsAbstractDataSourceWidget::addRasterLayers,
           this, &QgsDataSourceManagerDialog::rasterLayersAdded );
  // Mesh
  connect( dlg, &QgsAbstractDataSourceWidget::addMeshLayer, this, &QgsDataSourceManagerDialog::addMeshLayer );
  // Vector tile
  connect( dlg, &QgsAbstractDataSourceWidget::addVectorTileLayer, this, &QgsDataSourceManagerDialog::addVectorTileLayer );
  // Point Cloud
  connect( dlg, &QgsAbstractDataSourceWidget::addPointCloudLayer, this, &QgsDataSourceManagerDialog::addPointCloudLayer );
  // Virtual
  connect( dlg, &QgsAbstractDataSourceWidget::replaceVectorLayer,
           this, &QgsDataSourceManagerDialog::replaceSelectedVectorLayer );
  // Common
  connect( dlg, &QgsAbstractDataSourceWidget::connectionsChanged, this, &QgsDataSourceManagerDialog::connectionsChanged );
  connect( this, &QgsDataSourceManagerDialog::providerDialogsRefreshRequested, dlg, &QgsAbstractDataSourceWidget::refresh );

  // Message
  connect( dlg, &QgsAbstractDataSourceWidget::pushMessage, this, [ = ]( const QString & title, const QString & message, const Qgis::MessageLevel level )
  {
    mMessageBar->pushMessage( title, message, level );
  } );
}

void QgsDataSourceManagerDialog::showEvent( QShowEvent *e )
{
  ui->mOptionsStackedWidget->currentWidget()->show();
  QgsOptionsDialogBase::showEvent( e );
  resizeAlltabs( ui->mOptionsStackedWidget->currentIndex() );
}
