#pragma region "包含头文件（减少重复）"
/*---------------自定义-----------------*/
#include "plugin_vector_odata2shapefile.h"
#include "ui_class/tool_JBDX2Shapefile_class.h"
#include "ui_class/tool_DZB2Shapefile_class.h"
#include "ui_class/tool_DZB2ShapefileWithSpecification_class.h"
/*---------------自定义-----------------*/


/*------------QGIS-----------------*/
#include "qgisinterface.h"
#include "qgsguiutils.h"
#include "qgsproject.h"
#include "qgsmessagebar.h"
#include "qgsmapcanvas.h"
#include "qgsapplication.h"
/*------------QGIS-----------------*/


/*-------------Qt--------------------*/
#include <QAction>
#include <QMessageBox>
#include <QtCore/QProcess>
/*-------------Qt--------------------*/


/*------------------消息日志--------------------*/
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"
/*------------------消息日志--------------------*/

#pragma endregion

#pragma region "定义当前插件使用的一些常量"
static const QString sName = QObject::tr("ODATA格式转化工具箱");
static const QString sDescription = QObject::tr(R"(提供ODATA格式转化功能，包括：JBDX到Shapefile转化工具和DZB到Shapefile转化工具；)");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 3.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
//  插件图标
static const QString sPluginIcon = QStringLiteral(":/plugin_vector_odata2shapefile_toolbox/icons/plugin_icon.svg");
#pragma endregion

#pragma region "矢量数据ODATA转化为Shapefile插件类相关函数"
QgsPluginVectorOdata2Shapefile::QgsPluginVectorOdata2Shapefile(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

QgsPluginVectorOdata2Shapefile::~QgsPluginVectorOdata2Shapefile()
{
}

void QgsPluginVectorOdata2Shapefile::initGui()
{
  if (mActionJBDX2Shapefile)
  {
    delete mActionJBDX2Shapefile;
  }
  if (mActionDZB2Shapefile)
  {
    delete mActionDZB2Shapefile;
  }
  if (mActionDZB2ShapefileWithSpecification)
  {
    delete mActionDZB2ShapefileWithSpecification;
  }

#pragma region "1、工具：JBDX2Shapefile"
	/*****************************************************/
	/*    工具：JBDX2Shapefile   	*/
	/*****************************************************/
  // Create the action for tool
  mActionJBDX2Shapefile = new QAction(QIcon(":/plugin_vector_odata2shapefile_toolbox/icons/JBDX2Shapefile_icon.svg"), tr("将 JBDX 转换为 Shapefile "), this);
  mActionJBDX2Shapefile->setObjectName(QStringLiteral("mActionJBDX2Shapefile"));
  // Set the what's this text
  mActionJBDX2Shapefile->setWhatsThis(tr("将 JBDX 转换为 Shapefile"));
  // Connect the action to the run
  connect(mActionJBDX2Shapefile, &QAction::triggered, this, &QgsPluginVectorOdata2Shapefile::JBDX2Shapefile);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mActionJBDX2Shapefile);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：ODATA格式转化工具箱"), mActionJBDX2Shapefile);
  mActionJBDX2Shapefile->setEnabled(true);
#pragma endregion

#pragma region "2、工具：DZB2Shapefile"
	/*****************************************************/
	/*    工具：DZB2Shapefile   	*/
	/*****************************************************/
  // Create the action for tool
  mActionDZB2Shapefile = new QAction(QIcon(":/plugin_vector_odata2shapefile_toolbox/icons/DZB2Shapefile_icon.svg"), tr("将 DZB 转换为 Shapefile "), this);
  mActionDZB2Shapefile->setObjectName(QStringLiteral("mActionDZB2Shapefile"));
  // Set the what's this text
  mActionDZB2Shapefile->setWhatsThis(tr("将 DZB 转换为 Shapefile"));
  // Connect the action to the run
  connect(mActionDZB2Shapefile, &QAction::triggered, this, &QgsPluginVectorOdata2Shapefile::DZB2Shapefile);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mActionDZB2Shapefile);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：ODATA格式转化工具箱"), mActionDZB2Shapefile);
  mActionDZB2Shapefile->setEnabled(true);
