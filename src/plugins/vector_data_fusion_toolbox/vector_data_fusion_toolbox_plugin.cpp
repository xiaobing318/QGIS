#pragma region "1 包含头文件（减少重复）"
/*-------------QT-------------*/
#include <QAction>

/*-------------QGIS-------------*/
#include "qgisinterface.h"
#include "qgsmessagebar.h"

/*-------------语义融合工具相关头文件-------------*/
#include "vector_data_fusion_toolbox_plugin.h"

#include "JJBDX_NJBDX_semantic_fusion.h"
#include "DZHT_NJBDX_semantic_fusion.h"
#include "JBHT_NJBDX_semantic_fusion.h"
#include "JYDH_NJBDX_semantic_fusion.h"
#include "OSM_NJBDX_semantic_fusion.h"
#include "QQCT_NJBDX_semantic_fusion.h"
#include "SW_NJBDX_semantic_fusion.h"

/*-------------矢量数据要素提取工具相关头文件-------------*/
#include "attribute_extract.h"

#pragma endregion

#pragma region "2 全局静态常量"
static const QString sName = QObject::tr("多源矢量数据语义融合工具箱");
static const QString sDescription = QObject::tr("对不同的矢量数据源（JJBDX、OSM、DZHT、JBHT、SW、JYDH、QQCT七种不同的数据源）进行语义融合处理");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
static const QString sPluginIcon = QStringLiteral(":/vector_data_fusion_toolbox/icons/vector_data_fusion_toolbox.svg");
#pragma endregion

#pragma region "3 QgsVectorDataFusionToolBox类函数"
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
  if (mAction_JJBDX_NJBDX_semantic_fusion)
  {
    delete mAction_JJBDX_NJBDX_semantic_fusion;
  }
  if (mAction_OSM_NJBDX_semantic_fusion)
  {
    delete mAction_OSM_NJBDX_semantic_fusion;
  }
  if (mAction_DZHT_NJBDX_semantic_fusion)
  {
    delete mAction_DZHT_NJBDX_semantic_fusion;
  }
  if (mAction_JBHT_NJBDX_semantic_fusion)
  {
    delete mAction_JBHT_NJBDX_semantic_fusion;
  }
  if (mAction_SW_NJBDX_semantic_fusion)
  {
    delete mAction_SW_NJBDX_semantic_fusion;
  }
  if (mAction_JYDH_NJBDX_semantic_fusion)
  {
    delete mAction_JYDH_NJBDX_semantic_fusion;
  }
  if (mAction_QQCT_NJBDX_semantic_fusion)
  {
    delete mAction_QQCT_NJBDX_semantic_fusion;
  }
  if (mActionAttributeExtractTool)
  {
    delete mActionAttributeExtractTool;
  }
  
  
