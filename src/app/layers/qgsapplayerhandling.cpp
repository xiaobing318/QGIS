/***************************************************************************
    qgsapplayerhandling.cpp
    -------------------------
    begin                : July 2022
    copyright            : (C) 2022 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsapplayerhandling.h"

#include "qgsconfig.h"
#include "qgsmaplayer.h"
#include "qgsmeshlayer.h"
#include "qgsproject.h"
#include "qgsprojecttimesettings.h"
#include "qgspointcloudlayer.h"
#include "qgsmeshlayertemporalproperties.h"
#include "qgisapp.h"
#include "qgsmessagebar.h"
#ifdef HAVE_3D
#include "qgspointcloudlayer3drenderer.h"
#endif
#include "canvas/qgscanvasrefreshblocker.h"
#include "qgsproviderutils.h"
#include "qgszipitem.h"
#include "qgsproviderregistry.h"
#include "qgsprovidersublayerdetails.h"
#include "qgsprovidersublayersdialog.h"
#include "qgslayertreenode.h"
#include "qgslayertree.h"
#include "qgslayertreeview.h"
#include "qgsgui.h"
#include "qgsmbtiles.h"
#include "qgsmessagelog.h"
#include "qgsapplication.h"
#include "qgsvectortilelayer.h"
#include "qgsprojectstorageregistry.h"
#include "qgsprojectstorage.h"
#include "qgsmaplayerfactory.h"
#include "qgsrasterlayer.h"
#include "qgsauthguiutils.h"
#include "qgslayerdefinition.h"
#include "qgspluginlayer.h"
#include "qgspluginlayerregistry.h"
#include "qgsmessagebaritem.h"
#include "qgsdockwidget.h"
#include "qgseditorwidgetregistry.h"
#include "qgsweakrelation.h"
#include "qgsfieldformatterregistry.h"
#include "qgsmaplayerutils.h"
#include "qgsfieldformatter.h"
#include "qgsabstractdatabaseproviderconnection.h"
#include "qgsogrproviderutils.h"

#include <QObject>
#include <QMessageBox>
#include <QFileDialog>
#include <QUrlQuery>

void QgsAppLayerHandling::postProcessAddedLayer( QgsMapLayer *layer )
{
  switch ( layer->type() )
  {
    case QgsMapLayerType::VectorLayer:
    case QgsMapLayerType::RasterLayer:
    {
      bool ok = false;
      layer->loadDefaultStyle( ok );
      layer->loadDefaultMetadata( ok );
      break;
    }

    case QgsMapLayerType::PluginLayer:
      break;

    case QgsMapLayerType::MeshLayer:
    {
      QgsMeshLayer *meshLayer = qobject_cast< QgsMeshLayer *>( layer );
      QDateTime referenceTime = QgsProject::instance()->timeSettings()->temporalRange().begin();
      if ( !referenceTime.isValid() ) // If project reference time is invalid, use current date
        referenceTime = QDateTime( QDate::currentDate(), QTime( 0, 0, 0 ), Qt::UTC );

      if ( meshLayer->dataProvider() && !qobject_cast< QgsMeshLayerTemporalProperties * >( meshLayer->temporalProperties() )->referenceTime().isValid() )
        qobject_cast< QgsMeshLayerTemporalProperties * >( meshLayer->temporalProperties() )->setReferenceTime( referenceTime, meshLayer->dataProvider()->temporalCapabilities() );

      bool ok = false;
      meshLayer->loadDefaultStyle( ok );
      meshLayer->loadDefaultMetadata( ok );
      break;
    }

    case QgsMapLayerType::VectorTileLayer:
    {
      bool ok = false;
      QString error = layer->loadDefaultStyle( ok );
      if ( !ok )
        QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Error loading style" ), error, Qgis::MessageLevel::Warning );
      error = layer->loadDefaultMetadata( ok );
      if ( !ok )
        QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Error loading layer metadata" ), error, Qgis::MessageLevel::Warning );

      break;
    }

    case QgsMapLayerType::AnnotationLayer:
    case QgsMapLayerType::GroupLayer:
      break;

    case QgsMapLayerType::PointCloudLayer:
    {
      bool ok = false;
      layer->loadDefaultStyle( ok );
      layer->loadDefaultMetadata( ok );

#ifdef HAVE_3D
      if ( !layer->renderer3D() )
      {
        QgsPointCloudLayer *pcLayer = qobject_cast< QgsPointCloudLayer * >( layer );
        // If the layer has no 3D renderer and syncing 3D to 2D renderer is enabled, we create a renderer and set it up with the 2D renderer
        if ( pcLayer->sync3DRendererTo2DRenderer() )
        {
          std::unique_ptr< QgsPointCloudLayer3DRenderer > renderer3D = std::make_unique< QgsPointCloudLayer3DRenderer >();
          renderer3D->convertFrom2DRenderer( pcLayer->renderer() );
          layer->setRenderer3D( renderer3D.release() );
        }
      }
#endif
      break;
    }
  }
}

void QgsAppLayerHandling::postProcessAddedLayers( const QList<QgsMapLayer *> &layers )
{
  std::map<QString, int> mapPathToReferenceCount;
  std::map<QString, QList< QgsWeakRelation >> mapPathToRelations;

  QgsProviderMetadata *ogrProviderMetadata = QgsProviderRegistry::instance()->providerMetadata( QStringLiteral( "ogr" ) );
  for ( QgsMapLayer *layer : layers )
  {
    switch ( layer->type() )
    {
      case QgsMapLayerType::VectorLayer:
      {
        QgsVectorLayer *vl = qobject_cast< QgsVectorLayer * >( layer );

        // try to automatically load related tables for OGR layers
        if ( vl->providerType() == QLatin1String( "ogr" ) )
        {
          const QVariantMap uriParts = ogrProviderMetadata->decodeUri( layer->source() );
          const QString layerName = uriParts.value( QStringLiteral( "layerName" ) ).toString();
          if ( layerName.isEmpty() )
            continue;

          // If this dataset is read more than once, collect and store all its
          // relationships
          const QString path = uriParts.value( QStringLiteral( "path" ) ).toString();
          if ( ++mapPathToReferenceCount[path] == 2 )
          {
            std::unique_ptr< QgsAbstractDatabaseProviderConnection > conn { QgsMapLayerUtils::databaseConnection( vl ) };
            if ( conn && ( conn->capabilities() & QgsAbstractDatabaseProviderConnection::Capability::RetrieveRelationships ) )
            {
              const QList< QgsWeakRelation > relations = conn->relationships( QString(), QString() );
              mapPathToRelations[path] = relations;
            }
          }

          // If this is a OGR dataset referenced by several layers, do not
          // open a new connection on it but reuse the results of the first pass
          auto iterMapPathToRelations = mapPathToRelations.find( path );
          if ( iterMapPathToRelations != mapPathToRelations.end() )
          {
            if ( !iterMapPathToRelations->second.isEmpty() )
            {
              QList< QgsWeakRelation > layerRelations;
              for ( const QgsWeakRelation &rel : std::as_const( iterMapPathToRelations->second ) )
              {
                const QVariantMap leftParts = ogrProviderMetadata->decodeUri( rel.referencedLayerSource() );
                const QString leftTableName = leftParts.value( QStringLiteral( "layerName" ) ).toString();
                if ( leftTableName == layerName )
                {
                  layerRelations << rel;
                }
              }
              if ( !layerRelations.isEmpty() )
              {
                vl->setWeakRelations( layerRelations );
                resolveVectorLayerDependencies( vl, QgsMapLayer::StyleCategory::Relations, QgsVectorLayerRef::MatchType::Source, DependencyFlag::LoadAllRelationships | DependencyFlag::SilentLoad );
                resolveVectorLayerWeakRelations( vl, QgsVectorLayerRef::MatchType::Source, true );
              }
            }
            continue;
          }

          // first need to create weak relations!!
          std::unique_ptr< QgsAbstractDatabaseProviderConnection > conn { QgsMapLayerUtils::databaseConnection( vl ) };
          if ( conn && ( conn->capabilities() & QgsAbstractDatabaseProviderConnection::Capability::RetrieveRelationships ) )
          {
            const QList< QgsWeakRelation > relations = conn->relationships( QString(), layerName );
            if ( !relations.isEmpty() )
            {
              vl->setWeakRelations( relations );
              resolveVectorLayerDependencies( vl, QgsMapLayer::StyleCategory::Relations, QgsVectorLayerRef::MatchType::Source, DependencyFlag::LoadAllRelationships | DependencyFlag::SilentLoad );
              resolveVectorLayerWeakRelations( vl, QgsVectorLayerRef::MatchType::Source, true );
            }
          }
        }
        break;
      }
      case QgsMapLayerType::RasterLayer:
      case QgsMapLayerType::PluginLayer:
      case QgsMapLayerType::MeshLayer:
      case QgsMapLayerType::VectorTileLayer:
      case QgsMapLayerType::AnnotationLayer:
      case QgsMapLayerType::PointCloudLayer:
      case QgsMapLayerType::GroupLayer:
        break;
    }
  }
}

QList< QgsMapLayer * > QgsAppLayerHandling::addOgrVectorLayers(
  const QStringList &layers,
  const QString &encoding,
  const QString &dataSourceType,
  bool &ok,
  bool showWarningOnInvalid )
{
  /*
  * 杨小兵-2024-03-18
  * 添加OGR向量图层的方法：
    1. **初始化和参数检查**：函数接受一个QStringList类型的图层列表，一个指定编码的QString，一个数据源类型的QString，一个布尔引用ok来标记操作是否成功，
    以及一个用于控制是否在无效图层时显示警告的布尔值。函数开始时，首先设置`ok = false`，初始化一些局部变量，如用于添加的图层列表`layersToAdd`和已添加
    图层列表`addedLayers`。
    
    2. **遍历图层URI处理**：对于每个图层URI，根据数据源类型（文件、数据库或其他）进行处理，尝试解析出基础名称`baseName`。这个基础名称可能会根据用户的设
    置进行格式化。
    
    3. **查询子图层**：使用`QgsProviderRegistry`查询每个URI的子图层。过滤掉非向量子图层，只保留向量类型的子图层。
    
    4. **处理子图层添加逻辑**：根据查询到的子图层信息，决定是否询问用户添加哪些子图层。这个决定基于用户的设置、子图层的数量和类型。如果只有一个子图层且用
    户设置为自动添加，或者在询问用户后，用户选择添加特定的子图层，这些子图层会被添加到`addedLayers`列表中。
    
    5. **错误处理和反馈**：如果URI代表的数据源无效，会根据`showWarningOnInvalid`参数决定是否显示警告消息。如果是由于数据源类型为`vsicurl`而失败，则会
    询问用户是否尝试使用"File"数据源类型重试。
    
    6. **图层注册与后处理**：成功添加的图层会被注册到`QgsProject`中，并对每个添加的图层执行后处理操作，如设置编码和请求用户输入坐标系转换信息。
    
    7. **返回值和操作结果**：函数返回添加的图层列表。如果至少有一个图层被成功添加，或者在询问用户子图层时用户选择了取消（但没有错误发生），`ok`会被设置
    为`true`。
    此方法的应用场景主要是在QGIS项目中动态地添加OGR支持的向量图层，处理过程考虑了多种数据源类型和用户交互情况，体现了灵活性和健壮性。从计算机实现的角度看，
  此方法利用了QGIS的图层管理和用户界面交互机制，通过条件判断、循环遍历和动态内存管理来实现功能，展现了现代C++编程的特点。


  */
  //note: this method ONLY supports vector layers from the OGR provider!
  ok = false;

  QgsCanvasRefreshBlocker refreshBlocker;

  QList<QgsMapLayer *> layersToAdd;
  QList<QgsMapLayer *> addedLayers;
  QgsSettings settings;
  bool userAskedToAddLayers = false;

  for ( const QString &layerUri : layers )
  {
    /*
    * 杨小兵-2024-03-18
    一、解释
      上述代码行中，`const QString uri = layerUri.trimmed();`的作用是创建一个新的常量字符串`uri`，它是`layerUri`去除前导和尾随空白（如空格、制表符、
    换行符等）后的结果。
    
    二、详细解释如下：
    - `layerUri`是一个`QString`类型的变量，它可能包含一个图层的URI（统一资源标识符），这个URI用于定位和访问数据。在GIS应用中，这些URI可能指向本地文件
    路径、数据库连接或网络资源等。
    - `trimmed()`是`QString`类的一个成员函数，它返回一个新的字符串，这个字符串是从原始字符串中移除了前导和尾随的空白字符后的结果。空白字符包括空格、制表
    符、回车符等。注意，`trimmed()`函数不会修改原始字符串。
    
    三、为什么需要使用`trimmed()`：
    1. **移除无意义的空白字符**：URI前后的空白字符在大多数情况下是无意义的，可能是由于不小心在输入时加入的，或者在复制粘贴过程中无意中包含进来的。这些空白
    字符如果不被移除，可能会导致URI解析失败或者找不到相应的资源。
    2. **提高鲁棒性**：在处理用户输入或外部数据时，总是存在格式不一致的可能性。通过`trimmed()`移除无关的空白字符，可以提高程序对输入数据的容错能力，确保即
    便在不理想的输入条件下也能正常工作。
    3. **统一数据格式**：在后续的处理过程中，统一的数据格式有助于减少错误和异常情况的发生。例如，如果后续的代码逻辑依赖于URI格式的一致性，去除前后的空白字
    符可以确保所有URI在格式上的一致性。
      综上，使用`trimmed()`是为了确保处理的URI格式正确、统一，并且提高了代码对于不规范输入的鲁棒性，是常见的字符串预处理步骤之一。
    */
    const QString uri = layerUri.trimmed();
    /*
    * 杨小兵-2024-03-18
    一、解释
      这段代码的作用是基于输入的数据源类型（如文件、数据库、目录或协议）和数据源URI，确定并设置一个基础名称`baseName`，这个基础名称用于后续创建图层或者进
    行其他相关操作。代码逻辑可以分为三部分：

    1. **处理文件类型数据源**:
       - 首先，创建一个`srcWithoutLayername`字符串，其值为`uri`的副本。这一步是为了处理可能包含在URI中的附加信息，如图层名称或其他参数。
       - 使用`indexOf('|')`查找`'|'`字符在URI中的位置，这通常用于分隔文件路径和图层名称或其他参数。如果找到了`'|'`字符，就使用`resize(posPipe)`方法截
       断字符串，移除管道符号（`'|'`）及其后的所有内容。这一步的目的是提取出纯粹的文件路径，无论是否附加了图层名或参数。
       - 调用`QgsProviderUtils::suggestLayerNameFromFilePath(srcWithoutLayername)`根据处理后的文件路径建议一个图层名称，这个名称被赋值给`baseName`。
       - 如果URI是指向ZIP或TAR文件中的图层（通过`vsiPrefix`检查得出），则会调用`askUserForZipItemLayers`函数询问用户是否要添加ZIP或TAR文件中的图层。如
       果用户同意，当前迭代会通过`continue`跳过，继续处理下一个URI。
    
    2. **处理数据库类型数据源**:
       - 调用`QgsProviderRegistry::instance()->decodeUri`方法解析URI，尝试从中提取数据库名称。如果成功提取到数据库名称，就使用这个名称作为`baseName`。
       如果没有提取到有效的数据库名称，就直接使用URI作为`baseName`。这反映了从数据库类型的数据源中提取基础名称的逻辑，其中基础名称可能是数据库的名称或者在
       无法确定数据库名称时使用URI。
    
    3. **处理目录或协议类型数据源**:
       - 直接调用`QgsProviderUtils::suggestLayerNameFromFilePath(uri)`根据URI建议一个图层名称。这表明，如果数据源类型不是文件也不是数据库（可能是目录
       或某种协议类型），则直接使用URI来建议一个图层名称。
      总结而言，这段代码的核心目的是根据不同类型的数据源URI提取或生成一个适当的图层基础名称`baseName`。这个基础名称可能是基于文件路径、数据库名称、或者直接根
    据URI生成的，依据于数据源的类型和特定的URI格式。这一处理过程对于确保图层名称的一致性和可识别性非常重要，尤其是在涉及多种数据源类型时。
    */
    QString baseName;
    if ( dataSourceType == QLatin1String( "file" ) )
    {
      QString srcWithoutLayername( uri );
      int posPipe = srcWithoutLayername.indexOf( '|' );
      if ( posPipe >= 0 )
        srcWithoutLayername.resize( posPipe );
      baseName = QgsProviderUtils::suggestLayerNameFromFilePath( srcWithoutLayername );

      // if needed prompt for zipitem layers
      QString vsiPrefix = QgsZipItem::vsiPrefix( uri );
      if ( ! uri.startsWith( QLatin1String( "/vsi" ), Qt::CaseInsensitive ) &&
           ( vsiPrefix == QLatin1String( "/vsizip/" ) || vsiPrefix == QLatin1String( "/vsitar/" ) ) )
      {
        if ( askUserForZipItemLayers( uri, { QgsMapLayerType::VectorLayer } ) )
          continue;
      }
    }
    else if ( dataSourceType == QLatin1String( "database" ) )
    {
      // Try to extract the database name and use it as base name
      // sublayers names (if any) will be appended to the layer name
      const QVariantMap parts( QgsProviderRegistry::instance()->decodeUri( QStringLiteral( "ogr" ), uri ) );
      if ( parts.value( QStringLiteral( "databaseName" ) ).isValid() )
        baseName = parts.value( QStringLiteral( "databaseName" ) ).toString();
      else
        baseName = uri;
    }
    else //directory //protocol
    {
      baseName = QgsProviderUtils::suggestLayerNameFromFilePath( uri );
    }

    if ( settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool() )
    {
      baseName = QgsMapLayer::formatLayerName( baseName );
    }

    QgsDebugMsgLevel( "completeBaseName: " + baseName, 2 );
    const bool isVsiCurl { uri.startsWith( QLatin1String( "/vsicurl" ), Qt::CaseInsensitive ) };
    const auto scheme { QUrl( uri ).scheme() };
    const bool isRemoteUrl { scheme.startsWith( QLatin1String( "http" ) ) || scheme == QLatin1String( "ftp" ) };

    std::unique_ptr< QgsTemporaryCursorOverride > cursorOverride;
    if ( isVsiCurl || isRemoteUrl )
    {
      cursorOverride = std::make_unique< QgsTemporaryCursorOverride >( Qt::WaitCursor );
      QgisApp::instance()->visibleMessageBar()->pushInfo( QObject::tr( "Remote layer" ), QObject::tr( "loading %1, please wait …" ).arg( uri ) );
      qApp->processEvents();
    }

    QList< QgsProviderSublayerDetails > sublayers = QgsProviderRegistry::instance()->providerMetadata( QStringLiteral( "ogr" ) )->querySublayers( uri, Qgis::SublayerQueryFlag::IncludeSystemTables );
    // filter out non-vector sublayers
    sublayers.erase( std::remove_if( sublayers.begin(), sublayers.end(), []( const QgsProviderSublayerDetails & sublayer )
    {
      return sublayer.type() != QgsMapLayerType::VectorLayer;
    } ), sublayers.end() );

    cursorOverride.reset();

    const QVariantMap uriParts = QgsProviderRegistry::instance()->decodeUri( QStringLiteral( "ogr" ), uri );
    const QString path = uriParts.value( QStringLiteral( "path" ) ).toString();

    if ( !sublayers.empty() )
    {
      userAskedToAddLayers = true;

      const bool detailsAreIncomplete = QgsProviderUtils::sublayerDetailsAreIncomplete( sublayers, QgsProviderUtils::SublayerCompletenessFlag::IgnoreUnknownFeatureCount );
      const bool singleSublayerOnly = sublayers.size() == 1;
      QString groupName;

      if ( !singleSublayerOnly || detailsAreIncomplete )
      {
        // ask user for sublayers (unless user settings dictate otherwise!)
        switch ( shouldAskUserForSublayers( sublayers ) )
        {
          case SublayerHandling::AskUser:
          {
            // prompt user for sublayers
            QgsProviderSublayersDialog dlg( uri, path, sublayers, {QgsMapLayerType::VectorLayer}, QgisApp::instance() );

            if ( dlg.exec() )
              sublayers = dlg.selectedLayers();
            else
              sublayers.clear(); // dialog was canceled, so don't add any sublayers
            groupName = dlg.groupName();
            break;
          }

          case SublayerHandling::LoadAll:
          {
            if ( detailsAreIncomplete )
            {
              // requery sublayers, resolving geometry types
              sublayers = QgsProviderRegistry::instance()->querySublayers( uri, Qgis::SublayerQueryFlag::ResolveGeometryType );
              // filter out non-vector sublayers
              sublayers.erase( std::remove_if( sublayers.begin(), sublayers.end(), []( const QgsProviderSublayerDetails & sublayer )
              {
                return sublayer.type() != QgsMapLayerType::VectorLayer;
              } ), sublayers.end() );
            }
            break;
          }

          case SublayerHandling::AbortLoading:
            sublayers.clear(); // don't add any sublayers
            break;
        };
      }
      else if ( detailsAreIncomplete )
      {
        // requery sublayers, resolving geometry types
        sublayers = QgsProviderRegistry::instance()->querySublayers( uri, Qgis::SublayerQueryFlag::ResolveGeometryType );
        // filter out non-vector sublayers
        sublayers.erase( std::remove_if( sublayers.begin(), sublayers.end(), []( const QgsProviderSublayerDetails & sublayer )
        {
          return sublayer.type() != QgsMapLayerType::VectorLayer;
        } ), sublayers.end() );
      }

      // now add sublayers
      if ( !sublayers.empty() )
      {
        addedLayers << addSublayers( sublayers, baseName, groupName );
      }

    }
    else
    {
      QString msg = QObject::tr( "%1 is not a valid or recognized data source." ).arg( uri );
      // If the failed layer was a vsicurl type, give the user a chance to try the normal download.
      if ( isVsiCurl &&
           QMessageBox::question( QgisApp::instance(), QObject::tr( "Invalid Data Source" ),
                                  QObject::tr( "Download with \"Protocol\" source type has failed, do you want to try the \"File\" source type?" ) ) == QMessageBox::Yes )
      {
        QString fileUri = uri;
        fileUri.replace( QLatin1String( "/vsicurl/" ), " " );
        return addOgrVectorLayers( QStringList() << fileUri, encoding, dataSourceType, showWarningOnInvalid );
      }
      else if ( showWarningOnInvalid )
      {
        QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Invalid Data Source" ), msg, Qgis::MessageLevel::Critical );
      }
    }
  }

  // make sure at least one layer was successfully added
  if ( layersToAdd.isEmpty() )
  {
    // we also return true if we asked the user for sublayers, but they choose none. In this case nothing
    // went wrong, so we shouldn't return false and cause GUI warnings to appear
    ok = userAskedToAddLayers || !addedLayers.isEmpty();
  }

  // Register this layer with the layers registry
  QgsProject::instance()->addMapLayers( layersToAdd );
  for ( QgsMapLayer *l : std::as_const( layersToAdd ) )
  {
    QgisApp::instance()->askUserForDatumTransform( l->crs(), QgsProject::instance()->crs(), l );
    QgsAppLayerHandling::postProcessAddedLayer( l );
  }
  QgisApp::instance()->activateDeactivateLayerRelatedActions( QgisApp::instance()->activeLayer() );

  ok = true;
  addedLayers.append( layersToAdd );

  for ( QgsMapLayer *l : std::as_const( addedLayers ) )
  {
    if ( !encoding.isEmpty() )
    {
      if ( QgsVectorLayer *vl = qobject_cast< QgsVectorLayer * >( l ) )
        vl->setProviderEncoding( encoding );
    }
  }

  return addedLayers;
}

