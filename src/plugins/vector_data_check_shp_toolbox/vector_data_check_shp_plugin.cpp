#include "vector_data_check_shp_plugin.h"

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

#include "ui_class/se_feature_attribute_check.h"
#include "ui_class/se_feature_category_statistics.h"
#include "ui_class/se_feature_extent_check.h"
#include "ui_class/se_layer_validity_check.h"
#include "ui_class/se_layer_integrity_check.h"
#include "ui_class/se_feature_attribute_check_yth.h"


// 消息日志
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"

static const QString sName = QObject::tr("格式转换成果检查插件");
static const QString sDescription = QObject::tr("提供格式转换成果检查功能，包括：要素范围检查、要素属性检查、要素个数统计、图层文件可用性检查、图层文件完整性检查等。");
static const QString sCategory = QObject::tr("矢量");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;

// 插件图标
static const QString sPluginIcon = QObject::tr("格式转换成果检查插件"); 

QgsVectorDataCheckShpPlugin::QgsVectorDataCheckShpPlugin(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

QgsVectorDataCheckShpPlugin::~QgsVectorDataCheckShpPlugin()
{
}



void QgsVectorDataCheckShpPlugin::initGui()
{
	delete mActionFeatureAttributeCheck;
	delete mActionFeatureExtentCheck;
	delete mActionFeatureCategoryStatistics;
	delete mActionLayerValidityCheck;
	delete mActionLayerIntegrityCheck;
	delete mActionFeatureAttributeCheckYTH;

	// 程序运行目录
	QString curExePath = QCoreApplication::applicationDirPath();

	/*****************************************************/
	/*    “【格式转换成果检查】-【要素属性检查】”菜单	*/
	/*****************************************************/
	QIcon iconFeatureAttributeCheck;
	QString strFeatureAttributeCheck = curExePath + "/resource/toolbox/feature_attribute_check.png";
	iconFeatureAttributeCheck.addFile(strFeatureAttributeCheck);
	mActionFeatureAttributeCheck = new QAction(iconFeatureAttributeCheck, tr("要素属性检查"), this);
	mActionFeatureAttributeCheck->setObjectName(QStringLiteral("mActionFeatureAttributeCheck"));
	mActionFeatureAttributeCheck->setToolTip(tr("格式转换成果属性值规范性检查"));
	connect(mActionFeatureAttributeCheck, &QAction::triggered, this, &QgsVectorDataCheckShpPlugin::FeatureAttributeCheck);
	mQGisIface->addVectorToolBarIcon(mActionFeatureAttributeCheck);
	mQGisIface->addPluginToVectorMenu(tr("&格式转换成果检查"), mActionFeatureAttributeCheck);
	mActionFeatureAttributeCheck->setEnabled(true);


	/*****************************************************/
	/*    “【格式转换成果检查】-【要素范围检查】”菜单	*/
	/*****************************************************/
	QIcon iconFeatureExtentCheck;
	QString strFeatureExtentCheck = curExePath + "/resource/toolbox/feature_extent_check.png";
	iconFeatureExtentCheck.addFile(strFeatureExtentCheck);
	mActionFeatureExtentCheck = new QAction(iconFeatureExtentCheck, tr("要素范围检查"), this);
	mActionFeatureExtentCheck->setObjectName(QStringLiteral("mActionFeatureExtentCheck"));
	mActionFeatureExtentCheck->setToolTip(tr("格式转换成果数据范围正确性检查"));
	connect(mActionFeatureExtentCheck, &QAction::triggered, this, &QgsVectorDataCheckShpPlugin::FeatureExtentCheck);
	mQGisIface->addVectorToolBarIcon(mActionFeatureExtentCheck);
	mQGisIface->addPluginToVectorMenu(tr("&格式转换成果检查"), mActionFeatureExtentCheck);
	mActionFeatureExtentCheck->setEnabled(true);


	/*****************************************************/
	/*    “【格式转换成果检查】-【要素分类统计】”菜单	*/
	/*****************************************************/
	QIcon iconFeatureCategoryStatistics;
	QString strFeatureCategoryStatistics = curExePath + "/resource/toolbox/feature_statistic.png";
	iconFeatureCategoryStatistics.addFile(strFeatureCategoryStatistics);
	mActionFeatureCategoryStatistics = new QAction(iconFeatureCategoryStatistics, tr("要素分类统计"), this);
	mActionFeatureCategoryStatistics->setObjectName(QStringLiteral("mActionFeatureCategoryStatistics"));
	mActionFeatureCategoryStatistics->setToolTip(tr("格式转换成果数据要素条目数缺失检查"));
	connect(mActionFeatureCategoryStatistics, &QAction::triggered, this, &QgsVectorDataCheckShpPlugin::FeatureCategoryStatistics);
	mQGisIface->addVectorToolBarIcon(mActionFeatureCategoryStatistics);
	mQGisIface->addPluginToVectorMenu(tr("&格式转换成果检查"), mActionFeatureCategoryStatistics);
	mActionFeatureCategoryStatistics->setEnabled(true);


	/*****************************************************/
	/* “【格式转换成果检查】-【图层文件可用性检查】”菜单*/
	/*****************************************************/
	QIcon iconLayerValidityCheck;
	QString strLayerValidityCheck = curExePath + "/resource/toolbox/layer_validity_check.png";
	iconLayerValidityCheck.addFile(strLayerValidityCheck);
	mActionLayerValidityCheck = new QAction(iconLayerValidityCheck, tr("图层文件可用性检查"), this);
	mActionLayerValidityCheck->setObjectName(QStringLiteral("mActionLayerValidityCheck"));
	mActionLayerValidityCheck->setToolTip(tr("检查图层文件可用性"));
	connect(mActionLayerValidityCheck, &QAction::triggered, this, &QgsVectorDataCheckShpPlugin::LayerValidityCheck);
	mQGisIface->addVectorToolBarIcon(mActionLayerValidityCheck);
	mQGisIface->addPluginToVectorMenu(tr("&格式转换成果检查"), mActionLayerValidityCheck);
	mActionLayerValidityCheck->setEnabled(true);


	/*****************************************************/
	/* “【格式转换成果检查】-【图层文件完整性检查】”菜单*/
	/*****************************************************/
	QIcon iconLayerIntegrityCheck;
	QString strLayerIntegrityCheck = curExePath + "/resource/toolbox/layer_integrity_check.png";
	iconLayerIntegrityCheck.addFile(strLayerIntegrityCheck);
	mActionLayerIntegrityCheck = new QAction(iconLayerIntegrityCheck, tr("图层文件完整性检查"), this);
	mActionLayerIntegrityCheck->setObjectName(QStringLiteral("mActionLayerIntegrityCheck"));
	mActionLayerIntegrityCheck->setToolTip(tr("检查图层文件完整性，包括：shx文件、dbf文件、prj文件"));
	connect(mActionLayerIntegrityCheck, &QAction::triggered, this, &QgsVectorDataCheckShpPlugin::LayerIntegrityCheck);
	mQGisIface->addVectorToolBarIcon(mActionLayerIntegrityCheck);
	mQGisIface->addPluginToVectorMenu(tr("&格式转换成果检查"), mActionLayerIntegrityCheck);
	mActionLayerIntegrityCheck->setEnabled(true);

	/*****************************************************/
	/*    “【格式转换成果检查】-【一体化数据属性检查】”菜单	*/
	/*****************************************************/
	QIcon iconFeatureAttributeCheckYTH;
	QString strFeatureAttributeCheckYTH = curExePath + "/resource/toolbox/feature_attribute_check.png";
	iconFeatureAttributeCheckYTH.addFile(strFeatureAttributeCheckYTH);
	mActionFeatureAttributeCheckYTH = new QAction(iconFeatureAttributeCheckYTH, tr("一体化数据属性检查"), this);
	mActionFeatureAttributeCheckYTH->setObjectName(QStringLiteral("mActionFeatureAttributeCheckYTH"));
	mActionFeatureAttributeCheckYTH->setToolTip(tr("一体化数据属性值规范性检查"));
	connect(mActionFeatureAttributeCheckYTH, &QAction::triggered, this, &QgsVectorDataCheckShpPlugin::FeatureAttributeCheckYTH);
	mQGisIface->addVectorToolBarIcon(mActionFeatureAttributeCheckYTH);
	mQGisIface->addPluginToVectorMenu(tr("&一体化数据成果检查"), mActionFeatureAttributeCheckYTH);
	mActionFeatureAttributeCheckYTH->setEnabled(true);
	updateActions();
}


void QgsVectorDataCheckShpPlugin::FeatureAttributeCheck()
{
	CSE_FeatureAttributeCheckDialog* pDlg = new CSE_FeatureAttributeCheckDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsVectorDataCheckShpPlugin::FeatureExtentCheck()
{
	CSE_FeatureExtentCheckDialog* pDlg = new CSE_FeatureExtentCheckDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsVectorDataCheckShpPlugin::FeatureCategoryStatistics()
{
	CSE_FeatureCategoryStatisticsDialog* pDlg = new CSE_FeatureCategoryStatisticsDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsVectorDataCheckShpPlugin::LayerValidityCheck()
{
	CSE_LayerValidityCheckDialog* pDlg = new CSE_LayerValidityCheckDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsVectorDataCheckShpPlugin::LayerIntegrityCheck()
{
	CSE_LayerIntegrityCheckDialog* pDlg = new CSE_LayerIntegrityCheckDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsVectorDataCheckShpPlugin::FeatureAttributeCheckYTH()
{
	CSE_FeatureAttributeCheckYTHDialog* pDlg = new CSE_FeatureAttributeCheckYTHDialog(mQGisIface, nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}



void QgsVectorDataCheckShpPlugin::unload()
{
	// 去掉ui界面
	mQGisIface->removePluginVectorMenu(tr("&格式转换成果检查"), mActionFeatureAttributeCheck);
	mQGisIface->removeVectorToolBarIcon(mActionFeatureAttributeCheck);

	mQGisIface->removePluginVectorMenu(tr("&格式转换成果检查"), mActionFeatureExtentCheck);
	mQGisIface->removeVectorToolBarIcon(mActionFeatureExtentCheck);

	mQGisIface->removePluginVectorMenu(tr("&格式转换成果检查"), mActionFeatureCategoryStatistics);
	mQGisIface->removeVectorToolBarIcon(mActionFeatureCategoryStatistics);

	mQGisIface->removePluginVectorMenu(tr("&格式转换成果检查"), mActionLayerValidityCheck);
	mQGisIface->removeVectorToolBarIcon(mActionLayerValidityCheck);

	mQGisIface->removePluginVectorMenu(tr("&格式转换成果检查"), mActionLayerIntegrityCheck);
	mQGisIface->removeVectorToolBarIcon(mActionLayerIntegrityCheck);

	mQGisIface->removePluginVectorMenu(tr("&一体化数据成果检查"), mActionFeatureAttributeCheckYTH);
	mQGisIface->removeVectorToolBarIcon(mActionFeatureAttributeCheckYTH);

	delete mActionFeatureAttributeCheck;
	delete mActionFeatureExtentCheck;
	delete mActionFeatureCategoryStatistics;
	delete mActionLayerValidityCheck;
	delete mActionLayerIntegrityCheck;
	delete mActionFeatureAttributeCheckYTH;
}


void QgsVectorDataCheckShpPlugin::updateActions()
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
	return new QgsVectorDataCheckShpPlugin(qgisInterfacePointer);
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


