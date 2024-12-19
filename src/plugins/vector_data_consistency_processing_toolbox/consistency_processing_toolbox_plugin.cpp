#pragma region "包含头文件（减少重复）"
/*-------------------QGIS相关的头文件------------------*/
#include "qgisinterface.h"
#include "qgsmessagebar.h"
/*-------------------QGIS相关的头文件------------------*/

/*------------------------QT--------------------------*/
#include <QAction>
#include <QString>
/*------------------------QT--------------------------*/


/*-------------要素一致性处理工具相关头文件-------------*/
#include "consistency_processing_toolbox_plugin.h"
/*-------------要素一致性处理工具相关头文件-------------*/
#pragma endregion

#pragma region "插件（要素一致性处理工具箱）描述信息"
static const QString sName = QObject::tr("要素一致性处理工具箱");
static const QString sDescription = QObject::tr("一致性几何编辑处理工具（人机交互）、一致性匹配表单检查工具（人工交互）、一致性匹配工具（自动化）、一致性属连接处理工具（人工交互）");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 0.1");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
// 插件图标
static const QString sPluginIcon = QStringLiteral(":/vector_data_consistency_processing_toolbox/icons/consistency_processing_toolbox_plugin.svg");
#pragma endregion

#pragma region "构造函数、析构函数"
QgsConsistencyProcessingToolboxPlugin::QgsConsistencyProcessingToolboxPlugin(QgisInterface* qgisInterface)
  : QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
  , mQGisIface(qgisInterface)
{

}

QgsConsistencyProcessingToolboxPlugin::~QgsConsistencyProcessingToolboxPlugin()
{
  if(mAction_consistency_matching_tool)
  {
    delete mAction_consistency_matching_tool;
  }
  if(mAction_consistent_geometry_editing_and_processing_tool)
  {
    delete mAction_consistent_geometry_editing_and_processing_tool;
  }
  if(mAction_consistency_matching_form_checking_tool)
  {
    delete mAction_consistency_matching_form_checking_tool;
  }
  if(mAction_consistency_attribute_connection_processing_tool)
  {
    delete mAction_consistency_attribute_connection_processing_tool;
  }
}
#pragma endregion