QgsPointCloudLayer *QgsAppLayerHandling::addPointCloudLayer( const QString &uri, const QString &baseName, const QString &provider, bool showWarningOnInvalid )
{
  QgsCanvasRefreshBlocker refreshBlocker;
  QgsSettings settings;

  QString base( baseName );

  if ( settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool() )
  {
    base = QgsMapLayer::formatLayerName( base );
  }

  QgsDebugMsgLevel( "completeBaseName: " + base, 2 );

  // create the layer
  std::unique_ptr<QgsPointCloudLayer> layer( new QgsPointCloudLayer( uri, base, provider ) );

  if ( !layer || !layer->isValid() )
  {
    if ( showWarningOnInvalid )
    {
      QString msg = QObject::tr( "%1 is not a valid or recognized data source, error: \"%2\"" ).arg( uri, layer->error().message( QgsErrorMessage::Format::Text ) );
      QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Invalid Data Source" ), msg, Qgis::MessageLevel::Critical );
    }

    // since the layer is bad, stomp on it
    return nullptr;
  }

  QgsAppLayerHandling::postProcessAddedLayer( layer.get() );


  QgsProject::instance()->addMapLayer( layer.get() );
  QgisApp::instance()->activateDeactivateLayerRelatedActions( QgisApp::instance()->activeLayer() );

  return layer.release();
}

