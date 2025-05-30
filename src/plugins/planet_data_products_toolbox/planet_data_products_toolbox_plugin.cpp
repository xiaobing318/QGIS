#pragma region "包含头文件（减少重复）"
/*-------------------QGIS相关的头文件------------------*/
#include "qgisinterface.h"
#include "qgsmessagebar.h"
/*-------------------QGIS相关的头文件------------------*/

/*------------------------QT--------------------------*/
#include <QAction>
#include <QString>
/*------------------------QT--------------------------*/


/*-------------星球数据产品工具集插件头文件-------------*/
#include "planet_data_products_toolbox_plugin.h"
#include "cse_analysisreadydataprocess_tool.h"
/*-------------星球数据产品工具集插件头文件-------------*/
#pragma endregion

#pragma region "插件（星球数据产品工具箱）描述信息"
static const QString sName = QObject::tr("星球数据产品工具箱");
static const QString sDescription = QObject::tr("矢量数据翻译工具功能是支持对输入的shp数据的指定字段内容进行翻译，并将翻译成果赋值给指定字段。其中，支持翻译字段的语言自动识别。");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 0.1");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
// 插件图标
static const QString sPluginIcon = QStringLiteral(":/planet_data_products_toolbox/icons/plugin_icon.svg");
#pragma endregion

#pragma region "构造函数、析构函数"
QgsPlanetDataProductsToolboxPlugin::QgsPlanetDataProductsToolboxPlugin(QgisInterface* qgisInterface)
  : QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
  , mQGisIface(qgisInterface)
{

}

QgsPlanetDataProductsToolboxPlugin::~QgsPlanetDataProductsToolboxPlugin()
{
  //  1、矢量数据翻译工具
  if(mAction_vector_data_translation_tool)
  {
    delete mAction_vector_data_translation_tool;
  }
  //  2、分析就绪数据处理工具
  if(mAction_analysis_ready_data_process_tool)
  {
    delete mAction_analysis_ready_data_process_tool;
  }
  //  3、点要素分级处理工具
  if(mAction_point_feature_hierarchical_processing_tool)
  {
    delete mAction_point_feature_hierarchical_processing_tool;
  }
  //  4、其他工具
}
#pragma endregion

#pragma region "插件公开槽函数API接口"
void QgsPlanetDataProductsToolboxPlugin::initGui()
{
#pragma region "确保之前的actions对象指针都被释放了"
  //  确保之前的actions对象指针都被释放了

  //  1、矢量数据翻译工具
  if(mAction_vector_data_translation_tool)
  {
    delete mAction_vector_data_translation_tool;
  }

  //  2、分析就绪数据处理工具（临时：给到空天院）
  if (mAction_analysis_ready_data_process_tool)
  {
    delete mAction_analysis_ready_data_process_tool;
  }

  //  3、点要素分级处理工具
  if (mAction_point_feature_hierarchical_processing_tool)
  {
    delete mAction_point_feature_hierarchical_processing_tool;
  }

  //  4、其他工具


#pragma endregion

#pragma region "1、矢量数据翻译工具"
  // Create the action for tool
  mAction_vector_data_translation_tool = new QAction(
    QIcon(":/planet_data_products_toolbox/icons/translator_colors_icon.svg"), 
    tr("矢量数据翻译工具"), this);
  mAction_vector_data_translation_tool->setObjectName(QStringLiteral("mAction_vector_data_translation_tool"));
  // Set the what's this text
  mAction_vector_data_translation_tool->setWhatsThis(tr("矢量数据翻译工具"));
  // Connect the action to the run
  connect(
    mAction_vector_data_translation_tool, &QAction::triggered, 
    this, &QgsPlanetDataProductsToolboxPlugin::vector_data_translation_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_vector_data_translation_tool);
  mQGisIface->addPluginToVectorMenu(tr("&星球数据产品工具箱"), mAction_vector_data_translation_tool);
  mAction_vector_data_translation_tool->setEnabled(true);

#pragma endregion

#pragma region "2、分析就绪数据处理工具（临时：给到空天院）"
  // Create the action for tool
  mAction_analysis_ready_data_process_tool = new QAction(
    QIcon(":/planet_data_products_toolbox/icons/analysis_ready_data_process_tool_icon.svg"),
    tr("分析就绪数据处理工具"), this);
  mAction_analysis_ready_data_process_tool->setObjectName(QStringLiteral("mAction_analysis_ready_data_process_tool"));
  // Set the what's this text
  mAction_analysis_ready_data_process_tool->setWhatsThis(tr("分析就绪数据处理工具"));
  // Connect the action to the run
  connect(
    mAction_analysis_ready_data_process_tool, &QAction::triggered,
    this, &QgsPlanetDataProductsToolboxPlugin::analysis_ready_data_process_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_analysis_ready_data_process_tool);
  mQGisIface->addPluginToVectorMenu(tr("&星球数据产品工具箱"), mAction_analysis_ready_data_process_tool);
  mAction_analysis_ready_data_process_tool->setEnabled(true);
#pragma endregion


#pragma region "3、点要素分级处理工具"
  // Create the action for tool
  mAction_point_feature_hierarchical_processing_tool = new QAction(
    QIcon(":/planet_data_products_toolbox/icons/point_feature_hierarchical_processing_tool_icon.svg"),
    tr("点要素分级处理工具"), this);
  mAction_point_feature_hierarchical_processing_tool->setObjectName(QStringLiteral("mAction_point_feature_hierarchical_processing_tool"));
  // Set the what's this text
  mAction_point_feature_hierarchical_processing_tool->setWhatsThis(tr("点要素分级处理工具"));
  // Connect the action to the run
  connect(
    mAction_point_feature_hierarchical_processing_tool, &QAction::triggered,
    this, &QgsPlanetDataProductsToolboxPlugin::point_feature_hierarchical_processing_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_point_feature_hierarchical_processing_tool);
  mQGisIface->addPluginToVectorMenu(tr("&星球数据产品工具箱"), mAction_point_feature_hierarchical_processing_tool);
  mAction_point_feature_hierarchical_processing_tool->setEnabled(true);
#pragma endregion


#pragma region "4、其他工具"

#pragma endregion

}

