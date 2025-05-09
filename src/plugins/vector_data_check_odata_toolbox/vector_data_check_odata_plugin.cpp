#include "vector_data_check_odata_plugin.h"

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


#include "ui_class/se_layer_integrity_check.h"
#include "ui_class/se_layer_rareword_statistics.h"
#include "ui_class/se_layer_naming_standardization_check.h"
#include "ui_class/se_layer_code_standardization_check.h"

// 消息日志
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"
#include <qgisplugin.h>

static const QString sName = QObject::tr("odata数据质量检查插件");
static const QString sDescription = QObject::tr("提供odata数据质量检查功能，包括：数据文件编码规范性检查（属性文件是否存在乱码、\
	坐标文件是否存在乱码、是否存在异常值、字段缺失或多余）、图幅文件与数据文件命名规范检查、图层文件完整性检查、生僻字统计。");
static const QString sCategory = QObject::tr("矢量");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;

// 插件图标
static const QString sPluginIcon = QObject::tr("odata数据质量检查插件"); 

QgsVectorDataCheckOdataPlugin::QgsVectorDataCheckOdataPlugin(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

QgsVectorDataCheckOdataPlugin::~QgsVectorDataCheckOdataPlugin()
{
}



void QgsVectorDataCheckOdataPlugin::initGui()
{
	delete mActionLayerIntegrityCheck;
	delete mActionLayerRarewordStatistics;
	delete mActionLayerNamingStandardizationCheck;
	delete mActionLayerCodeStandardizationCheck;

	// 程序运行目录
	QString curExePath = QCoreApplication::applicationDirPath();

	/*****************************************************/
	/* “【odata数据质量检查】-【图层文件完整性检查】”菜单*/
	/*****************************************************/
	QIcon iconLayerIntegrityCheck;
	QString strLayerIntegrityCheck = curExePath + "/resource/toolbox/layer_integrity_check.png";
	iconLayerIntegrityCheck.addFile(strLayerIntegrityCheck);
	mActionLayerIntegrityCheck = new QAction(iconLayerIntegrityCheck, tr("文件完整性检查"), this);
	mActionLayerIntegrityCheck->setObjectName(QStringLiteral("mActionLayerIntegrityCheck"));
	mActionLayerIntegrityCheck->setToolTip(tr("图层文件完整性要求：每个图层都应该包含4个文件，SX文件，ZB文件，TP文件，MS文件，每个图幅文件包含一个SMS文件"));
	connect(mActionLayerIntegrityCheck, &QAction::triggered, this, &QgsVectorDataCheckOdataPlugin::LayerIntegrityCheck);
	mQGisIface->addVectorToolBarIcon(mActionLayerIntegrityCheck);
	mQGisIface->addPluginToVectorMenu(tr("&odata数据质量检查"), mActionLayerIntegrityCheck);
	mActionLayerIntegrityCheck->setEnabled(true);

	/*****************************************************/
	/* “【odata数据质量检查】-【生僻字统计】”菜单*/
	/*****************************************************/
	QIcon iconLayerRarewordStatistics;
	QString strLayerRarewordStatistics = curExePath + "/resource/toolbox/feature_statistic.png";
	iconLayerRarewordStatistics.addFile(strLayerRarewordStatistics);
	mActionLayerRarewordStatistics = new QAction(iconLayerRarewordStatistics, tr("生僻字统计"), this);
	mActionLayerRarewordStatistics->setObjectName(QStringLiteral("mActionLayerRarewordStatistics"));
	mActionLayerRarewordStatistics->setToolTip(tr("统计图层属性文件中的生僻字"));
	connect(mActionLayerRarewordStatistics, &QAction::triggered, this, &QgsVectorDataCheckOdataPlugin::LayerRarewordStatistics);
	mQGisIface->addVectorToolBarIcon(mActionLayerRarewordStatistics);
	mQGisIface->addPluginToVectorMenu(tr("&odata数据质量检查"), mActionLayerRarewordStatistics);
	mActionLayerRarewordStatistics->setEnabled(true);

	/*****************************************************/
	/* “【odata数据质量检查】-【命名规范性检查】”菜单*/
	/*****************************************************/
	QIcon iconLayerNamingStandCheck;
	QString strLayerNamingStandCheck = curExePath + "/resource/toolbox/naming_standardization.png";
	iconLayerNamingStandCheck.addFile(strLayerNamingStandCheck);
	mActionLayerNamingStandardizationCheck = new QAction(iconLayerNamingStandCheck, tr("命名正确性检查"), this);
	mActionLayerNamingStandardizationCheck->setObjectName(QStringLiteral("mActionLayerNamingStandardizationCheck"));
	mActionLayerNamingStandardizationCheck->setToolTip(tr("检查图幅文件命名规范性、数据文件命名规范性（图幅长度和比例尺对应）"));
	connect(mActionLayerNamingStandardizationCheck, &QAction::triggered, this, &QgsVectorDataCheckOdataPlugin::LayerNamingStandardizationCheck);
	mQGisIface->addVectorToolBarIcon(mActionLayerNamingStandardizationCheck);
	mQGisIface->addPluginToVectorMenu(tr("&odata数据质量检查"), mActionLayerNamingStandardizationCheck);
	mActionLayerNamingStandardizationCheck->setEnabled(true);


	/*****************************************************/
	/* “【odata数据质量检查】-【数据编码规范性检查】”菜单*/
	/*****************************************************/
	QIcon iconLayerCodeStandCheck;
	QString strLayerCodeStandCheck = curExePath + "/resource/toolbox/data_code_check.png";
	iconLayerCodeStandCheck.addFile(strLayerCodeStandCheck);
	mActionLayerCodeStandardizationCheck = new QAction(iconLayerCodeStandCheck, tr("数据编码规范性检查"), this);
	mActionLayerCodeStandardizationCheck->setObjectName(QStringLiteral("mActionLayerCodeStandardizationCheck"));
	mActionLayerCodeStandardizationCheck->setToolTip(tr("检查SX文件是否乱码、ZB文件是否存在乱码或异常值（超出正常精度要求，小数点后6位）、字段（SX文件）缺失或多余"));
	connect(mActionLayerCodeStandardizationCheck, &QAction::triggered, this, &QgsVectorDataCheckOdataPlugin::LayerCodeStandardizationCheck);
	mQGisIface->addVectorToolBarIcon(mActionLayerCodeStandardizationCheck);
	mQGisIface->addPluginToVectorMenu(tr("&odata数据质量检查"), mActionLayerCodeStandardizationCheck);
	mActionLayerCodeStandardizationCheck->setEnabled(true);



	updateActions();
}



void QgsVectorDataCheckOdataPlugin::LayerRarewordStatistics()
{
	CSE_LayerRarewordStatisticsDialog* pDlg = new CSE_LayerRarewordStatisticsDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsVectorDataCheckOdataPlugin::LayerIntegrityCheck()
{
	CSE_LayerIntegrityCheckDialog* pDlg = new CSE_LayerIntegrityCheckDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsVectorDataCheckOdataPlugin::LayerNamingStandardizationCheck()
{
	CSE_LayerNamingStandardizationCheckDialog* pDlg = new CSE_LayerNamingStandardizationCheckDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsVectorDataCheckOdataPlugin::LayerCodeStandardizationCheck()
{
	CSE_LayerCodeStandardizationCheckDialog* pDlg = new CSE_LayerCodeStandardizationCheckDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}



void QgsVectorDataCheckOdataPlugin::unload()
{
	// 去掉ui界面
	mQGisIface->removePluginVectorMenu(tr("&odata数据质量检查"), mActionLayerIntegrityCheck);
	mQGisIface->removeVectorToolBarIcon(mActionLayerIntegrityCheck);
	delete mActionLayerIntegrityCheck;

	mQGisIface->removePluginVectorMenu(tr("&odata数据质量检查"), mActionLayerRarewordStatistics);
	mQGisIface->removeVectorToolBarIcon(mActionLayerRarewordStatistics);
	delete mActionLayerRarewordStatistics;

	mQGisIface->removePluginVectorMenu(tr("&odata数据质量检查"), mActionLayerNamingStandardizationCheck);
	mQGisIface->removeVectorToolBarIcon(mActionLayerNamingStandardizationCheck);
	delete mActionLayerNamingStandardizationCheck;

	mQGisIface->removePluginVectorMenu(tr("&odata数据质量检查"), mActionLayerCodeStandardizationCheck);
	mQGisIface->removeVectorToolBarIcon(mActionLayerCodeStandardizationCheck);
	delete mActionLayerCodeStandardizationCheck;

}


void QgsVectorDataCheckOdataPlugin::updateActions()
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
	return new QgsVectorDataCheckOdataPlugin(qgisInterfacePointer);
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