QgsPluginLayer *QgsAppLayerHandling::addPluginLayer( const QString &uri, const QString &baseName, const QString &provider )
{
  QgsPluginLayer *layer = QgsApplication::pluginLayerRegistry()->createLayer( provider, uri );
  if ( !layer )
    return nullptr;

  layer->setName( baseName );

  QgsProject::instance()->addMapLayer( layer );

  return layer;
}

QgsVectorTileLayer *QgsAppLayerHandling::addVectorTileLayer( const QString &uri, const QString &baseName, bool showWarningOnInvalid )
{
  QgsCanvasRefreshBlocker refreshBlocker;
  QgsSettings settings;

  QString base( baseName );

  if ( settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool() )
  {
    base = QgsMapLayer::formatLayerName( base );
  }

  QgsDebugMsgLevel( "completeBaseName: " + base, 2 );

  // create the layer
  const QgsVectorTileLayer::LayerOptions options( QgsProject::instance()->transformContext() );
  std::unique_ptr<QgsVectorTileLayer> layer( new QgsVectorTileLayer( uri, base, options ) );

  if ( !layer || !layer->isValid() )
  {
    if ( showWarningOnInvalid )
    {
      QString msg = QObject::tr( "%1 is not a valid or recognized data source." ).arg( uri );
      QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Invalid Data Source" ), msg, Qgis::MessageLevel::Critical );
    }

    // since the layer is bad, stomp on it
    return nullptr;
  }

  QgsAppLayerHandling::postProcessAddedLayer( layer.get() );

  QgsProject::instance()->addMapLayer( layer.get() );
  QgisApp::instance()->activateDeactivateLayerRelatedActions( QgisApp::instance()->activeLayer() );

  return layer.release();
}

