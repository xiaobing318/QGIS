/*-------------QGIS-------------*/
#include "qgisinterface.h"
#include "qgsmessagebar.h"

/*-------------QT-------------*/
#include <QAction>

/*-------------语义融合工具相关头文件-------------*/
#include "vector_data_fusion_toolbox_plugin.h"

/*-------------（矢量要素）数据提取工具相关头文件-------------*/
#include "attribute_extract.h"

static const QString sName = QObject::tr("矢量数据融合工具箱");
static const QString sDescription = QObject::tr("对不同的矢量数据源（JBDX、OSM、S57等不同的数据源）进行融合处理");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 0.1");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
// 插件图标
static const QString sPluginIcon = QStringLiteral(":/vector_data_fusion_toolbox/icons/vector_data_fusion_toolbox.png");

QgsVectorDataFusionToolBox::QgsVectorDataFusionToolBox(QgisInterface* qgisInterface)
  : QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
  , mQGisIface(qgisInterface)
{
}

QgsVectorDataFusionToolBox::~QgsVectorDataFusionToolBox()
{
  //  在initGui中释放actions对象指针
}

void QgsVectorDataFusionToolBox::initGui()
{
  //  确保之前的actions对象指针都被释放了
  delete mAction_semantic_fusion;
  delete mActionAttributeExtractTool;
  
#pragma region "语义融合处理工具"
  // Create the action for tool
  mAction_semantic_fusion = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/semantic_fusion.png"), tr("语义融合处理工具"), this);
  mAction_semantic_fusion->setObjectName(QStringLiteral("mAction_semantic_fusion"));
  // Set the what's this text
  mAction_semantic_fusion->setWhatsThis(tr("JBDX、OSM、S57、SYDH数据源语义融合处理工具"));
  // Connect the action to the run
  connect(mAction_semantic_fusion, &QAction::triggered, this, &QgsVectorDataFusionToolBox::semantic_fusion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_semantic_fusion);
  mQGisIface->addPluginToVectorMenu(tr("&矢量数据语义融合处理工具箱"), mAction_semantic_fusion);
  mAction_semantic_fusion->setEnabled(true);

#pragma endregion

#pragma region "（矢量要素）数据提取工具"
  // Create the action for tool
  mActionAttributeExtractTool = new QAction(QIcon(":/vector_data_feature_extraction/icons/vector_data_feature_extraction.png"), tr("（矢量要素）数据提取工具"), this);
  mActionAttributeExtractTool->setObjectName(QStringLiteral("mActionAttributeExtractTool"));
  // Set the what's this text
  mActionAttributeExtractTool->setWhatsThis(tr("图层提取、属性字段提取、属性值提取"));
  // Connect the action to the run
  connect(mActionAttributeExtractTool, &QAction::triggered, this, &QgsVectorDataFusionToolBox::vector_data_feature_extraction);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mActionAttributeExtractTool);
  mQGisIface->addPluginToVectorMenu(tr("&矢量数据语义融合处理工具箱"), mActionAttributeExtractTool);
  mActionAttributeExtractTool->setEnabled(true);

#pragma endregion

#pragma region "其他工具"

#pragma endregion

}

#pragma region "矢量数据语义融合工具箱中相关的actions"
//  1、语义融合工具
void QgsVectorDataFusionToolBox::semantic_fusion()
{
  qgs_semantic_fusion* pDlg = new qgs_semantic_fusion(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

  if (!pDlg)
  {
    return;
  }
  else if(pDlg->exec() == QDialog::Accepted)
  {
    delete pDlg;
  }
}

//  2、（矢量要素）数据提取工具

void QgsVectorDataFusionToolBox::vector_data_feature_extraction()
{
  SE_AttributeExtractDlg* pDlg = new SE_AttributeExtractDlg(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

  if (!pDlg)
  {
    return;
  }
  else if (pDlg->exec() == QDialog::Accepted)
  {
    delete pDlg;
  }
}
//  3、其他工具
#pragma endregion

void QgsVectorDataFusionToolBox::unload()
{
  // remove the GUI

  //  1、语义融合处理工具
  mQGisIface->removePluginVectorMenu(tr("&矢量数据语义融合处理工具箱"), mAction_semantic_fusion);
  mQGisIface->removeVectorToolBarIcon(mAction_semantic_fusion);

  //  2、（矢量要素）数据提取工具
  mQGisIface->removePluginVectorMenu(tr("&矢量数据语义融合处理工具箱"), mActionAttributeExtractTool);
  mQGisIface->removeVectorToolBarIcon(mActionAttributeExtractTool);

  //  3、其他工具


  delete mAction_semantic_fusion;
  delete mActionAttributeExtractTool;
}

void QgsVectorDataFusionToolBox::help()
{
  // TODO: help
}


/**
 * Required extern functions needed  for every plugin
 * These functions can be called prior to creating an instance
 * of the plugin class
 */
 // Class factory to return a new instance of the plugin class
QGISEXTERN QgisPlugin* classFactory(QgisInterface* qgisInterfacePointer)
{
  return new QgsVectorDataFusionToolBox(qgisInterfacePointer);
}

// Return the name of the plugin - note that we do not user class members as
// the class may not yet be insantiated when this method is called.
QGISEXTERN const QString* name()
{
  return &sName;
}

// Return the description
QGISEXTERN const QString* description()
{
  return &sDescription;
}

// Return the category
QGISEXTERN const QString* category()
{
  return &sCategory;
}

// Return the type (either UI or MapLayer plugin)
QGISEXTERN int type()
{
  return sPluginType;
}

// Return the version number for the plugin
QGISEXTERN const QString* version()
{
  return &sPluginVersion;
}

QGISEXTERN const QString* icon()
{
  return &sPluginIcon;
}

// Delete ourself
QGISEXTERN void unload(QgisPlugin* pluginPointer)
{
  delete pluginPointer;
}