#pragma region "插件公开槽函数API接口"
void QgsConsistencyProcessingToolboxPlugin::initGui()
{
#pragma region "确保之前的actions对象指针都被释放了"
  //  确保之前的actions对象指针都被释放了
  if(mAction_consistency_matching_tool)
  {
    delete mAction_consistency_matching_tool;
  }
  if(mAction_consistent_geometry_editing_and_processing_tool)
  {
    delete mAction_consistent_geometry_editing_and_processing_tool;
  }
  if(mAction_consistency_matching_form_checking_tool)
  {
    delete mAction_consistency_matching_form_checking_tool;
  }
  if(mAction_consistency_attribute_connection_processing_tool)
  {
    delete mAction_consistency_attribute_connection_processing_tool;
  }
#pragma endregion

#pragma region "1、一致性匹配工具（自动化）"
  // Create the action for tool
  mAction_consistency_matching_tool = new QAction(
    QIcon(":/vector_data_consistency_processing_toolbox/icons/consistency_matching_tool.svg"), 
    tr("一致性匹配工具"), this);
  mAction_consistency_matching_tool->setObjectName(QStringLiteral("mAction_consistency_matching_tool"));
  // Set the what's this text
  mAction_consistency_matching_tool->setWhatsThis(tr("实现shapefile、Geopackage两种数据类型的本底数据和匹配数据之间的空间匹配和属性匹配"));
  // Connect the action to the run
  connect(
    mAction_consistency_matching_tool, &QAction::triggered, 
    this, &QgsConsistencyProcessingToolboxPlugin::consistency_matching_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_consistency_matching_tool);
  mQGisIface->addPluginToVectorMenu(tr("&一致性处理工具箱"), mAction_consistency_matching_tool);
  mAction_consistency_matching_tool->setEnabled(true);

#pragma endregion

#pragma region "2、一致性几何编辑处理工具（人机交互）"
  // Create the action for tool
  mAction_consistent_geometry_editing_and_processing_tool = new QAction(
    QIcon(":/vector_data_consistency_processing_toolbox/icons/consistent_geometry_editing_and_processing_tool.svg"), 
    tr("一致性几何编辑处理工具"), this);
  mAction_consistent_geometry_editing_and_processing_tool->setObjectName(QStringLiteral("mAction_consistent_geometry_editing_and_processing_tool"));
  // Set the what's this text
  mAction_consistent_geometry_editing_and_processing_tool->setWhatsThis(
    tr("1、支持阈值范围内点数据按照指定属性进行融合剔除（打标记）处理（自动化批量处理），通过配置文件设置参数（融合的图层、阈值大小、融合字段）\
    在输入数据集上进行赋值标记处理，赋值取值为“匹配”。\
    2、支持线要素的一键打断处理（通过直线分割选中要素，在分割线和所选对象的交点处打断）\
    3、支持框选要素一键延伸处理（通常为直线延伸）\
    4、支持框选要素一键修剪线处理 \
    5、支持面要素一键分割处理 \
    6、支持一键提取中心点 \
    7、支持一键提取中心线 \
    8、支持一键合并处理，可跨图层（图层合并）或不跨图层（融合）"));
  // Connect the action to the run
  connect(
    mAction_consistent_geometry_editing_and_processing_tool, &QAction::triggered, 
    this, &QgsConsistencyProcessingToolboxPlugin::consistent_geometry_editing_and_processing_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_consistent_geometry_editing_and_processing_tool);
  mQGisIface->addPluginToVectorMenu(tr("&一致性处理工具箱"), mAction_consistent_geometry_editing_and_processing_tool);
  mAction_consistent_geometry_editing_and_processing_tool->setEnabled(true);

#pragma endregion

#pragma region "3、一致性匹配表单检查工具（人工交互）"
  // Create the action for tool
  mAction_consistency_matching_form_checking_tool = new QAction(
    QIcon(":/vector_data_consistency_processing_toolbox/icons/consistency_matching_form_checking_tool.svg"), 
    tr("一致性匹配表单检查工具"), this);
  mAction_consistency_matching_form_checking_tool->setObjectName(QStringLiteral("mAction_consistency_matching_form_checking_tool"));
  // Set the what's this text
  mAction_consistency_matching_form_checking_tool->setWhatsThis(
    tr("支持对匹配表单与源数据的联动选中查看处理和可视定位，如：\
    支持匹配表单的编辑更新（表内容修改、表单行的删除）；"));
  // Connect the action to the run
  connect(
    mAction_consistency_matching_form_checking_tool, &QAction::triggered, 
    this, &QgsConsistencyProcessingToolboxPlugin::consistency_matching_form_checking_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_consistency_matching_form_checking_tool);
  mQGisIface->addPluginToVectorMenu(tr("&一致性处理工具箱"), mAction_consistency_matching_form_checking_tool);
  mAction_consistency_matching_form_checking_tool->setEnabled(true);
#pragma endregion

#pragma region "4、一致性属连接处理工具（人工交互）"
  // Create the action for tool
  mAction_consistency_attribute_connection_processing_tool = new QAction(
    QIcon(":/vector_data_consistency_processing_toolbox/icons/consistency_attribute_connection_processing_tool.svg"), 
    tr("一致性属连接处理工具"), this);
  mAction_consistency_attribute_connection_processing_tool->setObjectName(QStringLiteral("mAction_consistency_attribute_connection_processing_tool"));
  // Set the what's this text
  mAction_consistency_attribute_connection_processing_tool->setWhatsThis(
    tr(
      "1、支持依据匹配处理产生的匹配关联表进行两套数据属性挂接处理（即将一个图层的属性按照匹配表挂接到另外一个图层中），支持一对一或一对多挂接\
       2、支持通过视窗界面人工框选两个图层的要素实现一键属性挂接处理\
       3、支持一键挂接之后的标记处理，即在对目标要素和连接要素的“标记”字段赋值为“匹配存在”"));
  // Connect the action to the run
  connect(
    mAction_consistency_attribute_connection_processing_tool, &QAction::triggered, 
    this, &QgsConsistencyProcessingToolboxPlugin::consistency_attribute_connection_processing_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_consistency_attribute_connection_processing_tool);
  mQGisIface->addPluginToVectorMenu(tr("&一致性处理工具箱"), mAction_consistency_attribute_connection_processing_tool);
  mAction_consistency_attribute_connection_processing_tool->setEnabled(true);
#pragma endregion

}