bool QgsAppLayerHandling::askUserForZipItemLayers( const QString &path, const QList<QgsMapLayerType> &acceptableTypes )
{
  // query sublayers
  QList< QgsProviderSublayerDetails > sublayers = QgsProviderRegistry::instance()->querySublayers( path, Qgis::SublayerQueryFlag::IncludeSystemTables );

  // filter out non-matching sublayers
  sublayers.erase( std::remove_if( sublayers.begin(), sublayers.end(), [acceptableTypes]( const QgsProviderSublayerDetails & sublayer )
  {
    return !acceptableTypes.empty() && !acceptableTypes.contains( sublayer.type() );
  } ), sublayers.end() );

  if ( sublayers.empty() )
    return false;

  const bool detailsAreIncomplete = QgsProviderUtils::sublayerDetailsAreIncomplete( sublayers, QgsProviderUtils::SublayerCompletenessFlag::IgnoreUnknownFeatureCount );
  const bool singleSublayerOnly = sublayers.size() == 1;
  QString groupName;

  if ( !singleSublayerOnly || detailsAreIncomplete )
  {
    // ask user for sublayers (unless user settings dictate otherwise!)
    switch ( shouldAskUserForSublayers( sublayers ) )
    {
      case SublayerHandling::AskUser:
      {
        // prompt user for sublayers
        QgsProviderSublayersDialog dlg( path, path, sublayers, acceptableTypes, QgisApp::instance() );

        if ( dlg.exec() )
          sublayers = dlg.selectedLayers();
        else
          sublayers.clear(); // dialog was canceled, so don't add any sublayers
        groupName = dlg.groupName();
        break;
      }

      case SublayerHandling::LoadAll:
      {
        if ( detailsAreIncomplete )
        {
          // requery sublayers, resolving geometry types
          sublayers = QgsProviderRegistry::instance()->querySublayers( path, Qgis::SublayerQueryFlag::ResolveGeometryType );
          sublayers.erase( std::remove_if( sublayers.begin(), sublayers.end(), [acceptableTypes]( const QgsProviderSublayerDetails & sublayer )
          {
            return !acceptableTypes.empty() && !acceptableTypes.contains( sublayer.type() );
          } ), sublayers.end() );
        }
        break;
      }

      case SublayerHandling::AbortLoading:
        sublayers.clear(); // don't add any sublayers
        break;
    };
  }
  else if ( detailsAreIncomplete )
  {
    // requery sublayers, resolving geometry types
    sublayers = QgsProviderRegistry::instance()->querySublayers( path, Qgis::SublayerQueryFlag::ResolveGeometryType );
    sublayers.erase( std::remove_if( sublayers.begin(), sublayers.end(), [acceptableTypes]( const QgsProviderSublayerDetails & sublayer )
    {
      return !acceptableTypes.empty() && !acceptableTypes.contains( sublayer.type() );
    } ), sublayers.end() );
  }

  // now add sublayers
  if ( !sublayers.empty() )
  {
    QgsCanvasRefreshBlocker refreshBlocker;
    QgsSettings settings;

    QString base = QgsProviderUtils::suggestLayerNameFromFilePath( path );
    if ( settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool() )
    {
      base = QgsMapLayer::formatLayerName( base );
    }

    addSublayers( sublayers, base, groupName );
    QgisApp::instance()->activateDeactivateLayerRelatedActions( QgisApp::instance()->activeLayer() );
  }

  return true;
}

QgsAppLayerHandling::SublayerHandling QgsAppLayerHandling::shouldAskUserForSublayers( const QList<QgsProviderSublayerDetails> &layers, bool hasNonLayerItems )
{
  if ( hasNonLayerItems )
    return SublayerHandling::AskUser;

  QgsSettings settings;
  const Qgis::SublayerPromptMode promptLayers = settings.enumValue( QStringLiteral( "qgis/promptForSublayers" ), Qgis::SublayerPromptMode::AlwaysAsk );

  switch ( promptLayers )
  {
    case Qgis::SublayerPromptMode::AlwaysAsk:
      return SublayerHandling::AskUser;

    case Qgis::SublayerPromptMode::AskExcludingRasterBands:
    {
      // if any non-raster layers are found, we ask the user. Otherwise we load all
      for ( const QgsProviderSublayerDetails &sublayer : layers )
      {
        if ( sublayer.type() != QgsMapLayerType::RasterLayer )
          return SublayerHandling::AskUser;
      }
      return SublayerHandling::LoadAll;
    }

    case Qgis::SublayerPromptMode::NeverAskSkip:
      return SublayerHandling::AbortLoading;

    case Qgis::SublayerPromptMode::NeverAskLoadAll:
      return SublayerHandling::LoadAll;
  }

  return SublayerHandling::AskUser;
}

QList<QgsMapLayer *> QgsAppLayerHandling::addSublayers( const QList<QgsProviderSublayerDetails> &layers, const QString &baseName, const QString &groupName )
{
  QgsLayerTreeGroup *group = nullptr;
  if ( !groupName.isEmpty() )
  {
    int index { 0 };
    QgsLayerTreeNode *currentNode { QgisApp::instance()->layerTreeView()->currentNode() };
    if ( currentNode && currentNode->parent() )
    {
      if ( QgsLayerTree::isGroup( currentNode ) )
      {
        group = qobject_cast<QgsLayerTreeGroup *>( currentNode )->insertGroup( 0, groupName );
      }
      else if ( QgsLayerTree::isLayer( currentNode ) )
      {
        const QList<QgsLayerTreeNode *> currentNodeSiblings { currentNode->parent()->children() };
        int nodeIdx { 0 };
        for ( const QgsLayerTreeNode *child : std::as_const( currentNodeSiblings ) )
        {
          nodeIdx++;
          if ( child == currentNode )
          {
            index = nodeIdx;
            break;
          }
        }
        group = qobject_cast<QgsLayerTreeGroup *>( currentNode->parent() )->insertGroup( index, groupName );
      }
      else
      {
        group = QgsProject::instance()->layerTreeRoot()->insertGroup( 0, groupName );
      }
    }
    else
    {
      group = QgsProject::instance()->layerTreeRoot()->insertGroup( 0, groupName );
    }
  }

  QgsSettings settings;
  const bool formatLayerNames = settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool();

  // if we aren't adding to a group, we need to add the layers in reverse order so that they maintain the correct
  // order in the layer tree!
  QList<QgsProviderSublayerDetails> sortedLayers = layers;
  if ( groupName.isEmpty() )
  {
    std::reverse( sortedLayers.begin(), sortedLayers.end() );
  }

  QList< QgsMapLayer * > result;
  result.reserve( sortedLayers.size() );

  QgsOgrProviderUtils::DeferDatasetClosing deferDatasetClosing;

  for ( const QgsProviderSublayerDetails &sublayer : std::as_const( sortedLayers ) )
  {
    QgsProviderSublayerDetails::LayerOptions options( QgsProject::instance()->transformContext() );
    options.loadDefaultStyle = false;

    std::unique_ptr<QgsMapLayer> layer( sublayer.toLayer( options ) );
    if ( !layer )
      continue;

    QgsMapLayer *ml = layer.get();
    // if we aren't adding to a group, then we're iterating the layers in the reverse order
    // so account for that in the returned list of layers
    if ( groupName.isEmpty() )
      result.insert( 0, ml );
    else
      result << ml;

    QString layerName = layer->name();
    if ( formatLayerNames )
    {
      layerName = QgsMapLayer::formatLayerName( layerName );
    }

    const bool projectWasEmpty = QgsProject::instance()->mapLayers().empty();

    // if user has opted to add sublayers to a group, then we don't need to include the
    // filename in the layer's name, because the group is already titled with the filename.
    // But otherwise, we DO include the file name so that users can differentiate the source
    // when multiple layers are loaded from a GPX file or similar (refs https://github.com/qgis/QGIS/issues/37551)
    if ( group )
    {
      if ( !layerName.isEmpty() )
        layer->setName( layerName );
      else if ( !baseName.isEmpty() )
        layer->setName( baseName );
      QgsProject::instance()->addMapLayer( layer.release(), false );
      group->addLayer( ml );
    }
    else
    {
      if ( layerName != baseName && !layerName.isEmpty() && !baseName.isEmpty() )
        layer->setName( QStringLiteral( "%1 — %2" ).arg( baseName, layerName ) );
      else if ( !layerName.isEmpty() )
        layer->setName( layerName );
      else if ( !baseName.isEmpty() )
        layer->setName( baseName );
      QgsProject::instance()->addMapLayer( layer.release() );
    }

    // Some of the logic relating to matching a new project's CRS to the first layer added CRS is deferred to happen when the event loop
    // next runs -- so in those cases we can't assume that the project's CRS has been matched to the actual desired CRS yet.
    // In these cases we don't need to show the coordinate operation selection choice, so just hardcode an exception in here to avoid that...
    QgsCoordinateReferenceSystem projectCrsAfterLayerAdd = QgsProject::instance()->crs();
    const QgsGui::ProjectCrsBehavior projectCrsBehavior = QgsSettings().enumValue( QStringLiteral( "/projections/newProjectCrsBehavior" ),  QgsGui::UseCrsOfFirstLayerAdded, QgsSettings::App );
    switch ( projectCrsBehavior )
    {
      case QgsGui::UseCrsOfFirstLayerAdded:
      {
        if ( projectWasEmpty )
          projectCrsAfterLayerAdd = ml->crs();
        break;
      }

      case QgsGui::UsePresetCrs:
        break;
    }

    QgisApp::instance()->askUserForDatumTransform( ml->crs(), projectCrsAfterLayerAdd, ml );
  }

  if ( group )
  {
    // Respect if user don't want the new group of layers visible.
    QgsSettings settings;
    const bool newLayersVisible = settings.value( QStringLiteral( "/qgis/new_layers_visible" ), true ).toBool();
    if ( !newLayersVisible )
      group->setItemVisibilityCheckedRecursive( newLayersVisible );
  }

  // Post process all added layers
  for ( QgsMapLayer *ml : std::as_const( result ) )
  {
    QgsAppLayerHandling::postProcessAddedLayer( ml );
  }

  return result;
}