#pragma region "1、矢量数据语义融合处理工具"
  /********************************************************************************************************/
  // Create the action for tool
  mAction_JJBDX_NJBDX_semantic_fusion = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/JJBDX.svg"), tr("军用地形图数据语义融合工具"), this);
  mAction_JJBDX_NJBDX_semantic_fusion->setObjectName(QStringLiteral("mAction_JJBDX_NJBDX_semantic_fusion"));
  // Set the what's this text
  mAction_JJBDX_NJBDX_semantic_fusion->setWhatsThis(tr("军用地形图数据语义融合工具"));
  // Connect the action to the run
  connect(mAction_JJBDX_NJBDX_semantic_fusion, &QAction::triggered, this, &QgsVectorDataFusionToolBox::JJBDX_NJBDX_semantic_fusion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_JJBDX_NJBDX_semantic_fusion);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_JJBDX_NJBDX_semantic_fusion);
  mAction_JJBDX_NJBDX_semantic_fusion->setEnabled(true);
  /********************************************************************************************************/

  /********************************************************************************************************/
  // Create the action for tool
  mAction_OSM_NJBDX_semantic_fusion = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/OSM.svg"), tr("OSM数据语义融合工具"), this);
  mAction_OSM_NJBDX_semantic_fusion->setObjectName(QStringLiteral("mAction_OSM_NJBDX_semantic_fusion"));
  // Set the what's this text
  mAction_OSM_NJBDX_semantic_fusion->setWhatsThis(tr("OSM数据语义融合工具"));
  // Connect the action to the run
  connect(mAction_OSM_NJBDX_semantic_fusion, &QAction::triggered, this, &QgsVectorDataFusionToolBox::OSM_NJBDX_semantic_fusion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_OSM_NJBDX_semantic_fusion);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_OSM_NJBDX_semantic_fusion);
  mAction_OSM_NJBDX_semantic_fusion->setEnabled(true);
  /********************************************************************************************************/

  /********************************************************************************************************/
  // Create the action for tool
  mAction_DZHT_NJBDX_semantic_fusion = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/DZHT.svg"), tr("电子海图数据语义融合工具"), this);
  mAction_DZHT_NJBDX_semantic_fusion->setObjectName(QStringLiteral("mAction_DZHT_NJBDX_semantic_fusion"));
  // Set the what's this text
  mAction_DZHT_NJBDX_semantic_fusion->setWhatsThis(tr("电子海图数据语义融合工具"));
  // Connect the action to the run
  connect(mAction_DZHT_NJBDX_semantic_fusion, &QAction::triggered, this, &QgsVectorDataFusionToolBox::DZHT_NJBDX_semantic_fusion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_DZHT_NJBDX_semantic_fusion);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_DZHT_NJBDX_semantic_fusion);
  mAction_DZHT_NJBDX_semantic_fusion->setEnabled(true);
  /********************************************************************************************************/

  /********************************************************************************************************/
  // Create the action for tool
  mAction_JBHT_NJBDX_semantic_fusion = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/JBHT.svg"), tr("军用海图数据语义融合工具"), this);
  mAction_JBHT_NJBDX_semantic_fusion->setObjectName(QStringLiteral("mAction_JBHT_NJBDX_semantic_fusion"));
  // Set the what's this text
  mAction_JBHT_NJBDX_semantic_fusion->setWhatsThis(tr("军用海图数据语义融合工具"));
  // Connect the action to the run
  connect(mAction_JBHT_NJBDX_semantic_fusion, &QAction::triggered, this, &QgsVectorDataFusionToolBox::JBHT_NJBDX_semantic_fusion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_JBHT_NJBDX_semantic_fusion);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_JBHT_NJBDX_semantic_fusion);
  mAction_JBHT_NJBDX_semantic_fusion->setEnabled(true);
  /********************************************************************************************************/

  /********************************************************************************************************/
  // Create the action for tool
  mAction_SW_NJBDX_semantic_fusion = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/SW.svg"), tr("导航数据语义融合工具"), this);
  mAction_SW_NJBDX_semantic_fusion->setObjectName(QStringLiteral("mAction_SW_NJBDX_semantic_fusion"));
  // Set the what's this text
  mAction_SW_NJBDX_semantic_fusion->setWhatsThis(tr("导航数据语义融合工具"));
  // Connect the action to the run
  connect(mAction_SW_NJBDX_semantic_fusion, &QAction::triggered, this, &QgsVectorDataFusionToolBox::SW_NJBDX_semantic_fusion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_SW_NJBDX_semantic_fusion);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_SW_NJBDX_semantic_fusion);
  mAction_SW_NJBDX_semantic_fusion->setEnabled(true);
  /********************************************************************************************************/

  /********************************************************************************************************/
  // Create the action for tool
  mAction_JYDH_NJBDX_semantic_fusion = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/JYDH.svg"), tr("军用导航数据语义融合工具"), this);
  mAction_JYDH_NJBDX_semantic_fusion->setObjectName(QStringLiteral("mAction_JYDH_NJBDX_semantic_fusion"));
  // Set the what's this text
  mAction_JYDH_NJBDX_semantic_fusion->setWhatsThis(tr("军用导航数据语义融合工具"));
  // Connect the action to the run
  connect(mAction_JYDH_NJBDX_semantic_fusion, &QAction::triggered, this, &QgsVectorDataFusionToolBox::JYDH_NJBDX_semantic_fusion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_JYDH_NJBDX_semantic_fusion);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_JYDH_NJBDX_semantic_fusion);
  mAction_JYDH_NJBDX_semantic_fusion->setEnabled(true);
  /********************************************************************************************************/

  /********************************************************************************************************/
  // Create the action for tool
  mAction_QQCT_NJBDX_semantic_fusion = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/QQCT.svg"), tr("全球测图数据语义融合工具"), this);
  mAction_QQCT_NJBDX_semantic_fusion->setObjectName(QStringLiteral("mAction_QQCT_NJBDX_semantic_fusion"));
  // Set the what's this text
  mAction_QQCT_NJBDX_semantic_fusion->setWhatsThis(tr("全球测图数据语义融合工具"));
  // Connect the action to the run
  connect(mAction_QQCT_NJBDX_semantic_fusion, &QAction::triggered, this, &QgsVectorDataFusionToolBox::QQCT_NJBDX_semantic_fusion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_QQCT_NJBDX_semantic_fusion);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_QQCT_NJBDX_semantic_fusion);
  mAction_QQCT_NJBDX_semantic_fusion->setEnabled(true);
  /********************************************************************************************************/
