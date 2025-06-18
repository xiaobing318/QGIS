#include "geoextractandprocess_plugin.h"
#include "geoextractandprocess_plugin_gui.h"
#include "geoextractandprocess_progress_dialog.h"
#include "geoextractandprocess_plugin_setmergeparams.h"
#include "convert_operationarea_and_sheet.h"
#include "create_operationarea_between.h"
#include "image_spatial_accuracy_check.h"
#include "vector_spatial_accuracy_check.h"

#include "qgisinterface.h"
#include "qgsguiutils.h"
#include "qgsproject.h"
#include "qgsmessagebar.h"
#include "qgsmapcanvas.h"
#include "qgsapplication.h"

#include "set_attribute_extract_params.h"
#include "create_operationarea_range.h"
#include "automerge.h"
#include "multiscale_addfield.h"
#include "multiscale_match.h"
#include "better_than_5w_match.h"
#include "better_than_5w_merge_features.h"
#include "convert_data_to_gjb.h"
#include "convert_data_to_gb_or_gjb.h"
#include "image_mapping.h"
#include "moving_target_mapping.h"
#include "image_mapping_gdxdata.h"
#include "image_mapping_layout_vectorization.h"
#include "copy_shapefiles.h"
#include "moving_target_mapping_layout_vectorization.h"


#include <QAction>
#include <QMessageBox>
#include <QtCore/QProcess>

// 消息日志
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"

/*作业区数据库结构体*/
struct OperationAreaDBInfo
{
	string strName;						// 作业区数据库名称
	string strFilePath;					// 作业区数据库全路径
	vector<string> vInputDataPaths;		// 入库数据所在路径

	OperationAreaDBInfo()
	{
		strName = "";
		strFilePath = "";
		vInputDataPaths.clear();
	}
};

static const QString sName = QObject::tr("地理数据提取与加工");
static const QString sDescription = QObject::tr("提供系列比例尺提取与加工、多尺度数据融合、优于1:5万数据融合处理等功能。");
static const QString sCategory = QObject::tr("Vector");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;

// 插件图标
static const QString sPluginIcon = QStringLiteral(":/resource/geoextractandprocess_createdb.png");

/*************************************************/
//===================内部函数====================//
/*判断当前作业区名称是否已经存在*/
bool bIsInRefDBVector(string strOpName, vector<OperationAreaDBInfo>& vOpDBInfos, int& iIndex)
{
	iIndex = -1;
	for (int i = 0; i < vOpDBInfos.size(); i++)
	{
		if (strOpName == vOpDBInfos[i].strName)
		{
			iIndex = i;
			return true;
		}
	}

	return false;
}

/**************************************************/