QList< QgsMapLayer * > QgsAppLayerHandling::openLayer( const QString &fileName, bool &ok, bool allowInteractive, bool suppressBulkLayerPostProcessing )
{
  QList< QgsMapLayer * > openedLayers;
  auto postProcessAddedLayers = [suppressBulkLayerPostProcessing, &openedLayers]
  {
    if ( !suppressBulkLayerPostProcessing )
      QgsAppLayerHandling::postProcessAddedLayers( openedLayers );
  };

  ok = false;
  const QFileInfo fileInfo( fileName );

  // highest priority = delegate to provider registry to handle
  const QList< QgsProviderRegistry::ProviderCandidateDetails > candidateProviders = QgsProviderRegistry::instance()->preferredProvidersForUri( fileName );
  if ( candidateProviders.size() == 1 && candidateProviders.at( 0 ).layerTypes().size() == 1 )
  {
    // one good candidate provider and possible layer type -- that makes things nice and easy!
    switch ( candidateProviders.at( 0 ).layerTypes().at( 0 ) )
    {
      case QgsMapLayerType::VectorLayer:
      case QgsMapLayerType::RasterLayer:
      case QgsMapLayerType::MeshLayer:
      case QgsMapLayerType::AnnotationLayer:
      case QgsMapLayerType::PluginLayer:
      case QgsMapLayerType::VectorTileLayer:
      case QgsMapLayerType::GroupLayer:
        // not supported here yet!
        break;

      case QgsMapLayerType::PointCloudLayer:
      {
        if ( QgsPointCloudLayer *layer = addPointCloudLayer( fileName, fileInfo.completeBaseName(), candidateProviders.at( 0 ).metadata()->key(), true ) )
        {
          ok = true;
          openedLayers << layer;
        }
        else
        {
          // The layer could not be loaded and the reason has been reported by the provider, we can exit now
          return {};
        }
        break;
      }
    }
  }

  if ( ok )
  {
    postProcessAddedLayers();
    return openedLayers;
  }

  CPLPushErrorHandler( CPLQuietErrorHandler );

  // if needed prompt for zipitem layers
  QString vsiPrefix = QgsZipItem::vsiPrefix( fileName );
  if ( vsiPrefix == QLatin1String( "/vsizip/" ) || vsiPrefix == QLatin1String( "/vsitar/" ) )
  {
    if ( askUserForZipItemLayers( fileName, {} ) )
    {
      CPLPopErrorHandler();
      ok = true;
      return openedLayers;
    }
  }

  if ( fileName.endsWith( QStringLiteral( ".mbtiles" ), Qt::CaseInsensitive ) )
  {
    QgsMbTiles reader( fileName );
    if ( reader.open() )
    {
      if ( reader.metadataValue( "format" ) == QLatin1String( "pbf" ) )
      {
        // these are vector tiles
        QUrlQuery uq;
        uq.addQueryItem( QStringLiteral( "type" ), QStringLiteral( "mbtiles" ) );
        uq.addQueryItem( QStringLiteral( "url" ), fileName );
        const QgsVectorTileLayer::LayerOptions options( QgsProject::instance()->transformContext() );
        std::unique_ptr<QgsVectorTileLayer> vtLayer( new QgsVectorTileLayer( uq.toString(), fileInfo.completeBaseName(), options ) );
        if ( vtLayer->isValid() )
        {
          openedLayers << vtLayer.get();
          QgsProject::instance()->addMapLayer( vtLayer.release() );
          postProcessAddedLayers();
          ok = true;
          return openedLayers;
        }
      }
      else // raster tiles
      {
        // prefer to use WMS provider's implementation to open MBTiles rasters
        QUrlQuery uq;
        uq.addQueryItem( QStringLiteral( "type" ), QStringLiteral( "mbtiles" ) );
        uq.addQueryItem( QStringLiteral( "url" ), QUrl::fromLocalFile( fileName ).toString() );
        if ( QgsRasterLayer *rasterLayer = addRasterLayer( uq.toString(), fileInfo.completeBaseName(), QStringLiteral( "wms" ) ) )
        {
          openedLayers << rasterLayer;
          postProcessAddedLayers();
          ok = true;
          return openedLayers;
        }
      }
    }
  }
  else if ( fileName.endsWith( QStringLiteral( ".vtpk" ), Qt::CaseInsensitive ) )
  {
    // these are vector tiles
    QUrlQuery uq;
    uq.addQueryItem( QStringLiteral( "type" ), QStringLiteral( "vtpk" ) );
    uq.addQueryItem( QStringLiteral( "url" ), fileName );
    const QgsVectorTileLayer::LayerOptions options( QgsProject::instance()->transformContext() );
    std::unique_ptr<QgsVectorTileLayer> vtLayer( new QgsVectorTileLayer( uq.toString(), fileInfo.completeBaseName(), options ) );
    if ( vtLayer->isValid() )
    {
      openedLayers << vtLayer.get();
      QgsAppLayerHandling::postProcessAddedLayer( vtLayer.get() );
      QgsProject::instance()->addMapLayer( vtLayer.release() );
      postProcessAddedLayers();
      ok = true;
      return openedLayers;
    }
  }

  QList< QgsProviderSublayerModel::NonLayerItem > nonLayerItems;
  if ( QgsProjectStorage *ps = QgsApplication::projectStorageRegistry()->projectStorageFromUri( fileName ) )
  {
    const QStringList projects = ps->listProjects( fileName );
    for ( const QString &project : projects )
    {
      QgsProviderSublayerModel::NonLayerItem projectItem;
      projectItem.setType( QStringLiteral( "project" ) );
      projectItem.setName( project );
      projectItem.setUri( QStringLiteral( "%1://%2?projectName=%3" ).arg( ps->type(), fileName, project ) );
      projectItem.setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/mIconQgsProjectFile.svg" ) ) );
      nonLayerItems << projectItem;
    }
  }

  // query sublayers
  QList< QgsProviderSublayerDetails > sublayers = QgsProviderRegistry::instance()->querySublayers( fileName, Qgis::SublayerQueryFlag::IncludeSystemTables );

  if ( !sublayers.empty() || !nonLayerItems.empty() )
  {
    const bool detailsAreIncomplete = QgsProviderUtils::sublayerDetailsAreIncomplete( sublayers, QgsProviderUtils::SublayerCompletenessFlag::IgnoreUnknownFeatureCount );
    const bool singleSublayerOnly = sublayers.size() == 1;
    QString groupName;

    if ( allowInteractive && ( !singleSublayerOnly || detailsAreIncomplete || !nonLayerItems.empty() ) )
    {
      // ask user for sublayers (unless user settings dictate otherwise!)
      switch ( shouldAskUserForSublayers( sublayers, !nonLayerItems.empty() ) )
      {
        case SublayerHandling::AskUser:
        {
          // prompt user for sublayers
          QgsProviderSublayersDialog dlg( fileName, fileName, sublayers, {}, QgisApp::instance() );
          dlg.setNonLayerItems( nonLayerItems );

          if ( dlg.exec() )
          {
            sublayers = dlg.selectedLayers();
            nonLayerItems = dlg.selectedNonLayerItems();
          }
          else
          {
            sublayers.clear(); // dialog was canceled, so don't add any sublayers
            nonLayerItems.clear();
          }
          groupName = dlg.groupName();
          break;
        }

        case SublayerHandling::LoadAll:
        {
          if ( detailsAreIncomplete )
          {
            // requery sublayers, resolving geometry types
            sublayers = QgsProviderRegistry::instance()->querySublayers( fileName, Qgis::SublayerQueryFlag::ResolveGeometryType );
          }
          break;
        }

        case SublayerHandling::AbortLoading:
          sublayers.clear(); // don't add any sublayers
          break;
      };
    }
    else if ( detailsAreIncomplete )
    {
      // requery sublayers, resolving geometry types
      sublayers = QgsProviderRegistry::instance()->querySublayers( fileName, Qgis::SublayerQueryFlag::ResolveGeometryType );
    }

    ok = true;

    // now add sublayers
    if ( !sublayers.empty() )
    {
      QgsCanvasRefreshBlocker refreshBlocker;
      QgsSettings settings;

      QString base = QgsProviderUtils::suggestLayerNameFromFilePath( fileName );
      if ( settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool() )
      {
        base = QgsMapLayer::formatLayerName( base );
      }

      openedLayers.append( addSublayers( sublayers, base, groupName ) );
      QgisApp::instance()->activateDeactivateLayerRelatedActions( QgisApp::instance()->activeLayer() );
    }
    else if ( !nonLayerItems.empty() )
    {
      ok = true;
      QgsCanvasRefreshBlocker refreshBlocker;
      if ( QgisApp::instance()->checkTasksDependOnProject() )
        return {};

      // possibly save any pending work before opening a different project
      if ( QgisApp::instance()->checkUnsavedLayerEdits() && QgisApp::instance()->checkMemoryLayers() && QgisApp::instance()->saveDirty() )
      {
        // error handling and reporting is in addProject() function
        QgisApp::instance()->addProject( nonLayerItems.at( 0 ).uri() );
      }
      return {};
    }
  }

  CPLPopErrorHandler();

  if ( !ok )
  {
    // maybe a known file type, which couldn't be opened due to a missing dependency... (eg. las for a non-pdal-enabled build)
    QgsProviderRegistry::UnusableUriDetails details;
    if ( QgsProviderRegistry::instance()->handleUnusableUri( fileName, details ) )
    {
      ok = true;

      if ( details.detailedWarning.isEmpty() )
        QgisApp::instance()->visibleMessageBar()->pushMessage( QString(), details.warning, Qgis::MessageLevel::Critical );
      else
        QgisApp::instance()->visibleMessageBar()->pushMessage( QString(), details.warning, details.detailedWarning, Qgis::MessageLevel::Critical );
    }
  }

  if ( !ok )
  {
    // we have no idea what this file is...
    QgsMessageLog::logMessage( QObject::tr( "Unable to load %1" ).arg( fileName ) );

    const QString msg = QObject::tr( "%1 is not a valid or recognized data source." ).arg( fileName );
    QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Invalid Data Source" ), msg, Qgis::MessageLevel::Critical );
  }
  else
  {
    postProcessAddedLayers();
  }

  return openedLayers;
}