void QgsPlanetDataProductsToolboxPlugin::unload()
{
  // remove the GUI

  //  1、矢量数据翻译工具
  mQGisIface->removePluginVectorMenu(tr("&星球数据产品工具箱"), mAction_vector_data_translation_tool);
  mQGisIface->removeVectorToolBarIcon(mAction_vector_data_translation_tool);

  //  2、分析就绪数据处理工具（临时：给到空天院）
  mQGisIface->removePluginVectorMenu(tr("&星球数据产品工具箱"), mAction_analysis_ready_data_process_tool);
  mQGisIface->removeVectorToolBarIcon(mAction_analysis_ready_data_process_tool);

  //  3、点要素分级处理工具
  mQGisIface->removePluginVectorMenu(tr("&星球数据产品工具箱"), mAction_point_feature_hierarchical_processing_tool);
  mQGisIface->removeVectorToolBarIcon(mAction_point_feature_hierarchical_processing_tool);

  //  4、其他工具


  //  释放在内存中分配的空间
  delete mAction_vector_data_translation_tool;
  mAction_vector_data_translation_tool = nullptr;

  delete mAction_analysis_ready_data_process_tool;
  mAction_analysis_ready_data_process_tool = nullptr;

  delete mAction_point_feature_hierarchical_processing_tool;
  mAction_point_feature_hierarchical_processing_tool = nullptr;
}

void QgsPlanetDataProductsToolboxPlugin::help()
{
  // TODO: help
}

  //  1、矢量数据翻译工具
void QgsPlanetDataProductsToolboxPlugin::vector_data_translation_tool()
{
  vector_data_translation* pDlg = new vector_data_translation(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }

  // 用户点击了取消：删除对话框对象，释放内存
  delete pDlg;
}

//  2、分析就绪数据处理工具（临时：给到空天院）
void QgsPlanetDataProductsToolboxPlugin::analysis_ready_data_process_tool()
{
  CSE_AnalysisReadyDataProcess* pDlg = new CSE_AnalysisReadyDataProcess(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }

  // 用户点击了取消：删除对话框对象，释放内存
  delete pDlg;
}

//  3、点要素分级处理工具
void QgsPlanetDataProductsToolboxPlugin::point_feature_hierarchical_processing_tool()
{
  point_feature_hierarchical_processing* pDlg = new point_feature_hierarchical_processing(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }

  // 用户点击了取消：删除对话框对象，释放内存
  delete pDlg;
}


//  4、其他工具（注：目前还没有实现这部分的代码）

#pragma endregion

#pragma region "QGIS插件实现必须的函数（为了使得插件可以在QGIS主体上正常运行）"
/**
 * Required extern functions needed  for every plugin
 * These functions can be called prior to creating an instance
 * of the plugin class
 */
 // Class factory to return a new instance of the plugin class
QGISEXTERN QgisPlugin* classFactory(QgisInterface* qgisInterfacePointer)
{
  return new QgsPlanetDataProductsToolboxPlugin(qgisInterfacePointer);
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
#pragma endregion
