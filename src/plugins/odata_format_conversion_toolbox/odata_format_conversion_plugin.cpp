#include "odata_format_conversion_plugin.h"
#include "ui_class/se_pbf2gpkg.h"
#include "ui_class/se_text_files_gbk2utf8.h"
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

#include "ui_class/se_odata2shp.h"
#include "ui_class/se_annotation_pointer_reverse_extraction.h"


// 消息日志
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"

static const QString sName = QObject::tr("线划图数据格式转换工具箱");
static const QString sDescription = QObject::tr("提供线划图数据格式转换功能，包括：1、odata格式数据转shp格式数据；2、文本文件字符集编码转化工具（GBK-->UTF8）；3、shapefile文件之间的注记指针反向挂接；4、从PBF文件提取数据存储在GeoPackage中");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;

// 插件图标
static const QString sPluginIcon = QStringLiteral(":/odata_format_conversion_toolbox/images/odata_format_conversion_toolbox_icon.svg");

QgsOdataFormatConversionPlugin::QgsOdataFormatConversionPlugin(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

QgsOdataFormatConversionPlugin::~QgsOdataFormatConversionPlugin()
{
}

void QgsOdataFormatConversionPlugin::initGui()
{
  if (mActionVectorConvert)
  {
    delete mActionVectorConvert;
  }
  if (mActionTextFilesGBK2UTF8_tool)
  {
    delete mActionTextFilesGBK2UTF8_tool;
  }
  if (mActionAnnoPointerReverseExtract)
  {
    delete mActionAnnoPointerReverseExtract;
  }
  if (mAction_pbf2gpkg)
  {
    delete mAction_pbf2gpkg;
  }
	
#pragma region "1、菜单：odata2shapefile格式转换"
	/*****************************************************/
	/*    “【格式转换】-【线划图数据格式转换】”菜单	*/
	/*****************************************************/
  // Create the action for tool
  mActionVectorConvert = new QAction(QIcon(":/odata_format_conversion_toolbox/images/odata2shp_icon.svg"), tr("将odata数据转换为shp格式数据"), this);
  mActionVectorConvert->setObjectName(QStringLiteral("mActionVectorConvert"));
  // Set the what's this text
  mActionVectorConvert->setWhatsThis(tr("将ODATA文本数据格式转化成shapefile二进制文件格式"));
  // Connect the action to the run
  connect(mActionVectorConvert, &QAction::triggered, this, &QgsOdataFormatConversionPlugin::OdataFormatConversion);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mActionVectorConvert);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：odata2shapefile格式转换"), mActionVectorConvert);
  mActionVectorConvert->setEnabled(true);


  /*****************************************************/
  /*    “【格式转换】-【字符集编码转换】”菜单	*/
  /*****************************************************/
  // Create the action for tool
  mActionTextFilesGBK2UTF8_tool = new QAction(QIcon(":/odata_format_conversion_toolbox/images/se_text_files_gbk2utf8_icon.svg"), tr("文本文件字符集编码转化工具（GBK-->UTF8）"), this);
  mActionTextFilesGBK2UTF8_tool->setObjectName(QStringLiteral("mActionTextFilesGBK2UTF8_tool"));
  // Set the what's this text
  mActionTextFilesGBK2UTF8_tool->setWhatsThis(tr("文本文件字符集编码转化工具（GBK-->UTF8）"));
  // Connect the action to the run
  connect(mActionTextFilesGBK2UTF8_tool, &QAction::triggered, this, &QgsOdataFormatConversionPlugin::TextFilesGBK2UTF8_tool);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mActionTextFilesGBK2UTF8_tool);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：odata2shapefile格式转换"), mActionTextFilesGBK2UTF8_tool);
  mActionTextFilesGBK2UTF8_tool->setEnabled(true);
#pragma endregion

#pragma region "2、菜单：shapefile文件注记去重"
	/*****************************************************/
	/*    “【注记去重】-【注记指针反向挂接】”菜单	*/
	/*****************************************************/
  // Create the action for tool
  mActionAnnoPointerReverseExtract = new QAction(QIcon(":/odata_format_conversion_toolbox/images/annotation_reverse_extract_icon.svg"), tr("shapefile文件之间的注记指针反向挂接"), this);
  mActionAnnoPointerReverseExtract->setObjectName(QStringLiteral("mActionAnnoPointerReverseExtract"));
  // Set the what's this text
  mActionAnnoPointerReverseExtract->setWhatsThis(tr("shapefile文件之间的注记指针反向挂接"));
  // Connect the action to the run
  connect(mActionAnnoPointerReverseExtract, &QAction::triggered, this, &QgsOdataFormatConversionPlugin::AnnoPointerReverseExtract);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mActionAnnoPointerReverseExtract);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：shapefile文件注记去重"), mActionAnnoPointerReverseExtract);
  mActionAnnoPointerReverseExtract->setEnabled(true);