QgsVectorLayer *QgsAppLayerHandling::addVectorLayer( const QString &uri, const QString &baseName, const QString &provider )
{
  return addLayerPrivate< QgsVectorLayer >( QgsMapLayerType::VectorLayer, uri, baseName, !provider.isEmpty() ? provider : QLatin1String( "ogr" ), true );
}

QgsRasterLayer *QgsAppLayerHandling::addRasterLayer( const QString &uri, const QString &baseName, const QString &provider )
{
  return addLayerPrivate< QgsRasterLayer >( QgsMapLayerType::RasterLayer, uri, baseName, !provider.isEmpty() ? provider : QLatin1String( "gdal" ), true );
}

QgsMeshLayer *QgsAppLayerHandling::addMeshLayer( const QString &uri, const QString &baseName, const QString &provider )
{
  return addLayerPrivate< QgsMeshLayer >( QgsMapLayerType::MeshLayer, uri, baseName, provider, true );
}

QList<QgsMapLayer *> QgsAppLayerHandling::addGdalRasterLayers( const QStringList &uris, bool &ok, bool showWarningOnInvalid )
{
  ok = false;
  if ( uris.empty() )
  {
    return {};
  }

  QgsCanvasRefreshBlocker refreshBlocker;

  // this is messy since some files in the list may be rasters and others may
  // be ogr layers. We'll set returnValue to false if one or more layers fail
  // to load.

  QList< QgsMapLayer * > res;

  for ( const QString &uri : uris )
  {
    QString errMsg;

    // if needed prompt for zipitem layers
    QString vsiPrefix = QgsZipItem::vsiPrefix( uri );
    if ( ( !uri.startsWith( QLatin1String( "/vsi" ), Qt::CaseInsensitive ) || uri.endsWith( QLatin1String( ".zip" ) ) || uri.endsWith( QLatin1String( ".tar" ) ) ) &&
         ( vsiPrefix == QLatin1String( "/vsizip/" ) || vsiPrefix == QLatin1String( "/vsitar/" ) ) )
    {
      if ( askUserForZipItemLayers( uri, { QgsMapLayerType::RasterLayer } ) )
        continue;
    }

    const bool isVsiCurl { uri.startsWith( QLatin1String( "/vsicurl" ), Qt::CaseInsensitive ) };
    const bool isRemoteUrl { uri.startsWith( QLatin1String( "http" ) ) || uri == QLatin1String( "ftp" ) };

    std::unique_ptr< QgsTemporaryCursorOverride > cursorOverride;
    if ( isVsiCurl || isRemoteUrl )
    {
      cursorOverride = std::make_unique< QgsTemporaryCursorOverride >( Qt::WaitCursor );
      QgisApp::instance()->visibleMessageBar()->pushInfo( QObject::tr( "Remote layer" ), QObject::tr( "loading %1, please wait …" ).arg( uri ) );
      qApp->processEvents();
    }

    if ( QgsRasterLayer::isValidRasterFileName( uri, errMsg ) )
    {
      QFileInfo myFileInfo( uri );

      // set the layer name to the file base name unless provided explicitly
      QString layerName;
      const QVariantMap uriDetails = QgsProviderRegistry::instance()->decodeUri( QStringLiteral( "gdal" ), uri );
      if ( !uriDetails[ QStringLiteral( "layerName" ) ].toString().isEmpty() )
      {
        layerName = uriDetails[ QStringLiteral( "layerName" ) ].toString();
      }
      else
      {
        layerName = QgsProviderUtils::suggestLayerNameFromFilePath( uri );
      }

      // try to create the layer
      cursorOverride.reset();
      QgsRasterLayer *layer = addLayerPrivate< QgsRasterLayer >( QgsMapLayerType::RasterLayer, uri, layerName, QStringLiteral( "gdal" ), showWarningOnInvalid );
      res << layer;

      if ( layer && layer->isValid() )
      {
        //only allow one copy of a ai grid file to be loaded at a
        //time to prevent the user selecting all adfs in 1 dir which
        //actually represent 1 coverage,

        if ( myFileInfo.fileName().endsWith( QLatin1String( ".adf" ), Qt::CaseInsensitive ) )
        {
          break;
        }
      }
      // if layer is invalid addLayerPrivate() will show the error

    } // valid raster filename
    else
    {
      ok = false;

      // Issue message box warning unless we are loading from cmd line since
      // non-rasters are passed to this function first and then successfully
      // loaded afterwards (see main.cpp)
      if ( showWarningOnInvalid )
      {
        QString msg = QObject::tr( "%1 is not a supported raster data source" ).arg( uri );
        if ( !errMsg.isEmpty() )
          msg += '\n' + errMsg;

        QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Unsupported Data Source" ), msg, Qgis::MessageLevel::Critical );
      }
    }
  }
  return res;
}

void QgsAppLayerHandling::addMapLayer( QgsMapLayer *mapLayer )
{
  QgsCanvasRefreshBlocker refreshBlocker;

  if ( mapLayer->isValid() )
  {
    // Register this layer with the layers registry
    QList<QgsMapLayer *> myList;
    myList << mapLayer;
    QgsProject::instance()->addMapLayers( myList );

    QgisApp::instance()->askUserForDatumTransform( mapLayer->crs(), QgsProject::instance()->crs(), mapLayer );
  }
  else
  {
    QString msg = QObject::tr( "The layer is not a valid layer and can not be added to the map" );
    QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Layer is not valid" ), msg, Qgis::MessageLevel::Critical );
  }
}

void QgsAppLayerHandling::openLayerDefinition( const QString &filename )
{
  QString errorMessage;
  QgsReadWriteContext context;
  bool loaded = false;

  QFile file( filename );
  if ( !file.open( QIODevice::ReadOnly ) )
  {
    errorMessage = QStringLiteral( "Can not open file" );
  }
  else
  {
    QDomDocument doc;
    QString message;
    if ( !doc.setContent( &file, &message ) )
    {
      errorMessage = message;
    }
    else
    {
      QFileInfo fileinfo( file );
      QDir::setCurrent( fileinfo.absoluteDir().path() );

      context.setPathResolver( QgsPathResolver( filename ) );
      context.setProjectTranslator( QgsProject::instance() );

      loaded = QgsLayerDefinition::loadLayerDefinition( doc, QgsProject::instance(), QgsProject::instance()->layerTreeRoot(), errorMessage, context );
    }
  }

  if ( loaded )
  {
    const QList< QgsReadWriteContext::ReadWriteMessage > messages = context.takeMessages();
    QVector< QgsReadWriteContext::ReadWriteMessage > shownMessages;
    for ( const QgsReadWriteContext::ReadWriteMessage &message : messages )
    {
      if ( shownMessages.contains( message ) )
        continue;

      QgisApp::instance()->visibleMessageBar()->pushMessage( QString(), message.message(), message.categories().join( '\n' ), message.level() );

      shownMessages.append( message );
    }
  }
  else if ( !loaded || !errorMessage.isEmpty() )
  {
    QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Error loading layer definition" ), errorMessage, Qgis::MessageLevel::Warning );
  }
}

void QgsAppLayerHandling::addLayerDefinition()
{
  QgsSettings settings;
  QString lastUsedDir = settings.value( QStringLiteral( "UI/lastQLRDir" ), QDir::homePath() ).toString();

  QString path = QFileDialog::getOpenFileName( QgisApp::instance(), QStringLiteral( "Add Layer Definition File" ), lastUsedDir, QStringLiteral( "*.qlr" ) );
  if ( path.isEmpty() )
    return;

  QFileInfo fi( path );
  settings.setValue( QStringLiteral( "UI/lastQLRDir" ), fi.path() );

  openLayerDefinition( path );
}

QList< QgsMapLayer * > QgsAppLayerHandling::addDatabaseLayers( const QStringList &layerPathList, const QString &providerKey, bool &ok )
{
  ok = false;
  QList<QgsMapLayer *> myList;

  if ( layerPathList.empty() )
  {
    // no layers to add so bail out, but
    // allow mMapCanvas to handle events
    // first
    return {};
  }

  QgsCanvasRefreshBlocker refreshBlocker;

  QApplication::setOverrideCursor( Qt::WaitCursor );

  const auto constLayerPathList = layerPathList;
  for ( const QString &layerPath : constLayerPathList )
  {
    // create the layer
    QgsDataSourceUri uri( layerPath );

    QgsVectorLayer::LayerOptions options { QgsProject::instance()->transformContext() };
    options.loadDefaultStyle = false;
    QgsVectorLayer *layer = new QgsVectorLayer( uri.uri( false ), uri.table(), providerKey, options );
    Q_CHECK_PTR( layer );

    if ( ! layer )
    {
      QApplication::restoreOverrideCursor();

      // XXX insert meaningful whine to the user here
      return {};
    }

    if ( layer->isValid() )
    {
      // add to list of layers to register
      //with the central layers registry
      myList << layer;
    }
    else
    {
      QgsMessageLog::logMessage( QObject::tr( "%1 is an invalid layer - not loaded" ).arg( layerPath ) );
      QLabel *msgLabel = new QLabel( QObject::tr( "%1 is an invalid layer and cannot be loaded. Please check the <a href=\"#messageLog\">message log</a> for further info." ).arg( layerPath ), QgisApp::instance()->messageBar() );
      msgLabel->setWordWrap( true );
      QObject::connect( msgLabel, &QLabel::linkActivated, QgisApp::instance()->logDock(), &QWidget::show );
      QgsMessageBarItem *item = new QgsMessageBarItem( msgLabel, Qgis::MessageLevel::Warning );
      QgisApp::instance()->messageBar()->pushItem( item );
      delete layer;
    }
    //qWarning("incrementing iterator");
  }

  QgsProject::instance()->addMapLayers( myList );

  // load default style after adding to process readCustomSymbology signals
  const auto constMyList = myList;
  for ( QgsMapLayer *l : constMyList )
  {
    bool ok;
    l->loadDefaultStyle( ok );
    l->loadDefaultMetadata( ok );
  }

  QApplication::restoreOverrideCursor();

  ok = true;
  return myList;
}