void QgsConsistencyProcessingToolboxPlugin::unload()
{
  // remove the GUI

  //  1、一致性匹配工具（自动化）
  mQGisIface->removePluginVectorMenu(tr("&一致性处理工具箱"), mAction_consistency_matching_tool);
  mQGisIface->removeVectorToolBarIcon(mAction_consistency_matching_tool);

  //  2、一致性几何编辑处理工具（人机交互）
  mQGisIface->removePluginVectorMenu(tr("&一致性处理工具箱"), mAction_consistent_geometry_editing_and_processing_tool);
  mQGisIface->removeVectorToolBarIcon(mAction_consistent_geometry_editing_and_processing_tool);

  //  3、一致性匹配表单检查工具（人工交互）
  mQGisIface->removePluginVectorMenu(tr("&一致性处理工具箱"), mAction_consistency_matching_form_checking_tool);
  mQGisIface->removeVectorToolBarIcon(mAction_consistency_matching_form_checking_tool);

  //  4、一致性属连接处理工具（人工交互）
  mQGisIface->removePluginVectorMenu(tr("&一致性处理工具箱"), mAction_consistency_attribute_connection_processing_tool);
  mQGisIface->removeVectorToolBarIcon(mAction_consistency_attribute_connection_processing_tool);


  delete mAction_consistency_matching_tool;
  mAction_consistency_matching_tool = nullptr;
  delete mAction_consistent_geometry_editing_and_processing_tool;
  mAction_consistent_geometry_editing_and_processing_tool = nullptr;
  delete mAction_consistency_matching_form_checking_tool;
  mAction_consistency_matching_form_checking_tool = nullptr;
  delete mAction_consistency_attribute_connection_processing_tool;
  mAction_consistency_attribute_connection_processing_tool = nullptr;

}

void QgsConsistencyProcessingToolboxPlugin::help()
{
  // TODO: help
}

  //  1、一致性匹配工具（自动化）
void QgsConsistencyProcessingToolboxPlugin::consistency_matching_tool()
{
  qgs_consistency_matching* pDlg = new qgs_consistency_matching(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }

  // 用户点击了取消：删除对话框对象，释放内存
  //  TODO：之前所做的一些插件并没有对已分配内存释放，后面需要完善工具
  delete pDlg;
}
  //  2、一致性几何编辑处理工具（人机交互）（注：目前还没有实现这部分的代码）
void QgsConsistencyProcessingToolboxPlugin::consistent_geometry_editing_and_processing_tool()
{
  //qgs_consistent_geometry_editing_and_processing* pDlg = new qgs_consistent_geometry_editing_and_processing(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //if (!pDlg)
  //{
  //  return;
  //}
  //else if(pDlg->exec() == QDialog::Accepted)
  //{
  //  delete pDlg;
  //}
}
  //  3、一致性匹配表单检查工具（人工交互）（注：目前还没有实现这部分的代码）
void QgsConsistencyProcessingToolboxPlugin::consistency_matching_form_checking_tool()
{
  //qgs_consistency_matching_form_checking* pDlg = new qgs_consistency_matching_form_checking(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //if (!pDlg)
  //{
  //  return;
  //}
  //else if(pDlg->exec() == QDialog::Accepted)
  //{
  //  delete pDlg;
  //}
}
  //  4、一致性属连接处理工具（人工交互）（注：目前还没有实现这部分的代码）
void QgsConsistencyProcessingToolboxPlugin::consistency_attribute_connection_processing_tool()
{
  //qgs_consistency_attribute_connection_processing* pDlg = new qgs_consistency_attribute_connection_processing(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  //if (!pDlg)
  //{
  //  return;
  //}
  //else if(pDlg->exec() == QDialog::Accepted)
  //{
  //  delete pDlg;
  //}
}

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
  return new QgsConsistencyProcessingToolboxPlugin(qgisInterfacePointer);
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