#pragma endregion

#pragma region "3、菜单：地理空间数据处理小工具集"
	/*****************************************************/
	/*    “【地理空间数据处理小工具集合】-【pbf文件格式向GeoPackage文件格式转化】”菜单	*/
	/*****************************************************/
  // Create the action for tool
  mAction_pbf2gpkg = new QAction(QIcon(":/odata_format_conversion_toolbox/images/pbf2gpkg_icon.svg"), tr("提取PBF数据存储在GeoPackage"), this);
  mAction_pbf2gpkg->setObjectName(QStringLiteral("mAction_pbf2gpkg"));
  // Set the what's this text
  mAction_pbf2gpkg->setWhatsThis(tr("从PBF文件提取数据存储在GeoPackage中"));
  // Connect the action to the run
  connect(mAction_pbf2gpkg, &QAction::triggered, this, &QgsOdataFormatConversionPlugin::pbf2gpkg_func);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_pbf2gpkg);
  mQGisIface->addPluginToVectorMenu(tr("&菜单：地理空间数据处理小工具集"), mAction_pbf2gpkg);
  mAction_pbf2gpkg->setEnabled(true);

	/*****************************************************/
	/*    “【地理空间数据处理小工具集合】-【其他的工具】”菜单	*/
	/*****************************************************/

#pragma endregion

	updateActions();
}

void QgsOdataFormatConversionPlugin::OdataFormatConversion()
{
	CSE_VectorFormatConversionDialog* pDlg = new CSE_VectorFormatConversionDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }
  else
  {
    // 用户点击了取消：删除对话框对象，释放内存
    delete pDlg;
  }
}

void QgsOdataFormatConversionPlugin::AnnoPointerReverseExtract()
{
	CSE_AnnoPointerReverseExtractDialog* pDlg = new CSE_AnnoPointerReverseExtractDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }
  else
  {
    // 用户点击了取消：删除对话框对象，释放内存
    delete pDlg;
  }
}

void QgsOdataFormatConversionPlugin::pbf2gpkg_func()
{
	CSE_pbf2gpkg_dialog* pDlg = new CSE_pbf2gpkg_dialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }
  else
  {
    // 用户点击了取消：删除对话框对象，释放内存
    delete pDlg;
  }
}

void QgsOdataFormatConversionPlugin::TextFilesGBK2UTF8_tool()
{
  TextFilesGBK2UTF8* pDlg = new TextFilesGBK2UTF8(mQGisIface, nullptr, QgsGuiUtils::ModalDialogFlags);

  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }
  else
  {
    // 用户点击了取消：删除对话框对象，释放内存
    delete pDlg;
  }
}

void QgsOdataFormatConversionPlugin::unload()
{
#pragma region "1、菜单：odata2shapefile格式转换"
	// 去掉ui界面
	mQGisIface->removePluginVectorMenu(tr("&菜单：odata2shapefile格式转换"), mActionVectorConvert);
	mQGisIface->removeVectorToolBarIcon(mActionVectorConvert);
	delete mActionVectorConvert;

  // 去掉ui界面
  mQGisIface->removePluginVectorMenu(tr("&菜单：odata2shapefile格式转换"), mActionTextFilesGBK2UTF8_tool);
  mQGisIface->removeVectorToolBarIcon(mActionTextFilesGBK2UTF8_tool);
  delete mActionTextFilesGBK2UTF8_tool;

#pragma endregion

#pragma region "2、菜单：shapefile文件注记去重"

	// 去掉ui界面
	mQGisIface->removePluginVectorMenu(tr("&菜单：shapefile文件注记去重"), mActionAnnoPointerReverseExtract);
	mQGisIface->removeVectorToolBarIcon(mActionAnnoPointerReverseExtract);
	delete mActionAnnoPointerReverseExtract;
#pragma endregion

#pragma region "3、菜单：地理空间数据处理小工具集"
  // 去掉ui界面
  mQGisIface->removePluginVectorMenu(tr("&菜单：地理空间数据处理小工具集"), mAction_pbf2gpkg);
  mQGisIface->removeVectorToolBarIcon(mAction_pbf2gpkg);
  delete mAction_pbf2gpkg;
#pragma endregion

}

void QgsOdataFormatConversionPlugin::updateActions()
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
	return new QgsOdataFormatConversionPlugin(qgisInterfacePointer);
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