QgsGeoExtractAndProcessPlugin::QgsGeoExtractAndProcessPlugin(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{
	m_vGpkgPath.clear();
	m_iScaleType = 0;
	m_dDistance = 0;
	m_vLayerMatchParam.clear();
	m_vMultiScaleMatchParam.clear();
}

QgsGeoExtractAndProcessPlugin::~QgsGeoExtractAndProcessPlugin()
{
}

void QgsGeoExtractAndProcessPlugin::initGui()
{
	delete mActionCreateDbAndInputData;
	delete mActionSetMergeParams;
	delete mActionExtractData;
	delete mActionCreateOperationArea;
	delete mActionConvertOperationAreaAndSheet;
	delete mActionAutoMerge;
	delete mActionOpBetweenTempDB;
	delete mActionBetweenOpAutoMerge;
	delete mActionImageSpatialAccuracyCheck;
	delete mActionVectorSpatialAccuracyCheck;
	delete mActionPreProcessMultiScaleData;
	delete mActionSetBetterThan5WMatchParams;
	delete mActionExtractContour;
	delete mActionConvertDataToGB_OR_JB;
	//delete mActionImageMapping;
	delete mActionMovingTargetMapping;
	delete mActionImageMapping_GdxData;
	delete mActionImageMapping_LayoutVectorization;
	delete mActionCopyShapefiles;

	// 程序运行目录
	QString curExePath = QCoreApplication::applicationDirPath();

	/*****************************************************/
	/*    “作业区与图幅计算— 创建作业区”菜单          */
	/*****************************************************/
	QIcon iconCreateOperationAreaIcon;
	QString strIconCreateOperationAreaIcon = curExePath + "/resource/geoextractandprocess/createop.png";
	iconCreateOperationAreaIcon.addFile(strIconCreateOperationAreaIcon);
	mActionCreateOperationArea = new QAction(iconCreateOperationAreaIcon, tr("创建作业区"), this);
	mActionCreateOperationArea->setObjectName(QStringLiteral("mActionCreateOperationArea"));
	mActionCreateOperationArea->setWhatsThis(tr("根据作业区范围创建作业区矢量要素文件"));
	connect(mActionCreateOperationArea, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::CreateOperationArea);
	mQGisIface->addVectorToolBarIcon(mActionCreateOperationArea);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺提取与加工"), mActionCreateOperationArea);
	mActionCreateOperationArea->setEnabled(true);

	/*****************************************************/
	/*“作业区与图幅计算— 作业区名称与图幅号转换”菜单  */
	/*****************************************************/
	QIcon iconConvertOpAndSheetIcon;
	QString strIconConvertOpAndSheet = curExePath + "/resource/geoextractandprocess/convert_sheet_op.png";
	iconConvertOpAndSheetIcon.addFile(strIconConvertOpAndSheet);
	mActionConvertOperationAreaAndSheet = new QAction(iconConvertOpAndSheetIcon, tr("作业区名称与图幅号转换"), this);
	mActionConvertOperationAreaAndSheet->setObjectName(QStringLiteral("mActionConvertOperationAreaAndSheet"));
	mActionConvertOperationAreaAndSheet->setWhatsThis(tr("作业区名称与图幅号转换"));
	connect(mActionConvertOperationAreaAndSheet, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ConvertOpNameAndSheet);
	mQGisIface->addVectorToolBarIcon(mActionConvertOperationAreaAndSheet);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺提取与加工"), mActionConvertOperationAreaAndSheet);
	mActionConvertOperationAreaAndSheet->setEnabled(true);

	/********************************/
	/*“创建作业区数据库并入库”菜单 */
	/********************************/
	QIcon iconCreateDB;
	QString strIconCreateDB = curExePath + "/resource/geoextractandprocess/db_in.png";
	iconCreateDB.addFile(strIconCreateDB);

	mActionCreateDbAndInputData = new QAction(iconCreateDB, tr("作业区建库及数据入库"), this);
	mActionCreateDbAndInputData->setObjectName(QStringLiteral("mActionCreateDbAndInputData"));
	// 设置菜单提示
	mActionCreateDbAndInputData->setWhatsThis(tr("创建作业区数据库并入库"));
	// 关联菜单和槽函数
	connect(mActionCreateDbAndInputData, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::CreateDbAndInputData);

	// Add the icon to the toolbar
	mQGisIface->addVectorToolBarIcon(mActionCreateDbAndInputData);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺提取与加工"), mActionCreateDbAndInputData);
	mActionCreateDbAndInputData->setEnabled(true);

	/********************************/
	/*    “接边检测参数设置”菜单 */
	/********************************/
	QIcon iconSetMergeParams;
	QString strIconSetMergeParams = curExePath + "/resource/geoextractandprocess/set_param.png";
	iconSetMergeParams.addFile(strIconSetMergeParams);
	mActionSetMergeParams = new QAction(iconSetMergeParams, tr("设置接边检测参数"), this);
	mActionSetMergeParams->setObjectName(QStringLiteral("mActionSetMergeParams"));
	mActionSetMergeParams->setWhatsThis(tr("设置接边检测参数"));
	connect(mActionSetMergeParams, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::SetMergeParams);
	mQGisIface->addVectorToolBarIcon(mActionSetMergeParams);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺提取与加工"), mActionSetMergeParams);
	mActionSetMergeParams->setEnabled(true);

	/********************************/
	/*    “作业区自动接边”菜单    */
	/********************************/
	QIcon iconAutoMerge;
	QString strIconAutoMerge = curExePath + "/resource/geoextractandprocess/auto_merge.png";
	iconAutoMerge.addFile(strIconAutoMerge);
	mActionAutoMerge = new QAction(iconAutoMerge, tr("作业区自动接边"), this);
	mActionAutoMerge->setObjectName(QStringLiteral("mActionAutoMerge"));
	mActionAutoMerge->setWhatsThis(tr("作业区自动接边"));
	connect(mActionAutoMerge, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::AutoMerge);
	mQGisIface->addVectorToolBarIcon(mActionAutoMerge);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺提取与加工"), mActionAutoMerge);
	mActionAutoMerge->setEnabled(true);

	/************************************/
	/* “创建作业区间临时接边库”菜单	*/
	/************************************/
	QIcon iconCreateTempOpBetweenDB;
	QString strIconCreateTempOpBetweenDB = curExePath + "/resource/geoextractandprocess/create_between_op.png";
	iconCreateTempOpBetweenDB.addFile(strIconCreateTempOpBetweenDB);
	mActionOpBetweenTempDB = new QAction(iconCreateTempOpBetweenDB, tr("创建作业区间临时接边库"), this);
	mActionOpBetweenTempDB->setObjectName(QStringLiteral("mActionOpBetweenTempDB"));
	mActionOpBetweenTempDB->setWhatsThis(tr("创建作业区间临时接边库"));
	connect(mActionOpBetweenTempDB, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::CreateOpBetweenTempDB);
	mQGisIface->addVectorToolBarIcon(mActionOpBetweenTempDB);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺提取与加工"), mActionOpBetweenTempDB);
	mActionOpBetweenTempDB->setEnabled(true);

	/********************************/
	/*   “作业区之间自动接边”菜单 */
	/********************************/
	QIcon iconBetweenOpAutoMerge;
	QString strIconBetweenOpAutoMerge = curExePath + "/resource/geoextractandprocess/auto_merge.png";
	iconBetweenOpAutoMerge.addFile(strIconBetweenOpAutoMerge);
	mActionBetweenOpAutoMerge = new QAction(iconBetweenOpAutoMerge, tr("作业区之间自动接边"), this);
	mActionBetweenOpAutoMerge->setObjectName(QStringLiteral("mActionBetweenOpAutoMerge"));
	mActionBetweenOpAutoMerge->setWhatsThis(tr("作业区之间自动接边"));
	connect(mActionBetweenOpAutoMerge, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::BetweenOpAutoMerge);
	mQGisIface->addVectorToolBarIcon(mActionBetweenOpAutoMerge);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺提取与加工"), mActionBetweenOpAutoMerge);
	mActionBetweenOpAutoMerge->setEnabled(true);

	/******************************************************/
	/*  系列比例尺地图要素提取—“要素提取”菜单          */
	/******************************************************/
	QIcon iconExtractDataIcon;
	QString strIconExtractData = curExePath + "/resource/geoextractandprocess/extract_layers.png";
	iconExtractDataIcon.addFile(strIconExtractData);
	mActionExtractData = new QAction(iconExtractDataIcon, tr("要素提取"), this);
	mActionExtractData->setObjectName(QStringLiteral("mActionExtractData"));
	mActionExtractData->setWhatsThis(tr("要素提取"));
	connect(mActionExtractData, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ExtractData);
	mQGisIface->addVectorToolBarIcon(mActionExtractData);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺地图要素提取"), mActionExtractData);
	mActionExtractData->setEnabled(true);

	/*****************************************************************/
	/*    “测绘地理数据预处理— 影像数据空间精度检查”菜单          */
	/*****************************************************************/
	QIcon iconImageSpatialAccuracyCheck;
	QString strIconImageSpatialAccuracyCheck = curExePath + "/resource/geoextractandprocess/image_accuracy_check.png";
	iconImageSpatialAccuracyCheck.addFile(strIconImageSpatialAccuracyCheck);
	mActionImageSpatialAccuracyCheck = new QAction(iconImageSpatialAccuracyCheck, tr("影像数据空间精度检查"), this);
	mActionImageSpatialAccuracyCheck->setObjectName(QStringLiteral("mActionImageSpatialAccuracyCheck"));
	mActionImageSpatialAccuracyCheck->setWhatsThis(tr("影像数据空间精度检查"));
	connect(mActionImageSpatialAccuracyCheck, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ImageSpatialAccuracyCheck);
	mQGisIface->addRasterToolBarIcon(mActionImageSpatialAccuracyCheck);
	mQGisIface->addPluginToRasterMenu(tr("&测绘地理数据预处理"), mActionImageSpatialAccuracyCheck);
	mActionImageSpatialAccuracyCheck->setEnabled(true);

	/*****************************************************************/
	/*    “测绘地理数据预处理— 矢量数据空间精度检查”菜单          */
	/*****************************************************************/
	QIcon iconVectorSpatialAccuracyCheck;
	QString strIconVectorSpatialAccuracyCheck = curExePath + "/resource/geoextractandprocess/vector_accuracy_check.png";
	iconVectorSpatialAccuracyCheck.addFile(strIconVectorSpatialAccuracyCheck);
	mActionVectorSpatialAccuracyCheck = new QAction(iconVectorSpatialAccuracyCheck, tr("矢量数据空间精度检查"), this);
	mActionVectorSpatialAccuracyCheck->setObjectName(QStringLiteral("mActionVectorSpatialAccuracyCheck"));
	mActionVectorSpatialAccuracyCheck->setWhatsThis(tr("矢量数据空间精度检查"));
	connect(mActionVectorSpatialAccuracyCheck, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::VectorSpatialAccuracyCheck);
	mQGisIface->addVectorToolBarIcon(mActionVectorSpatialAccuracyCheck);
	mQGisIface->addPluginToVectorMenu(tr("&测绘地理数据预处理"), mActionVectorSpatialAccuracyCheck);
	mActionVectorSpatialAccuracyCheck->setEnabled(true);

	//=================================================================//

	/*****************************************************************/
	/*    “多尺度数据一致性处理— 预处理”菜单                      */
	/*****************************************************************/
	QIcon iconPreProcessMultiScaleData;
	QString strIconPreProcessMultiScaleData = curExePath + "/resource/geoextractandprocess/preprocess.png";
	iconPreProcessMultiScaleData.addFile(strIconPreProcessMultiScaleData);
	mActionPreProcessMultiScaleData = new QAction(iconPreProcessMultiScaleData, tr("预处理"), this);
	mActionPreProcessMultiScaleData->setObjectName(QStringLiteral("mActionPreProcessMultiScaleData"));
	mActionPreProcessMultiScaleData->setWhatsThis(tr("预处理"));
	connect(mActionPreProcessMultiScaleData, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::PreProcessMultiScaleData);
	mQGisIface->addVectorToolBarIcon(mActionPreProcessMultiScaleData);
	mQGisIface->addPluginToVectorMenu(tr("&多尺度数据一致性处理"), mActionPreProcessMultiScaleData);
	mActionPreProcessMultiScaleData->setEnabled(true);

	/*****************************************************************/
	/*    “多尺度数据一致性处理— 多尺度匹配参数设置”菜单          */
	/*****************************************************************/
	QIcon iconSetMultiScaleDataParams;
	QString strIconSetMultiScaleDataParams = curExePath + "/resource/geoextractandprocess/set_param.png";
	iconSetMultiScaleDataParams.addFile(strIconSetMultiScaleDataParams);
	mActionSetMultiScaleDataParams = new QAction(iconSetMultiScaleDataParams, tr("多尺度匹配参数设置"), this);
	mActionSetMultiScaleDataParams->setObjectName(QStringLiteral("mActionSetMultiScaleDataParams"));
	mActionSetMultiScaleDataParams->setWhatsThis(tr("多尺度匹配参数设置"));
	connect(mActionSetMultiScaleDataParams, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::SetMultiScaleDataMatchParams);
	mQGisIface->addVectorToolBarIcon(mActionSetMultiScaleDataParams);
	mQGisIface->addPluginToVectorMenu(tr("&多尺度数据一致性处理"), mActionSetMultiScaleDataParams);
	mActionSetMultiScaleDataParams->setEnabled(true);
	

	/*****************************************************************/
	/*    “优于1:5万数据融合处理— 矢量数据语义融合”菜单		 */
	/*****************************************************************/
	QIcon iconConvertDataToGJB;
	QString strIconConvertDataToGJB = curExePath + "/resource/geoextractandprocess/gjb_integration.png";
	iconConvertDataToGJB.addFile(strIconConvertDataToGJB);
	mActionConvertDataToGJB = new QAction(iconConvertDataToGJB, tr("矢量数据语义融合"), this);
	mActionConvertDataToGJB->setObjectName(QStringLiteral("mActionConvertDataToGJB"));
	mActionConvertDataToGJB->setWhatsThis(tr("矢量数据语义融合"));
	connect(mActionConvertDataToGJB, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ConvertDataToGJB);
	mQGisIface->addVectorToolBarIcon(mActionConvertDataToGJB);
	mQGisIface->addPluginToVectorMenu(tr("&优于1:5万数据融合处理"), mActionConvertDataToGJB);
	mActionConvertDataToGJB->setEnabled(true);

	/*****************************************************************/
	/*    “优于1:5万数据融合处理— 国/军标数据导出”菜单		     */
	/*****************************************************************/
	QIcon iconConvertDataToGB_OR_JB;
	QString strIconConvertDataToGB_OR_JB = curExePath + "/resource/geoextractandprocess/gjb_export.png";
	iconConvertDataToGB_OR_JB.addFile(strIconConvertDataToGB_OR_JB);
	mActionConvertDataToGB_OR_JB = new QAction(iconConvertDataToGB_OR_JB, tr("国/军标数据导出"), this);
	mActionConvertDataToGB_OR_JB->setObjectName(QStringLiteral("mActionConvertDataToGB_OR_JB"));
	mActionConvertDataToGB_OR_JB->setWhatsThis(tr("国/军标数据导出"));
	connect(mActionConvertDataToGB_OR_JB, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ConvertDataToGB_OR_JB);
	mQGisIface->addVectorToolBarIcon(mActionConvertDataToGB_OR_JB);
	mQGisIface->addPluginToVectorMenu(tr("&优于1:5万数据融合处理"), mActionConvertDataToGB_OR_JB);
	mActionConvertDataToGB_OR_JB->setEnabled(true);

	/*****************************************************************/
	/*    “优于1:5万数据融合处理— 数据匹配”菜单          */
	/*****************************************************************/
	QIcon iconSetBetterThan5WMatchParams;
	QString strIconSetBetterThan5WMatchParams = curExePath + "/resource/geoextractandprocess/set_param.png";
	iconSetBetterThan5WMatchParams.addFile(strIconSetBetterThan5WMatchParams);
	mActionSetBetterThan5WMatchParams = new QAction(iconSetBetterThan5WMatchParams, tr("数据匹配"), this);
	mActionSetBetterThan5WMatchParams->setObjectName(QStringLiteral("mActionSetBetterThan5WMatchParams"));
	mActionSetBetterThan5WMatchParams->setWhatsThis(tr("数据匹配"));
	connect(mActionSetBetterThan5WMatchParams, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::SetBetterThan5WMatchParams);
	mQGisIface->addVectorToolBarIcon(mActionSetBetterThan5WMatchParams);
	mQGisIface->addPluginToVectorMenu(tr("&优于1:5万数据融合处理"), mActionSetBetterThan5WMatchParams);
	mActionSetBetterThan5WMatchParams->setEnabled(true);

	/*****************************************************************/
	/*    “优于1:5万数据融合处理— 要素合并”菜单					 */
	/*****************************************************************/
	QIcon iconBetterThan5wMergeFeatures;

	QString strIconBetterThan5wMergeFeatures = curExePath + "/resource/geoextractandprocess/merge_feature.png";
	iconBetterThan5wMergeFeatures.addFile(strIconBetterThan5wMergeFeatures);
	mActionBetterThan5wMergeFeatures = new QAction(iconBetterThan5wMergeFeatures, tr("要素合并"), this);
	mActionBetterThan5wMergeFeatures->setObjectName(QStringLiteral("mActionBetterThan5wMergeFeatures"));
	mActionBetterThan5wMergeFeatures->setWhatsThis(tr("要素合并"));
	connect(mActionBetterThan5wMergeFeatures, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::BetterThan5wMergeFeatures);
	mQGisIface->addVectorToolBarIcon(mActionBetterThan5wMergeFeatures);
	mQGisIface->addPluginToVectorMenu(tr("&优于1:5万数据融合处理"), mActionBetterThan5wMergeFeatures);
	mActionBetterThan5wMergeFeatures->setEnabled(true);


	/*****************************************************************/
	/*    “测绘地理数据预处理— 提取等高线”菜单					 */
	/*****************************************************************/
	QIcon iconExtractContour;
	QString strIconExtractContour = curExePath + "/resource/geoextractandprocess/contour.png";
	iconExtractContour.addFile(strIconExtractContour);
	mActionExtractContour = new QAction(iconExtractContour, tr("提取等高线"), this);
	mActionExtractContour->setObjectName(QStringLiteral("mActionBetterThan5wMergeFeatures"));
	mActionExtractContour->setWhatsThis(tr("提取等高线"));
	connect(mActionExtractContour, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ExtractContour);
	mQGisIface->addVectorToolBarIcon(mActionExtractContour);
	mQGisIface->addPluginToVectorMenu(tr("&系列比例尺地图要素提取"), mActionExtractContour);
	mActionExtractContour->setEnabled(true);

	/*****************************************************************/
	/*    “栅格— 影像图制图”菜单					 */
	/*****************************************************************/
	QIcon iconImageMapping;
	QString strIconImageMapping = curExePath + "/resource/geoextractandprocess/image_mapping.png";
	iconImageMapping.addFile(strIconImageMapping);
	mActionImageMapping = new QAction(iconImageMapping, tr("影像图制图"), this);
	mActionImageMapping->setObjectName(QStringLiteral("mActionImageMapping"));
	mActionImageMapping->setWhatsThis(tr("影像图制图"));
	connect(mActionImageMapping, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ImageMapping);
	mQGisIface->addRasterToolBarIcon(mActionImageMapping);
	mQGisIface->addPluginToRasterMenu(tr("&影像图制图"), mActionImageMapping);
	mActionImageMapping->setEnabled(true);

	/*****************************************************************/
	/*    “栅格— 动目标专题图制作”菜单					 */
	/*****************************************************************/
	/*QIcon iconMovingTargetMapping;
	QString strIconMovingTargetMapping = curExePath + "/resource/geoextractandprocess/moving_target_mapping.png";
	iconMovingTargetMapping.addFile(strIconMovingTargetMapping);
	mActionMovingTargetMapping = new QAction(iconMovingTargetMapping, tr("动目标专题制图"), this);
	mActionMovingTargetMapping->setObjectName(QStringLiteral("mActionMovingTargetMapping"));
	mActionMovingTargetMapping->setWhatsThis(tr("动目标专题制图"));
  // 杨小兵-2024-09-06：为了WX系统集成重新集成的【动目标制图】功能，废弃之前的【动目标制图】功能
  connect(mActionMovingTargetMapping, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::MovingTargetMapping);
	mQGisIface->addRasterToolBarIcon(mActionMovingTargetMapping);
	mQGisIface->addPluginToRasterMenu(tr("&动目标专题制图"), mActionMovingTargetMapping);
	mActionMovingTargetMapping->setEnabled(true);*/


  /*****************************************************************/
  /*    “栅格— 动目标专题图制作”菜单					 */
  /*****************************************************************/
  // 杨小兵-2024-09-06：为了WX系统集成重新集成的【动目标制图】功能，废弃之前的【动目标制图】功能
  QIcon iconMovingTargetMapping_LayoutVectorization;
  QString strIconMovingTargetMapping_LayoutVectorization = curExePath + "/resource/geoextractandprocess/moving_target_mapping_layout_vectorization.png";
  iconMovingTargetMapping_LayoutVectorization.addFile(strIconMovingTargetMapping_LayoutVectorization);
  mActionMovingTargetMapping_LayoutVectorization = new QAction(iconMovingTargetMapping_LayoutVectorization, tr("动目标专题制图"), this);
  mActionMovingTargetMapping_LayoutVectorization->setObjectName(QStringLiteral("mActionMovingTargetMapping_LayoutVectorization"));
  mActionMovingTargetMapping_LayoutVectorization->setWhatsThis(tr("动目标专题制图"));
  connect(mActionMovingTargetMapping_LayoutVectorization, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::MovingTargetMappingLayoutVectorization);
  mQGisIface->addRasterToolBarIcon(mActionMovingTargetMapping_LayoutVectorization);
  mQGisIface->addPluginToRasterMenu(tr("&动目标专题制图"), mActionMovingTargetMapping_LayoutVectorization);
  mActionMovingTargetMapping_LayoutVectorization->setEnabled(true);


	/*****************************************************************/
	/*					“栅格— 影像图制图— 国地信数据”菜单					 */
	/*****************************************************************/
	QIcon iconImageMapping_GdxData;
	QString strIconImageMapping_GdxData = curExePath + "/resource/geoextractandprocess/image_mapping_gdx.png";
	iconImageMapping_GdxData.addFile(strIconImageMapping_GdxData);
	mActionImageMapping_GdxData = new QAction(iconImageMapping_GdxData, tr("影像图制图（国地信数据）"), this);
	mActionImageMapping_GdxData->setObjectName(QStringLiteral("mActionImageMapping_GdxData"));
	mActionImageMapping_GdxData->setWhatsThis(tr("影像图制图（国地信数据）"));
	connect(mActionImageMapping_GdxData, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ImageMapping_GdxData);
	mQGisIface->addRasterToolBarIcon(mActionImageMapping_GdxData);
	mQGisIface->addPluginToRasterMenu(tr("&影像图制图"), mActionImageMapping_GdxData);
	mActionImageMapping_GdxData->setEnabled(true);


#pragma region "影像图制图（整饰要素矢量化）"

	/*****************************************************************/
	/*					“栅格— 影像图制图— 整饰要素矢量化”菜单	 */
	/*****************************************************************/
	QIcon iconImageMapping_LayoutVectorization;
	QString strIconImageMapping_LayoutVectorization = curExePath + "/resource/geoextractandprocess/image_mapping_layout_vectorization.png";
	iconImageMapping_LayoutVectorization.addFile(strIconImageMapping_LayoutVectorization);
	mActionImageMapping_LayoutVectorization = new QAction(iconImageMapping_LayoutVectorization, tr("影像图制图（整饰要素矢量化）"), this);
	mActionImageMapping_LayoutVectorization->setObjectName(QStringLiteral("mActionImageMapping_LayoutVectorization"));
	mActionImageMapping_LayoutVectorization->setToolTip(tr("影像图制图（整饰要素矢量化）"));

	connect(mActionImageMapping_LayoutVectorization, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::ImageMappingLayoutVectorization);
	mQGisIface->addRasterToolBarIcon(mActionImageMapping_LayoutVectorization);
	mQGisIface->addPluginToRasterMenu(tr("&影像图制图"), mActionImageMapping_LayoutVectorization);
	mActionImageMapping_LayoutVectorization->setEnabled(true);

#pragma endregion


#pragma region "矢量数据迁移"

	/*****************************************************************/
	/*					“栅格— 影像图制图— 矢量数据迁移”菜单	 */
	/*****************************************************************/
	QIcon iconCopyShapefiles;
	QString strIconCopyShapefiles = curExePath + "/resource/geoextractandprocess/copy_shapefiles.png";
	iconCopyShapefiles.addFile(strIconCopyShapefiles);
	mActionCopyShapefiles = new QAction(iconCopyShapefiles, tr("矢量数据迁移"), this);
	mActionCopyShapefiles->setObjectName(QStringLiteral("mActionCopyShapefiles"));
	mActionCopyShapefiles->setToolTip(tr("矢量数据迁移"));

	connect(mActionCopyShapefiles, &QAction::triggered, this, &QgsGeoExtractAndProcessPlugin::CopyShapefiles);
	mQGisIface->addRasterToolBarIcon(mActionCopyShapefiles);
	mQGisIface->addPluginToRasterMenu(tr("&影像图制图"), mActionCopyShapefiles);
	mActionCopyShapefiles->setEnabled(true);



#pragma endregion
	updateActions();
}

// 创建作业区并入库
void QgsGeoExtractAndProcessPlugin::CreateDbAndInputData()
{
	QgsGeoExtractAndProcessPluginGui* pPluginGui = new QgsGeoExtractAndProcessPluginGui(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

	pPluginGui->show();

	if (pPluginGui->exec() == QDialog::Accepted)
	{
		QgsMessageLog::logMessage(tr("创建作业区并入库开始..."), tr("地理提取与加工-系列比例尺提取与加工"), Qgis::Info);

		//=======================================//
		/*       作业区建库及入库流程            */
		//=======================================//
		/* 一、根据输入图幅的数据，自动计算所属作业区，如果分别属于n个不同作业区，
			   则创建n个作业区数据库，并分别进行入库
		*/
		//=======================================//
		// 获取数据库存储目录
		QString qstrDBPath = pPluginGui->GetDBPath();
		string strDBPath = qstrDBPath.toLocal8Bit();

		// 获取入库数据目录
		QString qstrInputDataPath = pPluginGui->GetInputDataPath();
		string strInputDataPath = qstrInputDataPath.toLocal8Bit();

		QString qstrLog;
		qstrLog.sprintf("数据库存储目录：%s", strDBPath.c_str());
		QgsMessageLog::logMessage(QString(tr("数据库存储目录: ")) + qstrDBPath, tr("地理提取与加工-系列比例尺提取与加工"), Qgis::Info);

		qstrLog.clear();
		qstrLog.sprintf("入库数据存储目录：%s", strInputDataPath.c_str());
		QgsMessageLog::logMessage(QString(tr("入库数据存储目录: ")) + qstrInputDataPath, tr("地理提取与加工-系列比例尺提取与加工"), Qgis::Info);

		int iResult = 0;
		int i = 0;
		int j = 0;
		// 作业区数据库信息
		vector<OperationAreaDBInfo> vOpDBInfos;
		vOpDBInfos.clear();

		// 获取当前路径下所有文件或文件夹
		QStringList qstrPathList = GetDirPathOfSplDir(qstrInputDataPath);
		// 如果当前选择单个图幅文件夹目录
		if (qstrPathList.size() == 0)
		{
			string strInputDataPath = qstrInputDataPath.toLocal8Bit();

			// 文件夹名称
			string strFolderName;

			GetFolderNameFromPath(strInputDataPath, strFolderName);

			// 根据图幅名称获取作业区名称
			string strOperationAreaName;
			iResult = CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(strFolderName, strOperationAreaName);
			if (iResult != 0)
			{
				// 记录日志
				return;
			}

			OperationAreaDBInfo OpDBInfo;
			OpDBInfo.strName = strOperationAreaName;
			OpDBInfo.strFilePath = strDBPath + "/" + strOperationAreaName + ".gpkg";
			OpDBInfo.vInputDataPaths.push_back(strInputDataPath);
			vOpDBInfos.push_back(OpDBInfo);
		}
		// 如果是多个图幅所在文件夹
		else
		{
			for (i = 0; i < qstrPathList.size(); i++)
			{
				// 每个子目录
				string strInputDataPath = qstrPathList[i].toLocal8Bit();

				// 文件夹名称
				string strFolderName;

				GetFolderNameFromPath(strInputDataPath, strFolderName);

				// 根据图幅名称获取作业区名称
				string strOperationAreaName;
				iResult = CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(strFolderName, strOperationAreaName);
				if (iResult != 0)
				{
					// 记录日志
					return;
				}

				// 判断作业区名称是否已经存在
				// 如果已经存在，则将当前目录存入所在作业区的数据路径列表中；
				// 如果不存在，则记录在新的作业区名称中；
				int iOpIndex = 0;		// 当前作业区名称索引
				// 如果作业区名称已经存在
				if (bIsInRefDBVector(strOperationAreaName, vOpDBInfos, iOpIndex))
				{
					vOpDBInfos[iOpIndex].vInputDataPaths.push_back(strInputDataPath);
				}
				else
				{
					OperationAreaDBInfo OpDBInfo;
					OpDBInfo.strName = strOperationAreaName;
					OpDBInfo.strFilePath = strDBPath + "/" + strOperationAreaName + ".gpkg";
					OpDBInfo.vInputDataPaths.push_back(strInputDataPath);
					vOpDBInfos.push_back(OpDBInfo);
				}
			}
		}

		// 记录作业区gpkg数据库全路径
		m_vGpkgPath.clear();
		for (i = 0; i < vOpDBInfos.size(); i++)
		{
			m_vGpkgPath.push_back(vOpDBInfos[i].strFilePath);
		}

		/*----------------------------------------*/
		/*-----对每个作业区分别创建gpkg数据库-----*/
		/*----------------------------------------*/

		// 作业区内接边匹配表结构
		vector<SE_Field> vOpFields;
		vOpFields.clear();

		CreateOperationAreaMergeTableFields_Inner(vOpFields);

		for (i = 0; i < vOpDBInfos.size(); i++)
		{
			// 接边匹配表格名称（数据库名称+"_MatchTable"）
			string strTableName = vOpDBInfos[i].strName + "_MatchTable";

			// 图层标识符，默认与表格名称相同，也可以单独设置
			string strLayerIdentifier = strTableName;

			// 图层描述
			string strLayerDescription = vOpDBInfos[i].strName + "_Def";

			// 图层几何类型（属性表无几何类型）
			SE_GeometryType geoType = SE_NoneType;

			// 默认EPSG空间参考系编码
			int iEPSGCode = 4490;

			// 是否包括Z坐标
			bool bZcoordinate = false;

			// 是否包括M数值
			bool bMvalue = false;

			int iResult = CSE_GeoExtractAndProcess::CreateGpkgDB(vOpDBInfos[i].strName,
				strDBPath,
				strTableName,
				strLayerIdentifier,
				strLayerDescription,
				geoType,
				iEPSGCode,
				vOpFields,
				bZcoordinate,
				bMvalue);

			if (iResult != 0)
			{
				// 记录日志
				continue;
			}

			/*----------------------------------------------*/
			/*-----对作业区对应的图幅文件夹数据进行入库-----*/
			/*----------------------------------------------*/
			for (j = 0; j < vOpDBInfos[i].vInputDataPaths.size(); j++)
			{
				iResult = CSE_GeoExtractAndProcess::PutDataToGpkgDB(vOpDBInfos[i].vInputDataPaths[j],
					vOpDBInfos[i].strFilePath);

				if (iResult != 0)
				{
					// 记录日志
					continue;
				}
			}
		}
		// 增加日志信息
		QgsMessageLog::logMessage(tr("数据入库完成"), tr("地理提取与加工-系列比例尺提取与加工"), Qgis::Success);
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("作业区建库及数据入库"));
		msgBox.setText(tr("数据入库完成！"));
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Cancel);
		msgBox.exec();
		delete pPluginGui;
		return;
	}

	delete pPluginGui;
}

