#include "vector_data_tile_customization_toolbox_plugin.h"

/*------------QGIS include-----------------*/
#include "qgisinterface.h"
#include "qgsguiutils.h"
#include "qgsproject.h"
#include "qgsmessagebar.h"
#include "qgsmapcanvas.h"
#include "qgsapplication.h"

/*-------------Qt--------------------*/
#include <QAction>
#include <QMessageBox>
#include <QtCore/QProcess>
#include <QDesktopServices>
#include <QUrl>

#include "URL_jump.h"

// 消息日志
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"

static const QString sName = QObject::tr("矢量数据瓦片定制工具箱");
static const QString sDescription = QObject::tr("提供网页跳转等定制化工具");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
// 插件图标
static const QString sPluginIcon = QStringLiteral(":/vector_data_tile_customization_toolbox/images/url_jump.png");

vector_data_tile_customization_toolbox::vector_data_tile_customization_toolbox(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

vector_data_tile_customization_toolbox::~vector_data_tile_customization_toolbox()
{
}



void vector_data_tile_customization_toolbox::initGui()
{
	//  确保之前的actions对象指针都被释放了
	delete mAction_url_jum;

#pragma region "网页跳转工具"
  // Create the action for tool
  mAction_url_jum = new QAction(QIcon(":/vector_data_tile_customization_toolbox/images/vector_data_tile_customization_toolbox.png"), tr("网页跳转工具"), this);
  mAction_url_jum->setObjectName(QStringLiteral("mAction_url_jum"));
  // Set the what's this text
  mAction_url_jum->setWhatsThis(tr("网页跳转工具"));
  // Connect the action to the run
  connect(mAction_url_jum, &QAction::triggered, this, &vector_data_tile_customization_toolbox::URL_jump);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_url_jum);
  mQGisIface->addPluginToVectorMenu(tr("&矢量数据瓦片定制工具箱"), mAction_url_jum);
  mAction_url_jum->setEnabled(true);

#pragma endregion


#pragma region "其他工具"

#pragma endregion

  updateActions();
}


void vector_data_tile_customization_toolbox::URL_jump()
{
	//url_jump_dialog* pDlg = new url_jump_dialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	//pDlg->setModal(false);
	//pDlg->show();
  QDesktopServices::openUrl(QUrl(QString("http://202.107.245.41:1203/#/sliceMana")));
}


void vector_data_tile_customization_toolbox::unload()
{
	// remove the GUI

	//  1、网页跳转工具
	mQGisIface->removePluginVectorMenu(tr("&矢量数据瓦片定制工具箱"), mAction_url_jum);
	mQGisIface->removeVectorToolBarIcon(mAction_url_jum);

	//  2、其他工具


	delete mAction_url_jum;

	
}


void vector_data_tile_customization_toolbox::updateActions()
{
	// 更新菜单状态
}

/**
 * Required extern functions needed  for every plugin
 * These functions can be called prior to creating an instance
 * of the plugin class
 */
 // Class factory to return a new instance of the plugin class
QGISEXTERN QgisPlugin* classFactory(QgisInterface* qgisInterfacePointer)
{
	return new vector_data_tile_customization_toolbox(qgisInterfacePointer);
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