#pragma endregion


#pragma region "3、工具：DZB2ShapefileWithSpecification"
	/*****************************************************/
	/*    工具：DZB2ShapefileWithSpecification   	*/
	/*****************************************************/
  // Create the action for tool
  mActionDZB2ShapefileWithSpecification = new QAction(QIcon(":/plugin_vector_odata2shapefile_toolbox/icons/DZB2ShapefileWithSpecification_icon.svg"), tr("将 DZB（符合规范版本）转换为 Shapefile "), this);
  mActionDZB2ShapefileWithSpecification->setObjectName(QStringLiteral("mActionDZB2ShapefileWithSpecification"));
  // Set the what's this text
  mActionDZB2ShapefileWithSpecification->setWhatsThis(tr("将 DZB（符合规范版本）转换为 Shapefile"));
  // Connect the action to the run
  connect(mActionDZB2ShapefileWithSpecification, &QAction::triggered, this, &QgsPluginVectorOdata2Shapefile::DZB2ShapefileWithSpecification);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mActionDZB2ShapefileWithSpecification);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：ODATA格式转化工具箱"), mActionDZB2ShapefileWithSpecification);
  mActionDZB2ShapefileWithSpecification->setEnabled(true);
#pragma endregion

	updateActions();
}

void QgsPluginVectorOdata2Shapefile::JBDX2Shapefile()
{
  ToolJBDX2ShapefileDialog dlg(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
  dlg.setModal(true); // 确保对话框是模态的

  if (dlg.exec() == QDialog::Accepted)
  {
    // 处理用户确认的逻辑
  }
  // 栈上对象会在函数结束时自动销毁
}

void QgsPluginVectorOdata2Shapefile::DZB2Shapefile()
{
  ToolDZB2ShapefileDialog dlg(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
  dlg.setModal(true); // 确保对话框是模态的

  if (dlg.exec() == QDialog::Accepted)
  {
    // 处理用户确认的逻辑
  }
  // 栈上对象会在函数结束时自动销毁
}

void QgsPluginVectorOdata2Shapefile::DZB2ShapefileWithSpecification()
{
  ToolDZB2ShapefileWithSpecificationDialog dlg(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
  dlg.setModal(true); // 确保对话框是模态的

  if (dlg.exec() == QDialog::Accepted)
  {
    // 处理用户确认的逻辑
  }
  // 栈上对象会在函数结束时自动销毁
}


void QgsPluginVectorOdata2Shapefile::unload()
{
	// JBDX2Shapefile:去掉ui界面
	mQGisIface->removePluginVectorMenu(tr("&菜单：ODATA格式转化工具箱"), mActionJBDX2Shapefile);
	mQGisIface->removeVectorToolBarIcon(mActionJBDX2Shapefile);
	delete mActionJBDX2Shapefile;

	// DZB2Shapefile:去掉ui界面
	mQGisIface->removePluginVectorMenu(tr("&菜单：ODATA格式转化工具箱"), mActionDZB2Shapefile);
	mQGisIface->removeVectorToolBarIcon(mActionDZB2Shapefile);
	delete mActionDZB2Shapefile;

	// DZB2ShapefileWithSpecification:去掉ui界面
	mQGisIface->removePluginVectorMenu(tr("&菜单：ODATA格式转化工具箱"), mActionDZB2ShapefileWithSpecification);
	mQGisIface->removeVectorToolBarIcon(mActionDZB2ShapefileWithSpecification);
	delete mActionDZB2ShapefileWithSpecification;
}

void QgsPluginVectorOdata2Shapefile::updateActions()
{
	// 更新菜单状态
}
#pragma endregion

#pragma region "QGIS插件类中必须存在的一些函数接口"
/**
 * Required extern functions needed  for every plugin
 * These functions can be called prior to creating an instance
 * of the plugin class
 */
 // Class factory to return a new instance of the plugin class
QGISEXTERN QgisPlugin* classFactory(QgisInterface* qgisInterfacePointer)
{
	return new QgsPluginVectorOdata2Shapefile(qgisInterfacePointer);
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