/*
* 杨小兵-2024-02-24
  这个函数是用于在一个地理信息系统(GIS)软件中添加一个图层的私有方法。它利用了模板编程来允许不同类型的图层被添加，如矢量图层、栅格图层等。
该方法主要功能是根据提供的URI、名称、提供者键值和其他参数来创建并添加一个新的图层到项目中。接下来，我将详细解释这个函数的处理逻辑：

1. **初始化设置与阻止画布刷新**：首先，函数通过创建一个`QgsSettings`对象来读取QGIS的配置，并使用`QgsCanvasRefreshBlocker`来阻止在添
加图层时自动刷新画布，以提高性能。

2. **格式化图层名称**：根据用户的设置决定是否格式化图层的名称。如果用户设置了`qgis/formatLayerName`为`true`，则会格式化名称。

3. **处理认证需求**：通过正则表达式检查URI是否包含认证配置（例如，是否需要密码）。如果需要，且用户界面未被禁用，会提示用户输入密码。

4. **解析URI**：使用`QgsProviderRegistry`的`decodeUri`方法来解析URI，并可能通过`QgsPathResolver`处理路径，以确保所有内置路径和本地
化路径正确扩展。

5. **查询子图层能力**：检查数据提供者是否支持查询子图层。这是为了处理那些能够包含多个子图层的数据源，例如，一个单一的文件可能包含多个矢量图层。

6. **查询并处理子图层**：如果数据源支持子图层查询，函数会查询这些子图层，并根据类型过滤不匹配的子图层。如果有多个匹配的子图层，可能会根据用户
的设置提示用户选择。

7. **添加图层**：根据前面的步骤，可能会添加一个或多个子图层。如果不支持查询子图层，或者只有一个匹配的子图层，直接使用`QgsMapLayerFactory`
创建并添加图层。在图层被成功创建后，会根据设置再次格式化图层名称，并将图层添加到项目中。

8. **后处理**：对添加的图层进行一些后处理操作，如请求用户进行坐标系转换的确认，以及执行一些与添加的图层相关的后处理操作。

9. **更新UI状态**：最后，根据当前活动图层更新用户界面的状态，激活或禁用与图层相关的操作。

  整个过程体现了GIS软件在处理图层添加时的复杂性，包括对图层名称的格式化、对需要认证的数据源的处理、对包含多个子图层的数据源的支持以及与用户界面
的交互。这个函数体现了软件工程中的一些常见模式，比如条件逻辑、资源管理（如使用阻止器模式管理画布刷新）和用户交互。

二、总结
1、这里使用QgsMapLayerFactory函数创建并且添加图层
2、
*/
template<typename T>
T *QgsAppLayerHandling::addLayerPrivate( QgsMapLayerType type, const QString &uri, const QString &name, const QString &providerKey, bool guiWarnings )
{
#pragma region "初始化设置与阻止画布刷新、格式化图层名称、处理认证需求、解析URI"
  QgsSettings settings;

  QgsCanvasRefreshBlocker refreshBlocker;

  QString baseName = settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool() ? QgsMapLayer::formatLayerName( name ) : name;

  // if the layer needs authentication, ensure the master password is set
  const thread_local QRegularExpression rx( "authcfg=([a-z]|[A-Z]|[0-9]){7}" );
  if ( rx.match( uri ).hasMatch() )
  {
    if ( !QgsAuthGuiUtils::isDisabled( QgisApp::instance()->messageBar() ) )
    {
      QgsApplication::authManager()->setMasterPassword( true );
    }
  }

  QVariantMap uriElements = QgsProviderRegistry::instance()->decodeUri( providerKey, uri );
  QString path = uri;
  if ( uriElements.contains( QStringLiteral( "path" ) ) )
  {
    // run layer path through QgsPathResolver so that all inbuilt paths and other localised paths are correctly expanded
    path = QgsPathResolver().readPath( uriElements.value( QStringLiteral( "path" ) ).toString() );
    uriElements[ QStringLiteral( "path" ) ] = path;
  }
  // Not all providers implement decodeUri(), so use original uri if uriElements is empty
  const QString updatedUri = uriElements.isEmpty() ? uri : QgsProviderRegistry::instance()->encodeUri( providerKey, uriElements );
#pragma endregion

#pragma region "查询子图层能力、查询并处理子图层"
  const bool canQuerySublayers = QgsProviderRegistry::instance()->providerMetadata( providerKey ) &&
                                 ( QgsProviderRegistry::instance()->providerMetadata( providerKey )->capabilities() & QgsProviderMetadata::QuerySublayers );

  T *result = nullptr;
  //  如果存在子图层，那么执行if中的处理逻辑，如果不存在子图层，则执行else中的处理逻辑
  if ( canQuerySublayers )
  {
    // query sublayers
    QList< QgsProviderSublayerDetails > sublayers = QgsProviderRegistry::instance()->providerMetadata( providerKey ) ?
        QgsProviderRegistry::instance()->providerMetadata( providerKey )->querySublayers( updatedUri, Qgis::SublayerQueryFlag::IncludeSystemTables )
        : QgsProviderRegistry::instance()->querySublayers( updatedUri );

    // filter out non-matching sublayers
    sublayers.erase( std::remove_if( sublayers.begin(), sublayers.end(), [type]( const QgsProviderSublayerDetails & sublayer )
    {
      return sublayer.type() != type;
    } ), sublayers.end() );

    if ( sublayers.empty() )
    {
      if ( guiWarnings )
      {
        QString msg = QObject::tr( "%1 is not a valid or recognized data source." ).arg( uri );
        QgisApp::instance()->visibleMessageBar()->pushMessage( QObject::tr( "Invalid Data Source" ), msg, Qgis::MessageLevel::Critical );
      }

      // since the layer is bad, stomp on it
      return nullptr;
    }
    else if ( sublayers.size() > 1 || QgsProviderUtils::sublayerDetailsAreIncomplete( sublayers, QgsProviderUtils::SublayerCompletenessFlag::IgnoreUnknownFeatureCount ) )
    {
      // ask user for sublayers (unless user settings dictate otherwise!)
      switch ( shouldAskUserForSublayers( sublayers ) )
      {
        case SublayerHandling::AskUser:
        {
          QgsProviderSublayersDialog dlg( updatedUri, path, sublayers, {type}, QgisApp::instance() );
          if ( dlg.exec() )
          {
            const QList< QgsProviderSublayerDetails > selectedLayers = dlg.selectedLayers();
            if ( !selectedLayers.isEmpty() )
            {
              result = qobject_cast< T * >( addSublayers( selectedLayers, baseName, dlg.groupName() ).value( 0 ) );
            }
          }
          break;
        }
        case SublayerHandling::LoadAll:
        {
          result = qobject_cast< T * >( addSublayers( sublayers, baseName, QString() ).value( 0 ) );
          break;
        }
        case SublayerHandling::AbortLoading:
          break;
      };
    }
    else
    {
      result = qobject_cast< T * >( addSublayers( sublayers, name, QString() ).value( 0 ) );

      if ( result )
      {
        QString base( baseName );
        if ( settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool() )
        {
          base = QgsMapLayer::formatLayerName( base );
        }
        result->setName( base );
      }
    }
  }
  else
  {
    QgsMapLayerFactory::LayerOptions options( QgsProject::instance()->transformContext() );
    options.loadDefaultStyle = false;
    //  创建图层
    result = qobject_cast< T * >( QgsMapLayerFactory::createLayer( uri, name, type, options, providerKey ) );
    if ( result )
    {
      QString base( baseName );
      if ( settings.value( QStringLiteral( "qgis/formatLayerName" ), false ).toBool() )
      {
        base = QgsMapLayer::formatLayerName( base );
      }
      result->setName( base );
      //  添加图层
      QgsProject::instance()->addMapLayer( result );

      QgisApp::instance()->askUserForDatumTransform( result->crs(), QgsProject::instance()->crs(), result );
      QgsAppLayerHandling::postProcessAddedLayer( result );
    }
  }

#pragma endregion

#pragma region "更新UI状态"
  QgisApp::instance()->activateDeactivateLayerRelatedActions( QgisApp::instance()->activeLayer() );
  return result;
#pragma endregion

}