// 设置接边参数
void QgsGeoExtractAndProcessPlugin::SetMergeParams()
{
	// 调用设置接边检测参数对话框
	QgsSetMergeParams* pSetMergeParams = new QgsSetMergeParams(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

	pSetMergeParams->show();

	if (pSetMergeParams->exec() == QDialog::Accepted)
	{
		// 获取线或面选择状态
		bool bLineChecked = false;
		bool bPolygonChecked = false;

		pSetMergeParams->GetGeoTypeChecked(bLineChecked, bPolygonChecked);

		// 获取接边比例尺
		int iScaleType = pSetMergeParams->GetScaleType();
		m_iScaleType = iScaleType;

		// 获取接边缓冲距离
		double dDistance = pSetMergeParams->GetBufferDistance();
		m_dDistance = dDistance;

		// 获取接边参数
		vector<LayerMatchParam> vLayerMatchParam;
		vLayerMatchParam.clear();
		pSetMergeParams->GetLayerMatchParams(vLayerMatchParam);

		vector<LayerMatchParam> vFinalLayerMatchParam;
		vFinalLayerMatchParam.clear();
		// 如果字段匹配数为0，则当前图层不参与拼接
		for (int i = 0; i < vLayerMatchParam.size(); i++)
		{
			LayerMatchParam param = vLayerMatchParam[i];
			// 如果有选择的匹配字段，则进行记录接边参数
			if (param.vFields.size() > 0)
			{
				param.bLineStringChecked = bLineChecked;
				param.bPolygonChecked = bPolygonChecked;
				vFinalLayerMatchParam.push_back(param);
			}
		}

		m_vLayerMatchParam = vFinalLayerMatchParam;
	}

	delete pSetMergeParams;
}

// 作业区内自动接边
void QgsGeoExtractAndProcessPlugin::AutoMerge()
{
	QgsAutoMergeDialog* pDlg = new QgsAutoMergeDialog(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
	pDlg->show();

	if (pDlg->exec() == QDialog::Accepted)
	{
		// 如果接边参数没有设置，则进行提示
		if (m_vLayerMatchParam.size() == 0)
		{
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("请设置接边检测参数！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}

		for (int i = 0; i < m_vGpkgPath.size(); i++)
		{
			// 调用接口进行自动接边
			/*调用接边接口进行计算*/
			vector<LayerMergeRecord> vLayerMergeRecord;
			vLayerMergeRecord.clear();
			int iResult = CSE_GeoExtractAndProcess::OpAutoMerge(m_vLayerMatchParam,
				m_iScaleType,
				m_dDistance,
				m_vGpkgPath[i],
				vLayerMergeRecord);

			if (iResult != 0)
			{
				// 记录日志
				continue;
			}
		}
	}
	else
	{
		delete pDlg;
		return;
	}

	QMessageBox msgBox;
	msgBox.setWindowTitle(tr("地理提取与加工"));
	msgBox.setText(tr("自动接边处理完成！"));
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	msgBox.setDefaultButton(QMessageBox::Cancel);
	msgBox.exec();
	delete pDlg;
}

// 要素提取
void QgsGeoExtractAndProcessPlugin::ExtractData()
{
	// 调用要素提取参数对话框
	QgsSetAttributeExtractParamsDlg* pExtractDataDlg = new QgsSetAttributeExtractParamsDlg(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

	pExtractDataDlg->show();

	if (pExtractDataDlg->exec() == QDialog::Accepted)
	{
		// 获取输入数据类型
		int iInputDataType = pExtractDataDlg->GetInputDataType();

		// 获取输入数据路径
		QString qstrInputDataPath = pExtractDataDlg->GetInputDataPath();

		// 获取输出数据类型
		int iOutputDataType = pExtractDataDlg->GetOutputDataType();

		// 获取输出数据路径
		QString qstrOutputDataPath = pExtractDataDlg->GetOutputDataPath();

		// 获取要素提取参数
		vector<ExtractDataParam> vExtractDataParam;
		vExtractDataParam.clear();

		pExtractDataDlg->GetExtractDataParams(vExtractDataParam);

		// 比例尺类型备用参数
		int iScaleType = pExtractDataDlg->GetSacleType();

		// 判断是否需要输出分幅数据
		bool bOutputSheet = pExtractDataDlg->GetCheckState();

		//GeoExtractAndProcessProgressFunc pfnProgress;
		// 进度回调函数待补充（生成进度条）
		// 调用要素提取接口
		string strInputDataPath = qstrInputDataPath.toLocal8Bit();
		string strOutputDataPath = qstrOutputDataPath.toLocal8Bit();
		int iResult = CSE_GeoExtractAndProcess::ExtractDataByAttribute(
			iInputDataType,
			iScaleType,
			bOutputSheet,
			vExtractDataParam,
			strInputDataPath,
			iOutputDataType,
			strOutputDataPath,
			nullptr);

		if (iResult != 0)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("要素提取失败！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Cancel);
			msgBox.exec();
			delete pExtractDataDlg;
			return;
		}
		else
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("要素提取完成！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Cancel);
			msgBox.exec();
			delete pExtractDataDlg;
			return;
		}
	}
	delete pExtractDataDlg;
}

// 根据作业区范围创建作业区矢量数据文件
void QgsGeoExtractAndProcessPlugin::CreateOperationArea()
{
	// 调用创建作业区文件对话框
	QgsCreateOperationAreaRange* pCreateOpDlg = new QgsCreateOperationAreaRange(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

	pCreateOpDlg->show();

	if (pCreateOpDlg->exec() == QDialog::Accepted)
	{
		// 获取输入数据路径
		QString qstrInputFilePath = pCreateOpDlg->GetInputFilePath();

		// 获取输出数据路径
		QString qstrOutputDataPath = pCreateOpDlg->GetLayerSavePath();

		// 获取输入类型
		int iInputType = pCreateOpDlg->GetInputType();

		// 获取比例尺
		double dScale = pCreateOpDlg->GetScale();

		// 范围
		SE_DRect dRect;
		pCreateOpDlg->GetOperationAreaRange(dRect.dleft, dRect.dtop, dRect.dright, dRect.dbottom);

		string strInputPath = qstrInputFilePath.toLocal8Bit();
		string strSavePath = qstrOutputDataPath.toLocal8Bit();
		vector<string> vOperationName;
		vOperationName.clear();

		int iResult = 0;
		// 如果是文件输入
		if (iInputType == 1)
		{
			iResult = CSE_GeoExtractAndProcess::CreateOperationAreaDataByFile(
				strInputPath,
				dScale,
				strSavePath);
		}
		// 范围输入
		else if (iInputType == 2)
		{
			iResult = CSE_GeoExtractAndProcess::CreateOperationAreaDataByRect(
				dRect,
				dScale,
				strSavePath);
		}

		if (iResult != 0)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("创建作业区划分文件失败！"));
			msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
			msgBox.setDefaultButton(QMessageBox::Cancel);
			msgBox.exec();
			return;
		}
		else
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("创建作业区划分文件完毕！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}
	}
	delete pCreateOpDlg;
}

// 作业区名称与图幅号转换
void QgsGeoExtractAndProcessPlugin::ConvertOpNameAndSheet()
{
	// 调用创建作业区文件对话框
	QgsConvertOperationAreaAndSheet* pConvertDlg = new QgsConvertOperationAreaAndSheet(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

	pConvertDlg->show();
	if (pConvertDlg->exec() == QDialog::Accepted)
	{
	}
	delete pConvertDlg;
}

// 创建作业区间临时接边库
void QgsGeoExtractAndProcessPlugin::CreateOpBetweenTempDB()
{
	QgsCreateOperationAreaBetweenDlg* pDlg = new QgsCreateOperationAreaBetweenDlg(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);

	pDlg->show();
	if (pDlg->exec() == QDialog::Accepted)
	{
		// 作业区之间接边匹配表结构
		vector<SE_Field> vOpBetweenFields;
		vOpBetweenFields.clear();

		// 获取作业区之间接边记录表字段
		CreateOperationAreaMergeTableFields_Between(vOpBetweenFields);

		// 临时数据库名称
		QString qstrTempOpDBName = pDlg->GetTempDBName();
		string strTempOpDBName = qstrTempOpDBName.toLocal8Bit();

		// 临时数据库存储路径
		QString qstrTempOpDBSavePath = pDlg->GetTempDBSavePath();
		m_qstrOpBetweenTempDBPath = qstrTempOpDBSavePath;
		string strOpBetweenTempDBPath = qstrTempOpDBSavePath.toLocal8Bit();

		// 作业区数据库存储路径
		QString qstrOpDBPath = pDlg->GetOpDBPath();
		m_qstrOpDBPath = qstrOpDBPath;
		string strOpDBPath = qstrOpDBPath.toLocal8Bit();

		// 接边匹配表格名称（数据库名称+"_MatchTable"）
		string strTableName = strTempOpDBName + "_MatchTable";

		// 图层标识符，默认与表格名称相同，也可以单独设置
		string strLayerIdentifier = strTableName;

		// 图层描述
		string strLayerDescription = strTempOpDBName + "_Def";

		// 图层几何类型（属性表无几何类型）
		SE_GeometryType geoType = SE_NoneType;

		// 默认EPSG空间参考系编码
		int iEPSGCode = 4490;

		// 是否包括Z坐标
		bool bZcoordinate = false;

		// 是否包括M数值
		bool bMvalue = false;

		int iResult = CSE_GeoExtractAndProcess::CreateGpkgDB(strTempOpDBName,
			strOpBetweenTempDBPath,
			strTableName,
			strLayerIdentifier,
			strLayerDescription,
			geoType,
			iEPSGCode,
			vOpBetweenFields,
			bZcoordinate,
			bMvalue);

		if (iResult != 0)
		{
			// 记录日志
			delete pDlg;
			return;
		}

		m_BetweenOpDBInfo.strBetweenDBName = strTempOpDBName;

		m_BetweenOpDBInfo.strBetweenDBFilePath = strOpBetweenTempDBPath + "/" + strTempOpDBName + ".gpkg";

		// 获取作业区数据库存储目录下所有gpkg
		QStringList gpkgNameList = GetGpkgFileNames(qstrOpDBPath);
		if (gpkgNameList.size() == 0)
		{
 
		}
		else
		{
			m_BetweenOpDBInfo.vOpDBPath.clear();
			for (int i = 0; i < gpkgNameList.size(); i++)
			{
				string strGpkgName = gpkgNameList[i].toLocal8Bit();
				string strGpkgPath = strOpDBPath + "/" + strGpkgName;
				m_BetweenOpDBInfo.vOpDBPath.push_back(strGpkgPath);
			}
		}

		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("创建作业区间临时接边库完成！"));
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Cancel);
		msgBox.exec();
		delete pDlg;
		return;
	}

	delete pDlg;
}

void QgsGeoExtractAndProcessPlugin::BetweenOpAutoMerge()
{
	QgsAutoMergeDialog* pDlg = new QgsAutoMergeDialog(mQGisIface->mainWindow(), QgsGuiUtils::ModalDialogFlags);
	pDlg->show();

	if (pDlg->exec() == QDialog::Accepted)
	{
		// 如果接边参数没有设置，则进行提示
		if (m_vLayerMatchParam.size() == 0)
		{
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("请设置接边检测参数！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}

		// 调用接口进行自动接边
		/*调用接边接口进行计算*/
		vector<LayerMergeRecord> vLayerMergeRecord;
		vLayerMergeRecord.clear();
		int iResult = CSE_GeoExtractAndProcess::BetweenOpAutoMerge(m_vLayerMatchParam,
			m_iScaleType,
			m_dDistance,
			m_BetweenOpDBInfo,
			vLayerMergeRecord);

		if (iResult != 0)
		{
			// 记录日志
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("作业区之间接边失败！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}
	}
	else
	{
		delete pDlg;
		return;
	}

	QMessageBox msgBox;
	msgBox.setWindowTitle(tr("地理提取与加工"));
	msgBox.setText(tr("自动接边处理完成！"));
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	msgBox.setDefaultButton(QMessageBox::Cancel);
	msgBox.exec();
	delete pDlg;
}

// 影像数据空间精度检查
void QgsGeoExtractAndProcessPlugin::ImageSpatialAccuracyCheck()
{
	QgsImageSpatialAccuracyCheckDlg* ImageCheckDlg = new QgsImageSpatialAccuracyCheckDlg();
	ImageCheckDlg->show();
}

void QgsGeoExtractAndProcessPlugin::VectorSpatialAccuracyCheck()
{
	QgsVectorSpatialAccuracyCheckDlg* VectorCheckDlg = new QgsVectorSpatialAccuracyCheckDlg();
	VectorCheckDlg->show();
}

void QgsGeoExtractAndProcessPlugin::PreProcessMultiScaleData()
{
	QgsPreProcessMultiScaleDataDlg* pDlg = new QgsPreProcessMultiScaleDataDlg();
	pDlg->show();
	if (pDlg->exec() == QDialog::Accepted)
	{
		// 获取输入数据路径
		QString qstrInputDataPath = pDlg->GetInputPath();
		string strInputDataPath = qstrInputDataPath.toLocal8Bit();

		// 获取输出数据路径
		QString qstrOutputDataPath = pDlg->GetOutputPath();
		string strOutputDataPath = qstrOutputDataPath.toLocal8Bit();

		int iResult = CSE_GeoExtractAndProcess::PreProcessMultiScaleData(strInputDataPath,
			strOutputDataPath);
		if (iResult != 0)
		{
			// 记录日志
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("多尺度数据一致性处理预处理失败！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}
		else
		{
			// 记录日志
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("多尺度数据一致性处理预处理成功！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}
	}
	delete pDlg;
}

// 多尺度数据一致性处理— 多尺度数据匹配参数设置
void QgsGeoExtractAndProcessPlugin::SetMultiScaleDataMatchParams()
{
	QgsSetMultiScaleDataMatchParams* pDlg = new QgsSetMultiScaleDataMatchParams();
	if (!pDlg)
	{
		return;
	}
	pDlg->show();
}

void QgsGeoExtractAndProcessPlugin::SetBetterThan5WMatchParams()
{
	QgsSetBetterThan5WMatchParams* pDlg = new QgsSetBetterThan5WMatchParams();
	if (!pDlg)
	{
		return;
	}

	pDlg->show();
	if (pDlg->exec() == QDialog::Accepted)
	{
		// 获取国标数据路径
		QString qstrGuoBiaoDataPath = pDlg->GetGuoBiaoDataPath();
		string strGuoBiaoDataPath = qstrGuoBiaoDataPath.toLocal8Bit();

		vector<string> vGuoBiaoDataPath;
		vGuoBiaoDataPath.clear();

		// 获取国标数据下所有图幅目录
		QStringList qstrPathList = GetDirPathOfSplDir(qstrGuoBiaoDataPath);
		if (qstrPathList.size() == 0)
		{
			vGuoBiaoDataPath.push_back(strGuoBiaoDataPath);
			
		}
		else
		{
			for (int i = 0; i < qstrPathList.size(); i++)
			{
				string strPathTemp = qstrPathList[i].toLocal8Bit();
				vGuoBiaoDataPath.push_back(strPathTemp);
			}
			
		}


		// 获取军标数据路径
		QString qstrJunBiaoDataPath = pDlg->GetJunBiaoDataPath();
		string strJunBiaoDataPath = qstrJunBiaoDataPath.toLocal8Bit();

		// 获取输出数据匹配数据库存储路径
		QString qstrDBSavePath = pDlg->GetMatchDBSavePath();
		string strDBSavePath = qstrDBSavePath.toLocal8Bit();

		// 获取点匹配阈值、线面匹配阈值
		double dPointDistance = 0;
		double dLineAndPolygonDistance = 0;
		pDlg->GetMatchDistance(dPointDistance, dLineAndPolygonDistance);

		// 获取匹配参数
		vector<LayerMatchParam> vMatchParam;
		vMatchParam.clear();

		pDlg->GetLayerMatchParams(vMatchParam);

		// 将匹配参数保存到运行环境下的config目录下，要素合并时需要
		SaveBetterThan5wMatchParams(vMatchParam);

		vector<LayerMatchParam> vFinalLayerMatchParam;
		vFinalLayerMatchParam.clear();
		// 如果字段匹配数为0或者几何要素匹配都未☑，则当前图层不参与匹配
		for (int i = 0; i < vMatchParam.size(); i++)
		{
			LayerMatchParam param = vMatchParam[i];

			if (!param.bPointChecked &&
				!param.bLineStringChecked &&
				!param.bPolygonChecked)
			{
				continue;
			}

			// 如果有选择的匹配字段，则进行记录接边参数
			if (param.vFields.size() > 0)
			{
				vFinalLayerMatchParam.push_back(param);
			}
		}

		int iResult = 0;
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr;
		iResult = CSE_GeoExtractAndProcess::BetterThan5wMatch(vGuoBiaoDataPath,
			strJunBiaoDataPath,
			strDBSavePath,
			dPointDistance,
			dLineAndPolygonDistance,
			dLineAndPolygonDistance,
			vFinalLayerMatchParam,
			pfnProgress);

		if (iResult != 0)
		{
			// 记录日志
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("优于1:5万数据融合匹配失败！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}
		else
		{
			// 记录日志
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("优于1:5万数据融合匹配成功！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}
	}

	delete pDlg;
}

void QgsGeoExtractAndProcessPlugin::BetterThan5wMergeFeatures()
{
	QgsBetterThan5wMergeFeaturesDlg* pDlg = new QgsBetterThan5wMergeFeaturesDlg();
	if (!pDlg)
	{
		return;
	}

	pDlg->show();
	if (pDlg->exec() == QDialog::Accepted)
	{
		// 获取军标数据路径
		QString qstrJunBiaoDataPath = pDlg->GetJBDataPath();
		string strJunBiaoDataPath = qstrJunBiaoDataPath.toLocal8Bit();

		// 获取融合匹配数据库文件路径
		QString qstrMatchDBPath = pDlg->GetMatchDBFilePath();
		string strMatchDBPath = qstrMatchDBPath.toLocal8Bit();

		// 获取数据保存目录
		QString qstrDataSavePath = pDlg->GetSaveDataPath();
		string strDataSavePath = qstrDataSavePath.toLocal8Bit();

		vector<string> vGuoBiaoDataPath;
		vGuoBiaoDataPath.clear();

		// 获取国标数据路径
		QString qstrGuoBiaoDataPath = pDlg->GetGBDataPath();
		string strGuoBiaoDataPath = qstrGuoBiaoDataPath.toLocal8Bit();
		QStringList qstrPathList = GetDirPathOfSplDir(qstrGuoBiaoDataPath);
		if (qstrPathList.size() == 0)
		{
			vGuoBiaoDataPath.push_back(strGuoBiaoDataPath);
		}
		else
		{
			for (int i = 0; i < qstrPathList.size(); i++)
			{
				string strPathTemp = qstrPathList[i].toLocal8Bit();
				vGuoBiaoDataPath.push_back(strPathTemp);
			}
		}



		// 读取配置文件
		vector<LayerMatchParam> vMatchParams;
		vMatchParams.clear();
		LoadBetterThan5wMatchParams(vMatchParams);

		GeoExtractAndProcessProgressFunc pfnProgress = nullptr;
		int iResult = CSE_GeoExtractAndProcess::BetterThan5wMergeFeatures(
			strJunBiaoDataPath,
			vGuoBiaoDataPath,
			strMatchDBPath,
			strDataSavePath,
			vMatchParams,
			pfnProgress);

		if (iResult != 0)
		{
			// 记录日志
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("要素合并失败！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}
		else
		{
			// 记录日志
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("要素合并完成！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			delete pDlg;
			return;
		}
	}

	delete pDlg;
}

void QgsGeoExtractAndProcessPlugin::ExtractContour()
{
	QString curPath = QCoreApplication::applicationDirPath();

	// ContourProcess.exe全路径
	QString qstrContourExe = curPath + "/ContourProcess/ContourProcess.exe";

	QProcess* pProcess = new QProcess();
	if (!pProcess)
	{
		return;
	}
	QStringList strList;
	pProcess->start(qstrContourExe, strList);

	bool bResult = false;
	bResult = pProcess->waitForFinished(-1);
	if (!bResult)
	{
		return;
	}
	delete pProcess;
}

// 国军标一体化数据生成
void QgsGeoExtractAndProcessPlugin::ConvertDataToGJB()
{
	QgsConvertDataToGJB* pDlg = new QgsConvertDataToGJB();
	if (!pDlg)
	{
		return;
	}
	pDlg->show();
}

void QgsGeoExtractAndProcessPlugin::ConvertDataToGB_OR_JB()
{
	QgsConvertDataToGB_OR_GJB* pDlg = new QgsConvertDataToGB_OR_GJB();
	if (!pDlg)
	{
		return;
	}
	pDlg->show();
}

// 影像图制作
void QgsGeoExtractAndProcessPlugin::ImageMapping()
{
	QgsImageMappingDialog* pDlg = new QgsImageMappingDialog();
	if (!pDlg)
	{
		return;
	}
	if (pDlg->exec() == QDialog::Accepted)
	{
		delete pDlg;
	}
}

// 影像图制作-国地信数据
void QgsGeoExtractAndProcessPlugin::ImageMapping_GdxData()
{
	QgsImageMappingGdxDataDialog* pDlg = new QgsImageMappingGdxDataDialog();
	if (!pDlg)
	{
		return;
	}
	if (pDlg->exec() == QDialog::Accepted)
	{
		delete pDlg;
	}
}

void QgsGeoExtractAndProcessPlugin::MovingTargetMapping()
{
	QgsMovingTargetMappingDialog* pDlg = new QgsMovingTargetMappingDialog();
	if (!pDlg)
	{
		return;
	}
	if (pDlg->exec() == QDialog::Accepted)
	{
		delete pDlg;
	}

}

void QgsGeoExtractAndProcessPlugin::ImageMappingLayoutVectorization()
{
	QgsImageMappingDialog_LayoutVectorization* pDlg = new QgsImageMappingDialog_LayoutVectorization();
	if (!pDlg)
	{
		return;
	}
	if (pDlg->exec() == QDialog::Accepted)
	{
		delete pDlg;
	}
}

void QgsGeoExtractAndProcessPlugin::CopyShapefiles()
{
	QgsCopyShapefilesDlg* pDlg = new QgsCopyShapefilesDlg();
	if (!pDlg)
	{
		return;
	}
	if (pDlg->exec() == QDialog::Accepted)
	{
		delete pDlg;
	}
}

void QgsGeoExtractAndProcessPlugin::MovingTargetMappingLayoutVectorization()
{
  QgsMovingTargetMappingLayoutVectorizationDialog* pDlg = new QgsMovingTargetMappingLayoutVectorizationDialog();
  if (!pDlg)
  {
    return;
  }
  if (pDlg->exec() == QDialog::Accepted)
  {
    delete pDlg;
  }
}


void QgsGeoExtractAndProcessPlugin::unload()
{
	//------------------------系列比例尺提取与加工---------------------------------------//
	// 去掉ui界面

	mQGisIface->removePluginVectorMenu(tr("&系列比例尺提取与加工"), mActionCreateOperationArea);
	mQGisIface->removeVectorToolBarIcon(mActionCreateOperationArea);
	mQGisIface->removePluginVectorMenu(tr("&系列比例尺提取与加工"), mActionConvertOperationAreaAndSheet);
	mQGisIface->removeVectorToolBarIcon(mActionConvertOperationAreaAndSheet);

	mQGisIface->removePluginVectorMenu(tr("&系列比例尺提取与加工"), mActionCreateDbAndInputData);
	mQGisIface->removeVectorToolBarIcon(mActionCreateDbAndInputData);
	mQGisIface->removePluginVectorMenu(tr("&系列比例尺提取与加工"), mActionSetMergeParams);
	mQGisIface->removeVectorToolBarIcon(mActionSetMergeParams);
	mQGisIface->removePluginVectorMenu(tr("&系列比例尺提取与加工"), mActionAutoMerge);
	mQGisIface->removeVectorToolBarIcon(mActionAutoMerge);
	mQGisIface->removePluginVectorMenu(tr("&系列比例尺提取与加工"), mActionOpBetweenTempDB);
	mQGisIface->removeVectorToolBarIcon(mActionOpBetweenTempDB);

	mQGisIface->removePluginVectorMenu(tr("&系列比例尺提取与加工"), mActionBetweenOpAutoMerge);
	mQGisIface->removeVectorToolBarIcon(mActionBetweenOpAutoMerge);

	mQGisIface->removePluginVectorMenu(tr("&系列比例尺地图要素提取"), mActionExtractData);
	mQGisIface->removeVectorToolBarIcon(mActionExtractData);

	//------------------------测绘地理数据预处理---------------------------------------//
	mQGisIface->removePluginRasterMenu(tr("&测绘地理数据预处理"), mActionImageSpatialAccuracyCheck);
	mQGisIface->removeRasterToolBarIcon(mActionImageSpatialAccuracyCheck);

	mQGisIface->removePluginVectorMenu(tr("&测绘地理数据预处理"), mActionVectorSpatialAccuracyCheck);
	mQGisIface->removeVectorToolBarIcon(mActionVectorSpatialAccuracyCheck);

	//------------------------多尺度数据一致性处理---------------------------------------//
	mQGisIface->removePluginVectorMenu(tr("&多尺度数据一致性处理"), mActionPreProcessMultiScaleData);
	mQGisIface->removeVectorToolBarIcon(mActionPreProcessMultiScaleData);

	mQGisIface->removePluginVectorMenu(tr("&多尺度数据一致性处理"), mActionSetMultiScaleDataParams);
	mQGisIface->removeVectorToolBarIcon(mActionSetMultiScaleDataParams);
	//-----------------------------------------------------------------------------------//

	//------------------------优于1:5万数据融合处理---------------------------------------//
	mQGisIface->removePluginVectorMenu(tr("&优于1:5万数据融合处理"), mActionSetBetterThan5WMatchParams);
	mQGisIface->removeVectorToolBarIcon(mActionSetBetterThan5WMatchParams);

	mQGisIface->removePluginVectorMenu(tr("&优于1:5万数据融合处理"), mActionBetterThan5wMergeFeatures);
	mQGisIface->removeVectorToolBarIcon(mActionBetterThan5wMergeFeatures);

	mQGisIface->removePluginVectorMenu(tr("&优于1:5万数据融合处理"), mActionConvertDataToGJB);
	mQGisIface->removeVectorToolBarIcon(mActionConvertDataToGJB);

	//-----------------------------------------------------------------------------------//
	//------------------------提取等高线---------------------------------------//
	mQGisIface->removePluginVectorMenu(tr("&系列比例尺地图要素提取"), mActionExtractContour);
	mQGisIface->removeVectorToolBarIcon(mActionExtractContour);

	//------------------------影像图制作---------------------------------------//
	mQGisIface->removePluginRasterMenu(tr("&影像图制作"), mActionImageMapping);
	mQGisIface->removeRasterToolBarIcon(mActionImageMapping);

	//------------------------动目标专题制图---------------------------------------//
	//mQGisIface->removePluginRasterMenu(tr("&动目标专题制图"), mActionMovingTargetMapping);
	//mQGisIface->removeRasterToolBarIcon(mActionMovingTargetMapping);

	//------------------------影像图制作（国地信数据）---------------------------------------//
	mQGisIface->removePluginRasterMenu(tr("&影像图制作"), mActionImageMapping_GdxData);
	mQGisIface->removeRasterToolBarIcon(mActionImageMapping_GdxData);

	//------------------------影像图制作（整饰要素矢量化）---------------------------------------//
	mQGisIface->removePluginRasterMenu(tr("&影像图制作"), mActionImageMapping_LayoutVectorization);
	mQGisIface->removeRasterToolBarIcon(mActionImageMapping_LayoutVectorization);

	//------------------------矢量数据迁移---------------------------------------//
	mQGisIface->removePluginRasterMenu(tr("&影像图制作"), mActionCopyShapefiles);
	mQGisIface->removeRasterToolBarIcon(mActionCopyShapefiles);

  //------------------------动目标专题制图（整饰要素矢量化）---------------------------------------//
  mQGisIface->removePluginRasterMenu(tr("&动目标专题制图"), mActionMovingTargetMapping_LayoutVectorization);
  mQGisIface->removeRasterToolBarIcon(mActionMovingTargetMapping_LayoutVectorization);


	delete mActionCreateDbAndInputData;
	delete mActionSetMergeParams;
	delete mActionAutoMerge;
	delete mActionOpBetweenTempDB;
	delete mActionBetweenOpAutoMerge;
	delete mActionExtractData;
	delete mActionCreateOperationArea;
	delete mActionConvertOperationAreaAndSheet;

	delete mActionImageSpatialAccuracyCheck;
	delete mActionVectorSpatialAccuracyCheck;

	delete mActionPreProcessMultiScaleData;
	delete mActionSetMultiScaleDataParams;

	delete mActionSetBetterThan5WMatchParams;
	delete mActionBetterThan5wMergeFeatures;
	delete mActionConvertDataToGJB;
	//delete mActionImageMapping;
	delete mActionMovingTargetMapping;
	delete mActionImageMapping_GdxData;
	delete mActionImageMapping_LayoutVectorization;
	delete mActionCopyShapefiles;
  delete mActionMovingTargetMapping_LayoutVectorization;
}

void QgsGeoExtractAndProcessPlugin::help()
{
	// TODO: help
}

void QgsGeoExtractAndProcessPlugin::updateActions()
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
	return new QgsGeoExtractAndProcessPlugin(qgisInterfacePointer);
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

/*获取指定目录下的文件夹个数*/
int QgsGeoExtractAndProcessPlugin::GetSubdirCountInSplDir(QString dirPath)
{
	return QDir(dirPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
}

/*获取在指定目录下的目录的路径*/
QStringList QgsGeoExtractAndProcessPlugin::GetDirPathOfSplDir(QString dirPath)
{
	QStringList dirPaths;
	QDir splDir(dirPath);
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	QFileInfo tempFileInfo;
	foreach(tempFileInfo, fileInfoListInSplDir) {
		dirPaths << tempFileInfo.absoluteFilePath();
	}
	return dirPaths;
}

/*创建作业区内接边记录表结构*/
void QgsGeoExtractAndProcessPlugin::CreateOperationAreaMergeTableFields_Inner(vector<SE_Field>& vFields)
{
	vFields.clear();
	SE_Field field;

	// 当前图幅编码
	field.strName = "CUR_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 当前FID
	field.strName = "CUR_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	// 邻接图幅编码
	field.strName = "ADJ_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 邻接FID
	field.strName = "ADJ_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	// 图层类型：A、B等
	field.strName = "LAYER_TYPE";
	field.eType = SE_String;
	field.iLength = 50;
	vFields.push_back(field);

	// 几何类型：line、polygon
	field.strName = "GEO_TYPE";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 自动接边标志
	field.strName = "AUTO_MERGE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 接边类型：强制法、平均法、优化法、人机交互编辑
	field.strName = "MERGE_TYPE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 检查完成标志
	field.strName = "CHECKED";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 验收完成标志
	field.strName = "ACCEPTED";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 备份字段1
	field.strName = "BACKUP1";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 备份字段2
	field.strName = "BACKUP2";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);
}

/*创建作业区之间接边记录表结构*/
void QgsGeoExtractAndProcessPlugin::CreateOperationAreaMergeTableFields_Between(vector<SE_Field>& vFields)
{
	vFields.clear();
	SE_Field field;

	// 当前图幅编码
	field.strName = "CUR_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 当前作业区名称
	field.strName = "CUR_OP";
	field.eType = SE_String;
	field.iLength = 50;
	vFields.push_back(field);

	// 当前FID
	field.strName = "CUR_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	// 邻接图幅编码
	field.strName = "ADJ_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 邻接作业区名称
	field.strName = "ADJ_OP";
	field.eType = SE_String;
	field.iLength = 50;
	vFields.push_back(field);

	// 邻接FID
	field.strName = "ADJ_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	// 图层类型：A、B等
	field.strName = "LAYER_TYPE";
	field.eType = SE_String;
	field.iLength = 50;
	vFields.push_back(field);

	// 几何类型：line、polygon
	field.strName = "GEO_TYPE";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 自动接边标志
	field.strName = "AUTO_MERGE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 接边类型：强制法、平均法、优化法、人机交互编辑
	field.strName = "MERGE_TYPE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 检查完成标志
	field.strName = "CHECKED";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 验收完成标志
	field.strName = "ACCEPTED";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 备份字段1
	field.strName = "BACKUP1";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 备份字段2
	field.strName = "BACKUP2";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);
}

/*获取指定目录的最后一级目录*/
void QgsGeoExtractAndProcessPlugin::GetFolderNameFromPath(string strPath, string& strFolderName)
{
	// 获取图幅文件夹名称
	int iIndex = strPath.find_last_of("/");
	if (iIndex != string::npos)
	{
		strFolderName = strPath.substr(iIndex + 1, strPath.length() - 1);
	}
	else
	{
		strFolderName = "";
	}
}

QStringList QgsGeoExtractAndProcessPlugin::GetGpkgFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.gpkg" << "*.GPKG";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

/*保存优于1:5万数据匹配参数*/
/*生成界面配置文件*/
void QgsGeoExtractAndProcessPlugin::SaveBetterThan5wMatchParams(vector<LayerMatchParam>& vLayerMatchParam)
{
	// QDomDocument类
	QDomDocument doc;
	QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"GBK\"");
	doc.appendChild(instruction);
	// 创建根节点  QDomElemet元素
	QDomElement root = doc.createElement("LayerMergeParams");
	// 添加根节点
	doc.appendChild(root);

	bool bPointChecked = false;
	bool bLineChecked = false;
	bool bPolygonChecked = false;

	for (int i = 0; i < vLayerMatchParam.size(); i++)
	{
		LayerMatchParam param = vLayerMatchParam[i];

		bPointChecked = param.bPointChecked;
		bLineChecked = param.bLineStringChecked;
		bPolygonChecked = param.bPolygonChecked;

		// 如果有选择的匹配字段，则进行接边匹配字段xml的生成
		if (param.vFields.size() > 0)
		{
			param.bLineStringChecked = bLineChecked;
			param.bPolygonChecked = bPolygonChecked;
			// 生成Layer节点
			QDomElement layerNode = doc.createElement("Layer");
			root.appendChild(layerNode);

			// 生成LayerName节点
			QDomElement layerName = doc.createElement("LayerName");
			layerNode.appendChild(layerName);
			QDomText strLayerNameText = doc.createTextNode(param.strLayerName.c_str());
			layerName.appendChild(strLayerNameText);

			// 生成PointChecked节点
			QDomElement pointChecked = doc.createElement("PointChecked");
			if (param.bPointChecked)
			{
				layerNode.appendChild(pointChecked);
				QDomText strPointCheckedText = doc.createTextNode("true");
				pointChecked.appendChild(strPointCheckedText);
			}
			else
			{
				layerNode.appendChild(pointChecked);
				QDomText strPointCheckedText = doc.createTextNode("false");
				pointChecked.appendChild(strPointCheckedText);
			}

			// 生成LineChecked节点
			QDomElement lineChecked = doc.createElement("LineChecked");
			if (param.bLineStringChecked)
			{
				layerNode.appendChild(lineChecked);
				QDomText strLineCheckedText = doc.createTextNode("true");
				lineChecked.appendChild(strLineCheckedText);
			}
			else
			{
				layerNode.appendChild(lineChecked);
				QDomText strLineCheckedText = doc.createTextNode("false");
				lineChecked.appendChild(strLineCheckedText);
			}

			// 生成PolygonChecked节点
			QDomElement polygonChecked = doc.createElement("PolygonChecked");
			if (param.bPolygonChecked)
			{
				layerNode.appendChild(polygonChecked);
				QDomText strPolygonCheckedText = doc.createTextNode("true");
				polygonChecked.appendChild(strPolygonCheckedText);
			}
			else
			{
				layerNode.appendChild(polygonChecked);
				QDomText strPolygonCheckedText = doc.createTextNode("false");
				polygonChecked.appendChild(strPolygonCheckedText);
			}

			// 生成Fields节点
			QDomElement fields = doc.createElement("Fields");
			layerNode.appendChild(fields);
			for (int j = 0; j < param.vFields.size(); j++)
			{
				// 生成Field节点
				QDomElement field = doc.createElement("Field");
				fields.appendChild(field);
				QString qstr = QString::fromLocal8Bit(param.vFields[j].c_str());
				QDomText strFieldText = doc.createTextNode(qstr);
				field.appendChild(strFieldText);
			}
		}
	}

	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_match_params.xml";

	QFile file(strFileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		return;
	QTextStream out(&file);
	out.setCodec("GBK");
	doc.save(out, 4, QDomNode::EncodingFromTextStream);
	file.close();
}

void QgsGeoExtractAndProcessPlugin::LoadBetterThan5wMatchParams(vector<LayerMatchParam>& vLayerMatchParam)
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_match_params.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 循环遍历每个子节点
		LayerMatchParam param;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					param.strLayerName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "PointChecked")
				{
					if (elementTemp.text() == "true")
					{
						param.bPointChecked = true;
					}
					else
					{
						param.bPointChecked = false;
					}
				}

				else if (elementTemp.tagName() == "LineChecked")
				{
					if (elementTemp.text() == "true")
					{
						param.bLineStringChecked = true;
					}
					else
					{
						param.bLineStringChecked = false;
					}
				}
				else if (elementTemp.tagName() == "PolygonChecked")
				{
					if (elementTemp.text() == "true")
					{
						param.bPolygonChecked = true;
					}
					else
					{
						param.bPolygonChecked = false;
					}
				}
				else if (elementTemp.tagName() == "Fields")
				{
					// 获取字段列表
					QDomNodeList fieldList = nodeTemp.childNodes();
					for (int j = 0; j < fieldList.size(); j++)
					{
						QDomNode fieldNode = fieldList.at(j);
						QDomElement fieldElement = fieldNode.toElement();
						QString qstrField = fieldElement.text();
						string strField = qstrField.toLocal8Bit();
						param.vFields.push_back(strField);
					}
				}
			}
		}

		vLayerMatchParam.push_back(param);

		node = node.nextSibling();//读取下一个兄弟节点
	}
}

void QgsGeoExtractAndProcessPlugin::LoadBetterThan5wConfig_GBLayer(vector<GBLayerInfo>& vGBLayerInfo)
{
	vGBLayerInfo.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/GB_Layer.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 循环遍历每个子节点
		GBLayerInfo gbLayerInfo;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					gbLayerInfo.strName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "LayerNameAlias")
				{
					gbLayerInfo.strAlias = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "GJBLayerList")
				{
					// 获取GJB图层列表
					QDomNodeList gjbLayerList = nodeTemp.childNodes();
					for (int j = 0; j < gjbLayerList.size(); j++)
					{
						QDomNode gjbNode = gjbLayerList.at(j);
						QDomElement gjbElement = gjbNode.toElement();
						QString qstrGJB = gjbElement.text();
						string strGJBLayer = qstrGJB.toLocal8Bit();
						gbLayerInfo.vGJBLayer.push_back(strGJBLayer);
					}
				}
			}
		}

		vGBLayerInfo.push_back(gbLayerInfo);

		node = node.nextSibling();//读取下一个兄弟节点
	}
}

void QgsGeoExtractAndProcessPlugin::LoadBetterThan5wConfig_JBLayer(vector<JBLayerInfo>& vJBLayerInfo)
{
	vJBLayerInfo.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/JB_Layer.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 循环遍历每个子节点
		JBLayerInfo jbLayerInfo;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					jbLayerInfo.strName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "LayerNameAlias")
				{
					jbLayerInfo.strAlias = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "GJBLayerList")
				{
					// 获取GJB图层列表
					QDomNodeList gjbLayerList = nodeTemp.childNodes();
					for (int j = 0; j < gjbLayerList.size(); j++)
					{
						QDomNode gjbNode = gjbLayerList.at(j);
						QDomElement gjbElement = gjbNode.toElement();
						QString qstrGJB = gjbElement.text();
						string strGJBLayer = qstrGJB.toLocal8Bit();
						jbLayerInfo.vGJBLayer.push_back(strGJBLayer);
					}
				}
			}
		}

		vJBLayerInfo.push_back(jbLayerInfo);

		node = node.nextSibling();//读取下一个兄弟节点
	}
}

void QgsGeoExtractAndProcessPlugin::LoadBetterThan5wConfig_GJBLayerField(vector<GJBLayerInfo>& vGJBLayerInfo)
{
	vGJBLayerInfo.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/GJB_LayerField.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			// 循环遍历每个子节点
			GJBLayerInfo gjbLayerInfo;
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					gjbLayerInfo.strName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "LayerNameAlias")
				{
					gjbLayerInfo.strAlias = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "Field_List")
				{
					// 获取GJB图层字段列表
					QDomNodeList gjbFieldList = nodeTemp.childNodes();
					for (int j = 0; j < gjbFieldList.size(); j++)
					{
						// Field节点
						QDomNode gjbFieldNode = gjbFieldList.at(j);

						// 获取Field节点的所有子节点
						QDomNodeList fieldNodeList = gjbFieldNode.childNodes();
						GJBFieldInfo fieldInfo;
						for (int k = 0; k < fieldNodeList.size(); k++)
						{
							QDomNode fieldInfoNode = fieldNodeList.at(k);
							QDomElement elementfieldInfo = fieldInfoNode.toElement();

							// 依次判断字段节点下所有节点的名称
							if (elementfieldInfo.tagName() == "Name")
							{
								fieldInfo.strName = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Alias")
							{
								fieldInfo.strAlias = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Must")
							{
								fieldInfo.strMust = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Default")
							{
								fieldInfo.strDefault = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Type")
							{
								fieldInfo.strType = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Len")
							{
								fieldInfo.iLength = elementfieldInfo.text().toLocal8Bit().toInt();
							}

							else if (elementfieldInfo.tagName() == "Unit")
							{
								fieldInfo.strUnit = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "ValidValues")
							{
								fieldInfo.strValidValues = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Detail")
							{
								fieldInfo.strDetail = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "KeyField")
							{
								fieldInfo.strKeyField = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "DataSource_List")
							{
								// 获取DataSource_List节点的所有子节点
								QDomNodeList dataSourceNodeList = fieldInfoNode.childNodes();
								for (int m = 0; m < dataSourceNodeList.size(); m++)
								{
									QDomNode dataSourceNode = dataSourceNodeList.at(m);

									// 获取DataSource节点下所有节点
									QDomNodeList dataSourceTempList = dataSourceNode.childNodes();

									for (int n = 0; n < dataSourceTempList.size(); n++)
									{
										QDomNode dataSourceTemp = dataSourceTempList.at(n);
										QDomElement dataSourceInfo = dataSourceTemp.toElement();

										if (dataSourceInfo.tagName() == "DataSourceName")
										{
											string strDataSourceName = dataSourceInfo.text().toLocal8Bit();
											fieldInfo.vDataSourceName.push_back(strDataSourceName);
										}

										else if (dataSourceInfo.tagName() == "DataSourceLayerName")
										{
											string strDataSourceLayerName = dataSourceInfo.text().toLocal8Bit();
											fieldInfo.vDataSourceLayerName.push_back(strDataSourceLayerName);
										}

										else if (dataSourceInfo.tagName() == "DataSourceField")
										{
											string strDataSourceField = dataSourceInfo.text().toLocal8Bit();
											fieldInfo.vDataSourceField.push_back(strDataSourceField);
										}
									}
								}
							}
						}
						gjbLayerInfo.vFields.push_back(fieldInfo);
					}
				}
			}

			vGJBLayerInfo.push_back(gjbLayerInfo);
		}
		node = node.nextSibling();//读取下一个兄弟节点
	}
}
