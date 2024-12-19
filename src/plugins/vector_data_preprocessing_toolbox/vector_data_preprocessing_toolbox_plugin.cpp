/*------------QGIS include-----------------*/
#include "qgisinterface.h"
#include "qgsguiutils.h"
#include "qgsproject.h"
#include "qgsmessagebar.h"
#include "qgsmapcanvas.h"
#include "qgsapplication.h"
#include "qgsmessagelog.h"


/*-------------Qt--------------------*/
#include <QAction>
#include <QMessageBox>
#include <QtCore/QProcess>

/*-----------------自定义工具头文件----------------*/
//  矢量数据预处理工具箱头文件
#include "vector_data_preprocessing_toolbox_plugin.h"
//  坐标转化工具头文件
#include "se_geocoordsys_trans.h"
//  源数据检查工具头文件
#include "source_data_inspection_tool.h"
#include "source_data_inspection_tool_data_structure.h"



static const QString sName = QObject::tr("矢量数据预处理工具箱");
static const QString sDescription = QObject::tr("提供地理坐标系转换、投影坐标系转换功能，支持BJS54、XAS80、WGS84及境外局部坐标系向CGCS2000坐标系转换，支持高斯、UTM、墨卡托投影");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
// 插件图标
static const QString sPluginIcon = QStringLiteral(":/vector_data_preprocessing_toolbox/images/geocoordsys_trans.png");

QgsCoordinateTransformationToolBox::QgsCoordinateTransformationToolBox(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

QgsCoordinateTransformationToolBox::~QgsCoordinateTransformationToolBox()
{
}



void QgsCoordinateTransformationToolBox::initGui()
{
	//  确保之前的actions对象指针都被释放了
	delete mAction_GeoCoordSysTrans;
  delete mAction_source_data_inspection_tool;

	// 程序运行目录
	QString curExePath = QCoreApplication::applicationDirPath();

#pragma region "坐标转化工具"
  // Create the action for tool
  mAction_GeoCoordSysTrans = new QAction(QIcon(":/vector_data_preprocessing_toolbox/images/geocoordsys_trans.png"), tr("矢量数据坐标转化工具"), this);
  mAction_GeoCoordSysTrans->setObjectName(QStringLiteral("mAction_GeoCoordSysTrans"));
  // Set the what's this text
  mAction_GeoCoordSysTrans->setWhatsThis(tr("地理坐标系转换工具"));
  // Connect the action to the run
  connect(mAction_GeoCoordSysTrans, &QAction::triggered, this, &QgsCoordinateTransformationToolBox::GeoCoordSysTrans);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_GeoCoordSysTrans);
  mQGisIface->addPluginToVectorMenu(tr("&矢量数据预处理工具箱"), mAction_GeoCoordSysTrans);
  mAction_GeoCoordSysTrans->setEnabled(true);

#pragma endregion


#pragma region "矢量源数据检查工具"
  // Create the action for tool
  mAction_source_data_inspection_tool = new QAction(QIcon(":/vector_data_preprocessing_toolbox/images/geocoordsys_trans.png"), tr("矢量源数据检查工具"), this);
  mAction_source_data_inspection_tool->setObjectName(QStringLiteral("mAction_source_data_inspection_tool"));
  // Set the what's this text
  mAction_source_data_inspection_tool->setWhatsThis(tr("矢量源数据检查工具"));
  // Connect the action to the run
  connect(mAction_source_data_inspection_tool, &QAction::triggered, this, &QgsCoordinateTransformationToolBox::source_data_inspection_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_source_data_inspection_tool);
  mQGisIface->addPluginToVectorMenu(tr("&矢量数据预处理工具箱"), mAction_source_data_inspection_tool);
  mAction_source_data_inspection_tool->setEnabled(true);

#pragma endregion


#pragma region "其他工具"

#pragma endregion

	updateActions();
}


void QgsCoordinateTransformationToolBox::GeoCoordSysTrans()
{
	CSE_GeoCoordSysTransDialog* pDlg = new CSE_GeoCoordSysTransDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }
  else
  {
    //  杨小兵-2024-06-04：用户点击了取消：删除对话框对象，释放内存
    delete pDlg;
  }

}

void QgsCoordinateTransformationToolBox::ProjCoordSysTrans()
{
}

void QgsCoordinateTransformationToolBox::source_data_inspection_tool()
{
  qgs_source_data_inspection_tool* pDlg = new qgs_source_data_inspection_tool(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
  if (!pDlg)
  {
    return;
  }
  //  杨小兵-2024-06-04：用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }

  // 用户点击了取消：删除对话框对象，释放内存
  delete pDlg;

}

void QgsCoordinateTransformationToolBox::unload()
{
	// remove the GUI

	//  1、坐标转化处理工具
	mQGisIface->removePluginVectorMenu(tr("&矢量数据语义融合处理工具箱"), mAction_GeoCoordSysTrans);
	mQGisIface->removeVectorToolBarIcon(mAction_GeoCoordSysTrans);

  //  2、矢量源数据检查工具
  mQGisIface->removePluginVectorMenu(tr("&矢量数据语义融合处理工具箱"), mAction_source_data_inspection_tool);
  mQGisIface->removeVectorToolBarIcon(mAction_source_data_inspection_tool);

	//  3、其他工具


	delete mAction_GeoCoordSysTrans;
  delete mAction_source_data_inspection_tool;

	
}


void QgsCoordinateTransformationToolBox::updateActions()
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
	return new QgsCoordinateTransformationToolBox(qgisInterfacePointer);
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