#pragma endregion

#pragma region "2、矢量数据要素提取工具"
  // Create the action for tool
  mActionAttributeExtractTool = new QAction(QIcon(":/vector_data_fusion_toolbox/icons/vector_data_feature_extraction.svg"), tr("矢量数据要素选取工具"), this);
  mActionAttributeExtractTool->setObjectName(QStringLiteral("mActionAttributeExtractTool"));
  // Set the what's this text
  mActionAttributeExtractTool->setWhatsThis(tr("图层提取、属性字段提取、属性值提取"));
  // Connect the action to the run
  connect(mActionAttributeExtractTool, &QAction::triggered, this, &QgsVectorDataFusionToolBox::vector_data_feature_extraction);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mActionAttributeExtractTool);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mActionAttributeExtractTool);
  mActionAttributeExtractTool->setEnabled(true);

#pragma endregion

#pragma region "3、其他工具"

#pragma endregion

}

//  1、语义融合工具集合
void QgsVectorDataFusionToolBox::JJBDX_NJBDX_semantic_fusion()
{
  //  创建语义融合处理对象
  qgs_JJBDX_NJBDX_semantic_fusion* pDlg = new qgs_JJBDX_NJBDX_semantic_fusion(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //  判断创建的指针是否为空
  if (!pDlg)
  {
    return;
  }
  //  对话框被接受时，这样可以使得界面可以被显示出来
  if (pDlg->exec() == QDialog::Accepted)
  {
    // nothing
  }
  // 确保总是删除
  delete pDlg; 
}

void QgsVectorDataFusionToolBox::OSM_NJBDX_semantic_fusion()
{
  //  创建语义融合处理对象
  qgs_OSM_NJBDX_semantic_fusion* pDlg = new qgs_OSM_NJBDX_semantic_fusion(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //  判断创建的指针是否为空
  if (!pDlg)
  {
    return;
  }
  //  对话框被接受时，这样可以使得界面可以被显示出来
  if (pDlg->exec() == QDialog::Accepted)
  {
    // nothing
  }
  // 确保总是删除
  delete pDlg;
}

void QgsVectorDataFusionToolBox::DZHT_NJBDX_semantic_fusion()
{
  //  创建语义融合处理对象
  qgs_DZHT_NJBDX_semantic_fusion* pDlg = new qgs_DZHT_NJBDX_semantic_fusion(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //  判断创建的指针是否为空
  if (!pDlg)
  {
    return;
  }
  //  对话框被接受时，这样可以使得界面可以被显示出来
  if (pDlg->exec() == QDialog::Accepted)
  {
    // nothing
  }
  // 确保总是删除
  delete pDlg;
}

void QgsVectorDataFusionToolBox::JBHT_NJBDX_semantic_fusion()
{
  //  创建语义融合处理对象
  qgs_JBHT_NJBDX_semantic_fusion* pDlg = new qgs_JBHT_NJBDX_semantic_fusion(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //  判断创建的指针是否为空
  if (!pDlg)
  {
    return;
  }
  //  对话框被接受时，这样可以使得界面可以被显示出来
  if (pDlg->exec() == QDialog::Accepted)
  {
    // nothing
  }
  // 确保总是删除
  delete pDlg;
}

void QgsVectorDataFusionToolBox::SW_NJBDX_semantic_fusion()
{
  //  创建语义融合处理对象
  qgs_SW_NJBDX_semantic_fusion* pDlg = new qgs_SW_NJBDX_semantic_fusion(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //  判断创建的指针是否为空
  if (!pDlg)
  {
    return;
  }
  //  对话框被接受时，这样可以使得界面可以被显示出来
  if (pDlg->exec() == QDialog::Accepted)
  {
    // nothing
  }
  // 确保总是删除
  delete pDlg;
}

void QgsVectorDataFusionToolBox::JYDH_NJBDX_semantic_fusion()
{
  //  创建语义融合处理对象
  qgs_JYDH_NJBDX_semantic_fusion* pDlg = new qgs_JYDH_NJBDX_semantic_fusion(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //  判断创建的指针是否为空
  if (!pDlg)
  {
    return;
  }
  //  对话框被接受时，这样可以使得界面可以被显示出来
  if (pDlg->exec() == QDialog::Accepted)
  {
    // nothing
  }
  // 确保总是删除
  delete pDlg;
}

void QgsVectorDataFusionToolBox::QQCT_NJBDX_semantic_fusion()
{
  //  创建语义融合处理对象
  qgs_QQCT_NJBDX_semantic_fusion* pDlg = new qgs_QQCT_NJBDX_semantic_fusion(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //  判断创建的指针是否为空
  if (!pDlg)
  {
    return;
  }
  //  对话框被接受时，这样可以使得界面可以被显示出来
  if (pDlg->exec() == QDialog::Accepted)
  {
    // nothing
  }
  // 确保总是删除
  delete pDlg;
}

//  2、矢量数据要素提取工具
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

void QgsVectorDataFusionToolBox::unload()
{
  // remove the GUI

  //  1、语义融合处理工具
  mQGisIface->removePluginVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_JJBDX_NJBDX_semantic_fusion);
  mQGisIface->removeVectorToolBarIcon(mAction_JJBDX_NJBDX_semantic_fusion);

  mQGisIface->removePluginVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_OSM_NJBDX_semantic_fusion);
  mQGisIface->removeVectorToolBarIcon(mAction_OSM_NJBDX_semantic_fusion);

  mQGisIface->removePluginVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_DZHT_NJBDX_semantic_fusion);
  mQGisIface->removeVectorToolBarIcon(mAction_DZHT_NJBDX_semantic_fusion);

  mQGisIface->removePluginVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_JBHT_NJBDX_semantic_fusion);
  mQGisIface->removeVectorToolBarIcon(mAction_JBHT_NJBDX_semantic_fusion);

  mQGisIface->removePluginVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_SW_NJBDX_semantic_fusion);
  mQGisIface->removeVectorToolBarIcon(mAction_SW_NJBDX_semantic_fusion);

  mQGisIface->removePluginVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_JYDH_NJBDX_semantic_fusion);
  mQGisIface->removeVectorToolBarIcon(mAction_JYDH_NJBDX_semantic_fusion);

  mQGisIface->removePluginVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mAction_QQCT_NJBDX_semantic_fusion);
  mQGisIface->removeVectorToolBarIcon(mAction_QQCT_NJBDX_semantic_fusion);

  //  2、（矢量要素）数据提取工具
  mQGisIface->removePluginVectorMenu(tr("&菜单：矢量数据语义融合处理工具"), mActionAttributeExtractTool);
  mQGisIface->removeVectorToolBarIcon(mActionAttributeExtractTool);

  //  3、其他工具

  delete mAction_JJBDX_NJBDX_semantic_fusion;
  delete mAction_OSM_NJBDX_semantic_fusion;
  delete mAction_DZHT_NJBDX_semantic_fusion;
  delete mAction_JBHT_NJBDX_semantic_fusion;
  delete mAction_SW_NJBDX_semantic_fusion;
  delete mAction_JYDH_NJBDX_semantic_fusion;
  delete mAction_QQCT_NJBDX_semantic_fusion;
  delete mActionAttributeExtractTool;
}

void QgsVectorDataFusionToolBox::help()
{
  // TODO: help
}
#pragma endregion

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