const QList<QgsVectorLayerRef> QgsAppLayerHandling::findBrokenLayerDependencies( QgsVectorLayer *vl, QgsMapLayer::StyleCategories categories, QgsVectorLayerRef::MatchType matchType, DependencyFlags dependencyFlags )
{
  QList<QgsVectorLayerRef> brokenDependencies;

  if ( categories.testFlag( QgsMapLayer::StyleCategory::Forms ) )
  {
    for ( int i = 0; i < vl->fields().count(); i++ )
    {
      const QgsEditorWidgetSetup setup = QgsGui::editorWidgetRegistry()->findBest( vl, vl->fields().field( i ).name() );
      QgsFieldFormatter *fieldFormatter = QgsApplication::fieldFormatterRegistry()->fieldFormatter( setup.type() );
      if ( fieldFormatter )
      {
        const QList<QgsVectorLayerRef> constDependencies { fieldFormatter->layerDependencies( setup.config() ) };
        for ( const QgsVectorLayerRef &dependency : constDependencies )
        {
          // I guess we need and isNull()/isValid() method for the ref
          if ( dependency.layer ||
               ! dependency.name.isEmpty() ||
               ! dependency.source.isEmpty() ||
               ! dependency.layerId.isEmpty() )
          {
            const QgsVectorLayer *depVl { QgsVectorLayerRef( dependency ).resolveWeakly( QgsProject::instance(), matchType ) };
            if ( ! depVl || ! depVl->isValid() )
            {
              brokenDependencies.append( dependency );
            }
          }
        }
      }
    }
  }

  if ( categories.testFlag( QgsMapLayer::StyleCategory::Relations ) )
  {
    // Check for layer weak relations
    const QList<QgsWeakRelation> weakRelations { vl->weakRelations() };
    for ( const QgsWeakRelation &weakRelation : weakRelations )
    {
      QList< QgsVectorLayerRef > dependencies;

      if ( !( dependencyFlags & DependencyFlag::LoadAllRelationships ) )
      {
        // This is the big question: do we really
        // want to automatically load the referencing layer(s) too?
        // This could potentially lead to a cascaded load of a
        // long list of layers.

        // for now, unless we are forcing load of all relationships we only consider relationships
        // where the referencing layer is a match.
        if ( weakRelation.referencingLayer().resolveWeakly( QgsProject::instance(), matchType ) != vl )
        {
          continue;
        }
      }

      switch ( weakRelation.cardinality() )
      {
        case Qgis::RelationshipCardinality::ManyToMany:
        {
          if ( !weakRelation.mappingTable().resolveWeakly( QgsProject::instance(), matchType ) )
            dependencies << weakRelation.mappingTable();
          FALLTHROUGH;
        }

        case Qgis::RelationshipCardinality::OneToOne:
        case Qgis::RelationshipCardinality::OneToMany:
        case Qgis::RelationshipCardinality::ManyToOne:
        {
          if ( !weakRelation.referencedLayer().resolveWeakly( QgsProject::instance(), matchType ) )
            dependencies << weakRelation.referencedLayer();

          if ( !weakRelation.referencingLayer().resolveWeakly( QgsProject::instance(), matchType ) )
            dependencies << weakRelation.referencingLayer();

          break;
        }
      }

      for ( const QgsVectorLayerRef &dependency : std::as_const( dependencies ) )
      {
        // Make sure we don't add it twice if it was already added by the form widgets check
        bool refFound = false;
        for ( const QgsVectorLayerRef &otherRef : std::as_const( brokenDependencies ) )
        {
          if ( ( !dependency.layerId.isEmpty() && dependency.layerId == otherRef.layerId )
               || ( dependency.source == otherRef.source && dependency.provider == otherRef.provider ) )
          {
            refFound = true;
            break;
          }
        }
        if ( ! refFound )
        {
          brokenDependencies.append( dependency );
        }
      }
    }
  }
  return brokenDependencies;
}

void QgsAppLayerHandling::resolveVectorLayerDependencies( QgsVectorLayer *vl, QgsMapLayer::StyleCategories categories, QgsVectorLayerRef::MatchType matchType, DependencyFlags dependencyFlags )
{
  if ( vl && vl->isValid() )
  {
    const QList<QgsVectorLayerRef> dependencies { findBrokenLayerDependencies( vl, categories, matchType, dependencyFlags ) };
    for ( const QgsVectorLayerRef &dependency : dependencies )
    {
      // Check for projects without layer dependencies (see 7e8c7b3d0e094737336ff4834ea2af625d2921bf)
      if ( QgsProject::instance()->mapLayer( dependency.layerId ) || ( dependency.name.isEmpty() && dependency.source.isEmpty() ) )
      {
        continue;
      }
      // try to aggressively resolve the broken dependencies
      QgsVectorLayer *loadedLayer = nullptr;
      const QString providerName { vl->dataProvider()->name() };
      QgsProviderMetadata *providerMetadata { QgsProviderRegistry::instance()->providerMetadata( providerName ) };
      if ( providerMetadata )
      {
        // Retrieve the DB connection (if any)

        std::unique_ptr< QgsAbstractDatabaseProviderConnection > conn { QgsMapLayerUtils::databaseConnection( vl ) };
        if ( conn )
        {
          QString tableSchema;
          QString tableName;
          const QVariantMap sourceParts = providerMetadata->decodeUri( dependency.source );

          // This part should really be abstracted out to the connection classes or to the providers directly.
          // Different providers decode the uri differently, for example we don't get the table name out of OGR
          // but the layerName/layerId instead, so let's try different approaches

          // This works for GPKG
          tableName = sourceParts.value( QStringLiteral( "layerName" ) ).toString();

          // This works for PG and spatialite
          if ( tableName.isEmpty() )
          {
            tableName = sourceParts.value( QStringLiteral( "table" ) ).toString();
            tableSchema = sourceParts.value( QStringLiteral( "schema" ) ).toString();
          }

          // Helper to find layers in connections
          auto layerFinder = [ &conn, &dependency, &providerName ]( const QString & tableSchema, const QString & tableName ) -> QgsVectorLayer *
          {
            // First try the current schema (or no schema if it's not supported from the provider)
            try
            {
              const QString layerUri { conn->tableUri( tableSchema, tableName )};
              // Aggressive doesn't mean stupid: check if a layer with the same URI
              // was already loaded, this catches a corner case for renamed/moved GPKGS
              // where the dependency was actually loaded but it was found as broken
              // because the source does not match anymore (for instance when loaded
              // from a style definition).
              QStringList layerUris;
              for ( auto it = QgsProject::instance()->mapLayers().cbegin(); it != QgsProject::instance()->mapLayers().cend(); ++it )
              {
                if ( it.value()->publicSource() == layerUri )
                {
                  return nullptr;
                }
              }
              // Load it!
              std::unique_ptr< QgsVectorLayer > newVl = std::make_unique< QgsVectorLayer >( layerUri, !dependency.name.isEmpty() ? dependency.name : tableName, providerName );
              if ( newVl->isValid() )
              {
                QgsVectorLayer *res = newVl.get();
                QgsProject::instance()->addMapLayer( newVl.release() );
                return res;
              }
            }
            catch ( QgsProviderConnectionException & )
            {
              // Do nothing!
            }
            return nullptr;
          };

          loadedLayer = layerFinder( tableSchema, tableName );

          // Try different schemas
          if ( ! loadedLayer && conn->capabilities().testFlag( QgsAbstractDatabaseProviderConnection::Capability::Schemas ) && ! tableSchema.isEmpty() )
          {
            const QStringList schemas { conn->schemas() };
            for ( const QString &schemaName : schemas )
            {
              if ( schemaName != tableSchema )
              {
                loadedLayer = layerFinder( schemaName, tableName );
              }
              if ( loadedLayer )
              {
                break;
              }
            }
          }
        }
      }
      if ( ! loadedLayer )
      {
        const QString msg { QObject::tr( "layer '%1' requires layer '%2' to be loaded but '%2' could not be found, please load it manually if possible." ).arg( vl->name(), dependency.name ) };
        QgisApp::instance()->messageBar()->pushWarning( QObject::tr( "Missing layer form dependency" ), msg );
      }
      else if ( !( dependencyFlags & DependencyFlag::SilentLoad ) )
      {
        QgisApp::instance()->messageBar()->pushSuccess( QObject::tr( "Missing layer form dependency" ), QObject::tr( "Layer dependency '%2' required by '%1' was automatically loaded." )
            .arg( vl->name(),
                  loadedLayer->name() ) );
      }
    }
  }
}

void QgsAppLayerHandling::resolveVectorLayerWeakRelations( QgsVectorLayer *vectorLayer, QgsVectorLayerRef::MatchType matchType, bool guiWarnings )
{
  if ( vectorLayer && vectorLayer->isValid() )
  {
    const QList<QgsWeakRelation> constWeakRelations { vectorLayer->weakRelations( ) };
    for ( const QgsWeakRelation &rel : constWeakRelations )
    {
      const QList< QgsRelation > relations { rel.resolvedRelations( QgsProject::instance(), matchType ) };
      for ( const QgsRelation &relation : relations )
      {
        if ( relation.isValid() )
        {
          // Avoid duplicates
          const QList<QgsRelation> constRelations { QgsProject::instance()->relationManager()->relations().values() };
          for ( const QgsRelation &other : constRelations )
          {
            if ( relation.hasEqualDefinition( other ) )
            {
              continue;
            }
          }
          QgsProject::instance()->relationManager()->addRelation( relation );
        }
        else if ( guiWarnings )
        {
          QgisApp::instance()->messageBar()->pushWarning( QObject::tr( "Invalid relationship %1" ).arg( relation.name() ), relation.validationError() );
        }
      }
    }
  }
}

void QgsAppLayerHandling::onVectorLayerStyleLoaded( QgsVectorLayer *vl, QgsMapLayer::StyleCategories categories )
{
  if ( vl && vl->isValid( ) )
  {

    // Check broken dependencies in forms
    if ( categories.testFlag( QgsMapLayer::StyleCategory::Forms ) )
    {
      resolveVectorLayerDependencies( vl );
    }

    // Check broken relations and try to restore them
    if ( categories.testFlag( QgsMapLayer::StyleCategory::Relations ) )
    {
      resolveVectorLayerWeakRelations( vl );
    }
  }
}

